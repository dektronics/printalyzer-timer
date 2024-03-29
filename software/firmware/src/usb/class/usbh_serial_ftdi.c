/*
 * Basic structure and initial framework code covered under the following license:
 * Copyright (c) 2024, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * -------------------------------------------------------------------------
 *
 * Chipset type and baud rate selection code is covered under the following license:
 * ChibiOS - Copyright (C) 2006..2017 Giovanni Di Sirio
 *           Copyright (C) 2015..2019 Diego Ismirlian, (dismirlian(at)google's mail)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * -------------------------------------------------------------------------
 *
 * All other code covered under the base project license.
 *
 */
#include "usbh_core.h"
#include "usbh_serial_ftdi.h"

#define LOG_TAG "usbh_serial_ftdi"
#include <elog.h>

#define DEV_FORMAT "/dev/ttyUSB%d"

/* Port Identifier Table */
#define FTDI_PIT_DEFAULT    0   /* SIOA */
#define FTDI_PIT_SIOA       1   /* SIOA */
#define FTDI_PIT_SIOB       2   /* SIOB */
#define FTDI_PIT_PARALLEL   3   /* Parallel */

static int usbh_serial_ftdi_set_line_coding(struct usbh_serial_class *serial_class, struct cdc_line_coding *line_coding);
static int usbh_serial_ftdi_get_line_coding(struct usbh_serial_class *serial_class, struct cdc_line_coding *line_coding);
static int usbh_serial_ftdi_set_line_state(struct usbh_serial_class *serial_class, bool dtr, bool rts);
static int usbh_serial_ftdi_bulk_in_check_result(struct usbh_serial_class *serial_class, uint8_t *buffer, int nbytes);

static struct usbh_serial_class_interface const vtable = {
    .set_line_coding = usbh_serial_ftdi_set_line_coding,
    .get_line_coding = usbh_serial_ftdi_get_line_coding,
    .set_line_state = usbh_serial_ftdi_set_line_state,
    .bulk_in_transfer = NULL, /* default implementation */
    .bulk_in_check_result = usbh_serial_ftdi_bulk_in_check_result,
    .bulk_out_transfer = NULL /* default implementation */
};

#define HPORT(x) (x->base.hport)
#define SETUP_PACKET(x) (x->base.hport->setup)

static int usbh_serial_ftdi_match(uint8_t class, uint8_t subclass, uint8_t protocol, uint16_t vid, uint16_t pid)
{
    /*
     * FTDI devices can show up under several different PIDs, so we need to
     * check them all. The VID and other properties are already matched
     * against the class info structure before this function is called.
     */
    switch (pid) {
    case 0x6001:
    case 0x6010:
    case 0x6011:
    case 0x6014:
    case 0x6015:
    case 0xE2E6:
        return 1;
    default:
        return 0;
    }
}

static struct usbh_serial_ftdi *usbh_serial_ftdi_class_alloc(void)
{
    struct usbh_serial_ftdi *ftdi_class = pvPortMalloc(sizeof(struct usbh_serial_ftdi));
    if (ftdi_class) {
        memset(ftdi_class, 0, sizeof(struct usbh_serial_ftdi));
        ftdi_class->base.vtable = &vtable;
    }
    return ftdi_class;
}

static void usbh_serial_ftdi_class_free(struct usbh_serial_ftdi *ftdi_class)
{
    vPortFree(ftdi_class);
}

static uint32_t baudrate_get_divisor(usbh_ftdi_type_t ftdi_type, uint32_t baud)
{
    static const uint8_t divfrac[8] = {0, 3, 2, 4, 1, 5, 6, 7};
    uint32_t divisor;

    if (ftdi_type == USBH_FTDI_TYPE_A) {
        uint32_t divisor3 = ((48000000UL / 2) + baud / 2) / baud;
        log_d("FTDI: desired=%dbps, real=%dbps", baud, (48000000UL / 2) / divisor3);
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
        if (ftdi_type == USBH_FTDI_TYPE_B) {
            divisor = ((48000000UL / 2) + baud / 2) / baud;
            log_d("FTDI: desired=%dbps, real=%dbps", baud, (48000000UL / 2) / divisor);
        } else {
            /* hi-speed baud rate is 10-bit sampling instead of 16-bit */
            if (baud < 1200)
                baud = 1200;
            divisor = (120000000UL * 8 + baud * 5) / (baud * 10);
            log_d("FTDI: desired=%dbps, real=%dbps", baud, (120000000UL * 8) / divisor / 10);
        }
        divisor = (divisor >> 3) | (divfrac[divisor & 0x7] << 14);

        /* Deal with special cases for highest baud rates. */
        if (divisor == 1)
            divisor = 0;
        else if (divisor == 0x4001)
            divisor = 1;

        if (ftdi_type == USBH_FTDI_TYPE_H)
            divisor |= 0x00020000;
    }
    return divisor;
}

int usbh_serial_ftdi_reset(struct usbh_serial_ftdi *ftdi_class)
{
    struct usb_setup_packet *setup = SETUP_PACKET(ftdi_class);

    setup->bmRequestType = USB_REQUEST_DIR_OUT | USB_REQUEST_VENDOR | USB_REQUEST_RECIPIENT_DEVICE;
    setup->bRequest = SIO_RESET_REQUEST;
    setup->wValue = 0;
    setup->wIndex = ftdi_class->base.intf;
    setup->wLength = 0;

    return usbh_control_transfer(HPORT(ftdi_class), setup, NULL);
}

static int usbh_serial_ftdi_set_modem(struct usbh_serial_ftdi *ftdi_class, uint16_t value)
{
    struct usb_setup_packet *setup = SETUP_PACKET(ftdi_class);

    setup->bmRequestType = USB_REQUEST_DIR_OUT | USB_REQUEST_VENDOR | USB_REQUEST_RECIPIENT_DEVICE;
    setup->bRequest = SIO_SET_MODEM_CTRL_REQUEST;
    setup->wValue = value;
    setup->wIndex = ftdi_class->base.intf;
    setup->wLength = 0;

    return usbh_control_transfer(HPORT(ftdi_class), setup, NULL);
}

static int usbh_serial_ftdi_set_baudrate(struct usbh_serial_ftdi *ftdi_class, uint32_t baudrate)
{
    struct usb_setup_packet *setup = SETUP_PACKET(ftdi_class);
    uint32_t itdf_divisor;
    uint16_t value;
    uint8_t baudrate_high;

    itdf_divisor = baudrate_get_divisor(ftdi_class->ftdi_type, baudrate);
    value = itdf_divisor & 0xFFFF;
    baudrate_high = (itdf_divisor >> 16) & 0xff;

    setup->bmRequestType = USB_REQUEST_DIR_OUT | USB_REQUEST_VENDOR | USB_REQUEST_RECIPIENT_DEVICE;
    setup->bRequest = SIO_SET_BAUDRATE_REQUEST;
    setup->wValue = value;
    setup->wIndex = (baudrate_high << 8) | ftdi_class->base.intf;
    setup->wLength = 0;

    return usbh_control_transfer(HPORT(ftdi_class), setup, NULL);
}

static int usbh_serial_ftdi_set_data_format(struct usbh_serial_ftdi *ftdi_class, uint8_t databits, uint8_t parity, uint8_t stopbits, uint8_t isbreak)
{
    /**
     * D0-D7 databits  BITS_7=7, BITS_8=8
     * D8-D10 parity  NONE=0, ODD=1, EVEN=2, MARK=3, SPACE=4
     * D11-D12 		STOP_BIT_1=0, STOP_BIT_15=1, STOP_BIT_2=2 
     * D14  		BREAK_OFF=0, BREAK_ON=1
     **/

    uint16_t value = ((isbreak & 0x01) << 14) | ((stopbits & 0x03) << 11) | ((parity & 0x0f) << 8) | (databits & 0x0f);

    struct usb_setup_packet *setup = SETUP_PACKET(ftdi_class);

    setup->bmRequestType = USB_REQUEST_DIR_OUT | USB_REQUEST_VENDOR | USB_REQUEST_RECIPIENT_DEVICE;
    setup->bRequest = SIO_SET_DATA_REQUEST;
    setup->wValue = value;
    setup->wIndex = ftdi_class->base.intf;
    setup->wLength = 0;

    return usbh_control_transfer(HPORT(ftdi_class), setup, NULL);
}

static int usbh_serial_ftdi_set_latency_timer(struct usbh_serial_ftdi *ftdi_class, uint16_t value)
{
    struct usb_setup_packet *setup = SETUP_PACKET(ftdi_class);

    setup->bmRequestType = USB_REQUEST_DIR_OUT | USB_REQUEST_VENDOR | USB_REQUEST_RECIPIENT_DEVICE;
    setup->bRequest = SIO_SET_LATENCY_TIMER_REQUEST;
    setup->wValue = value;
    setup->wIndex = ftdi_class->base.intf;
    setup->wLength = 0;

    return usbh_control_transfer(HPORT(ftdi_class), setup, NULL);
}

static int usbh_serial_ftdi_set_flow_ctrl(struct usbh_serial_ftdi *ftdi_class, uint16_t value)
{
    struct usb_setup_packet *setup = SETUP_PACKET(ftdi_class);

    setup->bmRequestType = USB_REQUEST_DIR_OUT | USB_REQUEST_VENDOR | USB_REQUEST_RECIPIENT_DEVICE;
    setup->bRequest = SIO_SET_FLOW_CTRL_REQUEST;

    /*
     * This is copied from our previous implementation, and differs from the
     * CherryUSB library upstream. Not sure which is correct, so it will need
     * to be tested if flow control is ever actually used.
     */

    if (value & SIO_XON_XOFF_HS) {
        setup->wValue = (0x13 << 8) | 0x11;
    } else {
        setup->wValue = 0;
    }

    setup->wIndex = (value & 0xFF00) | ftdi_class->base.intf;
    setup->wLength = 0;

    return usbh_control_transfer(HPORT(ftdi_class), setup, NULL);
}

static int usbh_serial_ftdi_read_modem_status(struct usbh_serial_ftdi *ftdi_class)
{
    struct usb_setup_packet *setup = SETUP_PACKET(ftdi_class);
    int ret;

    setup->bmRequestType = USB_REQUEST_DIR_IN | USB_REQUEST_VENDOR | USB_REQUEST_RECIPIENT_DEVICE;
    setup->bRequest = SIO_POLL_MODEM_STATUS_REQUEST;
    setup->wValue = 0x0000;
    setup->wIndex = ftdi_class->base.intf;
    setup->wLength = 2;

    ret = usbh_control_transfer(HPORT(ftdi_class), setup, ftdi_class->control_buf);
    if (ret < 0) {
        return ret;
    }
    memcpy(ftdi_class->modem_status, ftdi_class->control_buf, 2);
    return ret;
}

int usbh_serial_ftdi_set_line_coding(struct usbh_serial_class *serial_class, struct cdc_line_coding *line_coding)
{
    struct usbh_serial_ftdi *ftdi_class = (struct usbh_serial_ftdi *)serial_class;
    memcpy((uint8_t *)&serial_class->line_coding, line_coding, sizeof(struct cdc_line_coding));
    usbh_serial_ftdi_set_baudrate(ftdi_class, line_coding->dwDTERate);
    return usbh_serial_ftdi_set_data_format(ftdi_class, line_coding->bDataBits, line_coding->bParityType, line_coding->bCharFormat, 0);
}

int usbh_serial_ftdi_get_line_coding(struct usbh_serial_class *serial_class, struct cdc_line_coding *line_coding)
{
    memcpy(line_coding, (uint8_t *)&serial_class->line_coding, sizeof(struct cdc_line_coding));
    return 0;
}

int usbh_serial_ftdi_set_line_state(struct usbh_serial_class *serial_class, bool dtr, bool rts)
{
    int ret;
    struct usbh_serial_ftdi *ftdi_class = (struct usbh_serial_ftdi *)serial_class;

    if (dtr) {
        usbh_serial_ftdi_set_modem(ftdi_class, SIO_SET_DTR_HIGH);
    } else {
        usbh_serial_ftdi_set_modem(ftdi_class, SIO_SET_DTR_LOW);
    }

    if (rts) {
        ret = usbh_serial_ftdi_set_modem(ftdi_class, SIO_SET_RTS_HIGH);
    } else {
        ret = usbh_serial_ftdi_set_modem(ftdi_class, SIO_SET_RTS_LOW);
    }

    return ret;
}

static usbh_ftdi_type_t usbh_serial_ftdi_match_type(const struct usbh_hubport *hport)
{
    /* This code assumes the VID/PID and class information has already been validated */

    switch (hport->device_desc.bcdDevice) {
    case 0x200:     //AM
        log_i("FTDI: Type A chip");
        return USBH_FTDI_TYPE_A;
    case 0x400:     //BM
    case 0x500:     //2232C
    case 0x600:     //R
    case 0x1000:    //230X
        log_i("FTDI: Type B chip");
        return USBH_FTDI_TYPE_B;
    case 0x700:     //2232H;
    case 0x800:     //4232H;
    case 0x900:     //232H;
        log_i("FTDI: Type H chip");
        return USBH_FTDI_TYPE_H;
    default:
        return USBH_FTDI_TYPE_UNKNOWN;
    }
}

static int usbh_serial_ftdi_connect(struct usbh_hubport *hport, uint8_t intf)
{
    struct usb_endpoint_descriptor *ep_desc;
    uint8_t devnum = 0;
    int ret = 0;

    if (!usbh_serial_increment_count(&devnum)) {
        log_w("Too many serial devices attached");
        return -USB_ERR_NODEV;
    }

    usbh_ftdi_type_t ftdi_type = usbh_serial_ftdi_match_type(hport);
    if (ftdi_type == USBH_FTDI_TYPE_UNKNOWN) {
        log_e("Unrecognized chip type");
        usbh_serial_decrement_count(devnum);
        return -USB_ERR_INVAL;
    }

    struct usbh_serial_ftdi *ftdi_class = usbh_serial_ftdi_class_alloc();
    if (ftdi_class == NULL) {
        log_e("Fail to alloc ftdi_class");
        usbh_serial_decrement_count(devnum);
        return -USB_ERR_NOMEM;
    }

    HPORT(ftdi_class) = hport;
    ftdi_class->ftdi_type = ftdi_type;
    ftdi_class->base.devnum = devnum;

    if (ftdi_type == USBH_FTDI_TYPE_H) {
        //TODO Need to test this, as the device might simply show up differently on this USB stack
        ftdi_class->base.intf = intf + FTDI_PIT_SIOA;
    } else {
        ftdi_class->base.intf = intf;
    }

    hport->config.intf[intf].priv = ftdi_class;

    do {
        ret = usbh_serial_ftdi_reset(ftdi_class);
        if (ret < 0) { break; }

        ret = usbh_serial_ftdi_set_flow_ctrl(ftdi_class, SIO_DISABLE_FLOW_CTRL);
        if (ret < 0) { break; }

        ret = usbh_serial_ftdi_set_latency_timer(ftdi_class, 0x10);
        if (ret < 0) { break; }

        ret = usbh_serial_ftdi_read_modem_status(ftdi_class);
        if (ret < 0) { break; }
    } while (0);
    if (ret < 0) {
        log_e("Failed to initialize FTDI device: %d", ret);
        usbh_serial_ftdi_class_free(ftdi_class);
        return ret;
    }

    log_i("modem status:%02x:%02x", ftdi_class->modem_status[0], ftdi_class->modem_status[1]);

    for (uint8_t i = 0; i < hport->config.intf[intf].altsetting[0].intf_desc.bNumEndpoints; i++) {
        ep_desc = &hport->config.intf[intf].altsetting[0].ep[i].ep_desc;

        if (ep_desc->bEndpointAddress & 0x80) {
            USBH_EP_INIT(ftdi_class->base.bulkin, ep_desc);
        } else {
            USBH_EP_INIT(ftdi_class->base.bulkout, ep_desc);
        }
    }

    snprintf(hport->config.intf[intf].devname, CONFIG_USBHOST_DEV_NAMELEN, DEV_FORMAT, ftdi_class->base.devnum);

    log_i("Register FTDI Class:%s", hport->config.intf[intf].devname);

    usbh_serial_run((struct usbh_serial_class *)ftdi_class);
    return ret;
}

static int usbh_serial_ftdi_disconnect(struct usbh_hubport *hport, uint8_t intf)
{
    int ret = 0;

    struct usbh_serial_ftdi *ftdi_class = (struct usbh_serial_ftdi *)hport->config.intf[intf].priv;

    if (ftdi_class) {
        if (ftdi_class->base.bulkin) {
            usbh_kill_urb(&ftdi_class->base.bulkin_urb);
        }

        if (ftdi_class->base.bulkout) {
            usbh_kill_urb(&ftdi_class->base.bulkout_urb);
        }

        if (hport->config.intf[intf].devname[0] != '\0') {
            log_i("Unregister FTDI Class:%s", hport->config.intf[intf].devname);
            usbh_serial_stop((struct usbh_serial_class *)ftdi_class);
        }

        usbh_serial_decrement_count(ftdi_class->base.devnum);
        usbh_serial_ftdi_class_free(ftdi_class);
    }


    return ret;
}

int usbh_serial_ftdi_bulk_in_check_result(struct usbh_serial_class *serial_class, uint8_t *buffer, int nbytes)
{
    if (nbytes >= 2) {
        /* FTDI returns two modem status bytes that need to be skipped */
        memmove(buffer, buffer + 2, nbytes - 2);
        nbytes -= 2;
    }
    return nbytes;
}

const struct usbh_class_driver serial_ftdi_class_driver = {
    .driver_name = "ftdi",
    .connect = usbh_serial_ftdi_connect,
    .disconnect = usbh_serial_ftdi_disconnect,
    .match = usbh_serial_ftdi_match
};

CLASS_INFO_DEFINE const struct usbh_class_info serial_ftdi_class_info = {
    .match_flags = USB_CLASS_MATCH_VENDOR | USB_CLASS_MATCH_INTF_CLASS | USB_CLASS_MATCH_CUSTOM_FUNC,
    .class = 0xff,
    .subclass = 0xff,
    .protocol = 0xff,
    .vid = 0x0403,
    .pid = 0x00,
    .class_driver = &serial_ftdi_class_driver
};
