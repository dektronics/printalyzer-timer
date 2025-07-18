#include "usbh_core.h"
#include "usbh_serial_pl2303.h"

#define LOG_TAG "usbh_serial_pl2303"
#include <elog.h>

#define DEV_FORMAT "/dev/ttyUSB%d"

#define PL2303_SET_REQUEST              0x01
#define PL2303_SET_REQUEST_PL2303HXN    0x80
#define PL2303_SET_CRTSCTS              0x41
#define PL2303_SET_CRTSCTS_PL2303X      0x61
#define PL2303_SET_CRTSCTS_PL2303HXN    0xFA
#define PL2303_CLEAR_CRTSCTS_PL2303HXN  0xFF
#define PL2303_CRTSCTS_REG_PL2303HXN    0x0A
#define PL2303_STATUS_REG_PL2303HX      0x8080

#define PL2303_STATE_INDEX 8

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

#define UT_WRITE_CLASS_INTERFACE (USB_REQUEST_DIR_OUT | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_INTERFACE)
#define UT_READ_CLASS_INTERFACE  (USB_REQUEST_DIR_IN | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_INTERFACE)
#define UT_WRITE_VENDOR_DEVICE   (USB_REQUEST_DIR_OUT | USB_REQUEST_VENDOR | USB_REQUEST_RECIPIENT_DEVICE)
#define UT_READ_VENDOR_DEVICE    (USB_REQUEST_DIR_IN | USB_REQUEST_VENDOR | USB_REQUEST_RECIPIENT_DEVICE)

static int usbh_serial_pl2303_set_line_coding(struct usbh_serial_class *serial_class, struct cdc_line_coding *line_coding);
static int usbh_serial_pl2303_get_line_coding(struct usbh_serial_class *serial_class, struct cdc_line_coding *line_coding);
static int usbh_serial_pl2303_set_line_state(struct usbh_serial_class *serial_class, bool dtr, bool rts);

static struct usbh_serial_class_interface const vtable = {
    .set_line_coding = usbh_serial_pl2303_set_line_coding,
    .get_line_coding = usbh_serial_pl2303_get_line_coding,
    .set_line_state = usbh_serial_pl2303_set_line_state,
    .bulk_in_transfer = NULL, /* default implementation */
    .bulk_out_transfer = NULL /* default implementation */
};

#define HPORT(x) (x->base.hport)
#define SETUP_PACKET(x) (x->base.hport->setup)

static struct usbh_serial_pl2303 *usbh_serial_pl2303_class_alloc(void)
{
    struct usbh_serial_pl2303 *pl2303_class = pvPortMalloc(sizeof(struct usbh_serial_pl2303));
    if (pl2303_class) {
        memset(pl2303_class, 0, sizeof(struct usbh_serial_pl2303));
        pl2303_class->base.vtable = &vtable;
    }
    return pl2303_class;
}

static void usbh_serial_pl2303_class_free(struct usbh_serial_pl2303 *pl2303_class)
{
    vPortFree(pl2303_class);
}

static int usbh_serial_pl2303_do(struct usbh_serial_pl2303 *pl2303_class,
    uint8_t req_type, uint8_t request, uint16_t value, uint16_t index,
    uint16_t length)
{
    /*
     * This is just a simple wrapper for a basic control request, since the
     * initialization code needs to do a lot of nondescript operations of
     * this type.
     * Requests with more obvious naming and typing information will have
     * their own dedicated functions.
     */
    struct usb_setup_packet *setup;

    if (!pl2303_class || !pl2303_class->base.hport) {
        return -USB_ERR_INVAL;
    }
    setup = SETUP_PACKET(pl2303_class);

    setup->bmRequestType = req_type;
    setup->bRequest = request;
    setup->wValue = value;
    setup->wIndex = index;
    setup->wLength = length;

    return usbh_control_transfer(HPORT(pl2303_class), setup, pl2303_class->control_buf);
}

int usbh_serial_pl2303_set_line_coding(struct usbh_serial_class *serial_class, struct cdc_line_coding *line_coding)
{
    struct usbh_serial_pl2303 *pl2303_class = (struct usbh_serial_pl2303 *)serial_class;
    struct usb_setup_packet *setup;

    if (!pl2303_class || !pl2303_class->base.hport) {
        return -USB_ERR_INVAL;
    }
    setup = serial_class->hport->setup;

    memcpy((uint8_t *)&serial_class->line_coding, line_coding, sizeof(struct cdc_line_coding));

    setup->bmRequestType = UT_WRITE_CLASS_INTERFACE;
    setup->bRequest = CDC_REQUEST_SET_LINE_CODING;
    setup->wValue = 0;
    setup->wIndex = 0;
    setup->wLength = sizeof(struct cdc_line_coding);

    memcpy(pl2303_class->control_buf, (uint8_t *)line_coding, sizeof(struct cdc_line_coding));
    return usbh_control_transfer(serial_class->hport, setup, pl2303_class->control_buf);
}

int usbh_serial_pl2303_get_line_coding(struct usbh_serial_class *serial_class, struct cdc_line_coding *line_coding)
{
    memcpy(line_coding, (uint8_t *)&serial_class->line_coding, sizeof(struct cdc_line_coding));
    return 0;
}

int usbh_serial_pl2303_set_line_state(struct usbh_serial_class *serial_class, bool dtr, bool rts)
{
    struct usb_setup_packet *setup;

    if (!serial_class || !serial_class->hport) {
        return -USB_ERR_INVAL;
    }
    setup = serial_class->hport->setup;

    setup->bmRequestType = UT_WRITE_CLASS_INTERFACE;
    setup->bRequest = CDC_REQUEST_SET_CONTROL_LINE_STATE;
    setup->wValue = (dtr ? SET_CONTROL_LINE_STATE_DTR : 0) | (rts ? SET_CONTROL_LINE_STATE_RTS : 0);
    setup->wIndex = 0;
    setup->wLength = 0;

    return usbh_control_transfer(serial_class->hport, setup, NULL);
}

static int usbh_serial_pl2303_get_chiptype(struct usbh_serial_pl2303 *pl2303_class)
{
    int ret = 0;

    switch (HPORT(pl2303_class)->device_desc.bcdDevice) {
    case 0x0300:
        pl2303_class->chiptype = USBH_PL2303_TYPE_PL2303HX;
        /* or TA, that is HX with external crystal */
        break;
    case 0x0400:
        pl2303_class->chiptype = USBH_PL2303_TYPE_PL2303HXD;
        /* or EA, that is HXD with ESD protection */
        /* or RA, that has internal voltage level converter that works only up to 1Mbaud (!) */
        break;
    case 0x0500:
        pl2303_class->chiptype = USBH_PL2303_TYPE_PL2303HXD;
        /* in fact it's TB, that is HXD with external crystal */
        break;
    default:
        /* NOTE: I have no info about the bcdDevice for the base PL2303 (up to 1.2Mbaud,
           only fixed rates) and for PL2303SA (8-pin chip, up to 115200 baud */
        /* Determine the chip type.  This algorithm is taken from Linux. */
        if (HPORT(pl2303_class)->device_desc.bDeviceClass == 0x02) {
            pl2303_class->chiptype = USBH_PL2303_TYPE_PL2303;
        } else if (HPORT(pl2303_class)->device_desc.bMaxPacketSize0 == 0x40) {
            pl2303_class->chiptype = USBH_PL2303_TYPE_PL2303HX;
        } else {
            pl2303_class->chiptype = USBH_PL2303_TYPE_PL2303;
        }
        break;
    }

    /*
     * The new chip revision PL2303HXN is only compatible with the new
     * PLCOM_SET_REQUEST_PL2303HXN command. Issuing the old command
     * PLCOM_SET_REQUEST to the new chip raises an error. Thus, PL2303HX
     * and PL2303HXN can be distinguished by issuing an old-style request
     * (on a status register) to the new chip and checking the error.
     */
    if (pl2303_class->chiptype == USBH_PL2303_TYPE_PL2303HX) {
        struct usb_setup_packet *setup = SETUP_PACKET(pl2303_class);

        setup->bmRequestType = UT_READ_VENDOR_DEVICE;
        setup->bRequest = PL2303_SET_REQUEST;
        setup->wValue = PL2303_STATUS_REG_PL2303HX;
        setup->wIndex = 0;
        setup->wLength = 1;

        ret = usbh_control_transfer(HPORT(pl2303_class), setup, pl2303_class->control_buf);
        if (ret == -USB_ERR_STALL) {
            pl2303_class->chiptype = USBH_PL2303_TYPE_PL2303HXN;
            ret = 0;
        } else if (ret < 0) {
            log_w("Error checking chip type: %d", ret);
            return ret;
        }
    }

    switch (pl2303_class->chiptype) {
    case USBH_PL2303_TYPE_PL2303:
        log_d("chiptype = 2303");
        break;
    case USBH_PL2303_TYPE_PL2303HX:
        log_d("chiptype = 2303HX/TA");
        break;
    case USBH_PL2303_TYPE_PL2303HXN:
        log_d("chiptype = 2303HXN");
        break;
    case USBH_PL2303_TYPE_PL2303HXD:
        log_d("chiptype = 2303HXD/TB/RA/EA");
        break;
    default:
        log_d("chiptype = [%d]", pl2303_class->chiptype);
        break;
    }

    return ret;
}

static int usbh_serial_pl2303_connect(struct usbh_hubport *hport, uint8_t intf)
{
    struct usb_endpoint_descriptor *ep_desc;
    uint8_t devnum = 0;
    int ret = 0;

    if (!usbh_serial_increment_count(&devnum)) {
        log_w("Too many serial devices attached");
        return -USB_ERR_NODEV;
    }

    struct usbh_serial_pl2303 *pl2303_class = usbh_serial_pl2303_class_alloc();
    if (pl2303_class == NULL) {
        log_e("Fail to alloc pl2303_class");
        usbh_serial_decrement_count(devnum);
        return -USB_ERR_NOMEM;
    }

    HPORT(pl2303_class) = hport;
    pl2303_class->base.intf = intf;
    pl2303_class->base.devnum = devnum;

    hport->config.intf[intf].priv = pl2303_class;

    do {
        ret = usbh_serial_pl2303_get_chiptype(pl2303_class);
        if (ret < 0) { break; }

        /* Startup reset sequence, if necessary for the chip type */
        if (pl2303_class->chiptype != USBH_PL2303_TYPE_PL2303HXN) {
            struct usb_setup_packet *setup = SETUP_PACKET(pl2303_class);

            setup->bmRequestType = UT_WRITE_VENDOR_DEVICE;
            setup->bRequest = PL2303_SET_REQUEST;
            setup->wValue = 0;
            setup->wIndex = pl2303_class->base.intf;
            setup->wLength = 0;

            ret = usbh_control_transfer(HPORT(pl2303_class), setup, pl2303_class->control_buf);
            if (ret < 0) {
                log_w("Initialization reset failed: %d", ret);
                break;
            }
        }

        if (pl2303_class->chiptype == USBH_PL2303_TYPE_PL2303) {
            /* HX variants seem to lock up after a clear stall request. */
            /*
             * The FreeBSD code sets the stall flags on the in and out pipes
             * here. Have no idea exactly how to do this, or if it is necessary.
             * May just leave this code unwritten until test hardware is available.
             */
        } else if (pl2303_class->chiptype == USBH_PL2303_TYPE_PL2303HX
                   || pl2303_class->chiptype == USBH_PL2303_TYPE_PL2303HXD) {
            /* Reset upstream data pipes */
            ret = usbh_serial_pl2303_do(pl2303_class, UT_WRITE_VENDOR_DEVICE, PL2303_SET_REQUEST, 8, 0, 0);
            if (ret < 0) {
                log_w("Could not reset upstream data pipes (8,0): %d", ret);
                break;
            }
            ret = usbh_serial_pl2303_do(pl2303_class, UT_WRITE_VENDOR_DEVICE, PL2303_SET_REQUEST, 9, 0, 0);
            if (ret < 0) {
                log_w("Could not reset upstream data pipes (9,0): %d", ret);
                break;
            }
        } else if (pl2303_class->chiptype == USBH_PL2303_TYPE_PL2303HXN) {
            /* Reset upstream data pipes */
            ret = usbh_serial_pl2303_do(pl2303_class, UT_WRITE_VENDOR_DEVICE, PL2303_SET_REQUEST_PL2303HXN, 0x07, 0x03, 0);
            if (ret < 0) {
                log_w("Could not reset upstream data pipes (7,3): %d", ret);
                break;
            }
        }

        /* Final device initialization, if necessary for the chip type */
        if (pl2303_class->chiptype != USBH_PL2303_TYPE_PL2303HXN) {
            if (usbh_serial_pl2303_do(pl2303_class, UT_READ_VENDOR_DEVICE, PL2303_SET_REQUEST, 0x8484, 0, 1) < 0
                || usbh_serial_pl2303_do(pl2303_class, UT_WRITE_VENDOR_DEVICE, PL2303_SET_REQUEST, 0x0404, 0, 0) < 0
                || usbh_serial_pl2303_do(pl2303_class, UT_READ_VENDOR_DEVICE, PL2303_SET_REQUEST, 0x8484, 0, 1) < 0
                || usbh_serial_pl2303_do(pl2303_class, UT_READ_VENDOR_DEVICE, PL2303_SET_REQUEST, 0x8383, 0, 1) < 0
                || usbh_serial_pl2303_do(pl2303_class, UT_READ_VENDOR_DEVICE, PL2303_SET_REQUEST, 0x8484, 0, 1) < 0
                || usbh_serial_pl2303_do(pl2303_class, UT_WRITE_VENDOR_DEVICE, PL2303_SET_REQUEST, 0x0404, 1, 0) < 0
                || usbh_serial_pl2303_do(pl2303_class, UT_READ_VENDOR_DEVICE, PL2303_SET_REQUEST, 0x8484, 0, 1) < 0
                || usbh_serial_pl2303_do(pl2303_class, UT_READ_VENDOR_DEVICE, PL2303_SET_REQUEST, 0x8383, 0, 1) < 0
                || usbh_serial_pl2303_do(pl2303_class, UT_WRITE_VENDOR_DEVICE, PL2303_SET_REQUEST, 0, 1, 0) < 0
                || usbh_serial_pl2303_do(pl2303_class, UT_WRITE_VENDOR_DEVICE, PL2303_SET_REQUEST, 1, 0, 0) < 0) {
                log_w("Could not complete init sequence");
                ret = -USB_ERR_INVAL;
                break;
            }

            if (pl2303_class->chiptype != USBH_PL2303_TYPE_PL2303) {
                ret = usbh_serial_pl2303_do(pl2303_class, UT_WRITE_VENDOR_DEVICE, PL2303_SET_REQUEST, 2, 0x44, 0);
            } else {
                ret = usbh_serial_pl2303_do(pl2303_class, UT_WRITE_VENDOR_DEVICE, PL2303_SET_REQUEST, 2, 0x24, 0);
            }
            if (ret < 0) {
                log_w("Could not complete final init request: %d", ret);
                break;
            }
        }
    } while (0);
    if (ret < 0) {
        log_e("Failed to initialize PL2303 device: %d", ret);
        usbh_serial_pl2303_class_free(pl2303_class);
        return ret;
    }

    for (uint8_t i = 0; i < hport->config.intf[intf].altsetting[0].intf_desc.bNumEndpoints; i++) {
        ep_desc = &hport->config.intf[intf].altsetting[0].ep[i].ep_desc;
        if (USB_GET_ENDPOINT_TYPE(ep_desc->bmAttributes) == USB_ENDPOINT_TYPE_INTERRUPT) {
            continue;
        } else {
            if (ep_desc->bEndpointAddress & 0x80) {
                USBH_EP_INIT(pl2303_class->base.bulkin, ep_desc);
            } else {
                USBH_EP_INIT(pl2303_class->base.bulkout, ep_desc);
            }
        }
    }

    snprintf(hport->config.intf[intf].devname, CONFIG_USBHOST_DEV_NAMELEN, DEV_FORMAT, pl2303_class->base.devnum);

    log_i("Register PL2303 Class:%s", hport->config.intf[intf].devname);

    usbh_serial_run((struct usbh_serial_class *)pl2303_class);
    return ret;
}

static int usbh_serial_pl2303_disconnect(struct usbh_hubport *hport, uint8_t intf)
{
    int ret = 0;

    struct usbh_serial_pl2303 *pl2303_class = (struct usbh_serial_pl2303 *)hport->config.intf[intf].priv;

    if (pl2303_class) {
        if (pl2303_class->base.bulkin) {
            usbh_kill_urb(&pl2303_class->base.bulkin_urb);
        }

        if (pl2303_class->base.bulkout) {
            usbh_kill_urb(&pl2303_class->base.bulkout_urb);
        }

        if (hport->config.intf[intf].devname[0] != '\0') {
            usb_osal_thread_schedule_other();
            log_i("Unregister PL2303 Class:%s", hport->config.intf[intf].devname);
            usbh_serial_stop((struct usbh_serial_class *)pl2303_class);
        }

        usbh_serial_decrement_count(pl2303_class->base.devnum);
        usbh_serial_pl2303_class_free(pl2303_class);
    }

    return ret;
}

static const uint16_t serial_pl2303_id_table[][2] = {
    { 0x067B, 0x2303 }, /* PL2303 Serial (ATEN/IOGEAR UC232A) */
    { 0x067B, 0x23A3 }, /* PL2303HXN Serial, type GC */
    { 0x067B, 0x23B3 }, /* PL2303HXN Serial, type GB */
    { 0x067B, 0x23C3 }, /* PL2303HXN Serial, type GT */
    { 0x067B, 0x23D3 }, /* PL2303HXN Serial, type GL */
    { 0x067B, 0x23E3 }, /* PL2303HXN Serial, type GE */
    { 0x067B, 0x23F3 }, /* PL2303HXN Serial, type GS */
    { 0, 0 },
};

const struct usbh_class_driver serial_pl2303_class_driver = {
    .driver_name = "pl2303",
    .connect = usbh_serial_pl2303_connect,
    .disconnect = usbh_serial_pl2303_disconnect
};

CLASS_INFO_DEFINE const struct usbh_class_info serial_pl2303_class_info = {
    .match_flags = USB_CLASS_MATCH_VID_PID | USB_CLASS_MATCH_INTF_CLASS,
    .bInterfaceClass = 0xff,
    .bInterfaceSubClass = 0x00,
    .bInterfaceProtocol = 0x00,
    .id_table = serial_pl2303_id_table,
    .class_driver = &serial_pl2303_class_driver
};
