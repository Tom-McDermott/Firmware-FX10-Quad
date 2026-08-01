/*
 * Copyright (c) 2026 [COPYRIGHT HOLDER TBD]
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
 */
/***************************************************************************//**
* \file quad_vendor.c
* \version 0.0.1
*
* EP0 vendor command handling for the Quad-PSWS-FPGA receiver firmware.
*
* This first increment implements the "prove host <-> device comms" commands:
*   QUAD_CMD_PING        (0xAC) - 4-byte heartbeat
*   QUAD_CMD_GET_VERSION (0xFF) - firmware version string
*   QUAD_CMD_GET_STATUS  (0xFE) - structured status block
* The remaining commands (Si5345 / FPGA / ADC / AGC / streaming) are added in
* subsequent increments; unrecognized codes fall through to the caller's stall.
*
* See ../../ep0-command-set-v0.0.1.md for the full command specification.
*
*******************************************************************************/

#include "FreeRTOS.h"
#include "cy_pdl.h"
#include "cy_device.h"
#include "cy_usb_common.h"
#include "cy_usb_usbd.h"
#include "cy_usb_app.h"
#include "quad_vendor.h"
#include "quad_fpga.h"
#include "quad_si5345.h"
#include "quad_adc.h"
#include "quad_stream.h"
#include "cy_usb_i2c.h"
#include <string.h>

/*
 * Global status block. Identity fields are set at init; the rest are updated by
 * the various subsystems as they come online (FPGA loader, link training, ...).
 */
static quad_status_t glQuadStatus = {
    .magic      = QUAD_STATUS_MAGIC,
    .fw_version = QUAD_FW_VERSION_BCD,
    .state      = QUAD_STATE_IDLE,
    .last_error = QUAD_ERR_NONE,
};

/* Version string returned by GET_VERSION. */
static const char glQuadVersionStr[] = QUAD_FW_VERSION_STR;

/*
 * EP0 IN response scratch buffer. EP0 data transfers use the High-BandWidth DMA
 * engine, so the buffer must live in HB-reachable RAM (.hbBufSection), matching
 * the example's Ep0TempBuffer. 64 bytes is the EP0 max packet at High-Speed and
 * is large enough for every response here (status is 24 bytes).
 */
static __attribute__((section(".hbBufSection"), used, aligned(32)))
uint8_t glQuadEp0Buf[64];

/*
 * EP0 OUT receive buffer for bitstream chunks (FPGA_LOAD). Also in HB RAM. 4 KB
 * is the recommended chunk size (see ep0-command-set-v0.0.1.md §3); it bounds
 * the per-transfer bit-bang time.
 */
static __attribute__((section(".hbBufSection"), used, aligned(32)))
uint8_t glQuadRxBuf[4096];

quad_status_t *Cy_Quad_GetStatus(void)
{
    return &glQuadStatus;
}

/* Copy up to wLength bytes of a response into the HB EP0 buffer and send it. */
static bool quad_send_ep0(cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                          const void *src, uint16_t len, uint16_t wLength)
{
    uint16_t n = (wLength < len) ? wLength : len;

    if (n > sizeof(glQuadEp0Buf)) {
        n = sizeof(glQuadEp0Buf);
    }
    memcpy(glQuadEp0Buf, src, n);
    return (Cy_USB_USBD_SendEndp0Data(pUsbdCtxt, glQuadEp0Buf, n)
            == CY_USBD_STATUS_SUCCESS);
}

/*
 * Receive an EP0 OUT data stage of len bytes into buf and wait for completion.
 * On failure the EP0 IN endpoint is stalled here. Returns true on success.
 */
static bool quad_recv_ep0(cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                          uint8_t *buf, uint16_t len)
{
    uint16_t loopCnt = 250u;

    if (Cy_USB_USBD_RecvEndp0Data(pUsbdCtxt, buf, len) != CY_USBD_STATUS_SUCCESS) {
        Cy_USB_USBD_EndpSetClearStall(pUsbdCtxt, 0x00, CY_USB_ENDP_DIR_IN, true);
        return false;
    }

    while ((!Cy_USBD_IsEp0ReceiveDone(pUsbdCtxt)) && (loopCnt--)) {
        Cy_SysLib_DelayUs(10);
    }

    if (!Cy_USBD_IsEp0ReceiveDone(pUsbdCtxt)) {
        Cy_USB_USBD_RetireRecvEndp0Data(pUsbdCtxt);
        Cy_USB_USBD_EndpSetClearStall(pUsbdCtxt, 0x00, CY_USB_ENDP_DIR_IN, true);
        return false;
    }

    return true;
}

bool Cy_Quad_VendorRqtHandler(cy_stc_usb_app_ctxt_t *pAppCtxt)
{
    cy_stc_usb_usbd_ctxt_t *pUsbdCtxt = pAppCtxt->pUsbdCtxt;
    uint8_t  bRequest = pUsbdCtxt->setupReq.bRequest;
    uint16_t wLength  = pUsbdCtxt->setupReq.wLength;

    switch (bRequest) {
        case QUAD_CMD_START_STREAM:
            /*
             * Assert LinkTrain, train both LVDS links, then start the
             * LVDS->EP1-IN data path. Training blocks up to
             * QUAD_TRAIN_TIMEOUT_MS; ACK the (no-data) request afterwards.
             * Success/failure is reported via GET_STATUS (state / last_error).
             */
            (void)Cy_QuadStream_Start(pAppCtxt);
            Cy_USBD_SendACkSetupDataStatusStage(pUsbdCtxt);
            return true;

        case QUAD_CMD_STOP_STREAM:
            Cy_QuadStream_Stop(pAppCtxt);
            Cy_USBD_SendACkSetupDataStatusStage(pUsbdCtxt);
            return true;

        case QUAD_CMD_PING: {
            /* Return the magic word so the host can confirm two-way comms. */
            uint32_t beat = QUAD_STATUS_MAGIC;
            return quad_send_ep0(pUsbdCtxt, &beat, sizeof(beat), wLength);
        }

        case QUAD_CMD_GET_VERSION:
            /* Include the terminating NUL in the reported length. */
            return quad_send_ep0(pUsbdCtxt, glQuadVersionStr,
                                 (uint16_t)sizeof(glQuadVersionStr), wLength);

        case QUAD_CMD_GET_STATUS:
            return quad_send_ep0(pUsbdCtxt, &glQuadStatus,
                                 (uint16_t)sizeof(glQuadStatus), wLength);

        case QUAD_CMD_RESET:
            /* RX888-compatible reset: ACK, wait (wValue+1) ms, then reset. */
            Cy_USBD_SendACkSetupDataStatusStage(pUsbdCtxt);
            Cy_SysLib_Delay((uint32_t)pUsbdCtxt->setupReq.wValue + 1u);
            NVIC_SystemReset();
            return true;                 /* not reached */

        case QUAD_CMD_GPIO_WRITE: {
            /* wValue bit0 = level; wIndex = (portNum << 8) | pinNum. */
            uint8_t  port  = (uint8_t)(pUsbdCtxt->setupReq.wIndex >> 8);
            uint8_t  pin   = (uint8_t)(pUsbdCtxt->setupReq.wIndex & 0xFFu);
            uint8_t  level = (uint8_t)(pUsbdCtxt->setupReq.wValue & 0x1u);
            cy_stc_gpio_pin_config_t cfg;

            memset((void *)&cfg, 0, sizeof(cfg));
            cfg.driveMode = CY_GPIO_DM_STRONG_IN_OFF;
            cfg.hsiom     = HSIOM_SEL_GPIO;
            cfg.outVal    = level;
            (void)Cy_GPIO_Pin_Init(Cy_GPIO_PortToAddr(port), pin, &cfg);
            Cy_USBD_SendACkSetupDataStatusStage(pUsbdCtxt);
            return true;
        }

        case QUAD_CMD_GPIO_READ: {
            /* wIndex = (portNum << 8) | pinNum; returns 1 byte (level). */
            uint8_t  port = (uint8_t)(pUsbdCtxt->setupReq.wIndex >> 8);
            uint8_t  pin  = (uint8_t)(pUsbdCtxt->setupReq.wIndex & 0xFFu);
            cy_stc_gpio_pin_config_t cfg;
            uint8_t  level;

            memset((void *)&cfg, 0, sizeof(cfg));
            cfg.driveMode = CY_GPIO_DM_HIGHZ;
            cfg.hsiom     = HSIOM_SEL_GPIO;
            (void)Cy_GPIO_Pin_Init(Cy_GPIO_PortToAddr(port), pin, &cfg);
            level = (uint8_t)Cy_GPIO_Read(Cy_GPIO_PortToAddr(port), pin);
            return quad_send_ep0(pUsbdCtxt, &level, 1u, wLength);
        }

        case QUAD_CMD_I2C_WRITE: {
            /* wValue = 7-bit addr; wIndex = (addrWidth<<8)|reg; data = 1 byte. */
            uint16_t addr      = pUsbdCtxt->setupReq.wValue;
            uint8_t  addrWidth = (uint8_t)(pUsbdCtxt->setupReq.wIndex >> 8);
            uint8_t  reg       = (uint8_t)(pUsbdCtxt->setupReq.wIndex & 0xFFu);

            if ((wLength == 0u) || (wLength > sizeof(glQuadRxBuf))) {
                return false;
            }
            if (!quad_recv_ep0(pUsbdCtxt, glQuadRxBuf, wLength)) {
                return true;
            }
            if (Cy_I2C_Write(addr, reg, glQuadRxBuf[0], addrWidth, 1)
                    != CY_SCB_I2C_SUCCESS) {
                glQuadStatus.last_error = QUAD_ERR_I2C_NAK;
            }
            return true;
        }

        case QUAD_CMD_I2C_READ: {
            /* wValue = 7-bit addr; wIndex = (addrWidth<<8)|reg; read wLength bytes.
             * NOTE: Cy_I2C_Read requires addrWidth 1 or 2 (register-based); a
             * register-less pure read (e.g. PCF8574) is not yet supported. */
            uint16_t addr      = pUsbdCtxt->setupReq.wValue;
            uint8_t  addrWidth = (uint8_t)(pUsbdCtxt->setupReq.wIndex >> 8);
            uint8_t  reg       = (uint8_t)(pUsbdCtxt->setupReq.wIndex & 0xFFu);
            uint16_t n         = wLength;

            if ((n == 0u) || (n > sizeof(glQuadEp0Buf))) {
                return false;
            }
            if (Cy_I2C_Read(addr, reg, glQuadEp0Buf, addrWidth, (uint8_t)n)
                    != CY_SCB_I2C_SUCCESS) {
                glQuadStatus.last_error = QUAD_ERR_I2C_NAK;
                return false;            /* stall to signal the read failed */
            }
            return (Cy_USB_USBD_SendEndp0Data(pUsbdCtxt, glQuadEp0Buf, n)
                    == CY_USBD_STATUS_SUCCESS);
        }

        case QUAD_CMD_SI5345_LOAD: {
            /* wValue = phase; data stage = {page,reg,data} records. */
            uint16_t phase = pUsbdCtxt->setupReq.wValue;

            if ((wLength == 0u) || (wLength > sizeof(glQuadRxBuf))) {
                return false;
            }
            if (!quad_recv_ep0(pUsbdCtxt, glQuadRxBuf, wLength)) {
                return true;
            }
            /* Reset the device once, just before the preamble block. */
            if (phase == QUAD_SI5345_PHASE_PREAMBLE) {
                Cy_QuadSi5345_ResetPulse();
            }
            (void)Cy_QuadSi5345_WriteRecords(glQuadRxBuf, wLength);
            return true;
        }

        case QUAD_CMD_SI5345_DELAY:
            /* Optional firmware-side delay (wValue ms); host sleep is preferred. */
            Cy_SysLib_Delay(pUsbdCtxt->setupReq.wValue);
            Cy_USBD_SendACkSetupDataStatusStage(pUsbdCtxt);
            return true;

        case QUAD_CMD_FPGA_RESET: {
            /* Pulse PROGRAM_B and wait for INIT_B; no data stage. */
            bool ok = Cy_QuadFpga_Reset();
            glQuadStatus.fpga_initb = (uint8_t)ok;
            glQuadStatus.fpga_bytes = 0;
            glQuadStatus.state      = QUAD_STATE_IDLE;
            glQuadStatus.last_error = ok ? QUAD_ERR_NONE : QUAD_ERR_FPGA_INITB_LOW;
            Cy_USBD_SendACkSetupDataStatusStage(pUsbdCtxt);
            return true;
        }

        case QUAD_CMD_FPGA_LOAD: {
            /* wValue = phase; data stage = raw bitstream bytes. */
            uint16_t phase = pUsbdCtxt->setupReq.wValue;

            if ((wLength == 0u) || (wLength > sizeof(glQuadRxBuf))) {
                return false;   /* invalid length -> caller stalls */
            }
            if (!quad_recv_ep0(pUsbdCtxt, glQuadRxBuf, wLength)) {
                return true;    /* receive failed and was already stalled */
            }

            Cy_QuadFpga_ShiftBytes(glQuadRxBuf, wLength);
            if (phase == QUAD_FPGA_PHASE_END) {
                (void)Cy_QuadFpga_Finish();
            }
            return true;
        }

        case QUAD_CMD_ADC_CONFIG: {
            /* wValue = device select (0 both / 1 AB / 2 CD); data = {addr,data}. */
            uint16_t dev = pUsbdCtxt->setupReq.wValue;

            if ((wLength == 0u) || (wLength > sizeof(glQuadRxBuf))) {
                return false;
            }
            if (!quad_recv_ep0(pUsbdCtxt, glQuadRxBuf, wLength)) {
                return true;
            }
            Cy_QuadAdc_WriteRecords((uint8_t)dev, glQuadRxBuf, wLength);
            glQuadStatus.adc_configured = 1u;
            return true;
        }

        case QUAD_CMD_ADC_TESTPAT:
            /* wValue = mode (0 off / 1 ramp); no data stage. */
            Cy_QuadAdc_SetTestPattern((uint8_t)pUsbdCtxt->setupReq.wValue);
            Cy_USBD_SendACkSetupDataStatusStage(pUsbdCtxt);
            return true;

        case QUAD_CMD_FPGA_STATUS: {
            /* 8-byte poll: [initB, lastError, 0, 0, bytesLoaded(u32 LE)]. */
            uint8_t  rsp[8];
            uint32_t bytes = Cy_QuadFpga_BytesLoaded();

            rsp[0] = (uint8_t)Cy_QuadFpga_InitBHigh();
            rsp[1] = glQuadStatus.last_error;
            rsp[2] = 0u;
            rsp[3] = 0u;
            memcpy(&rsp[4], &bytes, sizeof(bytes));
            return quad_send_ep0(pUsbdCtxt, rsp, (uint16_t)sizeof(rsp), wLength);
        }

        default:
            /* Not a quad command (or not yet implemented): let the caller stall. */
            return false;
    }
}
