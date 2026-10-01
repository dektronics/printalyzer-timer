#include "app_descriptor.h"

const __attribute__((section(".boot_descriptor"))) boot_descriptor_t boot_descriptor = {
    .magic_word = BOOT_DESCRIPTOR_MAGIC_WORD,
    .version = "v0.9.2",
    .build_date = BOOTLOADER_BUILD_DATE,
    .build_describe = BOOTLOADER_BUILD_DESCRIBE,
    .crc32 = 0xFFFFFFFF /* This is overwritten by the build process */
};

#ifndef __CDT_PARSER__
_Static_assert(sizeof(BOOTLOADER_BUILD_DATE) <= sizeof(boot_descriptor.build_date), "BOOTLOADER_BUILD_DATE is longer than version field in structure");
_Static_assert(sizeof(BOOTLOADER_BUILD_DESCRIBE) <= sizeof(boot_descriptor.build_describe), "BOOTLOADER_BUILD_DESCRIBE is longer than version field in structure");
#endif

const boot_descriptor_t *boot_descriptor_get()
{
    return &boot_descriptor;
}
