#include <stm32f4xx_hal.h>

#include <machine/endian.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include <cmsis_os.h>

#include "logger.h"
#include "board_config.h"
#include "usb_host.h"
#include "keypad.h"
#include "bootloader.h"
#include "bootloader_task.h"

/* Uncomment for testing */
/* #define FORCE_BOOTLOADER */

UART_HandleTypeDef huart1;
I2C_HandleTypeDef hi2c1;
SMBUS_HandleTypeDef hsmbus2;
SPI_HandleTypeDef hspi1;
CRC_HandleTypeDef hcrc;

static uint32_t startup_bkp1r = 0;

static void system_clock_config(void);
static void peripheral_common_clock_config(void);
static void mpu_config();

static void read_startup_flags(void);

static void usart1_uart_init(void);
static void usart_deinit(void);
static void gpio_init(void);
static void gpio_deinit(void);
static void i2c1_init(void);
static void i2c2_smbus_init(void);
static void i2c_deinit(void);
static void spi1_init(void);
static void spi_deinit(void);
static void crc_init(void);
static void crc_deinit(void);

static void startup_messages();
static uint8_t check_startup_action();
static void start_bootloader(bootloader_trigger_t trigger);
void deinit_peripherals();
void save_startup_flags();
bool start_application();

void Error_Handler(void);
void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

void system_clock_config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /* Configure the main internal regulator output voltage */
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    /*
     * Initialize the RCC Oscillators according to the specified parameters
     * in the RCC_OscInitTypeDef structure.
     */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 8;
    RCC_OscInitStruct.PLL.PLLN = 180;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 8;
    RCC_OscInitStruct.PLL.PLLR = 2;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    /* Activate the Over-Drive mode */
    if (HAL_PWREx_EnableOverDrive() != HAL_OK) {
        Error_Handler();
    }

    /* Initialize the CPU, AHB and APB buses clocks */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                                  |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) {
        Error_Handler();
    }

    /* Activate the MCO2 pin to supply a 24MHz clock to the on-board USB hub */
    HAL_RCC_MCOConfig(RCC_MCO2, RCC_MCO2SOURCE_PLLI2SCLK, RCC_MCODIV_2);
}

void peripheral_common_clock_config(void)
{
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

    /* Initializes the peripherals clock */
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_PLLI2S;
    PeriphClkInitStruct.PLLI2S.PLLI2SN = 96;
    PeriphClkInitStruct.PLLI2S.PLLI2SP = RCC_PLLI2SP_DIV2;
    PeriphClkInitStruct.PLLI2S.PLLI2SM = 8;
    PeriphClkInitStruct.PLLI2S.PLLI2SR = 4;
    PeriphClkInitStruct.PLLI2S.PLLI2SQ = 2;
    PeriphClkInitStruct.PLLI2SDivQ = 1;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK) {
        Error_Handler();
    }
}

void mpu_config(void)
{
    MPU_Region_InitTypeDef MPU_InitStruct = {0};

    HAL_MPU_Disable();

    MPU_InitStruct.Enable = MPU_REGION_ENABLE;
    MPU_InitStruct.BaseAddress = 0x20000000;
    MPU_InitStruct.Size = MPU_REGION_SIZE_128KB;
    MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
    MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
    MPU_InitStruct.IsCacheable = MPU_ACCESS_CACHEABLE;
    MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
    MPU_InitStruct.Number = MPU_REGION_NUMBER0;
    MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
    MPU_InitStruct.SubRegionDisable = 0x00;
    MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
    HAL_MPU_ConfigRegion(&MPU_InitStruct);

    HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}

void read_startup_flags(void)
{
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();
    startup_bkp1r = RTC->BKP1R;
    RTC->BKP1R = 0;
    HAL_PWR_DisableBkUpAccess();
    __HAL_RCC_PWR_CLK_DISABLE();
}

void usart1_uart_init(void)
{
    /*
     * USART1 is used for debug logging
     */
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart1) != HAL_OK) {
        Error_Handler();
    }
}

void usart_deinit(void)
{
    HAL_UART_DeInit(&huart1);
}

void gpio_init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* GPIO Ports Clock Enable */
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    /* Configure GPIO pin Output Level */
    HAL_GPIO_WritePin(GPIOC, LED_RESET_Pin | DISP_RESET_Pin | KEY_RESET_Pin | RELAY_SFLT_Pin, GPIO_PIN_RESET);

    /* Configure GPIO pin Output Level */
    HAL_GPIO_WritePin(GPIOA, DISP_CS_Pin | DISP_DC_Pin, GPIO_PIN_RESET);

    /* Configure GPIO pin Output Level */
    HAL_GPIO_WritePin(GPIOB, USB_HUB_RESET_Pin | USB_DRIVE_VBUS_Pin, GPIO_PIN_RESET);

    /* Configure GPIO pin Output Level */
    HAL_GPIO_WritePin(RELAY_ENLG_GPIO_Port, RELAY_ENLG_Pin, GPIO_PIN_RESET);

    /* Configure unused GPIO pins: PC13 PC14 PC15 PC0 PC1 PC3 PC6 PC7 PC8 PC10 */
    GPIO_InitStruct.Pin = GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15 | GPIO_PIN_0
                          | GPIO_PIN_1 | GPIO_PIN_3 | GPIO_PIN_6 | GPIO_PIN_7
                          | GPIO_PIN_8 | GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    /* Configure GPIO pins: LED_RESET_Pin DISP_RESET_Pin RELAY_SFLT_Pin */
    GPIO_InitStruct.Pin = LED_RESET_Pin | DISP_RESET_Pin | RELAY_SFLT_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    /* Configure unused GPIO pins: PA0 PA1 PA2 PA3 PA8 PA9 PA11 PA12 */
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3
                          | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_11 | GPIO_PIN_12;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* Configure GPIO pins: DISP_CS_Pin DISP_DC_Pin */
    GPIO_InitStruct.Pin = DISP_CS_Pin | DISP_DC_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* Configure GPIO pin: KEY_RESET_Pin */
    GPIO_InitStruct.Pin = KEY_RESET_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(KEY_RESET_GPIO_Port, &GPIO_InitStruct);

    /* Configure unused GPIO pins: PB0 PB1 PB2 PB4 PB9 */
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_4
                          | GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* Configure GPIO pins: USB_HUB_RESET_Pin USB_DRIVE_VBUS_Pin */
    GPIO_InitStruct.Pin = USB_HUB_RESET_Pin | USB_DRIVE_VBUS_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* Configure GPIO pin: USB_HUB_CLK_Pin */
    GPIO_InitStruct.Pin = USB_HUB_CLK_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF0_MCO;
    HAL_GPIO_Init(USB_HUB_CLK_GPIO_Port, &GPIO_InitStruct);

    /* Configure GPIO pin: RELAY_ENLG_Pin */
    GPIO_InitStruct.Pin = RELAY_ENLG_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(RELAY_ENLG_GPIO_Port, &GPIO_InitStruct);

    /* Configure GPIO pin: KEY_INT_Pin */
    GPIO_InitStruct.Pin = KEY_INT_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(KEY_INT_GPIO_Port, &GPIO_InitStruct);

    /* Not currently enabling any GPIO interrupts in the bootloader */
    /* EXTI interrupt init*/
    /* HAL_NVIC_SetPriority(EXTI9_5_IRQn, 5, 0); */
    /* HAL_NVIC_EnableIRQ(EXTI9_5_IRQn); */
}

void gpio_deinit(void)
{
    /* Not currently enabling any GPIO interrupts in the bootloader */
    /* EXTI interrupt deinit */
    /* HAL_NVIC_DisableIRQ(EXTI9_5_IRQn); */

    /* De-initialize GPIO pins */
    HAL_GPIO_DeInit(KEY_INT_GPIO_Port, KEY_INT_Pin);
    HAL_GPIO_DeInit(RELAY_ENLG_GPIO_Port, RELAY_ENLG_Pin);
    HAL_GPIO_DeInit(USB_HUB_CLK_GPIO_Port, USB_HUB_CLK_Pin);
    HAL_GPIO_DeInit(GPIOB, USB_HUB_RESET_Pin | USB_DRIVE_VBUS_Pin);
    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_4 | GPIO_PIN_9);
    HAL_GPIO_DeInit(KEY_RESET_GPIO_Port, KEY_RESET_Pin);
    HAL_GPIO_DeInit(GPIOA, DISP_CS_Pin | DISP_DC_Pin);
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_8 |
                           GPIO_PIN_9 | GPIO_PIN_11 | GPIO_PIN_12);
    HAL_GPIO_DeInit(GPIOC, LED_RESET_Pin | DISP_RESET_Pin | RELAY_SFLT_Pin);
    HAL_GPIO_DeInit(GPIOC, GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15 | GPIO_PIN_0 |
                           GPIO_PIN_1 | GPIO_PIN_3 | GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_10);

    /* GPIO Ports Clock Disable */
    __HAL_RCC_GPIOC_CLK_DISABLE();
    __HAL_RCC_GPIOH_CLK_DISABLE();
    __HAL_RCC_GPIOA_CLK_DISABLE();
    __HAL_RCC_GPIOB_CLK_DISABLE();
    __HAL_RCC_GPIOD_CLK_DISABLE();
}

void i2c1_init(void)
{
    /*
     * I2C1 is used for on-board devices
     */
    hi2c1.Instance = I2C1;
    hi2c1.Init.ClockSpeed = 400000;
    hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1 = 0;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2 = 0;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c1) != HAL_OK) {
        Error_Handler();
    }
}

void i2c2_smbus_init(void)
{
    /*
     * I2C2 is used to configure the on-board USB hub
     */
    hsmbus2.Instance = I2C2;
    hsmbus2.Init.ClockSpeed = 100000;
    hsmbus2.Init.OwnAddress1 = 0;
    hsmbus2.Init.AddressingMode = SMBUS_ADDRESSINGMODE_7BIT;
    hsmbus2.Init.DualAddressMode = SMBUS_DUALADDRESS_DISABLE;
    hsmbus2.Init.OwnAddress2 = 0;
    hsmbus2.Init.GeneralCallMode = SMBUS_GENERALCALL_DISABLE;
    hsmbus2.Init.NoStretchMode = SMBUS_NOSTRETCH_DISABLE;
    hsmbus2.Init.PacketErrorCheckMode = SMBUS_PEC_DISABLE;
    hsmbus2.Init.PeripheralMode = SMBUS_PERIPHERAL_MODE_SMBUS_HOST;
    if (HAL_SMBUS_Init(&hsmbus2) != HAL_OK) {
        Error_Handler();
    }
}

void i2c_deinit(void)
{
    HAL_SMBUS_DeInit(&hsmbus2);
    HAL_I2C_DeInit(&hi2c1);
}

void spi1_init(void)
{
    /*
     * SPI1 is used for the display module
     */
    hspi1.Instance = SPI1;
    hspi1.Init.Mode = SPI_MODE_MASTER;
    hspi1.Init.Direction = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi1.Init.NSS = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
    hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.CRCPolynomial = 10;
    if (HAL_SPI_Init(&hspi1) != HAL_OK) {
        Error_Handler();
    }
}

void spi_deinit(void)
{
    HAL_SPI_DeInit(&hspi1);
}

void crc_init(void)
{
    hcrc.Instance = CRC;
    if (HAL_CRC_Init(&hcrc) != HAL_OK) {
        Error_Handler();
    }
}

void crc_deinit(void)
{
    HAL_CRC_DeInit(&hcrc);
}

uint8_t check_startup_action()
{
#ifdef FORCE_BOOTLOADER
    return 1;
#else
    uint8_t button_counter = 0;

    if ((startup_bkp1r & 0xFF000000UL) == 0xBB000000UL) {
        BL_PRINTF("Bootloader requested by main application\r\n");
        return 0xBB;
    }

    while (keypad_poll() & KEYPAD_MENU && button_counter < 90) {
        if (button_counter == 5) {
            BL_PRINTF("Release button to enter Bootloader.\r\n");
        }

        HAL_Delay(100);
        button_counter++;
    }

    if (button_counter < 90) {
        if (button_counter > 5) {
            BL_PRINTF("Enter bootloader...\r\n");
            return 1;
        }
    }

    return 0;
#endif
}

int main(void)
{
    /*
     * Initialize the HAL, which will reset of all peripherals, initialize
     * the Flash interface and the Systick.
     */
    HAL_Init();

    /* Configure the system clock */
    system_clock_config();
    peripheral_common_clock_config();

    /* Initialize the MPU */
    mpu_config();

    /* Read startup flags */
    read_startup_flags();

    /* Initialize all configured peripherals */
    usart1_uart_init();
    gpio_init();
    i2c1_init();
    i2c2_smbus_init();
    spi1_init();
    crc_init();

    /* Print startup log messages */
    startup_messages();

    /* Initialize the keypad controller */
    if (keypad_init(&hi2c1) != HAL_OK) {
        Error_Handler();
    }

    /* Check the keypad to determine the startup action */
    uint8_t action = check_startup_action();
    if (action == 1) {
        start_bootloader(TRIGGER_USER_BUTTON);
    } else if (action == 0xBB) {
        start_bootloader(TRIGGER_FIRMWARE);
    } else {
        if (!start_application()) {
            start_bootloader(TRIGGER_CHECKSUM_FAIL);
        }
    }

    return 0;
}

void startup_messages()
{
    BL_PRINTF("Starting Printalyzer bootloader...\r\n");

    uint32_t hal_ver = HAL_GetHalVersion();
    uint8_t hal_ver_code = ((uint8_t)(hal_ver)) & 0x0F;
    uint16_t *flash_size = (uint16_t*)(FLASHSIZE_BASE);

    BL_PRINTF("HAL Version: %d.%d.%d%c\r\n",
        ((uint8_t)(hal_ver >> 24)) & 0x0F,
        ((uint8_t)(hal_ver >> 16)) & 0x0F,
        ((uint8_t)(hal_ver >> 8)) & 0x0F,
        hal_ver_code > 0 ? (char)hal_ver_code : ' ');
    BL_PRINTF("Device ID: 0x%lX\r\n", HAL_GetDEVID());
    BL_PRINTF("Revision ID: %ld\r\n", HAL_GetREVID());
    BL_PRINTF("Flash size: %dk\r\n", *flash_size);
    BL_PRINTF("SysClock: %ldMHz\r\n", HAL_RCC_GetSysClockFreq() / 1000000);

    BL_PRINTF("Unique ID: %08lX%08lX%08lX\r\n",
        __bswap32(HAL_GetUIDw0()),
        __bswap32(HAL_GetUIDw1()),
        __bswap32(HAL_GetUIDw2()));

    BL_PRINTF("Bootloader build date: %s\r\n", BOOTLOADER_BUILD_DATE);
    BL_PRINTF("Bootloader build describe: %s\r\n", BOOTLOADER_BUILD_DESCRIBE);
    BL_PRINTF("\r\n");
}

void start_bootloader(bootloader_trigger_t trigger)
{
    BL_PRINTF("Start Bootloader\r\n");

    switch (trigger) {
    case TRIGGER_USER_BUTTON:
        BL_PRINTF("Trigger: User Button\r\n");
        break;
    case TRIGGER_FIRMWARE:
        BL_PRINTF("Trigger: Firmware\r\n");
        break;
    case TRIGGER_CHECKSUM_FAIL:
        BL_PRINTF("Trigger: Checksum Fail\r\n");
        break;
    default:
        break;
    }

    /* Initialize the FreeRTOS scheduler */
    osKernelInitialize();

    /* Create the main bootloader task */
    bootloader_task_init(trigger);

    /* Start the FreeRTOS scheduler */
    osKernelStart();

    while (1) { }
}

void deinit_peripherals()
{
    crc_deinit();
    spi_deinit();
    i2c_deinit();
    gpio_deinit();
    usart_deinit();
}

void save_startup_flags()
{
    /*
     * Save startup data into the RTC backup registers, since some restart
     * conditions can cause it to get cleared before the application code
     * can read it.
     */
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();
    RTC->BKP0R = (RCC->CSR & 0xFF000000UL); /* Reset flags */
    HAL_PWR_DisableBkUpAccess();
    __HAL_RCC_PWR_CLK_DISABLE();
}

bool start_application()
{
    BL_PRINTF("Check for application\r\n");

    if (bootloader_check_for_application() == BL_OK) {
        /* Verify application checksum */
        if(bootloader_verify_checksum(&hcrc) != BL_OK) {
            BL_PRINTF("Checksum Error.\r\n");
            return false;
        } else {
            BL_PRINTF("Checksum OK.\r\n");
        }
        BL_PRINTF("Launching Application...\r\n");

        /* De-initialize all the peripherals */
        deinit_peripherals();

        /* Save startup flags so the application can read them */
        save_startup_flags();

        /* Start the application */
        bootloader_jump_to_application();
    } else {
        return false;
    }
    return true;
}

/**
 * @brief  Period elapsed callback in non blocking mode
 * @note   This function is called when the TIM11 interrupt takes place,
 * inside HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick()
 * to increment a global variable "uwTick" that is used as application
 * time base.
 * @param  htim TIM handle
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM11) {
        HAL_IncTick();
    }
}

void Error_Handler(void)
{
    __disable_irq();
    __ASM volatile("BKPT #01");
    while (1) { }
}

#ifdef USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t *file, uint32_t line)
{
    BL_PRINTF("Assert failed: file %s on line %ld\r\n", file, line);
}
#endif /* USE_FULL_ASSERT */
