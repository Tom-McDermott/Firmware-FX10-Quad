/***************************************************************************//**
* \file main.c
* \version 1.0
*
* Main source file for the EZ-USB FX LVDS to USB data streaming application.
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
#include "timers.h"
#include "cy_pdl.h"
#include <string.h>
#include "cy_usb_common.h"
#include "cy_usbhs_cal_drv.h"
#include "cy_usb_usbd.h"
#include "cy_usb_app.h"
#include "cy_usbd_version.h"
#include "cy_fault_handlers.h"
#include "cy_debug.h"
#include "cy_hbdma.h"
#include "cy_hbdma_mgr.h"
#include "cy_hbdma_version.h"
#include "cy_lvds.h"
#include "cy_fx_common.h"
#include "cy_usb_qspi.h"
#include "cybsp.h"
#include "cy_gpif_header_lvds.h"
#include "../app_version.h"

cy_stc_usb_usbd_ctxt_t          usbdCtxt;
cy_stc_usb_cal_ctxt_t           hsCalCtxt;
cy_stc_usbss_cal_ctxt_t         ssCalCtxt;
cy_stc_hbdma_context_t          HBW_DrvCtxt;            /* High BandWidth DMA driver context. */
cy_stc_hbdma_dscr_list_t        HBW_DscrList;           /* High BandWidth DMA descriptor free list. */
cy_stc_hbdma_buf_mgr_t          HBW_BufMgr;             /* High BandWidth DMA buffer manager. */
cy_stc_hbdma_mgr_context_t      HBW_MgrCtxt;            /* High BandWidth DMA manager context. */
cy_stc_usb_app_ctxt_t           appCtxt;
cy_stc_lvds_context_t           lvdsContext;

#if (!USBFS_LOGS_ENABLE)
/* Context structure for the debug logging UART module (where used). */
static cy_stc_scb_uart_context_t glDbgUartCtxt;
#endif /* (!USBFS_LOGS_ENABLE) */

/* RAM buffer used to hold debug log data. */
#define LOGBUF_RAM_SZ           (1024U)
volatile uint32_t LogDataBuffer[LOGBUF_RAM_SZ / 4U];

/*****************************************************************************
 * Function Name: Cy_LVDS_GpifEventCb
 *****************************************************************************
 *
 * GPIF event callback function.
 *
 * \param smNo
 * State machine number
 *
 * \param gpifEvent
 * GPIF event type
 *
 * \param cntxt
 * App context
 *
 ****************************************************************************/
void Cy_LVDS_GpifEventCb (uint8_t smNo, cy_en_lvds_gpif_event_type_t gpifEvent, void *cntxt)
{
    DBG_APP_INFO("GPIF SM interrupt\r\n");
}

/*****************************************************************************
 * Function Name: Cy_LVDS_PhyEventCb
 *****************************************************************************
 *
 * LVDS PHY event callback function.
 *
 * \param smNo
 * State machine number
 *
 * \param phyEvent
 * PHY event type
 *
 * \param cntxt
 * App context
 *
 ****************************************************************************/
void Cy_LVDS_PhyEventCb (uint8_t smNo, cy_en_lvds_phy_events_t phyEvent, void *cntxt)
{
    cy_en_scb_i2c_status_t status;
    cy_stc_usb_app_ctxt_t *pAppCtxt = (cy_stc_usb_app_ctxt_t *)cntxt;

    if (pAppCtxt == NULL) {
        return;
    }

    if (phyEvent == CY_LVDS_PHY_L3_ENTRY)
    {
        if (smNo == 0)
        {
            DBG_APP_INFO("P0_L3_Entry\r\n");
        }
        else
        {
            DBG_APP_INFO("P1_L3_Entry\r\n");
        }
    }

    if (phyEvent == CY_LVDS_PHY_TRAINING_DONE)
    {
        pAppCtxt->isPhyTrainingDone = true;
    }

    if (phyEvent == CY_LVDS_PHY_LNK_TRAIN_BLK_DET)
    {
        DBG_APP_INFO("Port %d Training Block Detected\r\n",smNo);
        if (smNo)
        {
            pAppCtxt->isLinkTrainingDone |= (1 << smNo);
            SET_BIT(pAppCtxt->fpgaTrainingCtrl, P1_TRAINING_DONE);
        }
        else
        {
            pAppCtxt->isLinkTrainingDone |= (1 << smNo);
            SET_BIT(pAppCtxt->fpgaTrainingCtrl, P0_TRAINING_DONE);
        }

        status = Cy_I2C_Write(FPGASLAVE_ADDR, FPGA_PHY_LINK_CONTROL_ADDRESS,
                pAppCtxt->fpgaTrainingCtrl, FPGA_I2C_ADDRESS_WIDTH, FPGA_I2C_DATA_WIDTH);
        ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    }

    if (phyEvent == CY_LVDS_PHY_LNK_TRAIN_BLK_DET_FAIL)
    {
        DBG_APP_ERR("Port %d Training Block Detect Failed\r\n", smNo);
    }
}

/*****************************************************************************
 * Function Name: Cy_LVDS_LowPowerEventCb
 *****************************************************************************
 *
 * LVDS low power event callback function.
 *
 * \param lowPowerEvent
 * Type of low power event received
 *
 * \param cntxt
 * App context
 *
 ****************************************************************************/
void Cy_LVDS_LowPowerEventCb (cy_en_lvds_low_power_events_t lowPowerEvent, void *cntxt)
{
    if (lowPowerEvent == CY_LVDS_LOW_POWER_LNK0_L3_EXIT)
    {
        DBG_APP_INFO("P0_L3_Exit\r\n");
    }

    if (lowPowerEvent == CY_LVDS_LOW_POWER_LNK1_L3_EXIT)
    {
        DBG_APP_INFO("P1_L3_Exit\r\n");
    }
}

/*****************************************************************************
 * Function Name: Cy_LVDS_GpifErrorCb
 *****************************************************************************
 *
 * GPIF error callback function.
 *
 * \param smNo
 * State machine number
 *
 * \param gpifError
 * GPIF error type
 *
 * \param cntxt
 * App context
 *
 ****************************************************************************/
void Cy_LVDS_GpifErrorCb (uint8_t smNo, cy_en_lvds_gpif_error_t gpifError, void *cntxt)
{
    switch (gpifError)
    {
        case CY_LVDS_GPIF_ERROR_IN_ADDR_OVER_WRITE:
            DBG_APP_ERR("CY_LVDS_GPIF_ERROR_IN_ADDR_OVER_WRITE\n\r");
            break;

        case CY_LVDS_GPIF_ERROR_EG_ADDR_NOT_VALID:
            DBG_APP_ERR("CY_LVDS_GPIF_ERROR_EG_ADDR_NOT_VALID\n\r");
            break;

        case CY_LVDS_GPIF_ERROR_DMA_DATA_RD_ERROR:
            DBG_APP_ERR("CY_LVDS_GPIF_ERROR_DMA_DATA_RD_ERROR\n\r");
            break;

        case CY_LVDS_GPIF_ERROR_DMA_DATA_WR_ERROR:
            DBG_APP_ERR("CY_LVDS_GPIF_ERROR_DMA_DATA_WR_ERROR\n\r");
            break;

        case CY_LVDS_GPIF_ERROR_DMA_ADDR_RD_ERROR:
            DBG_APP_ERR("CY_LVDS_GPIF_ERROR_DMA_DATA_WR_ERROR\n\r");
            break;

        case CY_LVDS_GPIF_ERROR_DMA_ADDR_WR_ERROR:
            DBG_APP_ERR("CY_LVDS_GPIF_ERROR_DMA_ADDR_WR_ERROR\n\r");
            break;

        case CY_LVDS_GPIF_ERROR_INVALID_STATE_ERROR:
            DBG_APP_ERR("CY_LVDS_GPIF_ERROR_INVALID_STATE_ERROR\n\r");
            break;
    }
}

/*****************************************************************************
 * Function Name: Cy_LVDS_GpifThreadErrorCb
 *****************************************************************************
 *
 * GPIF thread error callback function.
 *
 * \param threadNum
 * Thread number
 *
 * \param errType
 * Type of error detected.
 *
 * \param cntxt
 * App context
 *
 ****************************************************************************/
void Cy_LVDS_GpifThreadErrorCb (cy_en_lvds_gpif_thread_no_t threadNum, cy_en_lvds_gpif_thread_error_t errType,
        void *cntxt)
{
    switch (errType)
    {
        case CY_LVDS_GPIF_THREAD_DIR_ERROR:
            DBG_APP_ERR("Thread: %d - CY_LVDS_GPIF_THREAD_DIR_ERROR\n\r",threadNum);
            break;

        case CY_LVDS_GPIF_THREAD_WR_OVERFLOW:
            DBG_APP_ERR("Thread: %d -CY_LVDS_GPIF_THREAD_WR_OVERFLOW\n\r",threadNum);
            break;

        case CY_LVDS_GPIF_THREAD_RD_UNDERRUN:
            DBG_APP_ERR("Thread: %d -CY_LVDS_GPIF_THREAD_RD_UNDERRUN\n\r",threadNum);
            break;

        case CY_LVDS_GPIF_THREAD_SCK_ACTIVE:
            DBG_APP_ERR("Thread: %d -CY_LVDS_GPIF_THREAD_SCK_ACTIVE\n\r",threadNum);
            break;

        case CY_LVDS_GPIF_THREAD_ADAP_OVERFLOW:
            DBG_APP_ERR("Thread: %d -CY_LVDS_GPIF_THREAD_ADAP_OVERFLOW\n\r",threadNum);
            break;

        case CY_LVDS_GPIF_THREAD_ADAP_UNDERFLOW:
            DBG_APP_ERR("Thread: %d -CY_LVDS_GPIF_THREAD_ADAP_UNDERFLOW\n\r",threadNum);
            break;

        case CY_LVDS_GPIF_THREAD_READ_FORCE_END:
            DBG_APP_ERR("Thread: %d -CY_LVDS_GPIF_THREAD_READ_FORCE_END\n\r",threadNum);
            break;

        case CY_LVDS_GPIF_THREAD_READ_BURST_ERR:
            DBG_APP_ERR("Thread: %d -CY_LVDS_GPIF_THREAD_READ_BURST_ERR\n\r",threadNum);
            break;
    }
}

/* LVDS callback structure. */
cy_stc_lvds_app_cb_t glAppLvdsCallbacks =
{
    .gpif_events = Cy_LVDS_GpifEventCb,
    .gpif_error = Cy_LVDS_GpifErrorCb,
    .gpif_thread_error = Cy_LVDS_GpifThreadErrorCb,
    .gpif_thread_event = NULL,
    .phy_events = Cy_LVDS_PhyEventCb,
    .low_power_events   = Cy_LVDS_LowPowerEventCb
};

/*
 * Notes on DMA Buffer RAM Usage:
 * 1. The initial part of the buffer RAM is reserved for the descriptors used by the DMA manager.
 *    The space used for this is reserved using the gHbDmaDescriptorSpace array which is placed
 *    in a section named ".hbDmaDescriptor". This array should have a minimum size of 8192 (8 KB)
 *    and has a default size allocation of 16384 bytes (16 KB). No other data should be placed
 *    in this section.
 *
 * 2. The descriptor region is followed by RW data structures which are placed in the ".descSection".
 *    Only data members placed in this section will be initialized during the firmware load
 *    process.
 *
 * 3. The ".descSection" is followed by the ".hbBufSection" which will hold data structures
 *    which do not need to be explicitly initialized (equivalent of ".bss" section).
 *
 * 4. This is followed by the ".hbDmaBufferHeap" section which will be used to allocate all
 *    the DMA buffers from. The gHbDmaBufferHeap array represents the memory region which will
 *    be given to the DMA buffer manager to allocate buffers from and can be sized based on the
 *    available memory. No other data or variables should be placed in this section.
 *
 * Any pre-initialized data which is to be placed in the High BandWidth Buffer RAM should be
 * added to the ".descSection". Any non-initialized data which is to be placed in the High
 * BandWidth Buffer RAM should be added to the ".hbBufSection".
 */

/* Region of 16 KB reserved for High BandWidth DMA descriptors. */
static __attribute__ ((section(".hbDmaDescriptor"), used)) uint32_t gHbDmaDescriptorSpace[16384 / 4];

#if CYFX_512K_RAM
/* Region of 448 KB reserved for DMA buffer heap. */
static __attribute__ ((section(".hbDmaBufferHeap"), used)) uint32_t gHbDmaBufferHeap[448 * 1024 / 4];
#else
/* Region of 960 KB reserved for DMA buffer heap. */
static __attribute__ ((section(".hbDmaBufferHeap"), used)) uint32_t gHbDmaBufferHeap[960 * 1024 / 4];
#endif /* CYFX_512K_RAM */

/*******************************************************************************
 * Function name: InitHbDma
 ****************************************************************************//**
 *
 * Initialize the High BandWidth DMA manager and related structures.
 *
 * \return
 * true if initialization is completed successfully, false otherwise.
 *
 ********************************************************************************/
bool InitHbDma (void)
{
    cy_en_hbdma_status_t      drvstat;
    cy_en_hbdma_mgr_status_t  mgrstat;

    /* Initialize the HBW DMA driver layer. Only USB DMA adapter is used. */
    drvstat = Cy_HBDma_Init(LVDSSS_LVDS, USB32DEV, &HBW_DrvCtxt, 0, 0);
    if (drvstat != CY_HBDMA_SUCCESS)
    {
        return false;
    }

    /* Verify that gHbDmaDescriptorSpace is located at the base of the DMA buffer SRAM. */
    if ((uint32_t)gHbDmaDescriptorSpace != CY_HBW_SRAM_BASE_ADDR) {
        LOG_ERROR("High BandWidth DMA descriptors not placed at the correct address\r\n");
        return false;
    }

    /* Setup a HBW DMA descriptor list using the space reserved in gHbDmaDescriptorSpace. */
    mgrstat = Cy_HBDma_DscrList_Create(&HBW_DscrList, sizeof(gHbDmaDescriptorSpace) / 16);
    if (mgrstat != CY_HBDMA_MGR_SUCCESS)
    {
        return false;
    }

    /* Initialize the DMA buffer manager to use the gHbDmaBufferHeap region. */
    mgrstat = Cy_HBDma_BufMgr_Create(&HBW_BufMgr, (uint32_t *)gHbDmaBufferHeap, sizeof(gHbDmaBufferHeap));
    if (mgrstat != CY_HBDMA_MGR_SUCCESS)
    {
        return false;
    }

    /* Initialize the HBW DMA channel manager. */
    mgrstat = Cy_HBDma_Mgr_Init(&HBW_MgrCtxt, &HBW_DrvCtxt, &HBW_DscrList, &HBW_BufMgr);
    if (mgrstat != CY_HBDMA_MGR_SUCCESS)
    {
        return false;
    }

    /* Request DMA callback handling in ISR context. */
    Cy_HBDma_Mgr_DmaCallbackConfigure(&HBW_MgrCtxt, true);

#if (!LVDS_LB_EN)
    /* Both LVDS DMA adapters are used only for ingress transfers. */
    Cy_HBDma_Mgr_SetLvdsAdapterIngressMode(&HBW_MgrCtxt, true, true);
#else
    /* LVDS adapter 0 is ingress only in this case. */
    Cy_HBDma_Mgr_SetLvdsAdapterIngressMode(&HBW_MgrCtxt, true, false);
#endif /* (!LVDS_LB_EN) */

    /*
     * Register USBD stack context structure with HBW DMA manager so that
     * we can use HBDma channels for USB-HS transfers as well.
     */
    Cy_HBDma_Mgr_RegisterUsbContext(&HBW_MgrCtxt, &usbdCtxt);

    return true;
}

extern void xPortPendSVHandler (void);
extern void xPortSysTickHandler (void);
extern void vPortSVCHandler (void);

/*******************************************************************************
 * Function name: SysTickIntrWrapper
 ****************************************************************************//**
 *
 * Wrapper function for SysTick timer interrupt. Increments the tick number maintained
 * by USBD stack and calls the FreeRTOS SysTick handler.
 *
 ********************************************************************************/
void SysTickIntrWrapper (void)
{
    Cy_USBD_TickIncrement(&usbdCtxt);
    xPortSysTickHandler();
}

/*****************************************************************************
 * Function Name: vPortSetupTimerInterrupt
 *****************************************************************************
 * Summary
 *  Function called by FreeRTOS kernel to start a timer used for task
 *  scheduling. We enable the SysTick interrupt with a period of 1 ms in
 *  this function.
 *
 ****************************************************************************/
void vPortSetupTimerInterrupt (void)
{
    /* Register the exception vectors. */
    Cy_SysInt_SetVector(PendSV_IRQn, xPortPendSVHandler);
    Cy_SysInt_SetVector(SVCall_IRQn, vPortSVCHandler);
    Cy_SysInt_SetVector(SysTick_IRQn, SysTickIntrWrapper);

    /* Start the SysTick timer with a period of 1 ms. */
    Cy_SysTick_SetClockSource(CY_SYSTICK_CLOCK_SOURCE_CLK_CPU);
    Cy_SysTick_SetReload(Cy_SysClk_ClkFastGetFrequency() / 1000U);
    Cy_SysTick_Clear();
    Cy_SysTick_Enable();
}

/*****************************************************************************
 * Function Name: VbusDetGpio_ISR
 *****************************************************************************
 *
 * Interrupt handler for the Vbus detect GPIO transition detection.
 *
 ****************************************************************************/
static void VbusDetGpio_ISR (void)
{
    /* Remember that VBus change has happened and disable the interrupt. */
    appCtxt.vbusChangeIntr = true;
    Cy_USBD_AddEvtToLog(&usbdCtxt, CY_SSCAL_EVT_VBUS_CHG_INTR);
    Cy_GPIO_SetInterruptMask(VBUS_DETECT_GPIO_PORT, VBUS_DETECT_GPIO_PIN, 0);
    Cy_USB_AppSignalTask(&appCtxt, EV_DEVSTATE_CHG);
}

/*****************************************************************************
 * Function Name: LvdsAdapter0_ISR
 *****************************************************************************
 *
 * Interrupt handler for the interrupt from LVDS DMA adapter #0.
 *
 ****************************************************************************/
void LvdsAdapter0_ISR (void)
{
    /* Data received from LVDS port. Force DMA ready status to off. */
    if (appCtxt.isLvdsWltoUsbHs) {
        Cy_LVDS_PhyGpioClr(LVDSSS_LVDS, 0, FX_DMA_RDY_IO);
        appCtxt.fwDmaReadyStatus = false;
    }

    Cy_HBDma_HandleInterrupts(&HBW_DrvCtxt, CY_HBDMA_ADAP_LVDS_0);
    portYIELD_FROM_ISR(true);
}

/*****************************************************************************
 * Function Name: LvdsAdapter1_ISR
 *****************************************************************************
 *
 * Interrupt handler for the interrupt from LVDS DMA adapter #1.
 *
 ****************************************************************************/
void LvdsAdapter1_ISR (void)
{
    /* Data received from LVDS port. Force DMA ready status to off. */
    if (appCtxt.isLvdsWltoUsbHs) {
        Cy_LVDS_PhyGpioClr(LVDSSS_LVDS, 0, FX_DMA_RDY_IO);
        appCtxt.fwDmaReadyStatus = false;
    }

    Cy_HBDma_HandleInterrupts(&HBW_DrvCtxt, CY_HBDMA_ADAP_LVDS_1);
    portYIELD_FROM_ISR(true);
}

/*****************************************************************************
 * Function Name: LvdsLpm_ISR
 *****************************************************************************
 *
 * Interrupt handler for LVDS low power interrupts.
 *
 ****************************************************************************/
void LvdsLpm_ISR (void)
{
    Cy_LVDS_LowPowerIrqHandler(LVDSSS_LVDS, &lvdsContext);
}

/*****************************************************************************
 * Function Name: Lvds_ISR
 *****************************************************************************
 *
 * Interrupt handler for general LVDS block interrupts.
 *
 ****************************************************************************/
void Lvds_ISR (void)
{
    Cy_LVDS_IrqHandler(LVDSSS_LVDS, &lvdsContext);
}

/*****************************************************************************
 * Function Name: InitPeripherals
 *****************************************************************************
 *
 * Initialize the logging interface and GPIOs
 *
 ****************************************************************************/
void InitPeripherals (void)
{
    cy_stc_gpio_pin_config_t pinCfg;
    cy_stc_sysint_t          intrCfg;
    cy_stc_debug_config_t    dbgCfg;

#if (!USBFS_LOGS_ENABLE)
    /* Initialize SCB for debug log output. */
    Cy_SCB_UART_Init(LOG_SCB_HW, &LOG_SCB_config, &glDbgUartCtxt);
    Cy_SCB_UART_Enable(LOG_SCB_HW);
#endif /* (!USBFS_LOGS_ENABLE) */

    dbgCfg.pBuffer   = (uint8_t *)LogDataBuffer;
    dbgCfg.traceLvl  = 3U;
    dbgCfg.bufSize   = LOGBUF_RAM_SZ;
#if USBFS_LOGS_ENABLE
    dbgCfg.dbgIntfce = CY_DEBUG_INTFCE_USBFS_CDC;
#else
    dbgCfg.dbgIntfce = CY_DEBUG_INTFCE_UART_SCB1;
#endif /* USBFS_LOGS_ENABLE */
    dbgCfg.printNow  = true;                    /* Printing messages immediately. */
    Cy_Debug_LogInit(&dbgCfg);
    Cy_SysLib_Delay(500);

    memset ((void *)&pinCfg, 0, sizeof(pinCfg));

    /* Configure VBus detect GPIO. */
    pinCfg.driveMode = CY_GPIO_DM_HIGHZ;
    pinCfg.hsiom     = HSIOM_SEL_GPIO;
    pinCfg.intEdge   = CY_GPIO_INTR_BOTH;
    pinCfg.intMask   = 0x01UL;
    (void)Cy_GPIO_Pin_Init(VBUS_DETECT_GPIO_PORT, VBUS_DETECT_GPIO_PIN, &pinCfg);

    memset ((void *)&pinCfg, 0, sizeof(pinCfg));

    /* Configure input GPIO. */
    pinCfg.driveMode = CY_GPIO_DM_HIGHZ;
    pinCfg.hsiom = HSIOM_SEL_GPIO;
   (void)Cy_GPIO_Pin_Init(TI180_CDONE_PORT, TI180_CDONE_PIN, &pinCfg);

    /* Configure RESET FPGA GPIO. */
    pinCfg.driveMode = CY_GPIO_DM_STRONG_IN_OFF;
    pinCfg.hsiom     = TI180_INIT_RESET_GPIO;
    (void)Cy_GPIO_Pin_Init(TI180_INIT_RESET_PORT, TI180_INIT_RESET_PIN, &pinCfg);

    Cy_GPIO_Clr(TI180_INIT_RESET_PORT, TI180_INIT_RESET_PIN);
    Cy_SysLib_Delay(20);
    Cy_GPIO_Set(TI180_INIT_RESET_PORT, TI180_INIT_RESET_PIN);

    /* Register edge detect interrupt for Vbus detect GPIO. */
    intrCfg.intrSrc = VBUS_DETECT_GPIO_INTR;
    intrCfg.intrPriority = 7;
    Cy_SysInt_Init(&intrCfg, VbusDetGpio_ISR);
    NVIC_EnableIRQ(intrCfg.intrSrc);

    /* Register the ISR and enable the interrupt for HBWDMA Adaptor 0. */
    intrCfg.intrSrc      = lvds2usb32ss_lvds_dma_adap0_int_o_IRQn;
    intrCfg.intrPriority = 3;
    Cy_SysInt_Init(&intrCfg, LvdsAdapter0_ISR);
    NVIC_EnableIRQ(lvds2usb32ss_lvds_dma_adap0_int_o_IRQn);

    /* Register the ISR and enable the interrupt for HBWDMA Adaptor 1. */
    intrCfg.intrSrc      = lvds2usb32ss_lvds_dma_adap1_int_o_IRQn;
    intrCfg.intrPriority = 3;
    Cy_SysInt_Init(&intrCfg, LvdsAdapter1_ISR);
    NVIC_EnableIRQ(lvds2usb32ss_lvds_dma_adap1_int_o_IRQn);

    /* Register the ISR and enable the interrupt for LVDS. */
    intrCfg.intrSrc      = lvds2usb32ss_lvds_int_o_IRQn;
    intrCfg.intrPriority = 6;
    Cy_SysInt_Init(&intrCfg, Lvds_ISR);
    NVIC_EnableIRQ(lvds2usb32ss_lvds_int_o_IRQn);

    intrCfg.intrSrc      = lvds2usb32ss_lvds_wakeup_int_o_IRQn;
    intrCfg.intrPriority = 0;
    Cy_SysInt_Init(&intrCfg, LvdsLpm_ISR);
    NVIC_EnableIRQ(lvds2usb32ss_lvds_wakeup_int_o_IRQn);
}

/*****************************************************************************
 * Function Name: PrintVersionInfo
 ******************************************************************************
 *
 * Function to print the stack and application version information.
 *
 * Parameters:
 *  type: Type of version string.
 *  typeLen: Length of version type string.
 *  vMajor: Major version number (0 - 99)
 *  vMinor: Minor version number (0 - 99)
 *  vPatch: Patch version number (0 - 99)
 *  vBuild: Build number (0 - 9999)
 *
 *****************************************************************************/
void PrintVersionInfo (const char *type, uint8_t typeLen,
                       uint8_t vMajor, uint8_t vMinor, uint8_t vPatch, uint16_t vBuild)
{
    char tString[32];

    memcpy(tString, type, typeLen);
    tString[typeLen++] = '0' + (vMajor / 10);
    tString[typeLen++] = '0' + (vMajor % 10);
    tString[typeLen++] = '.';
    tString[typeLen++] = '0' + (vMinor / 10);
    tString[typeLen++] = '0' + (vMinor % 10);
    tString[typeLen++] = '.';
    tString[typeLen++] = '0' + (vPatch / 10);
    tString[typeLen++] = '0' + (vPatch % 10);
    tString[typeLen++] = '.';
    tString[typeLen++] = '0' + (vBuild / 1000);
    tString[typeLen++] = '0' + ((vBuild % 1000) / 100);
    tString[typeLen++] = '0' + ((vBuild % 100) / 10);
    tString[typeLen++] = '0' + (vBuild % 10);
    tString[typeLen++] = '\r';
    tString[typeLen++] = '\n';
    tString[typeLen]   = 0;

    DBG_APP_INFO("%s", tString);
}

/*****************************************************************************
 * Function Name: UsbHS_ISR
 ******************************************************************************
 * Summary:
 *  Handler for USB-HS interrupts.
 *
 *****************************************************************************/
void UsbHS_ISR (void)
{
    if (Cy_USBHS_Cal_IntrHandler(&hsCalCtxt))
    {
        portYIELD_FROM_ISR(true);
    }
}

/*****************************************************************************
 * Function Name: UsbSS_ISR
 ******************************************************************************
 *
 * Handler for USB SuperSpeed block interrupts.
 *
 *****************************************************************************/
void UsbSS_ISR (void)
{
    Cy_USBSS_Cal_IntrHandler(&ssCalCtxt);
    portYIELD_FROM_ISR(true);
}

/*****************************************************************************
 * Function Name: UsbIngressDma_ISR
 ******************************************************************************
 *
 * Handler for USB Ingress DMA adapter interrupts.
 *
 *****************************************************************************/
void UsbIngressDma_ISR (void)
{
    /* Call the HBDMA interrupt handler with the appropriate adapter ID. */
    Cy_HBDma_HandleInterrupts(&HBW_DrvCtxt, CY_HBDMA_ADAP_USB_IN);
    portYIELD_FROM_ISR(true);
}

/*****************************************************************************
 * Function Name: UsbEgressDma_ISR
 ******************************************************************************
 *
 * Handler for USB Egress DMA adapter interrupts.
 *
 *****************************************************************************/
void UsbEgressDma_ISR (void)
{
    /* Call the HBDMA interrupt handler with the appropriate adapter ID. */
    Cy_HBDma_HandleInterrupts(&HBW_DrvCtxt, CY_HBDMA_ADAP_USB_EG);
    portYIELD_FROM_ISR(true);
}

/*****************************************************************************
 * Function Name: UsbDevInit
 *****************************************************************************
 *
 * Initialize USB device block and interrupts.
 *
 ****************************************************************************/
void UsbDevInit (void)
{
    const char start_string[] = "*** LVDS to USB Stream App Start ***\r\n";
    cy_stc_sysint_t intrCfg;

    /* Initialize the external peripheral interfaces. */
    InitPeripherals();

    /* Send a start-up string using the logging SCB. */
    DBG_APP_INFO("%s", start_string);

    /* Print application and USBD stack version information. */
    PrintVersionInfo("APP_VERSION: ", 13, APP_VERSION_MAJOR, APP_VERSION_MINOR,
            APP_VERSION_PATCH, APP_VERSION_BUILD);
    PrintVersionInfo("USBD_VERSION: ", 14, USBD_VERSION_MAJOR, USBD_VERSION_MINOR,
            USBD_VERSION_PATCH, USBD_VERSION_BUILD);
    PrintVersionInfo("HBDMA_VERSION: ", 15, HBDMA_VERSION_MAJOR, HBDMA_VERSION_MINOR,
            HBDMA_VERSION_PATCH, HBDMA_VERSION_BUILD);

    /* Initialize the I2C master block. */
    Cy_USB_I2CInit();

    /* Register ISR for and enable USBHS Interrupt. */
    intrCfg.intrSrc      = usbhsdev_interrupt_u2d_active_o_IRQn;
    intrCfg.intrPriority = 4;
    Cy_SysInt_Init(&intrCfg, UsbHS_ISR);
    NVIC_EnableIRQ(intrCfg.intrSrc);

    intrCfg.intrSrc      = usbhsdev_interrupt_u2d_dpslp_o_IRQn;
    intrCfg.intrPriority = 4;
    Cy_SysInt_Init(&intrCfg, UsbHS_ISR);
    NVIC_EnableIRQ(intrCfg.intrSrc);

    /* Register ISR for and enable USBSS Interrupt. */
    intrCfg.intrSrc      = lvds2usb32ss_usb32_int_o_IRQn;
    intrCfg.intrPriority = 4;
    Cy_SysInt_Init(&intrCfg, &UsbSS_ISR);
    NVIC_EnableIRQ(intrCfg.intrSrc);

    /* Map the USB32 wakeup interrupt to the same ISR. */
    intrCfg.intrSrc      = lvds2usb32ss_usb32_wakeup_int_o_IRQn;
    intrCfg.intrPriority = 4;
    Cy_SysInt_Init(&intrCfg, &UsbSS_ISR);
    NVIC_EnableIRQ(intrCfg.intrSrc);

    /* Register ISR for and enable USBSS Ingress DMA Interrupt. */
    intrCfg.intrSrc      = lvds2usb32ss_usb32_ingrs_dma_int_o_IRQn;
    intrCfg.intrPriority = 3;
    Cy_SysInt_Init(&intrCfg, &UsbIngressDma_ISR);
    NVIC_EnableIRQ(intrCfg.intrSrc);

    /* Register ISR for and enable USBSS Egress DMA Interrupt. */
    intrCfg.intrSrc      = lvds2usb32ss_usb32_egrs_dma_int_o_IRQn;
    intrCfg.intrPriority = 3;
    Cy_SysInt_Init(&intrCfg, &UsbEgressDma_ISR);
    NVIC_EnableIRQ(intrCfg.intrSrc);
}

/*****************************************************************************
 * Function Name: InEpDma_ISR
 ******************************************************************************
 *
 * Handler for DMA transfer completion on DataWire1 channels corresponding to
 * USB-HS IN endpoints. The same ISR can be used for all DataWire1 channels
 * mapped to USB-HS IN endpoints.
 *
 *****************************************************************************/
void InEpDma_ISR (void)
{
    /*
     * Just call the handler function in the DMA manager and then yield to
     * the next high priority thread.
     */
    Cy_HBDma_Mgr_HandleDW1Interrupt(&HBW_MgrCtxt);
    portYIELD_FROM_ISR(true);
}

/*****************************************************************************
 * Function Name: EnableEpDmaISRs
 ******************************************************************************
 *
 * Registers Interrupt Service Routines for the DMA (DW) DMA channels associated
 * with IN endpoints.
 *
 * \param pAppCtxt
 * Pointer to application context structure.
 *
 *****************************************************************************/
static void EnableEpDmaISRs (cy_stc_usb_app_ctxt_t *pAppCtxt)
{
    Cy_USB_AppInitDmaIntr(pAppCtxt, LVDS_STREAMING_EP, CY_USB_ENDP_DIR_IN, InEpDma_ISR);
}

/*****************************************************************************
 * Function Name: Cy_USBSS_DeInit
 ******************************************************************************
 *
 * Function to make sure USB32 block is disconnected from host and disabled
 * at start.
 *
 * \param pCalCtxt
 * Pointer to USB-SS CAL context.
 *
 *****************************************************************************/
void Cy_USBSS_DeInit (cy_stc_usbss_cal_ctxt_t *pCalCtxt)
{
    USB32DEV_Type *base = pCalCtxt->regBase;
    USB32DEV_MAIN_Type  *USB32DEV_MAIN = &base->USB32DEV_MAIN;

    /* Disable the clock for USB3.2 function */
    USB32DEV_MAIN->CTRL &= ~USB32DEV_MAIN_CTRL_CLK_EN_Msk;

    /* Disable PHYSS */
    base->USB32DEV_PHYSS.USB40PHY[0].USB40PHY_TOP.TOP_CTRL_0 &=
                    ~(USB32DEV_PHYSS_USB40PHY_TOP_CTRL_0_REG_PWR_GOOD_CORE_RX_Msk |
                     USB32DEV_PHYSS_USB40PHY_TOP_CTRL_0_REG_PWR_GOOD_CORE_PLL_Msk |
                     USB32DEV_PHYSS_USB40PHY_TOP_CTRL_0_REG_VBUS_Msk |
                     USB32DEV_PHYSS_USB40PHY_TOP_CTRL_0_REG_PHYSS_EN_Msk |
                     USB32DEV_PHYSS_USB40PHY_TOP_CTRL_0_PCLK_EN_Msk);

    base->USB32DEV_PHYSS.USB40PHY[1].USB40PHY_TOP.TOP_CTRL_0 &=
                    ~(USB32DEV_PHYSS_USB40PHY_TOP_CTRL_0_REG_PWR_GOOD_CORE_RX_Msk |
                     USB32DEV_PHYSS_USB40PHY_TOP_CTRL_0_REG_PWR_GOOD_CORE_PLL_Msk |
                     USB32DEV_PHYSS_USB40PHY_TOP_CTRL_0_REG_VBUS_Msk |
                     USB32DEV_PHYSS_USB40PHY_TOP_CTRL_0_REG_PHYSS_EN_Msk |
                     USB32DEV_PHYSS_USB40PHY_TOP_CTRL_0_PCLK_EN_Msk);

    /* Disable the SuperSpeed Device function */
    USB32DEV_MAIN->CTRL &= ~USB32DEV_MAIN_CTRL_SSDEV_ENABLE_Msk;
}

/*****************************************************************************
 * Function Name: Cy_ConfigFpgaRegister
 ******************************************************************************
 *
 * Configure the data stream on the FPGA using I2C registers.
 *
 * \return
 * Status code from the PDL functions.
 *
 *****************************************************************************/
cy_en_scb_i2c_status_t
Cy_ConfigFpgaRegister (void)
{
    /* FPGA will send data corresponding to a 4K video frame. */
    uint16_t width = 3840;
    uint16_t height = 2160;
    cy_en_scb_i2c_status_t status = CY_SCB_I2C_SUCCESS;

    /* Disable camera before configuring FPGA register */
    status = Cy_I2C_Write(FPGASLAVE_ADDR, DEVICE0_OFFSET + FPGA_DEVICE_STREAM_ENABLE_ADDRESS, CAMERA_APP_DISABLE,
            FPGA_I2C_ADDRESS_WIDTH, FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    if (status != CY_SCB_I2C_SUCCESS)
        return status;

    /* Select UVC data format. */
    status = Cy_I2C_Write(FPGASLAVE_ADDR, FPGA_UVC_U3V_SELECTION_ADDRESS, FPGA_UVC_ENABLE,
            FPGA_I2C_ADDRESS_WIDTH, FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    if (status != CY_SCB_I2C_SUCCESS)
        return status;

    /* Since this is not a video application, there is no header to be added. */
    status = Cy_I2C_Write(FPGASLAVE_ADDR, FPGA_UVC_HEADER_CTRL_ADDRESS, FPGA_UVC_HEADER_DISABLE,
            FPGA_I2C_ADDRESS_WIDTH, FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    if (status != CY_SCB_I2C_SUCCESS)
        return status;

    Cy_SysLib_DelayUs(1000);

    /* Only one active device needed for this application. */
    status = Cy_I2C_Write(FPGASLAVE_ADDR, FPGA_ACTIVE_DEVICE_MASK_ADDRESS, 0x01,
            FPGA_I2C_ADDRESS_WIDTH, FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    if (status != CY_SCB_I2C_SUCCESS)
        return status;

    Cy_SysLib_DelayUs(1000);

    /* Disable format conversion */
    status = Cy_I2C_Write(FPGASLAVE_ADDR, DEVICE0_OFFSET + FPGA_DEVICE_STREAM_MODE_ADDRESS, NO_CONVERSION,
            FPGA_I2C_ADDRESS_WIDTH, FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    if (status != CY_SCB_I2C_SUCCESS)
        return status;

    /* Configure FPGA to generate colorbar internally. */
    status = Cy_I2C_Write(FPGASLAVE_ADDR, DEVICE0_OFFSET + DEVICE_SOURCE_TYPE_ADDRESS, INTERNAL_COLORBAR,
            FPGA_I2C_ADDRESS_WIDTH, FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    if (status != CY_SCB_I2C_SUCCESS)
        return status;

    /* Switch between devices only at start of new frame. */
    status = Cy_I2C_Write(FPGASLAVE_ADDR, DEVICE0_OFFSET + DEVICE_FLAG_INFO_ADDRESS, NEW_FRAME_START,
            FPGA_I2C_ADDRESS_WIDTH, FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    if (status != CY_SCB_I2C_SUCCESS)
        return status;

#if PORT0_THREAD_INTLV
    /* Configure FPGA to send data on interleaved basis switching between Thread-0 and Thread-1. */

    /* Number of threads is 2. */
    status = Cy_I2C_Write(FPGASLAVE_ADDR, DEVICE0_OFFSET + DEVICE_ACTIVE_THREAD_INFO_ADDRESS, 2,
            FPGA_I2C_ADDRESS_WIDTH, FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

    /* Select the second thread for use. */
    status = Cy_I2C_Write(FPGASLAVE_ADDR, DEVICE0_OFFSET + DEVICE_THREAD2_INFO_ADDRESS, CY_LVDS_GPIF_THREAD_1,
            FPGA_I2C_ADDRESS_WIDTH, FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

    /* Set socket corresponding to Thread-1. */
    status = Cy_I2C_Write(FPGASLAVE_ADDR, DEVICE0_OFFSET + DEVICE_THREAD2_SOCKET_INFO_ADDRESS, 0,
            FPGA_I2C_ADDRESS_WIDTH, FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

    /* Select the first thread for use. */
    status = Cy_I2C_Write(FPGASLAVE_ADDR, DEVICE0_OFFSET + DEVICE_THREAD1_INFO_ADDRESS, CY_LVDS_GPIF_THREAD_0,
            FPGA_I2C_ADDRESS_WIDTH, FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

    /* Set socket corresponding to Thread-0 */
    status = Cy_I2C_Write(FPGASLAVE_ADDR, DEVICE0_OFFSET + DEVICE_THREAD1_SOCKET_INFO_ADDRESS, 0,
            FPGA_I2C_ADDRESS_WIDTH, FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

#else

    /* Number of threads is 1. */
    status = Cy_I2C_Write(FPGASLAVE_ADDR, DEVICE0_OFFSET + DEVICE_ACTIVE_THREAD_INFO_ADDRESS, 1,
            FPGA_I2C_ADDRESS_WIDTH, FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

    /* No second thread required. */
    status = Cy_I2C_Write(FPGASLAVE_ADDR, DEVICE0_OFFSET + DEVICE_THREAD2_INFO_ADDRESS, 0,
            FPGA_I2C_ADDRESS_WIDTH, FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

    /* Clear socket corresponding to second thread. */
    status = Cy_I2C_Write(FPGASLAVE_ADDR, DEVICE0_OFFSET + DEVICE_THREAD2_SOCKET_INFO_ADDRESS, 0,
            FPGA_I2C_ADDRESS_WIDTH, FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

    /* Select the first thread to use. */
    status = Cy_I2C_Write(FPGASLAVE_ADDR, DEVICE0_OFFSET + DEVICE_THREAD1_INFO_ADDRESS, CY_LVDS_GPIF_THREAD_0,
            FPGA_I2C_ADDRESS_WIDTH, FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

    /* Select socket to be used by Thread-0. */
    status = Cy_I2C_Write(FPGASLAVE_ADDR, DEVICE0_OFFSET + DEVICE_THREAD1_SOCKET_INFO_ADDRESS, 0,
            FPGA_I2C_ADDRESS_WIDTH, FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

#endif  /*  PORT0_THREAD_INTLV */

    /* Set the default video resolution. */
    status = Cy_I2C_Write(FPGASLAVE_ADDR, DEVICE0_OFFSET + DEVICE_IMAGE_WIDTH_MSB_ADDRESS, CY_USB_GET_MSB(width),
            FPGA_I2C_ADDRESS_WIDTH, FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

    status = Cy_I2C_Write(FPGASLAVE_ADDR, DEVICE0_OFFSET + DEVICE_IMAGE_WIDTH_LSB_ADDRESS, CY_USB_GET_LSB(width),
            FPGA_I2C_ADDRESS_WIDTH, FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

    status = Cy_I2C_Write(FPGASLAVE_ADDR, DEVICE0_OFFSET + DEVICE_IMAGE_HEIGHT_MSB_ADDRESS, CY_USB_GET_MSB(height),
            FPGA_I2C_ADDRESS_WIDTH, FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

    status = Cy_I2C_Write(FPGASLAVE_ADDR, DEVICE0_OFFSET + DEVICE_IMAGE_HEIGHT_LSB_ADDRESS, CY_USB_GET_LSB(height),
            FPGA_I2C_ADDRESS_WIDTH, FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

    return status;
}

#if ((!FPGA_CONFIG_EN) && (!LVDS_LB_EN))
/*****************************************************************************
* Function Name: Cy_IsFPGAConfigured(void)
******************************************************************************
* Summary:
* Function to check for CDONE status
*
* Parameters:
* None
*
* Return:
*  0 if FPGA not configured, 1u if FPGA configured.
*****************************************************************************/
static bool Cy_IsFPGAConfigured(void)
{
    bool cdoneVal = false;
    uint32_t maxWait = CDONE_WAIT_TIMEOUT;

    while (cdoneVal == false)
    {
        /*Check if CDONE is HIGH or FPGA is configured */
        cdoneVal = Cy_GPIO_Read(TI180_CDONE_PORT, TI180_CDONE_PIN);
        Cy_SysLib_Delay(1);
        maxWait--;
        if (!maxWait)
        {
            break;
        }
    }

    if((maxWait == 0) && (cdoneVal == false))
    {
        LOG_ERROR("FPGA not configured \r\n");
        return false;
    }
    else
    {
        DBG_APP_INFO("FPGA is configured \r\n");
    }

    return true;
}
#endif /* ((!FPGA_CONFIG_EN) && (!LVDS_LB_EN)) */

/*****************************************************************************
 * Function Name: Cy_Usb_FpgaPhyLinkTrain_Info()
 ******************************************************************************
 *
 * Configure FPGA for PHY and LINK training.
 *
 * \param pAppCtxt
 * Application context pointer.
 *
 * \return
 * Success/failure status from the PDL functions.
 *
 *****************************************************************************/
cy_en_scb_i2c_status_t Cy_Usb_FpgaPhyLinkTrain_Info (cy_stc_usb_app_ctxt_t *pAppCtxt)
{
    cy_en_scb_i2c_status_t  status = CY_SCB_I2C_SUCCESS;

    /* Keep data streaming disabled. */
    status = Cy_I2C_Write(FPGASLAVE_ADDR, DEVICE0_OFFSET + FPGA_DEVICE_STREAM_ENABLE_ADDRESS, CAMERA_APP_DISABLE,
            FPGA_I2C_ADDRESS_WIDTH, FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    if (status != CY_SCB_I2C_SUCCESS)
        return status;
    Cy_SysLib_DelayUs(10);

    DBG_APP_INFO("Require PHY and LINK training for LVDS interface\r\n");
    SET_BIT(pAppCtxt->fpgaTrainingCtrl, (FPGA_LINK_CONTROL | FPGA_PHY_CONTROL));
    status = Cy_I2C_Write(FPGASLAVE_ADDR, FPGA_PHY_LINK_CONTROL_ADDRESS,
            pAppCtxt->fpgaTrainingCtrl, FPGA_I2C_ADDRESS_WIDTH, FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    Cy_SysLib_DelayUs(10);

    status = Cy_I2C_Write(FPGASLAVE_ADDR, FPGA_LVDS_PHY_TRAINING_ADDRESS, PHY_TRAINING_PATTERN_BYTE,
                           FPGA_I2C_ADDRESS_WIDTH, FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    Cy_SysLib_DelayUs(10);

    status = Cy_I2C_Write(FPGASLAVE_ADDR, FPGA_LVDS_LINK_TRAINING_BLK_P0_ADDRESS,
            CY_USB_DWORD_GET_BYTE0(LINK_TRAINING_PATTERN_BYTE),
            FPGA_I2C_ADDRESS_WIDTH, FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    Cy_SysLib_DelayUs (10);

    status = Cy_I2C_Write(FPGASLAVE_ADDR, FPGA_LVDS_LINK_TRAINING_BLK_P1_ADDRESS,
            CY_USB_DWORD_GET_BYTE1(LINK_TRAINING_PATTERN_BYTE),
            FPGA_I2C_ADDRESS_WIDTH, FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    Cy_SysLib_DelayUs (10);

    status = Cy_I2C_Write(FPGASLAVE_ADDR, FPGA_LVDS_LINK_TRAINING_BLK_P2_ADDRESS,
            CY_USB_DWORD_GET_BYTE2(LINK_TRAINING_PATTERN_BYTE),
            FPGA_I2C_ADDRESS_WIDTH, FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    Cy_SysLib_DelayUs (10);

    status = Cy_I2C_Write(FPGASLAVE_ADDR, FPGA_LVDS_LINK_TRAINING_BLK_P3_ADDRESS,
            CY_USB_DWORD_GET_BYTE3(LINK_TRAINING_PATTERN_BYTE),
            FPGA_I2C_ADDRESS_WIDTH, FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    Cy_SysLib_DelayUs (10);

    return status;
}

#if CUSTOM_TRAIN_ENABLE

/****************************************************************************
 * Function Name: Cy_LVDS_InitAndTrain
 ****************************************************************************
 *
 * Function to initialize the LVDS block, complete custom training and then
 * de-initialize the block.
 *
 ***************************************************************************/
static void
Cy_LVDS_InitAndTrain (
        void)
{
    uint32_t intState;

    /* First initialize the LVDS block in char mode for custom training. */
    Cy_LVDS_CustomTraining_Select(true, &HBW_MgrCtxt);
    Cy_LVDS_Init(LVDSSS_LVDS, 0, &cy_lvds0_config, &lvdsContext);

    /* Thread errors are expected during custom training. Don't register for those errors here. */
    Cy_LVDS_SetInterruptMask(LVDSSS_LVDS,
            LVDSSS_LVDS_LVDS_INTR_WD0_LNK0_TRAINING_DONE_Msk |
            LVDSSS_LVDS_LVDS_INTR_WD0_LNK1_TRAINING_DONE_Msk|
            LVDSSS_LVDS_LVDS_INTR_WD0_LNK0_TRAINING_BLK_DETECTED_Msk |
            LVDSSS_LVDS_LVDS_INTR_WD0_LNK1_TRAINING_BLK_DETECTED_Msk |
            LVDSSS_LVDS_LVDS_INTR_WD0_LNK0_TRAINING_BLK_DET_FAILD_Msk |
            LVDSSS_LVDS_LVDS_INTR_WD0_LNK1_TRAINING_BLK_DET_FAILD_Msk|
            LVDSSS_LVDS_LVDS_INTR_WD0_PHY_LINK0_INTERRUPT_Msk |
            LVDSSS_LVDS_LVDS_INTR_WD0_PHY_LINK1_INTERRUPT_Msk);

    /* Register callbacks from the LVDS driver. */
    Cy_LVDS_RegisterCallback(LVDSSS_LVDS, &glAppLvdsCallbacks, &lvdsContext, &appCtxt);

    /* Enable the LVDS block. */
    Cy_LVDS_Enable(LVDSSS_LVDS);

    /*
     * In the current FPGA implementation, we are using assertion of the BUFFER_RDY as the indication
     * for FPGA to move from PHY training to LINK training and data transfer. Hence, we need to ensure
     * that this signal will not get asserted until PHY training is completed and also that it gets
     * asserted once PHY training is done and we are ready to move on.
     *
     * The signal is overridden as a GPIO and kept low until PHY training gets completed.
     */
    Cy_LVDS_PhyGpioModeEnable(LVDSSS_LVDS, 0, CY_LVDS_PHY_GPIO_CTL5,
        CY_LVDS_PHY_GPIO_OUTPUT, CY_LVDS_PHY_GPIO_NO_INTERRUPT);

    /* Signal FPGA to start PHY training and then call Cy_LVDS_PhyTrainingStart() */
    Cy_LVDS_PhyGpioSet(LVDSSS_LVDS, 0, LINK_READY_CTL_PIN);
    Cy_LVDS_PhyTrainingStart(LVDSSS_LVDS, 0, cy_lvds0_config.phyConfig);

    /* Run the training task until it completes. */
    do {
        vTaskDelay(1);
    } while (Cy_LVDS_CustomTraining_Task(&lvdsContext));

    DBG_APP_INFO("Custom PhyTraining is complete\r\n");

    /* De-initialize the LVDS block completely. */
    Cy_LVDS_Disable(LVDSSS_LVDS);
    Cy_LVDS_Deinit(LVDSSS_LVDS, 0, &lvdsContext);

    /* Disable and re-enable the LVDS DMA adapters. */
    intState = Cy_SysLib_EnterCriticalSection();
    Cy_HBDma_DeInit(&HBW_DrvCtxt);
    Cy_HBDma_Init(LVDSSS_LVDS, USB32DEV, &HBW_DrvCtxt, 0, 0);
    Cy_HBDma_Mgr_SetLvdsAdapterIngressMode(&HBW_MgrCtxt, true, true);
    Cy_SysLib_ExitCriticalSection(intState);

    /* Disable custom training mode before re-initializing the LVDS block. */
    Cy_LVDS_CustomTraining_Select(false, &HBW_MgrCtxt);
}

#endif /* CUSTOM_TRAIN_ENABLE */

/*****************************************************************************
 * Function Name: InitLvdsInterface
 *****************************************************************************
 *
 * Initialize the LVDS interface.
 * If link loopback is used to source data, the DMA channel for the loopback
 * transmitter will also be initialized.
 *
 * \param pAppCtxt
 * Pointer to the application context structure.
 *
 ****************************************************************************/
void InitLvdsInterface (cy_stc_usb_app_ctxt_t *pAppCtxt)
{
#if LVDS_LB_EN
    cy_en_hbdma_mgr_status_t mgrstat;
    cy_stc_hbdma_chn_config_t chn_conf =
    {
        .size = HBDMA_BUFFER_SIZE + 0xE0,
        .count = 2,
        .chType = CY_HBDMA_TYPE_MEM_TO_IP,
        .bufferMode = true,
        .prodSckCount = 1,
        .prodSck = {CY_HBDMA_VIRT_SOCKET_WR, CY_HBDMA_VIRT_SOCKET_WR},
        .consSckCount = 1,
        .consSck = {CY_HBDMA_LVDS_SOCKET_17, CY_HBDMA_LVDS_SOCKET_17},
        .eventEnable = 0,
        .intrEnable = 0x3FF,
        .cb = HbDma_Cb_Loopback,
        .userCtx = (void *)&appCtxt
    };

    Cy_USBD_AddEvtToLog(&usbdCtxt, CY_USB_EVT_INIT_LVDS_LB_EN);
    Cy_LVDS_Init(LVDSSS_LVDS, 1, &cy_lvds1_config, &lvdsContext);

    /* In loopback case, the threads need to be enabled manually. */
    Cy_LVDS_GpifThreadConfig(LVDSSS_LVDS, 3, 1, 0, 0, 0);

    DBG_APP_INFO("Port1 loopback config done\r\n");

    mgrstat = Cy_HBDma_Channel_Create(&HBW_MgrCtxt, &lvdsLbPgmChannel, &chn_conf);
    if (mgrstat != CY_HBDMA_MGR_SUCCESS) {
        DBG_APP_ERR("Loopback channel create failed 0x%x\r\n", mgrstat);
    }
#endif /* LVDS_LB_EN */

    pAppCtxt->isPhyTrainingDone = false;

#if CUSTOM_TRAIN_ENABLE
    Cy_LVDS_InitAndTrain();
#endif /* CUSTOM_TRAIN_ENABLE */

    /* LVDS block initialization. */
    Cy_USBD_AddEvtToLog(&usbdCtxt, CY_USB_EVT_PPORT0_EN);
    Cy_LVDS_Init(LVDSSS_LVDS, 0, &cy_lvds0_config, &lvdsContext);
    DBG_APP_INFO("LVDS0_Init done\r\n");

#if !LVDS_LB_EN
    /* Enable all possible LVDS block interrupts. */
    Cy_LVDS_SetInterruptMask(LVDSSS_LVDS,
            LVDSSS_LVDS_LVDS_INTR_WD0_LNK0_TRAINING_DONE_Msk |
            LVDSSS_LVDS_LVDS_INTR_WD0_LNK1_TRAINING_DONE_Msk|
            LVDSSS_LVDS_LVDS_INTR_WD0_LNK0_TRAINING_BLK_DETECTED_Msk |
            LVDSSS_LVDS_LVDS_INTR_WD0_LNK1_TRAINING_BLK_DETECTED_Msk |
            LVDSSS_LVDS_LVDS_INTR_WD0_LNK0_TRAINING_BLK_DET_FAILD_Msk |
            LVDSSS_LVDS_LVDS_INTR_WD0_LNK1_TRAINING_BLK_DET_FAILD_Msk|
            LVDSSS_LVDS_LVDS_INTR_WD0_LNK0_L1_ENTRY_Msk |
            LVDSSS_LVDS_LVDS_INTR_WD0_LNK1_L1_ENTRY_Msk |
            LVDSSS_LVDS_LVDS_INTR_WD0_LNK0_L1_EXIT_Msk |
            LVDSSS_LVDS_LVDS_INTR_WD0_LNK1_L1_EXIT_Msk |
            LVDSSS_LVDS_LVDS_INTR_WD0_LNK0_L3_ENTRY_Msk |
            LVDSSS_LVDS_LVDS_INTR_WD0_LNK1_L3_ENTRY_Msk |
            LVDSSS_LVDS_LVDS_INTR_WD0_PHY_LINK0_INTERRUPT_Msk |
            LVDSSS_LVDS_LVDS_INTR_WD0_PHY_LINK1_INTERRUPT_Msk |
            LVDSSS_LVDS_LVDS_INTR_WD0_THREAD0_ERR_Msk |
            LVDSSS_LVDS_LVDS_INTR_WD0_THREAD1_ERR_Msk |
            LVDSSS_LVDS_LVDS_INTR_WD0_THREAD2_ERR_Msk |
            LVDSSS_LVDS_LVDS_INTR_WD0_THREAD3_ERR_Msk |
            LVDSSS_LVDS_LVDS_INTR_MASK_WD0_GPIF0_INTERRUPT_Msk);
#endif /* !LVDS_LB_EN */

    /* Register callbacks from the LVDS driver. */
    Cy_LVDS_RegisterCallback(LVDSSS_LVDS, &glAppLvdsCallbacks, &lvdsContext, &appCtxt);

    Cy_LVDS_Enable(LVDSSS_LVDS);
    DBG_APP_INFO("LVDSEn\r\n");

#if CUSTOM_TRAIN_ENABLE
    /* Apply the previously identified optimal slave DLL phases. */
    Cy_LVDS_CustomTraining_ApplyResults(&lvdsContext, 0);
#else
    /* Signal FPGA to start PHY training. */
    Cy_LVDS_PhyGpioSet(LVDSSS_LVDS, 0, LINK_READY_CTL_PIN);
#endif /* CUSTOM_TRAIN_ENABLE */

    /*
     * Run through PHY training. When custom training is enabled, this is still needed
     * so that the receiver can identify the byte boundaries correctly.
     */
    Cy_LVDS_PhyTrainingStart(LVDSSS_LVDS, 0, cy_lvds0_config.phyConfig);
    DBG_APP_INFO("PhyTraining done\r\n");

#if CUSTOM_TRAIN_ENABLE
    /* Remove GPIO override on BUFFER_RDY pin. */
    Cy_LVDS_PhyGpioModeDisable(LVDSSS_LVDS, 0, CY_LVDS_PHY_GPIO_CTL5);
#endif /* CUSTOM_TRAIN_ENABLE */

    /* Enable Thread0 by default. */
    Cy_LVDS_GpifThreadConfig(LVDSSS_LVDS, 0, 0, 0, 1, 4);

    Cy_USBD_AddEvtToLog(&usbdCtxt, CY_USB_EVT_LVDS_EN);
    Cy_LVDS_GpifSMStart(LVDSSS_LVDS, 0, STATE_START_P0, ALPHA_START_P0);

#if LVDS_LB_EN
    Cy_LVDS_GpifSMStart(LVDSSS_LVDS, 1, STATE_START_P1, ALPHA_START_P1);
#endif /* LVDS_LB_EN */

    DBG_APP_INFO("LVDS init complete\r\n");
}

/*****************************************************************************
 * Function Name: UsbSSConnectionEnable
 *****************************************************************************
 *
 * Enable the USB connection with the desired speed.
 *
 * \param pAppCtxt
 * Pointer to application context structure.
 *
 ****************************************************************************/
void UsbSSConnectionEnable (cy_stc_usb_app_ctxt_t *pAppCtxt)
{
    Cy_USBD_ConnectDevice(pAppCtxt->pUsbdCtxt, USB_CONN_TYPE);
    pAppCtxt->usbConnectDone = true;
}

/*****************************************************************************
* Function Name: DisableUsbBlock
******************************************************************************
* Summary:
*  Function to disable the USB 3.x block when terminating a connection due
*  to VBus removal.
*
*****************************************************************************/
static void DisableUsbBlock (void)
{
    /* Disable the USB block. */
    USB32DEV->USB32DEV_MAIN.CTRL &= ~USB32DEV_MAIN_CTRL_IP_ENABLED_Msk;

    /* Disable HBDMA adapter interrupts from the USB block. */
    NVIC_DisableIRQ(lvds2usb32ss_usb32_ingrs_dma_int_o_IRQn);
    NVIC_DisableIRQ(lvds2usb32ss_usb32_egrs_dma_int_o_IRQn);
}

/*****************************************************************************
* Function Name: EnableUsbBlock
******************************************************************************
* Summary:
*  Function to re-enable the USB32DEV IP block before enabling a new USB
*  connection.
*
*****************************************************************************/
static void EnableUsbBlock (void)
{
    /* Enable the interrupts from the USB DMA adapters. */
    NVIC_EnableIRQ(lvds2usb32ss_usb32_ingrs_dma_int_o_IRQn);
    NVIC_EnableIRQ(lvds2usb32ss_usb32_egrs_dma_int_o_IRQn);

    /* Enable the USB block as well. */
    USB32DEV->USB32DEV_MAIN.CTRL |= USB32DEV_MAIN_CTRL_IP_ENABLED_Msk;
}

/*****************************************************************************
 * Function Name: AppPrintUsbEventLog
 ******************************************************************************
 *
 *  Function to print out the USB event log buffer content.
 *
 * \param pAppCtxt
 *  Pointer to application context data structure.
 *
 *****************************************************************************/
void AppPrintUsbEventLog (cy_stc_usb_app_ctxt_t *pAppCtxt)
{
    uint16_t prevLogIdx = pAppCtxt->curEvtLogIndex;
    uint16_t temp;

    /* Print out any pending USB event log data. */
    pAppCtxt->curEvtLogIndex = Cy_USBD_GetEvtLogIndex(pAppCtxt->pUsbdCtxt);
    temp = pAppCtxt->curEvtLogIndex;

    while (temp != prevLogIdx) {
        DBG_APP_INFO("USBEVT: %x\r\n", pAppCtxt->pUsbEvtLogBuf[prevLogIdx]);
        prevLogIdx++;
        if (prevLogIdx == USB_EVT_LOG_SIZE) {
            prevLogIdx = 0u;
        }
    }
}

/*****************************************************************************
 * Function Name: UsbDeviceTaskHandler
 ******************************************************************************
 *
 * Entry function for the application task.
 *
 * \param pTaskParam
 * Application context pointer (opaque pointer).
 *
 *****************************************************************************/
void
UsbDeviceTaskHandler (void *pTaskParam)
{
    cy_stc_usb_app_ctxt_t *pAppCtxt = (cy_stc_usb_app_ctxt_t *)pTaskParam;
    bool intrEnabled = false;
    EventBits_t evStat;
    uint32_t loopCnt = 0, printCnt = 0;

    DBG_APP_INFO("LVDS to USB app task started\r\n");

#if (!LVDS_LB_EN)
#if FPGA_CONFIG_EN
    Cy_FPGAConfigPins(pAppCtxt,FPGA_CONFIG_MODE);
    Cy_QSPI_Start(pAppCtxt,&HBW_BufMgr);
    Cy_SPI_FlashInit(SPI_FLASH_0, true, false);

    if(Cy_FPGAConfigure(pAppCtxt,FPGA_CONFIG_MODE) == true)
    {
        DBG_APP_INFO("FPGA configuration complete \r\n");
        pAppCtxt->isFpgaConfigured = true;
    }
    else
    {
        LOG_ERROR("Failed to configure FPGA \r\n");
    }

#else
    if(true == Cy_IsFPGAConfigured())
    {
        pAppCtxt->isFpgaConfigured = true;
    }
#endif /* FPGA_CONFIG_EN */

    if ((pAppCtxt->isFpgaRegConfigured == false) && (pAppCtxt->isFpgaConfigured == true)) {
        /* Set the PHY and LINK training settings in the FPGA registers. */
        Cy_Usb_FpgaPhyLinkTrain_Info(pAppCtxt);

        if (0 == Cy_ConfigFpgaRegister()) {
            pAppCtxt->isFpgaRegConfigured = true;
            DBG_APP_INFO("Successfully configured Ti180 FPGA via I2C\n\r");
            vTaskDelay(50);
        } else {
            LOG_ERROR("Failed to configure FPGA via I2C \r\n");
            while (1) {
                vTaskDelay(100);
            }
        }
    }
#endif /* (!LVDS_LB_EN) */

    /* Initialize the LVDS interface. */
    InitLvdsInterface(pAppCtxt);

    /* Initialize application layer. */
    Cy_USB_AppInit(pAppCtxt, &usbdCtxt, DMAC, DW0, DW1, &HBW_MgrCtxt);
    pAppCtxt->usbConnectDone        = false;
    pAppCtxt->vbusPresent           = true;
    pAppCtxt->vbusChangeIntr        = false;

    /* Register callback functions. */
    Cy_USB_AppRegisterCallback(pAppCtxt);

    /* Register descriptors with the USB Stack. */
    CyApp_RegisterUsbDescriptors(pAppCtxt, USB_CONN_TYPE);

    /* Set DEVSTATE_CHG event so that USB connection will be attempted by the task. */
    Cy_USB_AppSignalTask(pAppCtxt, EV_DEVSTATE_CHG);

    /* Update the VBus presence status at start-up. */
    if (Cy_GPIO_Read(VBUS_DETECT_GPIO_PORT, VBUS_DETECT_GPIO_PIN) == VBUS_DETECT_STATE) {
        pAppCtxt->vbusPresent = true;
    } else {
        pAppCtxt->vbusPresent = false;
    }

    /* Make sure the EV_DEVSTATE_CHG task runs at least once. */
    Cy_USB_AppSignalTask(pAppCtxt, EV_DEVSTATE_CHG);

    while (1)
    {
        /* Wait for some work to be queued. We will wake up once in 100 ms to do book-keeping. */
        evStat = xEventGroupWaitBits(pAppCtxt->appEvGrpHandle, TASK_WAIT_EV_MASK, pdTRUE, pdFALSE, 100);

        loopCnt++;
        if (loopCnt > 100) {
            DBG_APP_INFO("TASKLOOP: %d\r\n", printCnt++);
            loopCnt = 0;

            /* Print out any pending USB event log data. */
            AppPrintUsbEventLog(pAppCtxt);
        }

        if ((evStat & EV_DEVSTATE_CHG) != 0)
        {
            if (pAppCtxt->vbusChangeIntr) {
                /*
                 * Debounce delay of 750 ms when VBus turns from ON to OFF and 50 ms when VBus turns from
                 * OFF to ON.
                 */
                if (pAppCtxt->vbusPresent) {
                    vTaskDelay(750);
                } else {
                    vTaskDelay(50);
                }

                /* Clear the VBus change flag, clear and re-enable the interrupt. */
                pAppCtxt->vbusChangeIntr = false;
                Cy_GPIO_ClearInterrupt(VBUS_DETECT_GPIO_PORT, VBUS_DETECT_GPIO_PIN);
                Cy_GPIO_SetInterruptMask(VBUS_DETECT_GPIO_PORT, VBUS_DETECT_GPIO_PIN, 1);

                if (Cy_GPIO_Read(VBUS_DETECT_GPIO_PORT, VBUS_DETECT_GPIO_PIN) == VBUS_DETECT_STATE) {
                    if (!pAppCtxt->vbusPresent) {
                        Cy_USBD_AddEvtToLog(pAppCtxt->pUsbdCtxt, CY_SSCAL_EVT_VBUS_PRESENT);
                        DBG_APP_INFO("VBus presence detected\r\n");
                        pAppCtxt->vbusPresent = true;
                    } else {
                        DBG_APP_INFO("Spurious GPIO INT - 1\r\n");
                    }
                } else {
                    if (pAppCtxt->vbusPresent) {
                        Cy_USBD_AddEvtToLog(pAppCtxt->pUsbdCtxt, CY_SSCAL_EVT_VBUS_ABSENT);
                        DBG_APP_INFO("VBus absence detected\r\n");
                        pAppCtxt->vbusPresent = false;
                    } else {
                        DBG_APP_INFO("Spurious GPIO INT - 0\r\n");
                    }
                }
            }

            if ((pAppCtxt->usbConnectDone == true) && (pAppCtxt->vbusPresent == false)) {
                Cy_USB_AppDisableEndpDma(pAppCtxt);
                Cy_USBD_DisconnectDevice(pAppCtxt->pUsbdCtxt);
                Cy_USBSS_DeInit(pAppCtxt->pUsbdCtxt->pSsCalCtxt);
                DBG_APP_INFO("Disconnect due to VBus loss\r\n");
                pAppCtxt->usbConnectDone = false;
                pAppCtxt->devState = CY_USB_DEVICE_STATE_DISABLE;
                pAppCtxt->prevDevState = CY_USB_DEVICE_STATE_DISABLE;

                DisableUsbBlock();
            }

            if ((pAppCtxt->vbusPresent == true) && (pAppCtxt->usbConnectDone == false)) {
                DBG_APP_INFO("Connect due to VBus presence\r\n");
                EnableUsbBlock();
                UsbSSConnectionEnable(pAppCtxt);
            }
        }

        /* Handle any vendor specific control requests. */
        if ((evStat & EV_VENDOR_REQUEST) != 0) {
            Cy_USB_AppVendorRqtHandler(pAppCtxt);
        }

        /* Handle Reset request on streaming endpoint. */
        if ((evStat & EV_STREAM_RESET) != 0) {
            Cy_USB_AppHandleStreamReset(pAppCtxt);
        }

        /*
         * If we are/were in the configured state, we can run the loopback task.
         * Otherwise, cancel all DMA handling.
         */
        if (
                (pAppCtxt->usbConnectDone == false) ||
                (
                 (pAppCtxt->devState != CY_USB_DEVICE_STATE_CONFIGURED) &&
                 (pAppCtxt->prevDevState != CY_USB_DEVICE_STATE_CONFIGURED)
                )
           ) {
            Cy_USB_AppInitDmaIntr(pAppCtxt, LVDS_STREAMING_EP, CY_USB_ENDP_DIR_IN, NULL);
            intrEnabled = false;
        } else {
            if (!intrEnabled) {
                /* Register DMA transfer completion interrupts. */
                EnableEpDmaISRs(pAppCtxt);
                intrEnabled = true;
            }

            /* If the link is in USB2-L1, try and get it back into L0 so that transfers are not delayed. */
            if (
                    (pAppCtxt->devSpeed <= CY_USBD_USB_DEV_HS) &&
                    ((MXS40USBHSDEV_USBHSDEV->DEV_PWR_CS & USBHSDEV_DEV_PWR_CS_L1_SLEEP) != 0)
               ) {
                Cy_USBD_GetUSBLinkActive(pAppCtxt->pUsbdCtxt);
            }
        }
    }
}

/*****************************************************************************
 * Function Name: DebugLogTaskHandler
 ******************************************************************************
 *
 * Task that pushes debug log messages to the target interface.
 *
 * \param pTaskParam
 * Application context pointer (opaque pointer).
 *
 *****************************************************************************/
void
DebugLogTaskHandler (void *pTaskParam)
{
    while (1)
    {
        Cy_Debug_PrintLog();
        vTaskDelay(5);
    }
}

/*******************************************************************************
 * Function name: Cy_Fx3g2_InitPeripheralClocks
 ****************************************************************************//**
 *
 * Function used to enable clocks to different peripherals on the FX10/FX20 device.
 *
 * \param adcClkEnable
 * Whether to enable clock to the ADC in the USBSS block.
 *
 * \param usbfsClkEnable
 * Whether to enable bus reset detect clock input to the USBFS block.
 *
 *******************************************************************************/
void Cy_Fx3g2_InitPeripheralClocks (
        bool adcClkEnable,
        bool usbfsClkEnable)
{
    uint32_t peri_freq = Cy_SysClk_ClkPeriGetFrequency();

    if (adcClkEnable) {
        /* Divide PERI clock to get 1 MHz clock using 16-bit divider #1. */
        Cy_SysClk_PeriphSetDivider(CY_SYSCLK_DIV_16_BIT, 1, ((peri_freq / 1000000UL) - 1));
        Cy_SysClk_PeriphEnableDivider(CY_SYSCLK_DIV_16_BIT, 1);
        Cy_SysLib_DelayUs(10U);
        Cy_SysClk_PeriphAssignDivider(PCLK_LVDS2USB32SS_CLOCK_SAR, CY_SYSCLK_DIV_16_BIT, 1);
    }

    if (usbfsClkEnable) {
        /* Divide PERI clock to get 100 KHz clock using 16-bit divider #2. */
        Cy_SysClk_PeriphSetDivider(CY_SYSCLK_DIV_16_BIT, 2, ((peri_freq / 100000UL) - 1));
        Cy_SysClk_PeriphEnableDivider(CY_SYSCLK_DIV_16_BIT, 2);
        Cy_SysLib_DelayUs(10U);
        Cy_SysClk_PeriphAssignDivider(PCLK_USB_CLOCK_DEV_BRS, CY_SYSCLK_DIV_16_BIT, 2);
    }
}

/*******************************************************************************
 * Function name: Cy_Fx3G2_OnResetInit(void)
 ****************************************************************************//**
 *
 * This function performs initialization that is required to enable scatter
 * loading of data into the High BandWidth RAM during device boot-up. The FX10/FX20
 * device comes up with the High BandWidth RAM disabled and hence any attempt
 * to read/write the RAM will cause the processor to hang. The RAM needs to
 * be enabled with default clock settings to allow scatter loading to work.
 * This function needs to be called from Cy_OnResetUser.
 *
 *******************************************************************************/
void
Cy_Fx3G2_OnResetInit (
        void)
{
    /* Enable clk_hf4 with IMO as input. */
    SRSS->CLK_ROOT_SELECT[4] = SRSS_CLK_ROOT_SELECT_ENABLE_Msk;

    /* Enable LVDS2USB32SS IP and select clk_hf[4] as clock input. */
    MAIN_REG->CTRL = (
            MAIN_REG_CTRL_IP_ENABLED_Msk |
            (1UL << MAIN_REG_CTRL_NUM_FAST_AHB_STALL_CYCLES_Pos) |
            (1UL << MAIN_REG_CTRL_NUM_SLOW_AHB_STALL_CYCLES_Pos) |
            (3UL << MAIN_REG_CTRL_DMA_SRC_SEL_Pos));
}

/*****************************************************************************
* Function Name: main(void)
******************************************************************************
* Summary:
*  Entry to the application.
*
* Parameters:
*  void

* Return:
*  Does not return.
*****************************************************************************/
int main(void)
{
    BaseType_t status;

    /* Initialize the PDL driver library and set the clock variables. */
    Cy_PDL_Init(&cy_deviceIpBlockCfgFX3G2);

    /* Initialize the device clocks to the desired values. */
    cybsp_init();
    Cy_Fx3g2_InitPeripheralClocks(true, true);

    /* Unlock and then disable the watchdog. */
    Cy_WDT_Unlock();
    Cy_WDT_Disable();

    /* Enable interrupts. */
#if CY_CPU_CORTEX_M4
    __set_BASEPRI(0);
#endif /* CY_CPU_CORTEX_M4 */
    __enable_irq ();

    memset((uint8_t *)&appCtxt, 0, sizeof(appCtxt));
    memset((uint8_t *)&ssCalCtxt, 0, sizeof(ssCalCtxt));
    memset((uint8_t *)&hsCalCtxt, 0, sizeof(hsCalCtxt));
    memset((uint8_t *)&usbdCtxt, 0, sizeof(usbdCtxt));

    /* Setup all USB interrupt handlers. */
    UsbDevInit();

    /* Store IP base address in CAL context. */
    ssCalCtxt.regBase  = USB32DEV;
    hsCalCtxt.pCalBase = MXS40USBHSDEV_USBHSDEV;
    hsCalCtxt.pPhyBase = MXS40USBHSDEV_USBHSPHY;

    /*
     * Make sure any previous USB connection state is cleared. Give some delay to allow the host to process
     * disconnection.
     */
    Cy_USBSS_DeInit(&ssCalCtxt);
    Cy_SysLib_Delay(500);

    /* Initialize the HbDma IP and DMA Manager */
    InitHbDma();

    /* Initialize the USBD layer */
    Cy_USB_USBD_Init(&appCtxt, &usbdCtxt, DMAC, &hsCalCtxt, &ssCalCtxt, &HBW_MgrCtxt);

    /* Specify that DMA clock should be set to 240 MHz once USB 3.x connection is active. */
    Cy_USBD_SetDmaClkFreq(&usbdCtxt, CY_HBDMA_CLK_240_MHZ);

    /* Configure the LINK_READY pin as general purpose output and drive it low at start. */
    Cy_LVDS_PhyGpioModeDisable(LVDSSS_LVDS, 0, LINK_READY_CTL_PIN);
    Cy_LVDS_PhyGpioModeEnable(LVDSSS_LVDS, 0, LINK_READY_CTL_PIN,
            CY_LVDS_PHY_GPIO_OUTPUT, CY_LVDS_PHY_GPIO_NO_INTERRUPT);

    /* Enable stall cycles between AHB transactions to the DMA buffer RAM. */
    MAIN_REG->CTRL = (MAIN_REG->CTRL & 0xF00FFFFFUL) | 0x09900000UL;

    /* Allocate a memory block and register it as USB CAL event log buffer. */
    appCtxt.pUsbEvtLogBuf = (uint32_t *)Cy_HBDma_BufMgr_Alloc(&HBW_BufMgr, USB_EVT_LOG_SIZE * sizeof(uint32_t));
    Cy_USBD_InitEventLog(&usbdCtxt, appCtxt.pUsbEvtLogBuf, USB_EVT_LOG_SIZE);

    status = xTaskCreate(DebugLogTaskHandler, "LoggerTask", 500, (void *)&appCtxt, 4, &(appCtxt.logTaskHandle));
    if (status != pdPASS) {
        DBG_APP_ERR("LogTaskCreate failed\r\n");
        while (1) {
        }
    }

    appCtxt.appEvGrpHandle = xEventGroupCreate();
    if (appCtxt.appEvGrpHandle != NULL)
    {
        status = xTaskCreate(UsbDeviceTaskHandler, "UsbAppTask", 1000, (void *)&appCtxt, 5, &(appCtxt.appTaskHandle));
        if (status != pdPASS)
        {
            DBG_APP_ERR("xTaskCreate failed\r\n");
        }
    }
    else
    {
        DBG_APP_ERR("xEventGroupCreate failed\r\n");
        status = pdFALSE;
    }

    if (status == pdPASS)
    {
        /* Start the RTOS kernel scheduler. */
        vTaskStartScheduler();
    }

    while (1)
    {
        Cy_SysLib_Delay(100);
    }

    return 0;
}

/*******************************************************************************
 * Function name: Cy_OnResetUser
 ****************************************************************************//**
 *
 * Function called during start-up to perform any initialization required before
 * the application is loaded.
 *
 *******************************************************************************/
void Cy_OnResetUser (void)
{
    Cy_Fx3G2_OnResetInit();
}

/* [] END OF FILE */
