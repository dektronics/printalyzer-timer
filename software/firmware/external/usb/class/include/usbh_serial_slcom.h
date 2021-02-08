#ifndef __USBH_SERIAL_SLCOM_H
#define __USBH_SERIAL_SLCOM_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Driver for Silicon Laboratories CP2101/CP2102/CP2103/CP2104/CP2105
 * USB-Serial adapters.  Based on datasheet AN571, publicly available from
 * http://www.silabs.com/Support%20Documents/TechnicalDocs/AN571.pdf
 */

#include <stdbool.h>
#include "usbh_core.h"

extern USBH_ClassTypeDef VENDOR_SERIAL_SLCOM_Class;
#define USBH_VENDOR_SERIAL_SLCOM_CLASS &VENDOR_SERIAL_SLCOM_Class

typedef enum {
    USBH_SLCOM_STATE_IDLE = 0U,
    USBH_SLCOM_STATE_TRANSFER_DATA,
    USBH_SLCOM_STATE_ERROR
} usbh_slcom_state_t;

typedef enum {
    USBH_SLCOM_IDLE_DATA = 0U,
    USBH_SLCOM_SEND_DATA,
    USBH_SLCOM_SEND_DATA_WAIT,
    USBH_SLCOM_RECEIVE_DATA,
    USBH_SLCOM_RECEIVE_DATA_WAIT,
    USBH_SLCOM_CONTROL_DATA,
    USBH_SLCOM_CONTROL_DATA_WAIT
} usbh_slcom_data_state_t;

typedef struct {
    uint8_t in_pipe;
    uint8_t in_ep;
    uint16_t in_ep_size;

    uint8_t out_pipe;
    uint8_t out_ep;
    uint16_t out_ep_size;

    uint8_t control_pipe;
    uint8_t control_ep;
    uint16_t control_ep_size;

    uint8_t *in_ep_buffer;
    uint8_t *out_ep_buffer;
    uint8_t *control_ep_buffer;
} usbh_slcom_endpoints_t;

typedef struct _SLCOM_Process {
    usbh_slcom_state_t state;
    uint8_t msr;
    uint8_t lsr;
    uint8_t iface_no;
    uint8_t partnum;
    usbh_slcom_endpoints_t endpoints;
    usbh_slcom_data_state_t data_tx_state;
    usbh_slcom_data_state_t data_rx_state;
    usbh_slcom_data_state_t data_control_state;
    uint8_t *tx_data;
    uint8_t *rx_data;
    uint32_t tx_data_length;
    uint32_t rx_data_length;
    uint32_t timer;
    uint16_t poll;
} SLCOM_HandleTypeDef;

#define SLCOM_MIN_POLL 40U

/* Line control values */
#define SLCOM_STOP_BITS_1  0x00
#define SLCOM_STOP_BITS_15 0x01
#define SLCOM_STOP_BITS_2  0x02
#define SLCOM_PARITY_NONE  0x00
#define SLCOM_PARITY_ODD   0x10
#define SLCOM_PARITY_EVEN  0x20
#define SLCOM_PARITY_MARK  0x30
#define SLCOM_PARITY_SPACE 0x40
#define SLCOM_SET_DATA_BITS(x) ((x) << 8)

/**
 * Get whether the provided USB handle is a supported SLCOM device.
 *
 * Since many USB serial devices all appear under the same "vendor" device
 * class, this provides an easy way to determine if a device is appropriate
 * for this implementation.
 */
bool USBH_SLCOM_IsDeviceType(USBH_HandleTypeDef *phost);

USBH_StatusTypeDef USBH_SLCOM_Open(USBH_HandleTypeDef *phost);
USBH_StatusTypeDef USBH_SLCOM_Close(USBH_HandleTypeDef *phost);
USBH_StatusTypeDef USBH_SLCOM_SetBaudRate(USBH_HandleTypeDef *phost, uint32_t baudrate);
USBH_StatusTypeDef USBH_SLCOM_SetLineControl(USBH_HandleTypeDef *phost, uint16_t linecontrol);
USBH_StatusTypeDef USBH_SLCOM_SetRtsCts(USBH_HandleTypeDef *phost, uint8_t onoff);
USBH_StatusTypeDef USBH_SLCOM_SetDtr(USBH_HandleTypeDef *phost, uint8_t onoff);
USBH_StatusTypeDef USBH_SLCOM_SetRts(USBH_HandleTypeDef *phost, uint8_t onoff);
USBH_StatusTypeDef USBH_SLCOM_SetBreak(USBH_HandleTypeDef *phost, uint8_t onoff);

USBH_StatusTypeDef USBH_SLCOM_Transmit(USBH_HandleTypeDef *phost, const uint8_t *pbuff, uint32_t length);

/**
 * Weak function, implement to be notified of transmit complete.
 */
void USBH_SLCOM_TransmitCallback(USBH_HandleTypeDef *phost);

/**
 * Weak function, implement to be notified of new received data.
 */
void USBH_SLCOM_ReceiveCallback(USBH_HandleTypeDef *phost, uint8_t *data, size_t length);

#ifdef __cplusplus
}
#endif

#endif /* __USBH_SERIAL_SLCOM_H */
