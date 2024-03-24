/*-
 * Protocol constants, related comments, and reference logic is covered
 * under the following license:
 *
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD AND BSD-2-Clause-NetBSD
 *
 * Copyright (c) 2001-2003, 2005 Shunsuke Akiyama <akiyama@jp.FreeBSD.org>.
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * -------------------------------------------------------------------------
 *
 * All other code covered under the base project license.
 *
 */

#ifndef USBH_PL2303_H
#define USBH_PL2303_H

#include "usb_cdc.h"

/* Different PL2303 IC types */
typedef enum {
    USBH_PL2303_TYPE_UNKNOWN = 0,
    USBH_PL2303_TYPE_PL2303,
    USBH_PL2303_TYPE_PL2303HX,
    USBH_PL2303_TYPE_PL2303HXD,
    USBH_PL2303_TYPE_PL2303HXN
} usbh_pl2303_type_t;

struct usbh_serial_pl2303 {
    struct usbh_hubport *hport;
    struct usb_endpoint_descriptor *bulkin;  /* Bulk IN endpoint */
    struct usb_endpoint_descriptor *bulkout; /* Bulk OUT endpoint */
    struct usbh_urb bulkout_urb;
    struct usbh_urb bulkin_urb;

    struct cdc_line_coding line_coding;

    usbh_pl2303_type_t chiptype;
    uint8_t intf;
    uint8_t minor;
};

#ifdef __cplusplus
extern "C" {
#endif

int usbh_serial_pl2303_set_line_coding(struct usbh_serial_pl2303 *pl2303_class, struct cdc_line_coding *line_coding);
int usbh_serial_pl2303_get_line_coding(struct usbh_serial_pl2303 *pl2303_class, struct cdc_line_coding *line_coding);
int usbh_serial_pl2303_set_line_state(struct usbh_serial_pl2303 *pl2303_class, bool dtr, bool rts);

int usbh_serial_pl2303_bulk_in_transfer(struct usbh_serial_pl2303 *pl2303_class, uint8_t *buffer, uint32_t buflen, uint32_t timeout);
int usbh_serial_pl2303_bulk_out_transfer(struct usbh_serial_pl2303 *pl2303_class, uint8_t *buffer, uint32_t buflen, uint32_t timeout);

void usbh_serial_pl2303_run(struct usbh_serial_pl2303 *pl2303_class);
void usbh_serial_pl2303_stop(struct usbh_serial_pl2303 *pl2303_class);

#ifdef __cplusplus
}
#endif

#endif /* USBH_PL2303_H */
