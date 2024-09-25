#ifndef FT260_H
#define FT260_H

#include <stdint.h>
#include <stdbool.h>

struct usbh_hid;

#define FT260_SYSTEM_STATUS_MODE_DCFN0    0x01
#define FT260_SYSTEM_STATUS_MODE_DCFN1    0x02
#define FT260_SYSTEM_STATUS_CLOCK_12MHZ   0
#define FT260_SYSTEM_STATUS_CLOCK_24MHZ   1
#define FT260_SYSTEM_STATUS_CLOCK_48MHZ   2
#define FT260_SYSTEM_STATUS_UART_OFF      0
#define FT260_SYSTEM_STATUS_UART_RTS_CTS  1
#define FT260_SYSTEM_STATUS_UART_DTR_DSR  2
#define FT260_SYSTEM_STATUS_UART_XON_XOFF 3
#define FT260_SYSTEM_STATUS_UART_NO_FLOW  4

#define FT260_SYSTEM_STATUS_INTR_MASK         0x03
#define FT260_SYSTEM_STATUS_INTR_RISING_EDGE  0x00
#define FT260_SYSTEM_STATUS_INTR_LEVEL_HIGH   0x01
#define FT260_SYSTEM_STATUS_INTR_FALLING_EDGE 0x02
#define FT260_SYSTEM_STATUS_INTR_LEVEL_LOW    0x03

#define FT260_SYSTEM_STATUS_INTR_DUR_MASK     0x0C
#define FT260_SYSTEM_STATUS_INTR_DUR_1MS      0x04
#define FT260_SYSTEM_STATUS_INTR_DUR_5MS      0x08
#define FT260_SYSTEM_STATUS_INTR_DUR_30MS     0x0C

#define FT260_I2C_CONTROLLER_BUSY         0x01
#define FT260_I2C_ERROR                   0x02
#define FT260_I2C_SLAVE_ADDRESS_NOT_ACKED 0x04
#define FT260_I2C_DATA_NOT_ACKED          0x08
#define FT260_I2C_ARB_LOST                0x10
#define FT260_I2C_IDLE                    0x20
#define FT260_I2C_BUS_BUSY                0x40

/* GPIO 0-5 bits */
#define FT260_GPIO_0 0x01
#define FT260_GPIO_1 0x02
#define FT260_GPIO_2 0x04
#define FT260_GPIO_3 0x08
#define FT260_GPIO_4 0x10
#define FT260_GPIO_5 0x20

/* GPIO(EX) A-H bits */
#define FT260_GPIOEX_A 0x01
#define FT260_GPIOEX_B 0x02
#define FT260_GPIOEX_C 0x04
#define FT260_GPIOEX_D 0x08
#define FT260_GPIOEX_E 0x10
#define FT260_GPIOEX_F 0x20
#define FT260_GPIOEX_G 0x40
#define FT260_GPIOEX_H 0x80

typedef struct {
    uint16_t chip;
    uint8_t major;
    uint8_t minor;
} ft260_chip_version_t;

typedef struct {
    uint8_t chip_mode;
    uint8_t clock_control;
    uint8_t suspend_status;
    uint8_t pwren_status;
    uint8_t i2c_enable;
    uint8_t uart_mode;
    uint8_t hid_over_i2c_enable;
    uint8_t gpio2_function;
    uint8_t gpioa_function;
    uint8_t gpiog_function;
    uint8_t suspend_output_polarity;
    uint8_t enable_wakeup_int;
    uint8_t interrupt_condition;
    uint8_t enable_power_saving;
} ft260_system_status_t;

typedef struct {
    uint8_t gpio_value;
    uint8_t gpio_dir;
    uint8_t gpio_ex_value;
    uint8_t gpio_ex_dir;
} ft260_gpio_report_t;

int ft260_get_chip_version(struct usbh_hid *hid_class, ft260_chip_version_t *chip_version);
int ft260_get_system_status(struct usbh_hid *hid_class, ft260_system_status_t *system_status);
int ft260_set_system_clock(struct usbh_hid *hid_class, uint8_t clock_rate);

int ft260_set_i2c_clock_speed(struct usbh_hid *hid_class, uint16_t speed);

int ft260_get_i2c_status(struct usbh_hid *hid_class, uint8_t *bus_status, uint16_t *speed);

int ft260_i2c_reset(struct usbh_hid *hid_class);
int ft260_i2c_receive(struct usbh_hid *hid_class, uint8_t dev_address, uint8_t *data, uint16_t size);
int ft260_i2c_mem_read(struct usbh_hid *hid_class, uint8_t dev_address, uint8_t mem_address, uint8_t *data, uint16_t size);
int ft260_i2c_mem_write(struct usbh_hid *hid_class, uint8_t dev_address, uint8_t mem_address, const uint8_t *data, uint8_t size);
int ft260_i2c_is_device_ready(struct usbh_hid *hid_class, uint8_t dev_address);

int ft260_set_uart_enable_dcd_ri(struct usbh_hid *hid_class, bool enable);

int ft260_set_uart_enable_ri_wakeup(struct usbh_hid *hid_class, bool enable);
int ft260_set_uart_ri_wakeup_config(struct usbh_hid *hid_class, bool edge);

int ft260_gpio_read(struct usbh_hid *hid_class, ft260_gpio_report_t *gpio_report);
int ft260_gpio_write(struct usbh_hid *hid_class, const ft260_gpio_report_t *gpio_report);

#endif /* FT260_H */
