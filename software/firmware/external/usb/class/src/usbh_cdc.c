/*
 * USB Host CDC class driver.
 *
 * Substantially based on the CDC class driver from the STMicroelectronics
 * USB Host Library, extensively modified and rewritten to match the design
 * and operation of the other custom-written USB serial device drivers.
 *
 * Nevertheless, it should be considered a heavily derived work of the ST
 * implementation, which bears the following copyright block:
 *
 ******************************************************************************
 * @file    usbh_cdc.c
 * @author  MCD Application Team
 * @brief   This file is the CDC Layer Handlers for USB Host CDC class.
 *
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2015 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 * @verbatim
 *
 * ===================================================================
 *                       CDC Class Driver Description
 * ===================================================================
 *  This driver manages the "Universal Serial Bus Class Definitions for Communications Devices
 *  Revision 1.2 November 16, 2007" and the sub-protocol specification of "Universal Serial Bus
 *  Communications Class Subclass Specification for PSTN Devices Revision 1.2 February 9, 2007"
 *  This driver implements the following aspects of the specification:
 *    - Device descriptor management
 *    - Configuration descriptor management
 *    - Enumeration as CDC device with 2 data endpoints (IN and OUT) and 1 command endpoint (IN)
 *    - Requests management (as described in section 6.2 in specification)
 *    - Abstract Control Model compliant
 *    - Union Functional collection (using 1 IN endpoint for control)
 *    - Data interface class
 *
 * @endverbatim
 ******************************************************************************
 */

#include "usbh_cdc.h"

static USBH_StatusTypeDef USBH_CDC_InterfaceInit(USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef USBH_CDC_InterfaceDeInit(USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef USBH_CDC_Process(USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef USBH_CDC_SOFProcess(USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef USBH_CDC_ClassRequest(USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef CDC_SetLineCoding(USBH_HandleTypeDef *phost, CDC_LineCodingTypeDef *linecoding);
static USBH_StatusTypeDef CDC_GetLineCoding(USBH_HandleTypeDef *phost, CDC_LineCodingTypeDef *linecoding);
static USBH_StatusTypeDef CDC_SetControlLineState(USBH_HandleTypeDef *phost, uint16_t linestate);
static void CDC_ProcessTransmission(USBH_HandleTypeDef *phost);
static void CDC_ProcessReception(USBH_HandleTypeDef *phost);

USBH_ClassTypeDef CDC_Class = {
    "CDC",
    USB_CDC_CLASS,
    USBH_CDC_InterfaceInit,
    USBH_CDC_InterfaceDeInit,
    USBH_CDC_ClassRequest,
    USBH_CDC_Process,
    USBH_CDC_SOFProcess,
    NULL,
};

USBH_StatusTypeDef USBH_CDC_InterfaceInit(USBH_HandleTypeDef *phost)
{
    USBH_StatusTypeDef status;
    uint8_t interface;
    CDC_HandleTypeDef *CDC_Handle;

    USBH_UsrLog("CDC: USBH_CDC_InterfaceInit");

    interface = USBH_FindInterface(phost, COMMUNICATION_INTERFACE_CLASS_CODE,
        ABSTRACT_CONTROL_MODEL, NO_CLASS_SPECIFIC_PROTOCOL_CODE);

    if ((interface == 0xFFU) || (interface >= USBH_MAX_NUM_INTERFACES)) {
        /* No Valid Interface */
        USBH_ErrLog("CDC: Cannot find the interface for the CDC class.");
        return USBH_FAIL;
    }

    status = USBH_SelectInterface(phost, interface);
    if (status != USBH_OK) {
        return USBH_FAIL;
    }

    phost->pActiveClass->pData = (CDC_HandleTypeDef *)USBH_malloc(sizeof(CDC_HandleTypeDef));
    CDC_Handle = (CDC_HandleTypeDef *)phost->pActiveClass->pData;

    if (CDC_Handle == NULL) {
        USBH_ErrLog("CDC: Cannot allocate memory for CDC Handle");
        return USBH_FAIL;
    }

    /* Initialize CDC handler */
    (void)USBH_memset(CDC_Handle, 0, sizeof(CDC_HandleTypeDef));

    /* Collect the notification endpoint address and length */
    if ((phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[0].bEndpointAddress & 0x80U) != 0U) {
        CDC_Handle->CommItf.NotifEp = phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[0].bEndpointAddress;
        CDC_Handle->CommItf.NotifEpSize = phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[0].wMaxPacketSize;
    }

    /* Allocate the length for host channel number in */
    CDC_Handle->CommItf.NotifPipe = USBH_AllocPipe(phost, CDC_Handle->CommItf.NotifEp);

    /* Open pipe for Notification endpoint */
    (void)USBH_OpenPipe(phost, CDC_Handle->CommItf.NotifPipe, CDC_Handle->CommItf.NotifEp,
        phost->device.address, phost->device.speed, USB_EP_TYPE_INTR,
        CDC_Handle->CommItf.NotifEpSize);

    (void)USBH_LL_SetToggle(phost, CDC_Handle->CommItf.NotifPipe, 0U);

    interface = USBH_FindInterface(phost, DATA_INTERFACE_CLASS_CODE,
        RESERVED, NO_CLASS_SPECIFIC_PROTOCOL_CODE);

    if ((interface == 0xFFU) || (interface >= USBH_MAX_NUM_INTERFACES)) {
        /* No Valid Interface */
        USBH_ErrLog("CDC: Cannot Find the interface for Data Interface Class.");
        return USBH_FAIL;
    }

    /* Collect the class specific endpoint address and length */
    if ((phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[0].bEndpointAddress & 0x80U) != 0U) {
        CDC_Handle->DataItf.InEp = phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[0].bEndpointAddress;
        CDC_Handle->DataItf.InEpSize  = phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[0].wMaxPacketSize;
    } else {
        CDC_Handle->DataItf.OutEp = phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[0].bEndpointAddress;
        CDC_Handle->DataItf.OutEpSize  = phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[0].wMaxPacketSize;
    }

    if ((phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[1].bEndpointAddress & 0x80U) != 0U) {
        CDC_Handle->DataItf.InEp = phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[1].bEndpointAddress;
        CDC_Handle->DataItf.InEpSize  = phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[1].wMaxPacketSize;
    } else {
        CDC_Handle->DataItf.OutEp = phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[1].bEndpointAddress;
        CDC_Handle->DataItf.OutEpSize = phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[1].wMaxPacketSize;
    }

    /* Allocate the length for host channel number out */
    CDC_Handle->DataItf.OutPipe = USBH_AllocPipe(phost, CDC_Handle->DataItf.OutEp);

    /* Allocate the length for host channel number in */
    CDC_Handle->DataItf.InPipe = USBH_AllocPipe(phost, CDC_Handle->DataItf.InEp);

    /* Open channel for OUT endpoint */
    (void)USBH_OpenPipe(phost, CDC_Handle->DataItf.OutPipe, CDC_Handle->DataItf.OutEp,
        phost->device.address, phost->device.speed, USB_EP_TYPE_BULK,
        CDC_Handle->DataItf.OutEpSize);

    /* Open channel for IN endpoint */
    (void)USBH_OpenPipe(phost, CDC_Handle->DataItf.InPipe, CDC_Handle->DataItf.InEp,
        phost->device.address, phost->device.speed, USB_EP_TYPE_BULK,
        CDC_Handle->DataItf.InEpSize);

    /* Set initial state and polling interval */
    CDC_Handle->state = USBH_CDC_STATE_IDLE;
    if (CDC_Handle->Rx_Poll < CDC_MIN_POLL) {
        CDC_Handle->Rx_Poll = CDC_MIN_POLL;
    }

    /*
     * Enable our custom tweak to prevent the HAL USB interrupt handler from
     * re-activating the IN channel every single time it is halted with a
     * NAK/URB_NOTREADY event. Without this, attempting to poll without
     * available data will make the interrupt repeatedly fire and cause
     * problems.
     */
    HCD_HandleTypeDef *hhcd = phost->pData;
    hhcd->hc[CDC_Handle->DataItf.InPipe].no_reactivate_on_nak = 1;

    (void)USBH_LL_SetToggle(phost, CDC_Handle->DataItf.OutPipe, 0U);
    (void)USBH_LL_SetToggle(phost, CDC_Handle->DataItf.InPipe, 0U);

    USBH_UsrLog("CDC: InEpSize=%d, OutEpSize=%d",
        CDC_Handle->DataItf.InEpSize,
        CDC_Handle->DataItf.OutEpSize);

    CDC_Handle->DataItf.InEpBuffer = (uint8_t *)USBH_malloc(CDC_Handle->DataItf.InEpSize);
    if (!CDC_Handle->DataItf.InEpBuffer) {
        USBH_ErrLog("CDC: Cannot allocate memory for CDC receive buffer");
        return USBH_FAIL;
    }
    CDC_Handle->pRxData = CDC_Handle->DataItf.InEpBuffer;
    CDC_Handle->RxDataLength = CDC_Handle->DataItf.InEpSize;
    USBH_memset(CDC_Handle->DataItf.InEpBuffer, 0, CDC_Handle->DataItf.InEpSize);

    CDC_Handle->DataItf.OutEpBuffer = (uint8_t *)USBH_malloc(CDC_Handle->DataItf.OutEpSize);
    if (!CDC_Handle->DataItf.OutEpBuffer) {
        USBH_ErrLog("CDC: Cannot allocate memory for CDC transmit buffer");
        return USBH_FAIL;
    }
    USBH_memset(CDC_Handle->DataItf.OutEpBuffer, 0, CDC_Handle->DataItf.OutEpSize);

    return USBH_OK;
}

USBH_StatusTypeDef USBH_CDC_InterfaceDeInit(USBH_HandleTypeDef *phost)
{
    CDC_HandleTypeDef *CDC_Handle = (CDC_HandleTypeDef *) phost->pActiveClass->pData;

    /* Close and free the I/O pipes */

    if ((CDC_Handle->CommItf.NotifPipe) != 0U) {
        (void)USBH_ClosePipe(phost, CDC_Handle->CommItf.NotifPipe);
        (void)USBH_FreePipe(phost, CDC_Handle->CommItf.NotifPipe);
        CDC_Handle->CommItf.NotifPipe = 0U;     /* Reset the Channel as Free */
    }

    if ((CDC_Handle->DataItf.InPipe) != 0U) {
        (void)USBH_ClosePipe(phost, CDC_Handle->DataItf.InPipe);
        (void)USBH_FreePipe(phost, CDC_Handle->DataItf.InPipe);
        CDC_Handle->DataItf.InPipe = 0U;     /* Reset the Channel as Free */
    }

    if ((CDC_Handle->DataItf.OutPipe) != 0U) {
        (void)USBH_ClosePipe(phost, CDC_Handle->DataItf.OutPipe);
        (void)USBH_FreePipe(phost, CDC_Handle->DataItf.OutPipe);
        CDC_Handle->DataItf.OutPipe = 0U;    /* Reset the Channel as Free */
    }

    /* Free the I/O data buffers */

    if (CDC_Handle->DataItf.InEpBuffer) {
        USBH_free(CDC_Handle->DataItf.InEpBuffer);
        CDC_Handle->DataItf.InEpBuffer = NULL;
        CDC_Handle->pRxData = NULL;
        CDC_Handle->RxDataLength = 0;
    }

    if (CDC_Handle->DataItf.OutEpBuffer) {
        USBH_free(CDC_Handle->DataItf.OutEpBuffer);
        CDC_Handle->DataItf.OutEpBuffer = NULL;
        CDC_Handle->pTxData = NULL;
        CDC_Handle->TxDataLength = 0;
    }

    /* Free the device handle */

    if ((phost->pActiveClass->pData) != NULL) {
        USBH_free(phost->pActiveClass->pData);
        phost->pActiveClass->pData = 0U;
    }

    return USBH_OK;
}

USBH_StatusTypeDef USBH_CDC_ClassRequest(USBH_HandleTypeDef *phost)
{
    USBH_StatusTypeDef status;
    CDC_HandleTypeDef *CDC_Handle = (CDC_HandleTypeDef *)phost->pActiveClass->pData;

    /* Issue the get line coding request */
    status = CDC_GetLineCoding(phost, &CDC_Handle->LineCoding);
    if (status == USBH_OK) {
        phost->pUser(phost, HOST_USER_CLASS_ACTIVE);
    } else if (status == USBH_NOT_SUPPORTED) {
        USBH_ErrLog("CDC: Unable to get device line coding");
    }

    return status;
}

USBH_StatusTypeDef USBH_CDC_Process(USBH_HandleTypeDef *phost)
{
  USBH_StatusTypeDef status = USBH_BUSY;
  USBH_StatusTypeDef req_status = USBH_OK;
  CDC_HandleTypeDef *CDC_Handle = (CDC_HandleTypeDef *)phost->pActiveClass->pData;

  switch (CDC_Handle->state) {
  case USBH_CDC_STATE_IDLE:
      status = USBH_OK;
      break;

  case USBH_CDC_STATE_SET_LINE_CODING:
      req_status = CDC_SetLineCoding(phost, CDC_Handle->pUserLineCoding);

      if (req_status == USBH_OK) {
          CDC_Handle->state = USBH_CDC_STATE_GET_LAST_LINE_CODING;
      } else {
          if (req_status != USBH_BUSY) {
              CDC_Handle->state = USBH_CDC_STATE_ERROR;
          }
      }
      break;

  case USBH_CDC_STATE_GET_LAST_LINE_CODING:
      req_status = CDC_GetLineCoding(phost, &(CDC_Handle->LineCoding));

      if (req_status == USBH_OK) {
          CDC_Handle->state = USBH_CDC_STATE_IDLE;

          if ((CDC_Handle->LineCoding.b.bCharFormat == CDC_Handle->pUserLineCoding->b.bCharFormat)
              && (CDC_Handle->LineCoding.b.bDataBits == CDC_Handle->pUserLineCoding->b.bDataBits)
              && (CDC_Handle->LineCoding.b.bParityType == CDC_Handle->pUserLineCoding->b.bParityType)
              && (CDC_Handle->LineCoding.b.dwDTERate == CDC_Handle->pUserLineCoding->b.dwDTERate)) {
              USBH_CDC_LineCodingChanged(phost);
          }
      } else {
          if (req_status != USBH_BUSY) {
              CDC_Handle->state = USBH_CDC_STATE_ERROR;
          }
      }
      break;

  case USBH_CDC_STATE_SET_CONTROL_LINE_STATE:
      req_status = CDC_SetControlLineState(phost, CDC_Handle->userControlLineState);

      if (req_status == USBH_OK) {
          CDC_Handle->state = USBH_CDC_STATE_IDLE;
          CDC_Handle->ControlLineState = CDC_Handle->userControlLineState;

          USBH_CDC_ControlLineStateChanged(phost);

      } else {
          if (req_status != USBH_BUSY) {
              CDC_Handle->state = USBH_CDC_STATE_ERROR;
          }
      }
      break;

  case USBH_CDC_STATE_TRANSFER_DATA:
      CDC_Handle->Rx_Timer = phost->Timer;
      CDC_ProcessTransmission(phost);
      CDC_ProcessReception(phost);

      if (CDC_Handle->data_rx_state == USBH_CDC_IDLE_DATA && CDC_Handle->data_tx_state == USBH_CDC_IDLE_DATA) {
          CDC_Handle->state = USBH_CDC_STATE_IDLE;
      }
      break;

  case USBH_CDC_STATE_ERROR:
      req_status = USBH_ClrFeature(phost, 0x00U);
      if (req_status == USBH_OK) {
          /* Change the state to waiting */
          CDC_Handle->state = USBH_CDC_STATE_IDLE;
      }
      break;

  default:
      break;

  }

  return status;
}

USBH_StatusTypeDef USBH_CDC_SOFProcess(USBH_HandleTypeDef *phost)
{
    CDC_HandleTypeDef *CDC_Handle = (CDC_HandleTypeDef *)phost->pActiveClass->pData;

    if (CDC_Handle->state == USBH_CDC_STATE_IDLE) {
        if ((phost->Timer - CDC_Handle->Rx_Timer) >= CDC_Handle->Rx_Poll) {
            CDC_Handle->state = USBH_CDC_STATE_TRANSFER_DATA;
            CDC_Handle->data_rx_state = USBH_CDC_RECEIVE_DATA;

#if (USBH_USE_OS == 1U)
            phost->os_msg = (uint32_t)USBH_URB_EVENT;
#if (osCMSIS < 0x20000U)
            (void)osMessagePut(phost->os_event, phost->os_msg, 0U);
#else
            (void)osMessageQueuePut(phost->os_event, &phost->os_msg, 0U, 0U);
#endif
#endif
        }
    } else if (CDC_Handle->state == USBH_CDC_STATE_TRANSFER_DATA && CDC_Handle->data_rx_state == USBH_CDC_RECEIVE_DATA_WAIT) {
        if ((phost->Timer - CDC_Handle->Rx_Timer) >= CDC_Handle->Rx_Poll) {
#if (USBH_USE_OS == 1U)
            phost->os_msg = (uint32_t)USBH_URB_EVENT;
#if (osCMSIS < 0x20000U)
            (void)osMessagePut(phost->os_event, phost->os_msg, 0U);
#else
            (void)osMessageQueuePut(phost->os_event, &phost->os_msg, 0U, 0U);
#endif
#endif
        }
    }
    return USBH_OK;
}

USBH_StatusTypeDef USBH_CDC_Transmit(USBH_HandleTypeDef *phost, uint8_t *pbuff, uint32_t length)
{
    USBH_StatusTypeDef Status = USBH_BUSY;
    CDC_HandleTypeDef *CDC_Handle = (CDC_HandleTypeDef *)phost->pActiveClass->pData;

    if (length > CDC_Handle->DataItf.OutEpSize) {
        USBH_ErrLog("CDC: Transmit size too large: %lu > %d",
            length, CDC_Handle->DataItf.OutEpSize);
        return USBH_FAIL;
    }

    if ((CDC_Handle->state == USBH_CDC_STATE_IDLE) || (CDC_Handle->state == USBH_CDC_STATE_TRANSFER_DATA)) {
        USBH_memcpy(CDC_Handle->DataItf.OutEpBuffer, pbuff, length);
        CDC_Handle->pTxData = CDC_Handle->DataItf.OutEpBuffer;
        CDC_Handle->TxDataLength = length;
        CDC_Handle->state = USBH_CDC_STATE_TRANSFER_DATA;
        CDC_Handle->data_tx_state = USBH_CDC_SEND_DATA;
        Status = USBH_OK;

#if (USBH_USE_OS == 1U)
        phost->os_msg = (uint32_t)USBH_CLASS_EVENT;
#if (osCMSIS < 0x20000U)
        (void)osMessagePut(phost->os_event, phost->os_msg, 0U);
#else
        (void)osMessageQueuePut(phost->os_event, &phost->os_msg, 0U, 0U);
#endif
#endif
    }
    return Status;
}

void CDC_ProcessTransmission(USBH_HandleTypeDef *phost)
{
    CDC_HandleTypeDef *CDC_Handle = (CDC_HandleTypeDef *)phost->pActiveClass->pData;
    USBH_URBStateTypeDef URB_Status = USBH_URB_IDLE;

    switch (CDC_Handle->data_tx_state) {
    case USBH_CDC_SEND_DATA:
        if (CDC_Handle->TxDataLength > CDC_Handle->DataItf.OutEpSize) {
            /*
             * Right now this case is not possible, because we allocate TxData and limit
             * what can be sent based on OutEpSize
             */
            USBH_BulkSendData(phost,
                CDC_Handle->pTxData,
                CDC_Handle->DataItf.OutEpSize,
                CDC_Handle->DataItf.OutPipe,
                1U);
        } else {
            USBH_BulkSendData(phost,
                CDC_Handle->pTxData,
                (uint16_t)CDC_Handle->TxDataLength,
                CDC_Handle->DataItf.OutPipe,
                1U);
        }

        CDC_Handle->data_tx_state = USBH_CDC_SEND_DATA_WAIT;
        break;

    case USBH_CDC_SEND_DATA_WAIT:
        URB_Status = USBH_LL_GetURBState(phost, CDC_Handle->DataItf.OutPipe);

        /* Check if the transmission is complete */
        if (URB_Status == USBH_URB_DONE) {
            if (CDC_Handle->TxDataLength > CDC_Handle->DataItf.OutEpSize) {
                CDC_Handle->TxDataLength -= CDC_Handle->DataItf.OutEpSize;
                CDC_Handle->pTxData += CDC_Handle->DataItf.OutEpSize;
            } else {
                CDC_Handle->TxDataLength = 0U;
            }

            if (CDC_Handle->TxDataLength > 0U) {
                CDC_Handle->data_tx_state = USBH_CDC_SEND_DATA;
            } else {
                CDC_Handle->data_tx_state = USBH_CDC_IDLE_DATA;
                CDC_Handle->pTxData = NULL;
                CDC_Handle->TxDataLength = 0;
                USBH_CDC_TransmitCallback(phost);
            }

#if (USBH_USE_OS == 1U)
            phost->os_msg = (uint32_t)USBH_CLASS_EVENT;
#if (osCMSIS < 0x20000U)
            (void)osMessagePut(phost->os_event, phost->os_msg, 0U);
#else
            (void)osMessageQueuePut(phost->os_event, &phost->os_msg, 0U, 0U);
#endif
#endif
        } else {
            if (URB_Status == USBH_URB_NOTREADY) {
                CDC_Handle->data_tx_state = USBH_CDC_SEND_DATA;

#if (USBH_USE_OS == 1U)
                phost->os_msg = (uint32_t)USBH_CLASS_EVENT;
#if (osCMSIS < 0x20000U)
                (void)osMessagePut(phost->os_event, phost->os_msg, 0U);
#else
                (void)osMessageQueuePut(phost->os_event, &phost->os_msg, 0U, 0U);
#endif
#endif
            }
        }
        break;

    default:
        break;
    }
}

void CDC_ProcessReception(USBH_HandleTypeDef *phost)
{
    CDC_HandleTypeDef *CDC_Handle = (CDC_HandleTypeDef *)phost->pActiveClass->pData;
    USBH_URBStateTypeDef URB_Status = USBH_URB_IDLE;
    uint32_t length;

    switch (CDC_Handle->data_rx_state) {
    case USBH_CDC_IDLE_DATA:
        break;
    case USBH_CDC_RECEIVE_DATA:
        USBH_BulkReceiveData(phost,
            CDC_Handle->pRxData,
            CDC_Handle->DataItf.InEpSize,
            CDC_Handle->DataItf.InPipe);

        CDC_Handle->data_rx_state = USBH_CDC_RECEIVE_DATA_WAIT;

        break;

    case USBH_CDC_RECEIVE_DATA_WAIT:
        URB_Status = USBH_LL_GetURBState(phost, CDC_Handle->DataItf.InPipe);

        /* Check if the reception is complete */
        if (URB_Status == USBH_URB_DONE) {
            length = USBH_LL_GetLastXferSize(phost, CDC_Handle->DataItf.InPipe);

            if (((CDC_Handle->RxDataLength - length) > 0U) && (length > CDC_Handle->DataItf.InEpSize)) {
                CDC_Handle->RxDataLength -= length;
                CDC_Handle->pRxData += length;
                CDC_Handle->data_rx_state = USBH_CDC_RECEIVE_DATA;
            } else {
                CDC_Handle->data_rx_state = USBH_CDC_IDLE_DATA;

                size_t recv_length;
                if (CDC_Handle->pRxData == CDC_Handle->DataItf.InEpBuffer) {
                    recv_length = length;
                } else {
                    recv_length = CDC_Handle->pRxData - CDC_Handle->DataItf.InEpBuffer;
                    CDC_Handle->pRxData = CDC_Handle->DataItf.InEpBuffer;
                }

                if (recv_length > 0) {
                    USBH_CDC_ReceiveCallback(phost, CDC_Handle->pRxData, recv_length);
                }
                CDC_Handle->pRxData = CDC_Handle->DataItf.InEpBuffer;
                CDC_Handle->RxDataLength = CDC_Handle->DataItf.InEpSize;
            }

#if (USBH_USE_OS == 1U)
            phost->os_msg = (uint32_t)USBH_CLASS_EVENT;
#if (osCMSIS < 0x20000U)
            (void)osMessagePut(phost->os_event, phost->os_msg, 0U);
#else
            (void)osMessageQueuePut(phost->os_event, &phost->os_msg, 0U, 0U);
#endif
#endif
        } else if (URB_Status == USBH_URB_NOTREADY) {
            CDC_Handle->data_rx_state = USBH_CDC_IDLE_DATA;
        }
        break;

    default:
        break;
    }
}

USBH_StatusTypeDef USBH_CDC_Stop(USBH_HandleTypeDef *phost)
{
    CDC_HandleTypeDef *CDC_Handle = (CDC_HandleTypeDef *) phost->pActiveClass->pData;

    if (phost->gState == HOST_CLASS) {
        USBH_UsrLog("CDC: USBH_CDC_Stop");
        CDC_Handle->state = USBH_CDC_STATE_IDLE;
        USBH_ClosePipe(phost, CDC_Handle->CommItf.NotifPipe);
        USBH_ClosePipe(phost, CDC_Handle->DataItf.InPipe);
        USBH_ClosePipe(phost, CDC_Handle->DataItf.OutPipe);
    }
    return USBH_OK;
}

USBH_StatusTypeDef USBH_CDC_SetLineCoding(USBH_HandleTypeDef *phost, CDC_LineCodingTypeDef *linecoding)
{
    USBH_StatusTypeDef status;
    CDC_HandleTypeDef *CDC_Handle = (CDC_HandleTypeDef *) phost->pActiveClass->pData;

    if (phost->gState == HOST_CLASS) {
        CDC_Handle->state = USBH_CDC_STATE_SET_LINE_CODING;
        CDC_Handle->pUserLineCoding = linecoding;

#if (USBH_USE_OS == 1U)
        phost->os_msg = (uint32_t)USBH_CLASS_EVENT;
#if (osCMSIS < 0x20000U)
        (void)osMessagePut(phost->os_event, phost->os_msg, 0U);
#else
        (void)osMessageQueuePut(phost->os_event, &phost->os_msg, 0U, 0U);
#endif
#endif
        status = USBH_OK;
    } else if (phost->gState == HOST_CLASS_REQUEST) {
        status = CDC_SetLineCoding(phost, linecoding);
    } else {
        USBH_ErrLog("CDC: Invalid state for setting line coding: %d", phost->gState);
        status = USBH_FAIL;
    }

    return status;
}

USBH_StatusTypeDef CDC_SetLineCoding(USBH_HandleTypeDef *phost, CDC_LineCodingTypeDef *linecoding)
{
    phost->Control.setup.b.bmRequestType = USB_H2D | USB_REQ_TYPE_CLASS |
        USB_REQ_RECIPIENT_INTERFACE;

    phost->Control.setup.b.bRequest = CDC_SET_LINE_CODING;
    phost->Control.setup.b.wValue.w = 0U;
    phost->Control.setup.b.wIndex.w = 0U;
    phost->Control.setup.b.wLength.w = LINE_CODING_STRUCTURE_SIZE;

    return USBH_CtlReq(phost, linecoding->Array, LINE_CODING_STRUCTURE_SIZE);
}

USBH_StatusTypeDef USBH_CDC_GetLineCoding(USBH_HandleTypeDef *phost, CDC_LineCodingTypeDef *linecoding)
{
    CDC_HandleTypeDef *CDC_Handle = (CDC_HandleTypeDef *) phost->pActiveClass->pData;

    if ((phost->gState == HOST_CLASS) || (phost->gState == HOST_CLASS_REQUEST)) {
        *linecoding = CDC_Handle->LineCoding;
        return USBH_OK;
    } else {
        return USBH_FAIL;
    }
}

USBH_StatusTypeDef CDC_GetLineCoding(USBH_HandleTypeDef *phost, CDC_LineCodingTypeDef *linecoding)
{

    phost->Control.setup.b.bmRequestType = USB_D2H | USB_REQ_TYPE_CLASS | \
        USB_REQ_RECIPIENT_INTERFACE;

    phost->Control.setup.b.bRequest = CDC_GET_LINE_CODING;
    phost->Control.setup.b.wValue.w = 0U;
    phost->Control.setup.b.wIndex.w = 0U;
    phost->Control.setup.b.wLength.w = LINE_CODING_STRUCTURE_SIZE;

    return USBH_CtlReq(phost, linecoding->Array, LINE_CODING_STRUCTURE_SIZE);
}

USBH_StatusTypeDef USBH_CDC_SetControlLineState(USBH_HandleTypeDef *phost, uint16_t linestate)
{
    USBH_StatusTypeDef status;
    CDC_HandleTypeDef *CDC_Handle = (CDC_HandleTypeDef *) phost->pActiveClass->pData;

    if (phost->gState == HOST_CLASS) {
        CDC_Handle->state = USBH_CDC_STATE_SET_CONTROL_LINE_STATE;
        CDC_Handle->userControlLineState = linestate;

#if (USBH_USE_OS == 1U)
        phost->os_msg = (uint32_t)USBH_CLASS_EVENT;
#if (osCMSIS < 0x20000U)
        (void)osMessagePut(phost->os_event, phost->os_msg, 0U);
#else
        (void)osMessageQueuePut(phost->os_event, &phost->os_msg, 0U, 0U);
#endif
#endif
        status = USBH_OK;
    } else if (phost->gState == HOST_CLASS_REQUEST) {
        status = CDC_SetControlLineState(phost, linestate);
    } else {
        USBH_ErrLog("CDC: Invalid state for setting control line state: %d", phost->gState);
        status = USBH_FAIL;
    }

    return status;
}

USBH_StatusTypeDef CDC_SetControlLineState(USBH_HandleTypeDef *phost, uint16_t linestate)
{
    phost->Control.setup.b.bmRequestType = USB_H2D | USB_REQ_TYPE_CLASS |
        USB_REQ_RECIPIENT_INTERFACE;

    phost->Control.setup.b.bRequest = CDC_SET_CONTROL_LINE_STATE;
    phost->Control.setup.b.wValue.w = linestate;
    phost->Control.setup.b.wIndex.w = 0U;
    phost->Control.setup.b.wLength.w = 0U;
    return USBH_CtlReq(phost, NULL, 0);
}

USBH_StatusTypeDef USBH_CDC_GetControlLineState(USBH_HandleTypeDef *phost, uint16_t *linestate)
{
    CDC_HandleTypeDef *CDC_Handle = (CDC_HandleTypeDef *)phost->pActiveClass->pData;

    if ((phost->gState == HOST_CLASS) || (phost->gState == HOST_CLASS_REQUEST)) {
        *linestate = CDC_Handle->ControlLineState;
        return USBH_OK;
    } else {
        return USBH_FAIL;
    }
}

__weak void USBH_CDC_TransmitCallback(USBH_HandleTypeDef *phost)
{
    /* Prevent unused argument(s) compilation warning */
    UNUSED(phost);
}

__weak void USBH_CDC_ReceiveCallback(USBH_HandleTypeDef *phost, uint8_t *data, size_t length)
{
    /* Prevent unused argument(s) compilation warning */
    UNUSED(phost);
}

__weak void USBH_CDC_LineCodingChanged(USBH_HandleTypeDef *phost)
{
    /* Prevent unused argument(s) compilation warning */
    UNUSED(phost);
}

__weak void USBH_CDC_ControlLineStateChanged(USBH_HandleTypeDef *phost)
{
    /* Prevent unused argument(s) compilation warning */
    UNUSED(phost);
}
