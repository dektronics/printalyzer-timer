#include "main_menu.h"

#include <FreeRTOS.h>
#include <task.h>
#include <esp_log.h>

#include <stdio.h>
#include <string.h>

#include "display.h"
#include "keypad.h"
#include "led.h"

#include "menu_settings.h"
#include "menu_enlarger.h"
#include "menu_paper.h"
#include "menu_step_wedge.h"
#include "menu_diagnostics.h"

static const char *TAG = "main_menu";

static menu_result_t menu_about();

menu_result_t main_menu_start(state_controller_t *controller)
{
    ESP_LOGI(TAG, "Main menu");

    menu_result_t menu_result = MENU_OK;
    uint8_t option = 1;

    do {
        option = display_selection_list(
                "Main Menu", option,
                "Settings\n"
                "Enlarger Profiles\n"
                "Paper Profiles\n"
                "Step Wedge Properties\n"
                "Diagnostics\n"
                "About");

        if (option == 1) {
            menu_result = menu_settings();
        } else if (option == 2) {
            menu_result = menu_enlarger_profiles(controller);
        } else if (option == 3) {
            menu_result = menu_paper_profiles(controller);
        } else if (option == 4) {
            menu_result = menu_step_wedge();
        } else if (option == 5) {
            menu_result = menu_diagnostics();
        } else if (option == 6) {
            menu_result = menu_about();
        } else if (option == UINT8_MAX) {
            menu_result = MENU_TIMEOUT;
        }
    } while (option > 0 && menu_result != MENU_TIMEOUT);

    return menu_result;
}

menu_result_t menu_about()
{
    menu_result_t menu_result = MENU_OK;
    char buf[512];

    uint32_t hal_ver = HAL_GetHalVersion();
    uint8_t hal_ver_code = ((uint8_t)(hal_ver)) & 0x0F;
    uint32_t hal_sysclock = HAL_RCC_GetSysClockFreq();

    sprintf(buf,
            "Enlarging Timer & Exposure Meter\n"
            "\n"
            "HAL Version: %d.%d.%d%c\n"
            "FreeRTOS: %s\n"
            "SysClock: %ldMHz",
            ((uint8_t)(hal_ver >> 24)) & 0x0F,
            ((uint8_t)(hal_ver >> 16)) & 0x0F,
            ((uint8_t)(hal_ver >> 8)) & 0x0F,
            (hal_ver_code > 0 ? (char)hal_ver_code : ' '),
            tskKERNEL_VERSION_NUMBER,
            hal_sysclock / 1000000);

    uint8_t option = display_message(
            "Printalyzer",
            NULL,
            buf, " OK ");
    if (option == UINT8_MAX) {
        menu_result = MENU_TIMEOUT;
    }
    return menu_result;
}
