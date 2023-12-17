#include "fatfs.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <ff.h>
#include <ff_gen_drv.h>
#include <usbh_diskio.h>

#include "logger.h"

static char usbh_path[4]; /* USBH logical drive path */
static FATFS usbh_fatfs;  /* File system object for USBH logical drive */

static bool fatfs_initialized = false;
static bool fatfs_mounted = false;

void fatfs_init(void)
{
    memset(&usbh_fatfs, 0, sizeof(FATFS));
    fatfs_initialized = false;
    fatfs_mounted = false;

    uint8_t ret = FATFS_LinkDriver(&USBH_Driver, usbh_path);
    if (ret != 0) {
        BL_PRINTF("Unable to link USB driver to FATFS\r\n");
        return;
    }
    fatfs_initialized = true;
}

void fatfs_deinit(void)
{
    if (!fatfs_initialized) {
        return;
    }

    FATFS_UnLinkDriver(usbh_path);
    fatfs_initialized = false;
}

HAL_StatusTypeDef fatfs_mount()
{
    if (!fatfs_initialized || fatfs_mounted) {
        return HAL_ERROR;
    }

    /* USBHPath = "0:/" */

    FRESULT res = f_mount(&usbh_fatfs, (TCHAR const*)usbh_path, 0);
    if (res != FR_OK) {
        BL_PRINTF("f_mount error: %d\r\n", res);
        return HAL_ERROR;
    }
    fatfs_mounted = true;
    return HAL_OK;
}

void fatfs_unmount()
{
    if (!fatfs_initialized || !fatfs_mounted) {
        return;
    }

    f_mount(0, (TCHAR const*)usbh_path, 0);
    memset(&usbh_fatfs, 0, sizeof(FATFS));
    fatfs_mounted = false;
}
