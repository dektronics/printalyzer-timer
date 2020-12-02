#include "fatfs.h"

#include <stdio.h>
#include <ff.h>
#include <ff_gen_drv.h>
#include <usbh_diskio.h>

char usbh_path[4]; /* USBH logical drive path */
FATFS usbh_fatfs;  /* File system object for USBH logical drive */
FIL usbh_file;     /* File object for USBH */

void fatfs_init(void)
{
    uint8_t ret = FATFS_LinkDriver(&USBH_Driver, usbh_path);
    if (ret != 0) {
        printf("Unable to link USB driver to FATFS\r\n");
    }
}
