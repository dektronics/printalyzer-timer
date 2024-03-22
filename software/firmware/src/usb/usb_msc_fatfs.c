#include "usb_msc_fatfs.h"

#include <stdint.h>
#include <ff.h>

#include "usbh_core.h"
#include "usbh_msc.h"
#include "fatfs.h"

#define LOG_TAG "usb_msc"
#include <elog.h>

static struct usbh_msc *active_msc_class = NULL;
static bool usb_msc_mounted = false;

bool usbh_msc_fatfs_init()
{
    return true;
}

void usbh_msc_fatfs_attached(struct usbh_msc *msc_class)
{
    //TODO Need to make changes to support multiple MSC devices

    if (active_msc_class || msc_class->sdchar != 'a') {
        log_w("Multiple MSC devices not supported");
        return;
    }

    active_msc_class = msc_class;

    if (fatfs_mount() != HAL_OK) {
        return;
    }

    FRESULT res;

    char str[12];
    res = f_getlabel("0:", str, 0);
    if (res == FR_OK) {
        log_i("Drive label: \"%s\"", str);
    }

    FATFS *fs;
    DWORD free_clusters, free_sectors, total_sectors;
    res = f_getfree("0:", &free_clusters, &fs);
    if (res == FR_OK) {
        total_sectors = (fs->n_fatent - 2) * fs->csize;
        free_sectors = free_clusters * fs->csize;
        log_i("Drive has %ld KB total space, %ld KB available", total_sectors / 2, free_sectors / 2);
    }

    usb_msc_mounted = true;
}

void usbh_msc_fatfs_detached(struct usbh_msc *msc_class)
{
    if (msc_class == active_msc_class) {
        usb_msc_mounted = false;
        fatfs_unmount();
        active_msc_class = NULL;
    }
}

bool usbh_msc_is_mounted()
{
    return usb_msc_mounted;
}
