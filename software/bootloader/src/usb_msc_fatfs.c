#include "usb_msc_fatfs.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <ff.h>
#include <ff_gen_drv.h>

#include "usbh_core.h"
#include "usbh_msc.h"

#include "logger.h"

#ifndef __CDT_PARSER__
_Static_assert(CONFIG_USBHOST_MAX_MSC_CLASS == FF_VOLUMES, "CherryUSB and FATFS MSC counts should match");
#endif

typedef struct {
    struct usbh_msc *msc_class;
    char dev_serial[21];
    char usbh_path[4]; /* USBH logical drive path */
    char label[12];
    bool linked;
    bool mounted;
    bool ready;
    FATFS usbh_fatfs;  /* File system object for USBH logical drive */
} usbh_msc_fatfs_handle_t;

static usbh_msc_fatfs_handle_t msc_handles[CONFIG_USBHOST_MAX_MSC_CLASS];

static DSTATUS fatfs_diskio_initialize(BYTE pdrv);
static DSTATUS fatfs_diskio_status(BYTE pdrv);
static DRESULT fatfs_diskio_read(BYTE pdrv, BYTE *buff, DWORD sector, UINT count);
static DRESULT fatfs_diskio_write(BYTE pdrv, const BYTE *buff, DWORD sector, UINT count);
static DRESULT fatfs_diskio_ioctl(BYTE pdrv, BYTE cmd, void *buff);

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
    char string_buffer[128];

    if (devno > CONFIG_USBHOST_MAX_MSC_CLASS) {
        BL_PRINTF("Invalid device letter '%c'\r\n", msc_class->sdchar);
        return;
    }

    usbh_msc_fatfs_handle_t *handle = &msc_handles[devno];

    if (handle->msc_class) {
        BL_PRINTF("Device already attached\r\n");
        return;
    }

    /* Initialize handle state */
    handle->msc_class = msc_class;
    handle->dev_serial[0] = 0;
    handle->label[0] = 0;
    handle->linked = false;
    handle->mounted = false;
    handle->ready = false;
    memset(&handle->usbh_fatfs, 0, sizeof(FATFS));

    /* Get the device serial number */
    memset(string_buffer, 0, 128);
    ret = usbh_get_string_desc(msc_class->hport, USB_STRING_SERIAL_INDEX, (uint8_t *)string_buffer);
    if (ret < 0) {
        BL_PRINTF("Unable to get device serial number: %d\r\n", ret);
        return;
    }
    strncpy(handle->dev_serial, string_buffer, 21);
    handle->dev_serial[20] = '\0';
    BL_PRINTF("Drive serial: \"%s\"\r\n", handle->dev_serial);

    /* Link the driver callbacks */
    ret = FATFS_LinkDriver(&fatfs_usbh_msc_driver, handle->usbh_path, devno);
    if (ret != 0) {
        BL_PRINTF("Unable to link USB driver to FATFS\r\n");
        return;
    }
    handle->linked = true;

    /* Mount the volume */
    fres = f_mount(&handle->usbh_fatfs, (TCHAR const*)handle->usbh_path, 0);
    if (fres != FR_OK) {
        BL_PRINTF("f_mount error: %d\r\n", fres);
        return;
    }

    handle->mounted = true;

    /* Do some initial operations to make sure the volume works */
    fres = f_getlabel((TCHAR const*)handle->usbh_path, handle->label, 0);
    if (fres != FR_OK) {
        BL_PRINTF("f_getlabel error: %d\r\n", fres);
        return;
    }
    BL_PRINTF("Drive label: \"%s\"\r\n", handle->label);

    handle->ready = true;
}

void usbh_msc_fatfs_detached(struct usbh_msc *msc_class)
{
    int ret;
    FRESULT fres;
    const uint8_t devno = msc_class->sdchar - 'a';

    if (devno > CONFIG_USBHOST_MAX_MSC_CLASS) {
        BL_PRINTF("Invalid device letter '%c'\r\n", msc_class->sdchar);
        return;
    }

    usbh_msc_fatfs_handle_t *handle = &msc_handles[devno];

    if (!handle->msc_class) {
        BL_PRINTF("Device not attached\r\n");
        return;
    }

    handle->ready = false;

    if (handle->mounted) {
        fres = f_mount(0, (TCHAR const*)handle->usbh_path, 0);
        if (fres != FR_OK) {
            BL_PRINTF("f_unmount error: %d\r\n", fres);
        }
        memset(&handle->usbh_fatfs, 0, sizeof(FATFS));
        handle->mounted = false;
    }

    if (handle->linked) {
        ret = FATFS_UnLinkDriver(devno);
        if (ret != 0) {
            BL_PRINTF("Unable to unlink USB driver from FATFS\r\n");
        }
        handle->linked = false;
    }

    handle->msc_class = NULL;
}

bool usbh_msc_is_mounted(uint8_t num)
{
    return (num < CONFIG_USBHOST_MAX_MSC_CLASS) && msc_handles[num].ready;
}

void usbh_msc_unmount(uint8_t num)
{
    FRESULT fres;
    if (num > CONFIG_USBHOST_MAX_MSC_CLASS) {
        return;
    }

    usbh_msc_fatfs_handle_t *handle = &msc_handles[num];

    if (!handle->msc_class) {
        BL_PRINTF("Device not attached\r\n");
        return;
    }

    if (handle->mounted) {
        fres = f_mount(0, (TCHAR const*)handle->usbh_path, 0);
        if (fres != FR_OK) {
            BL_PRINTF("f_unmount error: %d\r\n", fres);
        }
        memset(&handle->usbh_fatfs, 0, sizeof(FATFS));
        handle->mounted = false;
    }
}

const char *usbh_msc_drive_label(uint8_t num)
{
    if (num < CONFIG_USBHOST_MAX_MSC_CLASS) {
        uint8_t pathnum = msc_handles[num].usbh_path[0] - '0';
        if (num != pathnum) {
            BL_PRINTF("Pathnum mismatch: dev=%d, path=\"%s\"\r\n", num, msc_handles[num].usbh_path);
        }
        return msc_handles[num].label;
    } else {
        return NULL;
    }
}

const char *usbh_msc_drive_serial(uint8_t num)
{
    if (num < CONFIG_USBHOST_MAX_MSC_CLASS) {
        uint8_t pathnum = msc_handles[num].usbh_path[0] - '0';
        if (num != pathnum) {
            BL_PRINTF("Pathnum mismatch: dev=%d, path=\"%s\"\r\n", num, msc_handles[num].usbh_path);
        }
        return msc_handles[num].dev_serial;
    } else {
        return NULL;
    }
}

uint8_t usbh_msc_max_drives()
{
    return CONFIG_USBHOST_MAX_MSC_CLASS;
}

/**
 * Initializes access to a drive
 * @param pdrv physical drive number
 * @return Operation status
 */
DSTATUS fatfs_diskio_initialize(BYTE pdrv)
{
    return RES_OK;
}

/**
 * Gets disk status
 * @param lun logical unit id
 * @return Operation status
 */
DSTATUS fatfs_diskio_status(BYTE pdrv)
{
    /* Not sure we currently have an equivalent way of checking this */
    return RES_OK;
}

/**
 * Read sectors
 * @param pdrv physical drive number
 * @param buff Data buffer to store read data
 * @param sector Sector address (LBA)
 * @param count Number of sectors to read (1..128)
 * @return Operation result
 */
DRESULT fatfs_diskio_read(BYTE pdrv, BYTE *buff, DWORD sector, UINT count)
{
    if (pdrv >= CONFIG_USBHOST_MAX_MSC_CLASS || !msc_handles[pdrv].msc_class) {
        return RES_ERROR;
    }
    return usbh_msc_scsi_read10(msc_handles[pdrv].msc_class, sector, buff, count);
}

/**
 * Write sectors
 * @param pdrv physical drive number
 * @param buff Data to be written
 * @param sector Sector address (LBA)
 * @param count Number of sectors to write (1..128)
 * @return Operation result
 */
DRESULT fatfs_diskio_write(BYTE pdrv, const BYTE *buff, DWORD sector, UINT count)
{
    if (pdrv >= CONFIG_USBHOST_MAX_MSC_CLASS || !msc_handles[pdrv].msc_class) {
        return RES_ERROR;
    }
    return usbh_msc_scsi_write10(msc_handles[pdrv].msc_class, sector, buff, count);
}

/**
 * I/O control operation
 * @param pdrv physical drive number
 * @param cmd Control code
 * @param buff Buffer to send/receive control data
 * @return Operation result
 */
DRESULT fatfs_diskio_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    if (pdrv >= CONFIG_USBHOST_MAX_MSC_CLASS || !msc_handles[pdrv].msc_class) {
        return RES_ERROR;
    }
    struct usbh_msc *msc_class = msc_handles[pdrv].msc_class;

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
