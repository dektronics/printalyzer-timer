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

#define FW_FILENAME "printalyzer-fw.bin"

extern SPI_HandleTypeDef hspi1;
extern CRC_HandleTypeDef hcrc;

osThreadId_t bootloader_task_handle = NULL;
const osThreadAttr_t bootloader_task_attributes = {
    .name = "bootloader",
    .stack_size = 2048 * 4,
    .priority = (osPriority_t)osPriorityNormal,
};

static void bootloader_task_run(void *argument);
static bool process_firmware_update();
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
            prompt_visible = false;
            if (process_firmware_update()) {
                osDelay(1000);
                display_static_message("Restarting...\r\n");
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
                display_static_message("Firmware update failed");
                while(1);
            }
        } else {
            if (prompt_visible) {
                if (keypad_poll() == KEYPAD_CANCEL) {
                    /* Start the application firmware */
                    display_static_message("Restarting...\r\n");
                    osDelay(1000);
                    display_clear();
                    bootloader_start_application();
                }
            } else {
                display_static_message(
                    "Insert a USB storage device to\n"
                    "install updated Printalyzer\n"
                    "firmware.");
                prompt_visible = true;
            }
        }
    }
}

bool process_firmware_update()
{
    display_static_message("Checking for firmware...");

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
    bool success = false;

    do {
        /* Check for flash write protection */
        if (bootloader_get_protection_status() & BL_PROTECTION_WRP) {
            BL_PRINTF("Flash write protection enabled\r\n");
            display_static_message(
                "Flash is write protected,\n"
                "cannot update firmware.\n"
                "Press Start button to disable\n"
                "write protection and restart.");

            key_count = 0;
            for (int i = 0; i < 200; i++) {
                osDelay(50);
                if (keypad_poll() == KEYPAD_START) {
                    key_count++;
                } else {
                    key_count = 0;
                }
                if (key_count > 2) {
                    display_static_message(
                        "Disabling write protection\n"
                        "and restarting...");
                    if (bootloader_config_protection(BL_PROTECTION_NONE) != BL_OK) {
                        display_static_message(
                            "Unable to disable\n"
                            "write protection!");
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
            break;
        }
        file_open = true;

        /* Check size of the firmware file */
        if (bootloader_check_size(f_size(&fp)) != BL_OK) {
            BL_PRINTF("Firmware size is invalid: %lu\r\n", f_size(&fp));
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
            break;
        }
        res = f_read(&fp, &image_descriptor, sizeof(app_descriptor_t), &bytes_read);
        if (res != FR_OK || bytes_read != sizeof(app_descriptor_t)) {
            BL_PRINTF("Unable to read the firmware file descriptor: %d\r\n", res);
            break;
        }

        f_rewind(&fp);

        if (calculated_crc != image_descriptor.crc32) {
            BL_PRINTF("Firmware checksum mismatch: %08lX != %08lX\r\n",
                image_descriptor.crc32, calculated_crc);
            break;
        }
        BL_PRINTF("Firmware checksum is okay.\r\n");

        if (image_descriptor.magic_word == APP_DESCRIPTOR_MAGIC_WORD) {
            char msg_buf[256];
            BL_SPRINTF(msg_buf,
                "Firmware found:\n"
                "\n"
                "%s\n"
                "%s (%s)\n"
                "\n"
                "Press Start button to proceed\n"
                "with the update.",
                image_descriptor.project_name,
                image_descriptor.version,
                image_descriptor.build_describe);
            display_static_message(msg_buf);
        } else {
            display_static_message(
                "Firmware found.\n"
                "\n"
                "Press Start button to proceed\n"
                "with the update.");
        }

        key_count = 0;
        do {
            osDelay(50);
            if (keypad_poll() == KEYPAD_START) {
                key_count++;
            } else {
                key_count = 0;
            }
            if (key_count > 2) {
                display_static_message("Starting firmware update...");
                osDelay(1000);
                break;
            }
        } while (1);

        /* Make sure the USB storage device is still there */
        if (!usb_msc_is_mounted()) {
            break;
        }

        /* Initialize bootloader and prepare for flash programming */
        bootloader_init();

        /* Erase existing flash contents */
        display_static_message("Erasing flash...");
        if (bootloader_erase() != BL_OK) {
            BL_PRINTF("Error erasing flash\r\n");
            display_static_message("Unable to erase flash!");
            break;
        }
        BL_PRINTF("Flash erased\r\n");

        /* Start programming */
        display_static_message("Programming firmware...");
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
                    break;
                }
            }
        } while (res == FR_OK && bytes_read > 0);

        bootloader_flash_end();

        if (status != BL_OK || res != FR_OK) {
            display_static_message("Programming error!");
            break;
        }

        BL_PRINTF("Firmware programmed\r\n");

        f_rewind(&fp);

        /* Verify flash content */
        display_static_message("Verifying firmware...");

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
        } while (res == FR_OK && bytes_read > 0);

        if (status != BL_OK || res != FR_OK) {
            display_static_message("Verification error!");
            break;
        }
        BL_PRINTF("Firmware verified\r\n");

        display_static_message("Verification complete");
        osDelay(1000);

#if(USE_WRITE_PROTECTION)
        /* Enable flash write protection */
        display_static_message(
            "Enabling write protection\n"
            "and restarting...");
        delay_with_usb(1000);
        if (bootloader_config_protection(BL_PROTECTION_WRP) != BL_OK) {
            display_static_message(
                "Unable to enable\n"
                "write protection!");
            while(1) { }
        }
#endif

        success = true;

    } while (0);

    if (file_open) {
        f_close(&fp);
    }

    return success;
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
