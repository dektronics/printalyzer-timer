#include "menu_firmware.h"

#include <string.h>
#include <ff.h>
#include <machine/endian.h>

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

/** Start address of the bootloader in flash */
#define BOOTLOADER_ADDRESS 0x08000000UL

#define BOOTLOADER_SIZE (64 * 1024)
#define FIRMWARE_SIZE (448 * 1024)

static const char *DISPLAY_TITLE = "Firmware Update";

typedef enum {
    BOOTLOADER_INVALID = -1,
    BOOTLOADER_NO_VERSION = 0,
    BOOTLOADER_FORMAT_1 = 1,
    BOOTLOADER_FORMAT_2 = 2
} bootloader_format_t;

typedef enum {
    VALIDATE_SUCCESS = 0,
    VALIDATE_FILE_NOT_FOUND,
    VALIDATE_FILE_READ_ERROR,
    VALIDATE_FILE_BAD,
    VALIDATE_FAILED
} validate_result_t;

static bootloader_format_t query_bootloader_version();
static bool file_picker_firmware_filter(const FILINFO *fno);
static validate_result_t validate_selected_file(const char *filename, app_descriptor_t *fw_descriptor);
static bool query_file_device(const char *file_path, uint8_t *dev_serial, size_t len, bootloader_format_t boot_format);

menu_result_t menu_firmware()
{
    char buf[256];
    char path_buf[256];
    uint8_t dev_serial[21];
    uint8_t option;
    size_t offset;
    validate_result_t validate_result;
    app_descriptor_t fw_descriptor;
    const app_descriptor_t *app_descriptor = app_descriptor_get();
    bootloader_format_t boot_format;

    boot_format = query_bootloader_version();

    if (boot_format == BOOTLOADER_INVALID) {
        option = display_message(
                "Unrecognized Bootloader",
                NULL,
                "\n"
                "Please refer to the recovery\n"
                "procedure in the user manual\n"
                "to update the firmware.\n",
                " OK ");
        if (option == UINT8_MAX) {
            return MENU_TIMEOUT;
        } else {
            return MENU_OK;
        }
    }

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
        if (!query_file_device(path_buf, dev_serial, sizeof(dev_serial), boot_format)) {
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

/**
 * Check the bootloader descriptor block to validate the bootloader version.
 *
 * Validates the bootloader checksum and inspects the bootloader versio to
 * determine the appropriate format to use when storing the selected firmware
 * file for upgrade purposes.
 * Ideally this format should never change, but it has already needed to change
 * due to some early bug fixes with handling USB MSC device properties.
 *
 * @return
 */
bootloader_format_t query_bootloader_version()
{
    static const version_t FORMAT_2_VERSION = { 0, 9, 2 };

    /*
     * Check the bootloader descriptor block to determine the appropriate
     * format to use when storing the selected firmware file name in settings.
     * Ideally this format should never change, but it has changed due to some
     * early bug fixes with handling USB mass storage device properties.
     */
    const boot_descriptor_t *boot_descriptor = boot_descriptor_get();
    uint32_t calculated_crc;
    version_t boot_version;

    /* Check if there is intentionally no boot descriptor block */
    if (boot_descriptor->crc32 == 0xFFFFFFFFUL || boot_descriptor->crc32 == 0x00000000UL) {
        log_i("Bootloader lacks descriptor block");
        return BOOTLOADER_NO_VERSION;
    }

    /* Log the descriptor properties */
    log_i("Boot version: %s", boot_descriptor->version);
    log_i("Build date: %s", boot_descriptor->build_date);
    log_i("Build describe: %s", boot_descriptor->build_describe);
    log_i("Build checksum: %08lX", __bswap32(boot_descriptor->crc32));

    /* Validate the bootloader */
    calculated_crc =
        HAL_CRC_Calculate(&hcrc, (uint32_t*)BOOTLOADER_ADDRESS, (uint32_t)((BOOTLOADER_SIZE - 4UL) / 4UL));

    if (boot_descriptor->crc32 != calculated_crc) {
        log_w("Bootloader checksum is invalid: %08lX != %08lX", __bswap32(boot_descriptor->crc32), __bswap32(calculated_crc));
        return BOOTLOADER_INVALID;
    }

    /* Check the version string and return the appropriate format version */
    if (!parse_version(&boot_version, boot_descriptor->version)) {
        log_w("Cannot parse bootloader version");
        return BOOTLOADER_INVALID;
    }

    if (compare_versions(&boot_version, &FORMAT_2_VERSION) >= 0) {
        log_d("Bootloader format 2");
        return BOOTLOADER_FORMAT_2;
    } else {
        log_d("Bootloader format 1");
        return BOOTLOADER_FORMAT_1;
    }
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
                /* This check should never fail, but safer to do it anyway */
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

bool query_file_device(const char *file_path, uint8_t *dev_serial, size_t len, bootloader_format_t boot_format)
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

    if (boot_format == BOOTLOADER_NO_VERSION || boot_format == BOOTLOADER_FORMAT_1) {
        /*
         * Format version 1 uses an incorrectly read version of the device
         * serial number, truncated to 20 characters.
         */
        log_d("Serial number format 1");

        memset(dev_serial, 0, len);

        /* Query the device serial number from a fixed index */
        if (!usb_msc_get_serial_fixed_index(dev_num, (char *)dev_serial, len)) {
            return false;
        }
        dev_serial[len - 1] = '\0';
        log_d("Volume serial: %s", dev_serial);

    } else {
        /*
         * Format version 2 uses the unique ID suggestion from the USB MSC
         * spec, which is a binary string composed of the VID, PID, and
         * the end of the serial number.  In this case, the serial number
         * is also correctly read from the device.
         */
        log_d("Serial number format 2");

        if (!usb_msc_get_unique_id(dev_num, dev_serial, len)) {
            return false;
        }

        elog_hexdump("Unique ID", 16, dev_serial, len);

        return true;
    }

    return true;
}