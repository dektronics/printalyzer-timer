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
#include "usbh_serial_ch34x.h"

#define LOG_TAG "usbh_serial_ch34x"
#include <elog.h>

#define DEV_FORMAT "/dev/ttyUSB%d"

static int usbh_serial_ch34x_set_line_coding(struct usbh_serial_class *serial_class, struct cdc_line_coding *line_coding);
static int usbh_serial_ch34x_get_line_coding(struct usbh_serial_class *serial_class, struct cdc_line_coding *line_coding);
static int usbh_serial_ch34x_set_line_state(struct usbh_serial_class *serial_class, bool dtr, bool rts);

static struct usbh_serial_class_interface const vtable = {
    .set_line_coding = usbh_serial_ch34x_set_line_coding,
    .get_line_coding = usbh_serial_ch34x_get_line_coding,
    .set_line_state = usbh_serial_ch34x_set_line_state,
    .bulk_in_transfer = NULL, /* default implementation */
    .bulk_out_transfer = NULL /* default implementation */
};

#define HPORT(x) (x->base.hport)
#define SETUP_PACKET(x) (x->base.hport->setup)

static struct usbh_serial_ch34x *usbh_serial_ch34x_class_alloc(void)
{
    struct usbh_serial_ch34x *ch34x_class = pvPortMalloc(sizeof(struct usbh_serial_ch34x));
    if (ch34x_class) {
        memset(ch34x_class, 0, sizeof(struct usbh_serial_ch34x));
        ch34x_class->base.vtable = &vtable;
    }
    return ch34x_class;
}

static void usbh_serial_ch34x_class_free(struct usbh_serial_ch34x *ch34x_class)
{
    vPortFree(ch34x_class);
}

static int usbh_serial_ch34x_get_baudrate_div(uint32_t baudrate, uint8_t *factor, uint8_t *divisor)
{
    uint8_t a;
    uint8_t b;
    uint32_t c;

    switch (baudrate) {
        case 921600:
            a = 0xf3;
            b = 7;
            break;

        case 307200:
            a = 0xd9;
            b = 7;
            break;

        default:
            if (baudrate > 6000000 / 255) {
                b = 3;
                c = 6000000;
            } else if (baudrate > 750000 / 255) {
                b = 2;
                c = 750000;
            } else if (baudrate > 93750 / 255) {
                b = 1;
                c = 93750;
            } else {
                b = 0;
                c = 11719;
            }
            a = (uint8_t)(c / baudrate);
            if (a == 0 || a == 0xFF) {
                return -USB_ERR_INVAL;
            }
            if ((c / a - baudrate) > (baudrate - c / (a + 1))) {
                a++;
            }
            a = (uint8_t)(256 - a);
            break;
    }

    *factor = a;
    *divisor = b;

    return 0;
}

static int usbh_serial_ch34x_get_version(struct usbh_serial_ch34x *ch34x_class)
{
    struct usb_setup_packet *setup;
    int ret;

    if (!ch34x_class || !ch34x_class->base.hport) {
        return -USB_ERR_INVAL;
    }
    setup = SETUP_PACKET(ch34x_class);
    setup->bmRequestType = USB_REQUEST_DIR_IN | USB_REQUEST_VENDOR | USB_REQUEST_RECIPIENT_DEVICE;
    setup->bRequest = CH34X_READ_VERSION;
    setup->wValue = 0;
    setup->wIndex = 0;
    setup->wLength = 2;

    ret = usbh_control_transfer(HPORT(ch34x_class), setup, ch34x_class->control_buf);
    if (ret < 0) {
        return ret;
    }

    log_i("Ch34x chip version %02x:%02x", ch34x_class->control_buf[0], ch34x_class->control_buf[1]);
    return ret;
}

static int usbh_serial_ch34x_flow_ctrl(struct usbh_serial_ch34x *ch34x_class)
{
    struct usb_setup_packet *setup;

    if (!ch34x_class || !ch34x_class->base.hport) {
        return -USB_ERR_INVAL;
    }
    setup = SETUP_PACKET(ch34x_class);

    setup->bmRequestType = USB_REQUEST_DIR_OUT | USB_REQUEST_VENDOR | USB_REQUEST_RECIPIENT_DEVICE;
    setup->bRequest = CH34X_WRITE_REG;
    setup->wValue = 0x2727;
    setup->wIndex = 0;
    setup->wLength = 0;

    return usbh_control_transfer(HPORT(ch34x_class), setup, NULL);
}

int usbh_serial_ch34x_set_line_coding(struct usbh_serial_class *serial_class, struct cdc_line_coding *line_coding)
{
    struct usb_setup_packet *setup;
    uint16_t reg_value = 0;
    uint16_t value = 0;
    uint8_t factor = 0;
    uint8_t divisor = 0;

    if (!serial_class || !serial_class->hport) {
        return -USB_ERR_INVAL;
    }
    setup = serial_class->hport->setup;
    memcpy((uint8_t *)&serial_class->line_coding, line_coding, sizeof(struct cdc_line_coding));

    /* refer to https://github.com/WCHSoftGroup/ch341ser_linux/blob/main/driver/ch341.c */

    switch (line_coding->bParityType) {
        case 0:
            break;
        case 1:
            reg_value |= CH341_L_PO;
            break;
        case 2:
            reg_value |= CH341_L_PE;
            break;
        case 3:
            reg_value |= CH341_L_PM;
            break;
        case 4:
            reg_value |= CH341_L_PS;
            break;
        default:
            return -USB_ERR_INVAL;
    }

    switch (line_coding->bDataBits) {
        case 5:
            reg_value |= CH341_L_D5;
            break;
        case 6:
            reg_value |= CH341_L_D6;
            break;
        case 7:
            reg_value |= CH341_L_D7;
            break;
        case 8:
            reg_value |= CH341_L_D8;
            break;
        default:
            return -USB_ERR_INVAL;
    }

    if (line_coding->bCharFormat == 2) {
        reg_value |= CH341_L_SB;
    }

    reg_value |= 0xC0;

    value |= 0x9c;
    value |= reg_value << 8;

    usbh_serial_ch34x_get_baudrate_div(line_coding->dwDTERate, &factor, &divisor);

    setup->bmRequestType = USB_REQUEST_DIR_OUT | USB_REQUEST_VENDOR | USB_REQUEST_RECIPIENT_DEVICE;
    setup->bRequest = CH34X_SERIAL_INIT;
    setup->wValue = value;
    setup->wIndex = (factor << 8) | 0x80 | divisor;
    setup->wLength = 0;

    return usbh_control_transfer(serial_class->hport, setup, NULL);
}

int usbh_serial_ch34x_get_line_coding(struct usbh_serial_class *serial_class, struct cdc_line_coding *line_coding)
{
    memcpy(line_coding, (uint8_t *)&serial_class->line_coding, sizeof(struct cdc_line_coding));
    return 0;
}

int usbh_serial_ch34x_set_line_state(struct usbh_serial_class *serial_class, bool dtr, bool rts)
{
    struct usb_setup_packet *setup;

    if (!serial_class || !serial_class->hport) {
        return -USB_ERR_INVAL;
    }
    setup = serial_class->hport->setup;

    setup->bmRequestType = USB_REQUEST_DIR_OUT | USB_REQUEST_VENDOR | USB_REQUEST_RECIPIENT_DEVICE;
    setup->bRequest = CH34X_MODEM_CTRL;
    setup->wValue = 0x0f | (dtr << 5) | (rts << 6);
    setup->wIndex = 0;
    setup->wLength = 0;

    return usbh_control_transfer(serial_class->hport, setup, NULL);
}

static int usbh_serial_ch34x_connect(struct usbh_hubport *hport, uint8_t intf)
{
    struct usb_endpoint_descriptor *ep_desc;
    uint8_t devnum = 0;
    int ret = 0;

    if (!usbh_serial_increment_count(&devnum)) {
        log_w("Too many serial devices attached");
        return -USB_ERR_NODEV;
    }

    struct usbh_serial_ch34x *ch34x_class = usbh_serial_ch34x_class_alloc();
    if (ch34x_class == NULL) {
        log_e("Fail to alloc ch34x_class");
        usbh_serial_decrement_count(devnum);
        return -USB_ERR_NOMEM;
    }

    HPORT(ch34x_class) = hport;
    ch34x_class->base.intf = intf;
    ch34x_class->base.devnum = devnum;

    hport->config.intf[intf].priv = ch34x_class;

    usbh_serial_ch34x_get_version(ch34x_class);
    usbh_serial_ch34x_flow_ctrl(ch34x_class);

    for (uint8_t i = 0; i < hport->config.intf[intf].altsetting[0].intf_desc.bNumEndpoints; i++) {
        ep_desc = &hport->config.intf[intf].altsetting[0].ep[i].ep_desc;
        if (USB_GET_ENDPOINT_TYPE(ep_desc->bmAttributes) == USB_ENDPOINT_TYPE_INTERRUPT) {
            continue;
        } else {
            if (ep_desc->bEndpointAddress & 0x80) {
                USBH_EP_INIT(ch34x_class->base.bulkin, ep_desc);
            } else {
                USBH_EP_INIT(ch34x_class->base.bulkout, ep_desc);
            }
        }
    }

    snprintf(hport->config.intf[intf].devname, CONFIG_USBHOST_DEV_NAMELEN, DEV_FORMAT, ch34x_class->base.devnum);

    log_i("Register CH34X Class:%s", hport->config.intf[intf].devname);

    usbh_serial_run((struct usbh_serial_class *)ch34x_class);
    return ret;
}

static int usbh_serial_ch34x_disconnect(struct usbh_hubport *hport, uint8_t intf)
{
    int ret = 0;

    struct usbh_serial_ch34x *ch34x_class = (struct usbh_serial_ch34x *)hport->config.intf[intf].priv;

    if (ch34x_class) {
        if (ch34x_class->base.bulkin) {
            usbh_kill_urb(&ch34x_class->base.bulkin_urb);
        }

        if (ch34x_class->base.bulkout) {
            usbh_kill_urb(&ch34x_class->base.bulkout_urb);
        }

        if (hport->config.intf[intf].devname[0] != '\0') {
            log_i("Unregister CH34X Class:%s", hport->config.intf[intf].devname);
            usbh_serial_stop((struct usbh_serial_class *)ch34x_class);
        }

        usbh_serial_decrement_count(ch34x_class->base.devnum);
        usbh_serial_ch34x_class_free(ch34x_class);
    }

    return ret;
}

static const uint16_t serial_ch34x_id_table[][2] = {
    { 0x1A86, 0x7523 }, /* CH340 */
    { 0x1A86, 0x7522 }, /* CH340K */
    { 0x1A86, 0x5523 }, /* CH341 */
    { 0x1A86, 0xe523 }, /* CH330 */
    { 0, 0 },
};

const struct usbh_class_driver serial_ch34x_class_driver = {
    .driver_name = "ch34x",
    .connect = usbh_serial_ch34x_connect,
    .disconnect = usbh_serial_ch34x_disconnect,
};

CLASS_INFO_DEFINE const struct usbh_class_info serial_ch34x_class_info = {
    .match_flags = USB_CLASS_MATCH_VID_PID | USB_CLASS_MATCH_INTF_CLASS,
    .class = 0xff,
    .subclass = 0x00,
    .protocol = 0x00,
    .id_table = serial_ch34x_id_table,
    .class_driver = &serial_ch34x_class_driver
};