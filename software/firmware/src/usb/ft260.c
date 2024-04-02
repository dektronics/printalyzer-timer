#include "ft260.h"

#include "usbh_core.h"
#include "usbh_hid.h"

#define LOG_TAG "ft260"
#include <elog.h>

#define HID_REPORT_FT260_CHIP_CODE        0xA0
#define HID_REPORT_FT260_SYSTEM_SETTING   0xA1
#define HID_REPORT_FT260_GPIO             0xB0
#define HID_REPORT_FT260_INTERRUPT_STATUS 0xB1

#define HID_REPORT_FT260_I2C_STATUS       0xC0
#define HID_REPORT_FT260_I2C_READ_REQUEST 0xC2
#define HID_REPORT_FT260_I2C_IO_BASE      0xD0

#define HID_REPORT_FT260_UART_STATUS      0xE0
#define HID_REPORT_FT260_UART_RI_DCD      0xE2
#define HID_REPORT_FT260_UART_IO_BASE     0xF0

#define FT260_I2C_NONE           0
#define FT260_I2C_START          0x02
#define FT260_I2C_REPEATED_START 0x03
#define FT260_I2C_STOP           0x04
#define FT260_I2C_START_AND_STOP 0x06

USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t g_ft260_buf[64];

static int ft260_set_report(struct usbh_hid *hid_class, uint8_t report_type, uint8_t report_id, uint8_t *buffer, uint32_t buflen);
static int ft260_get_report(struct usbh_hid *hid_class, uint8_t report_type, uint8_t report_id, uint8_t *buffer, uint32_t buflen);
static int ft260_i2c_write_request(struct usbh_hid *hid_class, uint8_t dev_address, uint8_t flags, const uint8_t *buffer, uint32_t buflen);
static int ft260_i2c_read_request(struct usbh_hid *hid_class, uint8_t dev_address, uint8_t flags, uint16_t len);
static int ft260_i2c_input_report(struct usbh_hid *hid_class, uint8_t *buffer, uint16_t buflen);

int ft260_get_chip_version(struct usbh_hid *hid_class, ft260_chip_version_t *chip_version)
{
    int ret;
    uint8_t buf[13];

    ret = ft260_get_report(hid_class, HID_REPORT_FEATURE, HID_REPORT_FT260_CHIP_CODE, buf, 13);
    if (ret < 0) {
        log_w("ft260_get_chip_version failed: %d", ret);
        return ret;
    }
    if (ret < 5) {
        log_w("ft260_get_chip_version unexpected length: %d", ret);
        return -USB_ERR_INVAL;
    }

    if (chip_version) {
        chip_version->chip = (buf[1] << 8) | buf[2];
        chip_version->minor = buf[3];
        chip_version->major = buf[4];
    }

    return ret;
}

int ft260_get_system_status(struct usbh_hid *hid_class, ft260_system_status_t *system_status)
{
    int ret;
    uint8_t buf[26];

    ret = ft260_get_report(hid_class, HID_REPORT_FEATURE, HID_REPORT_FT260_SYSTEM_SETTING, buf, 26);
    if (ret < 0) {
        log_w("ft260_get_system_status failed: %d", ret);
        return ret;
    }
    if (ret < 15) {
        log_w("ft260_get_system_status unexpected length: %d", ret);
        return -USB_ERR_INVAL;
    }

    if (system_status) {
        memset(system_status, 0, sizeof(ft260_system_status_t));

        system_status->chip_mode = buf[1];
        system_status->clock_control = buf[2];
        system_status->suspend_status = buf[3];
        system_status->pwren_status = buf[4];
        system_status->i2c_enable = buf[5];
        system_status->uart_mode = buf[6];
        system_status->hid_over_i2c_enable = buf[7];
        system_status->gpio2_function = buf[8];
        system_status->gpioa_function = buf[9];
        system_status->gpiog_function = buf[10];
        system_status->suspend_output_polarity = buf[11];
        system_status->enable_wakeup_int = buf[12];
        system_status->interrupt_condition = buf[13];
        system_status->enable_power_saving = buf[14];
    }

    return ret;
}

int ft250_set_i2c_clock_speed(struct usbh_hid *hid_class, uint16_t speed)
{
    int ret;
    uint8_t buf[4];

    buf[0] = HID_REPORT_FT260_SYSTEM_SETTING;
    buf[1] = 0x22; /* Set I2C clock speed */
    buf[2] = (uint8_t)(speed & 0x00FF);
    buf[3] = (uint8_t)((speed & 0xFF00) >> 8);

    ret = ft260_set_report(hid_class, HID_REPORT_FEATURE, HID_REPORT_FT260_SYSTEM_SETTING, buf, 4);
    if (ret < 0) {
        log_w("ft250_set_i2c_clock_speed failed: %d", ret);
        return ret;
    }

    return ret;

}

int ft260_get_i2c_status(struct usbh_hid *hid_class, uint8_t *bus_status, uint16_t *speed)
{
    int ret;
    uint8_t buf[5];

    ret = ft260_get_report(hid_class, HID_REPORT_FEATURE, HID_REPORT_FT260_I2C_STATUS, buf, 5);
    if (ret < 0) {
        log_w("ft260_get_i2c_status failed: %d", ret);
        return ret;
    }

    if (bus_status) {
        *bus_status = buf[1];
    }

    if (speed) {
        *speed = (uint16_t)(buf[3]) << 8 | buf[2];
    }

    return ret;
}

int ft260_i2c_mem_read(struct usbh_hid *hid_class, uint8_t dev_address, uint8_t mem_address, uint8_t *data, uint16_t size)
{
    int ret;
    uint8_t buf[64];

    // Note: This is somewhat scratch code with minimal error checking, and no support for multi-packet reads

    memset(buf, 0, sizeof(buf));

    /* Write request to set the memory address */
    ret = ft260_i2c_write_request(hid_class, dev_address, FT260_I2C_START, &mem_address, 1);
    if (ret < 0) {
        log_w("ft260_i2c_mem_read[1]: %d", ret);
        return ret;
    }

    /* Read request for the data */
    ret = ft260_i2c_read_request(hid_class, dev_address, FT260_I2C_REPEATED_START | FT260_I2C_STOP, size);
    if (ret < 0) {
        log_w("ft260_i2c_mem_read[2]: %d", ret);
        return ret;
    }

    /* Input report to read the data */
    ret = ft260_i2c_input_report(hid_class, data, size);
    if (ret < 0) {
        log_w("ft260_i2c_mem_read[3]: %d", ret);
        return ret;
    }

    if (ret != size) {
        log_w("Unexpected size returned: %d != %d", ret, size);
    }

    return ret;
}

int ft260_i2c_mem_write(struct usbh_hid *hid_class, uint8_t dev_address, uint8_t mem_address, const uint8_t *data, uint8_t size)
{
    int ret;

    /* Write request to set the memory address */
    ret = ft260_i2c_write_request(hid_class, dev_address, FT260_I2C_START, &mem_address, 1);
    if (ret < 0) {
        log_w("ft260_i2c_mem_write[1]: %d", ret);
        return ret;
    }


    /* Write request with the data to be written */
    ret = ft260_i2c_write_request(hid_class, dev_address, FT260_I2C_STOP, data, size);
    if (ret < 0) {
        log_w("ft260_i2c_mem_write[2]: %d", ret);
        return ret;
    }

    return ret;
}

int ft260_i2c_is_device_ready(struct usbh_hid *hid_class, uint8_t dev_address)
{
    int ret;
    //TODO Need to test this and see if we're actually doing things correctly and what error codes to expect
    ret = ft260_i2c_write_request(hid_class, dev_address, FT260_I2C_START, NULL, 0);
    if (ret < 0) {
        log_w("ft260_i2c_is_device_ready[1]: %d", ret);
        return ret;
    }
    return ret;
}

int ft260_set_uart_enable_dcd_ri(struct usbh_hid *hid_class, bool enable)
{
    int ret;
    uint8_t buf[3];

    buf[0] = HID_REPORT_FT260_SYSTEM_SETTING;
    buf[1] = 0x07; /* Enable UART DCD RI */
    buf[2] = enable ? 1 : 0;

    ret = ft260_set_report(hid_class, HID_REPORT_FEATURE, HID_REPORT_FT260_SYSTEM_SETTING, buf, 3);
    if (ret < 0) {
        log_w("ft260_set_uart_enable_dcd_ri failed: %d", ret);
        return ret;
    }

    return ret;
}

int ft260_set_uart_enable_ri_wakeup(struct usbh_hid *hid_class, bool enable)
{
    int ret;
    uint8_t buf[3];

    buf[0] = HID_REPORT_FT260_SYSTEM_SETTING;
    buf[1] = 0x0C; /* Enable UART RI Wakeup */
    buf[2] = enable ? 1 : 0;

    ret = ft260_set_report(hid_class, HID_REPORT_FEATURE, HID_REPORT_FT260_SYSTEM_SETTING, buf, 3);
    if (ret < 0) {
        log_w("ft260_set_uart_enable_ri_wakeup failed: %d", ret);
        return ret;
    }

    return ret;
}

int ft260_set_uart_ri_wakeup_config(struct usbh_hid *hid_class, bool edge)
{
    int ret;
    uint8_t buf[3];

    buf[0] = HID_REPORT_FT260_SYSTEM_SETTING;
    buf[1] = 0x0D; /* Set UART RI Wakeup Config */
    buf[2] = edge ? 1 : 0; /* true = falling, false = rising */

    ret = ft260_set_report(hid_class, HID_REPORT_FEATURE, HID_REPORT_FT260_SYSTEM_SETTING, buf, 3);
    if (ret < 0) {
        log_w("ft260_set_uart_ri_wakeup_config failed: %d", ret);
        return ret;
    }

    return ret;
}

int ft260_set_report(struct usbh_hid *hid_class, uint8_t report_type, uint8_t report_id, uint8_t *buffer, uint32_t buflen)
{
    /*
     * To send an output report, we need an aligned static buffer that lasts
     * the duration of the transaction. Its easier to implement this code here,
     * where we can be sure unrelated HID device drivers won't be trying to
     * share the same buffer.
     */
    struct usb_setup_packet *setup = hid_class->hport->setup;

    setup->bmRequestType = USB_REQUEST_DIR_OUT | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_INTERFACE;
    setup->bRequest = HID_REQUEST_SET_REPORT;
    setup->wValue = (uint16_t)(((uint32_t)report_type << 8U) | (uint32_t)report_id);
    setup->wIndex = 0;
    setup->wLength = buflen;

    memcpy(g_ft260_buf, buffer, buflen);
    return usbh_control_transfer(hid_class->hport, setup, g_ft260_buf);
}

int ft260_get_report(struct usbh_hid *hid_class, uint8_t report_type, uint8_t report_id, uint8_t *buffer, uint32_t buflen)
{
    struct usb_setup_packet *setup = hid_class->hport->setup;
    int ret;

    if (buflen > sizeof(g_ft260_buf)) {
        return -USB_ERR_INVAL;
    }

    setup->bmRequestType = USB_REQUEST_DIR_IN | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_INTERFACE;
    setup->bRequest = HID_REQUEST_GET_REPORT;
    setup->wValue = (uint16_t)(((uint32_t)report_type << 8U) | (uint32_t)report_id);
    setup->wIndex = 0;
    setup->wLength = buflen;

    memset(g_ft260_buf, 0, sizeof(buflen));
    ret = usbh_control_transfer(hid_class->hport, setup, g_ft260_buf);
    if (ret < 0) {
        return ret;
    }
    memcpy(buffer, g_ft260_buf, buflen);
    return ret;
}

static uint8_t ft260_i2c_report_id(uint8_t payload_size)
{
    if (payload_size == 0 || payload_size > 60) {
        return 0;
    }
    return 0xD0 + ((payload_size - 1) >> 2);
}

int ft260_i2c_write_request(struct usbh_hid *hid_class, uint8_t dev_address, uint8_t flags, const uint8_t *buffer, uint32_t buflen)
{
    int ret;
    const uint8_t report_id = ft260_i2c_report_id(buflen);
    if (report_id == 0) {
        log_w("Invalid length: %d", buflen);
        return -USB_ERR_INVAL;
    }

    const uint8_t transfer_len = ((report_id - 0xD0 + 1) * 4) + 4;

    memset(g_ft260_buf, 0, transfer_len);

    g_ft260_buf[0] = report_id;
    g_ft260_buf[1] = dev_address;
    g_ft260_buf[2] = flags;
    g_ft260_buf[3] = buflen;
    if (buflen > 0) {
        memcpy(g_ft260_buf + 4, buffer, buflen);
    }

    usbh_int_urb_fill(&hid_class->intout_urb, hid_class->hport, hid_class->intout, g_ft260_buf, transfer_len, 500, NULL, NULL);
    ret = usbh_submit_urb(&hid_class->intout_urb);
    if (ret < 0) {
        log_w("ft260_i2c_write_request error: %d", ret);
    }

    return ret;
}

int ft260_i2c_read_request(struct usbh_hid *hid_class, uint8_t dev_address, uint8_t flags, uint16_t len)
{
    int ret;

    memset(g_ft260_buf, 0, 5);

    g_ft260_buf[0] = 0xC2;
    g_ft260_buf[1] = dev_address;
    g_ft260_buf[2] = flags;
    g_ft260_buf[3] = len & 0x00FF;
    g_ft260_buf[4] = (len & 0xFF00) >> 8;

    usbh_int_urb_fill(&hid_class->intout_urb, hid_class->hport, hid_class->intout, g_ft260_buf, 5, 500, NULL, NULL);
    ret = usbh_submit_urb(&hid_class->intout_urb);
    if (ret < 0) {
        log_w("ft260_i2c_read_request error: %d", ret);
    }
    return ret;
}

int ft260_i2c_input_report(struct usbh_hid *hid_class, uint8_t *buffer, uint16_t buflen)
{
    int ret;
    int retry_count = 0;
    uint32_t offset = 0;

    if (!buffer) { return -USB_ERR_INVAL; }

    memset(g_ft260_buf, 0, sizeof(g_ft260_buf));

    do {
        usbh_int_urb_fill(&hid_class->intin_urb, hid_class->hport, hid_class->intin,
            g_ft260_buf, (buflen - offset) + 2, 500, NULL, NULL);
        ret = usbh_submit_urb(&hid_class->intin_urb);
        if (ret == -USB_ERR_NAK) {
            if (retry_count < 5) { retry_count++; continue; }
        }
        if (ret < 0) { break; }

#if 0
        log_d("ft260_i2c_input_report: buflen=%d, payload_len=%d, urb_len=%d", buflen, g_ft260_buf[1], hid_class->intin_urb.actual_length);
#endif
        if (offset + g_ft260_buf[1] > buflen) {
            log_w("ft260_i2c_input_report: input overrun: %d + %d > %d", offset, g_ft260_buf[1], buflen);
            ret = -USB_ERR_INVAL;
            break;
        }

        memcpy(buffer + offset, g_ft260_buf + 2, g_ft260_buf[1]);
        offset += g_ft260_buf[1];
        retry_count = 0;
    } while (offset < buflen);

    if (ret < 0) {
        log_w("ft260_i2c_input_report error: %d", ret);
        return ret;
    }

    return offset;
}
