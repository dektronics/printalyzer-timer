/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usbh_core.h"
#include "usbh_serial_cdc_acm.h"

#include <FreeRTOS.h>
#include <atomic.h>

#define LOG_TAG "usbh_serial_cdc"
#include <elog.h>

#define DEV_FORMAT "/dev/ttyACM%d"

static int usbh_serial_cdc_acm_set_line_coding(struct usbh_serial_class *serial_class, struct cdc_line_coding *line_coding);
static int usbh_serial_cdc_acm_get_line_coding(struct usbh_serial_class *serial_class, struct cdc_line_coding *line_coding);
static int usbh_serial_cdc_acm_set_line_state(struct usbh_serial_class *serial_class, bool dtr, bool rts);

static struct usbh_serial_class_interface const vtable = {
    .set_line_coding = usbh_serial_cdc_acm_set_line_coding,
    .get_line_coding = usbh_serial_cdc_acm_get_line_coding,
    .set_line_state = usbh_serial_cdc_acm_set_line_state,
    .bulk_in_transfer = NULL, /* default implementation */
    .bulk_out_transfer = NULL /* default implementation */
};

#define HPORT(x) (x->base.hport)
#define SETUP_PACKET(x) (x->base.hport->setup)

static struct usbh_serial_cdc_acm *usbh_serial_cdc_acm_class_alloc(void)
{
    struct usbh_serial_cdc_acm *cdc_acm_class = pvPortMalloc(sizeof(struct usbh_serial_cdc_acm));
    if (cdc_acm_class) {
        memset(cdc_acm_class, 0, sizeof(struct usbh_serial_cdc_acm));
        cdc_acm_class->base.vtable = &vtable;
    }
    return cdc_acm_class;
}

static void usbh_serial_cdc_acm_class_free(struct usbh_serial_cdc_acm *cdc_acm_class)
{
    vPortFree(cdc_acm_class);
}

int usbh_serial_cdc_acm_set_line_coding(struct usbh_serial_class *serial_class, struct cdc_line_coding *line_coding)
{
    struct usbh_serial_cdc_acm *cdc_acm_class = (struct usbh_serial_cdc_acm *)serial_class;
    if (!cdc_acm_class || !cdc_acm_class->base.hport) {
        return -USB_ERR_INVAL;
    }
    struct usb_setup_packet *setup = serial_class->hport->setup;

    setup->bmRequestType = USB_REQUEST_DIR_OUT | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_INTERFACE;
    setup->bRequest = CDC_REQUEST_SET_LINE_CODING;
    setup->wValue = 0;
    setup->wIndex = cdc_acm_class->base.intf;
    setup->wLength = 7;

    memcpy(cdc_acm_class->control_buf, line_coding, sizeof(struct cdc_line_coding));

    return usbh_control_transfer(serial_class->hport, setup, cdc_acm_class->control_buf);
}

int usbh_serial_cdc_acm_get_line_coding(struct usbh_serial_class *serial_class, struct cdc_line_coding *line_coding)
{
    struct usbh_serial_cdc_acm *cdc_acm_class = (struct usbh_serial_cdc_acm *)serial_class;
    int ret;

    if (!cdc_acm_class || !cdc_acm_class->base.hport) {
        return -USB_ERR_INVAL;
    }
    struct usb_setup_packet *setup = serial_class->hport->setup;
    setup->bmRequestType = USB_REQUEST_DIR_IN | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_INTERFACE;
    setup->bRequest = CDC_REQUEST_GET_LINE_CODING;
    setup->wValue = 0;
    setup->wIndex = cdc_acm_class->base.intf;
    setup->wLength = 7;

    ret = usbh_control_transfer(serial_class->hport, setup, cdc_acm_class->control_buf);
    if (ret < 0) {
        return ret;
    }
    memcpy(line_coding, cdc_acm_class->control_buf, sizeof(struct cdc_line_coding));
    return ret;
}

int usbh_serial_cdc_acm_set_line_state(struct usbh_serial_class *serial_class, bool dtr, bool rts)
{
    struct usbh_serial_cdc_acm *cdc_acm_class = (struct usbh_serial_cdc_acm *)serial_class;
    if (!cdc_acm_class || !cdc_acm_class->base.hport) {
        return -USB_ERR_INVAL;
    }
    struct usb_setup_packet *setup = serial_class->hport->setup;

    setup->bmRequestType = USB_REQUEST_DIR_OUT | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_INTERFACE;
    setup->bRequest = CDC_REQUEST_SET_CONTROL_LINE_STATE;
    setup->wValue = (dtr << 0) | (rts << 1);
    setup->wIndex = cdc_acm_class->base.intf;
    setup->wLength = 0;

    return usbh_control_transfer(serial_class->hport, setup, NULL);
}

static int usbh_serial_cdc_acm_connect(struct usbh_hubport *hport, uint8_t intf)
{
    struct usb_endpoint_descriptor *ep_desc;
    uint8_t devnum = 0;
    int ret = 0;

    if (!usbh_serial_increment_count(&devnum)) {
        log_w("Too many serial devices attached");
        return -USB_ERR_NODEV;
    }

    struct usbh_serial_cdc_acm *cdc_acm_class = usbh_serial_cdc_acm_class_alloc();
    if (cdc_acm_class == NULL) {
        USB_LOG_ERR("Fail to alloc cdc_acm_class\r\n");
        usbh_serial_decrement_count(devnum);
        return -USB_ERR_NOMEM;
    }

    HPORT(cdc_acm_class) = hport;
    cdc_acm_class->base.intf = intf;
    cdc_acm_class->base.devnum = devnum;

    hport->config.intf[intf].priv = cdc_acm_class;
    hport->config.intf[intf + 1].priv = NULL;

#ifdef CONFIG_USBHOST_CDC_ACM_NOTIFY
    ep_desc = &hport->base.config.intf[intf].altsetting[0].ep[0].ep_desc;
    USBH_EP_INIT(cdc_acm_class->intin, ep_desc);
#endif
    for (uint8_t i = 0; i < hport->config.intf[intf + 1].altsetting[0].intf_desc.bNumEndpoints; i++) {
        ep_desc = &hport->config.intf[intf + 1].altsetting[0].ep[i].ep_desc;

        if (ep_desc->bEndpointAddress & 0x80) {
            USBH_EP_INIT(cdc_acm_class->base.bulkin, ep_desc);
        } else {
            USBH_EP_INIT(cdc_acm_class->base.bulkout, ep_desc);
        }
    }

    snprintf(hport->config.intf[intf].devname, CONFIG_USBHOST_DEV_NAMELEN, DEV_FORMAT, cdc_acm_class->base.devnum);

    USB_LOG_INFO("Register CDC ACM Class:%s\r\n", hport->config.intf[intf].devname);

    usbh_serial_run((struct usbh_serial_class *)cdc_acm_class);
    return ret;
}

static int usbh_serial_cdc_acm_disconnect(struct usbh_hubport *hport, uint8_t intf)
{
    int ret = 0;

    struct usbh_serial_cdc_acm *cdc_acm_class = (struct usbh_serial_cdc_acm *)hport->config.intf[intf].priv;

    if (cdc_acm_class) {
        if (cdc_acm_class->base.bulkin) {
            usbh_kill_urb(&cdc_acm_class->base.bulkin_urb);
        }

        if (cdc_acm_class->base.bulkout) {
            usbh_kill_urb(&cdc_acm_class->base.bulkout_urb);
        }

#ifdef CONFIG_USBHOST_CDC_ACM_NOTIFY
        if (cdc_acm_class->intin) {
            usbh_kill_urb(&cdc_acm_class->intin_urb);
        }
#endif

        if (hport->config.intf[intf].devname[0] != '\0') {
            USB_LOG_INFO("Unregister CDC ACM Class:%s\r\n", hport->config.intf[intf].devname);
            usbh_serial_stop((struct usbh_serial_class *)cdc_acm_class);
        }

        usbh_serial_decrement_count(cdc_acm_class->base.devnum);
        usbh_serial_cdc_acm_class_free(cdc_acm_class);
    }

    return ret;
}

const struct usbh_class_driver cdc_serial_acm_class_driver = {
    .driver_name = "cdc_acm",
    .connect = usbh_serial_cdc_acm_connect,
    .disconnect = usbh_serial_cdc_acm_disconnect
};

CLASS_INFO_DEFINE const struct usbh_class_info cdc_serial_acm_class_info = {
    .match_flags = USB_CLASS_MATCH_INTF_CLASS | USB_CLASS_MATCH_INTF_SUBCLASS,
    .bInterfaceClass = USB_DEVICE_CLASS_CDC,
    .bInterfaceSubClass = CDC_ABSTRACT_CONTROL_MODEL,
    .bInterfaceProtocol = 0x00,
    .id_table = NULL,
    .class_driver = &cdc_serial_acm_class_driver
};
