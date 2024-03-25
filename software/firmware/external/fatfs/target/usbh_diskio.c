/**
  ******************************************************************************
  * @file    usbh_diskio.c (based on usbh_diskio_template.c v2.0.2)
  * @brief   USB Host Disk I/O driver
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

#include "ff_gen_drv.h"
#include "usbh_diskio.h"

#include "usbh_core.h"
#include "usbh_msc.h"

#define USB_DEFAULT_BLOCK_SIZE 512

struct usbh_msc *active_msc_class = NULL;

DSTATUS USBH_initialize (BYTE);
DSTATUS USBH_status (BYTE);
DRESULT USBH_read (BYTE, BYTE*, DWORD, UINT);

#if !FF_FS_READONLY
DRESULT USBH_write (BYTE, const BYTE*, DWORD, UINT);
#endif /* !FF_FS_READONLY */

DRESULT USBH_ioctl (BYTE, BYTE, void*);

const Diskio_drvTypeDef  USBH_Driver =
{
    USBH_initialize,
    USBH_status,
    USBH_read,
#if  _USE_WRITE == 1
    USBH_write,
#endif /* _USE_WRITE == 1 */
#if  _USE_IOCTL == 1
    USBH_ioctl,
#endif /* _USE_IOCTL == 1 */
};

/* Private functions ---------------------------------------------------------*/

/**
 * @brief  Initializes a Drive
 * @param  lun : lun id
 * @retval DSTATUS: Operation status
 */
DSTATUS USBH_initialize(BYTE lun)
{
    active_msc_class = (struct usbh_msc *)usbh_find_class_instance("/dev/sda");
    if (active_msc_class == NULL) {
        printf("Could not find /dev/sda\r\n");
        return RES_NOTRDY;
    }
    return RES_OK;
}

/**
 * @brief  Gets Disk Status
 * @param  lun : lun id
 * @retval DSTATUS: Operation status
 */
DSTATUS USBH_status(BYTE lun)
{
    return 0;
}

/**
 * @brief  Reads Sector(s)
 * @param  lun : lun id
 * @param  *buff: Data buffer to store read data
 * @param  sector: Sector address (LBA)
 * @param  count: Number of sectors to read (1..128)
 * @retval DRESULT: Operation result
 */
DRESULT USBH_read(BYTE lun, BYTE *buff, DWORD sector, UINT count)
{
    return usbh_msc_scsi_read10(active_msc_class, sector, buff, count);
}

/**
 * @brief  Writes Sector(s)
 * @param  lun : lun id
 * @param  *buff: Data to be written
 * @param  sector: Sector address (LBA)
 * @param  count: Number of sectors to write (1..128)
 * @retval DRESULT: Operation result
 */
#if !FF_FS_READONLY
DRESULT USBH_write(BYTE lun, const BYTE *buff, DWORD sector, UINT count)
{
    return usbh_msc_scsi_write10(active_msc_class, sector, buff, count);
}
#endif /* !FF_FS_READONLY */

/**
 * @brief  I/O control operation
 * @param  lun : lun id
 * @param  cmd: Control code
 * @param  *buff: Buffer to send/receive control data
 * @retval DRESULT: Operation result
 */
DRESULT USBH_ioctl(BYTE lun, BYTE cmd, void *buff)
{
    int result = 0;

    switch (cmd) {
    case CTRL_SYNC:
        result = RES_OK;
        break;

    case GET_SECTOR_SIZE:
        *(WORD *) buff = active_msc_class->blocksize;
        result = RES_OK;
        break;

    case GET_BLOCK_SIZE:
        *(DWORD *) buff = 1;
        result = RES_OK;
        break;

    case GET_SECTOR_COUNT:
        *(DWORD *) buff = active_msc_class->blocknum;
        result = RES_OK;
        break;

    default:
        result = RES_PARERR;
        break;
    }
}
