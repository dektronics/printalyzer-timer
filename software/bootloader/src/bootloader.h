/*
 * STM32F4 Bootloader Implementation
 *
 * This file contains all the necessary functions to handle
 * bootloader-specific features, such as application startup
 * and flash programming. It has been written to be specific to
 * the needs of this project, and thus may require modifications
 * to be usable on other MCU variants or memory layouts.
 *
 * It is based on the bootloader project written by Akos Pasztor, which
 * can be found here:
 * https://github.com/akospasztor/stm32-bootloader
 */

#ifndef BOOTLOADER_H
#define BOOTLOADER_H

#include <stm32f4xx_hal.h>

/** Start address of application space in flash */
#define APP_ADDRESS 0x08020000UL

/** End address of application space (address of last byte) */
#define END_ADDRESS 0x0807FFFBUL

/*
 * Note: The application space is currently hard-coded in numerous places
 * to use flash pages 5-7 as a variety of flash-related commands require
 * specifying specific pages via their own defined set of macros. Until
 * this is abstracted for all the relevant cases, care must be taken when
 * changing the application start address or size.
 */

/** Start address of the application descriptor structure in flash */
#define APP_DESCRIPTOR_ADDRESS 0x0807FF00UL

/** Address of System Memory (ST Bootloader) */
#define SYSMEM_ADDRESS 0x1FFF0000UL

/** Size of application in DWORD (32bits or 4bytes) */
#define APP_SIZE ((uint32_t)(((END_ADDRESS - APP_ADDRESS) + 3UL) / 4UL))

/* MCU RAM information (to check whether flash contains valid application) */
#define RAM_BASE SRAM1_BASE /*!< Start address of RAM */
#ifndef RAM_SIZE
#define RAM_SIZE 0x00020000UL /*!< RAM size in bytes */
#endif

#ifndef FLASH_SIZE
#define FLASH_SIZE 0x00080000UL /*!< Flash size in bytes */
#endif

/** Bootloader error codes */
typedef enum {
    BL_OK = 0,      /*!< No error */
    BL_NO_APP,      /*!< No application found in flash */
    BL_SIZE_ERROR,  /*!< New application is too large for flash */
    BL_CHKS_ERROR,  /*!< Application checksum error */
    BL_ERASE_ERROR, /*!< Flash erase error */
    BL_WRITE_ERROR, /*!< Flash write error */
    BL_OBP_ERROR    /*!< Flash option bytes programming error */
} bootloader_status_t;

/** Flash Protection Types */
typedef enum {
    BL_PROTECTION_NONE  = 0,   /*!< No flash protection */
    BL_PROTECTION_WRP   = 0x1, /*!< Flash write protection */
    BL_PROTECTION_RDP   = 0x2, /*!< Flash read protection */
    BL_PROTECTION_PCROP = 0x4, /*!< Flash proprietary code readout protection */
} bootloader_flash_protection_t;

bootloader_status_t bootloader_init(void);

bootloader_status_t bootloader_erase(void);

bootloader_status_t bootloader_flash_begin(void);
bootloader_status_t bootloader_flash_next(uint32_t data);
bootloader_status_t bootloader_flash_end(void);

bootloader_flash_protection_t bootloader_get_protection_status(void);
bootloader_status_t bootloader_config_protection(bootloader_flash_protection_t protection);

bootloader_status_t bootloader_check_size(uint32_t appsize);
bootloader_status_t bootloader_verify_checksum(CRC_HandleTypeDef *hcrc);
bootloader_status_t bootloader_check_for_application(void);

void bootloader_jump_to_application(void);

#endif /* BOOTLOADER_H */
