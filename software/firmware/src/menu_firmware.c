#include "menu_firmware.h"

#include <stdio.h>
#include <string.h>
#include <ff.h>

#define LOG_TAG "menu_firmware"
#include <elog.h>
#include <settings.h>

#include "main_task.h"
#include "usb_host.h"
#include "display.h"
#include "file_picker.h"
#include "app_descriptor.h"
#include "usb_msc_fatfs.h"

extern CRC_HandleTypeDef hcrc;

#define FIRMWARE_SIZE (384 * 1024)

static const char *DISPLAY_TITLE = "Firmware Update";

typedef enum {
    VALIDATE_SUCCESS = 0,
    VALIDATE_FILE_NOT_FOUND,
    VALIDATE_FILE_READ_ERROR,
    VALIDATE_FILE_BAD,
    VALIDATE_FAILED
} validate_result_t;

static bool file_picker_firmware_filter(const FILINFO *fno);
static validate_result_t validate_selected_file(const char *filename, app_descriptor_t *fw_descriptor);
static bool query_file_device(const char *file_path, char *dev_serial, size_t len);

menu_result_t menu_firmware()
{
    char buf[256];
    char path_buf[256];
    char dev_serial[21];
    uint8_t option;
    size_t offset;
    validate_result_t validate_result;
    app_descriptor_t fw_descriptor;
    const app_descriptor_t *app_descriptor = app_descriptor_get();

    /* Check if USB stick inserted */
    if (!usb_msc_is_mounted()) {
        option = display_message(
                "Update Firmware",
                NULL,
                "\n"
                "Please insert a USB storage\n"
                "device and try again.\n", " OK ");
        if (option == UINT8_MAX) {
            return MENU_TIMEOUT;
        } else {
            return MENU_OK;
        }
    }

    option = file_picker_show("Select Firmware Update", path_buf, sizeof(path_buf), file_picker_firmware_filter);
    if (option == MENU_TIMEOUT) {
        return MENU_TIMEOUT;
    } else if (option != MENU_OK) {
        return MENU_OK;
    }

    validate_result = validate_selected_file(path_buf, &fw_descriptor);

    if (validate_result == VALIDATE_SUCCESS) {
        if (!query_file_device(path_buf, dev_serial, sizeof(dev_serial))) {
            validate_result = VALIDATE_FILE_READ_ERROR;
        }
    }

    if (validate_result != VALIDATE_SUCCESS) {
        switch (validate_result) {
        case VALIDATE_FILE_NOT_FOUND:
            display_message(DISPLAY_TITLE, NULL,
                "\nFile not found!\n",
                " OK ");
            break;
        case VALIDATE_FILE_BAD:
            display_message(DISPLAY_TITLE, NULL,
                "\nFile is not a valid\n"
                "Printalyzer Enlarging Timer\n"
                "firmware image!",
                " OK ");
            break;
        case VALIDATE_FILE_READ_ERROR:
        case VALIDATE_FAILED:
        default:
            display_message(DISPLAY_TITLE, NULL,
                "\nUnable to read file!\n",
                " OK ");
            break;
        };
        return MENU_OK;
    }

    offset = 0;
    buf[0] = '\n';
    buf[1] = '\0';
    offset++;
    offset += menu_build_padded_format_row(buf + offset, "From:", "%s %s", app_descriptor->version, app_descriptor->build_date);
    offset += menu_build_padded_format_row(buf + offset, "  To:", "%s %s", fw_descriptor.version, fw_descriptor.build_date);

    option = display_message(
        "Install Firmware Update?\n",
        NULL,
        buf,
        " NO \n YES ");
    if (option == MENU_TIMEOUT) {
        return MENU_TIMEOUT;
    } else if (option != 2) {
        return MENU_OK;
    }

    display_static_list(DISPLAY_TITLE, "\n\nRestarting...");

    if (!settings_set_bootloader_firmware(dev_serial, fw_descriptor.crc32, path_buf + 3)) {
        display_static_list(DISPLAY_TITLE, "\n\nUnable to update!");
        osDelay(500);
        return MENU_OK;
    }

    osDelay(500);

    /* Set a flag in the RTC backup registers to tell the bootloader to start */
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();
    RTC->BKP1R = 0xBB000000UL;
    HAL_PWR_DisableBkUpAccess();
    __HAL_RCC_PWR_CLK_DISABLE();

    /* Shut down and restart the system */
    main_task_shutdown();

    return MENU_OK;
}

bool file_picker_firmware_filter(const FILINFO *fno)
{
    size_t len;

    /* Always allow directories */
    if(fno->fattrib & AM_DIR) {
        return true;
    }

    /* Exclude files that don't end in ".bin" */
    len = strlen(fno->fname);
    if (!(len > 4 && strncasecmp(fno->fname + (len - 4), ".bin", 4) == 0)) {
        return false;
    }

    return true;
}

validate_result_t validate_selected_file(const char *filename, app_descriptor_t *fw_descriptor)
{
    FRESULT res;
    FIL fp;
    bool file_open = false;
    uint8_t buf[512];
    UINT bytes_remaining;
    UINT bytes_to_read;
    UINT bytes_read;
    uint32_t calculated_crc = 0;
    app_descriptor_t image_descriptor = {0};
    validate_result_t result = VALIDATE_FAILED;

    display_static_list(DISPLAY_TITLE, "\n\nChecking selected file...");

    do {
        /* Open firmware file, if it exists */
        res = f_open(&fp, filename, FA_READ);
        if (res != FR_OK) {
            log_w("Unable to open firmware file: %d", res);
            result = VALIDATE_FILE_NOT_FOUND;
            break;
        }
        file_open = true;

        /* Check size of the firmware file */
        if (f_size(&fp) != FIRMWARE_SIZE) {
            log_w("Firmware size is invalid: %lu", f_size(&fp));
            result = VALIDATE_FILE_BAD;
            break;
        }
        log_i("Firmware size is okay.");

        /* Calculate the firmware file checksum */
        __HAL_CRC_DR_RESET(&hcrc);
        bytes_remaining = f_size(&fp) - 4;
        do {
            bytes_to_read = MIN(sizeof(buf), bytes_remaining);
            res = f_read(&fp, buf, bytes_to_read, &bytes_read);
            if (res == FR_OK) {
                /* This check should never fail, but safer to do it anyways */
                if ((bytes_read % 4) != 0) {
                    log_w("Bytes read are not word aligned");
                    result = VALIDATE_FILE_READ_ERROR;
                    break;
                }

                calculated_crc = HAL_CRC_Accumulate(&hcrc, (uint32_t *)buf, bytes_read / 4);

                bytes_remaining -= bytes_read;

                /* Break if at EOF */
                if (bytes_read < bytes_to_read) {
                    break;
                }
            } else {
                log_w("File read error: %d", res);
                result = VALIDATE_FILE_READ_ERROR;
                break;
            }
        } while (res == FR_OK && bytes_remaining > 0);

        __HAL_RCC_CRC_FORCE_RESET();
        __HAL_RCC_CRC_RELEASE_RESET();

        if (result == VALIDATE_FILE_READ_ERROR) { break; }

        /* Read the app descriptor from the end of the firmware file */
        res = f_lseek(&fp, f_tell(&fp) - (sizeof(app_descriptor_t) - 4));
        if (res != FR_OK) {
            log_w("Unable to seek to read the firmware file descriptor: %d", res);
            result = VALIDATE_FILE_READ_ERROR;
            break;
        }
        res = f_read(&fp, &image_descriptor, sizeof(app_descriptor_t), &bytes_read);
        if (res != FR_OK || bytes_read != sizeof(app_descriptor_t)) {
            log_w("Unable to read the firmware file descriptor: %d", res);
            result = VALIDATE_FILE_READ_ERROR;
            break;
        }

        f_rewind(&fp);

        if (calculated_crc != image_descriptor.crc32) {
            log_w("Firmware checksum mismatch: %08lX != %08lX",
                image_descriptor.crc32, calculated_crc);
            result = VALIDATE_FILE_BAD;
            break;
        }
        log_i("Firmware checksum is okay.");

        if (image_descriptor.magic_word != APP_DESCRIPTOR_MAGIC_WORD) {
            log_w("Bad magic");
            result = VALIDATE_FILE_BAD;
            break;
        }

        result = VALIDATE_SUCCESS;
    } while (0);

    if (file_open) {
        f_close(&fp);
    }

    if (fw_descriptor) {
        memcpy(fw_descriptor, &image_descriptor, sizeof(app_descriptor_t));
    }

    return result;
}

bool query_file_device(const char *file_path, char *dev_serial, size_t len)
{
    size_t path_len;
    uint8_t dev_num;

    /* Make sure the path is long enough to contain a prefix */
    path_len = strlen(file_path);
    if (path_len <= 3) {
        return false;
    }

    /* First character should contain a device number */
    if (file_path[0] < '0' || file_path[0] > '9') {
        return false;
    }
    dev_num = file_path[0] - '0';

    /* Device number should be followed by a ":/" */
    if (file_path[1] != ':' || file_path[2] != '/') {
        return false;
    }

    log_d("Device: %d", dev_num);
    log_d("File: %s", file_path + 3);

    /* Make sure device is still mounted */
    if (!usbh_msc_is_mounted(dev_num)) {
        return false;
    }

    /* Query the device serial number */
    if (!usb_msc_get_serial(dev_num, dev_serial, len)) {
        return false;
    }

    dev_serial[len - 1] = '\0';

    log_d("Volume serial: %s", dev_serial);

    return true;
}