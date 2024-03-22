/*
 * Copyright (c) 2024, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * -------------------------------------------------------------------------
 *
 * This driver has been forked from the CherryUSB project to modify it as
 * necessary for integration purposes. Those modifications include enhancements
 * to the device matching logic and logging changes.
 */
#include "usbh_core.h"
#include "usbh_serial_cp210x.h"

#define LOG_TAG "usbh_serial_cp210x"
#include <elog.h>

#define DEV_FORMAT "/dev/ttyUSB%d"

USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t g_cp210x_buf[64];

#define CONFIG_USBHOST_MAX_CP210X_CLASS 4

/* CP210X_GET_PARTNUM values from hardware */
#define CP210X_PARTNUM_CP2101   1
#define CP210X_PARTNUM_CP2102   2
#define CP210X_PARTNUM_CP2103   3
#define CP210X_PARTNUM_CP2104   4
#define CP210X_PARTNUM_CP2105   5

static struct usbh_serial_cp210x g_cp210x_class[CONFIG_USBHOST_MAX_CP210X_CLASS];
static uint32_t g_devinuse = 0;

static int usbh_serial_cp210x_match(uint8_t class, uint8_t subclass, uint8_t protocol, uint16_t vid, uint16_t pid)
{
    /* CP210X devices can show up under several different PIDs */
    switch (pid) {
    case 0xEA60: /* CP2102 - SILABS USB UART */
    case 0xEA61: /* CP210X_2 - CP210x Serial */
    case 0xEA70: /* CP210X_3 - CP210x Serial */
    case 0xEA80: /* CP210X_4 - CP210x Serial */
        return 1;
    default:
        return 0;
    }
}

static struct usbh_serial_cp210x *usbh_serial_cp210x_class_alloc(void)
{
    int devno;

    for (devno = 0; devno < CONFIG_USBHOST_MAX_CP210X_CLASS; devno++) {
        if ((g_devinuse & (1 << devno)) == 0) {
            g_devinuse |= (1 << devno);
            memset(&g_cp210x_class[devno], 0, sizeof(struct usbh_serial_cp210x));
            g_cp210x_class[devno].minor = devno;
            return &g_cp210x_class[devno];
        }
    }
    return NULL;
}

static void usbh_serial_cp210x_class_free(struct usbh_serial_cp210x *cp210x_class)
{
    int devno = cp210x_class->minor;

    if (devno >= 0 && devno < 32) {
        g_devinuse &= ~(1 << devno);
    }
    memset(cp210x_class, 0, sizeof(struct usbh_serial_cp210x));
}

static int usbh_serial_cp210x_get_part_number(struct usbh_serial_cp210x *cp210x_class, uint8_t *value)
{
    struct usb_setup_packet *setup = cp210x_class->hport->setup;
    int ret;

    setup->bmRequestType = USB_REQUEST_DIR_IN | USB_REQUEST_VENDOR | USB_REQUEST_RECIPIENT_INTERFACE;
    setup->bRequest = CP210X_VENDOR_SPECIFIC;
    setup->wValue = CP210X_GET_PARTNUM;
    setup->wIndex = cp210x_class->intf;
    setup->wLength = 1;

    g_cp210x_buf[0] = 0;
    ret = usbh_control_transfer(cp210x_class->hport, setup, g_cp210x_buf);
    if (ret < 0) {
        return ret;
    }
    if (value) {
        *value = g_cp210x_buf[0];
    }
    return ret;
}

static int usbh_serial_cp210x_enable(struct usbh_serial_cp210x *cp210x_class)
{
    struct usb_setup_packet *setup = cp210x_class->hport->setup;

    setup->bmRequestType = USB_REQUEST_DIR_OUT | USB_REQUEST_VENDOR | USB_REQUEST_RECIPIENT_INTERFACE;
    setup->bRequest = CP210X_IFC_ENABLE;
    setup->wValue = 1;
    setup->wIndex = cp210x_class->intf;
    setup->wLength = 0;

    return usbh_control_transfer(cp210x_class->hport, setup, NULL);
}

static int usbh_serial_cp210x_set_flow(struct usbh_serial_cp210x *cp210x_class)
{
    struct usb_setup_packet *setup = cp210x_class->hport->setup;

    setup->bmRequestType = USB_REQUEST_DIR_OUT | USB_REQUEST_VENDOR | USB_REQUEST_RECIPIENT_INTERFACE;
    setup->bRequest = CP210X_SET_FLOW;
    setup->wValue = 0;
    setup->wIndex = cp210x_class->intf;
    setup->wLength = 16;

    memset(g_cp210x_buf, 0, 16);
    g_cp210x_buf[13] = 0x20;
    return usbh_control_transfer(cp210x_class->hport, setup, g_cp210x_buf);
}

static int usbh_serial_cp210x_set_chars(struct usbh_serial_cp210x *cp210x_class)
{
    struct usb_setup_packet *setup = cp210x_class->hport->setup;

    setup->bmRequestType = USB_REQUEST_DIR_OUT | USB_REQUEST_VENDOR | USB_REQUEST_RECIPIENT_INTERFACE;
    setup->bRequest = CP210X_SET_CHARS;
    setup->wValue = 0;
    setup->wIndex = cp210x_class->intf;
    setup->wLength = 6;

    memset(g_cp210x_buf, 0, 6);
    g_cp210x_buf[0] = 0x80;
    g_cp210x_buf[4] = 0x88;
    g_cp210x_buf[5] = 0x28;
    return usbh_control_transfer(cp210x_class->hport, setup, g_cp210x_buf);
}

static int usbh_serial_cp210x_set_baudrate(struct usbh_serial_cp210x *cp210x_class, uint32_t baudrate)
{
    struct usb_setup_packet *setup = cp210x_class->hport->setup;

    /* Make sure the baudrate setting does not exceed our part's maximum */
    uint32_t maxspeed;
    switch (cp210x_class->partnum) {
    case CP210X_PARTNUM_CP2104:
    case CP210X_PARTNUM_CP2105:
        maxspeed = 2000000;
        break;
    case CP210X_PARTNUM_CP2101:
    case CP210X_PARTNUM_CP2102:
    case CP210X_PARTNUM_CP2103:
    default:
        /*
         * Datasheet for cp2102 says 921600 max.  Testing shows that
         * 1228800 and 1843200 work fine.
         */
        maxspeed = 1843200;
        break;
    }
    if (baudrate <= 0 || baudrate > maxspeed) {
        return -USB_ERR_INVAL;
    }

    setup->bmRequestType = USB_REQUEST_DIR_OUT | USB_REQUEST_VENDOR | USB_REQUEST_RECIPIENT_INTERFACE;
    setup->bRequest = CP210X_SET_BAUDRATE;
    setup->wValue = 0;
    setup->wIndex = cp210x_class->intf;
    setup->wLength = 4;

    memcpy(g_cp210x_buf, (uint8_t *)&baudrate, 4);
    return usbh_control_transfer(cp210x_class->hport, setup, g_cp210x_buf);
}

static int usbh_serial_cp210x_set_data_format(struct usbh_serial_cp210x *cp210x_class, uint8_t databits, uint8_t parity, uint8_t stopbits)
{
    struct usb_setup_packet *setup = cp210x_class->hport->setup;
    uint16_t value;

    value = ((databits & 0x0F) << 8) | ((parity & 0x0f) << 4) | ((stopbits & 0x03) << 0);

    setup->bmRequestType = USB_REQUEST_DIR_OUT | USB_REQUEST_VENDOR | USB_REQUEST_RECIPIENT_INTERFACE;
    setup->bRequest = CP210X_SET_LINE_CTL;
    setup->wValue = value;
    setup->wIndex = cp210x_class->intf;
    setup->wLength = 0;

    return usbh_control_transfer(cp210x_class->hport, setup, NULL);
}

static int usbh_serial_cp210x_set_mhs(struct usbh_serial_cp210x *cp210x_class, uint8_t dtr, uint8_t rts, uint8_t dtr_mask, uint8_t rts_mask)
{
    struct usb_setup_packet *setup = cp210x_class->hport->setup;
    uint16_t value;

    value = ((dtr & 0x01) << 0) | ((rts & 0x01) << 1) | ((dtr_mask & 0x01) << 8) | ((rts_mask & 0x01) << 9);

    setup->bmRequestType = USB_REQUEST_DIR_OUT | USB_REQUEST_VENDOR | USB_REQUEST_RECIPIENT_INTERFACE;
    setup->bRequest = CP210X_SET_MHS;
    setup->wValue = value;
    setup->wIndex = cp210x_class->intf;
    setup->wLength = 0;

    return usbh_control_transfer(cp210x_class->hport, setup, NULL);
}

int usbh_serial_cp210x_set_line_coding(struct usbh_serial_cp210x *cp210x_class, struct cdc_line_coding *line_coding)
{
    memcpy((uint8_t *)&cp210x_class->line_coding, line_coding, sizeof(struct cdc_line_coding));
    usbh_serial_cp210x_set_baudrate(cp210x_class, line_coding->dwDTERate);
    return usbh_serial_cp210x_set_data_format(cp210x_class, line_coding->bDataBits, line_coding->bParityType,
        line_coding->bCharFormat);
}

int usbh_serial_cp210x_get_line_coding(struct usbh_serial_cp210x *cp210x_class, struct cdc_line_coding *line_coding)
{
    memcpy(line_coding, (uint8_t *)&cp210x_class->line_coding, sizeof(struct cdc_line_coding));
    return 0;
}

int usbh_serial_cp210x_set_line_state(struct usbh_serial_cp210x *cp210x_class, bool dtr, bool rts)
{
    return usbh_serial_cp210x_set_mhs(cp210x_class, dtr, rts, 1, 1);
}

static int usbh_serial_cp210x_connect(struct usbh_hubport *hport, uint8_t intf)
{
    struct usb_endpoint_descriptor *ep_desc;
    int ret = 0;

    struct usbh_serial_cp210x *cp210x_class = usbh_serial_cp210x_class_alloc();
    if (cp210x_class == NULL) {
        log_e("Fail to alloc cp210x_class");
        return -USB_ERR_NOMEM;
    }

    cp210x_class->hport = hport;
    cp210x_class->intf = intf;

    hport->config.intf[intf].priv = cp210x_class;

    do {
        ret = usbh_serial_cp210x_get_part_number(cp210x_class, &cp210x_class->partnum);
        if (ret < 0) { break; }

        switch (cp210x_class->partnum) {
        case CP210X_PARTNUM_CP2101:
            log_i("CP210X: partnum=CP2101");
            break;
        case CP210X_PARTNUM_CP2102:
            log_i("CP210X: partnum=CP2102");
            break;
        case CP210X_PARTNUM_CP2103:
            log_i("CP210X: partnum=CP2103");
            break;
        case CP210X_PARTNUM_CP2104:
            log_i("CP210X: partnum=CP2104");
            break;
        case CP210X_PARTNUM_CP2105:
            log_i("CP210X: partnum=CP2105");
            break;
        default:
            log_i("CP210X: partnum=<unknown>");
            break;
        }

        ret = usbh_serial_cp210x_enable(cp210x_class);
        if (ret < 0) { break; }

        ret = usbh_serial_cp210x_set_flow(cp210x_class);
        if (ret < 0) { break; }

        ret = usbh_serial_cp210x_set_chars(cp210x_class);
        if (ret < 0) { break; }
    } while (0);
    if (ret < 0) {
        log_e("Failed to initialize CP210X device: %d", ret);
        usbh_serial_cp210x_class_free(cp210x_class);
        return ret;
    }

    for (uint8_t i = 0; i < hport->config.intf[intf].altsetting[0].intf_desc.bNumEndpoints; i++) {
        ep_desc = &hport->config.intf[intf].altsetting[0].ep[i].ep_desc;

        if (ep_desc->bEndpointAddress & 0x80) {
            USBH_EP_INIT(cp210x_class->bulkin, ep_desc);
        } else {
            USBH_EP_INIT(cp210x_class->bulkout, ep_desc);
        }
    }

    snprintf(hport->config.intf[intf].devname, CONFIG_USBHOST_DEV_NAMELEN, DEV_FORMAT, cp210x_class->minor);

    log_i("Register CP210X Class:%s", hport->config.intf[intf].devname);

    usbh_serial_cp210x_run(cp210x_class);
    return ret;
}

static int usbh_serial_cp210x_disconnect(struct usbh_hubport *hport, uint8_t intf)
{
    int ret = 0;

    struct usbh_serial_cp210x *cp210x_class = (struct usbh_serial_cp210x *)hport->config.intf[intf].priv;

    if (cp210x_class) {
        if (cp210x_class->bulkin) {
            usbh_kill_urb(&cp210x_class->bulkin_urb);
        }

        if (cp210x_class->bulkout) {
            usbh_kill_urb(&cp210x_class->bulkout_urb);
        }

        if (hport->config.intf[intf].devname[0] != '\0') {
            log_i("Unregister CP210X Class:%s", hport->config.intf[intf].devname);
            usbh_serial_cp210x_stop(cp210x_class);
        }

        usbh_serial_cp210x_class_free(cp210x_class);
    }

    return ret;
}

int usbh_serial_cp210x_bulk_in_transfer(struct usbh_serial_cp210x *cp210x_class, uint8_t *buffer, uint32_t buflen, uint32_t timeout)
{
    int ret;
    struct usbh_urb *urb = &cp210x_class->bulkin_urb;

    usbh_bulk_urb_fill(urb, cp210x_class->hport, cp210x_class->bulkin, buffer, buflen, timeout, NULL, NULL);
    ret = usbh_submit_urb(urb);
    if (ret == 0) {
        ret = urb->actual_length;
    }
    return ret;
}

int usbh_serial_cp210x_bulk_out_transfer(struct usbh_serial_cp210x *cp210x_class, uint8_t *buffer, uint32_t buflen, uint32_t timeout)
{
    int ret;
    struct usbh_urb *urb = &cp210x_class->bulkout_urb;

    usbh_bulk_urb_fill(urb, cp210x_class->hport, cp210x_class->bulkout, buffer, buflen, timeout, NULL, NULL);
    ret = usbh_submit_urb(urb);
    if (ret == 0) {
        ret = urb->actual_length;
    }
    return ret;
}

__WEAK void usbh_serial_cp210x_run(struct usbh_serial_cp210x *cp210x_class)
{
}

__WEAK void usbh_serial_cp210x_stop(struct usbh_serial_cp210x *cp210x_class)
{
}

const struct usbh_class_driver serial_cp210x_class_driver = {
    .driver_name = "cp210x",
    .connect = usbh_serial_cp210x_connect,
    .disconnect = usbh_serial_cp210x_disconnect,
    .match = usbh_serial_cp210x_match
};

CLASS_INFO_DEFINE const struct usbh_class_info serial_cp210x_class_info = {
    .match_flags = USB_CLASS_MATCH_VENDOR | USB_CLASS_MATCH_INTF_CLASS | USB_CLASS_MATCH_CUSTOM_FUNC,
    .class = 0xff,
    .subclass = 0xff,
    .protocol = 0xff,
    .vid = 0x10C4,
    .pid = 0x00,
    .class_driver = &serial_cp210x_class_driver
};