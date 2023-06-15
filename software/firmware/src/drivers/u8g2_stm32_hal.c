#include "u8g2_stm32_hal.h"

#include "stm32f4xx_hal.h"
#include <string.h>
#include <cmsis_os.h>
#include <esp_log.h>

static const char *TAG = "u8g2_hal";

static u8g2_display_handle_t display_handle = {0};

void u8g2_stm32_hal_init(u8g2_t *u8g2, const u8g2_display_handle_t *u8g2_display_handle)
{
    if (!u8g2_display_handle) {
        ESP_LOGE(TAG, "Display handle not initialized");
        return;
    }

    /* Make a local copy of the parameters for the callback functions. */
    memcpy(&display_handle, u8g2_display_handle, sizeof(u8g2_display_handle_t));

    /* Initialize the display driver */
    u8g2_Setup_ssd1322_nhd_256x64_f(u8g2, U8G2_R0, u8g2_stm32_spi_byte_cb, u8g2_stm32_gpio_and_delay_cb);
}

uint8_t u8g2_stm32_spi_byte_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    //ESP_LOGD(TAG, "spi_byte_cb: Received a msg: %d, arg_int: %d, arg_ptr: %p", msg, arg_int, arg_ptr);

    switch (msg) {
    case U8X8_MSG_BYTE_SET_DC:
        /* Set DC to arg_int */
        HAL_GPIO_WritePin(display_handle.dc_gpio_port, display_handle.dc_gpio_pin, arg_int ? GPIO_PIN_SET : GPIO_PIN_RESET);
        break;
    case U8X8_MSG_BYTE_INIT:
        /* Disable chip select */
        HAL_GPIO_WritePin(display_handle.cs_gpio_port, display_handle.cs_gpio_pin, GPIO_PIN_SET);
        break;
    case U8X8_MSG_BYTE_SEND: {
        /* Transmit bytes in arg_ptr, length is arg_int bytes */
        HAL_StatusTypeDef ret = HAL_SPI_Transmit(display_handle.hspi, (uint8_t *)arg_ptr, arg_int, HAL_MAX_DELAY);
        if (ret != HAL_OK) {
            ESP_LOGE(TAG, "HAL_SPI_Transmit error: %d", ret);
        }
        break;
    }
    case U8X8_MSG_BYTE_START_TRANSFER:
        /* Drop CS low to enable */
        HAL_GPIO_WritePin(display_handle.cs_gpio_port, display_handle.cs_gpio_pin, GPIO_PIN_RESET);
        break;
    case U8X8_MSG_BYTE_END_TRANSFER:
        /* Bring CS high to disable */
        HAL_GPIO_WritePin(display_handle.cs_gpio_port, display_handle.cs_gpio_pin, GPIO_PIN_SET);
        break;
    default:
        return 0;
    }
    return 1;
}

uint8_t u8g2_stm32_gpio_and_delay_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    //ESP_LOGD(TAG, "gpio_and_delay_cb: Received a msg: %d, arg_int: %d, arg_ptr: %p", msg, arg_int, arg_ptr);

    switch(msg) {
    case U8X8_MSG_GPIO_AND_DELAY_INIT:
        /*
         * Initialize the GPIO and DELAY HAL functions,
         * pin init for DC, RESET, CS.
         * These are all initialized elsewhere.
         */
        osDelay(1);
        break;
    case U8X8_MSG_DELAY_MILLI:
        /* Delay for the number of milliseconds passed in through arg_int */
        osDelay(arg_int);
        break;
    case U8X8_MSG_GPIO_CS:
        HAL_GPIO_WritePin(display_handle.cs_gpio_port, display_handle.cs_gpio_pin, arg_int ? GPIO_PIN_SET : GPIO_PIN_RESET);
        break;
    case U8X8_MSG_GPIO_RESET:
        /* Set the GPIO reset pin to the value passed in through arg_int */
        HAL_GPIO_WritePin(display_handle.reset_gpio_port, display_handle.reset_gpio_pin, arg_int ? GPIO_PIN_SET : GPIO_PIN_RESET);
        break;
    default:
        return 0;
    }
    return 1;
}
