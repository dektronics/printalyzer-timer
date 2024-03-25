#include "usb_msc_fatfs.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <ff.h>
#include <ff_gen_drv.h>

#include "usbh_core.h"
#include "usbh_msc.h"

#define LOG_TAG "usb_msc"
#include <elog.h>

#ifndef __CDT_PARSER__
_Static_assert(CONFIG_USBHOST_MAX_MSC_CLASS == FF_VOLUMES, "CherryUSB and FATFS MSC counts should match");
#endif

typedef struct {
    struct usbh_msc *msc_class;
    char usbh_path[4]; /* USBH logical drive path */
    bool linked;
    bool mounted;
    bool ready;
    FATFS usbh_fatfs;  /* File system object for USBH logical drive */
} usbh_msc_fatfs_handle_t;

static usbh_msc_fatfs_handle_t msc_handles[CONFIG_USBHOST_MAX_MSC_CLASS];

static DSTATUS fatfs_diskio_initialize(BYTE lun);
static DSTATUS fatfs_diskio_status(BYTE lun);
static DRESULT fatfs_diskio_read(BYTE lun, BYTE *buff, DWORD sector, UINT count);
static DRESULT fatfs_diskio_write(BYTE lun, const BYTE *buff, DWORD sector, UINT count);
static DRESULT fatfs_diskio_ioctl(BYTE lun, BYTE cmd, void *buff);

static const Diskio_drvTypeDef fatfs_usbh_msc_driver = {
    fatfs_diskio_initialize,
    fatfs_diskio_status,
    fatfs_diskio_read,
    fatfs_diskio_write,
    fatfs_diskio_ioctl
};

bool usbh_msc_fatfs_init()
{
    memset(msc_handles, 0, sizeof(usbh_msc_fatfs_handle_t));
    return true;
}

void usbh_msc_fatfs_attached(struct usbh_msc *msc_class)
{
    int ret;
    FRESULT fres;
    const uint8_t devno = msc_class->sdchar - 'a';

    if (devno > CONFIG_USBHOST_MAX_MSC_CLASS) {
        log_w("Invalid device letter '%c'", msc_class->sdchar);
        return;
    }

    usbh_msc_fatfs_handle_t *handle = &msc_handles[devno];

    if (handle->msc_class) {
        log_w("Device already attached");
        return;
    }

    /* Initialize handle state */
    handle->msc_class = msc_class;
    handle->linked = false;
    handle->mounted = false;
    handle->ready = false;
    memset(&handle->usbh_fatfs, 0, sizeof(FATFS));

    /* Link the driver callbacks */
    ret = FATFS_LinkDriverEx(&fatfs_usbh_msc_driver, handle->usbh_path, devno);
    if (ret != 0) {
        log_w("Unable to link USB driver to FATFS");
        return;
    }
    handle->linked = true;

    /* Mount the volume */
    fres = f_mount(&handle->usbh_fatfs, (TCHAR const*)handle->usbh_path, 0);
    if (fres != FR_OK) {
        log_e("f_mount error: %d", fres);
        return;
    }

    handle->mounted = true;

    /* Do some initial operations to make sure the volume works */
    char str[12];
    fres = f_getlabel((TCHAR const*)handle->usbh_path, str, 0);
    if (fres == FR_OK) {
        log_i("Drive label: \"%s\"", str);
    }

    FATFS *fs;
    DWORD free_clusters, free_sectors, total_sectors;
    fres = f_getfree((TCHAR const*)handle->usbh_path, &free_clusters, &fs);
    if (fres == FR_OK) {
        total_sectors = (fs->n_fatent - 2) * fs->csize;
        free_sectors = free_clusters * fs->csize;
        log_i("Drive has %ld KB total space, %ld KB available", total_sectors / 2, free_sectors / 2);
    }

    handle->ready = true;
}

void usbh_msc_fatfs_detached(struct usbh_msc *msc_class)
{
    int ret;
    FRESULT fres;
    const uint8_t devno = msc_class->sdchar - 'a';

    if (devno > CONFIG_USBHOST_MAX_MSC_CLASS) {
        log_w("Invalid device letter '%c'", msc_class->sdchar);
        return;
    }

    usbh_msc_fatfs_handle_t *handle = &msc_handles[devno];

    if (!handle->msc_class) {
        log_w("Device not attached");
        return;
    }

    handle->ready = false;

    if (handle->mounted) {
        fres = f_mount(0, (TCHAR const*)handle->usbh_path, 0);
        if (fres != FR_OK) {
            log_e("f_unmount error: %d", fres);
        }
        memset(&handle->usbh_fatfs, 0, sizeof(FATFS));
        handle->mounted = false;
    }

    if (handle->linked) {
        ret = FATFS_UnLinkDriverEx(handle->usbh_path, devno);
        if (ret != 0) {
            log_w("Unable to unlink USB driver from FATFS");
        }
        handle->linked = false;
    }

    handle->msc_class = NULL;
}

bool usbh_msc_is_mounted()
{
    for (uint8_t i = 0; i < CONFIG_USBHOST_MAX_MSC_CLASS; i++) {
        if (msc_handles[i].ready) { return true; }
    }
    return false;
}

/**
 * Initializes access to a drive
 * @param lun logical unit id
 * @return Operation status
 */
DSTATUS fatfs_diskio_initialize(BYTE lun)
{
    return RES_OK;
}

/**
 * Gets disk status
 * @param lun logical unit id
 * @return Operation status
 */
DSTATUS fatfs_diskio_status(BYTE lun)
{
    /* Not sure we currently have an equivalent way of checking this */
    return RES_OK;
}

/**
 * Read sectors
 * @param lun logical unit id
 * @param buff Data buffer to store read data
 * @param sector Sector address (LBA)
 * @param count Number of sectors to read (1..128)
 * @return Operation result
 */
DRESULT fatfs_diskio_read(BYTE lun, BYTE *buff, DWORD sector, UINT count)
{
    if (lun >= CONFIG_USBHOST_MAX_MSC_CLASS || !msc_handles[lun].msc_class) {
        return RES_ERROR;
    }
    return usbh_msc_scsi_read10(msc_handles[lun].msc_class, sector, buff, count);
}

/**
 * Write sectors
 * @param lun logical unit id
 * @param buff Data to be written
 * @param sector Sector address (LBA)
 * @param count Number of sectors to write (1..128)
 * @return Operation result
 */
DRESULT fatfs_diskio_write(BYTE lun, const BYTE *buff, DWORD sector, UINT count)
{
    if (lun >= CONFIG_USBHOST_MAX_MSC_CLASS || !msc_handles[lun].msc_class) {
        return RES_ERROR;
    }
    return usbh_msc_scsi_write10(msc_handles[lun].msc_class, sector, buff, count);
}

/**
 * I/O control operation
 * @param lun logical unit id
 * @param cmd Control code
 * @param buff Buffer to send/receive control data
 * @return Operation result
 */
DRESULT fatfs_diskio_ioctl(BYTE lun, BYTE cmd, void *buff)
{
    if (lun >= CONFIG_USBHOST_MAX_MSC_CLASS || !msc_handles[lun].msc_class) {
        return RES_ERROR;
    }
    struct usbh_msc *msc_class = msc_handles[lun].msc_class;

    int result = 0;

    switch (cmd) {
    case CTRL_SYNC:
        result = RES_OK;
        break;
    case GET_SECTOR_SIZE:
        *(WORD *) buff = msc_class->blocksize;
        result = RES_OK;
        break;
    case GET_BLOCK_SIZE:
        *(DWORD *) buff = 1;
        result = RES_OK;
        break;
    case GET_SECTOR_COUNT:
        *(DWORD *) buff = msc_class->blocknum;
        result = RES_OK;
        break;
    default:
        result = RES_PARERR;
        break;
    }
    return result;
}
