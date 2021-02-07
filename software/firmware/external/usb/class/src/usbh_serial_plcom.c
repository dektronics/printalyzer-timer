#include "usbh_serial_plcom.h"

#define PLCOM_CONFIG_INDEX         0
#define PLCOM_IFACE_INDEX          0
#define PLCOM_SECOND_IFACE_INDEX   1

#define PLCOM_SET_REQUEST              0x01
#define PLCOM_SET_REQUEST_PL2303HXN    0x80
#define PLCOM_SET_CRTSCTS              0x41
#define PLCOM_SET_CRTSCTS_PL2303X      0x61
#define PLCOM_SET_CRTSCTS_PL2303HXN    0xFA
#define PLCOM_CLEAR_CRTSCTS_PL2303HXN  0xFF
#define PLCOM_CRTSCTS_REG_PL2303HXN    0x0A
#define PLCOM_STATUS_REG_PL2303HX      0x8080

#define PLCOM_STATE_INDEX 8

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

static USBH_StatusTypeDef USBH_PLCOM_InterfaceInit(USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef USBH_PLCOM_InterfaceDeInit(USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef USBH_PLCOM_ClassRequest(USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef USBH_PLCOM_Process(USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef USBH_PLCOM_SOFProcess(USBH_HandleTypeDef *phost);

static USBH_StatusTypeDef PLCOM_PL2303_Do(USBH_HandleTypeDef *phost,
    uint8_t req_type, uint8_t request, uint16_t value, uint16_t index,
    uint16_t length);
static USBH_StatusTypeDef PLCOM_SetLineCoding(USBH_HandleTypeDef *phost, CDC_LineCodingTypeDef *linecoding);
static USBH_StatusTypeDef PLCOM_GetLineCoding(USBH_HandleTypeDef *phost, CDC_LineCodingTypeDef *linecoding);

static void PLCOM_ProcessNotification(USBH_HandleTypeDef *phost);
static void PLCOM_ProcessTransmission(USBH_HandleTypeDef *phost);
static void PLCOM_ProcessReception(USBH_HandleTypeDef *phost);

USBH_ClassTypeDef VENDOR_SERIAL_PLCOM_Class = {
    "VENDOR_SERIAL_PLCOM",
    0xFFU, /* VENDOR Class ID */
    USBH_PLCOM_InterfaceInit,
    USBH_PLCOM_InterfaceDeInit,
    USBH_PLCOM_ClassRequest,
    USBH_PLCOM_Process,
    USBH_PLCOM_SOFProcess,
    NULL,
};

bool USBH_PLCOM_IsDeviceType(USBH_HandleTypeDef *phost)
{
    if (!phost) {
        return false;
    }

    if (phost->device.CfgDesc.Itf_Desc[0].bInterfaceClass != VENDOR_SERIAL_PLCOM_Class.ClassCode) {
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

USBH_StatusTypeDef USBH_PLCOM_InterfaceInit(USBH_HandleTypeDef *phost)
{
    USBH_StatusTypeDef status;
    uint8_t interface;
    PLCOM_HandleTypeDef *PLCOM_Handle;

    USBH_UsrLog("PLCOM: USBH_PLCOM_InterfaceInit");

    if (!USBH_PLCOM_IsDeviceType(phost)) {
        USBH_UsrLog("PLCOM: Unrecognized device");
        return USBH_NOT_SUPPORTED;
    }

    interface = USBH_FindInterface(phost, 0xFFU, 0xFFU, 0xFFU);

    if (interface == 0xFFU || interface >= USBH_MAX_NUM_INTERFACES) {
        USBH_UsrLog("PLCOM: Cannot find the interface for Vendor Serial PLCOM class.");
        return USBH_FAIL;
    }

    status = USBH_SelectInterface(phost, interface);
    if (status != USBH_OK) {
        return USBH_FAIL;
    }

    phost->pActiveClass->pData = (PLCOM_HandleTypeDef *)USBH_malloc(sizeof(PLCOM_HandleTypeDef));
    PLCOM_Handle = (PLCOM_HandleTypeDef *)phost->pActiveClass->pData;

    if (!PLCOM_Handle) {
        USBH_ErrLog("PLCOM: Cannot allocate memory for PLCOM Handle");
        return USBH_FAIL;
    }

    USBH_memset(PLCOM_Handle, 0, sizeof(PLCOM_HandleTypeDef));

    switch (phost->device.DevDesc.bcdDevice) {
    case 0x0300:
        PLCOM_Handle->chiptype = TYPE_PL2303HX;
        /* or TA, that is HX with external crystal */
        break;
    case 0x0400:
        PLCOM_Handle->chiptype = TYPE_PL2303HXD;
        /* or EA, that is HXD with ESD protection */
        /* or RA, that has internal voltage level converter that works only up to 1Mbaud (!) */
        break;
    case 0x0500:
        PLCOM_Handle->chiptype = TYPE_PL2303HXD;
        /* in fact it's TB, that is HXD with external crystal */
        break;
    default:
        /* NOTE: I have no info about the bcdDevice for the base PL2303 (up to 1.2Mbaud,
           only fixed rates) and for PL2303SA (8-pin chip, up to 115200 baud */
        /* Determine the chip type.  This algorithm is taken from Linux. */
        if (phost->device.DevDesc.bDeviceClass == 0x02)
            PLCOM_Handle->chiptype = TYPE_PL2303;
        else if (phost->device.DevDesc.bMaxPacketSize == 0x40)
            PLCOM_Handle->chiptype = TYPE_PL2303HX;
        else
            PLCOM_Handle->chiptype = TYPE_PL2303;
        break;
    }

    /*
     * The new chip revision PL2303HXN is only compatible with the new
     * PLCOM_SET_REQUEST_PL2303HXN command. Issuing the old command
     * PLCOM_SET_REQUEST to the new chip raises an error. Thus, PL2303HX
     * and PL2303HXN can be distinguished by issuing an old-style request
     * (on a status register) to the new chip and checking the error.
     */
    if (PLCOM_Handle->chiptype == TYPE_PL2303HX) {
        uint8_t buf[4];
        phost->Control.setup.b.bmRequestType = UT_READ_VENDOR_DEVICE;
        phost->Control.setup.b.bRequest = PLCOM_SET_REQUEST;
        phost->Control.setup.b.wValue.w = PLCOM_STATUS_REG_PL2303HX;
        phost->Control.setup.b.wIndex.w = PLCOM_Handle->data_iface_no; // this isn't even initialized yet, it should be zero here
        phost->Control.setup.b.wLength.w = 1U;

        do {
            status = USBH_CtlReq(phost, buf, sizeof(buf));
        } while (status == USBH_BUSY);

        if (status != USBH_OK) {
            PLCOM_Handle->chiptype = TYPE_PL2303HXN;
        }
    }

    switch (PLCOM_Handle->chiptype) {
    case TYPE_PL2303:
        USBH_UsrLog("PLCOM: chiptype = 2303");
        break;
    case TYPE_PL2303HX:
        USBH_UsrLog("PLCOM: chiptype = 2303HX/TA");
        break;
    case TYPE_PL2303HXN:
        USBH_UsrLog("PLCOM: chiptype = 2303HXN");
        break;
    case TYPE_PL2303HXD:
        USBH_UsrLog("PLCOM: Chip type = 2303HXD/TB/RA/EA");
        break;
    default:
        USBH_UsrLog("PLCOM: chiptype = unknown %d", PLCOM_Handle->chiptype);
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

    PLCOM_Handle->ctrl_iface_no = interface;
    PLCOM_Handle->iface_index[1] = PLCOM_IFACE_INDEX;

    if (phost->device.CfgDesc.bNumInterfaces > PLCOM_SECOND_IFACE_INDEX) {
        /*
         * Note: This code may not work, we have no test devices to exercise
         * it, and the relevant device is likely excluded from this driver
         * anyways.
         */
        USBH_InterfaceDescTypeDef *pif = &phost->device.CfgDesc.Itf_Desc[PLCOM_SECOND_IFACE_INDEX];
        PLCOM_Handle->data_iface_no = pif->bInterfaceNumber;
        PLCOM_Handle->iface_index[0] = PLCOM_SECOND_IFACE_INDEX;
        interface = PLCOM_SECOND_IFACE_INDEX;
        status = USBH_SelectInterface(phost, interface);
        if (status != USBH_OK) {
            USBH_ErrLog("PLCOM: Cannot select secondary interface");
            return USBH_FAIL;
        }
    } else {
        PLCOM_Handle->data_iface_no = PLCOM_Handle->ctrl_iface_no;
        PLCOM_Handle->iface_index[0] = PLCOM_IFACE_INDEX;
    }

    /* Find the device endpoints */
    uint8_t max_endpoints = ((phost->device.CfgDesc.Itf_Desc[interface].bNumEndpoints <= USBH_MAX_NUM_ENDPOINTS) ?
        phost->device.CfgDesc.Itf_Desc[interface].bNumEndpoints : USBH_MAX_NUM_ENDPOINTS);

    for (uint8_t i = 0; i < max_endpoints; i++) {
        USBH_EpDescTypeDef *ep = &phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[i];

        if ((ep->bmAttributes & 0x03U) == USBH_EP_INTERRUPT) {
            PLCOM_Handle->endpoints.notif_ep = ep->bEndpointAddress;
            PLCOM_Handle->endpoints.notif_ep_size = ep->wMaxPacketSize;
        } else if ((ep->bmAttributes & 0x03U) == USBH_EP_BULK) {
            if (ep->bEndpointAddress & 0x80U) {
                PLCOM_Handle->endpoints.in_ep = ep->bEndpointAddress;
                PLCOM_Handle->endpoints.in_ep_size = ep->wMaxPacketSize;
            } else {
                PLCOM_Handle->endpoints.out_ep = ep->bEndpointAddress;
                PLCOM_Handle->endpoints.out_ep_size = ep->wMaxPacketSize;
            }
        }
    }

    if (PLCOM_Handle->endpoints.notif_ep == 0
        || PLCOM_Handle->endpoints.in_ep == 0
        || PLCOM_Handle->endpoints.out_ep == 0) {
        USBH_ErrLog("PLCOM: Cannot find all endpoints");
        return USBH_FAIL;
    }

    USBH_UsrLog("PLCOM: in_ep_size=%d, out_ep_size=%d, notif_ep_size=%d",
        PLCOM_Handle->endpoints.in_ep_size,
        PLCOM_Handle->endpoints.out_ep_size,
        PLCOM_Handle->endpoints.notif_ep_size);

    /* Open the notification pipe */
    PLCOM_Handle->endpoints.notif_pipe = USBH_AllocPipe(phost, PLCOM_Handle->endpoints.notif_ep);

    USBH_OpenPipe(phost, PLCOM_Handle->endpoints.notif_pipe, PLCOM_Handle->endpoints.notif_ep,
        phost->device.address, phost->device.speed, USB_EP_TYPE_INTR,
        PLCOM_Handle->endpoints.notif_ep_size);

    USBH_LL_SetToggle(phost, PLCOM_Handle->endpoints.notif_pipe, 0U);

    /* Open the bulk data I/O pipes */
    PLCOM_Handle->endpoints.out_pipe = USBH_AllocPipe(phost, PLCOM_Handle->endpoints.out_ep);
    PLCOM_Handle->endpoints.in_pipe = USBH_AllocPipe(phost, PLCOM_Handle->endpoints.in_ep);

    USBH_OpenPipe(phost, PLCOM_Handle->endpoints.out_pipe, PLCOM_Handle->endpoints.out_ep,
        phost->device.address, phost->device.speed, USB_EP_TYPE_BULK,
        PLCOM_Handle->endpoints.out_ep_size);
    USBH_OpenPipe(phost, PLCOM_Handle->endpoints.in_pipe, PLCOM_Handle->endpoints.in_ep,
        phost->device.address, phost->device.speed, USB_EP_TYPE_BULK,
        PLCOM_Handle->endpoints.in_ep_size);

    /*
     * Enable our custom tweak to prevent the HAL USB interrupt handler from
     * re-activating the IN channel every single time it is halted with a
     * NAK/URB_NOTREADY event. Without this, attempting to poll without
     * available data will make the interrupt repeatedly fire and cause
     * problems.
     */
    HCD_HandleTypeDef *hhcd = phost->pData;
    hhcd->hc[PLCOM_Handle->endpoints.in_pipe].no_reactivate_on_nak = 1;

    USBH_LL_SetToggle(phost, PLCOM_Handle->endpoints.out_pipe, 0U);
    USBH_LL_SetToggle(phost, PLCOM_Handle->endpoints.in_pipe, 0U);

    /* Allocate the I/O buffers */
    PLCOM_Handle->endpoints.notif_ep_buffer = (uint8_t *)USBH_malloc(PLCOM_Handle->endpoints.notif_ep_size);
    if (!PLCOM_Handle->endpoints.notif_ep_buffer) {
        USBH_ErrLog("PLCOM: Cannot allocate memory for PLCOM notification buffer");
        return USBH_FAIL;
    }
    USBH_memset(PLCOM_Handle->endpoints.notif_ep_buffer, 0, PLCOM_Handle->endpoints.notif_ep_size);

    PLCOM_Handle->endpoints.in_ep_buffer = (uint8_t *)USBH_malloc(PLCOM_Handle->endpoints.in_ep_size);
    if (!PLCOM_Handle->endpoints.in_ep_buffer) {
        USBH_ErrLog("PLCOM: Cannot allocate memory for PLCOM receive buffer");
        return USBH_FAIL;
    }
    PLCOM_Handle->rx_data = PLCOM_Handle->endpoints.in_ep_buffer;
    PLCOM_Handle->rx_data_length = PLCOM_Handle->endpoints.in_ep_size;
    USBH_memset(PLCOM_Handle->endpoints.in_ep_buffer, 0, PLCOM_Handle->endpoints.in_ep_size);

    PLCOM_Handle->endpoints.out_ep_buffer = (uint8_t *)USBH_malloc(PLCOM_Handle->endpoints.out_ep_size);
    if (!PLCOM_Handle->endpoints.out_ep_buffer) {
        USBH_ErrLog("PLCOM: Cannot allocate memory for PLCOM transmit buffer");
        return USBH_FAIL;
    }
    USBH_memset(PLCOM_Handle->endpoints.out_ep_buffer, 0, PLCOM_Handle->endpoints.out_ep_size);

    /* Startup reset sequence, if necessary for the chip type */
    if (PLCOM_Handle->chiptype != TYPE_PL2303HXN) {
        phost->Control.setup.b.bmRequestType = UT_WRITE_VENDOR_DEVICE;
        phost->Control.setup.b.bRequest = PLCOM_SET_REQUEST;
        phost->Control.setup.b.wValue.w = 0U;
        phost->Control.setup.b.wIndex.w = PLCOM_Handle->data_iface_no;
        phost->Control.setup.b.wLength.w = 0U;

        do {
            status = USBH_CtlReq(phost, NULL, 0);
        } while (status == USBH_BUSY);

        if (status != USBH_OK) {
            USBH_ErrLog("PLCOM: Initialization reset failed: %d", status);
            return USBH_FAIL;
        }
    }

    if (PLCOM_Handle->chiptype == TYPE_PL2303) {
        /* HX variants seem to lock up after a clear stall request. */
        /*
         * The FreeBSD code sets the stall flags on the in and out pipes
         * here. Have no idea exactly how to do this, or if it is necessary.
         * May just leave this code unwritten until test hardware is available.
         */
    } else if (PLCOM_Handle->chiptype == TYPE_PL2303HX
        || PLCOM_Handle->chiptype == TYPE_PL2303HXD) {
        /* Reset upstream data pipes */
        if (PLCOM_PL2303_Do(phost, UT_WRITE_VENDOR_DEVICE, PLCOM_SET_REQUEST, 8, 0, 0) != USBH_OK) {
            USBH_ErrLog("PLCOM: Could not reset upstream data pipes (8,0)");
            return USBH_FAIL;
        }
        if (PLCOM_PL2303_Do(phost, UT_WRITE_VENDOR_DEVICE, PLCOM_SET_REQUEST, 9, 0, 0) != USBH_OK) {
            USBH_ErrLog("PLCOM: Could not reset upstream data pipes (9,0)");
            return USBH_FAIL;
        }
    } else if (PLCOM_Handle->chiptype == TYPE_PL2303HXN) {
        /* Reset upstream data pipes */
        if (PLCOM_PL2303_Do(phost, UT_WRITE_VENDOR_DEVICE, PLCOM_SET_REQUEST_PL2303HXN, 0x07, 0x03, 0) != USBH_OK) {
            USBH_ErrLog("PLCOM: Could not reset upstream data pipes (7,3)");
            return USBH_FAIL;
        }
    }

    /* Final device initialization, if necessary for the chip type */
    if (PLCOM_Handle->chiptype != TYPE_PL2303HXN) {
        if (PLCOM_PL2303_Do(phost, UT_READ_VENDOR_DEVICE, PLCOM_SET_REQUEST, 0x8484, 0, 1) != USBH_OK
            || PLCOM_PL2303_Do(phost, UT_WRITE_VENDOR_DEVICE, PLCOM_SET_REQUEST, 0x0404, 0, 0) != USBH_OK
            || PLCOM_PL2303_Do(phost, UT_READ_VENDOR_DEVICE, PLCOM_SET_REQUEST, 0x8484, 0, 1) != USBH_OK
            || PLCOM_PL2303_Do(phost, UT_READ_VENDOR_DEVICE, PLCOM_SET_REQUEST, 0x8383, 0, 1) != USBH_OK
            || PLCOM_PL2303_Do(phost, UT_READ_VENDOR_DEVICE, PLCOM_SET_REQUEST, 0x8484, 0, 1) != USBH_OK
            || PLCOM_PL2303_Do(phost, UT_WRITE_VENDOR_DEVICE, PLCOM_SET_REQUEST, 0x0404, 1, 0) != USBH_OK
            || PLCOM_PL2303_Do(phost, UT_READ_VENDOR_DEVICE, PLCOM_SET_REQUEST, 0x8484, 0, 1) != USBH_OK
            || PLCOM_PL2303_Do(phost, UT_READ_VENDOR_DEVICE, PLCOM_SET_REQUEST, 0x8383, 0, 1) != USBH_OK
            || PLCOM_PL2303_Do(phost, UT_WRITE_VENDOR_DEVICE, PLCOM_SET_REQUEST, 0, 1, 0) != USBH_OK
            || PLCOM_PL2303_Do(phost, UT_WRITE_VENDOR_DEVICE, PLCOM_SET_REQUEST, 1, 0, 0) != USBH_OK) {
            USBH_ErrLog("PLCOM: Could not complete init sequence");
            return USBH_FAIL;
        }

        if (PLCOM_Handle->chiptype != TYPE_PL2303) {
            status = PLCOM_PL2303_Do(phost, UT_WRITE_VENDOR_DEVICE, PLCOM_SET_REQUEST, 2, 0x44, 0);
        } else {
            status = PLCOM_PL2303_Do(phost, UT_WRITE_VENDOR_DEVICE, PLCOM_SET_REQUEST, 2, 0x24, 0);
        }
        if (status != USBH_OK) {
            USBH_ErrLog("PLCOM: Could not complete final init request");
            return USBH_FAIL;
        }
    }

    PLCOM_Handle->state = USBH_PLCOM_STATE_IDLE;
    if (PLCOM_Handle->poll < PLCOM_MIN_POLL) {
        PLCOM_Handle->poll = PLCOM_MIN_POLL;
    }

    return USBH_OK;
}

USBH_StatusTypeDef USBH_PLCOM_InterfaceDeInit(USBH_HandleTypeDef *phost)
{
    PLCOM_HandleTypeDef *PLCOM_Handle = (PLCOM_HandleTypeDef *)phost->pActiveClass->pData;

    USBH_UsrLog("PLCOM: USBH_PLCOM_InterfaceDeInit");

    if (!phost->pActiveClass->pData) {
        return USBH_FAIL;
    }

    /* Close and free the notification pipe */
    if (PLCOM_Handle->endpoints.notif_pipe) {
        USBH_ClosePipe(phost, PLCOM_Handle->endpoints.notif_pipe);
        USBH_FreePipe(phost, PLCOM_Handle->endpoints.notif_pipe);
        PLCOM_Handle->endpoints.notif_pipe = 0U;
    }

    /* Close and free the I/O pipes */
    if (PLCOM_Handle->endpoints.in_pipe) {
        USBH_ClosePipe(phost, PLCOM_Handle->endpoints.in_pipe);
        USBH_FreePipe(phost, PLCOM_Handle->endpoints.in_pipe);
        PLCOM_Handle->endpoints.in_pipe = 0U;
    }
    if (PLCOM_Handle->endpoints.out_pipe) {
        USBH_ClosePipe(phost, PLCOM_Handle->endpoints.out_pipe);
        USBH_FreePipe(phost, PLCOM_Handle->endpoints.out_pipe);
        PLCOM_Handle->endpoints.out_pipe = 0U;
    }

    /* Free the I/O data buffers */
    if (PLCOM_Handle->endpoints.in_ep_buffer) {
        USBH_free(PLCOM_Handle->endpoints.in_ep_buffer);
        PLCOM_Handle->endpoints.in_ep_buffer = NULL;
        PLCOM_Handle->rx_data = NULL;
        PLCOM_Handle->rx_data_length = 0;
    }

    if (PLCOM_Handle->endpoints.out_ep_buffer) {
        USBH_free(PLCOM_Handle->endpoints.out_ep_buffer);
        PLCOM_Handle->endpoints.out_ep_buffer = NULL;
        PLCOM_Handle->tx_data = NULL;
        PLCOM_Handle->tx_data_length = 0;
    }

    if (PLCOM_Handle->endpoints.notif_ep_buffer) {
        USBH_free(PLCOM_Handle->endpoints.notif_ep_buffer);
        PLCOM_Handle->endpoints.notif_ep_buffer = NULL;
    }

    /* Free the device handle */
    if (phost->pActiveClass->pData) {
        USBH_free(phost->pActiveClass->pData);
        phost->pActiveClass->pData = 0U;
    }

    return USBH_OK;
}

USBH_StatusTypeDef PLCOM_PL2303_Do(USBH_HandleTypeDef *phost,
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

USBH_StatusTypeDef USBH_PLCOM_SetDtr(USBH_HandleTypeDef *phost, uint8_t onoff)
{
    PLCOM_HandleTypeDef *PLCOM_Handle = (PLCOM_HandleTypeDef *)phost->pActiveClass->pData;

    if (onoff) {
        PLCOM_Handle->line |= CDC_ACTIVATE_SIGNAL_DTR;
    } else {
        PLCOM_Handle->line &= ~CDC_ACTIVATE_SIGNAL_DTR;
    }

    phost->Control.setup.b.bmRequestType = UT_WRITE_CLASS_INTERFACE;
    phost->Control.setup.b.bRequest = CDC_SET_CONTROL_LINE_STATE;
    phost->Control.setup.b.wValue.w = PLCOM_Handle->line;
    phost->Control.setup.b.wIndex.w = PLCOM_Handle->data_iface_no;;
    phost->Control.setup.b.wLength.w = 0U;

    return USBH_CtlReq(phost, NULL, 0);
}

USBH_StatusTypeDef USBH_PLCOM_SetRts(USBH_HandleTypeDef *phost, uint8_t onoff)
{
    PLCOM_HandleTypeDef *PLCOM_Handle = (PLCOM_HandleTypeDef *)phost->pActiveClass->pData;

    if (onoff) {
        PLCOM_Handle->line |= CDC_ACTIVATE_CARRIER_SIGNAL_RTS;
    } else {
        PLCOM_Handle->line &= ~CDC_ACTIVATE_CARRIER_SIGNAL_RTS;
    }

    phost->Control.setup.b.bmRequestType = UT_WRITE_CLASS_INTERFACE;
    phost->Control.setup.b.bRequest = CDC_SET_CONTROL_LINE_STATE;
    phost->Control.setup.b.wValue.w = PLCOM_Handle->line;
    phost->Control.setup.b.wIndex.w = PLCOM_Handle->data_iface_no;;
    phost->Control.setup.b.wLength.w = 0U;

    return USBH_CtlReq(phost, NULL, 0);
}

USBH_StatusTypeDef USBH_PLCOM_SetBreak(USBH_HandleTypeDef *phost, uint8_t onoff)
{
    PLCOM_HandleTypeDef *PLCOM_Handle = (PLCOM_HandleTypeDef *)phost->pActiveClass->pData;
    uint16_t temp;

    temp = (onoff ? UCDC_BREAK_ON : UCDC_BREAK_OFF);

    phost->Control.setup.b.bmRequestType = UT_WRITE_CLASS_INTERFACE;
    phost->Control.setup.b.bRequest = CDC_SEND_BREAK;
    phost->Control.setup.b.wValue.w = temp;
    phost->Control.setup.b.wIndex.w = PLCOM_Handle->data_iface_no;;
    phost->Control.setup.b.wLength.w = 0U;

    return USBH_CtlReq(phost, NULL, 0);
}

USBH_StatusTypeDef USBH_PLCOM_SetRtsCts(USBH_HandleTypeDef *phost, uint8_t onoff)
{
    PLCOM_HandleTypeDef *PLCOM_Handle = (PLCOM_HandleTypeDef *)phost->pActiveClass->pData;

    phost->Control.setup.b.bmRequestType = UT_WRITE_VENDOR_DEVICE;
    if (onoff) {
        if (PLCOM_Handle->chiptype == TYPE_PL2303HXN) {
            phost->Control.setup.b.bRequest = PLCOM_SET_REQUEST_PL2303HXN;
            phost->Control.setup.b.wValue.w = PLCOM_CRTSCTS_REG_PL2303HXN;
            phost->Control.setup.b.wIndex.w = PLCOM_SET_CRTSCTS_PL2303HXN;
        } else {
            phost->Control.setup.b.bRequest = PLCOM_SET_REQUEST;
            phost->Control.setup.b.wValue.w = 0U;
            if (PLCOM_Handle->chiptype != TYPE_PL2303) {
                phost->Control.setup.b.wIndex.w = PLCOM_SET_CRTSCTS_PL2303X;
            } else {
                phost->Control.setup.b.wIndex.w = PLCOM_SET_CRTSCTS;
            }
        }
        phost->Control.setup.b.wLength.w = 0U;
    } else {
        if (PLCOM_Handle->chiptype == TYPE_PL2303HXN) {
            phost->Control.setup.b.bRequest = PLCOM_SET_REQUEST_PL2303HXN;
            phost->Control.setup.b.wValue.w = PLCOM_CRTSCTS_REG_PL2303HXN;
            phost->Control.setup.b.wIndex.w = PLCOM_CLEAR_CRTSCTS_PL2303HXN;
        } else {
            phost->Control.setup.b.bRequest = PLCOM_SET_REQUEST;
            phost->Control.setup.b.wValue.w = 0U;
            phost->Control.setup.b.wIndex.w = 0U;
        }
        phost->Control.setup.b.wLength.w = 0U;

    }
    return USBH_CtlReq(phost, NULL, 0);
}

USBH_StatusTypeDef USBH_PLCOM_SetLineCoding(USBH_HandleTypeDef *phost, CDC_LineCodingTypeDef *linecoding)
{
    USBH_StatusTypeDef status;
    PLCOM_HandleTypeDef *PLCOM_Handle = (PLCOM_HandleTypeDef *)phost->pActiveClass->pData;

    if (phost->gState == HOST_CLASS) {
        PLCOM_Handle->state = USBH_PLCOM_STATE_SET_LINE_CODING;
        PLCOM_Handle->user_linecoding = linecoding;

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
        status = PLCOM_SetLineCoding(phost, linecoding);
    } else {
        USBH_ErrLog("PLCOM: Invalid state for setting line coding: %d", phost->gState);
        status = USBH_FAIL;
    }

    return status;
}

USBH_StatusTypeDef PLCOM_SetLineCoding(USBH_HandleTypeDef *phost, CDC_LineCodingTypeDef *linecoding)
{
    phost->Control.setup.b.bmRequestType = UT_WRITE_CLASS_INTERFACE;
    phost->Control.setup.b.bRequest = CDC_SET_LINE_CODING;
    phost->Control.setup.b.wValue.w = 0U;
    phost->Control.setup.b.wIndex.w = 0U;
    phost->Control.setup.b.wLength.w = LINE_CODING_STRUCTURE_SIZE;

    return USBH_CtlReq(phost, linecoding->Array, LINE_CODING_STRUCTURE_SIZE);
}

USBH_StatusTypeDef USBH_PLCOM_GetLineCoding(USBH_HandleTypeDef *phost, CDC_LineCodingTypeDef *linecoding)
{
    PLCOM_HandleTypeDef *PLCOM_Handle = (PLCOM_HandleTypeDef *)phost->pActiveClass->pData;

    if ((phost->gState == HOST_CLASS) || (phost->gState == HOST_CLASS_REQUEST)) {
        *linecoding = PLCOM_Handle->linecoding;
        return USBH_OK;
    } else {
        return USBH_FAIL;
    }
}

USBH_StatusTypeDef PLCOM_GetLineCoding(USBH_HandleTypeDef *phost, CDC_LineCodingTypeDef *linecoding)
{
    phost->Control.setup.b.bmRequestType = UT_READ_CLASS_INTERFACE;
    phost->Control.setup.b.bRequest = CDC_GET_LINE_CODING;
    phost->Control.setup.b.wValue.w = 0U;
    phost->Control.setup.b.wIndex.w = 0U;
    phost->Control.setup.b.wLength.w = LINE_CODING_STRUCTURE_SIZE;

    return USBH_CtlReq(phost, linecoding->Array, LINE_CODING_STRUCTURE_SIZE);
}

USBH_StatusTypeDef USBH_PLCOM_ClassRequest(USBH_HandleTypeDef *phost)
{
    USBH_StatusTypeDef status;
    PLCOM_HandleTypeDef *PLCOM_Handle = (PLCOM_HandleTypeDef *)phost->pActiveClass->pData;

    status = PLCOM_GetLineCoding(phost, &PLCOM_Handle->linecoding);
    if (status == USBH_OK) {
        phost->pUser(phost, HOST_USER_CLASS_ACTIVE);
    } else if (status == USBH_NOT_SUPPORTED) {
        USBH_ErrLog("PLCOM: Unable to get device line coding");
    }

    return status;
}

USBH_StatusTypeDef USBH_PLCOM_Process(USBH_HandleTypeDef *phost)
{
    USBH_StatusTypeDef status = USBH_BUSY;
    USBH_StatusTypeDef req_status = USBH_OK;
    PLCOM_HandleTypeDef *PLCOM_Handle = (PLCOM_HandleTypeDef *)phost->pActiveClass->pData;

    switch (PLCOM_Handle->state) {
    case USBH_PLCOM_STATE_IDLE:
        status = USBH_OK;
        break;

    case USBH_PLCOM_STATE_SET_LINE_CODING:
      req_status = PLCOM_SetLineCoding(phost, PLCOM_Handle->user_linecoding);

      if (req_status == USBH_OK) {
          PLCOM_Handle->state = USBH_PLCOM_STATE_GET_LAST_LINE_CODING;
      } else {
          if (req_status != USBH_BUSY) {
              PLCOM_Handle->state = USBH_PLCOM_STATE_ERROR;
          }
      }
      break;

    case USBH_PLCOM_STATE_GET_LAST_LINE_CODING:
        req_status = PLCOM_GetLineCoding(phost, &(PLCOM_Handle->linecoding));

        if (req_status == USBH_OK) {
            PLCOM_Handle->state = USBH_PLCOM_STATE_IDLE;

            if ((PLCOM_Handle->linecoding.b.bCharFormat == PLCOM_Handle->user_linecoding->b.bCharFormat)
                && (PLCOM_Handle->linecoding.b.bDataBits == PLCOM_Handle->user_linecoding->b.bDataBits)
                && (PLCOM_Handle->linecoding.b.bParityType == PLCOM_Handle->user_linecoding->b.bParityType)
                && (PLCOM_Handle->linecoding.b.dwDTERate == PLCOM_Handle->user_linecoding->b.dwDTERate)) {
                USBH_PLCOM_LineCodingChanged(phost);
            }
        } else {
            if (req_status != USBH_BUSY) {
                PLCOM_Handle->state = USBH_PLCOM_STATE_ERROR;
            }
        }
        break;

    case USBH_PLCOM_STATE_TRANSFER_DATA:
        PLCOM_Handle->timer = phost->Timer;
        PLCOM_ProcessNotification(phost);
        PLCOM_ProcessTransmission(phost);
        PLCOM_ProcessReception(phost);
        if (PLCOM_Handle->data_rx_state == USBH_PLCOM_IDLE_DATA && PLCOM_Handle->data_tx_state == USBH_PLCOM_IDLE_DATA) {
            PLCOM_Handle->state = USBH_PLCOM_STATE_IDLE;
        }
        break;

    case USBH_PLCOM_STATE_ERROR:
        req_status = USBH_ClrFeature(phost, 0x00U);
        if (req_status == USBH_OK) {
            PLCOM_Handle->state = USBH_PLCOM_STATE_IDLE;
        }
        break;

    default:
        break;
    }

    return status;
}

USBH_StatusTypeDef USBH_PLCOM_SOFProcess(USBH_HandleTypeDef *phost)
{
    PLCOM_HandleTypeDef *PLCOM_Handle = (PLCOM_HandleTypeDef *)phost->pActiveClass->pData;

    if (PLCOM_Handle->state == USBH_PLCOM_STATE_IDLE) {
        if ((phost->Timer - PLCOM_Handle->timer) >= PLCOM_Handle->poll) {
            PLCOM_Handle->state = USBH_PLCOM_STATE_TRANSFER_DATA;
            if (PLCOM_Handle->data_notif_state == USBH_PLCOM_IDLE_DATA) {
                PLCOM_Handle->data_notif_state = USBH_PLCOM_NOTIFY_DATA;
            }
            if (PLCOM_Handle->data_rx_state == USBH_PLCOM_IDLE_DATA) {
                PLCOM_Handle->data_rx_state = USBH_PLCOM_RECEIVE_DATA;
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

USBH_StatusTypeDef USBH_PLCOM_Transmit(USBH_HandleTypeDef *phost, const uint8_t *pbuff, uint32_t length)
{
    USBH_StatusTypeDef status = USBH_BUSY;
    PLCOM_HandleTypeDef *PLCOM_Handle = (PLCOM_HandleTypeDef *)phost->pActiveClass->pData;

    if (length > PLCOM_Handle->endpoints.out_ep_size) {
        USBH_ErrLog("PLCOM: Transmit size too large: %lu > %d",
            length, PLCOM_Handle->endpoints.out_ep_size);
        return USBH_FAIL;
    }

    if ((PLCOM_Handle->state == USBH_PLCOM_STATE_IDLE) || (PLCOM_Handle->state == USBH_PLCOM_STATE_TRANSFER_DATA)) {
        USBH_memcpy(PLCOM_Handle->endpoints.out_ep_buffer, pbuff, length);
        PLCOM_Handle->tx_data = PLCOM_Handle->endpoints.out_ep_buffer;
        PLCOM_Handle->tx_data_length = length;
        PLCOM_Handle->state = USBH_PLCOM_STATE_TRANSFER_DATA;
        PLCOM_Handle->data_tx_state = USBH_PLCOM_SEND_DATA;
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

USBH_StatusTypeDef USBH_PLCOM_Stop(USBH_HandleTypeDef *phost)
{
    PLCOM_HandleTypeDef *PLCOM_Handle = (PLCOM_HandleTypeDef *)phost->pActiveClass->pData;

    if (phost->gState == HOST_CLASS) {
        USBH_UsrLog("PLCOM: USBH_PLCOM_Stop");
        PLCOM_Handle->state = USBH_PLCOM_STATE_IDLE;
        USBH_ClosePipe(phost, PLCOM_Handle->endpoints.notif_pipe);
        USBH_ClosePipe(phost, PLCOM_Handle->endpoints.in_pipe);
        USBH_ClosePipe(phost, PLCOM_Handle->endpoints.out_pipe);
    }
    return USBH_OK;
}

void PLCOM_ProcessNotification(USBH_HandleTypeDef *phost)
{
    PLCOM_HandleTypeDef *PLCOM_Handle = (PLCOM_HandleTypeDef *)phost->pActiveClass->pData;
    USBH_URBStateTypeDef URB_Status = USBH_URB_IDLE;
    uint32_t length;

    /*
     * Not entirely sure notification reads are working correctly, however
     * they are not important for basic functionality. Leaving this code
     * here as a placeholder for potentially fixing them in the future.
     */

    switch (PLCOM_Handle->data_notif_state) {
    case USBH_PLCOM_NOTIFY_DATA:
        USBH_InterruptReceiveData(phost,
            PLCOM_Handle->endpoints.notif_ep_buffer,
            PLCOM_Handle->endpoints.notif_ep_size,
            PLCOM_Handle->endpoints.notif_pipe);
        PLCOM_Handle->data_notif_state = USBH_PLCOM_NOTIFY_DATA_WAIT;
        break;
    case USBH_PLCOM_NOTIFY_DATA_WAIT:
        URB_Status = USBH_LL_GetURBState(phost, PLCOM_Handle->endpoints.notif_pipe);
        length = USBH_LL_GetLastXferSize(phost, PLCOM_Handle->endpoints.notif_pipe);

        if (URB_Status == USBH_URB_DONE || URB_Status == USBH_URB_IDLE) {
            if (length > PLCOM_STATE_INDEX) {
                uint8_t status = PLCOM_Handle->endpoints.notif_ep_buffer[PLCOM_STATE_INDEX];
                USBH_UsrLog("PLCOM: Notify status = 0x%02X", status);
            }
            PLCOM_Handle->data_notif_state = USBH_PLCOM_IDLE_DATA;
        } else if (URB_Status == USBH_URB_STALL) {
            if (USBH_ClrFeature(phost, PLCOM_Handle->endpoints.notif_ep) == USBH_OK) {
                PLCOM_Handle->data_notif_state = USBH_PLCOM_NOTIFY_DATA;
            }
        } else {
            PLCOM_Handle->data_notif_state = USBH_PLCOM_IDLE_DATA;
        }
        break;
    default:
        break;
    }
}

void PLCOM_ProcessTransmission(USBH_HandleTypeDef *phost)
{
    PLCOM_HandleTypeDef *PLCOM_Handle = (PLCOM_HandleTypeDef *)phost->pActiveClass->pData;
    USBH_URBStateTypeDef URB_Status = USBH_URB_IDLE;

    switch (PLCOM_Handle->data_tx_state) {
    case USBH_PLCOM_SEND_DATA:
        if (PLCOM_Handle->tx_data_length > PLCOM_Handle->endpoints.out_ep_size) {
            /*
             * Right now this case is not possible, because we allocate tx_data
             * and limit what can be sent based on out_ep_size
             */
            USBH_BulkSendData(phost,
                PLCOM_Handle->tx_data,
                PLCOM_Handle->endpoints.out_ep_size,
                PLCOM_Handle->endpoints.out_pipe,
                1U);
        } else {
            USBH_BulkSendData(phost,
                PLCOM_Handle->tx_data,
                (uint16_t)PLCOM_Handle->tx_data_length,
                PLCOM_Handle->endpoints.out_pipe,
                1U);
        }
        PLCOM_Handle->data_tx_state = USBH_PLCOM_SEND_DATA_WAIT;
        break;

    case USBH_PLCOM_SEND_DATA_WAIT:
        URB_Status = USBH_LL_GetURBState(phost, PLCOM_Handle->endpoints.out_pipe);

        /* Check if the transmission is complete */
        if (URB_Status == USBH_URB_DONE) {
            if (PLCOM_Handle->tx_data_length > PLCOM_Handle->endpoints.out_ep_size) {
                PLCOM_Handle->tx_data_length -= PLCOM_Handle->endpoints.out_ep_size;
                PLCOM_Handle->tx_data += PLCOM_Handle->endpoints.out_ep_size;
            } else {
                PLCOM_Handle->tx_data_length = 0U;
            }

            if (PLCOM_Handle->tx_data_length > 0U) {
                PLCOM_Handle->data_tx_state = USBH_PLCOM_SEND_DATA;
            } else {
                PLCOM_Handle->data_tx_state = USBH_PLCOM_IDLE_DATA;
                PLCOM_Handle->tx_data = NULL;
                PLCOM_Handle->tx_data_length = 0;
                USBH_PLCOM_TransmitCallback(phost);
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
            PLCOM_Handle->data_tx_state = USBH_PLCOM_SEND_DATA;

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

void PLCOM_ProcessReception(USBH_HandleTypeDef *phost)
{
    PLCOM_HandleTypeDef *PLCOM_Handle = (PLCOM_HandleTypeDef *)phost->pActiveClass->pData;
    USBH_URBStateTypeDef URB_Status = USBH_URB_IDLE;
    uint32_t length;

    switch (PLCOM_Handle->data_rx_state) {
    case USBH_PLCOM_RECEIVE_DATA:
        USBH_BulkReceiveData(phost,
            PLCOM_Handle->rx_data,
            PLCOM_Handle->endpoints.in_ep_size,
            PLCOM_Handle->endpoints.in_pipe);

        PLCOM_Handle->data_rx_state = USBH_PLCOM_RECEIVE_DATA_WAIT;
        break;

    case USBH_PLCOM_RECEIVE_DATA_WAIT:
        URB_Status = USBH_LL_GetURBState(phost, PLCOM_Handle->endpoints.in_pipe);

        if (URB_Status == USBH_URB_DONE) {
            length = USBH_LL_GetLastXferSize(phost, PLCOM_Handle->endpoints.in_pipe);

            if (((PLCOM_Handle->rx_data_length - length) > 0U) && (length > PLCOM_Handle->endpoints.in_ep_size)) {
                PLCOM_Handle->rx_data_length -= length;
                PLCOM_Handle->rx_data += length;
                PLCOM_Handle->data_rx_state = USBH_PLCOM_RECEIVE_DATA;
            } else {
                PLCOM_Handle->data_rx_state = USBH_PLCOM_IDLE_DATA;

                size_t recv_length;
                if (PLCOM_Handle->rx_data == PLCOM_Handle->endpoints.in_ep_buffer) {
                    recv_length = length;
                } else {
                    recv_length = PLCOM_Handle->rx_data - PLCOM_Handle->endpoints.in_ep_buffer;
                    PLCOM_Handle->rx_data = PLCOM_Handle->endpoints.in_ep_buffer;
                }

                if (recv_length > 0) {
                    USBH_PLCOM_ReceiveCallback(phost, PLCOM_Handle->rx_data, recv_length);
                }
                PLCOM_Handle->rx_data = PLCOM_Handle->endpoints.in_ep_buffer;
                PLCOM_Handle->rx_data_length = PLCOM_Handle->endpoints.in_ep_size;
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
            PLCOM_Handle->data_rx_state = USBH_PLCOM_IDLE_DATA;
        }
        break;

    default:
        break;
    }
}

__weak void USBH_PLCOM_TransmitCallback(USBH_HandleTypeDef *phost)
{
    /* Prevent unused argument(s) compilation warning */
    UNUSED(phost);
}

__weak void USBH_PLCOM_ReceiveCallback(USBH_HandleTypeDef *phost, uint8_t *data, size_t length)
{
    /* Prevent unused argument(s) compilation warning */
    UNUSED(phost);
}

__weak void USBH_PLCOM_LineCodingChanged(USBH_HandleTypeDef *phost)
{
    /* Prevent unused argument(s) compilation warning */
    UNUSED(phost);
}
