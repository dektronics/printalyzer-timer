#include "app_descriptor.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ff.h"

const __attribute__((section(".app_descriptor"))) app_descriptor_t app_descriptor = {
    .magic_word = APP_DESCRIPTOR_MAGIC_WORD,
    .project_name = "Printalyzer",
    .version = "v0.9.2",
    .build_date = APP_BUILD_DATE,
    .build_describe = APP_BUILD_DESCRIBE,
    .crc32 = 0xFFFFFFFF /* This is overwritten by the build process */
};

#ifndef __CDT_PARSER__
_Static_assert(sizeof(APP_BUILD_DATE) <= sizeof(app_descriptor.build_date), "APP_BUILD_DATE is longer than version field in structure");
_Static_assert(sizeof(APP_BUILD_DESCRIBE) <= sizeof(app_descriptor.build_describe), "APP_BUILD_DESCRIBE is longer than version field in structure");
#endif

const app_descriptor_t *app_descriptor_get()
{
    return &app_descriptor;
}

#ifdef APP_BUILD_DATE
DWORD get_fattime (void)
{
    /*
     * This function assumes that APP_BUILD_DATE has been set
     * with the exact format of "YYYY-MM-DD HH:MM".
     */

    /* Copy the build timestamp into an array of null-separated fields */
    char str[20] = {0};
    memcpy(str, APP_BUILD_DATE, sizeof(APP_BUILD_DATE));
    str[4] = '\0';
    str[7] = '\0';
    str[10] = '\0';
    str[13] = '\0';

    /* Parse out the timestamp components */
    const int tm_year = strtol(str, NULL, 10) - 1900;
    const int tm_mon = strtol(str + 5, NULL, 10) - 1;
    const int tm_mday = strtol(str + 8, NULL, 10);
    const int tm_hour = strtol(str + 11, NULL, 10);
    const int tm_min = strtol(str + 14, NULL, 10);

    /* Return the FatFs timestamp value */
    return (DWORD)(tm_year - 80) << 25 |
           (DWORD)(tm_mon + 1) << 21 |
           (DWORD)tm_mday << 16 |
           (DWORD)tm_hour << 11 |
           (DWORD)tm_min << 5;
}
#endif
