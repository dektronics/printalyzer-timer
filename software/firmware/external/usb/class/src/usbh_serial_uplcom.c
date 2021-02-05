#include "usbh_serial_uplcom.h"

#define UPLCOM_CONFIG_INDEX         0
#define UPLCOM_IFACE_INDEX          0
#define UPLCOM_SECOND_IFACE_INDEX   1

#define UPLCOM_SET_REQUEST              0x01
#define UPLCOM_SET_REQUEST_PL2303HXN    0x80
#define UPLCOM_SET_CRTSCTS              0x41
#define UPLCOM_SET_CRTSCTS_PL2303X      0x61
#define UPLCOM_SET_CRTSCTS_PL2303HXN    0xFA
#define UPLCOM_CLEAR_CRTSCTS_PL2303HXN  0xFF
#define UPLCOM_CRTSCTS_REG_PL2303HXN    0x0A
#define UPLCOM_STATUS_REG_PL2303HX      0x8080

#define UPLCOM_STATE_INDEX 8

#define RSAQ_STATUS_CTS           0x80
#define RSAQ_STATUS_OVERRUN_ERROR 0x40
#define RSAQ_STATUS_PARITY_ERROR  0x20
#define RSAQ_STATUS_FRAME_ERROR   0x10
#define RSAQ_STATUS_RING          0x08
#define RSAQ_STATUS_BREAK_ERROR   0x04
#define RSAQ_STATUS_DSR           0x02
#define RSAQ_STATUS_DCD           0x01

/* Relevant USB CDC constants */
#define UCDC_BREAK_ON           0xffff
#define UCDC_BREAK_OFF          0x0000

#define TYPE_PL2303             0
#define TYPE_PL2303HX           1
#define TYPE_PL2303HXD          2
#define TYPE_PL2303HXN          3

#define UT_WRITE_CLASS_INTERFACE (USB_H2D | USB_REQ_TYPE_CLASS | USB_REQ_RECIPIENT_INTERFACE)
#define UT_READ_CLASS_INTERFACE  (USB_D2H | USB_REQ_TYPE_CLASS | USB_REQ_RECIPIENT_INTERFACE)
#define UT_WRITE_VENDOR_DEVICE   (USB_H2D | USB_REQ_TYPE_VENDOR | USB_REQ_RECIPIENT_DEVICE)
#define UT_READ_VENDOR_DEVICE    (USB_D2H | USB_REQ_TYPE_VENDOR | USB_REQ_RECIPIENT_DEVICE)

static USBH_StatusTypeDef USBH_UPLCOM_InterfaceInit(USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef USBH_UPLCOM_InterfaceDeInit(USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef USBH_UPLCOM_ClassRequest(USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef USBH_UPLCOM_Process(USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef USBH_UPLCOM_SOFProcess(USBH_HandleTypeDef *phost);

static USBH_StatusTypeDef UPLCOM_PL2303_Do(USBH_HandleTypeDef *phost,
    uint8_t req_type, uint8_t request, uint16_t value, uint16_t index,
    uint16_t length);
static USBH_StatusTypeDef UPLCOM_SetLineCoding(USBH_HandleTypeDef *phost, CDC_LineCodingTypeDef *linecoding);
static USBH_StatusTypeDef UPLCOM_GetLineCoding(USBH_HandleTypeDef *phost, CDC_LineCodingTypeDef *linecoding);

static void UPLCOM_ProcessNotification(USBH_HandleTypeDef *phost);
static void UPLCOM_ProcessTransmission(USBH_HandleTypeDef *phost);
static void UPLCOM_ProcessReception(USBH_HandleTypeDef *phost);

USBH_ClassTypeDef VENDOR_SERIAL_UPLCOM_Class = {
    "VENDOR_SERIAL_UPLCOM",
    0xFFU, /* VENDOR Class ID */
    USBH_UPLCOM_InterfaceInit,
    USBH_UPLCOM_InterfaceDeInit,
    USBH_UPLCOM_ClassRequest,
    USBH_UPLCOM_Process,
    USBH_UPLCOM_SOFProcess,
    NULL,
};

bool USBH_UPLCOM_IsDeviceType(USBH_HandleTypeDef *phost)
{
    if (!phost) {
        return false;
    }

    if (phost->device.CfgDesc.Itf_Desc[0].bInterfaceClass != VENDOR_SERIAL_UPLCOM_Class.ClassCode) {
        return false;
    }

    if (phost->device.DevDesc.idVendor != 0x067b) {
        return false;
    }

    switch (phost->device.DevDesc.idProduct) {
    case 0x2303: // PL2303 Serial (ATEN/IOGEAR UC232A)
    case 0x23a3: // PL2303HXN Serial, type GC
    case 0x23b3: // PL2303HXN Serial, type GB
    case 0x23c3: // PL2303HXN Serial, type GT
    case 0x23d3: // PL2303HXN Serial, type GL
    case 0x23e3: // PL2303HXN Serial, type GE
    case 0x23f3: // PL2303HXN Serial, type GS
        return true;
    default:
        return false;
    }
}

USBH_StatusTypeDef USBH_UPLCOM_InterfaceInit(USBH_HandleTypeDef *phost)
{
    USBH_StatusTypeDef status;
    uint8_t interface;
    UPLCOM_HandleTypeDef *UPLCOM_Handle;

    USBH_UsrLog("UPLCOM: USBH_UPLCOM_InterfaceInit");

    if (!USBH_UPLCOM_IsDeviceType(phost)) {
        USBH_UsrLog("UPLCOM: Unrecognized device");
        return USBH_NOT_SUPPORTED;
    }

    interface = USBH_FindInterface(phost, 0xFFU, 0xFFU, 0xFFU);

    if (interface == 0xFFU || interface >= USBH_MAX_NUM_INTERFACES) {
        USBH_UsrLog("UPLCOM: Cannot find the interface for Vendor Serial UPLCOM class.");
        return USBH_FAIL;
    }

    status = USBH_SelectInterface(phost, interface);
    if (status != USBH_OK) {
        return USBH_FAIL;
    }

    phost->pActiveClass->pData = (UPLCOM_HandleTypeDef *)USBH_malloc(sizeof(UPLCOM_HandleTypeDef));
    UPLCOM_Handle = (UPLCOM_HandleTypeDef *)phost->pActiveClass->pData;

    if (!UPLCOM_Handle) {
        USBH_ErrLog("UPLCOM: Cannot allocate memory for UPLCOM Handle");
        return USBH_FAIL;
    }

    USBH_memset(UPLCOM_Handle, 0, sizeof(UPLCOM_HandleTypeDef));

    switch (phost->device.DevDesc.bcdDevice) {
    case 0x0300:
        UPLCOM_Handle->chiptype = TYPE_PL2303HX;
        /* or TA, that is HX with external crystal */
        break;
    case 0x0400:
        UPLCOM_Handle->chiptype = TYPE_PL2303HXD;
        /* or EA, that is HXD with ESD protection */
        /* or RA, that has internal voltage level converter that works only up to 1Mbaud (!) */
        break;
    case 0x0500:
        UPLCOM_Handle->chiptype = TYPE_PL2303HXD;
        /* in fact it's TB, that is HXD with external crystal */
        break;
    default:
        /* NOTE: I have no info about the bcdDevice for the base PL2303 (up to 1.2Mbaud,
           only fixed rates) and for PL2303SA (8-pin chip, up to 115200 baud */
        /* Determine the chip type.  This algorithm is taken from Linux. */
        if (phost->device.DevDesc.bDeviceClass == 0x02)
            UPLCOM_Handle->chiptype = TYPE_PL2303;
        else if (phost->device.DevDesc.bMaxPacketSize == 0x40)
            UPLCOM_Handle->chiptype = TYPE_PL2303HX;
        else
            UPLCOM_Handle->chiptype = TYPE_PL2303;
        break;
    }

    /*
     * The new chip revision PL2303HXN is only compatible with the new
     * UPLCOM_SET_REQUEST_PL2303HXN command. Issuing the old command
     * UPLCOM_SET_REQUEST to the new chip raises an error. Thus, PL2303HX
     * and PL2303HXN can be distinguished by issuing an old-style request
     * (on a status register) to the new chip and checking the error.
     */
    if (UPLCOM_Handle->chiptype == TYPE_PL2303HX) {
        uint8_t buf[4];
        phost->Control.setup.b.bmRequestType = UT_READ_VENDOR_DEVICE;
        phost->Control.setup.b.bRequest = UPLCOM_SET_REQUEST;
        phost->Control.setup.b.wValue.w = UPLCOM_STATUS_REG_PL2303HX;
        phost->Control.setup.b.wIndex.w = UPLCOM_Handle->data_iface_no; // this isn't even initialized yet, it should be zero here
        phost->Control.setup.b.wLength.w = 1U;

        do {
            status = USBH_CtlReq(phost, buf, sizeof(buf));
        } while (status == USBH_BUSY);

        if (status != USBH_OK) {
            UPLCOM_Handle->chiptype = TYPE_PL2303HXN;
        }
    }

    switch (UPLCOM_Handle->chiptype) {
    case TYPE_PL2303:
        USBH_UsrLog("UPLCOM: chiptype = 2303");
        break;
    case TYPE_PL2303HX:
        USBH_UsrLog("UPLCOM: chiptype = 2303HX/TA");
        break;
    case TYPE_PL2303HXN:
        USBH_UsrLog("UPLCOM: chiptype = 2303HXN");
        break;
    case TYPE_PL2303HXD:
        USBH_UsrLog("UPLCOM: Chip type = 2303HXD/TB/RA/EA");
        break;
    default:
        USBH_UsrLog("UPLCOM: chiptype = unknown %d", UPLCOM_Handle->chiptype);
        break;
    }

    /*
     * USB-RSAQ1 has two interfaces
     *
     *  USB-RSAQ1       | USB-RSAQ2
     * -----------------+-----------------
     * Interface 0      |Interface 0
     *  Interrupt(0x81) | Interrupt(0x81)
     * -----------------+ BulkIN(0x02)
     * Interface 1      | BulkOUT(0x83)
     *   BulkIN(0x02)   |
     *   BulkOUT(0x83)  |
     */

    UPLCOM_Handle->ctrl_iface_no = interface;
    UPLCOM_Handle->iface_index[1] = UPLCOM_IFACE_INDEX;

    if (phost->device.CfgDesc.bNumInterfaces > UPLCOM_SECOND_IFACE_INDEX) {
        /*
         * Note: This code may not work, we have no test devices to exercise
         * it, and the relevant device is likely excluded from this driver
         * anyways.
         */
        USBH_InterfaceDescTypeDef *pif = &phost->device.CfgDesc.Itf_Desc[UPLCOM_SECOND_IFACE_INDEX];
        UPLCOM_Handle->data_iface_no = pif->bInterfaceNumber;
        UPLCOM_Handle->iface_index[0] = UPLCOM_SECOND_IFACE_INDEX;
        interface = UPLCOM_SECOND_IFACE_INDEX;
        status = USBH_SelectInterface(phost, interface);
        if (status != USBH_OK) {
            USBH_ErrLog("UPLCOM: Cannot select secondary interface");
            return USBH_FAIL;
        }
    } else {
        UPLCOM_Handle->data_iface_no = UPLCOM_Handle->ctrl_iface_no;
        UPLCOM_Handle->iface_index[0] = UPLCOM_IFACE_INDEX;
    }

    /* Find the device endpoints */
    uint8_t max_endpoints = ((phost->device.CfgDesc.Itf_Desc[interface].bNumEndpoints <= USBH_MAX_NUM_ENDPOINTS) ?
        phost->device.CfgDesc.Itf_Desc[interface].bNumEndpoints : USBH_MAX_NUM_ENDPOINTS);

    for (uint8_t i = 0; i < max_endpoints; i++) {
        USBH_EpDescTypeDef *ep = &phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[i];

        if ((ep->bmAttributes & 0x03U) == USBH_EP_INTERRUPT) {
            UPLCOM_Handle->endpoints.notif_ep = ep->bEndpointAddress;
            UPLCOM_Handle->endpoints.notif_ep_size = ep->wMaxPacketSize;
        } else if ((ep->bmAttributes & 0x03U) == USBH_EP_BULK) {
            if (ep->bEndpointAddress & 0x80U) {
                UPLCOM_Handle->endpoints.in_ep = ep->bEndpointAddress;
                UPLCOM_Handle->endpoints.in_ep_size = ep->wMaxPacketSize;
            } else {
                UPLCOM_Handle->endpoints.out_ep = ep->bEndpointAddress;
                UPLCOM_Handle->endpoints.out_ep_size = ep->wMaxPacketSize;
            }
        }
    }

    if (UPLCOM_Handle->endpoints.notif_ep == 0
        || UPLCOM_Handle->endpoints.in_ep == 0
        || UPLCOM_Handle->endpoints.out_ep == 0) {
        USBH_ErrLog("UPLCOM: Cannot find all endpoints");
        return USBH_FAIL;
    }

    USBH_UsrLog("UPLCOM: in_ep_size=%d, out_ep_size=%d, notif_ep_size=%d",
        UPLCOM_Handle->endpoints.in_ep_size,
        UPLCOM_Handle->endpoints.out_ep_size,
        UPLCOM_Handle->endpoints.notif_ep_size);

    /* Open the notification pipe */
    UPLCOM_Handle->endpoints.notif_pipe = USBH_AllocPipe(phost, UPLCOM_Handle->endpoints.notif_ep);

    USBH_OpenPipe(phost, UPLCOM_Handle->endpoints.notif_pipe, UPLCOM_Handle->endpoints.notif_ep,
        phost->device.address, phost->device.speed, USB_EP_TYPE_INTR,
        UPLCOM_Handle->endpoints.notif_ep_size);

    USBH_LL_SetToggle(phost, UPLCOM_Handle->endpoints.notif_pipe, 0U);

    /* Open the bulk data I/O pipes */
    UPLCOM_Handle->endpoints.out_pipe = USBH_AllocPipe(phost, UPLCOM_Handle->endpoints.out_ep);
    UPLCOM_Handle->endpoints.in_pipe = USBH_AllocPipe(phost, UPLCOM_Handle->endpoints.in_ep);

    USBH_OpenPipe(phost, UPLCOM_Handle->endpoints.out_pipe, UPLCOM_Handle->endpoints.out_ep,
        phost->device.address, phost->device.speed, USB_EP_TYPE_BULK,
        UPLCOM_Handle->endpoints.out_ep_size);
    USBH_OpenPipe(phost, UPLCOM_Handle->endpoints.in_pipe, UPLCOM_Handle->endpoints.in_ep,
        phost->device.address, phost->device.speed, USB_EP_TYPE_BULK,
        UPLCOM_Handle->endpoints.in_ep_size);

    /*
     * Enable our custom tweak to prevent the HAL USB interrupt handler from
     * re-activating the IN channel every single time it is halted with a
     * NAK/URB_NOTREADY event. Without this, attempting to poll without
     * available data will make the interrupt repeatedly fire and cause
     * problems.
     */
    HCD_HandleTypeDef *hhcd = phost->pData;
    hhcd->hc[UPLCOM_Handle->endpoints.in_pipe].no_reactivate_on_nak = 1;

    USBH_LL_SetToggle(phost, UPLCOM_Handle->endpoints.out_pipe, 0U);
    USBH_LL_SetToggle(phost, UPLCOM_Handle->endpoints.in_pipe, 0U);

    /* Allocate the I/O buffers */
    UPLCOM_Handle->endpoints.notif_ep_buffer = (uint8_t *)USBH_malloc(UPLCOM_Handle->endpoints.notif_ep_size);
    if (!UPLCOM_Handle->endpoints.notif_ep_buffer) {
        USBH_ErrLog("UPLCOM: Cannot allocate memory for UPLCOM notification buffer");
        return USBH_FAIL;
    }
    USBH_memset(UPLCOM_Handle->endpoints.notif_ep_buffer, 0, UPLCOM_Handle->endpoints.notif_ep_size);

    UPLCOM_Handle->endpoints.in_ep_buffer = (uint8_t *)USBH_malloc(UPLCOM_Handle->endpoints.in_ep_size);
    if (!UPLCOM_Handle->endpoints.in_ep_buffer) {
        USBH_ErrLog("UPLCOM: Cannot allocate memory for UPLCOM receive buffer");
        return USBH_FAIL;
    }
    UPLCOM_Handle->rx_data = UPLCOM_Handle->endpoints.in_ep_buffer;
    UPLCOM_Handle->rx_data_length = UPLCOM_Handle->endpoints.in_ep_size;
    USBH_memset(UPLCOM_Handle->endpoints.in_ep_buffer, 0, UPLCOM_Handle->endpoints.in_ep_size);

    UPLCOM_Handle->endpoints.out_ep_buffer = (uint8_t *)USBH_malloc(UPLCOM_Handle->endpoints.out_ep_size);
    if (!UPLCOM_Handle->endpoints.out_ep_buffer) {
        USBH_ErrLog("UPLCOM: Cannot allocate memory for UPLCOM transmit buffer");
        return USBH_FAIL;
    }
    USBH_memset(UPLCOM_Handle->endpoints.out_ep_buffer, 0, UPLCOM_Handle->endpoints.out_ep_size);

    /* Startup reset sequence, if necessary for the chip type */
    if (UPLCOM_Handle->chiptype != TYPE_PL2303HXN) {
        phost->Control.setup.b.bmRequestType = UT_WRITE_VENDOR_DEVICE;
        phost->Control.setup.b.bRequest = UPLCOM_SET_REQUEST;
        phost->Control.setup.b.wValue.w = 0U;
        phost->Control.setup.b.wIndex.w = UPLCOM_Handle->data_iface_no;
        phost->Control.setup.b.wLength.w = 0U;

        do {
            status = USBH_CtlReq(phost, NULL, 0);
        } while (status == USBH_BUSY);

        if (status != USBH_OK) {
            USBH_ErrLog("UPLCOM: Initialization reset failed: %d", status);
            return USBH_FAIL;
        }
    }

    if (UPLCOM_Handle->chiptype == TYPE_PL2303) {
        /* HX variants seem to lock up after a clear stall request. */
        /*
         * The FreeBSD code sets the stall flags on the in and out pipes
         * here. Have no idea exactly how to do this, or if it is necessary.
         * May just leave this code unwritten until test hardware is available.
         */
    } else if (UPLCOM_Handle->chiptype == TYPE_PL2303HX
        || UPLCOM_Handle->chiptype == TYPE_PL2303HXD) {
        /* Reset upstream data pipes */
        if (UPLCOM_PL2303_Do(phost, UT_WRITE_VENDOR_DEVICE, UPLCOM_SET_REQUEST, 8, 0, 0) != USBH_OK) {
            USBH_ErrLog("UPLCOM: Could not reset upstream data pipes (8,0)");
            return USBH_FAIL;
        }
        if (UPLCOM_PL2303_Do(phost, UT_WRITE_VENDOR_DEVICE, UPLCOM_SET_REQUEST, 9, 0, 0) != USBH_OK) {
            USBH_ErrLog("UPLCOM: Could not reset upstream data pipes (9,0)");
            return USBH_FAIL;
        }
    } else if (UPLCOM_Handle->chiptype == TYPE_PL2303HXN) {
        /* Reset upstream data pipes */
        if (UPLCOM_PL2303_Do(phost, UT_WRITE_VENDOR_DEVICE, UPLCOM_SET_REQUEST_PL2303HXN, 0x07, 0x03, 0) != USBH_OK) {
            USBH_ErrLog("UPLCOM: Could not reset upstream data pipes (7,3)");
            return USBH_FAIL;
        }
    }

    /* Final device initialization, if necessary for the chip type */
    if (UPLCOM_Handle->chiptype != TYPE_PL2303HXN) {
        if (UPLCOM_PL2303_Do(phost, UT_READ_VENDOR_DEVICE, UPLCOM_SET_REQUEST, 0x8484, 0, 1) != USBH_OK
            || UPLCOM_PL2303_Do(phost, UT_WRITE_VENDOR_DEVICE, UPLCOM_SET_REQUEST, 0x0404, 0, 0) != USBH_OK
            || UPLCOM_PL2303_Do(phost, UT_READ_VENDOR_DEVICE, UPLCOM_SET_REQUEST, 0x8484, 0, 1) != USBH_OK
            || UPLCOM_PL2303_Do(phost, UT_READ_VENDOR_DEVICE, UPLCOM_SET_REQUEST, 0x8383, 0, 1) != USBH_OK
            || UPLCOM_PL2303_Do(phost, UT_READ_VENDOR_DEVICE, UPLCOM_SET_REQUEST, 0x8484, 0, 1) != USBH_OK
            || UPLCOM_PL2303_Do(phost, UT_WRITE_VENDOR_DEVICE, UPLCOM_SET_REQUEST, 0x0404, 1, 0) != USBH_OK
            || UPLCOM_PL2303_Do(phost, UT_READ_VENDOR_DEVICE, UPLCOM_SET_REQUEST, 0x8484, 0, 1) != USBH_OK
            || UPLCOM_PL2303_Do(phost, UT_READ_VENDOR_DEVICE, UPLCOM_SET_REQUEST, 0x8383, 0, 1) != USBH_OK
            || UPLCOM_PL2303_Do(phost, UT_WRITE_VENDOR_DEVICE, UPLCOM_SET_REQUEST, 0, 1, 0) != USBH_OK
            || UPLCOM_PL2303_Do(phost, UT_WRITE_VENDOR_DEVICE, UPLCOM_SET_REQUEST, 1, 0, 0) != USBH_OK) {
            USBH_ErrLog("UPLCOM: Could not complete init sequence");
            return USBH_FAIL;
        }

        if (UPLCOM_Handle->chiptype != TYPE_PL2303) {
            status = UPLCOM_PL2303_Do(phost, UT_WRITE_VENDOR_DEVICE, UPLCOM_SET_REQUEST, 2, 0x44, 0);
        } else {
            status = UPLCOM_PL2303_Do(phost, UT_WRITE_VENDOR_DEVICE, UPLCOM_SET_REQUEST, 2, 0x24, 0);
        }
        if (status != USBH_OK) {
            USBH_ErrLog("UPLCOM: Could not complete final init request");
            return USBH_FAIL;
        }
    }

    UPLCOM_Handle->state = USBH_UPLCOM_STATE_IDLE;
    if (UPLCOM_Handle->poll < UPLCOM_MIN_POLL) {
        UPLCOM_Handle->poll = UPLCOM_MIN_POLL;
    }

    return USBH_OK;
}

USBH_StatusTypeDef USBH_UPLCOM_InterfaceDeInit(USBH_HandleTypeDef *phost)
{
    UPLCOM_HandleTypeDef *UPLCOM_Handle = (UPLCOM_HandleTypeDef *)phost->pActiveClass->pData;

    USBH_UsrLog("UPLCOM: USBH_UPLCOM_InterfaceDeInit");

    if (!phost->pActiveClass->pData) {
        return USBH_FAIL;
    }

    /* Close and free the notification pipe */
    if (UPLCOM_Handle->endpoints.notif_pipe) {
        USBH_ClosePipe(phost, UPLCOM_Handle->endpoints.notif_pipe);
        USBH_FreePipe(phost, UPLCOM_Handle->endpoints.notif_pipe);
        UPLCOM_Handle->endpoints.notif_pipe = 0U;
    }

    /* Close and free the I/O pipes */
    if (UPLCOM_Handle->endpoints.in_pipe) {
        USBH_ClosePipe(phost, UPLCOM_Handle->endpoints.in_pipe);
        USBH_FreePipe(phost, UPLCOM_Handle->endpoints.in_pipe);
        UPLCOM_Handle->endpoints.in_pipe = 0U;
    }
    if (UPLCOM_Handle->endpoints.out_pipe) {
        USBH_ClosePipe(phost, UPLCOM_Handle->endpoints.out_pipe);
        USBH_FreePipe(phost, UPLCOM_Handle->endpoints.out_pipe);
        UPLCOM_Handle->endpoints.out_pipe = 0U;
    }

    /* Free the I/O data buffers */
    if (UPLCOM_Handle->endpoints.in_ep_buffer) {
        USBH_free(UPLCOM_Handle->endpoints.in_ep_buffer);
        UPLCOM_Handle->endpoints.in_ep_buffer = NULL;
        UPLCOM_Handle->rx_data = NULL;
        UPLCOM_Handle->rx_data_length = 0;
    }

    if (UPLCOM_Handle->endpoints.out_ep_buffer) {
        USBH_free(UPLCOM_Handle->endpoints.out_ep_buffer);
        UPLCOM_Handle->endpoints.out_ep_buffer = NULL;
        UPLCOM_Handle->tx_data = NULL;
        UPLCOM_Handle->tx_data_length = 0;
    }

    if (UPLCOM_Handle->endpoints.notif_ep_buffer) {
        USBH_free(UPLCOM_Handle->endpoints.notif_ep_buffer);
        UPLCOM_Handle->endpoints.notif_ep_buffer = NULL;
    }

    /* Free the device handle */
    if (phost->pActiveClass->pData) {
        USBH_free(phost->pActiveClass->pData);
        phost->pActiveClass->pData = 0U;
    }

    return USBH_OK;
}

USBH_StatusTypeDef UPLCOM_PL2303_Do(USBH_HandleTypeDef *phost,
    uint8_t req_type, uint8_t request, uint16_t value, uint16_t index,
    uint16_t length)
{
    USBH_StatusTypeDef status;
    uint8_t buf[4] = {0};

    phost->Control.setup.b.bmRequestType = req_type;
    phost->Control.setup.b.bRequest = request;
    phost->Control.setup.b.wValue.w = value;
    phost->Control.setup.b.wIndex.w = index;
    phost->Control.setup.b.wLength.w = length;

    do {
        status = USBH_CtlReq(phost, buf, sizeof(buf));
    } while (status == USBH_BUSY);

    return status;
}

USBH_StatusTypeDef USBH_UPLCOM_SetDtr(USBH_HandleTypeDef *phost, uint8_t onoff)
{
    UPLCOM_HandleTypeDef *UPLCOM_Handle = (UPLCOM_HandleTypeDef *)phost->pActiveClass->pData;

    if (onoff) {
        UPLCOM_Handle->line |= CDC_ACTIVATE_SIGNAL_DTR;
    } else {
        UPLCOM_Handle->line &= ~CDC_ACTIVATE_SIGNAL_DTR;
    }

    phost->Control.setup.b.bmRequestType = UT_WRITE_CLASS_INTERFACE;
    phost->Control.setup.b.bRequest = CDC_SET_CONTROL_LINE_STATE;
    phost->Control.setup.b.wValue.w = UPLCOM_Handle->line;
    phost->Control.setup.b.wIndex.w = UPLCOM_Handle->data_iface_no;;
    phost->Control.setup.b.wLength.w = 0U;

    return USBH_CtlReq(phost, NULL, 0);
}

USBH_StatusTypeDef USBH_UPLCOM_SetRts(USBH_HandleTypeDef *phost, uint8_t onoff)
{
    UPLCOM_HandleTypeDef *UPLCOM_Handle = (UPLCOM_HandleTypeDef *)phost->pActiveClass->pData;

    if (onoff) {
        UPLCOM_Handle->line |= CDC_ACTIVATE_CARRIER_SIGNAL_RTS;
    } else {
        UPLCOM_Handle->line &= ~CDC_ACTIVATE_CARRIER_SIGNAL_RTS;
    }

    phost->Control.setup.b.bmRequestType = UT_WRITE_CLASS_INTERFACE;
    phost->Control.setup.b.bRequest = CDC_SET_CONTROL_LINE_STATE;
    phost->Control.setup.b.wValue.w = UPLCOM_Handle->line;
    phost->Control.setup.b.wIndex.w = UPLCOM_Handle->data_iface_no;;
    phost->Control.setup.b.wLength.w = 0U;

    return USBH_CtlReq(phost, NULL, 0);
}

USBH_StatusTypeDef USBH_UPLCOM_SetBreak(USBH_HandleTypeDef *phost, uint8_t onoff)
{
    UPLCOM_HandleTypeDef *UPLCOM_Handle = (UPLCOM_HandleTypeDef *)phost->pActiveClass->pData;
    uint16_t temp;

    temp = (onoff ? UCDC_BREAK_ON : UCDC_BREAK_OFF);

    phost->Control.setup.b.bmRequestType = UT_WRITE_CLASS_INTERFACE;
    phost->Control.setup.b.bRequest = CDC_SEND_BREAK;
    phost->Control.setup.b.wValue.w = temp;
    phost->Control.setup.b.wIndex.w = UPLCOM_Handle->data_iface_no;;
    phost->Control.setup.b.wLength.w = 0U;

    return USBH_CtlReq(phost, NULL, 0);
}

USBH_StatusTypeDef USBH_UPLCOM_SetRtsCts(USBH_HandleTypeDef *phost, uint8_t onoff)
{
    UPLCOM_HandleTypeDef *UPLCOM_Handle = (UPLCOM_HandleTypeDef *)phost->pActiveClass->pData;

    phost->Control.setup.b.bmRequestType = UT_WRITE_VENDOR_DEVICE;
    if (onoff) {
        if (UPLCOM_Handle->chiptype == TYPE_PL2303HXN) {
            phost->Control.setup.b.bRequest = UPLCOM_SET_REQUEST_PL2303HXN;
            phost->Control.setup.b.wValue.w = UPLCOM_CRTSCTS_REG_PL2303HXN;
            phost->Control.setup.b.wIndex.w = UPLCOM_SET_CRTSCTS_PL2303HXN;
        } else {
            phost->Control.setup.b.bRequest = UPLCOM_SET_REQUEST;
            phost->Control.setup.b.wValue.w = 0U;
            if (UPLCOM_Handle->chiptype != TYPE_PL2303) {
                phost->Control.setup.b.wIndex.w = UPLCOM_SET_CRTSCTS_PL2303X;
            } else {
                phost->Control.setup.b.wIndex.w = UPLCOM_SET_CRTSCTS;
            }
        }
        phost->Control.setup.b.wLength.w = 0U;
    } else {
        if (UPLCOM_Handle->chiptype == TYPE_PL2303HXN) {
            phost->Control.setup.b.bRequest = UPLCOM_SET_REQUEST_PL2303HXN;
            phost->Control.setup.b.wValue.w = UPLCOM_CRTSCTS_REG_PL2303HXN;
            phost->Control.setup.b.wIndex.w = UPLCOM_CLEAR_CRTSCTS_PL2303HXN;
        } else {
            phost->Control.setup.b.bRequest = UPLCOM_SET_REQUEST;
            phost->Control.setup.b.wValue.w = 0U;
            phost->Control.setup.b.wIndex.w = 0U;
        }
        phost->Control.setup.b.wLength.w = 0U;

    }
    return USBH_CtlReq(phost, NULL, 0);
}

USBH_StatusTypeDef USBH_UPLCOM_SetLineCoding(USBH_HandleTypeDef *phost, CDC_LineCodingTypeDef *linecoding)
{
    USBH_StatusTypeDef status;
    UPLCOM_HandleTypeDef *UPLCOM_Handle = (UPLCOM_HandleTypeDef *)phost->pActiveClass->pData;

    if (phost->gState == HOST_CLASS) {
        UPLCOM_Handle->state = USBH_UPLCOM_STATE_SET_LINE_CODING;
        UPLCOM_Handle->user_linecoding = linecoding;

#if (USBH_USE_OS == 1U)
        phost->os_msg = (uint32_t)USBH_CLASS_EVENT;
#if (osCMSIS < 0x20000U)
        (void)osMessagePut(phost->os_event, phost->os_msg, 0U);
#else
        (void)osMessageQueuePut(phost->os_event, &phost->os_msg, 0U, 0U);
#endif
#endif
        status = USBH_OK;
    } else if (phost->gState == HOST_CLASS_REQUEST) {
        status = UPLCOM_SetLineCoding(phost, linecoding);
    } else {
        USBH_ErrLog("UPLCOM: Invalid state for setting line coding: %d", phost->gState);
        status = USBH_FAIL;
    }

    return status;
}

USBH_StatusTypeDef UPLCOM_SetLineCoding(USBH_HandleTypeDef *phost, CDC_LineCodingTypeDef *linecoding)
{
    phost->Control.setup.b.bmRequestType = UT_WRITE_CLASS_INTERFACE;
    phost->Control.setup.b.bRequest = CDC_SET_LINE_CODING;
    phost->Control.setup.b.wValue.w = 0U;
    phost->Control.setup.b.wIndex.w = 0U;
    phost->Control.setup.b.wLength.w = LINE_CODING_STRUCTURE_SIZE;

    return USBH_CtlReq(phost, linecoding->Array, LINE_CODING_STRUCTURE_SIZE);
}

USBH_StatusTypeDef USBH_UPLCOM_GetLineCoding(USBH_HandleTypeDef *phost, CDC_LineCodingTypeDef *linecoding)
{
    UPLCOM_HandleTypeDef *UPLCOM_Handle = (UPLCOM_HandleTypeDef *)phost->pActiveClass->pData;

    if ((phost->gState == HOST_CLASS) || (phost->gState == HOST_CLASS_REQUEST)) {
        *linecoding = UPLCOM_Handle->linecoding;
        return USBH_OK;
    } else {
        return USBH_FAIL;
    }
}

USBH_StatusTypeDef UPLCOM_GetLineCoding(USBH_HandleTypeDef *phost, CDC_LineCodingTypeDef *linecoding)
{
    phost->Control.setup.b.bmRequestType = UT_READ_CLASS_INTERFACE;
    phost->Control.setup.b.bRequest = CDC_GET_LINE_CODING;
    phost->Control.setup.b.wValue.w = 0U;
    phost->Control.setup.b.wIndex.w = 0U;
    phost->Control.setup.b.wLength.w = LINE_CODING_STRUCTURE_SIZE;

    return USBH_CtlReq(phost, linecoding->Array, LINE_CODING_STRUCTURE_SIZE);
}

USBH_StatusTypeDef USBH_UPLCOM_ClassRequest(USBH_HandleTypeDef *phost)
{
    USBH_StatusTypeDef status;
    UPLCOM_HandleTypeDef *UPLCOM_Handle = (UPLCOM_HandleTypeDef *)phost->pActiveClass->pData;

    status = UPLCOM_GetLineCoding(phost, &UPLCOM_Handle->linecoding);
    if (status == USBH_OK) {
        phost->pUser(phost, HOST_USER_CLASS_ACTIVE);
    } else if (status == USBH_NOT_SUPPORTED) {
        USBH_ErrLog("UPLCOM: Unable to get device line coding");
    }

    return status;
}

USBH_StatusTypeDef USBH_UPLCOM_Process(USBH_HandleTypeDef *phost)
{
    USBH_StatusTypeDef status = USBH_BUSY;
    USBH_StatusTypeDef req_status = USBH_OK;
    UPLCOM_HandleTypeDef *UPLCOM_Handle = (UPLCOM_HandleTypeDef *)phost->pActiveClass->pData;

    switch (UPLCOM_Handle->state) {
    case USBH_UPLCOM_STATE_IDLE:
        status = USBH_OK;
        break;

    case USBH_UPLCOM_STATE_SET_LINE_CODING:
      req_status = UPLCOM_SetLineCoding(phost, UPLCOM_Handle->user_linecoding);

      if (req_status == USBH_OK) {
          UPLCOM_Handle->state = USBH_UPLCOM_STATE_GET_LAST_LINE_CODING;
      } else {
          if (req_status != USBH_BUSY) {
              UPLCOM_Handle->state = USBH_UPLCOM_STATE_ERROR;
          }
      }
      break;

    case USBH_UPLCOM_STATE_GET_LAST_LINE_CODING:
        req_status = UPLCOM_GetLineCoding(phost, &(UPLCOM_Handle->linecoding));

        if (req_status == USBH_OK) {
            UPLCOM_Handle->state = USBH_UPLCOM_STATE_IDLE;

            if ((UPLCOM_Handle->linecoding.b.bCharFormat == UPLCOM_Handle->user_linecoding->b.bCharFormat)
                && (UPLCOM_Handle->linecoding.b.bDataBits == UPLCOM_Handle->user_linecoding->b.bDataBits)
                && (UPLCOM_Handle->linecoding.b.bParityType == UPLCOM_Handle->user_linecoding->b.bParityType)
                && (UPLCOM_Handle->linecoding.b.dwDTERate == UPLCOM_Handle->user_linecoding->b.dwDTERate)) {
                USBH_UPLCOM_LineCodingChanged(phost);
            }
        } else {
            if (req_status != USBH_BUSY) {
                UPLCOM_Handle->state = USBH_UPLCOM_STATE_ERROR;
            }
        }
        break;

    case USBH_UPLCOM_STATE_TRANSFER_DATA:
        UPLCOM_Handle->timer = phost->Timer;
        UPLCOM_ProcessNotification(phost);
        UPLCOM_ProcessTransmission(phost);
        UPLCOM_ProcessReception(phost);
        if (UPLCOM_Handle->data_rx_state == USBH_UPLCOM_IDLE && UPLCOM_Handle->data_tx_state == USBH_UPLCOM_IDLE) {
            UPLCOM_Handle->state = USBH_UPLCOM_STATE_IDLE;
        }
        break;

    case USBH_UPLCOM_STATE_ERROR:
        req_status = USBH_ClrFeature(phost, 0x00U);
        if (req_status == USBH_OK) {
            UPLCOM_Handle->state = USBH_UPLCOM_STATE_IDLE;
        }
        break;

    default:
        break;
    }

    return status;
}

USBH_StatusTypeDef USBH_UPLCOM_SOFProcess(USBH_HandleTypeDef *phost)
{
    UPLCOM_HandleTypeDef *UPLCOM_Handle = (UPLCOM_HandleTypeDef *)phost->pActiveClass->pData;

    if (UPLCOM_Handle->state == USBH_UPLCOM_STATE_IDLE) {
        if ((phost->Timer - UPLCOM_Handle->timer) >= UPLCOM_Handle->poll) {
            UPLCOM_Handle->state = USBH_UPLCOM_STATE_TRANSFER_DATA;
            if (UPLCOM_Handle->data_notif_state == USBH_UPLCOM_IDLE) {
                UPLCOM_Handle->data_notif_state = USBH_UPLCOM_NOTIFY_DATA;
            }
            if (UPLCOM_Handle->data_rx_state == USBH_UPLCOM_IDLE) {
                UPLCOM_Handle->data_rx_state = USBH_UPLCOM_RECEIVE_DATA;
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

USBH_StatusTypeDef USBH_UPLCOM_Transmit(USBH_HandleTypeDef *phost, const uint8_t *pbuff, uint32_t length)
{
    USBH_StatusTypeDef status = USBH_BUSY;
    UPLCOM_HandleTypeDef *UPLCOM_Handle = (UPLCOM_HandleTypeDef *)phost->pActiveClass->pData;

    if (length > UPLCOM_Handle->endpoints.out_ep_size) {
        USBH_ErrLog("UPLCOM: Transmit size too large: %lu > %d",
            length, UPLCOM_Handle->endpoints.out_ep_size);
        return USBH_FAIL;
    }

    if ((UPLCOM_Handle->state == USBH_UPLCOM_STATE_IDLE) || (UPLCOM_Handle->state == USBH_UPLCOM_STATE_TRANSFER_DATA)) {
        USBH_memcpy(UPLCOM_Handle->endpoints.out_ep_buffer, pbuff, length);
        UPLCOM_Handle->tx_data = UPLCOM_Handle->endpoints.out_ep_buffer;
        UPLCOM_Handle->tx_data_length = length;
        UPLCOM_Handle->state = USBH_UPLCOM_STATE_TRANSFER_DATA;
        UPLCOM_Handle->data_tx_state = USBH_UPLCOM_SEND_DATA;
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

USBH_StatusTypeDef USBH_UPLCOM_Stop(USBH_HandleTypeDef *phost)
{
    UPLCOM_HandleTypeDef *UPLCOM_Handle = (UPLCOM_HandleTypeDef *)phost->pActiveClass->pData;

    if (phost->gState == HOST_CLASS) {
        USBH_UsrLog("UPLCOM: USBH_UPLCOM_Stop");
        UPLCOM_Handle->state = USBH_UPLCOM_STATE_IDLE;
        USBH_ClosePipe(phost, UPLCOM_Handle->endpoints.notif_pipe);
        USBH_ClosePipe(phost, UPLCOM_Handle->endpoints.in_pipe);
        USBH_ClosePipe(phost, UPLCOM_Handle->endpoints.out_pipe);
    }
    return USBH_OK;
}

void UPLCOM_ProcessNotification(USBH_HandleTypeDef *phost)
{
    UPLCOM_HandleTypeDef *UPLCOM_Handle = (UPLCOM_HandleTypeDef *)phost->pActiveClass->pData;
    USBH_URBStateTypeDef URB_Status = USBH_URB_IDLE;
    uint32_t length;

    /*
     * Not entirely sure notification reads are working correctly, however
     * they are not important for basic functionality. Leaving this code
     * here as a placeholder for potentially fixing them in the future.
     */

    switch (UPLCOM_Handle->data_notif_state) {
    case USBH_UPLCOM_NOTIFY_DATA:
        USBH_InterruptReceiveData(phost,
            UPLCOM_Handle->endpoints.notif_ep_buffer,
            UPLCOM_Handle->endpoints.notif_ep_size,
            UPLCOM_Handle->endpoints.notif_pipe);
        UPLCOM_Handle->data_notif_state = USBH_UPLCOM_NOTIFY_DATA_WAIT;
        break;
    case USBH_UPLCOM_NOTIFY_DATA_WAIT:
        URB_Status = USBH_LL_GetURBState(phost, UPLCOM_Handle->endpoints.notif_pipe);
        length = USBH_LL_GetLastXferSize(phost, UPLCOM_Handle->endpoints.notif_pipe);

        if (URB_Status == USBH_URB_DONE || URB_Status == USBH_URB_IDLE) {
            if (length > UPLCOM_STATE_INDEX) {
                uint8_t status = UPLCOM_Handle->endpoints.notif_ep_buffer[UPLCOM_STATE_INDEX];
                USBH_UsrLog("UPLCOM: Notify status = 0x%02X", status);
            }
            UPLCOM_Handle->data_notif_state = USBH_UPLCOM_IDLE;
        } else if (URB_Status == USBH_URB_STALL) {
            if (USBH_ClrFeature(phost, UPLCOM_Handle->endpoints.notif_ep) == USBH_OK) {
                UPLCOM_Handle->data_notif_state = USBH_UPLCOM_NOTIFY_DATA;
            }
        } else {
            UPLCOM_Handle->data_notif_state = USBH_UPLCOM_IDLE;
        }
        break;
    default:
        break;
    }
}

void UPLCOM_ProcessTransmission(USBH_HandleTypeDef *phost)
{
    UPLCOM_HandleTypeDef *UPLCOM_Handle = (UPLCOM_HandleTypeDef *)phost->pActiveClass->pData;
    USBH_URBStateTypeDef URB_Status = USBH_URB_IDLE;

    switch (UPLCOM_Handle->data_tx_state) {
    case USBH_UPLCOM_SEND_DATA:
        if (UPLCOM_Handle->tx_data_length > UPLCOM_Handle->endpoints.out_ep_size) {
            /*
             * Right now this case is not possible, because we allocate tx_data
             * and limit what can be sent based on out_ep_size
             */
            USBH_BulkSendData(phost,
                UPLCOM_Handle->tx_data,
                UPLCOM_Handle->endpoints.out_ep_size,
                UPLCOM_Handle->endpoints.out_pipe,
                1U);
        } else {
            USBH_BulkSendData(phost,
                UPLCOM_Handle->tx_data,
                (uint16_t)UPLCOM_Handle->tx_data_length,
                UPLCOM_Handle->endpoints.out_pipe,
                1U);
        }
        UPLCOM_Handle->data_tx_state = USBH_UPLCOM_SEND_DATA_WAIT;
        break;

    case USBH_UPLCOM_SEND_DATA_WAIT:
        URB_Status = USBH_LL_GetURBState(phost, UPLCOM_Handle->endpoints.out_pipe);

        /* Check if the transmission is complete */
        if (URB_Status == USBH_URB_DONE) {
            if (UPLCOM_Handle->tx_data_length > UPLCOM_Handle->endpoints.out_ep_size) {
                UPLCOM_Handle->tx_data_length -= UPLCOM_Handle->endpoints.out_ep_size;
                UPLCOM_Handle->tx_data += UPLCOM_Handle->endpoints.out_ep_size;
            } else {
                UPLCOM_Handle->tx_data_length = 0U;
            }

            if (UPLCOM_Handle->tx_data_length > 0U) {
                UPLCOM_Handle->data_tx_state = USBH_UPLCOM_SEND_DATA;
            } else {
                UPLCOM_Handle->data_tx_state = USBH_UPLCOM_IDLE;
                UPLCOM_Handle->tx_data = NULL;
                UPLCOM_Handle->tx_data_length = 0;
                USBH_UPLCOM_TransmitCallback(phost);
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
            UPLCOM_Handle->data_tx_state = USBH_UPLCOM_SEND_DATA;

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

void UPLCOM_ProcessReception(USBH_HandleTypeDef *phost)
{
    UPLCOM_HandleTypeDef *UPLCOM_Handle = (UPLCOM_HandleTypeDef *)phost->pActiveClass->pData;
    USBH_URBStateTypeDef URB_Status = USBH_URB_IDLE;
    uint32_t length;

    switch (UPLCOM_Handle->data_rx_state) {
    case USBH_UPLCOM_RECEIVE_DATA:
        USBH_BulkReceiveData(phost,
            UPLCOM_Handle->rx_data,
            UPLCOM_Handle->endpoints.in_ep_size,
            UPLCOM_Handle->endpoints.in_pipe);

        UPLCOM_Handle->data_rx_state = USBH_UPLCOM_RECEIVE_DATA_WAIT;
        break;

    case USBH_UPLCOM_RECEIVE_DATA_WAIT:
        URB_Status = USBH_LL_GetURBState(phost, UPLCOM_Handle->endpoints.in_pipe);

        if (URB_Status == USBH_URB_DONE) {
            length = USBH_LL_GetLastXferSize(phost, UPLCOM_Handle->endpoints.in_pipe);

            if (((UPLCOM_Handle->rx_data_length - length) > 0U) && (length > UPLCOM_Handle->endpoints.in_ep_size)) {
                UPLCOM_Handle->rx_data_length -= length;
                UPLCOM_Handle->rx_data += length;
                UPLCOM_Handle->data_rx_state = USBH_UPLCOM_RECEIVE_DATA;
            } else {
                UPLCOM_Handle->data_rx_state = USBH_UPLCOM_IDLE;

                size_t recv_length;
                if (UPLCOM_Handle->rx_data == UPLCOM_Handle->endpoints.in_ep_buffer) {
                    recv_length = length;
                } else {
                    recv_length = UPLCOM_Handle->rx_data - UPLCOM_Handle->endpoints.in_ep_buffer;
                    UPLCOM_Handle->rx_data = UPLCOM_Handle->endpoints.in_ep_buffer;
                }

                if (recv_length > 0) {
                    USBH_UPLCOM_ReceiveCallback(phost, UPLCOM_Handle->rx_data, recv_length);
                }
                UPLCOM_Handle->rx_data = UPLCOM_Handle->endpoints.in_ep_buffer;
                UPLCOM_Handle->rx_data_length = UPLCOM_Handle->endpoints.in_ep_size;
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
            UPLCOM_Handle->data_rx_state = USBH_UPLCOM_IDLE;
        }
        break;

    default:
        break;
    }
}

__weak void USBH_UPLCOM_TransmitCallback(USBH_HandleTypeDef *phost)
{
    /* Prevent unused argument(s) compilation warning */
    UNUSED(phost);
}

__weak void USBH_UPLCOM_ReceiveCallback(USBH_HandleTypeDef *phost, uint8_t *data, size_t length)
{
    /* Prevent unused argument(s) compilation warning */
    UNUSED(phost);
}

__weak void USBH_UPLCOM_LineCodingChanged(USBH_HandleTypeDef *phost)
{
    /* Prevent unused argument(s) compilation warning */
    UNUSED(phost);
}
