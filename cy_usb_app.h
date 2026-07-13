/***************************************************************************//**
* \file cy_usb_app.h
* \version 1.0
*
* Defines application level macros, data types and function interfaces.
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

#ifndef _CY_USB_APP_H_
#define _CY_USB_APP_H_

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "event_groups.h"
#include "cy_debug.h"
#include "cy_hbdma_mgr.h"
#include "cy_usbhs_dw_wrapper.h"
#include "cy_usb_i2c.h"
#include "cy_fpga_ctrl_regs.h"

#if defined(__cplusplus)
extern "C" {
#endif

/* P4.0 is used for VBus detect functionality. */
#define VBUS_DETECT_GPIO_PORT           (P4_0_PORT)
#define VBUS_DETECT_GPIO_PIN            (P4_0_PIN)
#define VBUS_DETECT_GPIO_INTR           (ioss_interrupts_gpio_dpslp_4_IRQn)
#define VBUS_DETECT_STATE               (0u)

#define LINK_READY_CTL_PIN              (CY_LVDS_PHY_GPIO_CTL6)  /* ID of pin used to signal FPGA to start training. */
#define PHY_TRAINING_PATTERN_BYTE       (0x9C)          /* Value used for LVDS PHY training. */
#define LINK_TRAINING_PATTERN_BYTE      (0x125A4B78)    /* Value used for LVDS LINK training. */

#define FPS_DEFAULT                     (60)            /* FPGA generates data at rate equivalent to 8 Gbps. */

#define FX_DMA_RDY_IO                   (CY_LVDS_PHY_GPIO_CTL5) /* LVCMOS IO used as DMA-Ready signal to FPGA. */

/* GPIO port pins*/
#define TI180_INIT_RESET_GPIO           (P4_3_GPIO)
#define TI180_INIT_RESET_PORT           (P4_3_PORT)
#define TI180_INIT_RESET_PIN            (P4_3_PIN)

#define TI180_CDONE_PIN                 (P4_4_PIN)
#define TI180_CDONE_PORT                (P4_4_PORT)

#define CDONE_WAIT_TIMEOUT              (1000)

#define DELAY_MICRO(us)                 Cy_SysLib_DelayUs(us)
#define DELAY_MILLI(ms)                 Cy_SysLib_Delay(ms)

#define ASSERT(condition, value) checkStatus(__func__, __LINE__, condition, value, true);
#define ASSERT_NON_BLOCK(condition, value) checkStatus(__func__, __LINE__, condition, value, false);
#define ASSERT_AND_HANDLE(condition, value, failureHandler) checkStatusAndHandleFailure(__func__, __LINE__, condition, value, false, failureHandler);

#define SET_BIT(byte, mask)                     (byte) |= (mask)
#define CLR_BIT(byte, mask)                     (byte) &= ~(mask)
#define CHK_BIT(byte, mask)                     ((byte) & (mask))

/* Colour definitions to be used in debug logs. */
#define COLOR_RESET                             "\033[0m"
#define RED                                     "\033[0;31m"
#define CYAN                                    "\033[0;36m"
#define BLK                                     "\033[0;30m"
#define GRN                                     "\033[0;32m"
#define YEL                                     "\033[0;33m"
#define BLU                                     "\033[0;34m"
#define MAG                                     "\033[0;35m"
#define WHT                                     "\033[0;37m"

/* Code to print bold characters. */
#define BBLK                                    "\033[1;30m"
#define BWHT                                    "\033[1;37m"

/* Code to underline characters. */
#define UWHT                                    "\033[4;37m"

/* Code for high intensity (emphasised) text. */
#define HBLK                                    "\033[0;90m"
#define HRED                                    "\033[0;91m"
#define HGRN                                    "\033[0;92m"
#define HYEL                                    "\033[0;93m"
#define HBLU                                    "\033[0;94m"
#define HMAG                                    "\033[0;95m"
#define HCYN                                    "\033[0;96m"
#define HWHT                                    "\033[0;97m"

/* Code for bold emphasised text. */
#define BHBLK                                   "\033[1;90m"
#define BHWHT                                   "\033[1;97m"

#define LOG_COLOR(...)                          Cy_Debug_AddToLog(1,CYAN);\
                                                Cy_Debug_AddToLog(1,__VA_ARGS__); \
                                                Cy_Debug_AddToLog(1,COLOR_RESET);

#define LOG_ERROR(...)                          Cy_Debug_AddToLog(1,RED);\
                                                Cy_Debug_AddToLog(1,__VA_ARGS__); \
                                                Cy_Debug_AddToLog(1,COLOR_RESET);

#define LOG_CLR(CLR, ...)                       Cy_Debug_AddToLog(1,CLR);\
                                                Cy_Debug_AddToLog(1,__VA_ARGS__); \
                                                Cy_Debug_AddToLog(1,COLOR_RESET);


#define LOG_TRACE()                             LOG_COLOR("-->[%s]:%d\r\n", __func__, __LINE__);

#if LVDS_LB_EN

#define HBDMA_BUFFER_SIZE                          (0xFC00U)            /* DMA buffer size for link loopback. */

#else

#define HBDMA_BUFFER_SIZE                          (0xFC00U)            /* DMA buffer size for LVDS streaming. */

#endif /* LVDS_LB_EN */

#define EV_DEVSTATE_CHG         (0x00001U)      /* Event bit which indicates device state change. */
#define EV_STREAM_RESET         (0x00002U)      /* Event bit indicating that streaming should be reset. */
#define EV_VENDOR_REQUEST       (0x10000U)      /* Event bit which indicates that EP0 vendor request is pending. */
#define TASK_WAIT_EV_MASK       (EV_DEVSTATE_CHG | EV_VENDOR_REQUEST | EV_STREAM_RESET)

#define LVDS_STREAMING_EP       (0x01)          /* IN endpoint used to stream data from LVDS port. */

#define USB_EVT_LOG_SIZE        (512u)          /* Size of USB event log buffer in uint32_t entries. */
/*
 * USB application data structure which is bridge between USB system and device
 * functionality.
 */
struct cy_stc_usb_app_ctxt_
{
    cy_en_usb_device_state_t            devState;                       /* Current USB device state. */
    cy_en_usb_device_state_t            prevDevState;                   /* Previous USB device state. */
    cy_en_usb_speed_t                   devSpeed;                       /* Current USB connection speed. */
    uint8_t                             devAddr;                        /* Current USB device address. */
    uint8_t                             activeCfgNum;                   /* Selected USB configuration index. */
    cy_en_usb_enum_method_t             enumMethod;                     /* Selected control request handling method. */
    uint8_t                             prevAltSetting;                 /* Previous alternate setting. */

    cy_stc_app_endp_dma_set_t           endpInDma[CY_USB_MAX_ENDP_NUMBER];      /* DMA info for IN endpoints. */
    cy_stc_app_endp_dma_set_t           endpOutDma[CY_USB_MAX_ENDP_NUMBER];     /* DMA info for OUT endpoints. */

    DMAC_Type                           *pCpuDmacBase;                  /* Pointer to DMAC control register block. */
    DW_Type                             *pCpuDw0Base;                   /* Pointer to DW0 control register block. */
    DW_Type                             *pCpuDw1Base;                   /* Pointer to DW1 control register block. */

    cy_stc_usb_usbd_ctxt_t              *pUsbdCtxt;                     /* USBD stack context pointer. */
    cy_stc_hbdma_mgr_context_t          *pHbDmaMgr;                     /* DMA manager context pointer. */
    uint32_t                            *pUsbEvtLogBuf;                 /* Buffer used to capture USB event logs. */
    uint16_t                             curEvtLogIndex;                /* Location of events which have been printed. */

    uint8_t                             *qspiWriteBuffer;               /* Data buffer used to write to QSPI memory. */
    uint8_t                             *qspiReadBuffer;                /* Data buffer used to read from QSPI memory. */
    uint8_t                             fpgaVersion;                    /* FPGA version information. */

    bool                                usbConnectDone;                 /* Whether USB connection is complete. */
    bool                                vbusChangeIntr;                 /* Whether VBus change has been detected. */
    bool                                vbusPresent;                    /* Whether VBus supply is active. */

    TaskHandle_t                        appTaskHandle;                  /* Handle to main application task. */
    TaskHandle_t                        logTaskHandle;                  /* Handle to logging task. */
    EventGroupHandle_t                  appEvGrpHandle;                 /* Handle to event group used for signalling. */

    uint16_t                            dmaBufferSize;                  /* Current DMA buffer size. */
    uint8_t                             streamingRate;                  /* Streaming rate in video frames per second. */
    bool                                isFpgaConfigured;               /* Whether FPGA config is done. */
    bool                                isFpgaRegConfigured;            /* Whether FPGA register config is done. */
    bool                                isPhyTrainingDone;              /* Whether PHY training is complete. */
    uint8_t                             isLinkTrainingDone;             /* Whether link training is complete. */
    uint8_t                             fpgaTrainingCtrl;               /* Value (to be) updated in PHY_LINK_CONTROL. */

    bool                                isLvdsWltoUsbHs;                /* Whether we are streaming from LVDS WideLink to USBHS. */
    bool                                fwDmaReadyStatus;               /* Firmware based DMA ready status. */
};

typedef struct cy_stc_usb_app_ctxt_ cy_stc_usb_app_ctxt_t;

/* Structure used to configure loopback pattern generator. */
typedef struct
{
    uint8_t  *pBuffer;                                  /* RAM buffer pointer. */
    uint8_t   start;                                    /* Start bit. */
    uint8_t   end;                                      /* End bit. */
    uint8_t   dataMode;                                 /* Data mode. */
    uint8_t   dataSrc;                                  /* Source of data. */
    uint8_t   ctrlByte;                                 /* Control byte value. */
    uint16_t  repeatCount;                              /* Count for which current pattern is to be repeated. */
    uint32_t  dataL;                                    /* Lower 32 bits of data. */
    uint32_t  dataH;                                    /* Upper 32 bits of data. */
    uint32_t  ctrlBusVal;                               /* Control bus state. */
    uint32_t  lbPgmCount;                               /* Loopback program count. */
} cy_stc_lvds_loopback_config_t;

/* Data updated by the loopback pattern generator (16 bytes). */
typedef struct
{
    uint32_t dataWord0;                                 /* Bytes 3:0 */
    uint32_t dataWord1;                                 /* Bytes 7:4 */
    uint32_t dataWord2;                                 /* Bytes 11:8 */
    uint32_t dataWord3;                                 /* Bytes 15:12 */
} cy_stc_lvds_loopback_mem_t;

/* List of vendor specific control requests implemented by this application. */
typedef enum {
    REG_MEMORY_READ_CODE   = 0xA0,      /* Request used to read registers and/or memory. */
    REG_MEMORY_WRITE_CODE  = 0xA1,      /* Request used to write registers and/or memory. */
    BOOT_MODE_RQT_CODE     = 0xB7,      /* Request to return to bootloader mode. */
    DATA_XFER_TEST_CODE    = 0xB8,      /* Request to test vendor command handling. */
    DEVICE_RESET_CODE      = 0xE0,      /* Request to reset the device. */
    MS_VENDOR_CODE         = 0xF0,      /* Request used to fetch MS-OS descriptors. */
    GET_DEVSPEED_CMD       = 0xF6,      /* Command to get device speed information from the device. */
} cy_usb_app_vendor_opcode_t;

/******************************************************************************
 ************************* Global variable declarations ***********************
 *****************************************************************************/
extern uint8_t glOsString[];
extern uint8_t glOsCompatibilityId[];
extern uint8_t glOsFeature[];
extern cy_stc_hbdma_channel_t lvdsLbPgmChannel;
extern bool glIsFPGAConfigured;

/******************************************************************************
 **************************** Function declarations ***************************
 *****************************************************************************/

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
void checkStatus(const char *function, uint32_t line, uint8_t condition, uint32_t value, uint8_t isBlocking);

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
void checkStatusAndHandleFailure(const char *function, uint32_t line, uint8_t condition,
        uint32_t value, uint8_t isBlocking, void (*failureHandler)(void));

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
void Cy_USB_AppInit(cy_stc_usb_app_ctxt_t *pAppCtxt,
        cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
        DMAC_Type *pCpuDmacBase,
        DW_Type *pCpuDw0Base,
        DW_Type *pCpuDw1Base,
        cy_stc_hbdma_mgr_context_t *pHbDmaMgr);

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
void Cy_USB_AppRegisterCallback(cy_stc_usb_app_ctxt_t *pAppCtxt);

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
void Cy_USB_AppInitDmaIntr(cy_stc_usb_app_ctxt_t *pAppCtxt, uint32_t endpNumber,
        cy_en_usb_endp_dir_t endpDirection, cy_israddress userIsr);

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
void Cy_USB_AppTerminateDma(cy_stc_usb_app_ctxt_t *pAppCtxt, uint8_t endpNumber, cy_en_usb_endp_dir_t endpDirection);

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
bool Cy_USB_AppSignalTask(cy_stc_usb_app_ctxt_t *pAppCtxt, const EventBits_t evMask);

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
void InitLvdsInterface(cy_stc_usb_app_ctxt_t *pAppCtxt);

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
void Cy_USB_AppDisableEndpDma(cy_stc_usb_app_ctxt_t *pAppCtxt);

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
void AppPrintUsbEventLog(cy_stc_usb_app_ctxt_t *pAppCtxt);

/*******************************************************************************
 * Function name: CyApp_RegisterUsbDescriptors
 ****************************************************************************//**
 *
 * Register the USB descriptors with the USBD stack.
 *
 * \param pAppCtxt
 * Application layer context pointer.
 *
 * \param usbSpeed
 * Active USB connection speed. Used to switch descriptors where needed.
 *
 ********************************************************************************/
void CyApp_RegisterUsbDescriptors(cy_stc_usb_app_ctxt_t *pAppCtxt, cy_en_usb_speed_t usbSpeed);

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
void Cy_USB_AppVendorRqtHandler(cy_stc_usb_app_ctxt_t *pAppCtxt);

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
void HbDma_Cb_Loopback(struct cy_stc_hbdma_channel *handle,
                       cy_en_hbdma_cb_type_t type,
                       cy_stc_hbdma_buff_status_t *pbufStat,
                       void *userCtx);

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
void Cy_USB_AppHandleStreamReset(cy_stc_usb_app_ctxt_t *pAppCtxt);

#if defined(__cplusplus)
}
#endif

#endif /* _CY_USB_APP_H_ */

/*[]*/

