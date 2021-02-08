#include "usbh_serial_slcom.h"

/* Request types */
#define SLCOM_WRITE           0x41
#define SLCOM_READ            0xc1

/* Request codes */
#define SLCOM_IFC_ENABLE      0x00
#define SLCOM_SET_BAUDDIV     0x01
#define SLCOM_SET_LINE_CTL    0x03
#define SLCOM_SET_BREAK       0x05
#define SLCOM_SET_MHS         0x07
#define SLCOM_GET_MDMSTS      0x08
#define SLCOM_SET_EVENTMASK   0x0B
#define SLCOM_GET_EVENTMASK   0x0C
#define SLCOM_GET_COMM_STATUS 0x10
#define SLCOM_SET_FLOW        0x13
#define SLCOM_GET_EVENTSTATE  0x16
#define SLCOM_SET_BAUDRATE    0x1e
#define SLCOM_VENDOR_SPECIFIC 0xff

/* SLCOM_IFC_ENABLE values */
#define SLCOM_IFC_ENABLE_DIS  0x00
#define SLCOM_IFC_ENABLE_EN   0x01

/* SLCOM_SET_MHS/SLCOM_GET_MDMSTS values */
#define SLCOM_MHS_DTR_ON      0x0001
#define SLCOM_MHS_DTR_SET     0x0100
#define SLCOM_MHS_RTS_ON      0x0002
#define SLCOM_MHS_RTS_SET     0x0200
#define SLCOM_MHS_CTS         0x0010
#define SLCOM_MHS_DSR         0x0020
#define SLCOM_MHS_RI          0x0040
#define SLCOM_MHS_DCD         0x0080

/* SLCOM_SET_BAUDDIV values */
#define SLCOM_BAUDDIV_REF     3686400 /* 3.6864 MHz */

/* SLCOM_SET_BREAK values */
#define SLCOM_SET_BREAK_OFF   0x00
#define SLCOM_SET_BREAK_ON    0x01

/* SLCOM_SET_FLOW values - 1st word */
#define SLCOM_FLOW_DTR_ON     0x00000001 /* DTR static active */
#define SLCOM_FLOW_CTS_HS     0x00000008 /* CTS handshake */
/* SLCOM_SET_FLOW values - 2nd word */
#define SLCOM_FLOW_RTS_ON     0x00000040 /* RTS static active */
#define SLCOM_FLOW_RTS_HS     0x00000080 /* RTS handshake */

/* SLCOM_SET_EVENTMASK/SLCOM_GET_EVENTMASK/SLCOM_GET_EVENTSTATE values */
#define SLCOM_EVENT_RI_TRAILING_EDGE      0x0001
#define SLCOM_EVENT_RECEIVE_ALMOST_FULL   0x0004
#define SLCOM_EVENT_CHAR_RECEIVED         0x0100
#define SLCOM_EVENT_SPECIAL_CHAR_RECEIVED 0x0200
#define SLCOM_EVENT_TRANSMIT_QUEUE_EMPTY  0x0400
#define SLCOM_EVENT_CTS_CHANGED           0x0800
#define SLCOM_EVENT_DSR_CHANGED           0x1000
#define SLCOM_EVENT_DCD_CHANGED           0x2000
#define SLCOM_EVENT_LINE_BREAK_RECEIVED   0x4000
#define SLCOM_EVENT_LINE_STATUS_ERROR     0x8000

/* SLCOM_VENDOR_SPECIFIC values */
#define SLCOM_GET_PARTNUM     0x370B
#define SLCOM_WRITE_LATCH     0x37E1
#define SLCOM_READ_LATCH      0x00C2

/* SLCOM_GET_PARTNUM values from hardware */
#define SLCOM_PARTNUM_CP2101   1
#define SLCOM_PARTNUM_CP2102   2
#define SLCOM_PARTNUM_CP2103   3
#define SLCOM_PARTNUM_CP2104   4
#define SLCOM_PARTNUM_CP2105   5

static USBH_StatusTypeDef USBH_SLCOM_InterfaceInit(USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef USBH_SLCOM_InterfaceDeInit(USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef USBH_SLCOM_ClassRequest(USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef USBH_SLCOM_Process(USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef USBH_SLCOM_SOFProcess(USBH_HandleTypeDef *phost);
static void SLCOM_ProcessControl(USBH_HandleTypeDef *phost);
static void SLCOM_ProcessTransmission(USBH_HandleTypeDef *phost);
static void SLCOM_ProcessReception(USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef SLCOM_SetEventMask(USBH_HandleTypeDef *phost, uint16_t eventmask);
static USBH_StatusTypeDef SLCOM_GetEventState(USBH_HandleTypeDef *phost, uint16_t *eventstate);

USBH_ClassTypeDef VENDOR_SERIAL_SLCOM_Class = {
    "VENDOR_SERIAL_SLCOM",
    0xFFU, /* VENDOR Class ID */
    USBH_SLCOM_InterfaceInit,
    USBH_SLCOM_InterfaceDeInit,
    USBH_SLCOM_ClassRequest,
    USBH_SLCOM_Process,
    USBH_SLCOM_SOFProcess,
    NULL,
};

bool USBH_SLCOM_IsDeviceType(USBH_HandleTypeDef *phost)
{
    if (!phost) {
        return false;
    }

    if (phost->device.CfgDesc.Itf_Desc[0].bInterfaceClass != VENDOR_SERIAL_SLCOM_Class.ClassCode) {
        return false;
    }

    if (phost->device.DevDesc.idVendor != 0x10c4) {
        return false;
    }

    switch (phost->device.DevDesc.idProduct) {
    case 0xea60: /* CP2102 - SILABS USB UART */
    case 0xea61: /* CP210X_2 - CP210x Serial */
    case 0xea70: /* CP210X_3 - CP210x Serial */
    case 0xea80: /* CP210X_4 - CP210x Serial */
        return true;
    default:
        return false;
    }

    return false;
}

USBH_StatusTypeDef USBH_SLCOM_InterfaceInit(USBH_HandleTypeDef *phost)
{
    USBH_StatusTypeDef status;
    uint8_t interface;
    SLCOM_HandleTypeDef *SLCOM_Handle;
    HCD_HandleTypeDef *hhcd = phost->pData;
    USB_OTG_GlobalTypeDef *USBx = hhcd->Instance;

    USBH_UsrLog("SLCOM: USBH_SLCOM_InterfaceInit");

    if (!USBH_SLCOM_IsDeviceType(phost)) {
        USBH_UsrLog("SLCOM: Unrecognized device");
        return USBH_NOT_SUPPORTED;
    }

    interface = USBH_FindInterface(phost, 0xFFU, 0xFFU, 0xFFU);

    if (interface == 0xFFU || interface >= USBH_MAX_NUM_INTERFACES) {
        USBH_UsrLog("SLCOM: Cannot find the interface for Vendor Serial SLCOM class.");
        return USBH_FAIL;
    }

    status = USBH_SelectInterface(phost, interface);
    if (status != USBH_OK) {
        return USBH_FAIL;
    }

    phost->pActiveClass->pData = (SLCOM_HandleTypeDef *)USBH_malloc(sizeof(SLCOM_HandleTypeDef));
    SLCOM_Handle = (SLCOM_HandleTypeDef *)phost->pActiveClass->pData;

    if (!SLCOM_Handle) {
        USBH_ErrLog("SLCOM: Cannot allocate memory for SLCOM Handle");
        return USBH_FAIL;
    }

    USBH_memset(SLCOM_Handle, 0, sizeof(SLCOM_HandleTypeDef));

    SLCOM_Handle->iface_no = interface;

    /* Find the device endpoints */
    uint8_t max_endpoints = ((phost->device.CfgDesc.Itf_Desc[interface].bNumEndpoints <= USBH_MAX_NUM_ENDPOINTS) ?
        phost->device.CfgDesc.Itf_Desc[interface].bNumEndpoints : USBH_MAX_NUM_ENDPOINTS);

    for (uint8_t i = 0; i < max_endpoints; i++) {
        USBH_EpDescTypeDef *ep = &phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[i];

        if ((ep->bmAttributes & 0x03U) == USBH_EP_BULK) {
            if (ep->bEndpointAddress & 0x80U) {
                SLCOM_Handle->endpoints.in_ep = ep->bEndpointAddress;
                SLCOM_Handle->endpoints.in_ep_size = ep->wMaxPacketSize;
            } else {
                SLCOM_Handle->endpoints.out_ep = ep->bEndpointAddress;
                SLCOM_Handle->endpoints.out_ep_size = ep->wMaxPacketSize;
            }
        }
    }

    /* Control endpoint is assumed, and not mentioned in the config structure */
    SLCOM_Handle->endpoints.control_ep = 0;
    SLCOM_Handle->endpoints.control_ep_size = sizeof(USB_Setup_TypeDef) + 8;

    /* Make sure we found both data endpoints */
    if (SLCOM_Handle->endpoints.in_ep == 0
        || SLCOM_Handle->endpoints.out_ep == 0) {
        USBH_ErrLog("SLCOM: Cannot find bulk data endpoints");
        return USBH_FAIL;
    }

    USBH_UsrLog("SLCOM: in_ep=%d, in_ep_size=%d", SLCOM_Handle->endpoints.in_ep, SLCOM_Handle->endpoints.in_ep_size);
    USBH_UsrLog("SLCOM: out_ep=%d, out_ep_size=%d", SLCOM_Handle->endpoints.out_ep, SLCOM_Handle->endpoints.out_ep_size);
    USBH_UsrLog("SLCOM: control_ep=%d, control_ep_size=%d", SLCOM_Handle->endpoints.control_ep, SLCOM_Handle->endpoints.control_ep_size);

    /* Open the control pipe */
    SLCOM_Handle->endpoints.control_pipe = USBH_AllocPipe(phost, SLCOM_Handle->endpoints.control_ep);

    USBH_OpenPipe(phost, SLCOM_Handle->endpoints.control_pipe, SLCOM_Handle->endpoints.control_ep,
        phost->device.address, phost->device.speed, USB_EP_TYPE_CTRL,
        SLCOM_Handle->endpoints.control_ep_size);

    USBH_LL_SetToggle(phost, SLCOM_Handle->endpoints.control_pipe, 0U);

    /* Open the bulk data I/O pipes */
    SLCOM_Handle->endpoints.out_pipe = USBH_AllocPipe(phost, SLCOM_Handle->endpoints.out_ep);
    SLCOM_Handle->endpoints.in_pipe = USBH_AllocPipe(phost, SLCOM_Handle->endpoints.in_ep);

    USBH_OpenPipe(phost, SLCOM_Handle->endpoints.out_pipe, SLCOM_Handle->endpoints.out_ep,
        phost->device.address, phost->device.speed, USB_EP_TYPE_BULK,
        SLCOM_Handle->endpoints.out_ep_size);
    USBH_OpenPipe(phost, SLCOM_Handle->endpoints.in_pipe, SLCOM_Handle->endpoints.in_ep,
        phost->device.address, phost->device.speed, USB_EP_TYPE_BULK,
        SLCOM_Handle->endpoints.in_ep_size);

    USBH_LL_SetToggle(phost, SLCOM_Handle->endpoints.out_pipe, 0U);
    USBH_LL_SetToggle(phost, SLCOM_Handle->endpoints.in_pipe, 0U);

    /* Allocate the I/O buffers */
    SLCOM_Handle->endpoints.control_ep_buffer = (uint8_t *)USBH_malloc(SLCOM_Handle->endpoints.control_ep_size);
    if (!SLCOM_Handle->endpoints.control_ep_buffer) {
        USBH_ErrLog("SLCOM: Cannot allocate memory for SLCOM control buffer");
        return USBH_FAIL;
    }
    USBH_memset(SLCOM_Handle->endpoints.control_ep_buffer, 0, SLCOM_Handle->endpoints.control_ep_size);

    SLCOM_Handle->endpoints.in_ep_buffer = (uint8_t *)USBH_malloc(SLCOM_Handle->endpoints.in_ep_size);
    if (!SLCOM_Handle->endpoints.in_ep_buffer) {
        USBH_ErrLog("SLCOM: Cannot allocate memory for SLCOM receive buffer");
        return USBH_FAIL;
    }
    SLCOM_Handle->rx_data = SLCOM_Handle->endpoints.in_ep_buffer;
    SLCOM_Handle->rx_data_length = SLCOM_Handle->endpoints.in_ep_size;
    USBH_memset(SLCOM_Handle->endpoints.in_ep_buffer, 0, SLCOM_Handle->endpoints.in_ep_size);

    SLCOM_Handle->endpoints.out_ep_buffer = (uint8_t *)USBH_malloc(SLCOM_Handle->endpoints.out_ep_size);
    if (!SLCOM_Handle->endpoints.out_ep_buffer) {
        USBH_ErrLog("SLCOM: Cannot allocate memory for SLCOM transmit buffer");
        return USBH_FAIL;
    }
    USBH_memset(SLCOM_Handle->endpoints.out_ep_buffer, 0, SLCOM_Handle->endpoints.out_ep_size);

    /* Clear stall at first run */
    USB_OTG_EPTypeDef otg_ep_out = {
        .num = hhcd->hc[SLCOM_Handle->endpoints.out_pipe].ep_num,
        .is_in = hhcd->hc[SLCOM_Handle->endpoints.out_pipe].ep_is_in,
        .type = hhcd->hc[SLCOM_Handle->endpoints.out_pipe].ep_type
    };
    USB_EPSetStall(USBx, &otg_ep_out);

    USB_OTG_EPTypeDef otg_ep_in = {
        .num = hhcd->hc[SLCOM_Handle->endpoints.in_pipe].ep_num,
        .is_in = hhcd->hc[SLCOM_Handle->endpoints.in_pipe].ep_is_in,
        .type = hhcd->hc[SLCOM_Handle->endpoints.out_pipe].ep_type
    };
    USB_EPSetStall(USBx, &otg_ep_in);

    /* Do a request to get the part number */
    SLCOM_Handle->partnum = 0;
    phost->Control.setup.b.bmRequestType = SLCOM_READ;
    phost->Control.setup.b.bRequest = SLCOM_VENDOR_SPECIFIC;
    phost->Control.setup.b.wValue.w = SLCOM_GET_PARTNUM;
    phost->Control.setup.b.wIndex.w = SLCOM_Handle->iface_no;
    phost->Control.setup.b.wLength.w = sizeof(SLCOM_Handle->partnum);

    do {
        status = USBH_CtlReq(phost, &SLCOM_Handle->partnum, sizeof(SLCOM_Handle->partnum));
    } while (status == USBH_BUSY);

    if (status != USBH_OK) {
        USBH_ErrLog("SLCOM: Unable to query device part number: %d", status);
        return USBH_FAIL;
    }

    switch (SLCOM_Handle->partnum) {
    case SLCOM_PARTNUM_CP2101:
        USBH_UsrLog("SLCOM: partnum=CP2101");
        break;
    case SLCOM_PARTNUM_CP2102:
        USBH_UsrLog("SLCOM: partnum=CP2102");
        break;
    case SLCOM_PARTNUM_CP2103:
        USBH_UsrLog("SLCOM: partnum=CP2103");
        break;
    case SLCOM_PARTNUM_CP2104:
        USBH_UsrLog("SLCOM: partnum=CP2104");
        break;
    case SLCOM_PARTNUM_CP2105:
        USBH_UsrLog("SLCOM: partnum=CP2105");
        break;
    default:
        USBH_UsrLog("SLCOM: partnum=<unknown>");
        break;
    }

    /* Set the event mask */
    status = SLCOM_SetEventMask(phost, SLCOM_EVENT_CHAR_RECEIVED);
    if (status != USBH_OK) {
        USBH_ErrLog("SLCOM: Unable to set event mask: %d", status);
        return USBH_FAIL;
    }

    /* Set initial state and polling interval */
    SLCOM_Handle->state = USBH_SLCOM_STATE_IDLE;
    if (SLCOM_Handle->poll < SLCOM_MIN_POLL) {
        SLCOM_Handle->poll = SLCOM_MIN_POLL;
    }

    return USBH_OK;
}

USBH_StatusTypeDef USBH_SLCOM_InterfaceDeInit(USBH_HandleTypeDef *phost)
{
    SLCOM_HandleTypeDef *SLCOM_Handle = (SLCOM_HandleTypeDef *)phost->pActiveClass->pData;

    USBH_UsrLog("SLCOM: USBH_SLCOM_InterfaceDeInit");

    if (!phost->pActiveClass->pData) {
        return USBH_FAIL;
    }

    /* Close and free the control pipe */
    if (SLCOM_Handle->endpoints.control_pipe) {
        USBH_ClosePipe(phost, SLCOM_Handle->endpoints.control_pipe);
        USBH_FreePipe(phost, SLCOM_Handle->endpoints.control_pipe);
        SLCOM_Handle->endpoints.control_pipe = 0U;
    }

    /* Close and free the I/O pipes */
    if (SLCOM_Handle->endpoints.in_pipe) {
        USBH_ClosePipe(phost, SLCOM_Handle->endpoints.in_pipe);
        USBH_FreePipe(phost, SLCOM_Handle->endpoints.in_pipe);
        SLCOM_Handle->endpoints.in_pipe = 0U;
    }
    if (SLCOM_Handle->endpoints.out_pipe) {
        USBH_ClosePipe(phost, SLCOM_Handle->endpoints.out_pipe);
        USBH_FreePipe(phost, SLCOM_Handle->endpoints.out_pipe);
        SLCOM_Handle->endpoints.out_pipe = 0U;
    }

    /* Free the I/O data buffers */
    if (SLCOM_Handle->endpoints.in_ep_buffer) {
        USBH_free(SLCOM_Handle->endpoints.in_ep_buffer);
        SLCOM_Handle->endpoints.in_ep_buffer = NULL;
        SLCOM_Handle->rx_data = NULL;
        SLCOM_Handle->rx_data_length = 0;
    }

    if (SLCOM_Handle->endpoints.out_ep_buffer) {
        USBH_free(SLCOM_Handle->endpoints.out_ep_buffer);
        SLCOM_Handle->endpoints.out_ep_buffer = NULL;
        SLCOM_Handle->tx_data = NULL;
        SLCOM_Handle->tx_data_length = 0;
    }

    if (SLCOM_Handle->endpoints.control_ep_buffer) {
        USBH_free(SLCOM_Handle->endpoints.control_ep_buffer);
        SLCOM_Handle->endpoints.control_ep_buffer = NULL;
    }

    /* Free the device handle */
    if (phost->pActiveClass->pData) {
        USBH_free(phost->pActiveClass->pData);
        phost->pActiveClass->pData = 0U;
    }

    return USBH_OK;
}

USBH_StatusTypeDef USBH_SLCOM_Open(USBH_HandleTypeDef *phost)
{
    SLCOM_HandleTypeDef *SLCOM_Handle = (SLCOM_HandleTypeDef *)phost->pActiveClass->pData;

    phost->Control.setup.b.bmRequestType = SLCOM_WRITE;
    phost->Control.setup.b.bRequest = SLCOM_IFC_ENABLE;
    phost->Control.setup.b.wValue.w = SLCOM_IFC_ENABLE_EN;
    phost->Control.setup.b.wIndex.w = SLCOM_Handle->iface_no;
    phost->Control.setup.b.wLength.w = 0U;

    return USBH_CtlReq(phost, NULL, 0);
}

USBH_StatusTypeDef USBH_SLCOM_Close(USBH_HandleTypeDef *phost)
{
    SLCOM_HandleTypeDef *SLCOM_Handle = (SLCOM_HandleTypeDef *)phost->pActiveClass->pData;

    phost->Control.setup.b.bmRequestType = SLCOM_WRITE;
    phost->Control.setup.b.bRequest = SLCOM_IFC_ENABLE;
    phost->Control.setup.b.wValue.w = SLCOM_IFC_ENABLE_DIS;
    phost->Control.setup.b.wIndex.w = SLCOM_Handle->iface_no;
    phost->Control.setup.b.wLength.w = 0U;

    return USBH_CtlReq(phost, NULL, 0);
}

USBH_StatusTypeDef USBH_SLCOM_SetBaudRate(USBH_HandleTypeDef *phost, uint32_t baudrate)
{
    SLCOM_HandleTypeDef *SLCOM_Handle = (SLCOM_HandleTypeDef *)phost->pActiveClass->pData;

    uint32_t maxspeed;
    switch (SLCOM_Handle->partnum) {
    case SLCOM_PARTNUM_CP2104:
    case SLCOM_PARTNUM_CP2105:
        maxspeed = 2000000;
        break;
    case SLCOM_PARTNUM_CP2101:
    case SLCOM_PARTNUM_CP2102:
    case SLCOM_PARTNUM_CP2103:
    default:
        /*
         * Datasheet for cp2102 says 921600 max.  Testing shows that
         * 1228800 and 1843200 work fine.
         */
        maxspeed = 1843200;
        break;
    }
    if (baudrate <= 0 || baudrate > maxspeed) {
        return USBH_NOT_SUPPORTED;
    }

    phost->Control.setup.b.bmRequestType = SLCOM_WRITE;
    phost->Control.setup.b.bRequest = SLCOM_SET_BAUDRATE;
    phost->Control.setup.b.wValue.w = 0U;
    phost->Control.setup.b.wIndex.w = SLCOM_Handle->iface_no;
    phost->Control.setup.b.wLength.w = sizeof(baudrate);

    return USBH_CtlReq(phost, (uint8_t *)&baudrate, sizeof(baudrate));
}

USBH_StatusTypeDef USBH_SLCOM_SetLineControl(USBH_HandleTypeDef *phost, uint16_t linecontrol)
{
    SLCOM_HandleTypeDef *SLCOM_Handle = (SLCOM_HandleTypeDef *)phost->pActiveClass->pData;

    phost->Control.setup.b.bmRequestType = SLCOM_WRITE;
    phost->Control.setup.b.bRequest = SLCOM_SET_LINE_CTL;
    phost->Control.setup.b.wValue.w = linecontrol;
    phost->Control.setup.b.wIndex.w = SLCOM_Handle->iface_no;
    phost->Control.setup.b.wLength.w = 0U;

    return USBH_CtlReq(phost, NULL, 0);
}

USBH_StatusTypeDef USBH_SLCOM_SetRtsCts(USBH_HandleTypeDef *phost, uint8_t onoff)
{
    SLCOM_HandleTypeDef *SLCOM_Handle = (SLCOM_HandleTypeDef *)phost->pActiveClass->pData;
    uint32_t flowctrl[4];

    if (onoff) {
        flowctrl[0] = SLCOM_FLOW_DTR_ON | SLCOM_FLOW_CTS_HS;
        flowctrl[1] = SLCOM_FLOW_RTS_HS;
    } else {
        flowctrl[0] = SLCOM_FLOW_DTR_ON;
        flowctrl[1] = SLCOM_FLOW_RTS_ON;
    }
    flowctrl[2] = 0;
    flowctrl[3] = 0;

    phost->Control.setup.b.bmRequestType = SLCOM_WRITE;
    phost->Control.setup.b.bRequest = SLCOM_SET_FLOW;
    phost->Control.setup.b.wValue.w = 0;
    phost->Control.setup.b.wIndex.w = SLCOM_Handle->iface_no;
    phost->Control.setup.b.wLength.w = sizeof(flowctrl);

    return USBH_CtlReq(phost, (uint8_t *)&flowctrl, sizeof(flowctrl));
}

USBH_StatusTypeDef USBH_SLCOM_SetDtr(USBH_HandleTypeDef *phost, uint8_t onoff)
{
    SLCOM_HandleTypeDef *SLCOM_Handle = (SLCOM_HandleTypeDef *)phost->pActiveClass->pData;
    uint16_t ctl;

    ctl = onoff ? SLCOM_MHS_DTR_ON : 0;
    ctl |= SLCOM_MHS_DTR_SET;

    phost->Control.setup.b.bmRequestType = SLCOM_WRITE;
    phost->Control.setup.b.bRequest = SLCOM_SET_MHS;
    phost->Control.setup.b.wValue.w = ctl;
    phost->Control.setup.b.wIndex.w = SLCOM_Handle->iface_no;
    phost->Control.setup.b.wLength.w = 0U;

    return USBH_CtlReq(phost, NULL, 0);
}

USBH_StatusTypeDef USBH_SLCOM_SetRts(USBH_HandleTypeDef *phost, uint8_t onoff)
{
    SLCOM_HandleTypeDef *SLCOM_Handle = (SLCOM_HandleTypeDef *)phost->pActiveClass->pData;
    uint16_t ctl;

    ctl = onoff ? SLCOM_MHS_RTS_ON : 0;
    ctl |= SLCOM_MHS_RTS_SET;

    phost->Control.setup.b.bmRequestType = SLCOM_WRITE;
    phost->Control.setup.b.bRequest = SLCOM_SET_MHS;
    phost->Control.setup.b.wValue.w = ctl;
    phost->Control.setup.b.wIndex.w = SLCOM_Handle->iface_no;
    phost->Control.setup.b.wLength.w = 0U;

    return USBH_CtlReq(phost, NULL, 0);
}

USBH_StatusTypeDef USBH_SLCOM_SetBreak(USBH_HandleTypeDef *phost, uint8_t onoff)
{
    SLCOM_HandleTypeDef *SLCOM_Handle = (SLCOM_HandleTypeDef *)phost->pActiveClass->pData;
    uint16_t brk = onoff ? SLCOM_SET_BREAK_ON : SLCOM_SET_BREAK_OFF;

    phost->Control.setup.b.bmRequestType = SLCOM_WRITE;
    phost->Control.setup.b.bRequest = SLCOM_SET_BREAK;
    phost->Control.setup.b.wValue.w = brk;
    phost->Control.setup.b.wIndex.w = SLCOM_Handle->iface_no;
    phost->Control.setup.b.wLength.w = 0U;

    return USBH_CtlReq(phost, NULL, 0);
}

USBH_StatusTypeDef USBH_SLCOM_Transmit(USBH_HandleTypeDef *phost, const uint8_t *pbuff, uint32_t length)
{
    USBH_StatusTypeDef status = USBH_BUSY;
    SLCOM_HandleTypeDef *SLCOM_Handle = (SLCOM_HandleTypeDef *)phost->pActiveClass->pData;

    if (length > SLCOM_Handle->endpoints.out_ep_size) {
        USBH_ErrLog("SLCOM: Transmit size too large: %lu > %d",
            length, SLCOM_Handle->endpoints.out_ep_size);
        return USBH_FAIL;
    }

    if ((SLCOM_Handle->state == USBH_SLCOM_STATE_IDLE) || (SLCOM_Handle->state == USBH_SLCOM_STATE_TRANSFER_DATA)) {
        USBH_memcpy(SLCOM_Handle->endpoints.out_ep_buffer, pbuff, length);
        SLCOM_Handle->tx_data = SLCOM_Handle->endpoints.out_ep_buffer;
        SLCOM_Handle->tx_data_length = length;
        SLCOM_Handle->state = USBH_SLCOM_STATE_TRANSFER_DATA;
        SLCOM_Handle->data_tx_state = USBH_SLCOM_SEND_DATA;
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

USBH_StatusTypeDef USBH_SLCOM_ClassRequest(USBH_HandleTypeDef *phost)
{
    phost->pUser(phost, HOST_USER_CLASS_ACTIVE);
    return USBH_OK;
}

USBH_StatusTypeDef USBH_SLCOM_Process(USBH_HandleTypeDef *phost)
{
    USBH_StatusTypeDef status = USBH_BUSY;
    USBH_StatusTypeDef req_status = USBH_OK;
    SLCOM_HandleTypeDef *SLCOM_Handle = (SLCOM_HandleTypeDef *)phost->pActiveClass->pData;

    switch (SLCOM_Handle->state) {
    case USBH_SLCOM_STATE_IDLE:
        status = USBH_OK;
        break;

    case USBH_SLCOM_STATE_TRANSFER_DATA:
        SLCOM_Handle->timer = phost->Timer;
        SLCOM_ProcessControl(phost);
        SLCOM_ProcessTransmission(phost);
        SLCOM_ProcessReception(phost);
        if (SLCOM_Handle->data_rx_state == USBH_SLCOM_IDLE_DATA && SLCOM_Handle->data_tx_state == USBH_SLCOM_IDLE_DATA) {
            SLCOM_Handle->state = USBH_SLCOM_STATE_IDLE;
        }
        break;

    case USBH_SLCOM_STATE_ERROR:
        req_status = USBH_ClrFeature(phost, 0x00U);
        if (req_status == USBH_OK) {
            SLCOM_Handle->state = USBH_SLCOM_STATE_IDLE;
        }
        break;

    default:
        break;
    }

    return status;
}

USBH_StatusTypeDef USBH_SLCOM_SOFProcess(USBH_HandleTypeDef *phost)
{
    SLCOM_HandleTypeDef *SLCOM_Handle = (SLCOM_HandleTypeDef *)phost->pActiveClass->pData;

    if (SLCOM_Handle->state == USBH_SLCOM_STATE_IDLE) {
        if ((phost->Timer - SLCOM_Handle->timer) >= SLCOM_Handle->poll) {
            SLCOM_Handle->state = USBH_SLCOM_STATE_TRANSFER_DATA;
            if (SLCOM_Handle->data_rx_state == USBH_SLCOM_IDLE_DATA) {
                SLCOM_Handle->data_rx_state = USBH_SLCOM_RECEIVE_DATA;
            }

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

void SLCOM_ProcessControl(USBH_HandleTypeDef *phost)
{
    /*
     * Not handling the control pipe for now, since we do not really need
     * to know status that comes from it.
     */
}

void SLCOM_ProcessTransmission(USBH_HandleTypeDef *phost)
{
    SLCOM_HandleTypeDef *SLCOM_Handle = (SLCOM_HandleTypeDef *)phost->pActiveClass->pData;
    USBH_URBStateTypeDef URB_Status = USBH_URB_IDLE;

    switch (SLCOM_Handle->data_tx_state) {
    case USBH_SLCOM_SEND_DATA:
        if (SLCOM_Handle->tx_data_length > SLCOM_Handle->endpoints.out_ep_size) {
            /*
             * Right now this case is not possible, because we allocate tx_data
             * and limit what can be sent based on out_ep_size
             */
            USBH_BulkSendData(phost,
                SLCOM_Handle->tx_data,
                SLCOM_Handle->endpoints.out_ep_size,
                SLCOM_Handle->endpoints.out_pipe,
                1U);
        } else {
            USBH_BulkSendData(phost,
                SLCOM_Handle->tx_data,
                (uint16_t)SLCOM_Handle->tx_data_length,
                SLCOM_Handle->endpoints.out_pipe,
                1U);
        }
        SLCOM_Handle->data_tx_state = USBH_SLCOM_SEND_DATA_WAIT;
        break;

    case USBH_SLCOM_SEND_DATA_WAIT:
        URB_Status = USBH_LL_GetURBState(phost, SLCOM_Handle->endpoints.out_pipe);

        /* Check if the transmission is complete */
        if (URB_Status == USBH_URB_DONE) {
            if (SLCOM_Handle->tx_data_length > SLCOM_Handle->endpoints.out_ep_size) {
                SLCOM_Handle->tx_data_length -= SLCOM_Handle->endpoints.out_ep_size;
                SLCOM_Handle->tx_data += SLCOM_Handle->endpoints.out_ep_size;
            } else {
                SLCOM_Handle->tx_data_length = 0U;
            }

            if (SLCOM_Handle->tx_data_length > 0U) {
                SLCOM_Handle->data_tx_state = USBH_SLCOM_SEND_DATA;
            } else {
                SLCOM_Handle->data_tx_state = USBH_SLCOM_IDLE_DATA;
                SLCOM_Handle->tx_data = NULL;
                SLCOM_Handle->tx_data_length = 0;
                USBH_SLCOM_TransmitCallback(phost);
            }

#if (USBH_USE_OS == 1U)
            phost->os_msg = (uint32_t)USBH_CLASS_EVENT;
#if (osCMSIS < 0x20000U)
            (void)osMessagePut(phost->os_event, phost->os_msg, 0U);
#else
            (void)osMessageQueuePut(phost->os_event, &phost->os_msg, 0U, 0U);
#endif
#endif
        } else if (URB_Status == USBH_URB_NOTREADY) {
            SLCOM_Handle->data_tx_state = USBH_SLCOM_SEND_DATA;

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

void SLCOM_ProcessReception(USBH_HandleTypeDef *phost)
{
    SLCOM_HandleTypeDef *SLCOM_Handle = (SLCOM_HandleTypeDef *)phost->pActiveClass->pData;
    USBH_URBStateTypeDef URB_Status = USBH_URB_IDLE;
    uint16_t eventstate;
    uint32_t length;


    switch (SLCOM_Handle->data_rx_state) {
    case USBH_SLCOM_RECEIVE_DATA:
        if (SLCOM_GetEventState(phost, &eventstate) == USBH_OK) {
            if (eventstate & SLCOM_EVENT_CHAR_RECEIVED) {
                USBH_BulkReceiveData(phost,
                    SLCOM_Handle->rx_data,
                    SLCOM_Handle->endpoints.in_ep_size,
                    SLCOM_Handle->endpoints.in_pipe);

                SLCOM_Handle->data_rx_state = USBH_SLCOM_RECEIVE_DATA_WAIT;
            } else {
                SLCOM_Handle->data_rx_state = USBH_SLCOM_IDLE_DATA;
            }
        }
        break;

    case USBH_SLCOM_RECEIVE_DATA_WAIT:
        URB_Status = USBH_LL_GetURBState(phost, SLCOM_Handle->endpoints.in_pipe);

        if (URB_Status == USBH_URB_DONE) {
            length = USBH_LL_GetLastXferSize(phost, SLCOM_Handle->endpoints.in_pipe);

            if (((SLCOM_Handle->rx_data_length - length) > 0U) && (length > SLCOM_Handle->endpoints.in_ep_size)) {
                SLCOM_Handle->rx_data_length -= length;
                SLCOM_Handle->rx_data += length;
                SLCOM_Handle->data_rx_state = USBH_SLCOM_RECEIVE_DATA;
            } else {
                SLCOM_Handle->data_rx_state = USBH_SLCOM_IDLE_DATA;

                size_t recv_length;
                if (SLCOM_Handle->rx_data == SLCOM_Handle->endpoints.in_ep_buffer) {
                    recv_length = length;
                } else {
                    recv_length = SLCOM_Handle->rx_data - SLCOM_Handle->endpoints.in_ep_buffer;
                    SLCOM_Handle->rx_data = SLCOM_Handle->endpoints.in_ep_buffer;
                }

                if (recv_length > 0) {
                    USBH_SLCOM_ReceiveCallback(phost, SLCOM_Handle->rx_data, recv_length);
                }
                SLCOM_Handle->rx_data = SLCOM_Handle->endpoints.in_ep_buffer;
                SLCOM_Handle->rx_data_length = SLCOM_Handle->endpoints.in_ep_size;
            }

#if (USBH_USE_OS == 1U)
            phost->os_msg = (uint32_t)USBH_CLASS_EVENT;
#if (osCMSIS < 0x20000U)
            (void)osMessagePut(phost->os_event, phost->os_msg, 0U);
#else
            (void)osMessageQueuePut(phost->os_event, &phost->os_msg, 0U, 0U);
#endif
#endif
        } else if (URB_Status == USBH_URB_NOTREADY) {
            SLCOM_Handle->data_rx_state = USBH_SLCOM_IDLE_DATA;
        }
        break;

    default:
        break;
    }
}

USBH_StatusTypeDef SLCOM_SetEventMask(USBH_HandleTypeDef *phost, uint16_t eventmask)
{
    SLCOM_HandleTypeDef *SLCOM_Handle = (SLCOM_HandleTypeDef *)phost->pActiveClass->pData;
    USBH_StatusTypeDef status;

    phost->Control.setup.b.bmRequestType = SLCOM_WRITE;
    phost->Control.setup.b.bRequest = SLCOM_SET_EVENTMASK;
    phost->Control.setup.b.wValue.w = eventmask;
    phost->Control.setup.b.wIndex.w = SLCOM_Handle->iface_no;
    phost->Control.setup.b.wLength.w = 0U;

    do {
        status = USBH_CtlReq(phost, NULL, 0);
    } while (status == USBH_BUSY);

    return status;
}

USBH_StatusTypeDef SLCOM_GetEventState(USBH_HandleTypeDef *phost, uint16_t *eventstate)
{
    SLCOM_HandleTypeDef *SLCOM_Handle = (SLCOM_HandleTypeDef *)phost->pActiveClass->pData;
    USBH_StatusTypeDef status;
    uint8_t buf[2];

    phost->Control.setup.b.bmRequestType = SLCOM_READ;
    phost->Control.setup.b.bRequest = SLCOM_GET_EVENTSTATE;
    phost->Control.setup.b.wValue.w = 0U;
    phost->Control.setup.b.wIndex.w = SLCOM_Handle->iface_no;
    phost->Control.setup.b.wLength.w = sizeof(buf);

    do {
        status = USBH_CtlReq(phost, buf, sizeof(buf));
    } while (status == USBH_BUSY);

    if (status == USBH_OK) {
        if (eventstate) {
            *eventstate = (uint16_t)buf[0] << 8 | (uint16_t)buf[1];
        }
    }

    return status;
}

__weak void USBH_SLCOM_TransmitCallback(USBH_HandleTypeDef *phost)
{
    /* Prevent unused argument(s) compilation warning */
    UNUSED(phost);
}

__weak void USBH_SLCOM_ReceiveCallback(USBH_HandleTypeDef *phost, uint8_t *data, size_t length)
{
    /* Prevent unused argument(s) compilation warning */
    UNUSED(phost);
}
