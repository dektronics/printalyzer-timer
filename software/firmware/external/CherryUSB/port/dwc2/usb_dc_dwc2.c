/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usbd_core.h"
#include "usb_dwc2_reg.h"
#include "usb_dwc2_param.h"

// clang-format off
#if   defined ( __CC_ARM )
#ifndef   __UNALIGNED_UINT32_WRITE
  #define __UNALIGNED_UINT32_WRITE(addr, val)    ((*((__packed uint32_t *)(addr))) = (val))
#endif
#ifndef   __UNALIGNED_UINT32_READ
  #define __UNALIGNED_UINT32_READ(addr)          (*((const __packed uint32_t *)(addr)))
#endif
#elif defined (__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050)
#ifndef   __UNALIGNED_UINT32_WRITE
  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wpacked"
/*lint -esym(9058, T_UINT32_WRITE)*/ /* disable MISRA 2012 Rule 2.4 for T_UINT32_WRITE */
  __PACKED_STRUCT T_UINT32_WRITE { uint32_t v; };
  #pragma clang diagnostic pop
  #define __UNALIGNED_UINT32_WRITE(addr, val)    (void)((((struct T_UINT32_WRITE *)(void *)(addr))->v) = (val))
#endif
#ifndef   __UNALIGNED_UINT32_READ
  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wpacked"
/*lint -esym(9058, T_UINT32_READ)*/ /* disable MISRA 2012 Rule 2.4 for T_UINT32_READ */
  __PACKED_STRUCT T_UINT32_READ { uint32_t v; };
  #pragma clang diagnostic pop
  #define __UNALIGNED_UINT32_READ(addr)          (((const struct T_UINT32_READ *)(const void *)(addr))->v)
#endif
#elif defined ( __GNUC__ )
#ifndef   __UNALIGNED_UINT32_WRITE
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wpacked"
  #pragma GCC diagnostic ignored "-Wattributes"
  __PACKED_STRUCT T_UINT32_WRITE { uint32_t v; };
  #pragma GCC diagnostic pop
  #define __UNALIGNED_UINT32_WRITE(addr, val)    (void)((((struct T_UINT32_WRITE *)(void *)(addr))->v) = (val))
#endif
#ifndef   __UNALIGNED_UINT32_READ
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wpacked"
  #pragma GCC diagnostic ignored "-Wattributes"
  __PACKED_STRUCT T_UINT32_READ { uint32_t v; };
  #pragma GCC diagnostic pop
  #define __UNALIGNED_UINT32_READ(addr)          (((const struct T_UINT32_READ *)(const void *)(addr))->v)
#endif
#endif
// clang-format on

#define USBD_BASE (g_usbdev_bus[busid].reg_base)

#define USB_OTG_GLB      ((DWC2_GlobalTypeDef *)(USBD_BASE))
#define USB_OTG_DEV      ((DWC2_DeviceTypeDef *)(USBD_BASE + USB_OTG_DEVICE_BASE))
#define USB_OTG_PCGCCTL  *(__IO uint32_t *)((uint32_t)USBD_BASE + USB_OTG_PCGCCTL_BASE)
#define USB_OTG_INEP(i)  ((DWC2_INEndpointTypeDef *)(USBD_BASE + USB_OTG_IN_ENDPOINT_BASE + ((i)*USB_OTG_EP_REG_SIZE)))
#define USB_OTG_OUTEP(i) ((DWC2_OUTEndpointTypeDef *)(USBD_BASE + USB_OTG_OUT_ENDPOINT_BASE + ((i)*USB_OTG_EP_REG_SIZE)))
#define USB_OTG_FIFO(i)  *(__IO uint32_t *)(USBD_BASE + USB_OTG_FIFO_BASE + ((i)*USB_OTG_FIFO_SIZE))

extern uint32_t SystemCoreClock;

/* Endpoint state */
struct dwc2_ep_state {
    uint16_t ep_mps;    /* Endpoint max packet size */
    uint8_t ep_type;    /* Endpoint type */
    uint8_t ep_stalled; /* Endpoint stall flag */
    uint8_t *xfer_buf;
    uint32_t xfer_len;
    uint32_t actual_xfer_len;
};

/* Driver state */
USB_NOCACHE_RAM_SECTION struct dwc2_udc {
    USB_MEM_ALIGNX struct usb_setup_packet setup;
    USB_MEM_ALIGNX uint8_t pad; /* Pad to CONFIG_USB_ALIGN_SIZE bytes */
    struct dwc2_hw_params hw_params;
    struct dwc2_user_params user_params;
    struct dwc2_ep_state in_ep[16];  /*!< IN endpoint parameters*/
    struct dwc2_ep_state out_ep[16]; /*!< OUT endpoint parameters */
} g_dwc2_udc[CONFIG_USBDEV_MAX_BUS];

static inline int dwc2_reset(uint8_t busid)
{
    volatile uint32_t count = 0U;

    /* Wait for AHB master IDLE state. */
    do {
        if (++count > 200000U) {
            return -1;
        }
    } while ((USB_OTG_GLB->GRSTCTL & USB_OTG_GRSTCTL_AHBIDL) == 0U);

    /* Core Soft Reset */
    count = 0U;
    USB_OTG_GLB->GRSTCTL |= USB_OTG_GRSTCTL_CSRST;

    if (g_dwc2_udc[busid].hw_params.snpsid < 0x4F54420AU) {
        do {
            if (++count > 200000U) {
                USB_LOG_ERR("DWC2 reset timeout\r\n");
                return -1;
            }
        } while ((USB_OTG_GLB->GRSTCTL & USB_OTG_GRSTCTL_CSRST) == USB_OTG_GRSTCTL_CSRST);
    } else {
        do {
            if (++count > 200000U) {
                USB_LOG_ERR("DWC2 reset timeout\r\n");
                return -1;
            }
        } while ((USB_OTG_GLB->GRSTCTL & USB_OTG_GRSTCTL_CSRSTDONE) != USB_OTG_GRSTCTL_CSRSTDONE);

        USB_OTG_GLB->GRSTCTL &= ~USB_OTG_GRSTCTL_CSRST;
        USB_OTG_GLB->GRSTCTL |= USB_OTG_GRSTCTL_CSRSTDONE;
    }

    return 0;
}

static inline int dwc2_core_init(uint8_t busid)
{
    int ret;
    uint32_t regval;

    if (g_dwc2_udc[busid].user_params.phy_type == DWC2_PHY_TYPE_PARAM_FS) {
        /* Select FS Embedded PHY */
        USB_OTG_GLB->GUSBCFG |= USB_OTG_GUSBCFG_PHYSEL;
    } else {
        regval = USB_OTG_GLB->GUSBCFG;
        regval &= ~USB_OTG_GUSBCFG_PHYSEL;
        /* disable external vbus source */
        regval &= ~(USB_OTG_GUSBCFG_ULPIEVBUSD | USB_OTG_GUSBCFG_ULPIEVBUSI);
        /* disable ULPI FS/LS */
        regval &= ~(USB_OTG_GUSBCFG_ULPIFSLS | USB_OTG_GUSBCFG_ULPICSM);

        switch (g_dwc2_udc[busid].user_params.phy_type) {
            case DWC2_PHY_TYPE_PARAM_ULPI:
                regval |= USB_OTG_GUSBCFG_ULPI_UTMI_SEL;
                regval &= ~USB_OTG_GUSBCFG_PHYIF16;
                regval &= ~USB_OTG_GUSBCFG_DDR_SEL;

                if (g_dwc2_udc[busid].user_params.phy_utmi_width == 16) {
                    regval |= USB_OTG_GUSBCFG_PHYIF16;
                }
                break;
            case DWC2_PHY_TYPE_PARAM_UTMI:
                regval &= ~USB_OTG_GUSBCFG_ULPI_UTMI_SEL;
                regval &= ~USB_OTG_GUSBCFG_PHYIF16;
                break;

            default:
                break;
        }
        USB_OTG_GLB->GUSBCFG = regval;
    }

    /* Reset after a PHY select */
    ret = dwc2_reset(busid);
    return ret;
}

static inline void dwc2_set_mode(uint8_t busid, uint8_t mode)
{
    USB_OTG_GLB->GUSBCFG &= ~(USB_OTG_GUSBCFG_FHMOD | USB_OTG_GUSBCFG_FDMOD);

    if (mode == USB_OTG_MODE_HOST) {
        USB_OTG_GLB->GUSBCFG |= USB_OTG_GUSBCFG_FHMOD;
    } else if (mode == USB_OTG_MODE_DEVICE) {
        USB_OTG_GLB->GUSBCFG |= USB_OTG_GUSBCFG_FDMOD;
    }

    usbd_dwc2_delay_ms(50);
}

static inline int dwc2_flush_rxfifo(uint8_t busid)
{
    volatile uint32_t count = 0U;

    /* Wait for AHB master IDLE state. */
    do {
        if (++count > 200000U) {
            return -1;
        }
    } while ((USB_OTG_GLB->GRSTCTL & USB_OTG_GRSTCTL_AHBIDL) == 0U);

    count = 0;
    USB_OTG_GLB->GRSTCTL = USB_OTG_GRSTCTL_RXFFLSH;

    do {
        if (++count > 200000U) {
            return -1;
        }
    } while ((USB_OTG_GLB->GRSTCTL & USB_OTG_GRSTCTL_RXFFLSH) == USB_OTG_GRSTCTL_RXFFLSH);

    return 0;
}

static inline int dwc2_flush_txfifo(uint8_t busid, uint32_t num)
{
    volatile uint32_t count = 0U;

    /* Wait for AHB master IDLE state. */
    do {
        if (++count > 200000U) {
            return -1;
        }
    } while ((USB_OTG_GLB->GRSTCTL & USB_OTG_GRSTCTL_AHBIDL) == 0U);

    count = 0;
    USB_OTG_GLB->GRSTCTL = (USB_OTG_GRSTCTL_TXFFLSH | (num << 6));

    do {
        if (++count > 200000U) {
            return -1;
        }
    } while ((USB_OTG_GLB->GRSTCTL & USB_OTG_GRSTCTL_TXFFLSH) == USB_OTG_GRSTCTL_TXFFLSH);

    return 0;
}

static void dwc2_set_turnaroundtime(uint8_t busid, uint32_t hclk, uint8_t speed)
{
    uint32_t UsbTrd;

    /* The USBTRD is configured according to the tables below, depending on AHB frequency
  used by application. In the low AHB frequency range it is used to stretch enough the USB response
  time to IN tokens, the USB turnaround time, so to compensate for the longer AHB read access
  latency to the Data FIFO */
    if (speed == USB_OTG_SPEED_FULL) {
        if ((hclk >= 14200000U) && (hclk < 15000000U)) {
            /* hclk Clock Range between 14.2-15 MHz */
            UsbTrd = 0xFU;
        } else if ((hclk >= 15000000U) && (hclk < 16000000U)) {
            /* hclk Clock Range between 15-16 MHz */
            UsbTrd = 0xEU;
        } else if ((hclk >= 16000000U) && (hclk < 17200000U)) {
            /* hclk Clock Range between 16-17.2 MHz */
            UsbTrd = 0xDU;
        } else if ((hclk >= 17200000U) && (hclk < 18500000U)) {
            /* hclk Clock Range between 17.2-18.5 MHz */
            UsbTrd = 0xCU;
        } else if ((hclk >= 18500000U) && (hclk < 20000000U)) {
            /* hclk Clock Range between 18.5-20 MHz */
            UsbTrd = 0xBU;
        } else if ((hclk >= 20000000U) && (hclk < 21800000U)) {
            /* hclk Clock Range between 20-21.8 MHz */
            UsbTrd = 0xAU;
        } else if ((hclk >= 21800000U) && (hclk < 24000000U)) {
            /* hclk Clock Range between 21.8-24 MHz */
            UsbTrd = 0x9U;
        } else if ((hclk >= 24000000U) && (hclk < 27700000U)) {
            /* hclk Clock Range between 24-27.7 MHz */
            UsbTrd = 0x8U;
        } else if ((hclk >= 27700000U) && (hclk < 32000000U)) {
            /* hclk Clock Range between 27.7-32 MHz */
            UsbTrd = 0x7U;
        } else /* if(hclk >= 32000000) */
        {
            /* hclk Clock Range between 32-200 MHz */
            UsbTrd = 0x6U;
        }
    } else if (speed == USB_OTG_SPEED_HIGH) {
        UsbTrd = USBD_HS_TRDT_VALUE;
    } else {
        UsbTrd = USBD_DEFAULT_TRDT_VALUE;
    }

    USB_OTG_GLB->GUSBCFG &= ~USB_OTG_GUSBCFG_TRDT;
    USB_OTG_GLB->GUSBCFG |= (uint32_t)((UsbTrd << USB_OTG_GUSBCFG_TRDT_Pos) & USB_OTG_GUSBCFG_TRDT);
}

static void dwc2_set_txfifo(uint8_t busid, uint8_t fifo, uint16_t size)
{
    uint8_t i;
    uint32_t tx_offset;

    /*  TXn min size = 16 words. (n  : Transmit FIFO index)
      When a TxFIFO is not used, the Configuration should be as follows:
          case 1 :  n > m    and Txn is not used    (n,m  : Transmit FIFO indexes)
         --> Txm can use the space allocated for Txn.
         case2  :  n < m    and Txn is not used    (n,m  : Transmit FIFO indexes)
         --> Txn should be configured with the minimum space of 16 words
     The FIFO is used optimally when used TxFIFOs are allocated in the top
         of the FIFO.Ex: use EP1 and EP2 as IN instead of EP1 and EP3 as IN ones.
     When DMA is used 3n * FIFO locations should be reserved for internal DMA registers */

    tx_offset = USB_OTG_GLB->GRXFSIZ;

    if (fifo == 0U) {
        USB_OTG_GLB->DIEPTXF0_HNPTXFSIZ = ((uint32_t)size << 16) | tx_offset;
    } else {
        tx_offset += (USB_OTG_GLB->DIEPTXF0_HNPTXFSIZ) >> 16;
        for (i = 0U; i < (fifo - 1U); i++) {
            tx_offset += (USB_OTG_GLB->DIEPTXF[i] >> 16);
        }

        /* Multiply Tx_Size by 2 to get higher performance */
        USB_OTG_GLB->DIEPTXF[fifo - 1U] = ((uint32_t)size << 16) | tx_offset;
    }

    USB_LOG_INFO("fifo%d size:%04x, offset:%04x\r\n", fifo, size, tx_offset);
}

static uint8_t dwc2_get_devspeed(uint8_t busid)
{
    uint8_t speed;
    uint32_t DevEnumSpeed = USB_OTG_DEV->DSTS & USB_OTG_DSTS_ENUMSPD;

    if (DevEnumSpeed == DSTS_ENUMSPD_HS_PHY_30MHZ_OR_60MHZ) {
        speed = USB_OTG_SPEED_HIGH;
    } else if ((DevEnumSpeed == DSTS_ENUMSPD_FS_PHY_30MHZ_OR_60MHZ) ||
               (DevEnumSpeed == DSTS_ENUMSPD_FS_PHY_48MHZ)) {
        speed = USB_OTG_SPEED_FULL;
    } else {
        speed = 0xFU;
    }

    return speed;
}

static void dwc2_ep0_start_read_setup(uint8_t busid, uint8_t *psetup)
{
    USB_OTG_OUTEP(0U)->DOEPTSIZ = 0U;
    USB_OTG_OUTEP(0U)->DOEPTSIZ |= (USB_OTG_DOEPTSIZ_PKTCNT & (1U << 19));
    USB_OTG_OUTEP(0U)->DOEPTSIZ |= (3U * 8U);
    USB_OTG_OUTEP(0U)->DOEPTSIZ |= USB_OTG_DOEPTSIZ_STUPCNT;

    if (g_dwc2_udc[busid].user_params.device_dma_enable) {
        USB_OTG_OUTEP(0U)->DOEPDMA = (uint32_t)psetup;
        /* EP enable */
        USB_OTG_OUTEP(0U)->DOEPCTL |= USB_OTG_DOEPCTL_EPENA | USB_OTG_DOEPCTL_USBAEP;
    }
}

void dwc2_ep_write(uint8_t busid, uint8_t ep_idx, uint8_t *src, uint16_t len)
{
    uint32_t *pSrc = (uint32_t *)src;
    uint32_t count32b, i;

    count32b = ((uint32_t)len + 3U) / 4U;
    for (i = 0U; i < count32b; i++) {
        USB_OTG_FIFO((uint32_t)ep_idx) = __UNALIGNED_UINT32_READ(pSrc);
        pSrc++;
    }
}

void dwc2_ep_read(uint8_t busid, uint8_t *dest, uint16_t len)
{
    uint32_t *pDest = (uint32_t *)dest;
    uint32_t i;
    uint32_t count32b = ((uint32_t)len + 3U) / 4U;

    for (i = 0U; i < count32b; i++) {
        __UNALIGNED_UINT32_WRITE(pDest, USB_OTG_FIFO(0U));
        pDest++;
    }
}

static void dwc2_tx_fifo_empty_procecss(uint8_t busid, uint8_t ep_idx)
{
    uint32_t len;
    uint32_t len32b;
    uint32_t fifoemptymsk;

    len = g_dwc2_udc[busid].in_ep[ep_idx].xfer_len - g_dwc2_udc[busid].in_ep[ep_idx].actual_xfer_len;
    if (len > g_dwc2_udc[busid].in_ep[ep_idx].ep_mps) {
        len = g_dwc2_udc[busid].in_ep[ep_idx].ep_mps;
    }

    len32b = (len + 3U) / 4U;

    while (((USB_OTG_INEP(ep_idx)->DTXFSTS & USB_OTG_DTXFSTS_INEPTFSAV) >= len32b) &&
           (g_dwc2_udc[busid].in_ep[ep_idx].actual_xfer_len < g_dwc2_udc[busid].in_ep[ep_idx].xfer_len) && (g_dwc2_udc[busid].in_ep[ep_idx].xfer_len != 0U)) {
        /* Write the FIFO */
        len = g_dwc2_udc[busid].in_ep[ep_idx].xfer_len - g_dwc2_udc[busid].in_ep[ep_idx].actual_xfer_len;
        if (len > g_dwc2_udc[busid].in_ep[ep_idx].ep_mps) {
            len = g_dwc2_udc[busid].in_ep[ep_idx].ep_mps;
        }
        if (g_dwc2_udc[busid].in_ep[ep_idx].ep_type == USB_ENDPOINT_TYPE_ISOCHRONOUS) {
            if ((USB_OTG_DEV->DSTS & (1U << 8)) == 0U) {
                USB_OTG_INEP(ep_idx)->DIEPCTL &= ~USB_OTG_DIEPCTL_SD0PID_SEVNFRM;
                USB_OTG_INEP(ep_idx)->DIEPCTL |= USB_OTG_DIEPCTL_SODDFRM;
            } else {
                USB_OTG_INEP(ep_idx)->DIEPCTL &= ~USB_OTG_DIEPCTL_SODDFRM;
                USB_OTG_INEP(ep_idx)->DIEPCTL |= USB_OTG_DIEPCTL_SD0PID_SEVNFRM;
            }
            USB_OTG_INEP(ep_idx)->DIEPTSIZ &= ~(USB_OTG_DIEPTSIZ_MULCNT);
            USB_OTG_INEP(ep_idx)->DIEPTSIZ |= (USB_OTG_DIEPTSIZ_MULCNT & (1U << 29));
        }

        dwc2_ep_write(busid, ep_idx, g_dwc2_udc[busid].in_ep[ep_idx].xfer_buf, len);
        g_dwc2_udc[busid].in_ep[ep_idx].xfer_buf += len;
        g_dwc2_udc[busid].in_ep[ep_idx].actual_xfer_len += len;
    }

    if (g_dwc2_udc[busid].in_ep[ep_idx].xfer_len <= g_dwc2_udc[busid].in_ep[ep_idx].actual_xfer_len) {
        fifoemptymsk = (uint32_t)(0x1UL << (ep_idx & 0x0f));
        USB_OTG_DEV->DIEPEMPMSK &= ~fifoemptymsk;
    }
}

/**
  * @brief  dwc2_get_glb_intstatus: return the global USB interrupt status
  * @retval status
  */
static inline uint32_t dwc2_get_glb_intstatus(uint8_t busid)
{
    uint32_t tmpreg;

    tmpreg = USB_OTG_GLB->GINTSTS;
    tmpreg &= USB_OTG_GLB->GINTMSK;

    return tmpreg;
}

/**
  * @brief  dwc2_get_outeps_intstatus: return the USB device OUT endpoints interrupt status
  * @retval status
  */
static inline uint32_t dwc2_get_outeps_intstatus(uint8_t busid)
{
    uint32_t tmpreg;

    tmpreg = USB_OTG_DEV->DAINT;
    tmpreg &= USB_OTG_DEV->DAINTMSK;

    return ((tmpreg & 0xffff0000U) >> 16);
}

/**
  * @brief  dwc2_get_ineps_intstatus: return the USB device IN endpoints interrupt status
  * @retval status
  */
static inline uint32_t dwc2_get_ineps_intstatus(uint8_t busid)
{
    uint32_t tmpreg;

    tmpreg = USB_OTG_DEV->DAINT;
    tmpreg &= USB_OTG_DEV->DAINTMSK;

    return ((tmpreg & 0xFFFFU));
}

/**
  * @brief  Returns Device OUT EP Interrupt register
  * @param  epnum  endpoint number
  *          This parameter can be a value from 0 to 15
  * @retval Device OUT EP Interrupt register
  */
static inline uint32_t dwc2_get_outep_intstatus(uint8_t busid, uint8_t epnum)
{
    uint32_t tmpreg;

    tmpreg = USB_OTG_OUTEP((uint32_t)epnum)->DOEPINT;
    USB_OTG_OUTEP((uint32_t)epnum)->DOEPINT = tmpreg;
    tmpreg = tmpreg & USB_OTG_DEV->DOEPMSK;

    return tmpreg;
}

/**
  * @brief  Returns Device IN EP Interrupt register
  * @param  epnum  endpoint number
  *          This parameter can be a value from 0 to 15
  * @retval Device IN EP Interrupt register
  */
static inline uint32_t dwc2_get_inep_intstatus(uint8_t busid, uint8_t epnum)
{
    uint32_t tmpreg, msk, emp;

    msk = USB_OTG_DEV->DIEPMSK;
    emp = USB_OTG_DEV->DIEPEMPMSK;
    msk |= ((emp >> (epnum & 0x0F)) & 0x1U) << 7;

    tmpreg = USB_OTG_INEP((uint32_t)epnum)->DIEPINT;
    USB_OTG_INEP((uint32_t)epnum)->DIEPINT = tmpreg;
    tmpreg = tmpreg & msk;

    return tmpreg;
}

int usb_dc_init(uint8_t busid)
{
    int ret;
    uint32_t fifo_num;

    memset(&g_dwc2_udc[busid], 0, sizeof(struct dwc2_udc));

    usb_dc_low_level_init(busid);

    USB_LOG_INFO("========== dwc2 dcd params ==========\r\n");
    USB_LOG_INFO("CID:%08x\r\n", (unsigned int)USB_OTG_GLB->CID);
    USB_LOG_INFO("GSNPSID:%08x\r\n", (unsigned int)USB_OTG_GLB->GSNPSID);
    USB_LOG_INFO("GHWCFG1:%08x\r\n", (unsigned int)USB_OTG_GLB->GHWCFG1);
    USB_LOG_INFO("GHWCFG2:%08x\r\n", (unsigned int)USB_OTG_GLB->GHWCFG2);
    USB_LOG_INFO("GHWCFG3:%08x\r\n", (unsigned int)USB_OTG_GLB->GHWCFG3);
    USB_LOG_INFO("GHWCFG4:%08x\r\n", (unsigned int)USB_OTG_GLB->GHWCFG4);

    dwc2_get_hwparams(USBD_BASE, &g_dwc2_udc[busid].hw_params);
    dwc2_get_user_params(USBD_BASE, &g_dwc2_udc[busid].user_params);

    if (g_dwc2_udc[busid].user_params.phy_utmi_width == 0) {
        g_dwc2_udc[busid].user_params.phy_utmi_width = 8;
    }
    if (g_dwc2_udc[busid].user_params.total_fifo_size == 0) {
        g_dwc2_udc[busid].user_params.total_fifo_size = g_dwc2_udc[busid].hw_params.total_fifo_size;
    }

    USB_LOG_INFO("dwc2 has %d endpoints and dfifo depth(32-bit words) is %d\r\n",
                 g_dwc2_udc[busid].hw_params.num_dev_ep + 1,
                 g_dwc2_udc[busid].user_params.total_fifo_size);

    USB_OTG_GLB->GAHBCFG &= ~USB_OTG_GAHBCFG_GINT;

    USB_OTG_DEV->DCTL |= USB_OTG_DCTL_SDIS;

    /* This is vendor register */
    USB_OTG_GLB->GCCFG = g_dwc2_udc[busid].user_params.device_gccfg;

    ret = dwc2_core_init(busid);

    /* Force Device Mode*/
    dwc2_set_mode(busid, USB_OTG_MODE_DEVICE);

    if (g_dwc2_udc[busid].user_params.b_session_valid_override) {
        /* B-peripheral session valid override enable */
        USB_OTG_GLB->GOTGCTL |= USB_OTG_GOTGCTL_BVALOEN;
        USB_OTG_GLB->GOTGCTL |= USB_OTG_GOTGCTL_BVALOVAL;
    }

    USB_OTG_GLB->GUSBCFG |= USB_OTG_GUSBCFG_TOCAL;

    for (uint8_t i = 0U; i < 15U; i++) {
        USB_OTG_GLB->DIEPTXF[i] = 0U;
    }

    /* Restart the Phy Clock */
    USB_OTG_PCGCCTL = 0U;

    /* Device speed configuration */
    USB_OTG_DEV->DCFG &= ~USB_OTG_DCFG_DSPD;

    if (g_dwc2_udc[busid].user_params.phy_type != DWC2_PHY_TYPE_PARAM_FS) {
        USB_ASSERT_MSG(g_dwc2_udc[busid].hw_params.hs_phy_type != 0, "This dwc2 version does not support hs, so stop working");
        USB_OTG_DEV->DCFG |= USB_OTG_SPEED_HIGH;
    } else {
        if (g_dwc2_udc[busid].hw_params.hs_phy_type == 0) {
            USB_OTG_DEV->DCFG |= USB_OTG_SPEED_FULL;
        } else {
            USB_OTG_DEV->DCFG |= USB_OTG_SPEED_HIGH_IN_FULL;
        }
    }

    /* Clear all pending Device Interrupts */
    USB_OTG_DEV->DIEPMSK = 0U;
    USB_OTG_DEV->DOEPMSK = 0U;
    USB_OTG_DEV->DAINTMSK = 0U;

    /* Disable all interrupts. */
    USB_OTG_GLB->GINTMSK = 0U;
    /* Clear any pending interrupts */
    USB_OTG_GLB->GINTSTS = 0xBFFFFFFFU;

    /* Enable interrupts matching to the Device mode ONLY */
    USB_OTG_GLB->GINTMSK = USB_OTG_GINTMSK_USBRST | USB_OTG_GINTMSK_ENUMDNEM |
                           USB_OTG_GINTMSK_OEPINT | USB_OTG_GINTMSK_IEPINT |
                           USB_OTG_GINTMSK_USBSUSPM | USB_OTG_GINTMSK_WUIM;

    if (g_dwc2_udc[busid].user_params.device_dma_enable) {
        USB_ASSERT_MSG(g_dwc2_udc[busid].hw_params.arch == GHWCFG2_INT_DMA_ARCH, "This dwc2 version does not support dma mode, so stop working");

        USB_OTG_DEV->DCFG &= ~USB_OTG_DCFG_DESCDMA;
        USB_OTG_GLB->GAHBCFG &= ~USB_OTG_GAHBCFG_HBSTLEN;
        USB_OTG_GLB->GAHBCFG |= (USB_OTG_GAHBCFG_DMAEN | USB_OTG_GAHBCFG_HBSTLEN_4);
    } else {
        USB_OTG_GLB->GINTMSK |= USB_OTG_GINTMSK_RXFLVLM;
    }

#ifdef CONFIG_USBDEV_SOF_ENABLE
    USB_OTG_GLB->GINTMSK |= USB_OTG_GINTMSK_SOFM;
#endif

    USB_OTG_GLB->GRXFSIZ = g_dwc2_udc[busid].user_params.device_rx_fifo_size;

    fifo_num = g_dwc2_udc[busid].user_params.device_rx_fifo_size;
    for (uint8_t i = 0; i < (g_dwc2_udc[busid].hw_params.num_dev_ep + 1); i++) {
        dwc2_set_txfifo(busid, i, g_dwc2_udc[busid].user_params.device_tx_fifo_size[i]);
        fifo_num += g_dwc2_udc[busid].user_params.device_tx_fifo_size[i];

        USB_ASSERT_MSG(fifo_num <= g_dwc2_udc[busid].user_params.total_fifo_size, "Your fifo config is overflow, please check");
    }

    if (g_dwc2_udc[busid].user_params.phy_type != DWC2_PHY_TYPE_PARAM_FS) {
        USB_ASSERT_MSG(g_dwc2_udc[busid].user_params.device_rx_fifo_size >= (5 + 8 + 512 / 4 + 1 + 2 * 8 + 1), "Your rx fifo size config is invalid, please check");
    } else {
        USB_ASSERT_MSG(g_dwc2_udc[busid].user_params.device_rx_fifo_size >= (5 + 8 + 64 / 4 + 1 + 2 * 8 + 1), "Your rx fifo size config is invalid, please check");
    }

    ret = dwc2_flush_txfifo(busid, 0x10U);
    ret = dwc2_flush_rxfifo(busid);

    USB_OTG_GLB->GAHBCFG |= USB_OTG_GAHBCFG_GINT;
    USB_OTG_DEV->DCTL &= ~USB_OTG_DCTL_SDIS;

    return ret;
}

int usb_dc_deinit(uint8_t busid)
{
    USB_OTG_GLB->GAHBCFG |= USB_OTG_GAHBCFG_GINT;
    USB_OTG_DEV->DCTL |= USB_OTG_DCTL_SDIS;

    /* Clear Pending interrupt */
    for (uint8_t i = 0U; i < 15U; i++) {
        USB_OTG_INEP(i)->DIEPINT = 0xFB7FU;
        USB_OTG_OUTEP(i)->DOEPINT = 0xFB7FU;
    }

    /* Clear interrupt masks */
    USB_OTG_DEV->DIEPMSK = 0U;
    USB_OTG_DEV->DOEPMSK = 0U;
    USB_OTG_DEV->DAINTMSK = 0U;

    /* Flush the FIFO */
    dwc2_flush_txfifo(busid, 0x10U);
    dwc2_flush_rxfifo(busid);

    usb_dc_low_level_deinit(busid);
    return 0;
}

int usbd_set_address(uint8_t busid, const uint8_t addr)
{
    USB_OTG_DEV->DCFG &= ~(USB_OTG_DCFG_DAD);
    USB_OTG_DEV->DCFG |= ((uint32_t)addr << 4) & USB_OTG_DCFG_DAD;
    return 0;
}

int usbd_set_remote_wakeup(uint8_t busid)
{
    if (!(USB_OTG_DEV->DSTS & USB_OTG_DSTS_SUSPSTS)) {
        return -1;
    }
    USB_OTG_DEV->DCTL |= USB_OTG_DCTL_RWUSIG;
    usbd_dwc2_delay_ms(10);
    USB_OTG_DEV->DCTL &= ~USB_OTG_DCTL_RWUSIG;
    return 0;
}

uint8_t usbd_get_port_speed(uint8_t busid)
{
    uint8_t speed;
    uint32_t DevEnumSpeed = USB_OTG_DEV->DSTS & USB_OTG_DSTS_ENUMSPD;

    if (DevEnumSpeed == DSTS_ENUMSPD_HS_PHY_30MHZ_OR_60MHZ) {
        speed = USB_SPEED_HIGH;
    } else if ((DevEnumSpeed == DSTS_ENUMSPD_FS_PHY_30MHZ_OR_60MHZ) ||
               (DevEnumSpeed == DSTS_ENUMSPD_FS_PHY_48MHZ)) {
        speed = USB_SPEED_FULL;
    } else {
        speed = USB_SPEED_FULL;
    }

    return speed;
}

int usbd_ep_open(uint8_t busid, const struct usb_endpoint_descriptor *ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep->bEndpointAddress);

    USB_ASSERT_MSG(ep_idx < (g_dwc2_udc[busid].hw_params.num_dev_ep + 1), "Ep addr %02x overflow", ep->bEndpointAddress);

    if (USB_EP_DIR_IS_OUT(ep->bEndpointAddress)) {
        g_dwc2_udc[busid].out_ep[ep_idx].ep_mps = USB_GET_MAXPACKETSIZE(ep->wMaxPacketSize);
        g_dwc2_udc[busid].out_ep[ep_idx].ep_type = USB_GET_ENDPOINT_TYPE(ep->bmAttributes);

        USB_OTG_DEV->DAINTMSK |= USB_OTG_DAINTMSK_OEPM & (uint32_t)(1UL << (16 + ep_idx));

        if ((USB_OTG_OUTEP(ep_idx)->DOEPCTL & USB_OTG_DOEPCTL_USBAEP) == 0) {
            USB_OTG_OUTEP(ep_idx)->DOEPCTL |= (USB_GET_MAXPACKETSIZE(ep->wMaxPacketSize) & USB_OTG_DOEPCTL_MPSIZ) |
                                              ((uint32_t)USB_GET_ENDPOINT_TYPE(ep->bmAttributes) << 18) |
                                              USB_OTG_DIEPCTL_SD0PID_SEVNFRM |
                                              USB_OTG_DOEPCTL_USBAEP;
        }
    } else {
        uint16_t fifo_size;
        if (ep_idx == 0) {
            fifo_size = (USB_OTG_GLB->DIEPTXF0_HNPTXFSIZ >> 16);
        } else {
            fifo_size = (USB_OTG_GLB->DIEPTXF[ep_idx - 1U] >> 16);
        }

        USB_ASSERT_MSG((fifo_size * 4) >= USB_GET_MAXPACKETSIZE(ep->wMaxPacketSize), "Ep addr %02x fifo overflow", ep->bEndpointAddress);

        g_dwc2_udc[busid].in_ep[ep_idx].ep_mps = USB_GET_MAXPACKETSIZE(ep->wMaxPacketSize);
        g_dwc2_udc[busid].in_ep[ep_idx].ep_type = USB_GET_ENDPOINT_TYPE(ep->bmAttributes);

        USB_OTG_DEV->DAINTMSK |= USB_OTG_DAINTMSK_IEPM & (uint32_t)(1UL << ep_idx);

        if ((USB_OTG_INEP(ep_idx)->DIEPCTL & USB_OTG_DIEPCTL_USBAEP) == 0) {
            USB_OTG_INEP(ep_idx)->DIEPCTL |= (USB_GET_MAXPACKETSIZE(ep->wMaxPacketSize) & USB_OTG_DIEPCTL_MPSIZ) |
                                             ((uint32_t)USB_GET_ENDPOINT_TYPE(ep->bmAttributes) << 18) | (ep_idx << 22) |
                                             USB_OTG_DIEPCTL_SD0PID_SEVNFRM |
                                             USB_OTG_DIEPCTL_USBAEP;
        }
        dwc2_flush_txfifo(busid, ep_idx);
    }
    return 0;
}

int usbd_ep_close(uint8_t busid, const uint8_t ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    volatile uint32_t count = 0U;

    if (USB_EP_DIR_IS_OUT(ep)) {
        if (USB_OTG_OUTEP(ep_idx)->DOEPCTL & USB_OTG_DOEPCTL_EPENA) {
            USB_OTG_OUTEP(ep_idx)->DOEPCTL |= USB_OTG_DOEPCTL_SNAK;
            USB_OTG_OUTEP(ep_idx)->DOEPCTL |= USB_OTG_DOEPCTL_EPDIS;

            /* Wait for endpoint disabled interrupt */
            count = 0;
            do {
                if (++count > 50000) {
                    break;
                }
            } while ((USB_OTG_OUTEP(ep_idx)->DOEPINT & USB_OTG_DOEPINT_EPDISD) != USB_OTG_DOEPINT_EPDISD);

            /* Clear and unmask endpoint disabled interrupt */
            USB_OTG_OUTEP(ep_idx)->DOEPINT = USB_OTG_DOEPINT_EPDISD;
        }

        USB_OTG_DEV->DEACHMSK &= ~(USB_OTG_DAINTMSK_OEPM & ((uint32_t)(1UL << (ep_idx & 0x07)) << 16));
        USB_OTG_DEV->DAINTMSK &= ~(USB_OTG_DAINTMSK_OEPM & ((uint32_t)(1UL << (ep_idx & 0x07)) << 16));
        USB_OTG_OUTEP(ep_idx)->DOEPCTL = 0;
    } else {
        if (USB_OTG_INEP(ep_idx)->DIEPCTL & USB_OTG_DIEPCTL_EPENA) {
            USB_OTG_INEP(ep_idx)->DIEPCTL |= USB_OTG_DIEPCTL_SNAK;
            USB_OTG_INEP(ep_idx)->DIEPCTL |= USB_OTG_DIEPCTL_EPDIS;

            /* Wait for endpoint disabled interrupt */
            count = 0;
            do {
                if (++count > 50000) {
                    break;
                }
            } while ((USB_OTG_INEP(ep_idx)->DIEPINT & USB_OTG_DIEPINT_EPDISD) != USB_OTG_DIEPINT_EPDISD);

            /* Clear and unmask endpoint disabled interrupt */
            USB_OTG_INEP(ep_idx)->DIEPINT = USB_OTG_DIEPINT_EPDISD;
        }

        USB_OTG_DEV->DEACHMSK &= ~(USB_OTG_DAINTMSK_IEPM & (uint32_t)(1UL << (ep_idx & 0x07)));
        USB_OTG_DEV->DAINTMSK &= ~(USB_OTG_DAINTMSK_IEPM & (uint32_t)(1UL << (ep_idx & 0x07)));
        USB_OTG_INEP(ep_idx)->DIEPCTL = 0;
    }
    return 0;
}

int usbd_ep_set_stall(uint8_t busid, const uint8_t ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);

    if (USB_EP_DIR_IS_OUT(ep)) {
        if (((USB_OTG_OUTEP(ep_idx)->DOEPCTL & USB_OTG_DOEPCTL_EPENA) == 0U) && (ep_idx != 0U)) {
            USB_OTG_OUTEP(ep_idx)->DOEPCTL &= ~(USB_OTG_DOEPCTL_EPDIS);
        }
        USB_OTG_OUTEP(ep_idx)->DOEPCTL |= USB_OTG_DOEPCTL_STALL;
    } else {
        if (((USB_OTG_INEP(ep_idx)->DIEPCTL & USB_OTG_DIEPCTL_EPENA) == 0U) && (ep_idx != 0U)) {
            USB_OTG_INEP(ep_idx)->DIEPCTL &= ~(USB_OTG_DIEPCTL_EPDIS);
        }
        USB_OTG_INEP(ep_idx)->DIEPCTL |= USB_OTG_DIEPCTL_STALL;
    }

    if ((ep_idx == 0) && g_dwc2_udc[busid].user_params.device_dma_enable) {
        dwc2_ep0_start_read_setup(busid, (uint8_t *)&g_dwc2_udc[busid].setup);
    }

    return 0;
}

int usbd_ep_clear_stall(uint8_t busid, const uint8_t ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);

    if (USB_EP_DIR_IS_OUT(ep)) {
        USB_OTG_OUTEP(ep_idx)->DOEPCTL &= ~USB_OTG_DOEPCTL_STALL;
        if ((g_dwc2_udc[busid].out_ep[ep_idx].ep_type == USB_ENDPOINT_TYPE_INTERRUPT) ||
            (g_dwc2_udc[busid].out_ep[ep_idx].ep_type == USB_ENDPOINT_TYPE_BULK)) {
            USB_OTG_OUTEP(ep_idx)->DOEPCTL |= USB_OTG_DOEPCTL_SD0PID_SEVNFRM; /* DATA0 */
        }
    } else {
        USB_OTG_INEP(ep_idx)->DIEPCTL &= ~USB_OTG_DIEPCTL_STALL;
        if ((g_dwc2_udc[busid].in_ep[ep_idx].ep_type == USB_ENDPOINT_TYPE_INTERRUPT) ||
            (g_dwc2_udc[busid].in_ep[ep_idx].ep_type == USB_ENDPOINT_TYPE_BULK)) {
            USB_OTG_INEP(ep_idx)->DIEPCTL |= USB_OTG_DIEPCTL_SD0PID_SEVNFRM; /* DATA0 */
        }
    }
    return 0;
}

int usbd_ep_is_stalled(uint8_t busid, const uint8_t ep, uint8_t *stalled)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);

    if (USB_EP_DIR_IS_OUT(ep)) {
        if (USB_OTG_OUTEP(ep_idx)->DOEPCTL & USB_OTG_DOEPCTL_STALL) {
            *stalled = 1;
        } else {
            *stalled = 0;
        }
    } else {
        if (USB_OTG_INEP(ep_idx)->DIEPCTL & USB_OTG_DIEPCTL_STALL) {
            *stalled = 1;
        } else {
            *stalled = 0;
        }
    }
    return 0;
}

int usbd_ep_start_write(uint8_t busid, const uint8_t ep, const uint8_t *data, uint32_t data_len)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    uint32_t pktcnt = 0;

    USB_ASSERT_MSG(!((uint32_t)data % 0x04), "dwc2 data must be 4-byte aligned");

    if (g_dwc2_udc[busid].user_params.device_dma_enable) {
        USB_ASSERT_MSG(!((uint32_t)data % CONFIG_USB_ALIGN_SIZE), "dwc2 data must be %d-byte aligned", CONFIG_USB_ALIGN_SIZE);
    }

    if (!data && data_len) {
        return -1;
    }

    if (ep_idx && !(USB_OTG_INEP(ep_idx)->DIEPCTL & USB_OTG_DIEPCTL_MPSIZ)) {
        return -2;
    }

    g_dwc2_udc[busid].in_ep[ep_idx].xfer_buf = (uint8_t *)data;
    g_dwc2_udc[busid].in_ep[ep_idx].xfer_len = data_len;
    g_dwc2_udc[busid].in_ep[ep_idx].actual_xfer_len = 0;

    USB_OTG_INEP(ep_idx)->DIEPTSIZ &= ~(USB_OTG_DIEPTSIZ_PKTCNT);
    USB_OTG_INEP(ep_idx)->DIEPTSIZ &= ~(USB_OTG_DIEPTSIZ_XFRSIZ);

    if (data_len == 0) {
        USB_OTG_INEP(ep_idx)->DIEPTSIZ |= (USB_OTG_DIEPTSIZ_PKTCNT & (1U << 19));
        USB_OTG_INEP(ep_idx)->DIEPCTL |= (USB_OTG_DIEPCTL_CNAK | USB_OTG_DIEPCTL_EPENA);
        return 0;
    }

    if (ep_idx == 0) {
        if (data_len > g_dwc2_udc[busid].in_ep[ep_idx].ep_mps) {
            data_len = g_dwc2_udc[busid].in_ep[ep_idx].ep_mps;
        }
        g_dwc2_udc[busid].in_ep[ep_idx].xfer_len = data_len;
        USB_OTG_INEP(ep_idx)->DIEPTSIZ |= (USB_OTG_DIEPTSIZ_PKTCNT & (1U << 19));
        USB_OTG_INEP(ep_idx)->DIEPTSIZ |= (USB_OTG_DIEPTSIZ_XFRSIZ & data_len);
    } else {
        pktcnt = (uint16_t)((data_len + g_dwc2_udc[busid].in_ep[ep_idx].ep_mps - 1U) / g_dwc2_udc[busid].in_ep[ep_idx].ep_mps);

        USB_OTG_INEP(ep_idx)->DIEPTSIZ |= (USB_OTG_DIEPTSIZ_PKTCNT & (pktcnt << 19));
        USB_OTG_INEP(ep_idx)->DIEPTSIZ |= (USB_OTG_DIEPTSIZ_XFRSIZ & data_len);
    }

    if (g_dwc2_udc[busid].in_ep[ep_idx].ep_type == USB_ENDPOINT_TYPE_ISOCHRONOUS) {
        if ((USB_OTG_DEV->DSTS & (1U << 8)) == 0U) {
            USB_OTG_INEP(ep_idx)->DIEPCTL &= ~USB_OTG_DIEPCTL_SD0PID_SEVNFRM;
            USB_OTG_INEP(ep_idx)->DIEPCTL |= USB_OTG_DIEPCTL_SODDFRM;
        } else {
            USB_OTG_INEP(ep_idx)->DIEPCTL &= ~USB_OTG_DIEPCTL_SODDFRM;
            USB_OTG_INEP(ep_idx)->DIEPCTL |= USB_OTG_DIEPCTL_SD0PID_SEVNFRM;
        }
    }

    if (g_dwc2_udc[busid].in_ep[ep_idx].ep_type == USB_ENDPOINT_TYPE_ISOCHRONOUS ||
        g_dwc2_udc[busid].in_ep[ep_idx].ep_type == USB_ENDPOINT_TYPE_INTERRUPT) {
        USB_OTG_INEP(ep_idx)->DIEPTSIZ &= ~(USB_OTG_DIEPTSIZ_MULCNT);
        USB_OTG_INEP(ep_idx)->DIEPTSIZ |= (USB_OTG_DIEPTSIZ_MULCNT & ((usbd_get_ep_mult(busid, ep) + 1) << 29));
    }

    if (g_dwc2_udc[busid].user_params.device_dma_enable) {
        usb_dcache_clean((uintptr_t)data, USB_ALIGN_UP(data_len, CONFIG_USB_ALIGN_SIZE));
        USB_OTG_INEP(ep_idx)->DIEPDMA = (uint32_t)data;

        USB_OTG_INEP(ep_idx)->DIEPCTL |= (USB_OTG_DIEPCTL_CNAK | USB_OTG_DIEPCTL_EPENA);
    } else {
        USB_OTG_INEP(ep_idx)->DIEPCTL |= (USB_OTG_DIEPCTL_CNAK | USB_OTG_DIEPCTL_EPENA);
        /* Enable the Tx FIFO Empty Interrupt for this EP */
        if (data_len > 0U) {
            USB_OTG_DEV->DIEPEMPMSK |= 1UL << (ep_idx & 0x0f);
        }
    }
    return 0;
}

int usbd_ep_start_read(uint8_t busid, const uint8_t ep, uint8_t *data, uint32_t data_len)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    uint32_t pktcnt = 0;

    USB_ASSERT_MSG(!((uint32_t)data % 0x04), "dwc2 data must be 4-byte aligned");

    if (g_dwc2_udc[busid].user_params.device_dma_enable) {
        USB_ASSERT_MSG(!((uint32_t)data % CONFIG_USB_ALIGN_SIZE), "dwc2 data must be %d-byte aligned", CONFIG_USB_ALIGN_SIZE);
    }

    if (!data && data_len) {
        return -1;
    }

    if (ep_idx && !(USB_OTG_OUTEP(ep_idx)->DOEPCTL & USB_OTG_DOEPCTL_MPSIZ)) {
        return -2;
    }

    g_dwc2_udc[busid].out_ep[ep_idx].xfer_buf = (uint8_t *)data;
    g_dwc2_udc[busid].out_ep[ep_idx].xfer_len = data_len;
    g_dwc2_udc[busid].out_ep[ep_idx].actual_xfer_len = 0;

    USB_OTG_OUTEP(ep_idx)->DOEPTSIZ &= ~(USB_OTG_DOEPTSIZ_PKTCNT);
    USB_OTG_OUTEP(ep_idx)->DOEPTSIZ &= ~(USB_OTG_DOEPTSIZ_XFRSIZ);
    if (data_len == 0) {
        USB_OTG_OUTEP(ep_idx)->DOEPTSIZ |= (USB_OTG_DOEPTSIZ_PKTCNT & (1 << 19));
        USB_OTG_OUTEP(ep_idx)->DOEPTSIZ |= (USB_OTG_DOEPTSIZ_XFRSIZ & g_dwc2_udc[busid].out_ep[ep_idx].ep_mps);
        USB_OTG_OUTEP(ep_idx)->DOEPCTL |= (USB_OTG_DOEPCTL_CNAK | USB_OTG_DOEPCTL_EPENA);
        return 0;
    }

    if (ep_idx == 0) {
        if (data_len > g_dwc2_udc[busid].out_ep[ep_idx].ep_mps) {
            data_len = g_dwc2_udc[busid].out_ep[ep_idx].ep_mps;
        }
        g_dwc2_udc[busid].out_ep[ep_idx].xfer_len = data_len;
        USB_OTG_OUTEP(ep_idx)->DOEPTSIZ |= (USB_OTG_DOEPTSIZ_PKTCNT & (1U << 19));
        USB_OTG_OUTEP(ep_idx)->DOEPTSIZ |= (USB_OTG_DOEPTSIZ_XFRSIZ & data_len);
    } else {
        pktcnt = (uint16_t)((data_len + g_dwc2_udc[busid].out_ep[ep_idx].ep_mps - 1U) / g_dwc2_udc[busid].out_ep[ep_idx].ep_mps);

        USB_OTG_OUTEP(ep_idx)->DOEPTSIZ |= (USB_OTG_DOEPTSIZ_PKTCNT & (pktcnt << 19));
        USB_OTG_OUTEP(ep_idx)->DOEPTSIZ |= (USB_OTG_DOEPTSIZ_XFRSIZ & data_len);
    }

    if (g_dwc2_udc[busid].user_params.device_dma_enable) {
        USB_OTG_OUTEP(ep_idx)->DOEPDMA = (uint32_t)data;
    }
    if (g_dwc2_udc[busid].out_ep[ep_idx].ep_type == USB_ENDPOINT_TYPE_ISOCHRONOUS) {
        if ((USB_OTG_DEV->DSTS & (1U << 8)) == 0U) {
            USB_OTG_OUTEP(ep_idx)->DOEPCTL &= ~USB_OTG_DOEPCTL_SD0PID_SEVNFRM;
            USB_OTG_OUTEP(ep_idx)->DOEPCTL |= USB_OTG_DOEPCTL_SODDFRM;
        } else {
            USB_OTG_OUTEP(ep_idx)->DOEPCTL &= ~USB_OTG_DOEPCTL_SODDFRM;
            USB_OTG_OUTEP(ep_idx)->DOEPCTL |= USB_OTG_DOEPCTL_SD0PID_SEVNFRM;
        }
    }
    USB_OTG_OUTEP(ep_idx)->DOEPCTL |= (USB_OTG_DOEPCTL_CNAK | USB_OTG_DOEPCTL_EPENA);
    return 0;
}

void USBD_IRQHandler(uint8_t busid)
{
    uint32_t gint_status, temp, ep_idx, ep_intr, epint, read_count;
    gint_status = dwc2_get_glb_intstatus(busid);

    (void)read_count;

    if ((USB_OTG_GLB->GINTSTS & 0x1U) == USB_OTG_MODE_DEVICE) {
        /* Avoid spurious interrupt */
        if (gint_status == 0) {
            return;
        }

        if (!g_dwc2_udc[busid].user_params.device_dma_enable) {
            /* Handle RxQLevel Interrupt */
            if (gint_status & USB_OTG_GINTSTS_RXFLVL) {
                USB_MASK_INTERRUPT(USB_OTG_GLB, USB_OTG_GINTSTS_RXFLVL);

                temp = USB_OTG_GLB->GRXSTSP;
                ep_idx = temp & USB_OTG_GRXSTSP_EPNUM;

                if (((temp & USB_OTG_GRXSTSP_PKTSTS) >> USB_OTG_GRXSTSP_PKTSTS_Pos) == STS_DATA_UPDT) {
                    read_count = (temp & USB_OTG_GRXSTSP_BCNT) >> 4;
                    if (read_count != 0) {
                        dwc2_ep_read(busid, g_dwc2_udc[busid].out_ep[ep_idx].xfer_buf, read_count);
                        g_dwc2_udc[busid].out_ep[ep_idx].xfer_buf += read_count;
                    }
                } else if (((temp & USB_OTG_GRXSTSP_PKTSTS) >> USB_OTG_GRXSTSP_PKTSTS_Pos) == STS_SETUP_UPDT) {
                    read_count = (temp & USB_OTG_GRXSTSP_BCNT) >> 4;
                    dwc2_ep_read(busid, (uint8_t *)&g_dwc2_udc[busid].setup, read_count);
                } else {
                    /* ... */
                }
                USB_UNMASK_INTERRUPT(USB_OTG_GLB, USB_OTG_GINTSTS_RXFLVL);
            }
        }

        if (gint_status & USB_OTG_GINTSTS_OEPINT) {
            ep_idx = 0;
            ep_intr = dwc2_get_outeps_intstatus(busid);
            while (ep_intr != 0U) {
                if ((ep_intr & 0x1U) != 0U) {
                    epint = dwc2_get_outep_intstatus(busid, ep_idx);

                    if ((epint & USB_OTG_DOEPINT_XFRC) == USB_OTG_DOEPINT_XFRC) {
                        if (ep_idx == 0) {
                            if (usbd_get_ep0_next_state(busid) == USBD_EP0_STATE_SETUP) {
                                goto process_setup; // goto ep0 setup, xfer_len is not used
                            }

                            if (g_dwc2_udc[busid].out_ep[ep_idx].xfer_len == 0) {
                                /* If ep0 xfer_len is 0, it means that we are in outstatus phase */
                                g_dwc2_udc[busid].out_ep[ep_idx].actual_xfer_len = 0;
                            } else {
                                /* If ep0 xfer_len is not 0, it means that we are in outdata phase */
                                g_dwc2_udc[busid].out_ep[ep_idx].actual_xfer_len = g_dwc2_udc[busid].out_ep[ep_idx].xfer_len - ((USB_OTG_OUTEP(ep_idx)->DOEPTSIZ) & USB_OTG_DOEPTSIZ_XFRSIZ);
                            }

                            g_dwc2_udc[busid].out_ep[ep_idx].xfer_len = 0;
                            if (g_dwc2_udc[busid].user_params.device_dma_enable) {
                                usb_dcache_invalidate((uintptr_t)g_dwc2_udc[busid].out_ep[ep_idx].xfer_buf, USB_ALIGN_UP(g_dwc2_udc[busid].out_ep[ep_idx].actual_xfer_len, CONFIG_USB_ALIGN_SIZE));
                            }
                            usbd_event_ep_out_complete_handler(busid, 0x00, g_dwc2_udc[busid].out_ep[ep_idx].actual_xfer_len);

                            if (usbd_get_ep0_next_state(busid) == USBD_EP0_STATE_SETUP) {
                                /* Out status, start reading setup */
                                dwc2_ep0_start_read_setup(busid, (uint8_t *)&g_dwc2_udc[busid].setup);
                            }
                        } else {
                            g_dwc2_udc[busid].out_ep[ep_idx].actual_xfer_len = g_dwc2_udc[busid].out_ep[ep_idx].xfer_len - ((USB_OTG_OUTEP(ep_idx)->DOEPTSIZ) & USB_OTG_DOEPTSIZ_XFRSIZ);
                            g_dwc2_udc[busid].out_ep[ep_idx].xfer_len = 0;
                            if (g_dwc2_udc[busid].user_params.device_dma_enable) {
                                usb_dcache_invalidate((uintptr_t)g_dwc2_udc[busid].out_ep[ep_idx].xfer_buf, USB_ALIGN_UP(g_dwc2_udc[busid].out_ep[ep_idx].actual_xfer_len, CONFIG_USB_ALIGN_SIZE));
                            }
                            usbd_event_ep_out_complete_handler(busid, ep_idx, g_dwc2_udc[busid].out_ep[ep_idx].actual_xfer_len);
                        }
                    }
                // clang-format off
process_setup:
                    // clang-format on
                    if ((epint & USB_OTG_DOEPINT_STUP) == USB_OTG_DOEPINT_STUP) {
                        if (g_dwc2_udc[busid].user_params.device_dma_enable) {
                            usb_dcache_invalidate((uintptr_t)&g_dwc2_udc[busid].setup, USB_ALIGN_UP(8, CONFIG_USB_ALIGN_SIZE));
                        }
                        usbd_event_ep0_setup_complete_handler(busid, (uint8_t *)&g_dwc2_udc[busid].setup);
                    }
                }
                ep_intr >>= 1U;
                ep_idx++;
            }
        }
        if (gint_status & USB_OTG_GINTSTS_IEPINT) {
            ep_idx = 0U;
            ep_intr = dwc2_get_ineps_intstatus(busid);
            while (ep_intr != 0U) {
                if ((ep_intr & 0x1U) != 0U) {
                    epint = dwc2_get_inep_intstatus(busid, ep_idx);

                    if ((epint & USB_OTG_DIEPINT_XFRC) == USB_OTG_DIEPINT_XFRC) {
                        if (ep_idx == 0) {
                            g_dwc2_udc[busid].in_ep[ep_idx].actual_xfer_len = g_dwc2_udc[busid].in_ep[ep_idx].xfer_len - ((USB_OTG_INEP(ep_idx)->DIEPTSIZ) & USB_OTG_DIEPTSIZ_XFRSIZ);
                            g_dwc2_udc[busid].in_ep[ep_idx].xfer_len = 0;
                            usbd_event_ep_in_complete_handler(busid, 0x80, g_dwc2_udc[busid].in_ep[ep_idx].actual_xfer_len);

                            if (usbd_get_ep0_next_state(busid) == USBD_EP0_STATE_SETUP) {
                                /* In status, start reading setup */
                                dwc2_ep0_start_read_setup(busid, (uint8_t *)&g_dwc2_udc[busid].setup);
                            }
                        } else {
                            g_dwc2_udc[busid].in_ep[ep_idx].actual_xfer_len = g_dwc2_udc[busid].in_ep[ep_idx].xfer_len - ((USB_OTG_INEP(ep_idx)->DIEPTSIZ) & USB_OTG_DIEPTSIZ_XFRSIZ);
                            g_dwc2_udc[busid].in_ep[ep_idx].xfer_len = 0;
                            usbd_event_ep_in_complete_handler(busid, ep_idx | 0x80, g_dwc2_udc[busid].in_ep[ep_idx].actual_xfer_len);
                        }
                    }
                    if (!g_dwc2_udc[busid].user_params.device_dma_enable) {
                        if ((epint & USB_OTG_DIEPINT_TXFE) == USB_OTG_DIEPINT_TXFE) {
                            dwc2_tx_fifo_empty_procecss(busid, ep_idx);
                        }
                    }
                }
                ep_intr >>= 1U;
                ep_idx++;
            }
        }
        if (gint_status & USB_OTG_GINTSTS_USBRST) {
            USB_OTG_GLB->GINTSTS = USB_OTG_GINTSTS_USBRST;
            USB_OTG_DEV->DCTL &= ~USB_OTG_DCTL_RWUSIG;

            dwc2_flush_txfifo(busid, 0x10U);
            dwc2_flush_rxfifo(busid);

            for (uint8_t i = 0U; i < (g_dwc2_udc[busid].hw_params.num_dev_ep + 1); i++) {
                if (i == 0U) {
                    USB_OTG_INEP(i)->DIEPCTL = USB_OTG_DIEPCTL_SNAK;
                    USB_OTG_OUTEP(i)->DOEPCTL = USB_OTG_DOEPCTL_SNAK;
                } else {
                    if (USB_OTG_INEP(i)->DIEPCTL & USB_OTG_DIEPCTL_EPENA) {
                        USB_OTG_INEP(i)->DIEPCTL = (USB_OTG_DIEPCTL_EPDIS | USB_OTG_DIEPCTL_SNAK);
                    } else {
                        USB_OTG_INEP(i)->DIEPCTL = 0;
                    }
                    if (USB_OTG_OUTEP(i)->DOEPCTL & USB_OTG_DOEPCTL_EPENA) {
                        USB_OTG_OUTEP(i)->DOEPCTL = (USB_OTG_DOEPCTL_EPDIS | USB_OTG_DOEPCTL_SNAK);
                    } else {
                        USB_OTG_OUTEP(i)->DOEPCTL = 0;
                    }
                }
                USB_OTG_INEP(i)->DIEPTSIZ = 0U;
                USB_OTG_INEP(i)->DIEPINT = 0xFBFFU;
                USB_OTG_OUTEP(i)->DOEPTSIZ = 0U;
                USB_OTG_OUTEP(i)->DOEPINT = 0xFBFFU;
            }

            USB_OTG_DEV->DAINTMSK |= 0x10001U;

            USB_OTG_DEV->DOEPMSK = USB_OTG_DOEPMSK_STUPM |
                                   USB_OTG_DOEPMSK_XFRCM;

            USB_OTG_DEV->DIEPMSK = USB_OTG_DIEPMSK_XFRCM;

            memset(g_dwc2_udc[busid].in_ep, 0, sizeof(struct dwc2_ep_state) * 16);
            memset(g_dwc2_udc[busid].out_ep, 0, sizeof(struct dwc2_ep_state) * 16);
            usbd_event_reset_handler(busid);
            /* Start reading setup */
            dwc2_ep0_start_read_setup(busid, (uint8_t *)&g_dwc2_udc[busid].setup);
        }
        if (gint_status & USB_OTG_GINTSTS_ENUMDNE) {
            USB_OTG_GLB->GINTSTS = USB_OTG_GINTSTS_ENUMDNE;
            dwc2_set_turnaroundtime(busid, SystemCoreClock, dwc2_get_devspeed(busid));

            USB_OTG_DEV->DCTL |= USB_OTG_DCTL_CGINAK;
        }
        if (gint_status & USB_OTG_GINTSTS_PXFR_INCOMPISOOUT) {
            USB_OTG_GLB->GINTSTS = USB_OTG_GINTSTS_PXFR_INCOMPISOOUT;
        }

        if (gint_status & USB_OTG_GINTSTS_IISOIXFR) {
            USB_OTG_GLB->GINTSTS = USB_OTG_GINTSTS_IISOIXFR;
        }
#ifdef CONFIG_USBDEV_SOF_ENABLE
        if (gint_status & USB_OTG_GINTSTS_SOF) {
            USB_OTG_GLB->GINTSTS = USB_OTG_GINTSTS_SOF;
            usbd_event_sof_handler(busid);
        }
#endif
        if (gint_status & USB_OTG_GINTSTS_USBSUSP) {
            USB_OTG_GLB->GINTSTS = USB_OTG_GINTSTS_USBSUSP;
            usbd_event_suspend_handler(busid);
        }
        if (gint_status & USB_OTG_GINTSTS_WKUINT) {
            USB_OTG_GLB->GINTSTS = USB_OTG_GINTSTS_WKUINT;
            usbd_event_resume_handler(busid);
        }
        if (gint_status & USB_OTG_GINTSTS_OTGINT) {
            temp = USB_OTG_GLB->GOTGINT;
            if ((temp & USB_OTG_GOTGINT_SEDET) == USB_OTG_GOTGINT_SEDET) {
            } else {
            }
            USB_OTG_GLB->GOTGINT |= temp;
        }
    }
}
