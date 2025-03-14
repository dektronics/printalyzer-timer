#include "file_picker.h"

#include <stdio.h>
#include <stdlib.h>
#include <ff.h>

#define LOG_TAG "file_picker"
#include <elog.h>

#include "display.h"
#include "util.h"
#include "main_menu.h"
#include "utarray.h"
#include "bsdlib.h"
#include "usb_msc_fatfs.h"

typedef struct {
    BYTE fattrib;
    TCHAR altname[13];
    char list_entry[DISPLAY_MENU_ROW_LENGTH + 1];
} file_picker_entry_t;

typedef struct {
    BYTE fattrib;
    TCHAR altname[13];
    uint8_t option;
} file_picker_selection_t;

static uint8_t mounted_drive_count();
static menu_result_t drive_selection_impl(const char *title, uint8_t *num);
static menu_result_t file_picker_impl(const char *title, const char *path, uint8_t option, file_picker_selection_t *selection, file_picker_filter_func_t filter_func);
static void build_file_list_entry(file_picker_entry_t *entry, const FILINFO *fno);
static int file_entry_sort(const void *a, const void *b);

static UT_icd file_picker_entry_icd = {
    sizeof(file_picker_entry_t), NULL, NULL, NULL
};

menu_result_t file_picker_show(const char *title, char *filepath, size_t len, file_picker_filter_func_t filter_func)
{
    menu_result_t result;
    file_picker_selection_t selection;
    uint8_t option_list[32];
    char path_buf[512];
    size_t path_len = 0;
    size_t name_len;
    uint8_t option_index = 0;
    bool select_drive = true;
    uint8_t drive_num = 0;
    char *p = NULL;
    char *q = NULL;
    log_i("Open file picker");

    do {
        if (select_drive) {
            result = drive_selection_impl(title, &drive_num);
            if (result == MENU_OK) {
                strncpy(path_buf, "0:/", 4);
                path_buf[0] = '0' + drive_num;
                path_len = strlen(path_buf);
                p = strrchr(path_buf, '/');
                option_list[option_index] = 1;
                select_drive = false;
            } else if (result == MENU_CANCEL || result == MENU_TIMEOUT) {
                break;
            }
        }

        result = file_picker_impl(title, path_buf, option_list[option_index], &selection, filter_func);
        if (result == MENU_OK) {
            name_len = strlen(selection.altname);
            if (path_len + name_len + 1 > sizeof(path_buf) - 1 || option_index > sizeof(option_list) - 1) {
                log_w("Path too long");
                continue;
            }
            if (path_buf[path_len - 1] != '/') {
                strcat(path_buf, "/");
                path_len++;
            }
            strcat(path_buf, selection.altname);
            path_len += name_len;
            if (selection.fattrib & AM_DIR) {
                option_list[option_index++] = selection.option;
                continue;
            } else {
                break;
            }
        } else if (result == MENU_CANCEL) {
            if (option_index == 0) {
                if (mounted_drive_count() > 1) {
                    select_drive = true;
                    continue;
                } else {
                    break;
                }
            }

            q = strrchr(path_buf, '/');
            if (q && q > p) {
                path_len = q - path_buf;
                *q = '\0';
                option_index--;
            } else if (q && q == p) {
                q++;
                path_len = q - path_buf;
                *q = '\0';
                option_index--;
            } else {
                break;
            }
        } else if (result == MENU_TIMEOUT) {
            break;
        }
    } while (result != MENU_SAVE);

    if (result == MENU_OK) {
        if (filepath && len > 0) {
            strncpy(filepath, path_buf, len);
        }
    }
    return result;
}

uint8_t mounted_drive_count()
{
    uint8_t count = 0;
    for (uint8_t i = 0; i < usbh_msc_max_drives(); i++) {
        if (usbh_msc_is_mounted(i)) { count++; }
    }
    return count;
}

menu_result_t drive_selection_impl(const char *title, uint8_t *num)
{
    menu_result_t menu_result = MENU_CANCEL;
    char buf[256];
    uint8_t drive_num[8];
    uint8_t drive_count;
    uint8_t max;
    size_t offset = 0;
    uint8_t option = 0;
    uint8_t j = 0;

    log_i("Preparing drive picker");

    max = usbh_msc_max_drives();
    if (max > sizeof(drive_num)) {
        return MENU_CANCEL;
    }

    /* Handle the case of a single drive */
    drive_count = mounted_drive_count();
    if (drive_count < 2) {
        for (uint8_t i = 0; i < max; i++) {
            if (usbh_msc_is_mounted(i)) {
                *num = i;
                return MENU_OK;
            }
        }
    }

    for (uint8_t i = 0; i < max; i++) {
        if (usbh_msc_is_mounted(i)) {
            if (j > 0) {
                buf[offset++] = '\n';
            }

            if (*num == i) {
                option = j + 1;
            }
            drive_num[j++] = i;

            const char *label = usbh_msc_drive_label(i);
            if (label) {
                sprintf(buf + offset, "%c: %s", 'A' + i, label);
            } else {
                sprintf(buf + offset, "%c: <USB DRIVE>", 'A' + i);
            }
            offset += pad_str_to_length(buf + offset, ' ', DISPLAY_MENU_ROW_LENGTH);
        }
    }

    option = display_selection_list(title, option, buf);
    if (option == 0) {
        menu_result = MENU_CANCEL;
    } else if (option == UINT8_MAX) {
        menu_result = MENU_TIMEOUT;
    } else {
        *num = drive_num[option - 1];
        menu_result = MENU_OK;
    }
    return menu_result;
}

menu_result_t file_picker_impl(const char *title, const char *path, uint8_t option, file_picker_selection_t *selection, file_picker_filter_func_t filter_func)
{
    menu_result_t menu_result = MENU_CANCEL;
    FRESULT res;
    DIR dir;
    FILINFO fno;
    file_picker_entry_t file_entry;
    UT_array *file_entry_list = NULL;
    file_picker_entry_t *p;
    char *list_buf = NULL;
    size_t list_size = 0;
    size_t offset = 0;

    log_i("Preparing file picker");

    res = f_opendir(&dir, path);
    if (res == FR_OK) {
        utarray_new(file_entry_list, &file_picker_entry_icd);
        if (!file_entry_list) {
            return menu_result;
        }

        for (;;) {
            res = f_readdir(&dir, &fno);
            if (res != FR_OK || fno.fname[0] == 0) break;
            if (fno.fattrib & AM_HID) { continue; }
            if (filter_func) {
                if (!filter_func(&fno)) { continue; }
            }
            build_file_list_entry(&file_entry, &fno);
            utarray_push_back(file_entry_list, &file_entry);
            if (utarray_has_oom(file_entry_list)) {
                break;
            }
        }
        f_closedir(&dir);

        utarray_sort(file_entry_list, file_entry_sort);
    }

    if (!file_entry_list) {
        return menu_result;
    }

    /*
     * Until the whole u8g2 selection list implementation is rewritten,
     * we have to cap the length of the file list at an 8-bit row count.
     * Whenever that changes, the entire way this list is generated should
     * change to more efficiently support long lists.
     */
    list_size = utarray_len(file_entry_list);
    if (list_size > UINT8_MAX - 2) {
        list_size = UINT8_MAX - 2;
    }

    if (option == 0 || option > list_size) {
        option = 1;
    }

    list_buf = pvPortMalloc(list_size * (DISPLAY_MENU_ROW_LENGTH + 1));
    if (!list_buf) {
        log_e("Unable to allocate list buffer");
        utarray_free(file_entry_list);
        return menu_result;
    }

    p = NULL;
    while ((p = (file_picker_entry_t*)utarray_next(file_entry_list, p))) {
        strncpy(list_buf + offset, p->list_entry, DISPLAY_MENU_ROW_LENGTH);
        offset += DISPLAY_MENU_ROW_LENGTH;
        list_buf[offset++] = '\n';
    }
    if (offset > 0) {
        list_buf[offset - 1] = '\0';
    }

    log_i("Showing picker");
    option = display_selection_list(title, option, list_buf);
    if (option == 0) {
        menu_result = MENU_CANCEL;
    } else if (option == UINT8_MAX) {
        menu_result = MENU_TIMEOUT;
    } else {
        p = utarray_eltptr(file_entry_list, option - 1);
        if (p && selection) {
            selection->fattrib = p->fattrib;
            strncpy(selection->altname, p->altname, 13);
            selection->option = option;
        }
        menu_result = MENU_OK;
    }

    vPortFree(list_buf);
    utarray_free(file_entry_list);

    return menu_result;
}

void build_file_list_entry(file_picker_entry_t *entry, const FILINFO *fno)
{
    char size_buf[5];
    size_t name_len;
    size_t max_name_len = DISPLAY_MENU_ROW_LENGTH - sizeof(size_buf) - 1;
    int32_t fsize;

    memset(entry, 0, sizeof(file_picker_entry_t));
    entry->fattrib = fno->fattrib;
    if (fno->altname[0] == '\0') {
        strncpy(entry->altname, fno->fname, 13);
    } else {
        strncpy(entry->altname, fno->altname, 13);
    }

    name_len = strlen(fno->fname);
    if (name_len > max_name_len) {
        strncpy(entry->list_entry, fno->fname, max_name_len - 3);
        strncpy(entry->list_entry + (max_name_len - 3), "...", 4);
    } else {
        strncpy(entry->list_entry, fno->fname, name_len);
        pad_str_to_length(entry->list_entry, ' ', max_name_len);
    }

    if (fno->fattrib & AM_DIR) {
        strncpy(entry->list_entry + max_name_len, " <DIR>", 7);
    } else {
        if (fno->fsize > INT32_MAX) {
            fsize = INT32_MAX;
        } else {
            fsize = fno->fsize;
        }
        memset(size_buf, 0, sizeof(size_buf));
        if (humanize_number(size_buf, sizeof(size_buf), fsize, "", HN_AUTOSCALE, HN_B | HN_NOSPACE | HN_DECIMAL) > 0) {
            sprintf(entry->list_entry + max_name_len, " %5s", size_buf);
        }
    }
}

int file_entry_sort(const void *a, const void *b) {
    static char sort_buf1[DISPLAY_MENU_ROW_LENGTH - 9];
    static char sort_buf2[DISPLAY_MENU_ROW_LENGTH - 9];
    int result;
    file_picker_entry_t *entry1 = (file_picker_entry_t *)a;
    file_picker_entry_t *entry2 = (file_picker_entry_t *)b;

    /* Directories always come first */
    if (entry1->fattrib & AM_DIR && !(entry2->fattrib & AM_DIR)) {
        result = -1;
    } else if (!(entry1->fattrib & AM_DIR) && entry2->fattrib & AM_DIR) {
        result = 1;
    } else {
        /* Try sorting on the non-truncated portion of the display name */
        strxfrm(sort_buf1, entry1->list_entry, sizeof(sort_buf1));
        strxfrm(sort_buf2, entry2->list_entry, sizeof(sort_buf2));
        result = strcmp(sort_buf1, sort_buf2);
        if (result == 0) {
            /* As a last resort, sort on the altname */
            result = strcoll(entry1->altname, entry2->altname);
        }
    }
    return result;
}

bool file_picker_expand_filename(char *filename, size_t len, const char *filepath)
{
    char path_buf[512];
    char file_buf[32];
    char *p;
    FRESULT res;
    DIR dir;
    FILINFO fno;
    bool result = false;

    /* Length-check the input and make a working copy */
    if (strlen(filepath) > sizeof(path_buf) - 1) {
        return false;
    }
    strcpy(path_buf, filepath);

    /* Find the path delimiter */
    p = strrchr(filepath, '/');
    if (!p) {
        return false;
    }
    strncpy(path_buf, filepath, p - filepath);
    path_buf[p - filepath] = '\0';
    strncpy(file_buf, p + 1, sizeof(file_buf));

    res = f_opendir(&dir, path_buf);
    if (res == FR_OK) {
        for (;;) {
            res = f_readdir(&dir, &fno);
            if (res != FR_OK || fno.fname[0] == 0) break;
            if (strcmp(fno.altname, file_buf) == 0) {
                if (strlen(fno.fname) + 1 > len) {
                    break;
                }
                strncpy(filename, fno.fname, len);
                result = true;
                break;
            }
        }
        f_closedir(&dir);
    }

    return result;
}
