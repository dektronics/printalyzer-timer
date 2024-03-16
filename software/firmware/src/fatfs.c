#include "fatfs.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <ff.h>
#include <ff_gen_drv.h>
//#include <usbh_diskio.h>

#define LOG_TAG "fatfs"
#include <elog.h>

static char usbh_path[4]; /* USBH logical drive path */
static FATFS usbh_fatfs;  /* File system object for USBH logical drive */

static bool fatfs_initialized = false;
static bool fatfs_mounted = false;

//TODO Rewrite for new USB host stack

void fatfs_init(void)
{
#if 0
    memset(&usbh_fatfs, 0, sizeof(FATFS));
    fatfs_initialized = false;
    fatfs_mounted = false;

    uint8_t ret = FATFS_LinkDriver(&USBH_Driver, usbh_path);
    if (ret != 0) {
        printf("Unable to link USB driver to FATFS\r\n");
        return;
    }
    fatfs_initialized = true;
#endif
}

HAL_StatusTypeDef fatfs_mount()
{
#if 0
    if (!fatfs_initialized || fatfs_mounted) {
        return HAL_ERROR;
    }

    /* USBHPath = "0:/" */

    FRESULT res = f_mount(&usbh_fatfs, (TCHAR const*)usbh_path, 0);
    if (res != FR_OK) {
        log_e("f_mount error: %d", res);
        return HAL_ERROR;
    }
    fatfs_mounted = true;
    return HAL_OK;
#endif
    return HAL_ERROR;
}

void fatfs_unmount()
{
#if 0
    if (!fatfs_initialized || !fatfs_mounted) {
        return;
    }

    f_mount(0, (TCHAR const*)usbh_path, 0);
    memset(&usbh_fatfs, 0, sizeof(FATFS));
    fatfs_mounted = false;
#endif
}
