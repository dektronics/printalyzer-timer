#include <stm32f4xx_hal.h>

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <ff.h>

#include "board_config.h"
#include "usb_host.h"
#include "fatfs.h"
#include "keypad.h"
#include "display.h"
#include "bootloader.h"
#include "app_descriptor.h"

UART_HandleTypeDef huart1;

I2C_HandleTypeDef hi2c1;
I2C_HandleTypeDef hi2c2;

SPI_HandleTypeDef hspi1;
SPI_HandleTypeDef hspi2;

TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim9;

CRC_HandleTypeDef hcrc;

static void system_clock_config(void);
static void mpu_config();

static void usart1_uart_init(void);
static void gpio_init(void);
static void gpio_deinit(void);
static void i2c_init(void);
static void i2c_deinit(void);
static void spi_init(void);
static void spi_deinit(void);
static void tim1_init(void);
static void tim3_init(void);
static void tim9_init(void);
static void tim_deinit(void);
static void crc_init(void);
static void crc_deinit(void);

static void startup_messages();
static uint8_t check_startup_action();
static void start_bootloader();
static bool process_firmware_update();
static void start_system_memory();
static bool start_application();
static void delay_with_usb(uint32_t delay);

void Error_Handler(void);
void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

#ifndef MIN
#define MIN(a, b)  (((a) < (b)) ? (a) : (b))
#endif

#define FW_FILENAME "printalyzer-fw.bin"

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
    RCC_OscInitStruct.PLL.PLLN = 96;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 4;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    /* Initialize the CPU, AHB and APB buses clocks */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
        |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK) {
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

void usart1_uart_init(void)
{
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
    HAL_GPIO_WritePin(GPIOC, USB_DRIVE_VBUS_Pin|DMX512_RX_EN_Pin, GPIO_PIN_SET);

    /* Configure GPIO pin Output Level */
    HAL_GPIO_WritePin(GPIOA, BUZZ_EN1_Pin|BUZZ_EN2_Pin|DISP_CS_Pin|DISP_DC_Pin, GPIO_PIN_RESET);

    /* Configure GPIO pin Output Level */
    HAL_GPIO_WritePin(GPIOC, DISP_RES_Pin|DMX512_TX_EN_Pin|RELAY_SFLT_Pin|RELAY_ENLG_Pin, GPIO_PIN_RESET);

    /* Configure GPIO pin Output Level */
    HAL_GPIO_WritePin(SENSOR_VBUS_GPIO_Port, SENSOR_VBUS_Pin, GPIO_PIN_SET);

    /* Configure GPIO pin Output Level */
    HAL_GPIO_WritePin(LED_LE_GPIO_Port, LED_LE_Pin, GPIO_PIN_RESET);

    /* Configure unused GPIO pins: PC13 PC14 PC15 PC0 PC3 PC5 PC12 */
    GPIO_InitStruct.Pin = GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15|GPIO_PIN_0
        |GPIO_PIN_3|GPIO_PIN_5|GPIO_PIN_12;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    /* Configure GPIO pin: USB_DRIVE_VBUS_Pin */
    GPIO_InitStruct.Pin = USB_DRIVE_VBUS_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(USB_DRIVE_VBUS_GPIO_Port, &GPIO_InitStruct);

    /* Configure GPIO pin: USB_VBUS_OC_Pin */
    GPIO_InitStruct.Pin = USB_VBUS_OC_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(USB_VBUS_OC_GPIO_Port, &GPIO_InitStruct);

    /* Configure GPIO pins: BUZZ_EN1_Pin BUZZ_EN2_Pin DISP_CS_Pin DISP_DC_Pin */
    GPIO_InitStruct.Pin = BUZZ_EN1_Pin|BUZZ_EN2_Pin|DISP_CS_Pin|DISP_DC_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* Configure unused GPIO pin: PA3 */
    GPIO_InitStruct.Pin = GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* Configure GPIO pins: DISP_RES_Pin DMX512_TX_EN_Pin DMX512_RX_EN_Pin RELAY_SFLT_Pin RELAY_ENLG_Pin */
    GPIO_InitStruct.Pin = DISP_RES_Pin|DMX512_TX_EN_Pin|DMX512_RX_EN_Pin|RELAY_SFLT_Pin
        |RELAY_ENLG_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    /* Configure GPIO pins: SENSOR_INT_Pin KEY_INT_Pin */
    GPIO_InitStruct.Pin = SENSOR_INT_Pin|KEY_INT_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* Configure GPIO pin: SENSOR_VBUS_Pin */
    GPIO_InitStruct.Pin = SENSOR_VBUS_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(SENSOR_VBUS_GPIO_Port, &GPIO_InitStruct);

    /* Configure unused GPIO pins: PB12 PB4 */
    GPIO_InitStruct.Pin = GPIO_PIN_12|GPIO_PIN_4;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* Configure GPIO pin: LED_LE_Pin */
    GPIO_InitStruct.Pin = LED_LE_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_LE_GPIO_Port, &GPIO_InitStruct);

    /* Configure GPIO pins: DMX512_TX_Pin DMX512_RX_Pin */
    GPIO_InitStruct.Pin = DMX512_TX_Pin|DMX512_RX_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF8_USART6;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    /* Configure unused GPIO pin: PD2 */
    GPIO_InitStruct.Pin = GPIO_PIN_2;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    /* Not currently enabling any GPIO interrupts in the bootloader */
}

void gpio_deinit(void)
{
    /* De-initialize GPIO pins */
    HAL_GPIO_DeInit(GPIOD, GPIO_PIN_2);
    HAL_GPIO_DeInit(GPIOC, DMX512_TX_Pin|DMX512_RX_Pin);
    HAL_GPIO_DeInit(LED_LE_GPIO_Port, LED_LE_Pin);
    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_12|GPIO_PIN_4);
    HAL_GPIO_DeInit(SENSOR_VBUS_GPIO_Port, SENSOR_VBUS_Pin);
    HAL_GPIO_DeInit(GPIOB, SENSOR_INT_Pin|KEY_INT_Pin);
    HAL_GPIO_DeInit(GPIOC, DISP_RES_Pin|DMX512_TX_EN_Pin|DMX512_RX_EN_Pin
        |RELAY_SFLT_Pin|RELAY_ENLG_Pin);
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_3);
    HAL_GPIO_DeInit(GPIOA, BUZZ_EN1_Pin|BUZZ_EN2_Pin|DISP_CS_Pin|DISP_DC_Pin);
    HAL_GPIO_DeInit(USB_VBUS_OC_GPIO_Port, USB_VBUS_OC_Pin);
    HAL_GPIO_DeInit(USB_DRIVE_VBUS_GPIO_Port, USB_DRIVE_VBUS_Pin);
    HAL_GPIO_DeInit(GPIOC, GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15|GPIO_PIN_0
        |GPIO_PIN_3|GPIO_PIN_5|GPIO_PIN_12);

    /* GPIO Ports Clock Disable */
    __HAL_RCC_GPIOC_CLK_DISABLE();
    __HAL_RCC_GPIOH_CLK_DISABLE();
    __HAL_RCC_GPIOA_CLK_DISABLE();
    __HAL_RCC_GPIOB_CLK_DISABLE();
    __HAL_RCC_GPIOD_CLK_DISABLE();
}

void i2c_init(void)
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

    /*
     * I2C2 is used for off-board devices on the meter probe
     */
    hi2c2.Instance = I2C2;
    hi2c2.Init.ClockSpeed = 400000;
    hi2c2.Init.DutyCycle = I2C_DUTYCYCLE_2;
    hi2c2.Init.OwnAddress1 = 0;
    hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c2.Init.OwnAddress2 = 0;
    hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c2) != HAL_OK) {
        Error_Handler();
    }
}

void i2c_deinit(void)
{
    HAL_I2C_DeInit(&hi2c2);
    HAL_I2C_DeInit(&hi2c1);
}

void spi_init(void)
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
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4;
    hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.CRCPolynomial = 10;
    if (HAL_SPI_Init(&hspi1) != HAL_OK) {
        Error_Handler();
    }

    /*
     * SPI2 is used for the LED driver
     */
    hspi2.Instance = SPI2;
    hspi2.Init.Mode = SPI_MODE_MASTER;
    hspi2.Init.Direction = SPI_DIRECTION_2LINES;
    hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi2.Init.CLKPolarity = SPI_POLARITY_HIGH;
    hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi2.Init.NSS = SPI_NSS_SOFT;
    hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
    hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi2.Init.CRCPolynomial = 10;
    if (HAL_SPI_Init(&hspi2) != HAL_OK) {
        Error_Handler();
    }
}

void spi_deinit(void)
{
    HAL_SPI_DeInit(&hspi2);
    HAL_SPI_DeInit(&hspi1);
}

void tim1_init(void)
{
    /*
     * TIM1 is used for the rotary encoder
     */
    TIM_Encoder_InitTypeDef sConfig = {0};
    TIM_MasterConfigTypeDef sMasterConfig = {0};

    htim1.Instance = TIM1;
    htim1.Init.Prescaler = 0;
    htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim1.Init.Period = 65535;
    htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim1.Init.RepetitionCounter = 0;
    htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    sConfig.EncoderMode = TIM_ENCODERMODE_TI12;
    sConfig.IC1Polarity = TIM_ICPOLARITY_RISING;
    sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
    sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
    sConfig.IC1Filter = 0x0F;
    sConfig.IC2Polarity = TIM_ICPOLARITY_RISING;
    sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
    sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
    sConfig.IC2Filter = 0x0F;

    if (HAL_TIM_Encoder_Init(&htim1, &sConfig) != HAL_OK) {
        Error_Handler();
    }

    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;

    if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK) {
        Error_Handler();
    }
}

void tim3_init(void)
{
    /*
     * TIM3 is used to control LED brightness
     */
    TIM_MasterConfigTypeDef sMasterConfig = {0};
    TIM_OC_InitTypeDef sConfigOC = {0};

    htim3.Instance = TIM3;
    htim3.Init.Prescaler = 0;
    htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim3.Init.Period = 65535;
    htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    if (HAL_TIM_PWM_Init(&htim3) != HAL_OK) {
        Error_Handler();
    }

    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;

    if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK) {
        Error_Handler();
    }

    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = 32768;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;

    if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_3) != HAL_OK) {
        Error_Handler();
    }

    HAL_TIM_MspPostInit(&htim3);
}

void tim9_init(void)
{
    /*
     * TIM9 is used to control the piezo buzzer frequency
     */
    TIM_OC_InitTypeDef sConfigOC = {0};

    htim9.Instance = TIM9;
    htim9.Init.Prescaler = 0;
    htim9.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim9.Init.Period = 65535;
    htim9.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim9.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    if (HAL_TIM_PWM_Init(&htim9) != HAL_OK) {
        Error_Handler();
    }

    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = 32767;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;

    if (HAL_TIM_PWM_ConfigChannel(&htim9, &sConfigOC, TIM_CHANNEL_1) != HAL_OK) {
        Error_Handler();
    }

    HAL_TIM_MspPostInit(&htim9);
}

void tim_deinit(void)
{
    HAL_TIM_PWM_DeInit(&htim9);
    HAL_TIM_PWM_DeInit(&htim3);
    HAL_TIM_Encoder_DeInit(&htim1);
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
    uint8_t button_counter = 0;

    while (keypad_poll() & KEYPAD_MENU && button_counter < 90) {
        if (button_counter == 5) {
            printf("Release button to enter Bootloader.\r\n");
        }
        if (button_counter == 40) {
            printf("Release button to enter System Memory.\r\n");
        }

        HAL_Delay(100);
        button_counter++;
    }

    if (button_counter < 90) {
        if (button_counter > 40) {
            printf("Jump to system memory...\r\n");
            return 2;
        } else if (button_counter > 5) {
            printf("Enter bootloader...\r\n");
            return 1;
        }
    }

    return 0;
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

    /* Initialize the MPU */
    mpu_config();

    /* Initialize all configured peripherals */
    usart1_uart_init();
    gpio_init();
    i2c_init();
    spi_init();
    tim1_init();
    tim3_init();
    tim9_init();
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
        start_bootloader();
    } else if (action == 2) {
        start_system_memory();
    } else {
        if (!start_application()) {
            start_bootloader();
        }
    }

    return 0;
}

void startup_messages()
{
    printf("Starting Printalyzer bootloader...\r\n");

    uint32_t hal_ver = HAL_GetHalVersion();
    uint8_t hal_ver_code = ((uint8_t)(hal_ver)) & 0x0F;
    uint16_t *flash_size = (uint16_t*)(FLASHSIZE_BASE);

    printf("HAL Version: %d.%d.%d%c\r\n",
        ((uint8_t)(hal_ver >> 24)) & 0x0F,
        ((uint8_t)(hal_ver >> 16)) & 0x0F,
        ((uint8_t)(hal_ver >> 8)) & 0x0F,
        hal_ver_code > 0 ? (char)hal_ver_code : ' ');
    printf("Device ID: 0x%lX\r\n", HAL_GetDEVID());
    printf("Revision ID: %ld\r\n", HAL_GetREVID());
    printf("Flash size: %dk\r\n", *flash_size);
    printf("SysClock: %ldMHz\r\n", HAL_RCC_GetSysClockFreq() / 1000000);

    printf("Unique ID: %08lX%08lX%08lX\r\n",
        __bswap32(HAL_GetUIDw0()),
        __bswap32(HAL_GetUIDw1()),
        __bswap32(HAL_GetUIDw2()));

    printf("Bootloader build date: %s\r\n", BOOTLOADER_BUILD_DATE);
    printf("Bootloader build describe: %s\r\n", BOOTLOADER_BUILD_DESCRIBE);
    printf("\r\n");
}

void start_bootloader()
{
    printf("Start Bootloader\r\n");

    /* Initialize the display */
    const u8g2_display_handle_t display_handle = {
        .hspi = &hspi1,
        .reset_gpio_port = DISP_RES_GPIO_Port,
        .reset_gpio_pin = DISP_RES_Pin,
        .cs_gpio_port = DISP_CS_GPIO_Port,
        .cs_gpio_pin = DISP_CS_Pin,
        .dc_gpio_port = DISP_DC_GPIO_Port,
        .dc_gpio_pin = DISP_DC_Pin
    };
    display_init(&display_handle);
    display_enable(true);
    display_draw_test_pattern();

    /* Initialize the USB host */
    usb_host_init();

    /* Initialize the FATFS support code */
    fatfs_init();

    /* Main loop */
    bool prompt_visible = false;
    while (1) {
        usb_host_process();
        if (usb_msc_is_mounted()) {
            prompt_visible = false;
            if (process_firmware_update()) {
                delay_with_usb(1000);
                display_static_message("Restarting...\r\n");
                delay_with_usb(100);

                /* De-initialize the USB and FatFs components */
                usb_msc_unmount();
                fatfs_deinit();
                usb_host_deinit();

                /* Start the application firmware */
                HAL_Delay(1000);
                display_clear();
                start_application();
            } else {
                delay_with_usb(1000);
                display_static_message("Firmware update failed");
                while(1);
            }
        } else {
            if (!prompt_visible) {
                display_static_message(
                    "Insert a USB storage device to\n"
                    "install updated Printalyzer\n"
                    "firmware.");
                prompt_visible = true;
            }
        }
    }
}

bool process_firmware_update()
{
    display_static_message("Checking for firmware...");

    FRESULT res;
    FIL fp;
    bool file_open = false;
    uint8_t buf[512];
    UINT bytes_remaining;
    UINT bytes_to_read;
    UINT bytes_read;
    uint32_t calculated_crc = 0;
    app_descriptor_t image_descriptor = {0};
    uint32_t *flash_ptr;
    uint32_t data;
    uint32_t counter;
    int key_count = 0;
    bootloader_status_t status = BL_OK;
    bool success = false;

    do {
        /* Check for flash write protection */
        if (bootloader_get_protection_status() & BL_PROTECTION_WRP) {
            printf("Flash write protection enabled\r\n");
            display_static_message(
                "Flash is write protected,\n"
                "cannot update firmware.\n"
                "Press Start button to disable\n"
                "write protection and restart.");

            key_count = 0;
            for (int i = 0; i < 200; i++) {
                delay_with_usb(50);
                if (keypad_poll() == KEYPAD_START) {
                    key_count++;
                } else {
                    key_count = 0;
                }
                if (key_count > 2) {
                    display_static_message(
                        "Disabling write protection\n"
                        "and restarting...");
                    if (bootloader_config_protection(BL_PROTECTION_NONE) != BL_OK) {
                        display_static_message(
                            "Unable to disable\n"
                            "write protection!");
                    }
                    while(1) { }
                    break;
                }
            }
            return false;
        }

        /* Open firmware file, if it exists */
        res = f_open(&fp, FW_FILENAME, FA_READ);
        if (res != FR_OK) {
            printf("Unable to open firmware file: %d\r\n", res);
            break;
        }
        file_open = true;

        /* Check size of the firmware file */
        if (bootloader_check_size(f_size(&fp)) != BL_OK) {
            printf("Firmware size is invalid: %lu\r\n", f_size(&fp));
            break;
        }
        printf("Firmware size is okay.\r\n");

        /* Calculate the firmware file checksum */
        __HAL_CRC_DR_RESET(&hcrc);
        bytes_remaining = f_size(&fp) - 4;
        do {
            bytes_to_read = MIN(sizeof(buf), bytes_remaining);
            res = f_read(&fp, buf, bytes_to_read, &bytes_read);
            if (res == FR_OK) {
                /* This check should never fail, but safer to do it anyways */
                if ((bytes_read % 4) != 0) {
                    printf("Bytes read are not word aligned\r\n");
                    break;
                }

                calculated_crc = HAL_CRC_Accumulate(&hcrc, (uint32_t *)buf, bytes_read / 4);

                bytes_remaining -= bytes_read;

                /* Break if at EOF */
                if (bytes_read < bytes_to_read) {
                    break;
                }
            } else {
                printf("File read error: %d\r\n", res);
                break;
            }
        } while (res == FR_OK && bytes_remaining > 0);

        __HAL_RCC_CRC_FORCE_RESET();
        __HAL_RCC_CRC_RELEASE_RESET();

        /* Read the app descriptor from the end of the firmware file */
        res = f_lseek(&fp, f_tell(&fp) - (sizeof(app_descriptor_t) - 4));
        if (res != FR_OK) {
            printf("Unable to seek to read the firmware file descriptor: %d\r\n", res);
            break;
        }
        res = f_read(&fp, &image_descriptor, sizeof(app_descriptor_t), &bytes_read);
        if (res != FR_OK || bytes_read != sizeof(app_descriptor_t)) {
            printf("Unable to read the firmware file descriptor: %d\r\n", res);
            break;
        }

        f_rewind(&fp);

        if (calculated_crc != image_descriptor.crc32) {
            printf("Firmware checksum mismatch: %08lX != %08lX\r\n",
                image_descriptor.crc32, calculated_crc);
            break;
        }
        printf("Firmware checksum is okay.\r\n");

        if (image_descriptor.magic_word == APP_DESCRIPTOR_MAGIC_WORD) {
            char msg_buf[256];
            sprintf(msg_buf,
                "Firmware found:\n"
                "\n"
                "%s\n"
                "%s (%s)\n"
                "\n"
                "Press Start button to proceed\n"
                "with the update.",
                image_descriptor.project_name,
                image_descriptor.version,
                image_descriptor.build_describe);
            display_static_message(msg_buf);
        } else {
            display_static_message(
                "Firmware found.\n"
                "\n"
                "Press Start button to proceed\n"
                "with the update.");
        }

        key_count = 0;
        do {
            delay_with_usb(50);
            if (keypad_poll() == KEYPAD_START) {
                key_count++;
            } else {
                key_count = 0;
            }
            if (key_count > 2) {
                display_static_message("Starting firmware update...");
                delay_with_usb(1000);
                break;
            }
        } while (1);

        /* Make sure the USB storage device is still there */
        if (!usb_msc_is_mounted()) {
            break;
        }

        /* Initialize bootloader and prepare for flash programming */
        bootloader_init();

        /* Erase existing flash contents */
        display_static_message("Erasing flash...");
        if (bootloader_erase() != BL_OK) {
            printf("Error erasing flash\r\n");
            display_static_message("Unable to erase flash!");
            break;
        }
        printf("Flash erased\r\n");

        /* Start programming */
        display_static_message("Programming firmware...");
        counter = 0;
        bootloader_flash_begin();
        do {
            data = 0xFFFFFFFF;
            res = f_read(&fp, &data, 4, &bytes_read);
            if (res == FR_OK && bytes_read > 0) {
                status = bootloader_flash_next(data);
                if (status == BL_OK) {
                    counter++;
                } else {
                    printf("Programming error at byte %lu\r\n", (counter * 4));
                    break;
                }
            }
        } while (res == FR_OK && bytes_read > 0);

        bootloader_flash_end();

        if (status != BL_OK || res != FR_OK) {
            display_static_message("Programming error!");
            break;
        }

        printf("Firmware programmed\r\n");

        f_rewind(&fp);

        /* Verify flash content */
        display_static_message("Verifying firmware...");

        flash_ptr = (uint32_t*)APP_ADDRESS;
        counter = 0;
        status = BL_OK;
        do {
            res = f_read(&fp, buf, sizeof(buf), &bytes_read);
            if (res != FR_OK) {
                printf("File read error: %d\r\n", res);
                break;
            }
            if (memcmp(flash_ptr, buf, bytes_read) == 0) {
                counter += bytes_read;
                flash_ptr += bytes_read / 4;
            } else {
                printf("Verify error at byte %lu\r\n", counter);
                status = BL_CHKS_ERROR;
                break;
            }
        } while (res == FR_OK && bytes_read > 0);

        if (status != BL_OK || res != FR_OK) {
            display_static_message("Verification error!");
            break;
        }
        printf("Firmware verified\r\n");

        display_static_message("Verification complete");
        delay_with_usb(1000);

#if(USE_WRITE_PROTECTION)
        /* Enable flash write protection */
        display_static_message(
            "Enabling write protection\n"
            "and restarting...");
        delay_with_usb(1000);
        if (bootloader_config_protection(BL_PROTECTION_WRP) != BL_OK) {
            display_static_message(
                "Unable to enable\n"
                "write protection!");
            while(1) { }
        }
#endif

        success = true;

    } while (0);

    if (file_open) {
        f_close(&fp);
    }

    return success;
}

void start_system_memory()
{
    printf("Start System Memory\r\n");
    bootloader_jump_to_sysmem();
}

bool start_application()
{
    printf("Check for application\r\n");

    if (bootloader_check_for_application() == BL_OK) {
        /* Verify application checksum */
        if(bootloader_verify_checksum(&hcrc) != BL_OK) {
            printf("Checksum Error.\r\n");
            return false;
        } else {
            printf("Checksum OK.\r\n");
        }
        printf("Launching Application...\r\n");

        /* De-initialize all the peripherals */
        crc_deinit();
        tim_deinit();
        spi_deinit();
        i2c_deinit();
        gpio_deinit();

        /* Start the application */
        bootloader_jump_to_application();
    } else {
        return false;
    }
    return true;
}

void delay_with_usb(uint32_t delay)
{
    /*
     * Based on HAL_Delay() with USB host processing
     * inside the busy loop.
     */

    uint32_t tickstart = HAL_GetTick();
    uint32_t wait = delay;

    if (wait < HAL_MAX_DELAY) {
        wait += (uint32_t)(uwTickFreq);
    }

    while ((HAL_GetTick() - tickstart) < wait) {
        usb_host_process();
    }
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) {
    }
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
    printf("Assert failed: file %s on line %ld\r\n", file, line);
}
#endif /* USE_FULL_ASSERT */
