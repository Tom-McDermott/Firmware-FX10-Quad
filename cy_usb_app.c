/***************************************************************************//**
* \file cy_usb_app.c
* \version 1.0
*
* Implements the USB data handling part of the LVDS to USB streaming application.
*
*******************************************************************************
* \copyright
* (c) (2026), Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.
*
* SPDX-License-Identifier: Apache-2.0
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*******************************************************************************/

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "event_groups.h"
#include "cy_pdl.h"
#include "cy_device.h"
#include "cy_usb_common.h"
#include "cy_usbhs_cal_drv.h"
#include "cy_usbss_cal_drv.h"
#include "cy_usb_usbd.h"
#include "cy_usb_app.h"
#include "cy_hbdma.h"
#include "cy_hbdma_mgr.h"
#include "cy_lvds.h"
#include <string.h>
#include "cy_lvds.h"

/* Buffer used for debug control transfers. */
__attribute__ ((section(".hbBufSection"), used)) uint32_t Ep0TempBuffer[1024U] __attribute__ ((aligned (32)));

/* Buffer used for SET_SEL command handling. */
__attribute__ ((section(".hbBufSection"), used)) uint32_t SetSelDataBuffer[8] __attribute__ ((aligned (32)));

/*******************************************************************************
 * Function Name: checkStatus
 ****************************************************************************//**
 *
 * General purpose function used in assertion implementation. Prints error message
 * and blocks if required when condition is not true.
 *
 * \param function
 * Name of function being checked.
 *
 * \param line
 * Source file line number.
 *
 * \param condition
 * Condition to be checked.
 *
 * \param value
 * Additional status value.
 *
 * \param isBlocking
 * Whether function should block when condition is false.
 *******************************************************************************/
void checkStatus (const char *function, uint32_t line, uint8_t condition, uint32_t value, uint8_t isBlocking)
{
    if (!condition) {
        /* Application failed with the error code status */
        Cy_Debug_AddToLog(1, RED);
        Cy_Debug_AddToLog(1, "Function %s failed at line %d with status = 0x%x\r\n", function, line, value);
        Cy_Debug_AddToLog(1, COLOR_RESET);
        if (isBlocking) {
            /* Loop indefinitely */
            for (;;)
            {
            }
        }
    }
}

/*******************************************************************************
 * Function Name: checkStatusAndHandleFailure
 ****************************************************************************//**
 *
 * General purpose function used in assertion implementation. Prints error message,
 * calls an error handler and blocks if required when condition is not true.
 *
 * \param function
 * Name of function being checked.
 *
 * \param line
 * Source file line number.
 *
 * \param condition
 * Condition to be checked.
 *
 * \param value
 * Additional status value.
 *
 * \param isBlocking
 * Whether function should block when condition is false.
 *
 * \param failureHandler
 * Function to be called if condition is false.
 *******************************************************************************/
void checkStatusAndHandleFailure (const char *function, uint32_t line, uint8_t condition,
        uint32_t value, uint8_t isBlocking, void (*failureHandler)(void))
{
    if (!condition) {
        /* Application failed with the error code status */
        Cy_Debug_AddToLog(1, RED);
        Cy_Debug_AddToLog(1, "Function %s failed at line %d with status = 0x%x\r\n", function, line, value);
        Cy_Debug_AddToLog(1, COLOR_RESET);

        if (failureHandler != NULL) {
            (*failureHandler)();
        }

        if (isBlocking) {
            /* Loop indefinitely */
            for (;;)
            {
            }
        }
    }
}

/*******************************************************************************
 * Function Name: Cy_USB_IsValidMMIOAddr
 ****************************************************************************//**
 *
 * Check if the passed address is within the valid MMIO address range. Note that
 * this is not an exhaustive check as the MMIO range is not contiguous.
 *
 * \param address
 * Address to be checked for validity.
 *
 * \return
 * whether the address is valid.
 *******************************************************************************/
static bool Cy_USB_IsValidMMIOAddr (const uint32_t address)
{
    uint32_t periLastAddr = 0x700000;
    return ((address >= PERI_BASE) && (address < (PERI_BASE + periLastAddr)));
}


#if LVDS_LB_EN

cy_stc_hbdma_channel_t   lvdsLbPgmChannel;
static uint16_t          lvdsLbPgmSize = 0;
static volatile bool     lvdsLbFlowCtrl = false;
static volatile bool     lvdsLpbkBlocked = false;
static volatile uint32_t lvdsConsCount = 0;

/*******************************************************************************
 * Function Name: Cy_LVDS_InitLbPgm
 ****************************************************************************//**
 *
 * Function which fills a RAM buffer based on the loopback pattern specified.
 *
 * \param buffStat
 * Information about RAM buffer to be filled with the pattern.
 *
 * \param lbPgmConfig
 * Loopback pattern configuration.
 *******************************************************************************/
static void
Cy_LVDS_InitLbPgm (cy_stc_hbdma_buff_status_t *buffStat, cy_stc_lvds_loopback_config_t *lbPgmConfig)
{
    cy_stc_lvds_loopback_mem_t lbPgm;

    lbPgm.dataWord0 =   ((0x00000001) |
            (lbPgmConfig->start << 1) |
            (lbPgmConfig->end << 2) |
            (lbPgmConfig->dataMode << 4) |
            (lbPgmConfig->repeatCount << 8) |
            (lbPgmConfig->dataSrc << 20));
    lbPgm.dataWord1 =   ((lbPgmConfig->ctrlByte) |
            (lbPgmConfig->ctrlBusVal << 12));
    lbPgm.dataWord2 = lbPgmConfig->dataL;
    lbPgm.dataWord3 = lbPgmConfig->dataH;
    lbPgmConfig->pBuffer = buffStat->pBuffer + lbPgmConfig->lbPgmCount * 16;
    memcpy(lbPgmConfig->pBuffer, &lbPgm, 16);
    lbPgmConfig->lbPgmCount += 1;
}

/*******************************************************************************
 * Function Name: Cy_LVDS_PrepareLoopBackData
 ****************************************************************************//**
 *
 * Selects the loopback data pattern and fills the current RAM buffer based on it.
 *
 *******************************************************************************/
static void Cy_LVDS_PrepareLoopBackData (void)
{
    uint32_t loop = 0, lineCount = 0;
    cy_stc_hbdma_buff_status_t buffStat;

    cy_stc_lvds_loopback_config_t lbPgmConfig =
    {
        .lbPgmCount  = 0,
        .pBuffer     = NULL,
        .start       = 0,
        .end         = 0,
        .dataMode    = 0x00,
        .dataSrc     = 0x00,
        .ctrlByte    = 0x01,
        .repeatCount = 0x0001,
        .dataL       = 0x00000000,
        .dataH       = 0xDEADBEEF,
        .ctrlBusVal  = 0x00000000
    };

    if (Cy_HBDma_Channel_GetBuffer(&lvdsLbPgmChannel, &buffStat) != CY_HBDMA_MGR_SUCCESS) {
        DBG_APP_ERR("PrepareData: GetLpbkBuf failed\r\n");
        return;
    }

    lbPgmConfig.pBuffer = buffStat.pBuffer;

    /* Start command */
    lbPgmConfig.start = 1;
    Cy_LVDS_InitLbPgm(&buffStat, &lbPgmConfig);

    /* Few cycles of IDLE */
    lbPgmConfig.start = 0;
    for(loop = 0; loop < 10; loop++) {
        Cy_LVDS_InitLbPgm(&buffStat, &lbPgmConfig);
    }

    /* Fill the buffer with repeating 8 bytes of data. */
    lbPgmConfig.ctrlByte = 0x80;
    for (lineCount = 0; lineCount < (HBDMA_BUFFER_SIZE / 16); lineCount++) {
        lbPgmConfig.dataL = 0x12345678UL;
        lbPgmConfig.dataH = 0x9ABCDEF0UL;
        Cy_LVDS_InitLbPgm(&buffStat, &lbPgmConfig);
    }

    /* End of program command */
    lbPgmConfig.end = 0x1;
    lbPgmConfig.ctrlByte = 0x01;
    lbPgmConfig.repeatCount = 0x00000000;
    Cy_LVDS_InitLbPgm(&buffStat, &lbPgmConfig);

    lvdsLbPgmSize = lbPgmConfig.lbPgmCount * 16;

    buffStat.count = lvdsLbPgmSize;
    Cy_HBDma_Channel_CommitBuffer(&lvdsLbPgmChannel, &buffStat);
}

/*******************************************************************************
 * Function Name: Cy_LVDS_CommitLoopBackData
 ****************************************************************************//**
 *
 * Function that commits loopback data patterns which feed data to the LVDS
 * ingress socket.
 *
 *******************************************************************************/
static void
Cy_LVDS_CommitLoopBackData (void)
{
    cy_stc_hbdma_buff_status_t lbBuffStat;

    if (lvdsLpbkBlocked) {
        DBG_APP_INFO("Skip data commit due to streaming stop\r\n");
        return;
    }

    if (Cy_HBDma_Channel_GetBuffer(&lvdsLbPgmChannel, &lbBuffStat) != CY_HBDMA_MGR_SUCCESS) {
        DBG_APP_ERR("CommitData: GetLpbkBuf failed\r\n");
        return;
    }

    lbBuffStat.count = lvdsLbPgmSize;
    Cy_HBDma_Channel_CommitBuffer(&lvdsLbPgmChannel, &lbBuffStat);

    if (Cy_HBDma_Channel_GetBuffer(&lvdsLbPgmChannel, &lbBuffStat) != CY_HBDMA_MGR_SUCCESS) {
        DBG_APP_ERR("CommitData: GetLpbkBuf failed\r\n");
        return;
    }

    lbBuffStat.count = lvdsLbPgmSize;
    Cy_HBDma_Channel_CommitBuffer(&lvdsLbPgmChannel, &lbBuffStat);
}

/*******************************************************************************
 * Function Name: HbDma_Cb_Loopback
 ****************************************************************************//**
 *
 * DMA callback corresponding to the loopback transmitter. This function checks
 * whether the receiving channel is ready to receive data and then re-enables
 * the transmitter.
 *
 * \param handle
 * Handle to the loopback transmitter DMA channel.
 *
 * \param type
 * Type of DMA event (only consume events expected).
 *
 * \param pbufStat
 * Buffer status associated with the event.
 *
 * \param userCtx
 * User application context structure.
 *******************************************************************************/
void
HbDma_Cb_Loopback (
        cy_stc_hbdma_channel_t *handle,
        cy_en_hbdma_cb_type_t type,
        cy_stc_hbdma_buff_status_t *pbufStat,
        void *userCtx)
{
    cy_stc_hbdma_sock_t sckInfo;

    if (type == CY_HBDMA_CB_CONS_EVENT) {
        lvdsConsCount++;

        if ((lvdsConsCount & 0x01) == 0) {
            /*
             * If the LVDS ingress socket is active, commit the next buffer.
             * Otherwise, set a flag indicating flow control state.
             */
            Cy_HBDma_GetSocketStatus(handle->pContext->pDrvContext, CY_HBDMA_LVDS_SOCKET_00, &sckInfo);
            if (CY_HBDMA_STATUS_TO_SOCK_STATE(sckInfo.status) == CY_HBDMA_SOCK_STATE_ACTIVE) {
                Cy_LVDS_CommitLoopBackData();
            } else {
                lvdsLbFlowCtrl = true;
            }
        }
    }
}

#endif /* LVDS_LB_EN */

/*******************************************************************************
 * Function Name: Cy_USB_AppInit
 ****************************************************************************//**
 *
 * LVDS to USB streaming application specific initialization.
 *
 * \param pAppCtxt
 * Pointer to application context structure.
 *
 * \param pUsbdCtxt
 * Pointer to USBD stack context structure.
 *
 * \param pCpuDmacBase
 * Pointer to DMAC register set.
 *
 * \param pCpuDw0Base
 * Pointer to DW0 register set.
 *
 * \param pCpuDw1Base
 * Pointer to DW1 register set.
 *
 * \param pHbDmaMgr
 * Pointer to High BandWidth DMA manager context structure.
 *
 *******************************************************************************/
void
Cy_USB_AppInit (cy_stc_usb_app_ctxt_t *pAppCtxt,
        cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, DMAC_Type *pCpuDmacBase,
        DW_Type *pCpuDw0Base, DW_Type *pCpuDw1Base,
        cy_stc_hbdma_mgr_context_t *pHbDmaMgr)
{
    uint32_t index;
    cy_stc_app_endp_dma_set_t *pEndpInDma;
    cy_stc_app_endp_dma_set_t *pEndpOutDma;

    pAppCtxt->devState          = CY_USB_DEVICE_STATE_DISABLE;
    pAppCtxt->prevDevState      = CY_USB_DEVICE_STATE_DISABLE;
    pAppCtxt->devSpeed          = CY_USBD_USB_DEV_FS;
    pAppCtxt->devAddr           = 0x00;
    pAppCtxt->activeCfgNum      = 0x00;
    pAppCtxt->prevAltSetting    = 0x00;
    pAppCtxt->enumMethod        = CY_USB_ENUM_METHOD_FAST;
    pAppCtxt->pHbDmaMgr         = pHbDmaMgr;
    pAppCtxt->pCpuDmacBase      = pCpuDmacBase;
    pAppCtxt->pCpuDw0Base       = pCpuDw0Base;
    pAppCtxt->pCpuDw1Base       = pCpuDw1Base;
    pAppCtxt->pUsbdCtxt         = pUsbdCtxt;
    pAppCtxt->dmaBufferSize     = HBDMA_BUFFER_SIZE;
    pAppCtxt->streamingRate     = FPS_DEFAULT;

    for (index = 0x00; index < CY_USB_MAX_ENDP_NUMBER; index++) {
        pEndpInDma = &(pAppCtxt->endpInDma[index]);
        memset((void *)pEndpInDma, 0, sizeof(cy_stc_app_endp_dma_set_t));

        pEndpOutDma = &(pAppCtxt->endpOutDma[index]);
        memset((void *)pEndpOutDma, 0, sizeof(cy_stc_app_endp_dma_set_t));
    }

    /* Zero out the EP0 scratch buffer. */
    memset ((uint8_t *)Ep0TempBuffer, 0, sizeof(Ep0TempBuffer));
}

/*******************************************************************************
 * Function name: Cy_USB_AppSlpCallback
 ****************************************************************************//**
 *
 * This Function will be called by USBD layer when SLP message comes.
 *
 * \param pAppCtxt
 * application layer context pointer.
 *
 * \param pUsbdCtxt
 * USBD context
 *
 * \param pMsg
 * USB Message
 *
 ********************************************************************************/
static void
Cy_USB_AppSlpCallback (
        void *pUsbApp,
        cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
        cy_stc_usb_cal_msg_t *pMsg)
{
    cy_stc_usb_app_ctxt_t *pAppCtxt = (cy_stc_usb_app_ctxt_t *)pUsbApp;

    /*
     * Notify the DMA manager in case a SLP was received on USB-HS OUT endpoint causing transfer termination.
     * Note: This is not relevant in this application as there are no OUT endpoints in use.
     */
    if (pMsg->type == CY_USB_CAL_MSG_OUT_SLP) {
        Cy_HBDma_Mgr_HandleUsbShortInterrupt(pAppCtxt->pHbDmaMgr, (pMsg->data[0] & 0x7F), pMsg->data[1]);
    }
}

/*******************************************************************************
 * Function name: Cy_USB_AppZlpCallback
 ****************************************************************************//**
 *
 * This Function will be called by USBD layer when ZLP message comes
 *
 * \param pAppCtxt
 * application layer context pointer.
 *
 * \param pUsbdCtxt
 * USBD context
 *
 * \param pMsg
 * USB Message
 *
 ********************************************************************************/
static void
Cy_USB_AppZlpCallback (
        void *pUsbApp,
        cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
        cy_stc_usb_cal_msg_t *pMsg)
{
    cy_stc_usb_app_ctxt_t *pAppCtxt = (cy_stc_usb_app_ctxt_t *)pUsbApp;

    /*
     * Notify the DMA manager in case a ZLP was received on USB-HS OUT endpoint causing transfer termination.
     * Note: This is not relevant in this application as there are no OUT endpoints in use.
     */
    if (pMsg->type == CY_USB_CAL_MSG_OUT_ZLP) {
        Cy_HBDma_Mgr_HandleUsbShortInterrupt(pAppCtxt->pHbDmaMgr, (pMsg->data[0] & 0x7F), 0);
    }
}

/*******************************************************************************
 * Function name: Cy_USB_AppSetAddressCallback
 ****************************************************************************//**
 *
 * This Function will be called by USBD layer when a USB address has been assigned
 * to the device.
 *
 * \param pAppCtxt
 * Application layer context pointer.
 *
 * \param pUsbdCtxt
 * USBD context
 *
 * \param pMsg
 * USB Message
 *
 ********************************************************************************/
static void
Cy_USB_AppSetAddressCallback (
        void *pUsbApp,
        cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
        cy_stc_usb_cal_msg_t *pMsg)
{
    cy_stc_usb_app_ctxt_t *pAppCtxt = (cy_stc_usb_app_ctxt_t *)pUsbApp;

    /* Update the state variables. */
    pAppCtxt->devState     = CY_USB_DEVICE_STATE_ADDRESS;
    pAppCtxt->prevDevState = CY_USB_DEVICE_STATE_DEFAULT;
    pAppCtxt->devAddr      = pUsbdCtxt->devAddr;
    pAppCtxt->devSpeed     = Cy_USBD_GetDeviceSpeed(pUsbdCtxt);

    /* Check the type of USB connection and register appropriate descriptors. */
    CyApp_RegisterUsbDescriptors(pAppCtxt, pAppCtxt->devSpeed);
}

/*******************************************************************************
 * Function Name: Cy_USB_AppL1SleepCallback
 ****************************************************************************//**
 *
 * Called by USBD stack when USB 2.x connection enters L1 state.
 *
 * \param pAppCtxt
 * Application layer context pointer.
 *
 * \param pUsbdCtxt
 * USBD context
 *
 * \param pMsg
 * USB Message
 *
 *******************************************************************************/
static void
Cy_USB_AppL1SleepCallback (
        void *pUsbApp,
        cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
        cy_stc_usb_cal_msg_t *pMsg)
{
    (void)pUsbApp;
    (void)pUsbdCtxt;
    (void)pMsg;

    DBG_APP_TRACE("L1SleepCbk\r\n");
}

/*******************************************************************************
 * Function name: Cy_USB_AppL1ResumeCallback
 ****************************************************************************//**
 *
 * Called by USBD stack when USB 2.x link exits L1 state.
 *
 * \param pAppCtxt
 * application layer context pointer.
 *
 * \param pUsbdCtxt
 * USBD context
 *
 * \param pMsg
 * USB Message
 *
 ********************************************************************************/
static void
Cy_USB_AppL1ResumeCallback (
        void *pUsbApp,
        cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
        cy_stc_usb_cal_msg_t *pMsg)
{
    DBG_APP_TRACE("L1ResumeCbk\r\n");
}

/*******************************************************************************
 * Function name: Cy_USB_AppSetIntfCallback
 ****************************************************************************//**
 *
 * Called by USBD stack when a SET_INTERFACE request is received for any interface.
 *
 * \param pAppCtxt
 * application layer context pointer.
 *
 * \param pUsbdCtxt
 * USBD context
 *
 * \param pMsg
 * USB Message
 *
 ********************************************************************************/
static void
Cy_USB_AppSetIntfCallback (
        void *pAppCtxt,
        cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
        cy_stc_usb_cal_msg_t *pMsg)
{
    /* We can stall the request since only one alternate setting is supported in this configuration. */
    DBG_APP_INFO("SetIntfCbk\r\n");
    Cy_USB_USBD_EndpSetClearStall(pUsbdCtxt, 0x00, CY_USB_ENDP_DIR_IN, true);
}


/*******************************************************************************
 * Function Name: HbDma_Cb
 ****************************************************************************//**
 *
 * DMA callback function for the LVDS to USB streaming channel.
 *
 * \param handle
 * DMA channel handle.
 *
 * \param type
 * Type of event
 *
 * \param pbufStat
 * Buffer status associated with the event.
 *
 * \param userCtx
 * Callback context pointer.
 *
 *******************************************************************************/
static void HbDma_Cb (
        cy_stc_hbdma_channel_t *handle,
        cy_en_hbdma_cb_type_t type,
        cy_stc_hbdma_buff_status_t* pbufStat,
        void *userCtx)
{
    cy_en_hbdma_mgr_status_t   status;
    cy_stc_hbdma_buff_status_t buffStat;
    cy_stc_usb_app_ctxt_t     *pAppCtxt = (cy_stc_usb_app_ctxt_t *)userCtx;

    if (type == CY_HBDMA_CB_PROD_EVENT) {
        /* Ensure that we have an occupied data buffer to be handled. */
        status = Cy_HBDma_Channel_GetBuffer(handle, &buffStat);
        if (status != CY_HBDMA_MGR_SUCCESS) {
            DBG_APP_ERR("HB-DMA GetBuffer error %x\r\n", status);
            return;
        }

        /* Commit the data buffer with no modifications. */
        status = Cy_HBDma_Channel_CommitBuffer(handle, &buffStat);
        if (status != CY_HBDMA_MGR_SUCCESS) {
            DBG_APP_ERR("HB-DMA CommitBuffer error %x\r\n", status);
        }
    }

    if (type == CY_HBDMA_CB_CONS_EVENT) {
        if (pAppCtxt->isLvdsWltoUsbHs) {
            /* Previous data sent to USB host. We can give the DMA ready status at this stage. */
            if (pAppCtxt->fwDmaReadyStatus == false) {
                DBG_APP_TRACE("SET FWRDY\r\n");
                pAppCtxt->fwDmaReadyStatus = true;
                Cy_LVDS_PhyGpioSet(LVDSSS_LVDS, 0, FX_DMA_RDY_IO);
            }
        }

#if LVDS_LB_EN
        /* If loopback transmitter was in flow control state, trigger it once more. */
        if (lvdsLbFlowCtrl) {
            lvdsLbFlowCtrl = false;
            Cy_LVDS_CommitLoopBackData();
        }
#endif /* LVDS_LB_EN */
    }
}

/*******************************************************************************
 * Function Name: Cy_USB_AppSetupEndpDmaParamsSS
 ****************************************************************************//**
 *
 * Function to create High BandWidth DMA channels corresponding to OUT/IN endpoints.
 *
 * \param pUsbApp
 * USB application context structure.
 *
 * \param pEndpDscr
 * Pointer to endpoint descriptor for which DMA has to be configured.
 *
 * \param isUsb2
 * Specifies whether USB connection is in 2.x mode.
 *
 *******************************************************************************/
static void Cy_USB_AppSetupEndpDmaParamsSS (
        cy_stc_usb_app_ctxt_t *pUsbApp,
        uint8_t *pEndpDscr,
        bool isUsb2)
{
    cy_stc_hbdma_chn_config_t dmaConfig;
    cy_en_hbdma_mgr_status_t  mgrStat;
    cy_stc_app_endp_dma_set_t *pEndpDmaSet;
    uint32_t endpNumber, endpDir, endpType;
    uint16_t maxPktSize;

    Cy_USBD_GetEndpNumMaxPktDir(pEndpDscr, &endpNumber, &maxPktSize, &endpDir);
    Cy_USBD_GetEndpType(pEndpDscr, &endpType);

    /* We only support DMA configuration for LVDS_STREAMING_EP IN endpoint. */
    if ((endpDir) && (endpNumber == LVDS_STREAMING_EP)) {
        pEndpDmaSet = &(pUsbApp->endpInDma[endpNumber]);

        if (pEndpDmaSet->valid) {
            /* Make sure any previously created channel is destroyed, so that we can create
             * one afresh with the correct configuration.
             */
            Cy_HBDma_Channel_Reset(&(pEndpDmaSet->hbDmaChannel));
            Cy_HBDma_Channel_Destroy(&(pEndpDmaSet->hbDmaChannel));
        }

        dmaConfig.chType         = CY_HBDMA_TYPE_IP_TO_IP;
        dmaConfig.size           = pUsbApp->dmaBufferSize;      /* DMA buffer size in bytes */
        dmaConfig.count          = 3;                           /* Number of DMA buffers per channel. */
        dmaConfig.bufferMode     = true;                        /* Use DMA buffer mode to track socket count. */
        dmaConfig.prodHdrSize    = 0;                           /* No header to be added. */
        dmaConfig.prodBufSize    = pUsbApp->dmaBufferSize;      /* Same as DMA buffer size as there is no header. */
        dmaConfig.prodSckCount   = 1;                           /* No. of producer sockets */
        dmaConfig.consSckCount   = 1;                           /* No. of consumer Sockets */
        dmaConfig.prodSck[0]     = CY_HBDMA_LVDS_SOCKET_00;     /* Data comes from LVDS socket #0. */
        dmaConfig.prodSck[1]     = (cy_hbdma_socket_id_t)0;     /* Second producer socket is invalid. */
        dmaConfig.consSck[0]     = (cy_hbdma_socket_id_t)(CY_HBDMA_USBEG_SOCKET_00 + endpNumber);
        dmaConfig.consSck[1]     = (cy_hbdma_socket_id_t)0;     /* Second consumer socket is invalid. */
        dmaConfig.endpAddr       = endpNumber;                  /* Store endpoint index in the channel structure. */
        dmaConfig.usbMaxPktSize  = maxPktSize;                  /* Passing maxPktSize is mandatory under USBHS. */

#if (!LVDS_LB_EN)
        dmaConfig.eventEnable    = true;                        /* Route events between producer and consumer. */
        dmaConfig.intrEnable     = 0;                           /* No interrupts need to be processed. */
#if (PORT0_THREAD_INTLV)
        dmaConfig.prodSckCount   = 2;                           /* Use producer sockets in interleaved fashion. */
        dmaConfig.prodSck[1]     = CY_HBDMA_LVDS_SOCKET_01;     /* Use LVDS socket #1 also as data source. */
#endif /* (PORT0_THREAD_INTLV) */
#else
        dmaConfig.eventEnable    = false;                      /* No events to be sent. */
        dmaConfig.intrEnable     = (USB32DEV_ADAPTER_DMA_SCK_INTR_MASK_PRODUCE_EVENT_Msk |
                                    USB32DEV_ADAPTER_DMA_SCK_INTR_MASK_CONSUME_EVENT_Msk);
#endif /* LVDS_LB_EN */

        dmaConfig.cb             = HbDma_Cb;                    /* HB-DMA callback */
        dmaConfig.userCtx        = (void *)(pUsbApp);           /* Pass the application context as user context. */

        if (isUsb2) {
            /*
             * In USB 2.x case, change the consumer to USBHS_IN_EP. Also, we need to do data forwarding through
             * callbacks in this case as event signaling is not supported.
             */
            dmaConfig.consSck[0]    = (cy_hbdma_socket_id_t)(CY_HBDMA_USBHS_IN_EP_00 + endpNumber);
            dmaConfig.eventEnable   = false;
            dmaConfig.intrEnable    = (USB32DEV_ADAPTER_DMA_SCK_INTR_MASK_PRODUCE_EVENT_Msk |
                                       USB32DEV_ADAPTER_DMA_SCK_INTR_MASK_CONSUME_EVENT_Msk);
        }

        mgrStat = Cy_HBDma_Channel_Create(pUsbApp->pUsbdCtxt->pHBDmaMgr,
                &(pEndpDmaSet->hbDmaChannel), &dmaConfig);
        if (mgrStat == CY_HBDMA_MGR_SUCCESS) {
            if (pUsbApp->devSpeed != CY_USBD_USB_DEV_SS_GEN2X2) {
                /* Enable combining data from multiple buffers into one burst. */
                Cy_USBD_SetEpBurstMode(pUsbApp->pUsbdCtxt, endpNumber, CY_USB_ENDP_DIR_IN, true);
            }

            mgrStat = Cy_HBDma_Channel_Enable(&(pEndpDmaSet->hbDmaChannel), 0);
            DBG_APP_INFO("------Channel Enable endpNumber: %d enable mgrStat status=%x\r\n ..............", endpNumber, mgrStat);

            pEndpDmaSet->maxPktSize = maxPktSize;
            pEndpDmaSet->valid      = 0x01;
        } else {
            DBG_APP_ERR("-------Channel endpNumber %d create mgrStat status=%x\r\n", endpNumber, mgrStat);
        }
    }
}

/*******************************************************************************
 * Function Name: Cy_USB_AppSetupEndpDmaParams
 ****************************************************************************//**
 *
 * Function to configure DMA resources for USB endpoints.
 *
 * \param pUsbApp
 * USB application context structure.
 *
 * \param pEndpDscr
 * Pointer to endpoint descriptor for which DMA has to be configured.
 *
 *******************************************************************************/
void
Cy_USB_AppSetupEndpDmaParams (cy_stc_usb_app_ctxt_t *pUsbApp, uint8_t *pEndpDscr)
{
    /* Call the DMA setup function to create and enable the DMA channel. */
    Cy_USB_AppSetupEndpDmaParamsSS(pUsbApp, pEndpDscr, (pUsbApp->devSpeed <= CY_USBD_USB_DEV_HS));
}

/*******************************************************************************
 * Function Name: Cy_USB_AppConfigureEndp
 ****************************************************************************//**
 *
 * Function to configure USB endpoints based on configuration descriptor.
 *
 * \param pUsbdCtxt
 * USBD stack context structure.
 *
 * \param pEndpDscr
 * Pointer to endpoint descriptor for which DMA has to be configured.
 *
 *******************************************************************************/
void
Cy_USB_AppConfigureEndp (cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, uint8_t *pEndpDscr)
{
    cy_stc_usb_endp_config_t endpConfig;
    cy_en_usb_endp_dir_t endpDirection;
    bool valid;
    uint32_t endpType;
    uint32_t endpNumber, dir;
    uint16_t maxPktSize;
    uint8_t isoPkts = 0x00;
    uint8_t burstSize = 0x00;
    uint8_t maxStream = 0x00;
    uint8_t interval = 0x00;
    uint8_t *pCompDscr = NULL;
    cy_en_usbd_ret_code_t usbdRetCode;

    /* If it is not endpoint descriptor then return */
    if (!Cy_USBD_EndpDscrValid(pEndpDscr)) {
        return;
    }

    Cy_USBD_GetEndpNumMaxPktDir(pEndpDscr, &endpNumber, &maxPktSize, &dir);
    if (dir) {
        endpDirection = CY_USB_ENDP_DIR_IN;
    } else {
        endpDirection = CY_USB_ENDP_DIR_OUT;
    }
    Cy_USBD_GetEndpType(pEndpDscr, &endpType);

    if ((CY_USB_ENDP_TYPE_ISO == endpType) || (CY_USB_ENDP_TYPE_INTR == endpType)) {
        /* The ISOINPKS setting in the USBHS register is the actual packets per microframe value. */
        isoPkts = (
                (*((uint8_t *)(pEndpDscr + CY_USB_ENDP_DSCR_OFFSET_MAX_PKT + 1)) & CY_USB_ENDP_ADDL_XN_MASK)
                >> CY_USB_ENDP_ADDL_XN_POS) + 1;

        /* Fetch the endpoint service interval. */
        Cy_USBD_GetEndpInterval(pEndpDscr, &interval);
    }

    valid = 0x01;
    if (pUsbdCtxt->devSpeed > CY_USBD_USB_DEV_HS) {
        /* Get companion descriptor and from there get burstSize. */
        pCompDscr = Cy_USBD_GetSsEndpCompDscr(pUsbdCtxt, pEndpDscr);
        Cy_USBD_GetEndpCompnMaxburst(pCompDscr, &burstSize);
        Cy_USBD_GetEndpCompnMaxStream(pCompDscr, &maxStream);

        if (CY_USB_ENDP_TYPE_ISO == endpType) {
            Cy_USBD_GetEndpCompnAttribute(pCompDscr, &isoPkts);
            isoPkts = (isoPkts & 0x03U) + 0x01U;
            isoPkts *= burstSize;
        }
    }

    /* Prepare endpointConfig parameter. */
    endpConfig.endpType = (cy_en_usb_endp_type_t)endpType;
    endpConfig.endpDirection = endpDirection;
    endpConfig.valid = valid;
    endpConfig.endpNumber = endpNumber;
    endpConfig.maxPktSize = (uint32_t)maxPktSize;
    endpConfig.isoPkts = (uint32_t)isoPkts;
    endpConfig.burstSize = burstSize;
    endpConfig.streamID = maxStream;
    endpConfig.interval = interval;
    endpConfig.allowNakTillDmaRdy = false;
    usbdRetCode = Cy_USB_USBD_EndpConfig(pUsbdCtxt, endpConfig);

    if (pUsbdCtxt->devSpeed > CY_USBD_USB_DEV_HS) {
        /* Flush the endpoint memory. */
        Cy_USBD_FlushEndp(pUsbdCtxt, endpNumber, endpDirection);
    }

    /* Print status of the endpoint configuration to help debug. */
    DBG_APP_INFO("EPCFG:%d %x\r\n", endpNumber, (uint8_t)usbdRetCode);
}

/*******************************************************************************
 * Function Name: Cy_USB_AppSignalTask
 ****************************************************************************//**
 *
 * This function signals the application task when there is any work for it to perform.
 *
 * \param pAppCtxt
 * USB application context structure.
 *
 * \param evMask
 * Mask specifying event bits to be set.
 *
 * \return
 * Whether yield is required on exit from ISR.
 *
 *******************************************************************************/
bool
Cy_USB_AppSignalTask(cy_stc_usb_app_ctxt_t *pAppCtxt, const EventBits_t evMask)
{
    BaseType_t wakeTask = pdFALSE;

    xEventGroupSetBitsFromISR(pAppCtxt->appEvGrpHandle, evMask, &wakeTask);
    return (wakeTask != pdFALSE);
}

extern cy_stc_lvds_context_t lvdsContext;

/*******************************************************************************
 * Function name: Cy_USB_AppSetCfgCallback
 ****************************************************************************//**
 *
 * This function will be called by USBD layer when SET_CONFIG request is received.
 *
 * \param pAppCtxt
 * application layer context pointer.
 *
 * \param pUsbdCtxt
 * USBD context
 *
 * \param pMsg
 * USB Message
 *
 ********************************************************************************/
static void
Cy_USB_AppSetCfgCallback (
        void *pAppCtxt,
        cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
        cy_stc_usb_cal_msg_t *pMsg)
{
    cy_stc_usb_app_ctxt_t *pUsbApp;
    uint8_t *pActiveCfg, *pIntfDscr, *pEndpDscr;
    uint8_t index, numOfIntf, numOfEndp;
#if (!LVDS_LB_EN)
    cy_en_scb_i2c_status_t status = CY_SCB_I2C_SUCCESS;
#else
    uint32_t intState;
#endif /* (!LVDS_LB_EN) */

    pUsbApp = (cy_stc_usb_app_ctxt_t *)pAppCtxt;
    DBG_APP_INFO("SetCfgCb: Speed=%d\r\n", pUsbdCtxt->devSpeed);

    /* Print out the contents of the USB event log buffer before doing the configuration work. */
    AppPrintUsbEventLog(pUsbApp);

    /* Save the configuration number. */
    pUsbApp->activeCfgNum = pUsbdCtxt->activeCfgNum;

    /* Destroy any active DMA channels. */
    Cy_USB_AppDisableEndpDma(pUsbApp);

    /* Enable the DataWire DMA modules. */
    Cy_DMA_Enable(pUsbApp->pCpuDw0Base);
    Cy_DMA_Enable(pUsbApp->pCpuDw1Base);

    pActiveCfg = Cy_USB_USBD_GetActiveCfgDscr(pUsbdCtxt);
    if (!pActiveCfg) {
        /* Set config should be called when active config value > 0x00. */
        return;
    }

#if (!LVDS_LB_EN)
    pUsbApp->dmaBufferSize = HBDMA_BUFFER_SIZE;
    pUsbApp->streamingRate = FPS_DEFAULT;

    /* Identify whether we are operating in LVDS WideLink to USB 2.x transfer mode. */
    if ((lvdsContext.phyConfigP0->wideLink) && (pUsbApp->devSpeed <= CY_USBD_USB_DEV_HS)) {
        pUsbApp->isLvdsWltoUsbHs = true;
    } else {
        pUsbApp->isLvdsWltoUsbHs = false;
    }

    /* Update DMA buffer size used by firmware */
    status = Cy_I2C_Write(FPGASLAVE_ADDR, DEVICE0_OFFSET + DEVICE_BUFFER_SIZE_MSB_ADDRESS,
            CY_USB_GET_MSB(pUsbApp->dmaBufferSize),
            FPGA_I2C_ADDRESS_WIDTH, FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

    status = Cy_I2C_Write(FPGASLAVE_ADDR, DEVICE0_OFFSET + DEVICE_BUFFER_SIZE_LSB_ADDRESS,
            CY_USB_GET_LSB(pUsbApp->dmaBufferSize),
            FPGA_I2C_ADDRESS_WIDTH, FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

    /* Set the target streaming rate in FPS. */
    status = Cy_I2C_Write(FPGASLAVE_ADDR, DEVICE0_OFFSET + DEVICE_FPS_ADDRESS, pUsbApp->streamingRate,
            FPGA_I2C_ADDRESS_WIDTH, FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
#endif /* (!LVDS_LB_EN) */

    numOfIntf = Cy_USBD_FindNumOfIntf(pActiveCfg);
    if (numOfIntf == 0x00) {
        return;
    }

    for (index = 0x00; index < numOfIntf; index++) {
        /* During Set Config command always altSetting 0 will be active. */
        pIntfDscr = Cy_USBD_GetIntfDscr(pUsbdCtxt, index, 0x00);
        if (pIntfDscr == NULL) {
            DBG_APP_ERR("NULL intf dscr\r\n");
            return;
        }

        numOfEndp = Cy_USBD_FindNumOfEndp(pIntfDscr);
        if (numOfEndp == 0x00) {
            DBG_APP_WARN("No endpoints to be configured in interface %d\r\n", index);
            continue;
        }

        pEndpDscr = Cy_USBD_GetEndpDscr(pUsbdCtxt, pIntfDscr);
        while (numOfEndp != 0x00) {
            Cy_USB_AppConfigureEndp(pUsbdCtxt, pEndpDscr);
            Cy_USB_AppSetupEndpDmaParams(pAppCtxt, pEndpDscr);
            numOfEndp--;
            pEndpDscr = (pEndpDscr + (*(pEndpDscr + CY_USB_DSCR_OFFSET_LEN)));
            if (pUsbdCtxt->devSpeed > CY_USBD_USB_DEV_HS) {
                /* Skip over SS companion descriptor. */
                pEndpDscr += 6u;
            }
        }
    }

    pUsbApp->prevDevState = CY_USB_DEVICE_STATE_CONFIGURED;
    pUsbApp->devState = CY_USB_DEVICE_STATE_CONFIGURED;

    /* Signal the task that device state has changed. */
    Cy_USB_AppSignalTask(pUsbApp, EV_DEVSTATE_CHG);

#if LVDS_LB_EN
    lvdsConsCount  = 0;
    lvdsLbFlowCtrl = false;
    Cy_HBDma_Channel_Enable(&lvdsLbPgmChannel, 0);
    intState = Cy_SysLib_EnterCriticalSection();
    Cy_LVDS_PrepareLoopBackData();
    Cy_LVDS_PrepareLoopBackData();
    Cy_LVDS_GpifSetFwTrig(LVDSSS_LVDS, 1);
    Cy_SysLib_ExitCriticalSection(intState);
#else
    if (pUsbApp->isLvdsWltoUsbHs) {
        /* LVDS WL to USB 2.x transfer: We need to override the DMA_RDY signal as GPIO so
         * that LVDS ingress and USB egress transfers can be made non-overlapping.
         */
        Cy_LVDS_PhyGpioModeEnable(LVDSSS_LVDS, 0, FX_DMA_RDY_IO, CY_LVDS_PHY_GPIO_OUTPUT,
                CY_LVDS_PHY_GPIO_NO_INTERRUPT);

        DBG_APP_TRACE("CLR FWRDY\r\n");
        pUsbApp->fwDmaReadyStatus = false;
        Cy_LVDS_PhyGpioClr(LVDSSS_LVDS, 0, FX_DMA_RDY_IO);
    } else {
        /* Disable GPIO override on DMA ready signal. */
        Cy_LVDS_PhyGpioModeDisable(LVDSSS_LVDS, 0, FX_DMA_RDY_IO);
    }

    /* Enable the data stream from the FPGA. */
    DBG_APP_INFO("Enabling FPGA data stream\r\n");
    status = Cy_I2C_Write(FPGASLAVE_ADDR,
            DEVICE0_OFFSET + FPGA_DEVICE_STREAM_ENABLE_ADDRESS,
            CAMERA_APP_ENABLE, FPGA_I2C_ADDRESS_WIDTH, FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

    if (pUsbApp->isLvdsWltoUsbHs) {
        DBG_APP_TRACE("SET FWRDY\r\n");
        pUsbApp->fwDmaReadyStatus = true;
        Cy_LVDS_PhyGpioSet(LVDSSS_LVDS, 0, FX_DMA_RDY_IO);
    }
#endif /* LVDS_LB_EN */

    DBG_APP_INFO("SetCfgCb:End\r\n");
}

/*******************************************************************************
 * Function Name: Cy_USB_AppDisableEndpDma
 ****************************************************************************//**
 *
 * This function de-inits all active USB DMA channels as part of USB disconnect process.
 *
 * \param pAppCtxt
 * USB application context structure.
 *
 *******************************************************************************/
void
Cy_USB_AppDisableEndpDma (cy_stc_usb_app_ctxt_t *pAppCtxt)
{
    uint8_t i;

    if (pAppCtxt->devSpeed >= CY_USBD_USB_DEV_SS_GEN1) {
        /* If any DMA channels have been created, disable and destroy them. */
        for (i = 0; i < CY_USB_MAX_ENDP_NUMBER; i++) {
            if (pAppCtxt->endpInDma[i].valid) {
                DBG_APP_INFO("HBDMA destroy EP%d-In\r\n", i);
                Cy_HBDma_Channel_Disable(&(pAppCtxt->endpInDma[i].hbDmaChannel));
                Cy_HBDma_Channel_Destroy(&(pAppCtxt->endpInDma[i].hbDmaChannel));
                Cy_USBD_FlushEndp(pAppCtxt->pUsbdCtxt, i, CY_USB_ENDP_DIR_IN);
                pAppCtxt->endpInDma[i].valid = false;
            }

            if (pAppCtxt->endpOutDma[i].valid) {
                DBG_APP_INFO("HBDMA destroy EP%d-Out\r\n", i);
                Cy_HBDma_Channel_Disable(&(pAppCtxt->endpOutDma[i].hbDmaChannel));
                Cy_HBDma_Channel_Destroy(&(pAppCtxt->endpOutDma[i].hbDmaChannel));
                Cy_USBD_FlushEndp(pAppCtxt->pUsbdCtxt, i, CY_USB_ENDP_DIR_OUT);
                pAppCtxt->endpOutDma[i].valid = false;
            }
        }
    } else {
        for (i = 1; i < CY_USB_MAX_ENDP_NUMBER; i++) {
            if (pAppCtxt->endpInDma[i].valid) {
                /* Make sure the DMA channel is destroyed so that all memory is freed up. */
                Cy_HBDma_Channel_Destroy(&(pAppCtxt->endpInDma[i].hbDmaChannel));
            }

            if (pAppCtxt->endpOutDma[i].valid) {
                /* Make sure the DMA channel is destroyed so that all memory is freed up. */
                Cy_HBDma_Channel_Destroy(&(pAppCtxt->endpOutDma[i].hbDmaChannel));
            }
        }
    }

#if LVDS_LB_EN
    Cy_HBDma_Channel_Reset(&lvdsLbPgmChannel);
    Cy_HBDma_Channel_Enable(&lvdsLbPgmChannel, 0);
    lvdsLbFlowCtrl = false;
#endif /* LVDS_LB_EN */
}

/*******************************************************************************
 * Function name: Cy_USB_AppBusResetCallback
 ****************************************************************************//**
 *
 * This function will be called by USBD layer when there is a USB bus reset.
 *
 * \param pAppCtxt
 * application layer context pointer.
 *
 * \param pUsbdCtxt
 * USBD context
 *
 * \param pMsg
 * USB Message
 *
 ********************************************************************************/
static void
Cy_USB_AppBusResetCallback (
        void *pAppCtxt,
        cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
        cy_stc_usb_cal_msg_t *pMsg)
{
    cy_stc_usb_app_ctxt_t *pUsbApp;

    pUsbApp = (cy_stc_usb_app_ctxt_t *)pAppCtxt;
    Cy_USB_AppDisableEndpDma(pUsbApp);

    /* Re-initialize the application structures. */
    Cy_USB_AppInit(pUsbApp, pUsbdCtxt, pUsbApp->pCpuDmacBase,
                   pUsbApp->pCpuDw0Base, pUsbApp->pCpuDw1Base, pUsbApp->pHbDmaMgr);
    pUsbApp->devState     = CY_USB_DEVICE_STATE_RESET;
    pUsbApp->prevDevState = CY_USB_DEVICE_STATE_RESET;
    pUsbApp->activeCfgNum = 0;

    /* Signal the task that device state has changed. */
    Cy_USB_AppSignalTask(pUsbApp, EV_DEVSTATE_CHG);
}

/*
 * Function: Cy_USB_AppBusResetDoneCallback()
 * Description: This Function will be called by USBD  layer when
 *              set configuration command successful. This function
 *              does sanity check and prepare device for function
 *              to take over.
 * Parameter: pAppCtxt, cy_stc_usb_usbd_ctxt_t
 * return: void
 */
/*******************************************************************************
 * Function name: Cy_USB_AppBusResetDoneCallback
 ****************************************************************************//**
 *
 * This function will be called by USBD layer when USB 2.x reset is complete.
 *
 * \param pAppCtxt
 * application layer context pointer.
 *
 * \param pUsbdCtxt
 * USBD context
 *
 * \param pMsg
 * USB Message
 *
 ********************************************************************************/
static void
Cy_USB_AppBusResetDoneCallback (
        void *pAppCtxt,
        cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
        cy_stc_usb_cal_msg_t *pMsg)
{
    cy_stc_usb_app_ctxt_t *pUsbApp;

    pUsbApp = (cy_stc_usb_app_ctxt_t *)pAppCtxt;
    pUsbApp->devState = CY_USB_DEVICE_STATE_DEFAULT;
    pUsbApp->prevDevState = pUsbApp->devState;

    /* Signal the task that device state has changed. */
    Cy_USB_AppSignalTask(pUsbApp, EV_DEVSTATE_CHG);
}

/*******************************************************************************
 * Function name: Cy_USB_AppBusSpeedCallback
 ****************************************************************************//**
 *
 * This function will be called by USBD layer when USB-HS chirp handshake is successful.
 *
 * \param pAppCtxt
 * application layer context pointer.
 *
 * \param pUsbdCtxt
 * USBD context
 *
 * \param pMsg
 * USB Message
 *
 ********************************************************************************/
static void
Cy_USB_AppBusSpeedCallback (
        void *pAppCtxt,
        cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
        cy_stc_usb_cal_msg_t *pMsg)
{
    cy_stc_usb_app_ctxt_t *pUsbApp;

    pUsbApp = (cy_stc_usb_app_ctxt_t *)pAppCtxt;
    pUsbApp->devState = CY_USB_DEVICE_STATE_DEFAULT;
    pUsbApp->devSpeed = Cy_USBD_GetDeviceSpeed(pUsbdCtxt);

    /* Signal the task that device state has changed. */
    Cy_USB_AppSignalTask(pUsbApp, EV_DEVSTATE_CHG);
}

/*******************************************************************************
 * Function name: Cy_USB_AppHandleStreamReset
 ****************************************************************************//**
 *
 * Handle a stream reset request due to endpoint CLEAR_FEATURE(EP_HALT) command.
 *
 * \param pAppCtxt
 * Application layer context pointer.
 *
 ********************************************************************************/
void
Cy_USB_AppHandleStreamReset (cy_stc_usb_app_ctxt_t *pAppCtxt)
{
    cy_stc_hbdma_sock_t sckInfo;
    uint32_t pollCnt = 0;
#if (!LVDS_LB_EN)
    cy_en_scb_i2c_status_t status = CY_SCB_I2C_SUCCESS;
#else
    uint32_t intState;
#endif /* (!LVDS_LB_EN) */

    DBG_APP_INFO("Resetting streaming data path\r\n");

#if LVDS_LB_EN
    /*
     * We need to stop the loopback source channel first so that data does not accumulate on the
     * ingress thread interface after the streaming channel is reset. We want to ensure that the
     * loopback source is stopped cleanly at the end of a buffer. Since the transfer of each
     * loopback DMA buffer takes the order of 68 us, we set a flag indicating that no more data
     * should be committed and then wait for 150 us (more than 2 * 68 us). By this time, it
     * is guaranteed that the socket will reach an idle state.
     */
    lvdsLpbkBlocked = true;
    lvdsLbFlowCtrl  = false;
    Cy_SysLib_DelayUs(150);
    Cy_HBDma_Channel_Reset(&lvdsLbPgmChannel);
#endif /* LVDS_LB_EN */

    /* Stop the streaming channel and data source. */

    /* Wait until the first ingress socket has stalled or until a timeout. */
    do {
        Cy_HBDma_GetSocketStatus(pAppCtxt->pHbDmaMgr->pDrvContext, CY_HBDMA_LVDS_SOCKET_00, &sckInfo);
        if ((CY_HBDMA_STATUS_TO_SOCK_STATE(sckInfo.status) == CY_HBDMA_SOCK_STATE_STALL) || (pollCnt++ > 5000)) {
            break;
        }
        Cy_SysLib_DelayUs(1);
    } while (1);

#if PORT0_THREAD_INTLV
    /* Add some delay to get the second socket stalled as well. */
    Cy_SysLib_DelayUs(100);
#endif /* PORT0_THREAD_INTLV */

#if (!LVDS_LB_EN)
    /* Stop the data stream from the FPGA side. */
    status = Cy_I2C_Write(FPGASLAVE_ADDR, DEVICE0_OFFSET + FPGA_DEVICE_STREAM_ENABLE_ADDRESS, CAMERA_APP_DISABLE,
            FPGA_I2C_ADDRESS_WIDTH, FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
#endif /* (!LVDS_LB_EN) */

    if (pAppCtxt->isLvdsWltoUsbHs) {
        DBG_APP_TRACE("CLR FWRDY\r\n");
        pAppCtxt->fwDmaReadyStatus = false;
        Cy_LVDS_PhyGpioClr(LVDSSS_LVDS, 0, FX_DMA_RDY_IO);
    }

    /* Reset the DMA channel. */
    Cy_HBDma_Channel_Reset(&(pAppCtxt->endpInDma[LVDS_STREAMING_EP].hbDmaChannel));

    /* Flush and reset the endpoint and clear the STALL status. */
    Cy_USBSS_Cal_ClkStopOnEpRstEnable(pAppCtxt->pUsbdCtxt->pSsCalCtxt, true);
    Cy_USBD_FlushEndp(pAppCtxt->pUsbdCtxt, LVDS_STREAMING_EP, CY_USB_ENDP_DIR_IN);
    Cy_USB_USBD_EndpSetClearStall(pAppCtxt->pUsbdCtxt, LVDS_STREAMING_EP, CY_USB_ENDP_DIR_IN, false);
    Cy_USBD_ResetEndp(pAppCtxt->pUsbdCtxt, LVDS_STREAMING_EP, CY_USB_ENDP_DIR_IN, false);
    Cy_USBSS_Cal_ClkStopOnEpRstEnable(pAppCtxt->pUsbdCtxt->pSsCalCtxt, false);

    /* Re-enable the DMA channel. */
    Cy_HBDma_Channel_Enable(&(pAppCtxt->endpInDma[LVDS_STREAMING_EP].hbDmaChannel), 0);

#if (LVDS_LB_EN)
    /* Restart the loopback channel. */
    lvdsLbFlowCtrl  = false;
    lvdsLpbkBlocked = false;
    lvdsConsCount   = 0;
    Cy_HBDma_Channel_Enable(&lvdsLbPgmChannel, 0);
    intState = Cy_SysLib_EnterCriticalSection();
    Cy_LVDS_PrepareLoopBackData();
    Cy_LVDS_PrepareLoopBackData();
    Cy_SysLib_ExitCriticalSection(intState);
#else
    /* Re-start the data stream from the FPGA side. */
    status = Cy_I2C_Write(FPGASLAVE_ADDR, DEVICE0_OFFSET + FPGA_DEVICE_STREAM_ENABLE_ADDRESS, CAMERA_APP_ENABLE,
            FPGA_I2C_ADDRESS_WIDTH, FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

    if (pAppCtxt->isLvdsWltoUsbHs) {
        DBG_APP_TRACE("SET FWRDY\r\n");
        pAppCtxt->fwDmaReadyStatus = true;
        Cy_LVDS_PhyGpioSet(LVDSSS_LVDS, 0, FX_DMA_RDY_IO);
    }
#endif /* (LVDS_LB_EN) */

    DBG_APP_INFO("Stream reset complete\r\n");
}

/*******************************************************************************
 * Function name: Cy_USB_AppSetupCallback
 ****************************************************************************//**
 *
 * This function will be called by USBD layer when there is a control request
 * which needs to be handled at the application level.
 *
 * \param pAppCtxt
 * application layer context pointer.
 *
 * \param pUsbdCtxt
 * USBD context
 *
 * \param pMsg
 * USB Message
 *
 ********************************************************************************/
static void
Cy_USB_AppSetupCallback (
        void *pAppCtxt,
        cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
        cy_stc_usb_cal_msg_t *pMsg)
{
    cy_stc_usb_app_ctxt_t *pUsbApp = (cy_stc_usb_app_ctxt_t *)pAppCtxt;
    bool isReqHandled = false;
    cy_en_usbd_ret_code_t retStatus = CY_USBD_STATUS_SUCCESS;

    uint8_t bRequest, bReqType, bTarget;
    uint16_t wLength, wValue, wIndex;
    cy_en_usb_endp_dir_t epDir = CY_USB_ENDP_DIR_INVALID;
    uint32_t epNumber;

    bReqType = pUsbdCtxt->setupReq.bmRequest;
    bTarget = (bReqType & CY_USB_CTRL_REQ_RECIPENT_MASK);
    bRequest = pUsbdCtxt->setupReq.bRequest;
    wLength  = pUsbdCtxt->setupReq.wLength;
    wValue   = pUsbdCtxt->setupReq.wValue;
    wIndex = pUsbdCtxt->setupReq.wIndex;

#if USE_WINUSB
    /* Handle Microsoft OS String Descriptor request. */
    if ((bTarget == CY_USB_CTRL_REQ_RECIPENT_DEVICE) &&
            (bRequest == CY_USB_SC_GET_DESCRIPTOR) &&
            (wValue == ((CY_USB_STRING_DSCR << 8) | 0xEE))) {

        /* Make sure we do not send more data than requested. */
        if (wLength > glOsString[0]) {
            wLength = glOsString[0];
        }

        DBG_APP_INFO("OSString\r\n");
        retStatus = Cy_USB_USBD_SendEndp0Data(pUsbdCtxt, (uint8_t *)glOsString, wLength);
        if(retStatus == CY_USBD_STATUS_SUCCESS) {
            isReqHandled = true;
        }
    }

    /* Handle OS Compatibility and OS Feature requests */
    if (bRequest == MS_VENDOR_CODE) {
        if (wIndex == 0x04) {

            if (wLength > *((uint16_t *)glOsCompatibilityId)) {
                wLength = *((uint16_t *)glOsCompatibilityId);
            }

            DBG_APP_INFO("OSCompat\r\n");
            retStatus = Cy_USB_USBD_SendEndp0Data(pUsbdCtxt, (uint8_t *)glOsCompatibilityId, wLength);
            if(retStatus == CY_USBD_STATUS_SUCCESS) {
                isReqHandled = true;
            }
        }
        else if (wIndex == 0x05) {

            if (wLength > *((uint16_t *)glOsFeature)) {
                wLength = *((uint16_t *)glOsFeature);
            }

            DBG_APP_INFO("OSFeature\r\n");
            retStatus = Cy_USB_USBD_SendEndp0Data(pUsbdCtxt, (uint8_t *)glOsFeature, wLength);
            if(retStatus == CY_USBD_STATUS_SUCCESS) {
                isReqHandled = true;
            }
        }
        if (isReqHandled) {
            return;
        }
    }
#endif /* USE_WINUSB */

    /* SET_SEL request is supposed to have an OUT data phase of 6 bytes. */
    if ((bRequest == CY_USB_SC_SET_SEL) && (wLength == 6)) {
        /* SET_SEL request is only received in USBSS case and the Cy_USB_USBD_RecvEndp0Data is blocking. */
        retStatus = Cy_USB_USBD_RecvEndp0Data(pUsbdCtxt, (uint8_t *)SetSelDataBuffer, wLength);
        DBG_APP_INFO("SET_SEL: EP0 recv stat = %d, Data=%x:%x\r\n",
                retStatus, SetSelDataBuffer[0], SetSelDataBuffer[1]);
        isReqHandled = true;
    }

    if (((bReqType & CY_USB_CTRL_REQ_TYPE_MASK) >> CY_USB_CTRL_REQ_TYPE_POS) == CY_USB_CTRL_REQ_VENDOR)
    {
        Cy_USB_AppSignalTask(pUsbApp, EV_VENDOR_REQUEST);

        /* Vendor commands to be handled in task thread. */
        isReqHandled = true;
    }

    if (bRequest == CY_USB_SC_SET_FEATURE) {
        if (
                (bTarget == CY_USB_CTRL_REQ_RECIPENT_ENDP) &&
                (wValue == CY_USB_FEATURE_ENDP_HALT)
           ) {
            epDir = ((wIndex & 0x80UL) ? (CY_USB_ENDP_DIR_IN) : (CY_USB_ENDP_DIR_OUT));
            Cy_USB_USBD_EndpSetClearStall(pUsbdCtxt, ((uint32_t)wIndex & 0x7FUL), epDir, true);
            Cy_USBD_SendACkSetupDataStatusStage(pUsbdCtxt);
            isReqHandled = true;
        }

        if (bTarget == CY_USB_CTRL_REQ_RECIPENT_DEVICE) {
            if (wValue == CY_USB_FEATURE_DEVICE_REMOTE_WAKE) {
                /* Set Remote Wakeup enable: ACK the request. State is tracked by stack. */
                Cy_USBD_SendACkSetupDataStatusStage(pUsbdCtxt);
                isReqHandled = true;
            }

            if ((wValue == CY_USB_FEATURE_U1_ENABLE) || (wValue == CY_USB_FEATURE_U2_ENABLE)) {
                /* Set U1/U2 enable. ACK the request. Enable LPM if allowed by configuration. */
                Cy_USBD_SendACkSetupDataStatusStage(pUsbdCtxt);
                isReqHandled = true;
            }
        }

        if ((bTarget == CY_USB_CTRL_REQ_RECIPENT_INTF) && (wValue == CY_USB_FEATURE_FUNC_SUSPEND)) {
            /* We only support one interface. */
            if ((wIndex & 0xFFU) == 0) {
                Cy_USBD_SendACkSetupDataStatusStage(pUsbdCtxt);
                isReqHandled = true;
            }
        }
    }

    if (bRequest == CY_USB_SC_CLEAR_FEATURE) {
        if (
                (bTarget == CY_USB_CTRL_REQ_RECIPENT_ENDP) &&
                (wValue == CY_USB_FEATURE_ENDP_HALT)
           ) {
            DBG_APP_INFO("ClearFeature: %x\r\n", wIndex);
            epDir    = ((wIndex & 0x80UL) ? (CY_USB_ENDP_DIR_IN) : (CY_USB_ENDP_DIR_OUT));
            epNumber = (uint32_t)(wIndex & 0x7FUL);

            if ((epDir == CY_USB_ENDP_DIR_IN) && (epNumber == LVDS_STREAMING_EP)) {
                Cy_USB_AppSignalTask(pUsbApp, EV_STREAM_RESET);
            } else {
                /* Clear the stall condition in the EP-CS register. */
                Cy_USB_USBD_EndpSetClearStall(pUsbdCtxt, epNumber, epDir, false);
            }

            Cy_USBD_SendACkSetupDataStatusStage(pUsbdCtxt);
            isReqHandled = true;
        }

        if (bTarget == CY_USB_CTRL_REQ_RECIPENT_DEVICE) {
            if (wValue == CY_USB_FEATURE_DEVICE_REMOTE_WAKE) {
                /* Clear Remote Wakeup enable: ACK the request. State is tracked by stack. */
                Cy_USBD_SendACkSetupDataStatusStage(pUsbdCtxt);
                isReqHandled = true;
            }

            if ((wValue == CY_USB_FEATURE_U1_ENABLE) || (wValue == CY_USB_FEATURE_U2_ENABLE)) {
                /* Clear U1/U2 enable. ACK the request. Enable LPM if allowed by configuration. */
                Cy_USBD_SendACkSetupDataStatusStage(pUsbdCtxt);
                isReqHandled = true;
            }
        }

        if ((bTarget == CY_USB_CTRL_REQ_RECIPENT_INTF) && (wValue == CY_USB_FEATURE_FUNC_SUSPEND)) {
            /* We only support one interface. */
            if ((wIndex & 0xFFU) == 0) {
                Cy_USBD_SendACkSetupDataStatusStage(pUsbdCtxt);
                isReqHandled = true;
            }
        }
    }

    /*
     * If Request is not handled by the callback, Stall the command.
     */
    if(!isReqHandled) {
        Cy_USB_USBD_EndpSetClearStall(pUsbdCtxt, 0x00, CY_USB_ENDP_DIR_IN, TRUE);
    }
}

/*******************************************************************************
 * Function Name: Cy_USB_AppVendorRqtHandler
 ****************************************************************************//**
 *
 * Function to handle vendor specific control requests.
 *
 * \param pAppCtxt
 * USB application context structure.
 *
 *******************************************************************************/
void Cy_USB_AppVendorRqtHandler (cy_stc_usb_app_ctxt_t *pAppCtxt)
{
    cy_stc_usb_usbd_ctxt_t *pUsbdCtxt = pAppCtxt->pUsbdCtxt;
    cy_en_usbd_ret_code_t retStatus = CY_USBD_STATUS_FAILURE;
    uint16_t wLength, wValue, wIndex;
    uint8_t  bRequest, bReqType;
    bool     isReqHandled = false;
    uint16_t loopCnt = 250u;
    uint32_t baseAddr;
    uint16_t i;

    bReqType = pUsbdCtxt->setupReq.bmRequest;
    bRequest = pUsbdCtxt->setupReq.bRequest;
    wLength  = pUsbdCtxt->setupReq.wLength;
    wValue   = pUsbdCtxt->setupReq.wValue;
    wIndex   = pUsbdCtxt->setupReq.wIndex;

    if ((bRequest == DEVICE_RESET_CODE) && (bReqType == 0x40)) {
        /*
         * Device reset request:
         * 1. Get delay information.
         * 2. Initiate Status stage ACK.
         * 3. Do the reset functionality.
         */
        Cy_USBD_SendACkSetupDataStatusStage(pUsbdCtxt);
        isReqHandled = true;

        /* Wait for wValue + 1 ms  */
        Cy_SysLib_DelayUs(((wValue + 1) * 1000));
        NVIC_SystemReset();
    }

    /* Command to fetch SRAM content or MMIO registers for debug. */
    if (((bRequest == REG_MEMORY_READ_CODE) || (bRequest == REG_MEMORY_WRITE_CODE))
            &&  (wLength != 0) &&  (wLength <= 4096U) &&  ((wLength & 0x03) == 0))
    {
        if((bReqType & 0x80U) != 0)
        {
            baseAddr = (wValue << 16U) | wIndex;

            if ((baseAddr >= CY_HBW_SRAM_BASE_ADDR) &&
                    (baseAddr < CY_HBW_SRAM_LAST_ADDR))
            {
                Cy_HBDma_EvictReadCache(false);
                retStatus = Cy_USB_USBD_SendEndp0Data(pUsbdCtxt,
                        (uint8_t *)baseAddr,
                        wLength);
            } else if (Cy_USB_IsValidMMIOAddr(baseAddr)) {
                DBG_APP_INFO("Vendor Command 0x%x: Read from 0x%x\r\n", bRequest, baseAddr);
                for (i = 0; i < wLength / 4; i++) {
                    Ep0TempBuffer[i] = ((uint32_t *)baseAddr)[i];
                }
                retStatus = Cy_USB_USBD_SendEndp0Data(pUsbdCtxt,
                        (uint8_t *)Ep0TempBuffer,
                        wLength);
            }
            if (retStatus == CY_USBD_STATUS_SUCCESS) {
                isReqHandled = true;
            }
        }

        if ((bReqType & 0x80U) == 0)
        {
            baseAddr = (wValue << 16U) | wIndex;

            if ((baseAddr < CY_HBW_SRAM_BASE_ADDR) || (baseAddr >= CY_HBW_SRAM_LAST_ADDR))
            {
                /* Since we cannot use High BandWidth DMA to get data directly into non
                 * High BandWidth RAM regions, get the data into Ep0TempBuffer first and then copy
                 * it where it is supposed to go.
                 */
                retStatus = Cy_USB_USBD_RecvEndp0Data(pUsbdCtxt, (uint8_t *)Ep0TempBuffer, wLength);

                if (retStatus == CY_USBD_STATUS_SUCCESS) {
                    /* Wait until receive DMA transfer has been completed. */
                    while ((!Cy_USBD_IsEp0ReceiveDone(pUsbdCtxt)) && (loopCnt--)) {
                        vTaskDelay(1);
                    }

                    if (!Cy_USBD_IsEp0ReceiveDone(pUsbdCtxt)) {
                        Cy_USB_USBD_RetireRecvEndp0Data(pUsbdCtxt);
                    } else {
                        isReqHandled = true;

                        for (i = 0; i < wLength / 4; i++) {
                            ((uint32_t *)baseAddr)[i] = Ep0TempBuffer[i];
                        }
                    }
                }
            }
            else if (Cy_USB_IsValidMMIOAddr(baseAddr))
            {
                DBG_APP_INFO("Vendor Command 0x%x: Write to 0x%x\r\n", bRequest, baseAddr);
                retStatus = Cy_USB_USBD_RecvEndp0Data(pUsbdCtxt, (uint8_t *)baseAddr, wLength);

                if (retStatus == CY_USBD_STATUS_SUCCESS) {
                    /* Wait until receive DMA transfer has been completed. */
                    while ((!Cy_USBD_IsEp0ReceiveDone(pUsbdCtxt)) && (loopCnt--)) {
                        vTaskDelay(1);
                    }

                    if (!Cy_USBD_IsEp0ReceiveDone(pUsbdCtxt)) {
                        Cy_USB_USBD_RetireRecvEndp0Data(pUsbdCtxt);
                    } else {
                        isReqHandled = true;
                    }
                }
            }
        }
    }

    if ((bRequest == GET_DEVSPEED_CMD) && (wLength == 2)) {
        uint8_t *rspBuf_p = (uint8_t *)Ep0TempBuffer;

        rspBuf_p[0] = pUsbdCtxt->devSpeed;
        rspBuf_p[1] = USB_CONN_TYPE;
        retStatus = Cy_USB_USBD_SendEndp0Data(pUsbdCtxt, rspBuf_p, wLength);
        if (retStatus == CY_USBD_STATUS_SUCCESS) {
            isReqHandled = true;
        }
    }

    if ((bRequest == BOOT_MODE_RQT_CODE) && (wLength == 0)) {
        /* Set the boot mode request signature in RAM and reset to return to BL. */
        *((volatile uint32_t *)0x080003C0UL) = 0x544F4F42UL;
        *((volatile uint32_t *)0x080003C4UL) = 0x45444F4DUL;
        NVIC_SystemReset();
    }

    /* Request used to test out vendor specific control request handling. */
    if ((bRequest == DATA_XFER_TEST_CODE) && (wLength != 0) &&
            ((wValue & 0x3) == 0) && ((wValue + wLength) <= 4096U))
    {
        if ((bReqType & 0x80) != 0)
        {
            retStatus = Cy_USB_USBD_SendEndp0Data(pUsbdCtxt, ((uint8_t *)Ep0TempBuffer) + wValue, wLength);
        }
        else
        {
            retStatus = Cy_USB_USBD_RecvEndp0Data(pUsbdCtxt, ((uint8_t *)Ep0TempBuffer) + wValue, wLength);

            /* Wait until receive DMA transfer has been completed. */
            while ((!Cy_USBD_IsEp0ReceiveDone(pUsbdCtxt)) && (loopCnt--)) {
                Cy_SysLib_DelayUs(10);
            }

            if (!Cy_USBD_IsEp0ReceiveDone(pUsbdCtxt)) {
                Cy_USB_USBD_RetireRecvEndp0Data(pUsbdCtxt);
                Cy_USB_USBD_EndpSetClearStall(pUsbdCtxt, 0x00, CY_USB_ENDP_DIR_IN, true);
                return;
            }
        }

        if (retStatus == CY_USBD_STATUS_SUCCESS)
        {
            isReqHandled = true;
        }
        else
        {
            DBG_APP_ERR("EP0FAIL\r\n");
        }
    }

    if (!isReqHandled) {
        Cy_USB_USBD_EndpSetClearStall(pUsbdCtxt, 0x00, CY_USB_ENDP_DIR_IN, true);
    }
}

/*******************************************************************************
 * Function name: Cy_USB_AppSuspendCallback
 ****************************************************************************//**
 *
 * This function will be called by USBD layer when USB link is suspended.
 *
 * \param pAppCtxt
 * Application layer context pointer.
 *
 * \param pUsbdCtxt
 * USBD context
 *
 * \param pMsg
 * USB Message
 *
 ********************************************************************************/
static void
Cy_USB_AppSuspendCallback (
        void *pAppCtxt,
        cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
        cy_stc_usb_cal_msg_t *pMsg)
{
    cy_stc_usb_app_ctxt_t *pUsbApp;

    pUsbApp = (cy_stc_usb_app_ctxt_t *)pAppCtxt;
    pUsbApp->prevDevState = pUsbApp->devState;
    pUsbApp->devState = CY_USB_DEVICE_STATE_SUSPEND;

    /* Signal the task that device state has changed. */
    Cy_USB_AppSignalTask(pUsbApp, EV_DEVSTATE_CHG);
}

/*******************************************************************************
 * Function name: Cy_USB_AppResumeCallback
 ****************************************************************************//**
 *
 * This function will be called by USBD layer when USB link is resumed.
 *
 * \param pAppCtxt
 * Application layer context pointer.
 *
 * \param pUsbdCtxt
 * USBD context
 *
 * \param pMsg
 * USB Message
 *
 ********************************************************************************/
static void
Cy_USB_AppResumeCallback (
        void *pAppCtxt,
        cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
        cy_stc_usb_cal_msg_t *pMsg)
{
    cy_stc_usb_app_ctxt_t *pUsbApp;
    cy_en_usb_device_state_t tempState;

    pUsbApp = (cy_stc_usb_app_ctxt_t *)pAppCtxt;

    tempState =  pUsbApp->devState;
    pUsbApp->devState = pUsbApp->prevDevState;
    pUsbApp->prevDevState = tempState;

    /* Signal the task that device state has changed. */
    Cy_USB_AppSignalTask(pUsbApp, EV_DEVSTATE_CHG);
}


/*******************************************************************************
 * Function Name: Cy_USB_AppRegisterCallback
 ****************************************************************************//**
 *
 * Function to register USBD stack callback functions.
 *
 * \param pAppCtxt
 * USB application context structure.
 *
 *******************************************************************************/
void
Cy_USB_AppRegisterCallback (cy_stc_usb_app_ctxt_t *pAppCtxt)
{
    cy_stc_usb_usbd_ctxt_t *pUsbdCtxt = pAppCtxt->pUsbdCtxt;

    /* Register callbacks for various events sent by USBD stack. */
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_RESET, Cy_USB_AppBusResetCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_RESET_DONE, Cy_USB_AppBusResetDoneCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_BUS_SPEED, Cy_USB_AppBusSpeedCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_SETUP, Cy_USB_AppSetupCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_SUSPEND, Cy_USB_AppSuspendCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_RESUME, Cy_USB_AppResumeCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_SET_CONFIG, Cy_USB_AppSetCfgCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_SET_INTF, Cy_USB_AppSetIntfCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_L1_SLEEP, Cy_USB_AppL1SleepCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_L1_RESUME, Cy_USB_AppL1ResumeCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_ZLP, Cy_USB_AppZlpCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_SLP, Cy_USB_AppSlpCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_SETADDR, Cy_USB_AppSetAddressCallback);
}

/*******************************************************************************
 * Function name: Cy_USB_AppTerminateDma
 ****************************************************************************//**
 *
 * Abort any ongoing DMA transfers and return DMA state to default.
 *
 * \param pAppCtxt
 * Application layer context pointer.
 *
 * \param endpNumber
 * Endpoint index.
 *
 * \param endpDirection
 * Direction of the endpoint.
 *
 ********************************************************************************/
void
Cy_USB_AppTerminateDma (cy_stc_usb_app_ctxt_t *pAppCtxt, uint8_t endpNumber, cy_en_usb_endp_dir_t endpDirection)
{
    cy_stc_app_endp_dma_set_t *dmaset_p;

    /* Null pointer checks. */
    if (
            (pAppCtxt == NULL) ||
            (pAppCtxt->pCpuDw0Base == NULL) ||
            (pAppCtxt->pCpuDw1Base == NULL) ||
            (pAppCtxt->pCpuDmacBase == NULL)
       )
    {
        DBG_APP_ERR("TerminateDma:NULL\r\n");
        return;
    }

    if (endpDirection == CY_USB_ENDP_DIR_OUT) {
        dmaset_p = &(pAppCtxt->endpOutDma[endpNumber]);
        if (dmaset_p->valid) {
            Cy_HBDma_Channel_Disable(&(dmaset_p->hbDmaChannel));
        }
    } else {
        dmaset_p = &(pAppCtxt->endpInDma[endpNumber]);
        if (dmaset_p->valid) {
            Cy_HBDma_Channel_Disable(&(dmaset_p->hbDmaChannel));
        }
    }
}

/*******************************************************************************
 * Function name: Cy_USB_AppInitDmaIntr
 ****************************************************************************//**
 *
 * Register ISR for DataWire channel associated with a USB-HS endpoint.
 *
 * \param pAppCtxt
 * application layer context pointer.
 *
 * \param endpNumber
 * Endpoint index.
 *
 * \param endpDirection
 * Endpoint direction
 *
 * \param userIsr
 * ISR function pointer
 *
 ********************************************************************************/
void
Cy_USB_AppInitDmaIntr (cy_stc_usb_app_ctxt_t *pAppCtxt, uint32_t endpNumber,
        cy_en_usb_endp_dir_t endpDirection, cy_israddress userIsr)
{
    cy_stc_sysint_t intrCfg;

    if ((pAppCtxt != NULL) && (endpNumber > 0) && (endpNumber < CY_USB_MAX_ENDP_NUMBER)) {

        intrCfg.intrPriority = 5;
        if (endpDirection == CY_USB_ENDP_DIR_IN) {
            intrCfg.intrSrc = (IRQn_Type)(cpuss_interrupts_dw1_0_IRQn + endpNumber);
        } else {
            intrCfg.intrSrc = (IRQn_Type)(cpuss_interrupts_dw0_0_IRQn + endpNumber);
        }

        if (userIsr != NULL)  {
            /* If an ISR is provided, register it and enable the interrupt. */
            Cy_SysInt_Init(&intrCfg, userIsr);
            NVIC_EnableIRQ(intrCfg.intrSrc);
        } else {
            /* ISR is NULL. Disable the interrupt. */
            NVIC_DisableIRQ(intrCfg.intrSrc);
        }
    }
}

/*[]*/
