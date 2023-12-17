#include "usb_host.h"

#include <stdio.h>
#include <ff.h>

#include "logger.h"
#include "usbh_core.h"
#include "usbh_msc.h"
#include "fatfs.h"

USBH_HandleTypeDef hUsbHostFS;
static usb_app_state_t app_state = APPLICATION_IDLE;
static bool usb_msc_mounted = false;

static void usb_host_userprocess(USBH_HandleTypeDef *phost, uint8_t id);
static void usb_msc_active();
static void usb_msc_disconnect();

USBH_StatusTypeDef usb_host_init(void)
{
    if (USBH_Init(&hUsbHostFS, usb_host_userprocess, HOST_FS) != USBH_OK) {
        return USBH_FAIL;
    }
    if (USBH_RegisterClass(&hUsbHostFS, USBH_MSC_CLASS) != USBH_OK) {
        return USBH_FAIL;
    }
    if (USBH_Start(&hUsbHostFS) != USBH_OK) {
        return USBH_FAIL;
    }
    return USBH_OK;
}

void usb_host_deinit(void)
{
    USBH_DeInit(&hUsbHostFS);
}

void usb_host_process(void)
{
    USBH_Process(&hUsbHostFS);
}

void usb_host_userprocess(USBH_HandleTypeDef *phost, uint8_t id)
{
    uint8_t active_class = USBH_GetActiveClass(phost);

    switch(id) {
    case HOST_USER_SELECT_CONFIGURATION:
        break;

    case HOST_USER_DISCONNECTION:
        app_state = APPLICATION_DISCONNECT;
        if (active_class == USB_MSC_CLASS) {
            usb_msc_disconnect();
        }
        break;

    case HOST_USER_CLASS_ACTIVE:
        app_state = APPLICATION_READY;
        if (active_class == USB_MSC_CLASS) {
            usb_msc_active();
        }
        break;

    case HOST_USER_CONNECTION:
        app_state = APPLICATION_START;
        break;

    default:
        break;
    }
}

void usb_msc_active()
{
    BL_PRINTF("usb_msc_active\r\n");

    if (fatfs_mount() != HAL_OK) {
        return;
    }
    usb_msc_mounted = true;

    FRESULT res;

    char str[12];
    res = f_getlabel("0:", str, 0);
    if (res == FR_OK) {
        BL_PRINTF("Drive label: \"%s\"\r\n", str);
    }

    FATFS *fs;
    DWORD free_clusters, free_sectors, total_sectors;
    res = f_getfree("0:", &free_clusters, &fs);
    if (res == FR_OK) {
        total_sectors = (fs->n_fatent - 2) * fs->csize;
        free_sectors = free_clusters * fs->csize;
        BL_PRINTF("Drive has %ld KB total space, %ld KB available\r\n", total_sectors / 2, free_sectors / 2);
    }
}

void usb_msc_disconnect()
{
    BL_PRINTF("usb_msc_disconnect\r\n");
    fatfs_unmount();
    usb_msc_mounted = false;
}

bool usb_msc_is_mounted()
{
    return usb_msc_mounted;
}

void usb_msc_unmount()
{
    if (usb_msc_mounted) {
        fatfs_unmount();
        usb_msc_mounted = false;
    }
}
