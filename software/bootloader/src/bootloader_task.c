#include "bootloader_task.h"

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ff.h>
#include <cmsis_os.h>

#include "stm32f4xx_hal.h"

#include "logger.h"
#include "board_config.h"
#include "usb_host.h"
#include "keypad.h"
#include "display.h"
#include "bootloader.h"
#include "board_config.h"
#include "app_descriptor.h"
#include "m24m01.h"

#define FW_FILENAME "0:/DPD500FW.BIN"

/* EEPROM properties for the bootloader block */
#define PAGE_BOOTLOADER                  0x1FE00UL
#define BOOTLOADER_COMMAND               0   /* 1B (uint8_t) */
#define BOOTLOADER_FW_DEVICE             1   /* 21B (char[21]) */
#define BOOTLOADER_FW_CHECKSUM           22  /* 4B (uint32_t) */
#define BOOTLOADER_FW_FILE               256 /* char[256] */
#define EEPROM_PAGE_SIZE                 0x00100UL

extern I2C_HandleTypeDef hi2c1;
extern SPI_HandleTypeDef hspi1;
extern CRC_HandleTypeDef hcrc;

osThreadId_t bootloader_task_handle = NULL;
const osThreadAttr_t bootloader_task_attributes = {
    .name = "bootloader",
    .stack_size = 2048 * 4,
    .priority = (osPriority_t)osPriorityNormal,
};

typedef enum {
    UPDATE_SUCCESS = 0,
    UPDATE_FILE_NOT_FOUND,
    UPDATE_FILE_READ_ERROR,
    UPDATE_FILE_BAD,
    UPDATE_FLASH_ERROR,
    UPDATE_FAILED
} update_result_t;

typedef struct {
    uint8_t command;
    char dev_serial[21];
    uint32_t checksum;
    char file_path[256];
} bootloader_block_t;

static void bootloader_task_run(void *argument);
static void bootloader_loop_user_button();
static void bootloader_loop_firmware_trigger();
static void bootloader_loop_checksum_fail();
static bool read_bootloader_block(bootloader_block_t *block);
static update_result_t process_firmware_update(const char *file_path, uint32_t file_checksum);
static void bootloader_update_complete();
static void bootloader_update_failed(update_result_t update_result);
static void bootloader_start_application();
extern void deinit_peripherals();

#ifndef MIN
#define MIN(a, b)  (((a) < (b)) ? (a) : (b))
#endif

osStatus_t bootloader_task_init(bootloader_trigger_t trigger)
{
    const uint32_t param = (uint32_t)trigger;
    bootloader_task_handle = osThreadNew(bootloader_task_run, (void *)param, &bootloader_task_attributes);
    return osOK;
}

void bootloader_task_run(void *argument)
{
    const bootloader_trigger_t trigger = (bootloader_trigger_t)((uint32_t)argument);

    /* Initialize the display */
    const u8g2_display_handle_t display_handle = {
        .hspi = &hspi1,
        .reset_gpio_port = DISP_RESET_GPIO_Port,
        .reset_gpio_pin = DISP_RESET_Pin,
        .cs_gpio_port = DISP_CS_GPIO_Port,
        .cs_gpio_pin = DISP_CS_Pin,
        .dc_gpio_port = DISP_DC_GPIO_Port,
        .dc_gpio_pin = DISP_DC_Pin
    };
    display_init(&display_handle);
    display_enable(true);
    display_draw_test_pattern();

    /*
     * Initialize the USB host framework.
     * This framework does have a dedicated management task, but it is
     * not handled as part of the application code.
     */
    if (!usb_host_init()) {
        BL_PRINTF("Unable to initialize USB host\r\n");
    } else {
        if (!usb_hub_init()) {
            BL_PRINTF("Unable to initialize USB hub\r\n");
        }
    }

    /* Main loop */
    if (trigger == TRIGGER_USER_BUTTON) {
        bootloader_loop_user_button();
    } else if (trigger == TRIGGER_FIRMWARE) {
        bootloader_loop_firmware_trigger();
    } else if (trigger == TRIGGER_CHECKSUM_FAIL) {
        bootloader_loop_checksum_fail();
    }

    bootloader_update_complete();
}

void bootloader_loop_user_button()
{
    /*
     * Bootloader process when started by the recovery button:
     * - Wait for a USB device to be attached
     * - Check for a file with a known name
     * - Try to install this file
     */

    bool prompt_visible = false;
    while (1) {
        osDelay(10);
        if (usb_msc_is_mounted()) {
            update_result_t update_result;
            prompt_visible = false;

            update_result = process_firmware_update(FW_FILENAME, 0);

            if (update_result == UPDATE_SUCCESS) {
                return;
            } else {
                bootloader_update_failed(update_result);
                while(1);
            }

        } else {
            if (prompt_visible) {
                if (keypad_poll() == KEYPAD_CANCEL) {
                    return;
                }
            } else {
                display_graphic_insert_thumbdrive();
                prompt_visible = true;
            }
        }
    }

}

void bootloader_loop_firmware_trigger()
{
    /*
     * Bootloader process when started by the firmware update menu:
     * - Check the EEPROM for the bootloader block
     * - Poll attached USB devices for the file described in this block
     * - Try to install that file
     */

    bootloader_block_t block;
    uint32_t start_time;
    int dev_num = -1;
    char full_path[260];
    update_result_t update_result;

    /* Read the bootloader block */
    if (!read_bootloader_block(&block)) {
        return;
    }

    /* Check for the expected command code */
    if (block.command != 0xBB) {
        return;
    }

    display_graphic_checking_thumbdrive();

    /* Wait 10s for the expected thumbdrive to be inserted */
    start_time = osKernelGetTickCount();
    do {
        osDelay(100);
        dev_num = usb_msc_find_device(block.dev_serial);
        if (dev_num >= 0) { break; }
    } while(start_time + 10000 > osKernelGetTickCount());

    /* Device was not found */
    if (dev_num < 0) {
        bootloader_update_failed(UPDATE_FILE_NOT_FOUND);
        while(1);
    }

    /* Construct the full file path */
    sprintf_(full_path, "%d:/%s", dev_num, block.file_path);

    update_result = process_firmware_update(full_path, block.checksum);
    if (update_result != UPDATE_SUCCESS) {
        bootloader_update_failed(update_result);
        while(1);
    }
}

void bootloader_loop_checksum_fail()
{
    /*
     * Bootloader process when started by a firmware checksum failure:
     * - Check EEPROM for the bootloader block
     * - If block exists, look for the mentioned file
     * - If mentioned file cannot be found, fall back to the known-name file
     * - If block doesn't exist, look for the known-name file
     */

    bootloader_block_t block;
    uint32_t start_time;
    int dev_num = -1;
    char full_path[260];
    update_result_t update_result;

    /* Check for the bootloader block */
    if (read_bootloader_block(&block) && block.command == 0xBB) {

        display_graphic_checking_thumbdrive();

        /* Wait 10s for the expected thumbdrive to be inserted */
        start_time = osKernelGetTickCount();
        do {
            osDelay(100);
            dev_num = usb_msc_find_device(block.dev_serial);
            if (dev_num >= 0) { break; }
        } while(start_time + 10000 < osKernelGetTickCount());

        if (dev_num >= 0) {
            /* Construct the full file path */
            sprintf_(full_path, "%d:/%s", dev_num, block.file_path);

            update_result = process_firmware_update(full_path, block.checksum);
            if (update_result != UPDATE_SUCCESS) {
                bootloader_update_failed(update_result);
                osDelay(1000);
            }
        }
    }

    /* If we got here, act as if the button was held on startup */
    bootloader_loop_user_button();
}

static uint32_t copy_to_u32(const uint8_t *buf)
{
    return (uint32_t)buf[0] << 24
        | (uint32_t)buf[1] << 16
        | (uint32_t)buf[2] << 8
        | (uint32_t)buf[3];
}

bool read_bootloader_block(bootloader_block_t *block)
{
    HAL_StatusTypeDef ret = HAL_OK;
    uint8_t data[EEPROM_PAGE_SIZE];

    if (!block) { return false; }

    memset(block, 0, sizeof(bootloader_block_t));

    /* Read the first page, which contains commands and metadata */
    ret = m24m01_read_buffer(&hi2c1, PAGE_BOOTLOADER, data, sizeof(data));
    if (ret != HAL_OK) {
        BL_PRINTF("Unable to read bootloader block: %d\r\n", ret);
        return false;
    }

    /* If a command byte isn't populated, skip the rest of this */
    if (data[BOOTLOADER_COMMAND] == 0 || data[BOOTLOADER_COMMAND] == 0xFF) {
        return false;
    }

    block->command = data[BOOTLOADER_COMMAND];

    strncpy(block->dev_serial, (char *)(data + BOOTLOADER_FW_DEVICE), 21);
    block->dev_serial[20] = '\0';

    block->checksum = copy_to_u32(data + BOOTLOADER_FW_CHECKSUM);

    ret = m24m01_read_buffer(&hi2c1, PAGE_BOOTLOADER + BOOTLOADER_FW_FILE, data, sizeof(data));
    if (ret != HAL_OK) {
        BL_PRINTF("Unable to read bootloader block: %d\r\n", ret);
        return false;
    }

    strncpy(block->file_path, (char *)data, EEPROM_PAGE_SIZE);
    data[EEPROM_PAGE_SIZE - 1] = '\0';

    if (block->command != 0 && block->command != 0xFF) {
        return true;
    } else {
        return false;
    }
}

update_result_t process_firmware_update(const char *file_path, uint32_t file_checksum)
{
    display_graphic_checking_thumbdrive();

    FRESULT res;
    FIL fp;
    bool file_open = false;
    uint8_t buf[512];
    UINT bytes_remaining;
    UINT bytes_to_read;
    UINT bytes_read;
    uint32_t calculated_crc = 0;
    app_descriptor_t image_descriptor = {0};
    uint32_t *flash_ptr;
    uint32_t data;
    uint32_t counter;
    int key_count = 0;
    bootloader_status_t status = BL_OK;
    update_result_t result = UPDATE_FAILED;

    do {
        /* Check for flash write protection */
        if (bootloader_get_protection_status() & BL_PROTECTION_WRP) {
            BL_PRINTF("Flash write protection enabled\r\n");
            display_graphic_unlock_prompt();

            key_count = 0;
            for (int i = 0; i < 200; i++) {
                osDelay(50);
                if (keypad_poll() == KEYPAD_START) {
                    key_count++;
                } else {
                    key_count = 0;
                }
                if (key_count > 2) {
                    BL_PRINTF("Disabling write protection and restarting...\r\n");
                    display_graphic_restart();
                    if (bootloader_config_protection(BL_PROTECTION_NONE) != BL_OK) {
                        BL_PRINTF("Unable to disable write protection!\r\n");
                        display_graphic_failure();
                    }
                    while(1) { }
                }
            }
            return UPDATE_FAILED;
        }

        /* Open firmware file, if it exists */
        res = f_open(&fp, file_path, FA_READ);
        if (res != FR_OK) {
            BL_PRINTF("Unable to open firmware file: %d\r\n", res);
            result = UPDATE_FILE_NOT_FOUND;
            break;
        }
        file_open = true;

        /* Check size of the firmware file */
        if (bootloader_check_size(f_size(&fp)) != BL_OK) {
            BL_PRINTF("Firmware size is invalid: %lu\r\n", f_size(&fp));
            result = UPDATE_FILE_BAD;
            break;
        }
        BL_PRINTF("Firmware size is okay.\r\n");

        /* Calculate the firmware file checksum */
        __HAL_CRC_DR_RESET(&hcrc);
        bytes_remaining = f_size(&fp) - 4;
        do {
            bytes_to_read = MIN(sizeof(buf), bytes_remaining);
            res = f_read(&fp, buf, bytes_to_read, &bytes_read);
            if (res == FR_OK) {
                /* This check should never fail, but safer to do it anyways */
                if ((bytes_read % 4) != 0) {
                    BL_PRINTF("Bytes read are not word aligned\r\n");
                    result = UPDATE_FILE_READ_ERROR;
                    break;
                }

                calculated_crc = HAL_CRC_Accumulate(&hcrc, (uint32_t *)buf, bytes_read / 4);

                bytes_remaining -= bytes_read;

                /* Break if at EOF */
                if (bytes_read < bytes_to_read) {
                    break;
                }
            } else {
                BL_PRINTF("File read error: %d\r\n", res);
                break;
            }
        } while (res == FR_OK && bytes_remaining > 0);

        __HAL_RCC_CRC_FORCE_RESET();
        __HAL_RCC_CRC_RELEASE_RESET();

        /* Read the app descriptor from the end of the firmware file */
        res = f_lseek(&fp, f_tell(&fp) - (sizeof(app_descriptor_t) - 4));
        if (res != FR_OK) {
            BL_PRINTF("Unable to seek to read the firmware file descriptor: %d\r\n", res);
            result = UPDATE_FILE_READ_ERROR;
            break;
        }
        res = f_read(&fp, &image_descriptor, sizeof(app_descriptor_t), &bytes_read);
        if (res != FR_OK || bytes_read != sizeof(app_descriptor_t)) {
            BL_PRINTF("Unable to read the firmware file descriptor: %d\r\n", res);
            result = UPDATE_FILE_READ_ERROR;
            break;
        }

        f_rewind(&fp);

        if (calculated_crc != image_descriptor.crc32) {
            BL_PRINTF("Firmware checksum mismatch: %08lX != %08lX\r\n",
                image_descriptor.crc32, calculated_crc);
            result = UPDATE_FILE_BAD;
            break;
        }
        BL_PRINTF("Firmware checksum is okay.\r\n");

        if (image_descriptor.magic_word != APP_DESCRIPTOR_MAGIC_WORD) {
            BL_PRINTF("Bad magic\r\n");
            result = UPDATE_FILE_BAD;
            break;
        }

        if (file_checksum > 0 && file_checksum != image_descriptor.crc32) {
            BL_PRINTF("Firmware checksum unexpected: %08lX != %08lX\r\n",
            image_descriptor.crc32, file_checksum);
            result = UPDATE_FILE_BAD;
            break;
        }

        /* Only prompt if loading the fallback file */
        if (file_checksum == 0) {
            display_graphic_update_prompt();

            key_count = 0;
            do {
                keypad_key_t key;
                osDelay(50);
                key = keypad_poll();
                if (key == KEYPAD_START) {
                    key_count++;
                } else if (key == KEYPAD_CANCEL) {
                    f_close(&fp);
                    return UPDATE_FAILED;
                } else {
                    key_count = 0;
                }
                if (key_count > 2) {
                    break;
                }
            } while (1);
        }
        display_graphic_update_progress();
        osDelay(500);

        /* Initialize bootloader and prepare for flash programming */
        bootloader_init();

        /* Erase existing flash contents */
        BL_PRINTF("Erasing flash...\r\n");
        display_graphic_update_progress_increment(5);

        if (bootloader_erase() != BL_OK) {
            BL_PRINTF("Error erasing flash\r\n");
            display_graphic_update_progress_failure();
            result = UPDATE_FLASH_ERROR;
            break;
        }
        BL_PRINTF("Flash erased\r\n");

        /* Start programming */
        BL_PRINTF("Programming firmware...\r\n");
        display_graphic_update_progress_increment(10);
        counter = 0;
        bootloader_flash_begin();
        do {
            data = 0xFFFFFFFF;
            res = f_read(&fp, &data, 4, &bytes_read);
            if (res == FR_OK && bytes_read > 0) {
                status = bootloader_flash_next(data);
                if (status == BL_OK) {
                    counter++;
                } else {
                    BL_PRINTF("Programming error at byte %lu\r\n", (counter * 4));
                    result = UPDATE_FLASH_ERROR;
                    break;
                }
            }
            if ((counter % 1024) == 0) {
                uint8_t value = 10 + ((150 * counter) / APP_SIZE);
                if (value < 10) { value = 10; }
                else if (value > 160) { value = 160; }
                display_graphic_update_progress_increment(value);
            }
        } while (res == FR_OK && bytes_read > 0);

        bootloader_flash_end();

        if (status != BL_OK || res != FR_OK) {
            BL_PRINTF("Programming error!\r\n");
            display_graphic_update_progress_failure();
            if (res != FR_OK) {
                result = UPDATE_FILE_READ_ERROR;
            } else {
                result = UPDATE_FLASH_ERROR;
            }
            break;
        }

        BL_PRINTF("Firmware programmed\r\n");
        display_graphic_update_progress_increment(150);

        f_rewind(&fp);

        /* Verify flash content */
        BL_PRINTF("Verifying firmware...\r\n");

        flash_ptr = (uint32_t*)APP_ADDRESS;
        counter = 0;
        status = BL_OK;
        do {
            res = f_read(&fp, buf, sizeof(buf), &bytes_read);
            if (res != FR_OK) {
                BL_PRINTF("File read error: %d\r\n", res);
                break;
            }
            if (memcmp(flash_ptr, buf, bytes_read) == 0) {
                counter += bytes_read;
                flash_ptr += bytes_read / 4;
            } else {
                BL_PRINTF("Verify error at byte %lu\r\n", counter);
                status = BL_CHKS_ERROR;
                break;
            }
            if ((counter % 1024) == 0) {
                uint8_t value = 160 + ((50 * counter) / APP_SIZE);
                if (value < 160) { value = 160; }
                else if (value > 210) { value = 210; }
                display_graphic_update_progress_increment(value);
            }
        } while (res == FR_OK && bytes_read > 0);

        if (status != BL_OK || res != FR_OK) {
            BL_PRINTF("Verification error!\r\n");
            display_graphic_update_progress_failure();
            if (res != FR_OK) {
                result = UPDATE_FILE_READ_ERROR;
            } else {
                result = UPDATE_FLASH_ERROR;
            }
            break;
        }
        BL_PRINTF("Firmware verified\r\n");

        BL_PRINTF("Verification complete\r\n");
        display_graphic_update_progress_increment(210);
        display_graphic_update_progress_success();
        osDelay(1000);

#if(USE_WRITE_PROTECTION)
        /* Enable flash write protection */
        BL_PRINTF("Enabling write protection and restarting...\r\n");
        display_graphic_restart();
        osDelay(1000);
        if (bootloader_config_protection(BL_PROTECTION_WRP) != BL_OK) {
            BL_PRINTF("Unable to enable write protection!\r\n");
            display_graphic_failure();
            while(1) { }
        }
#endif

        result = UPDATE_SUCCESS;

    } while (0);

    if (file_open) {
        f_close(&fp);
    }

    return result;
}

void bootloader_update_complete()
{
    osDelay(1000);
    BL_PRINTF("Restarting...\r\n");
    display_graphic_restart();
    osDelay(100);

    /* De-initialize the USB and FatFs components */
    usb_msc_unmount();

    /* Start the application firmware */
    osDelay(1000);
    display_clear();
    bootloader_start_application();
}

void bootloader_update_failed(update_result_t update_result)
{
    osDelay(1000);
    BL_PRINTF("Firmware update failed\r\n");
    switch (update_result) {
    case UPDATE_FILE_NOT_FOUND:
        display_graphic_file_missing();
        break;
    case UPDATE_FILE_READ_ERROR:
        display_graphic_file_read_error();
        break;
    case UPDATE_FILE_BAD:
        display_graphic_file_bad();
        break;
    case UPDATE_FLASH_ERROR:
    case UPDATE_FAILED:
    default:
        display_graphic_failure();
        break;
    }
}

void bootloader_start_application()
{
    /* Shutdown the USB host stack */
    usb_host_deinit();

    /* Wait for things to settle */
    osDelay(200);

    /* Suspend the FreeRTOS scheduler */
    vTaskSuspendAll();

    /* Deinitialize the peripherals */
    deinit_peripherals();

    /* Trigger a restart */
    NVIC_SystemReset();
}
