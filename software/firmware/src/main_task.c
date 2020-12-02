#include "main_task.h"

#include <FreeRTOS.h>
#include <task.h>
#include <cmsis_os.h>

#include <stdio.h>

#include "usb_host.h"
#include "board_config.h"

osThreadId_t main_task_handle;

static void main_task_start(void *argument);

const osThreadAttr_t main_task_attributes = {
    .name = "main_task",
    .priority = (osPriority_t) osPriorityNormal,
    .stack_size = 2048 * 4
};

void main_task_init(void)
{
    main_task_handle = osThreadNew(main_task_start, NULL, &main_task_attributes);
}

void main_task_start(void *argument)
{
    UNUSED(argument);

    if (usb_host_init() != USBH_OK) {
        printf("Unable to initialize USB host\r\n");
    }

    printf("---- STM32 Startup ----\r\n");
    uint32_t hal_ver = HAL_GetHalVersion();
    uint8_t hal_ver_code = ((uint8_t)(hal_ver)) & 0x0F;
    uint32_t hal_sysclock = HAL_RCC_GetSysClockFreq();
    printf("HAL Version: %d.%d.%d%c\r\n",
        ((uint8_t)(hal_ver >> 24)) & 0x0F,
        ((uint8_t)(hal_ver >> 16)) & 0x0F,
        ((uint8_t)(hal_ver >> 8)) & 0x0F,
        hal_ver_code > 0 ? (char)hal_ver_code : ' ');
    printf("Revision ID: %ld\r\n", HAL_GetREVID());
    printf("Device ID: 0x%lX\r\n", HAL_GetDEVID());
    printf("FreeRTOS: %s\r\n", tskKERNEL_VERSION_NUMBER);
    printf("SysClock: %ldMHz\r\n", hal_sysclock / 1000000);

    uint8_t *uniqueId = (uint8_t*)0x1FFF7A10;
    printf("Unique ID: %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\r\n",
            uniqueId[0], uniqueId[1], uniqueId[2], uniqueId[3], uniqueId[4],
            uniqueId[5], uniqueId[6], uniqueId[7], uniqueId[8], uniqueId[9],
            uniqueId[10], uniqueId[11]);

    printf("-----------------------\r\n");

}
