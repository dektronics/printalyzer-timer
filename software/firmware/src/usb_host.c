#include "usb_host.h"

#include <cmsis_os.h>

#include "stm32f4xx.h"
#include "usbh_core.h"
#include "board_config.h"

#define LOG_TAG "usb_host"
#include <elog.h>

extern SMBUS_HandleTypeDef hsmbus2;

extern void Error_Handler(void);

static const uint8_t USB2422_ADDRESS = 0x2C << 1;

static HAL_StatusTypeDef smbus_master_block_write(
    SMBUS_HandleTypeDef *hsmbus,
    uint8_t dev_address, uint8_t mem_address,
    const uint8_t *data, uint8_t len);

void usb_hc_low_level_init(struct usbh_bus *bus)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

    /* Initialize the peripherals clock */
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_CLK48;
    PeriphClkInitStruct.PLLSAI.PLLSAIM = 8;
    PeriphClkInitStruct.PLLSAI.PLLSAIN = 96;
    PeriphClkInitStruct.PLLSAI.PLLSAIQ = 2;
    PeriphClkInitStruct.PLLSAI.PLLSAIP = RCC_PLLSAIP_DIV4;
    PeriphClkInitStruct.PLLSAIDivQ = 1;
    PeriphClkInitStruct.Clk48ClockSelection = RCC_CLK48CLKSOURCE_PLLSAIP;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK) {
        Error_Handler();
    }

    __HAL_RCC_GPIOB_CLK_ENABLE();

    /*
     * USB_OTG_HS GPIO Configuration
     * PB14     ------> USB_OTG_HS_DM
     * PB15     ------> USB_OTG_HS_DP
     */
    GPIO_InitStruct.Pin = GPIO_PIN_14|GPIO_PIN_15;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF12_OTG_HS_FS;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* Peripheral clock enable */
    __HAL_RCC_USB_OTG_HS_CLK_ENABLE();

    /* Peripheral interrupt init */
    HAL_NVIC_SetPriority(OTG_HS_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(OTG_HS_IRQn);
}

bool usb_host_init()
{
    /* Make sure the USB2422 hub is in reset, and the VBUS signal is low */
    HAL_GPIO_WritePin(GPIOB, USB_HUB_RESET_Pin|USB_DRIVE_VBUS_Pin, GPIO_PIN_RESET);

    /*
     * May need to modify this library function so the error code
     * is actually returned. It isn't, so not handling it.
     */
    usbh_initialize(0, USB_OTG_HS_PERIPH_BASE);
    return true;
}

bool usb_hub_init()
{
    HAL_StatusTypeDef ret = HAL_OK;

    /*
     * Initialize the USB2422 hub that sits between the MCU host controller
     * and the external USB ports.
     */

    /* Bring the USB2422 out of reset */
    HAL_GPIO_WritePin(USB_HUB_RESET_GPIO_Port, USB_HUB_RESET_Pin, GPIO_PIN_SET);
    osDelay(2);

    /* Enabling VBUS pin */
    HAL_GPIO_WritePin(USB_DRIVE_VBUS_GPIO_Port, USB_DRIVE_VBUS_Pin, GPIO_PIN_SET);
    osDelay(2);

    log_d("Loading USB2422 configuration");

    /* Initial contiguous set of USB2422 configuration values */
    static const uint8_t USB2422_MEM0[] = {
        0x24, /* VIDL (Vendor ID, LSB) */
        0x04, /* VIDM (Vendor ID, MSB) */
        0x22, /* PIDL (Product ID, LSB) */
        0x24, /* PIDM (Product ID, MSB) */
        0xA0, /* DIDL (Device ID, LSB) */
        0x00, /* DIDM (Device ID, MSB) */
        0xAB, /* CFG1 (SELF_BUS_PWR, HS_DISABLE, EOP_DISABLE, CURRENT_SNS=port, PORT_PWR=port) */
        0x20, /* CFG2 (Default) */
        0x02, /* CFG3 (Default) */
        0x00, /* NRD (all downstream ports removable) */
        0x00, /* PDS */
        0x00, /* PDB */
        0x01, /* MAXPS (MAX_PWR_SP=2mA) */
        0x32, /* MAXPB (MAX_PWR_BP=100mA) */
        0x01, /* HCMCS (HC_MAX_C_SP=2mA) */
        0x32, /* HCMCB (MAX_PWR_BP=100mA) */
        0x32  /* PWRT (POWER_ON_TIME=100ms) */
    };
    ret = smbus_master_block_write(&hsmbus2, USB2422_ADDRESS, 0x00, USB2422_MEM0, sizeof(USB2422_MEM0));
    if (ret != HAL_OK) {
        return false;
    }

    /* Enable pin-swap on all ports */
    static const uint8_t USB2422_PRTSP = 0x07;
    ret = smbus_master_block_write(&hsmbus2, USB2422_ADDRESS, 0xFA, &USB2422_PRTSP, 1);
    if (ret != HAL_OK) {
        return false;
    }

    /* Finish configuration and trigger USB_ATTACH */
    log_d("Triggering USB attach");
    static const uint8_t USB2422_STCD = 0x01;
    ret = smbus_master_block_write(&hsmbus2, USB2422_ADDRESS, 0xFF, &USB2422_STCD, 1);
    if (ret != HAL_OK) {
        return false;
    }

    return true;
}

HAL_StatusTypeDef smbus_master_block_write(
    SMBUS_HandleTypeDef *hsmbus,
    uint8_t dev_address, uint8_t mem_address,
    const uint8_t *data, uint8_t len)
{
    HAL_StatusTypeDef ret = HAL_OK;
    HAL_SMBUS_StateTypeDef state = HAL_SMBUS_STATE_RESET;
    uint8_t buf[34];

    if (!data || len == 0 || len > 32) { return HAL_ERROR; }

    /* Prepare the block write buffer */
    buf[0] = mem_address;
    buf[1] = len;
    memcpy(buf + 2, data, len);

    /* Send the block write to the bus */
    ret = HAL_SMBUS_Master_Transmit_IT(hsmbus,
        dev_address, buf, len + 2,
        SMBUS_FIRST_AND_LAST_FRAME_NO_PEC);
    if (ret != HAL_OK) {
        log_w("HAL_SMBUS_Master_Transmit_IT error: %d", ret);
        return ret;
    }

    /* Busy-wait until the block write is finished */
    do {
        state = HAL_SMBUS_GetState(hsmbus);
        switch (state) {
        case HAL_SMBUS_STATE_ABORT:
        case HAL_SMBUS_STATE_ERROR:
            ret = HAL_ERROR;
            break;
        case HAL_SMBUS_STATE_TIMEOUT:
            ret = HAL_TIMEOUT;
            break;
        default:
            break;
        }
        osThreadYield();
    } while(state != HAL_SMBUS_STATE_READY && ret == HAL_OK);

    if (ret != HAL_OK) {
        log_w("HAL_SMBUS_GetState error: %d", state);
        return ret;
    }

    return HAL_OK;
}
