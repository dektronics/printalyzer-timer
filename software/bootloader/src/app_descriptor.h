#ifndef APP_DESCRIPTOR_H
#define APP_DESCRIPTOR_H

#include <stdint.h>

#define APP_DESCRIPTOR_MAGIC_WORD 0xABCD7654

/**
 * Application descriptor structure.
 *
 * Note: The memory layout of this structure is not allowed to change,
 * because it is also handled by the bootloader and the build scripts.
 */
typedef struct {
    uint32_t magic_word;        /*!< Magic word APP_DESCRIPTOR_MAGIC_WORD */
    uint32_t reserved1[3];      /*!< Reserved */
    char project_name[32];      /*!< Project name */
    char version[32];           /*!< Application version */
    char build_date[20];        /*!< Build timestamp */
    char build_describe[64];    /*!< Build Git description */
    uint32_t reserved2[22];     /*!< Reserved */
    uint32_t crc32;             /*!< CRC-32 checksum using the STM32F4 hardware algorithm */
} app_descriptor_t;

#ifndef __CDT_PARSER__
_Static_assert(sizeof(app_descriptor_t) == 256, "app_descriptor_t should be 256 bytes");
#endif


#define BOOT_DESCRIPTOR_MAGIC_WORD 0xCDEF3210

/**
 * Bootloader descriptor structure.
 *
 * Note: The memory layout of this structure is not allowed to change,
 * because it is also handled by the application firmware and build scripts.
 */
typedef struct {
    uint32_t magic_word;        /*!< Magic word BOOT_DESCRIPTOR_MAGIC_WORD */
    char version[32];           /*!< Application version */
    char build_date[20];        /*!< Build timestamp */
    char build_describe[64];    /*!< Build Git description */
    uint32_t reserved;          /*!< Reserved */
    uint32_t crc32;             /*!< CRC-32 checksum using the STM32F4 hardware algorithm */
} boot_descriptor_t;

#ifndef __CDT_PARSER__
_Static_assert(sizeof(boot_descriptor_t) == 128, "boot_descriptor_t should be 128 bytes");
#endif


/**
 * Gets the descriptor block for the bootloader image.
 *
 * This structure is stored at a specific location within flash, and
 * can only be updated as part of the build process.
 */
const boot_descriptor_t *boot_descriptor_get();

#endif /* APP_DESCRIPTOR_H */
