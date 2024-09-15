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

#define FW_FILENAME "DPD500FW.BIN"

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

static void bootloader_task_run(void *argument);
static update_result_t process_firmware_update();
static void bootloader_start_application();
extern void deinit_peripherals();

#ifndef MIN
#define MIN(a, b)  (((a) < (b)) ? (a) : (b))
#endif

osStatus_t bootloader_task_init(void)
{
    bootloader_task_handle = osThreadNew(bootloader_task_run, NULL, &bootloader_task_attributes);
    return osOK;
}

void bootloader_task_run(void *argument)
{
    UNUSED(argument);

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
    bool prompt_visible = false;
    while (1) {
        osDelay(10);
        if (usb_msc_is_mounted()) {
            update_result_t update_result;
            prompt_visible = false;
            update_result = process_firmware_update();
            if (update_result == UPDATE_SUCCESS) {
                osDelay(1000);
                BL_PRINTF("Restarting...\r\n");
                display_graphic_restart();
                osDelay(100);

                /* De-initialize the USB and FatFs components */
                usb_msc_unmount();
                usb_host_deinit();

                /* Start the application firmware */
                osDelay(1000);
                display_clear();
                bootloader_start_application();
            } else {
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

                while(1);
            }
        } else {
            if (prompt_visible) {
                if (keypad_poll() == KEYPAD_CANCEL) {
                    /* Start the application firmware */
                    BL_PRINTF("Restarting...\r\n");
                    display_graphic_restart();
                    osDelay(1000);
                    display_clear();
                    bootloader_start_application();
                }
            } else {
                display_graphic_insert_thumbdrive();
                prompt_visible = true;
            }
        }
    }
}

update_result_t process_firmware_update()
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
            return false;
        }

        /* Open firmware file, if it exists */
        res = f_open(&fp, FW_FILENAME, FA_READ);
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

        display_graphic_update_prompt();

        key_count = 0;
        do {
            osDelay(50);
            if (keypad_poll() == KEYPAD_START) {
                key_count++;
            } else {
                key_count = 0;
            }
            if (key_count > 2) {
                display_graphic_update_progress();
                osDelay(1000);
                break;
            }
        } while (1);

        /* Make sure the USB storage device is still there */
        if (!usb_msc_is_mounted()) {
            result = UPDATE_FILE_READ_ERROR;
            break;
        }

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

        //BL_PRINTF("Firmware programmed\r\n");
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
