#include "bootloader.h"

#include <machine/endian.h>
#include <stdio.h>

#include "logger.h"
#include "app_descriptor.h"

typedef void (*jump_function_t)(void); /*!< Function pointer definition */

/** Private variable for tracking flashing progress */
static uint32_t flash_ptr = APP_ADDRESS;

/**
 * Initializes bootloader and flash.
 *
 * @retval BL_OK is returned in every case
 */
bootloader_status_t bootloader_init(void)
{
    __HAL_RCC_SYSCFG_CLK_ENABLE();

    HAL_FLASH_Unlock();

    /* Clear flash flags */
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR
        | FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR
        | FLASH_FLAG_RDERR);

    HAL_FLASH_Lock();

    return BL_OK;
}

/**
 * Erase the user application area in flash.
 *
 * @retval BL_OK upon success
 * @retval BL_ERR upon failure
 */
bootloader_status_t bootloader_erase(void)
{
    HAL_StatusTypeDef status = HAL_OK;
    uint32_t page_error = 0;

    FLASH_EraseInitTypeDef sEraseInit = {0};
    sEraseInit.TypeErase = FLASH_TYPEERASE_SECTORS;
    sEraseInit.Banks = FLASH_BANK_1;
    sEraseInit.Sector = FLASH_SECTOR_4; /* 0x08010000 */
    sEraseInit.NbSectors = 4;
    sEraseInit.VoltageRange = FLASH_VOLTAGE_RANGE_3;

    HAL_FLASH_Unlock();

    status = HAL_FLASHEx_Erase(&sEraseInit, &page_error);

    HAL_FLASH_Lock();

    return (status == HAL_OK) ? BL_OK : BL_ERASE_ERROR;
}

/**
 * Begin flash programming.
 *
 * This function unlocks the flash and sets the data pointer to the start of
 * the application flash area.
 *
 * @retval BL_OK is returned in every case
 */
bootloader_status_t bootloader_flash_begin(void)
{
    /* Reset flash destination address */
    flash_ptr = APP_ADDRESS;

    /* Unlock flash */
    HAL_FLASH_Unlock();

    return BL_OK;
}

/**
 * Program 32-bit data into flash.
 *
 * This function writes a 4 byte (32-bit) data chunk into the flash and
 * increments the data pointer.
 *
 * @param  data 32-bit data chunk to be written into flash
 * @retval BL_OK upon success
 * @retval BL_WRITE_ERROR upon failure
 */
bootloader_status_t bootloader_flash_next(uint32_t data)
{
    if (!(flash_ptr <= (FLASH_BASE + FLASH_SIZE - 4)) ||
        (flash_ptr < APP_ADDRESS)) {
        HAL_FLASH_Lock();
        return BL_WRITE_ERROR;
    }

    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, flash_ptr, data) == HAL_OK) {
        /* Check the written value */
        if (*(uint32_t*)flash_ptr != data) {
            /* Flash content doesn't match source content */
            HAL_FLASH_Lock();
            return BL_WRITE_ERROR;
        }
        /* Increment Flash destination address */
        flash_ptr += 4;
    } else {
        /* Error occurred while writing data into Flash */
        BL_PRINTF("Flash program error: 0x%08lX\r\n", HAL_FLASH_GetError());
        HAL_FLASH_Lock();
        return BL_WRITE_ERROR;
    }

    return BL_OK;
}

/**
 * Finish flash programming.
 *
 * This function finalizes the flash programming by locking the flash.
 *
 * @retval BL_OK is returned in every case
 */
bootloader_status_t bootloader_flash_end(void)
{
    /* Lock flash */
    HAL_FLASH_Lock();

    return BL_OK;
}

/**
 * Get the protection status of flash.
 *
 * @return Flash protection status consisting of 'bootloader_flash_protection_t' flags
 */
bootloader_flash_protection_t bootloader_get_protection_status(void)
{
    FLASH_OBProgramInitTypeDef sOBInit = {0};
    FLASH_AdvOBProgramInitTypeDef sAdvOBInit = {0};
    uint8_t protection = BL_PROTECTION_NONE;

    HAL_FLASH_Unlock();

    HAL_FLASHEx_OBGetConfig(&sOBInit);

    if (~(sOBInit.WRPSector) & (OB_WRP_SECTOR_4 | OB_WRP_SECTOR_5 | OB_WRP_SECTOR_6 | OB_WRP_SECTOR_7)) {
        protection |= BL_PROTECTION_WRP;
    }
    if (sOBInit.RDPLevel != OB_RDP_LEVEL_0) {
        protection |= BL_PROTECTION_RDP;
    }

    HAL_FLASHEx_AdvOBGetConfig(&sAdvOBInit);

    if ((sAdvOBInit.Sectors & 0x8000) && (~(sAdvOBInit.Sectors) & (OB_PCROP_SECTOR_4 | OB_PCROP_SECTOR_5 | OB_PCROP_SECTOR_6 | OB_PCROP_SECTOR_7))) {
        protection |= BL_PROTECTION_PCROP;
    }

    HAL_FLASH_Lock();

    return protection;
}

/**
 * Configures the write protection of flash.
 *
 * On success, this should trigger a restart of the system.
 *
 * @param  protection protection type consisting of 'bootloader_flash_protection_t' flags
 * @retval BL_OK upon success
 * @retval BL_OBP_ERROR upon failure
 */
bootloader_status_t bootloader_config_protection(bootloader_flash_protection_t protection)
{
    uint32_t protected_sectors = 0xFFF;
    FLASH_OBProgramInitTypeDef sOBPrev = {0};
    FLASH_OBProgramInitTypeDef sOBInit = {0};
    HAL_StatusTypeDef status = HAL_ERROR;

    /* Get previous configuration */
    HAL_FLASHEx_OBGetConfig(&sOBPrev);

    /* Only modify write protection */
    sOBInit.OptionType = OPTIONBYTE_WRP;

    /* Set whether the protection is being turned on or off */
    if (protection & BL_PROTECTION_WRP) {
        sOBInit.WRPState = OB_WRPSTATE_ENABLE;
    } else {
        sOBInit.WRPState = OB_WRPSTATE_DISABLE;
    }

    /* No read protection, keep BOR and reset settings */
    sOBInit.RDPLevel = OB_RDP_LEVEL0;
    sOBInit.USERConfig = sOBPrev.USERConfig;

    /* Get pages already write protected */
    protected_sectors = sOBPrev.WRPSector | (OB_WRP_SECTOR_4 | OB_WRP_SECTOR_5 | OB_WRP_SECTOR_6 | OB_WRP_SECTOR_7);

    do {
        /* Unlock flash to enable control register access */
        status = HAL_FLASH_Unlock();
        if (status != HAL_OK) { break;}

        /* Unlock the flash option bytes */
        status = HAL_FLASH_OB_Unlock();
        if (status != HAL_OK) { break;}

        /* Write the protection configuration */
        sOBInit.WRPSector = protected_sectors;
        status = HAL_FLASHEx_OBProgram(&sOBInit);
    } while (0);

    /* Lock the flash and option bytes */
    status |= HAL_FLASH_OB_Lock();
    status |= HAL_FLASH_Lock();

    /* On success, launch option byte loading which will trigger a restart */
    if (status == HAL_OK) {
        HAL_FLASH_OB_Launch();
    }

    return (status == HAL_OK) ? BL_OK : BL_OBP_ERROR;
}

/**
 * Check whether the new application fits into flash.
 *
 * @param  appsize size of the application
 * @retval BL_OK the application fits into flash
 * @retval BL_SIZE_ERROR the application does not fit into flash
 */
bootloader_status_t bootloader_check_size(uint32_t appsize)
{
    return ((FLASH_BASE + FLASH_SIZE - APP_ADDRESS) >= appsize) ? BL_OK
        : BL_SIZE_ERROR;
}

/**
 * Verify the checksum of the application located in flash.
 *
 * @retval BL_OK the calculated checksum matches the application checksum
 * @retval BL_CHKS_ERROR the calculated checksum does not match the
 *                       application checksum
 */
bootloader_status_t bootloader_verify_checksum(CRC_HandleTypeDef *hcrc)
{
    const app_descriptor_t *app_descriptor = (const app_descriptor_t *)APP_DESCRIPTOR_ADDRESS;
    volatile uint32_t calculated_crc = 0;

    calculated_crc =
        HAL_CRC_Calculate(hcrc, (uint32_t*)APP_ADDRESS, APP_SIZE);

    __HAL_RCC_CRC_FORCE_RESET();
    __HAL_RCC_CRC_RELEASE_RESET();

    if (app_descriptor->crc32 == calculated_crc) {
        return BL_OK;
    } else {
        BL_PRINTF("%08lX != %08lX\r\n", __bswap32(app_descriptor->crc32), __bswap32(calculated_crc));
    }

    return BL_CHKS_ERROR;
}

/**
 * Checks whether a valid application exists in flash.
 *
 * The check is performed by checking the very first word (4 bytes) of
 * the application firmware. In case of a valid application this word
 * must represent the initialization location of stack pointer, which
 * must be within the boundaries of RAM.
 *
 * @retval BL_OK the first word represents a valid stack pointer location
 * @retval BL_NO_APP the first word value is out of RAM boundaries
 */
bootloader_status_t bootloader_check_for_application(void)
{
    return (((*(uint32_t*)APP_ADDRESS) - RAM_BASE) <= RAM_SIZE) ? BL_OK
        : BL_NO_APP;
}

/**
 * Perform the jump to the user application in flash.
 *
 * The function carries out the following operations:
 *  - De-initialize the clock and peripheral configuration
 *  - Stop the SysTick
 *  - Set the vector table location
 *  - Set the stack pointer location
 *  - Perform the jump
 */
__USED __attribute__((optimize("O0"))) void bootloader_jump_to_application(void)
{
    uint32_t jump_address = *(__IO uint32_t*)(APP_ADDRESS + 4);
    jump_function_t jump = (jump_function_t)jump_address;

    /* De-initialize the clock and peripherals */
    HAL_RCC_DeInit();
    HAL_DeInit();

    /* Stop the SysTick */
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL  = 0;

    /* Set the vector table location */
    SCB->VTOR = APP_ADDRESS;

    /* Set the stack pointer location */
    __set_MSP(*(__IO uint32_t*)APP_ADDRESS);

    jump();
}
