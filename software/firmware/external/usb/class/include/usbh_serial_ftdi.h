#ifndef __USBH_SERIAL_FTDI_H
#define __USBH_SERIAL_FTDI_H

#ifdef __cplusplus
extern "C" {
#endif

/*
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
 * Protocol constants and related comments are covered under the following license:
 * SPDX-License-Identifier: BSD-2-Clause-NetBSD
 *
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdbool.h>
#include "usbh_core.h"

extern USBH_ClassTypeDef VENDOR_SERIAL_FTDI_Class;
#define USBH_VENDOR_SERIAL_FTDI_CLASS &VENDOR_SERIAL_FTDI_Class

typedef enum {
    USBH_FTDI_TYPE_A,
    USBH_FTDI_TYPE_B,
    USBH_FTDI_TYPE_H,
} usbh_ftdi_type_t;

typedef enum {
    USBH_FTDI_STATE_IDLE = 0U,
    USBH_FTDI_STATE_TRANSFER_DATA,
    USBH_FTDI_STATE_ERROR
} usbh_ftdi_state_t;

typedef enum {
    USBH_FTDI_IDLE = 0U,
    USBH_FTDI_SEND_DATA,
    USBH_FTDI_SEND_DATA_WAIT,
    USBH_FTDI_RECEIVE_DATA,
    USBH_FTDI_RECEIVE_DATA_WAIT,
} usbh_ftdi_data_state_t;

typedef struct {
    uint8_t in_pipe;
    uint8_t out_pipe;
    uint8_t in_ep;
    uint8_t out_ep;
    uint16_t in_ep_size;
    uint16_t out_ep_size;
    uint8_t *in_ep_buffer;
    uint8_t *out_ep_buffer;
} usbh_ftdi_data_intf_t;

typedef struct _FTDI_Process {
    usbh_ftdi_type_t type;
    uint16_t port_no;
    usbh_ftdi_state_t state;
    usbh_ftdi_data_state_t data_tx_state;
    usbh_ftdi_data_state_t data_rx_state;
    usbh_ftdi_data_intf_t data_intf;
    uint8_t *tx_data;
    uint8_t *rx_data;
    uint32_t tx_data_length;
    uint32_t rx_data_length;
    uint32_t timer;
    uint16_t poll;
} FTDI_HandleTypeDef;

#define FTDI_MIN_POLL 40U

/*
 * BmRequestType:  0100 0000B
 * bRequest:       FTDI_SIO_RESET
 * wValue:         Control Value
 *                   0 = Reset SIO
 *                   1 = Purge RX buffer
 *                   2 = Purge TX buffer
 * wIndex:         Port
 * wLength:        0
 * Data:           None
 *
 * The Reset SIO command has this effect:
 *
 *    Sets flow control set to 'none'
 *    Event char = 0x0d
 *    Event trigger = disabled
 *    Purge RX buffer
 *    Purge TX buffer
 *    Clear DTR
 *    Clear RTS
 *    baud and data format not reset
 *
 * The Purge RX and TX buffer commands affect nothing except the buffers
 */
/* FTDI_SIO_RESET */
#define FTDI_SIO_RESET_SIO 0
#define FTDI_SIO_RESET_PURGE_RX 1
#define FTDI_SIO_RESET_PURGE_TX 2

/*
 * BmRequestType:  0100 0000B
 * bRequest:       FTDI_SIO_SET_DATA
 * wValue:         Data characteristics (see below)
 * wIndex:         Port
 * wLength:        0
 * Data:           No
 *
 * Data characteristics
 *
 *   B0..7   Number of data bits
 *   B8..10  Parity
 *           0 = None
 *           1 = Odd
 *           2 = Even
 *           3 = Mark
 *           4 = Space
 *   B11..13 Stop Bits
 *           0 = 1
 *           1 = 1.5
 *           2 = 2
 *   B14..15 Reserved
 *
 */
/* FTDI_SIO_SET_DATA */
#define FTDI_SIO_SET_DATA_BITS(n) (n)
#define FTDI_SIO_SET_DATA_PARITY_NONE (0x0 << 8)
#define FTDI_SIO_SET_DATA_PARITY_ODD (0x1 << 8)
#define FTDI_SIO_SET_DATA_PARITY_EVEN (0x2 << 8)
#define FTDI_SIO_SET_DATA_PARITY_MARK (0x3 << 8)
#define FTDI_SIO_SET_DATA_PARITY_SPACE (0x4 << 8)
#define FTDI_SIO_SET_DATA_STOP_BITS_1 (0x0 << 11)
#define FTDI_SIO_SET_DATA_STOP_BITS_15 (0x1 << 11)
#define FTDI_SIO_SET_DATA_STOP_BITS_2 (0x2 << 11)
#define FTDI_SIO_SET_BREAK (0x1 << 14)

/*
 * BmRequestType:   0100 0000B
 * bRequest:        FTDI_SIO_MODEM_CTRL
 * wValue:          ControlValue (see below)
 * wIndex:          Port
 * wLength:         0
 * Data:            None
 *
 * NOTE: If the device is in RTS/CTS flow control, the RTS set by this
 * command will be IGNORED without an error being returned
 * Also - you can not set DTR and RTS with one control message
 *
 * ControlValue
 * B0    DTR state
 *          0 = reset
 *          1 = set
 * B1    RTS state
 *          0 = reset
 *          1 = set
 * B2..7 Reserved
 * B8    DTR state enable
 *          0 = ignore
 *          1 = use DTR state
 * B9    RTS state enable
 *          0 = ignore
 *          1 = use RTS state
 * B10..15 Reserved
 */
/* FTDI_SIO_MODEM_CTRL */
#define FTDI_SIO_SET_DTR_MASK 0x1
#define FTDI_SIO_SET_DTR_HIGH (1 | ( FTDI_SIO_SET_DTR_MASK  << 8))
#define FTDI_SIO_SET_DTR_LOW  (0 | ( FTDI_SIO_SET_DTR_MASK  << 8))
#define FTDI_SIO_SET_RTS_MASK 0x2
#define FTDI_SIO_SET_RTS_HIGH (2 | ( FTDI_SIO_SET_RTS_MASK << 8))
#define FTDI_SIO_SET_RTS_LOW (0 | ( FTDI_SIO_SET_RTS_MASK << 8))

/*
 *   BmRequestType:  0100 0000b
 *   bRequest:       FTDI_SIO_SET_FLOW_CTRL
 *   wValue:         Xoff/Xon
 *   wIndex:         Protocol/Port - hIndex is protocol / lIndex is port
 *   wLength:        0
 *   Data:           None
 *
 * hIndex protocol is:
 *   B0 Output handshaking using RTS/CTS
 *       0 = disabled
 *       1 = enabled
 *   B1 Output handshaking using DTR/DSR
 *       0 = disabled
 *       1 = enabled
 *   B2 Xon/Xoff handshaking
 *       0 = disabled
 *       1 = enabled
 *
 * A value of zero in the hIndex field disables handshaking
 *
 * If Xon/Xoff handshaking is specified, the hValue field should contain the
 * XOFF character and the lValue field contains the XON character.
 */
/* FTDI_SIO_SET_FLOW_CTRL */
#define FTDI_SIO_DISABLE_FLOW_CTRL 0x0
#define FTDI_SIO_RTS_CTS_HS 0x1
#define FTDI_SIO_DTR_DSR_HS 0x2
#define FTDI_SIO_XON_XOFF_HS 0x4

/*
 * DATA FORMAT
 *
 * IN Endpoint
 *
 * The device reserves the first two bytes of data on this endpoint to contain
 * the current values of the modem and line status registers. In the absence of
 * data, the device generates a message consisting of these two status bytes
 * every 40 ms
 *
 * Byte 0: Modem Status
 *
 * Offset       Description
 * B0   Reserved - must be 1
 * B1   Reserved - must be 0
 * B2   Reserved - must be 0
 * B3   Reserved - must be 0
 * B4   Clear to Send (CTS)
 * B5   Data Set Ready (DSR)
 * B6   Ring Indicator (RI)
 * B7   Receive Line Signal Detect (RLSD)
 *
 * Byte 1: Line Status
 *
 * Offset       Description
 * B0   Data Ready (DR)
 * B1   Overrun Error (OE)
 * B2   Parity Error (PE)
 * B3   Framing Error (FE)
 * B4   Break Interrupt (BI)
 * B5   Transmitter Holding Register (THRE)
 * B6   Transmitter Empty (TEMT)
 * B7   Error in RCVR FIFO
 */
#define FTDI_RS0_CTS    (1 << 4)
#define FTDI_RS0_DSR    (1 << 5)
#define FTDI_RS0_RI     (1 << 6)
#define FTDI_RS0_RLSD   (1 << 7)

#define FTDI_RS_DR      1
#define FTDI_RS_OE      (1<<1)
#define FTDI_RS_PE      (1<<2)
#define FTDI_RS_FE      (1<<3)
#define FTDI_RS_BI      (1<<4)
#define FTDI_RS_THRE    (1<<5)
#define FTDI_RS_TEMT    (1<<6)
#define FTDI_RS_FIFO    (1<<7)

/**
 * Get whether the provided USB handle is a supported FTDI device.
 *
 * Since many USB serial devices all appear under the same "vendor" device
 * class, this provides an easy way to determine if a device is appropriate
 * for this implementation.
 */
bool USBH_FTDI_IsDeviceType(USBH_HandleTypeDef *phost);

USBH_StatusTypeDef USBH_FTDI_Reset(USBH_HandleTypeDef *phost, int reset_type);
USBH_StatusTypeDef USBH_FTDI_SetBaudRate(USBH_HandleTypeDef *phost, uint32_t baudrate);
USBH_StatusTypeDef USBH_FTDI_SetData(USBH_HandleTypeDef *phost, uint16_t data);
USBH_StatusTypeDef USBH_FTDI_SetFlowControl(USBH_HandleTypeDef *phost, uint8_t flow_control);
USBH_StatusTypeDef USBH_FTDI_SetDtr(USBH_HandleTypeDef *phost, bool enabled);
USBH_StatusTypeDef USBH_FTDI_SetRts(USBH_HandleTypeDef *phost, bool enabled);

USBH_StatusTypeDef USBH_FTDI_Transmit(USBH_HandleTypeDef *phost, const uint8_t *pbuff, uint32_t length);

/**
 * Weak function, implement to be notified of transmit complete.
 */
void USBH_FTDI_TransmitCallback(USBH_HandleTypeDef *phost);

/**
 * Weak function, implement to be notified of new received data.
 */
void USBH_FTDI_ReceiveCallback(USBH_HandleTypeDef *phost, uint8_t *data, size_t length);

#ifdef __cplusplus
}
#endif

#endif /* __USBH_SERIAL_FTDI_H */
