#include "usbh_serial_ftdi.h"

/* Vendor Request Interface */
#define FTDI_SIO_RESET          0   /* Reset the port */
#define FTDI_SIO_MODEM_CTRL     1   /* Set the modem control register */
#define FTDI_SIO_SET_FLOW_CTRL  2   /* Set flow control register */
#define FTDI_SIO_SET_BAUD_RATE  3   /* Set baud rate */
#define FTDI_SIO_SET_DATA       4   /* Set the data characteristics of the port */
#define FTDI_SIO_GET_STATUS     5   /* Retrieve current value of status reg */
#define FTDI_SIO_SET_EVENT_CHAR 6   /* Set the event character */
#define FTDI_SIO_SET_ERROR_CHAR 7   /* Set the error character */
#define FTDI_SIO_SET_LATENCY    9   /* Set the latency timer */
#define FTDI_SIO_GET_LATENCY    10  /* Read the latency timer */
#define FTDI_SIO_SET_BITMODE    11  /* Set the bit bang I/O mode */
#define FTDI_SIO_GET_BITMODE    12  /* Read pin states from any mode */
#define FTDI_SIO_READ_EEPROM    144 /* Read eeprom word */
#define FTDI_SIO_WRITE_EEPROM   145 /* Write eeprom word */
#define FTDI_SIO_ERASE_EEPROM   146 /* Erase entire eeprom */

/* Port Identifier Table */
#define FTDI_PIT_DEFAULT    0   /* SIOA */
#define FTDI_PIT_SIOA       1   /* SIOA */
#define FTDI_PIT_SIOB       2   /* SIOB */
#define FTDI_PIT_PARALLEL   3   /* Parallel */

static USBH_StatusTypeDef USBH_FTDI_InterfaceInit(USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef USBH_FTDI_InterfaceDeInit(USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef USBH_FTDI_ClassRequest(USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef USBH_FTDI_Process(USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef USBH_FTDI_SOFProcess(USBH_HandleTypeDef *phost);

static uint32_t baudrate_get_divisor(const USBH_HandleTypeDef *phost, uint32_t baud);

static void FTDI_ProcessTransmission(USBH_HandleTypeDef *phost);
static void FTDI_ProcessReception(USBH_HandleTypeDef *phost);

USBH_ClassTypeDef VENDOR_SERIAL_FTDI_Class = {
    "VENDOR_SERIAL_FTDI",
    0xFFU, /* VENDOR Class ID */
    USBH_FTDI_InterfaceInit,
    USBH_FTDI_InterfaceDeInit,
    USBH_FTDI_ClassRequest,
    USBH_FTDI_Process,
    USBH_FTDI_SOFProcess,
    NULL,
};

bool USBH_FTDI_IsDeviceType(USBH_HandleTypeDef *phost)
{
    if (!phost) {
        return false;
    }

    if (phost->device.CfgDesc.Itf_Desc[0].bInterfaceClass != VENDOR_SERIAL_FTDI_Class.ClassCode) {
        return false;
    }

    if (phost->device.DevDesc.idVendor != 0x403) {
        return false;
    }

    switch (phost->device.DevDesc.idProduct) {
    case 0x6001:
    case 0x6010:
    case 0x6011:
    case 0x6014:
    case 0x6015:
    case 0xE2E6:
        return true;
        break;
    default:
        return false;
    }
}

USBH_StatusTypeDef USBH_FTDI_InterfaceInit(USBH_HandleTypeDef *phost)
{
    USBH_StatusTypeDef status;
    uint8_t interface;
    FTDI_HandleTypeDef *FTDI_Handle;

    USBH_UsrLog("FTDI: USBH_FTDI_InterfaceInit");

    if (phost->device.DevDesc.idVendor != 0x403) {
        USBH_UsrLog("Vendor device is not FTDI");
        return USBH_NOT_SUPPORTED;
    }

    switch (phost->device.DevDesc.idProduct) {
    case 0x6001:
    case 0x6010:
    case 0x6011:
    case 0x6014:
    case 0x6015:
    case 0xE2E6:
        break;
    default:
        USBH_UsrLog("FTDI: Unrecognized PID");
        return USBH_NOT_SUPPORTED;
    }

    interface = USBH_FindInterface(phost, 0xFFU, 0xFFU, 0xFFU);

    if (interface == 0xFFU || interface >= USBH_MAX_NUM_INTERFACES) {
        USBH_UsrLog("FTDI: Cannot find the interface for Vendor Serial FTDI class.");
        return USBH_FAIL;
    }

    status = USBH_SelectInterface(phost, interface);
    if (status != USBH_OK) {
        return USBH_FAIL;
    }

    phost->pActiveClass->pData = (FTDI_HandleTypeDef *)USBH_malloc(sizeof(FTDI_HandleTypeDef));
    FTDI_Handle = (FTDI_HandleTypeDef *)phost->pActiveClass->pData;

    if (!FTDI_Handle) {
        USBH_ErrLog("FTDI: Cannot allocate memory for FTDI Handle");
        return USBH_FAIL;
    }

    USBH_memset(FTDI_Handle, 0, sizeof(FTDI_HandleTypeDef));

    //TODO compare this to all device IDs in the FreeBSD code, possibly changing how this works
    switch (phost->device.DevDesc.bcdDevice) {
    case 0x200:     //AM
        USBH_UsrLog("FTDI: Type A chip");
        FTDI_Handle->type = USBH_FTDI_TYPE_A;
        FTDI_Handle->port_no = 0;
        break;
    case 0x400:     //BM
    case 0x500:     //2232C
    case 0x600:     //R
    case 0x1000:    //230X
        USBH_UsrLog("FTDI: Type B chip");
        FTDI_Handle->type = USBH_FTDI_TYPE_B;
        FTDI_Handle->port_no = 0;
        break;
    case 0x700:     //2232H;
    case 0x800:     //4232H;
    case 0x900:     //232H;
        USBH_UsrLog("FTDI: Type H chip");
        FTDI_Handle->type = USBH_FTDI_TYPE_H;
        FTDI_Handle->port_no = FTDI_PIT_SIOA + phost->device.CfgDesc.Itf_Desc[interface].bInterfaceNumber;
        break;
    default:
        USBH_ErrLog("FTDI: Unrecognized chip type");
        return USBH_NOT_SUPPORTED;
    }

    if (phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[0].bEndpointAddress & 0x80U) {
        FTDI_Handle->data_intf.in_ep = (phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[0].bEndpointAddress);
        FTDI_Handle->data_intf.in_ep_size = phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[0].wMaxPacketSize;
    } else {
        FTDI_Handle->data_intf.out_ep = (phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[0].bEndpointAddress);
        FTDI_Handle->data_intf.out_ep_size = phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[0].wMaxPacketSize;
    }

    if (phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[1].bEndpointAddress & 0x80U) {
        FTDI_Handle->data_intf.in_ep = (phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[1].bEndpointAddress);
        FTDI_Handle->data_intf.in_ep_size = phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[1].wMaxPacketSize;
    } else {
        FTDI_Handle->data_intf.out_ep = (phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[1].bEndpointAddress);
        FTDI_Handle->data_intf.out_ep_size = phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[1].wMaxPacketSize;
    }

    FTDI_Handle->data_intf.out_pipe = USBH_AllocPipe(phost, FTDI_Handle->data_intf.out_ep);
    FTDI_Handle->data_intf.in_pipe = USBH_AllocPipe(phost, FTDI_Handle->data_intf.in_ep);

    USBH_OpenPipe(phost, FTDI_Handle->data_intf.out_pipe, FTDI_Handle->data_intf.out_ep,
        phost->device.address, phost->device.speed,
        USB_EP_TYPE_BULK, FTDI_Handle->data_intf.out_ep_size);

    USBH_OpenPipe(phost, FTDI_Handle->data_intf.in_pipe, FTDI_Handle->data_intf.in_ep,
        phost->device.address, phost->device.speed, USB_EP_TYPE_BULK,
        FTDI_Handle->data_intf.in_ep_size);

    FTDI_Handle->state = USBH_FTDI_STATE_IDLE;
    if (FTDI_Handle->poll < FTDI_MIN_POLL) {
        FTDI_Handle->poll = FTDI_MIN_POLL;
    }

    USBH_LL_SetToggle(phost, FTDI_Handle->data_intf.in_pipe, 0U);
    USBH_LL_SetToggle(phost, FTDI_Handle->data_intf.out_pipe, 0U);

    USBH_UsrLog("FTDI: InEpSize=%d, OutEpSize=%d",
        FTDI_Handle->data_intf.in_ep_size,
        FTDI_Handle->data_intf.out_ep_size);

    FTDI_Handle->data_intf.in_ep_buffer = (uint8_t *)USBH_malloc(FTDI_Handle->data_intf.in_ep_size);
    if (!FTDI_Handle->data_intf.in_ep_buffer) {
        USBH_ErrLog("FTDI: Cannot allocate memory for FTDI receive buffer");
        return USBH_FAIL;
    }
    FTDI_Handle->rx_data = FTDI_Handle->data_intf.in_ep_buffer;
    FTDI_Handle->rx_data_length = FTDI_Handle->data_intf.in_ep_size;
    USBH_memset(FTDI_Handle->data_intf.in_ep_buffer, 0, FTDI_Handle->data_intf.in_ep_size);

    FTDI_Handle->data_intf.out_ep_buffer = (uint8_t *)USBH_malloc(FTDI_Handle->data_intf.out_ep_size);
    if (!FTDI_Handle->data_intf.out_ep_buffer) {
        USBH_ErrLog("FTDI: Cannot allocate memory for FTDI transmit buffer");
        return USBH_FAIL;
    }
    USBH_memset(FTDI_Handle->data_intf.out_ep_buffer, 0, FTDI_Handle->data_intf.out_ep_size);

    return USBH_OK;
}

USBH_StatusTypeDef USBH_FTDI_InterfaceDeInit(USBH_HandleTypeDef *phost)
{
    FTDI_HandleTypeDef *FTDI_Handle = (FTDI_HandleTypeDef *)phost->pActiveClass->pData;

    USBH_UsrLog("FTDI: USBH_FTDI_InterfaceDeInit");

    if (!phost->pActiveClass->pData) {
        return USBH_FAIL;
    }

    /* Close and free the I/O pipes */

    if (FTDI_Handle->data_intf.in_pipe) {
        USBH_ClosePipe(phost, FTDI_Handle->data_intf.in_pipe);
        USBH_FreePipe(phost, FTDI_Handle->data_intf.in_pipe);
        FTDI_Handle->data_intf.in_pipe = 0U;
    }

    if (FTDI_Handle->data_intf.out_pipe) {
        USBH_ClosePipe(phost, FTDI_Handle->data_intf.out_pipe);
        USBH_FreePipe(phost, FTDI_Handle->data_intf.out_pipe);
        FTDI_Handle->data_intf.out_pipe = 0U;
    }

    /* Free the I/O data buffers */

    if (FTDI_Handle->data_intf.in_ep_buffer) {
        USBH_free(FTDI_Handle->data_intf.in_ep_buffer);
        FTDI_Handle->data_intf.in_ep_buffer = NULL;
        FTDI_Handle->rx_data = NULL;
        FTDI_Handle->rx_data_length = 0;
    }

    if (FTDI_Handle->data_intf.out_ep_buffer) {
        USBH_free(FTDI_Handle->data_intf.out_ep_buffer);
        FTDI_Handle->data_intf.out_ep_buffer = NULL;
        FTDI_Handle->tx_data = NULL;
        FTDI_Handle->tx_data_length = 0;
    }

    /* Free the device handle */

    if (phost->pActiveClass->pData) {
        USBH_free(phost->pActiveClass->pData);
        phost->pActiveClass->pData = 0U;
    }

    return USBH_OK;
}

USBH_StatusTypeDef USBH_FTDI_Reset(USBH_HandleTypeDef *phost, int reset_type)
{
    FTDI_HandleTypeDef *FTDI_Handle = (FTDI_HandleTypeDef *)phost->pActiveClass->pData;
    phost->Control.setup.b.bmRequestType = USB_H2D | USB_REQ_TYPE_VENDOR | USB_REQ_RECIPIENT_DEVICE;

    phost->Control.setup.b.bRequest = FTDI_SIO_RESET;
    phost->Control.setup.b.wIndex.w = FTDI_Handle->port_no;
    phost->Control.setup.b.wLength.w = 0U;
    phost->Control.setup.b.wValue.w = reset_type;

    return USBH_CtlReq(phost, NULL, 0);
}

uint32_t baudrate_get_divisor(const USBH_HandleTypeDef *phost, uint32_t baud)
{
    FTDI_HandleTypeDef *FTDI_Handle = (FTDI_HandleTypeDef *)phost->pActiveClass->pData;
    usbh_ftdi_type_t type = FTDI_Handle->type;
    static const uint8_t divfrac[8] = {0, 3, 2, 4, 1, 5, 6, 7};
    uint32_t divisor;

    if (type == USBH_FTDI_TYPE_A) {
        uint32_t divisor3 = ((48000000UL / 2) + baud / 2) / baud;
        USBH_DbgLog("FTDI: desired=%dbps, real=%dbps", baud, (48000000UL / 2) / divisor3);
        if ((divisor3 & 0x7) == 7)
            divisor3++; /* round x.7/8 up to x+1 */

        divisor = divisor3 >> 3;
        divisor3 &= 0x7;
        if (divisor3 == 1)
            divisor |= 0xc000;
        else if (divisor3 >= 4)
            divisor |= 0x4000;
        else if (divisor3 != 0)
            divisor |= 0x8000;
        else if (divisor == 1)
            divisor = 0;    /* special case for maximum baud rate */
    } else {
        if (type == USBH_FTDI_TYPE_B) {
            divisor = ((48000000UL / 2) + baud / 2) / baud;
            USBH_DbgLog("FTDI: desired=%dbps, real=%dbps", baud, (48000000UL / 2) / divisor);
        } else {
            /* hi-speed baud rate is 10-bit sampling instead of 16-bit */
            if (baud < 1200)
                baud = 1200;
            divisor = (120000000UL * 8 + baud * 5) / (baud * 10);
            USBH_DbgLog("FTDI: desired=%dbps, real=%dbps", baud, (120000000UL * 8) / divisor / 10);
        }
        divisor = (divisor >> 3) | (divfrac[divisor & 0x7] << 14);

        /* Deal with special cases for highest baud rates. */
        if (divisor == 1)
            divisor = 0;
        else if (divisor == 0x4001)
            divisor = 1;

        if (type == USBH_FTDI_TYPE_H)
            divisor |= 0x00020000;
    }
    return divisor;
}

USBH_StatusTypeDef USBH_FTDI_SetBaudRate(USBH_HandleTypeDef *phost, uint32_t baudrate)
{
    FTDI_HandleTypeDef *FTDI_Handle = (FTDI_HandleTypeDef *)phost->pActiveClass->pData;
    uint32_t divisor = baudrate_get_divisor(phost, baudrate);
    uint16_t wValue = (uint16_t)divisor;
    uint16_t wIndex = (uint16_t)(divisor >> 16) | (FTDI_Handle->port_no);

    phost->Control.setup.b.bmRequestType = USB_H2D | USB_REQ_TYPE_VENDOR | USB_REQ_RECIPIENT_DEVICE;

    phost->Control.setup.b.bRequest = FTDI_SIO_SET_BAUD_RATE;
    phost->Control.setup.b.wValue.w = wValue;
    phost->Control.setup.b.wIndex.w = wIndex;
    phost->Control.setup.b.wLength.w = 0U;

    return USBH_CtlReq(phost, NULL, 0);
}

USBH_StatusTypeDef USBH_FTDI_SetData(USBH_HandleTypeDef *phost, uint16_t data)
{
    FTDI_HandleTypeDef *FTDI_Handle = (FTDI_HandleTypeDef *)phost->pActiveClass->pData;
    phost->Control.setup.b.bmRequestType = USB_H2D | USB_REQ_TYPE_VENDOR | USB_REQ_RECIPIENT_DEVICE;

    phost->Control.setup.b.bRequest = FTDI_SIO_SET_DATA;
    phost->Control.setup.b.wValue.w = data;
    phost->Control.setup.b.wIndex.w = FTDI_Handle->port_no;
    phost->Control.setup.b.wLength.w = 0U;

    return USBH_CtlReq(phost, NULL, 0);
}

USBH_StatusTypeDef USBH_FTDI_SetFlowControl(USBH_HandleTypeDef *phost, uint8_t flow_control)
{
    FTDI_HandleTypeDef *FTDI_Handle = (FTDI_HandleTypeDef *)phost->pActiveClass->pData;
    phost->Control.setup.b.bmRequestType = USB_H2D | USB_REQ_TYPE_VENDOR | USB_REQ_RECIPIENT_DEVICE;

    phost->Control.setup.b.bRequest = FTDI_SIO_SET_FLOW_CTRL;

    if (flow_control && FTDI_SIO_XON_XOFF_HS) {
        phost->Control.setup.b.wValue.w = (0x13 << 8) | 0x11;
    } else {
        phost->Control.setup.b.wValue.w = 0;
    }
    phost->Control.setup.b.wIndex.bw.msb = flow_control;
    phost->Control.setup.b.wIndex.bw.lsb = FTDI_Handle->port_no;

    phost->Control.setup.b.wLength.w = 0U;

    return USBH_CtlReq(phost, NULL, 0);
}

USBH_StatusTypeDef USBH_FTDI_SetDtr(USBH_HandleTypeDef *phost, bool enabled)
{
    FTDI_HandleTypeDef *FTDI_Handle = (FTDI_HandleTypeDef *)phost->pActiveClass->pData;
    phost->Control.setup.b.bmRequestType = USB_H2D | USB_REQ_TYPE_VENDOR | USB_REQ_RECIPIENT_DEVICE;

    phost->Control.setup.b.bRequest = FTDI_SIO_MODEM_CTRL;
    phost->Control.setup.b.wValue.w = enabled ? FTDI_SIO_SET_DTR_HIGH : FTDI_SIO_SET_DTR_LOW;
    phost->Control.setup.b.wIndex.w = FTDI_Handle->port_no;
    phost->Control.setup.b.wLength.w = 0U;

    return USBH_CtlReq(phost, NULL, 0);
}

USBH_StatusTypeDef USBH_FTDI_SetRts(USBH_HandleTypeDef *phost, bool enabled)
{
    FTDI_HandleTypeDef *FTDI_Handle = (FTDI_HandleTypeDef *)phost->pActiveClass->pData;
    phost->Control.setup.b.bmRequestType = USB_H2D | USB_REQ_TYPE_VENDOR | USB_REQ_RECIPIENT_DEVICE;

    phost->Control.setup.b.bRequest = FTDI_SIO_MODEM_CTRL;
    phost->Control.setup.b.wValue.w = enabled ? FTDI_SIO_SET_RTS_HIGH : FTDI_SIO_SET_RTS_LOW;
    phost->Control.setup.b.wIndex.w = FTDI_Handle->port_no;
    phost->Control.setup.b.wLength.w = 0U;

    return USBH_CtlReq(phost, NULL, 0);
}

USBH_StatusTypeDef USBH_FTDI_ClassRequest(USBH_HandleTypeDef *phost)
{
    USBH_UsrLog("FTDI: USBH_FTDI_ClassRequest");

    phost->pUser(phost, HOST_USER_CLASS_ACTIVE);

    return USBH_OK;
}

USBH_StatusTypeDef USBH_FTDI_Process(USBH_HandleTypeDef *phost)
{
    USBH_StatusTypeDef status = USBH_BUSY;
    USBH_StatusTypeDef req_status = USBH_OK;
    FTDI_HandleTypeDef *FTDI_Handle = (FTDI_HandleTypeDef *)phost->pActiveClass->pData;

    switch (FTDI_Handle->state) {
    case USBH_FTDI_STATE_IDLE:
        status = USBH_OK;
        break;

    case USBH_FTDI_STATE_TRANSFER_DATA:
        FTDI_Handle->timer = phost->Timer;
        FTDI_ProcessTransmission(phost);
        FTDI_ProcessReception(phost);
        if (FTDI_Handle->data_rx_state == USBH_FTDI_IDLE && FTDI_Handle->data_tx_state == USBH_FTDI_IDLE) {
            FTDI_Handle->state = USBH_FTDI_STATE_IDLE;
        }
        break;

    case USBH_FTDI_STATE_ERROR:
        req_status = USBH_ClrFeature(phost, 0x00U);
        if (req_status == USBH_OK) {
            FTDI_Handle->state = USBH_FTDI_STATE_IDLE;
        }
        break;

    default:
        break;
    }

    return status;
}

USBH_StatusTypeDef USBH_FTDI_SOFProcess(USBH_HandleTypeDef *phost)
{
    FTDI_HandleTypeDef *FTDI_Handle = (FTDI_HandleTypeDef *)phost->pActiveClass->pData;

    if (FTDI_Handle->state == USBH_FTDI_STATE_IDLE) {
        if ((phost->Timer - FTDI_Handle->timer) >= FTDI_Handle->poll) {
            FTDI_Handle->state = USBH_FTDI_STATE_TRANSFER_DATA;
            FTDI_Handle->data_rx_state = USBH_FTDI_RECEIVE_DATA;

#if (USBH_USE_OS == 1U)
            phost->os_msg = (uint32_t)USBH_URB_EVENT;
#if (osCMSIS < 0x20000U)
            (void)osMessagePut(phost->os_event, phost->os_msg, 0U);
#else
            (void)osMessageQueuePut(phost->os_event, &phost->os_msg, 0U, 0U);
#endif
#endif
        }
    }
    return USBH_OK;
}

USBH_StatusTypeDef USBH_FTDI_Transmit(USBH_HandleTypeDef *phost, const uint8_t *pbuff, uint32_t length)
{
    USBH_StatusTypeDef status = USBH_BUSY;
    FTDI_HandleTypeDef *FTDI_Handle = (FTDI_HandleTypeDef *)phost->pActiveClass->pData;

    if (length > FTDI_Handle->data_intf.out_ep_size) {
        USBH_ErrLog("FTDI: Transmit size too large: %lu > %d",
            length, FTDI_Handle->data_intf.out_ep_size);
        return USBH_FAIL;
    }

    if ((FTDI_Handle->state == USBH_FTDI_STATE_IDLE) || (FTDI_Handle->state == USBH_FTDI_STATE_TRANSFER_DATA)) {
        USBH_memcpy(FTDI_Handle->data_intf.out_ep_buffer, pbuff, length);
        FTDI_Handle->tx_data = FTDI_Handle->data_intf.out_ep_buffer;
        FTDI_Handle->tx_data_length = length;
        FTDI_Handle->state = USBH_FTDI_STATE_TRANSFER_DATA;
        FTDI_Handle->data_tx_state = USBH_FTDI_SEND_DATA;
        status = USBH_OK;

#if (USBH_USE_OS == 1U)
        phost->os_msg = (uint32_t)USBH_CLASS_EVENT;
#if (osCMSIS < 0x20000U)
        (void)osMessagePut(phost->os_event, phost->os_msg, 0U);
#else
        (void)osMessageQueuePut(phost->os_event, &phost->os_msg, 0U, 0U);
#endif
#endif
    }
    return status;
}

void FTDI_ProcessTransmission(USBH_HandleTypeDef *phost)
{
    FTDI_HandleTypeDef *FTDI_Handle = (FTDI_HandleTypeDef *)phost->pActiveClass->pData;
    USBH_URBStateTypeDef URB_Status = USBH_URB_IDLE;

    switch (FTDI_Handle->data_tx_state) {
    case USBH_FTDI_SEND_DATA:
        if (FTDI_Handle->tx_data_length > FTDI_Handle->data_intf.out_ep_size) {
            /*
             * Right now this case is not possible, because we allocate TxData and limit
             * what can be sent based on OutEpSize
             */
            USBH_BulkSendData(phost,
                FTDI_Handle->tx_data,
                FTDI_Handle->data_intf.out_ep_size,
                FTDI_Handle->data_intf.out_pipe,
                1U);
        } else {
            USBH_BulkSendData(phost,
                FTDI_Handle->tx_data,
                (uint16_t)FTDI_Handle->tx_data_length,
                FTDI_Handle->data_intf.out_pipe,
                1U);
        }
        FTDI_Handle->data_tx_state = USBH_FTDI_SEND_DATA_WAIT;
        break;

    case USBH_FTDI_SEND_DATA_WAIT:
        URB_Status = USBH_LL_GetURBState(phost, FTDI_Handle->data_intf.out_pipe);

        /* Check if the transmission is complete */
        if (URB_Status == USBH_URB_DONE) {
            if (FTDI_Handle->tx_data_length > FTDI_Handle->data_intf.out_ep_size) {
                FTDI_Handle->tx_data_length -= FTDI_Handle->data_intf.out_ep_size;
                FTDI_Handle->tx_data += FTDI_Handle->data_intf.out_ep_size;
            } else {
                FTDI_Handle->tx_data_length = 0U;
            }

            if (FTDI_Handle->tx_data_length > 0U) {
                FTDI_Handle->data_tx_state = USBH_FTDI_SEND_DATA;
            } else {
                FTDI_Handle->data_tx_state = USBH_FTDI_IDLE;
                FTDI_Handle->tx_data = NULL;
                FTDI_Handle->tx_data_length = 0;
                USBH_FTDI_TransmitCallback(phost);
            }

#if (USBH_USE_OS == 1U)
            phost->os_msg = (uint32_t)USBH_CLASS_EVENT;
#if (osCMSIS < 0x20000U)
            (void)osMessagePut(phost->os_event, phost->os_msg, 0U);
#else
            (void)osMessageQueuePut(phost->os_event, &phost->os_msg, 0U, 0U);
#endif
#endif
        } else {
            if (URB_Status == USBH_URB_NOTREADY) {
                FTDI_Handle->data_tx_state = USBH_FTDI_SEND_DATA;

#if (USBH_USE_OS == 1U)
                phost->os_msg = (uint32_t)USBH_CLASS_EVENT;
#if (osCMSIS < 0x20000U)
                (void)osMessagePut(phost->os_event, phost->os_msg, 0U);
#else
                (void)osMessageQueuePut(phost->os_event, &phost->os_msg, 0U, 0U);
#endif
#endif
            }
        }
        break;
    default:
        break;
    }
}

void FTDI_ProcessReception(USBH_HandleTypeDef *phost)
{
    FTDI_HandleTypeDef *FTDI_Handle = (FTDI_HandleTypeDef *)phost->pActiveClass->pData;
    USBH_URBStateTypeDef URB_Status = USBH_URB_IDLE;
    uint32_t length;

    switch (FTDI_Handle->data_rx_state) {
    case USBH_FTDI_IDLE:
        break;
    case USBH_FTDI_RECEIVE_DATA:
        USBH_BulkReceiveData(phost,
            FTDI_Handle->rx_data,
            FTDI_Handle->data_intf.in_ep_size,
            FTDI_Handle->data_intf.in_pipe);

        FTDI_Handle->data_rx_state = USBH_FTDI_RECEIVE_DATA_WAIT;

        break;

    case USBH_FTDI_RECEIVE_DATA_WAIT:
        URB_Status = USBH_LL_GetURBState(phost, FTDI_Handle->data_intf.in_pipe);

        if (URB_Status == USBH_URB_DONE) {
            /*
             * This code attempts to handle short reads, and truncates the two
             * status bytes that are prepended to the data coming from the
             * FTDI chip. It may not be doing everything correctly, and it may
             * not hold up under all use cases. However, it is sufficiently
             * functional for the simple intended use within this codebase.
             */
            length = USBH_LL_GetLastXferSize(phost, FTDI_Handle->data_intf.in_pipe);

            if (((FTDI_Handle->rx_data_length - length) > 0U) && (length > FTDI_Handle->data_intf.in_ep_size)) {
                if (length >= 2) {
                    memmove(FTDI_Handle->rx_data, FTDI_Handle->rx_data + 2, length - 2);
                    length -= 2;
                }
                FTDI_Handle->rx_data_length -= length;
                FTDI_Handle->rx_data += length;
                FTDI_Handle->data_rx_state = USBH_FTDI_RECEIVE_DATA;
            } else {
                FTDI_Handle->data_rx_state = USBH_FTDI_IDLE;

                size_t recv_length;
                if (FTDI_Handle->rx_data == FTDI_Handle->data_intf.in_ep_buffer) {
                    if (length >= 2) {
                        FTDI_Handle->rx_data += 2;
                        recv_length = length - 2;
                    } else {
                        recv_length = length;
                    }
                } else {
                    recv_length = FTDI_Handle->rx_data - FTDI_Handle->data_intf.in_ep_buffer;
                    if (length >= 2) {
                        memmove(FTDI_Handle->rx_data, FTDI_Handle->rx_data + 2, length - 2);
                        recv_length -= 2;
                    }
                    FTDI_Handle->rx_data = FTDI_Handle->data_intf.in_ep_buffer;
                }

                if (recv_length > 0) {
                    USBH_FTDI_ReceiveCallback(phost, FTDI_Handle->rx_data, recv_length);
                }
                FTDI_Handle->rx_data = FTDI_Handle->data_intf.in_ep_buffer;
                FTDI_Handle->rx_data_length = FTDI_Handle->data_intf.in_ep_size;
            }

#if (USBH_USE_OS == 1U)
            phost->os_msg = (uint32_t)USBH_CLASS_EVENT;
#if (osCMSIS < 0x20000U)
            (void)osMessagePut(phost->os_event, phost->os_msg, 0U);
#else
            (void)osMessageQueuePut(phost->os_event, &phost->os_msg, 0U, 0U);
#endif
#endif
        }
        break;
    default:
        break;
    }
}

__weak void USBH_FTDI_TransmitCallback(USBH_HandleTypeDef *phost)
{
    /* Prevent unused argument(s) compilation warning */
    UNUSED(phost);
}

__weak void USBH_FTDI_ReceiveCallback(USBH_HandleTypeDef *phost, uint8_t *data, size_t length)
{
    /* Prevent unused argument(s) compilation warning */
    UNUSED(phost);
}
