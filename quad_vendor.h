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
* \file quad_vendor.h
* \version 0.0.1
*
* EP0 vendor command handling for the Quad-PSWS-FPGA receiver firmware.
* See ../../ep0-command-set-v0.0.1.md for the full command specification.
*
*******************************************************************************/

#ifndef _QUAD_VENDOR_H_
#define _QUAD_VENDOR_H_

#include <stdint.h>
#include <stdbool.h>
#include "cy_usb_app.h"

#if defined(__cplusplus)
extern "C" {
#endif

/* Firmware version. BCD form: 0x0001 == v0.0.1. */
#define QUAD_FW_VERSION_BCD   (0x0001u)
#define QUAD_FW_VERSION_STR   "quad-fx10-firmware v0.0.1"

/* GET_STATUS magic word: ASCII "QUAD" (little-endian on the wire). */
#define QUAD_STATUS_MAGIC     (0x51554144ul)

/*
 * Quad-specific EP0 vendor command codes.
 *
 * The base I/O commands reuse the RX888/SDDC numeric codes so RX888-derived
 * host code and the PCF8574 parallel-I/O path work unchanged; quad-specific
 * commands live in 0xC0..0xFF.
 *
 * Codes are chosen to avoid the vendor commands inherited from the LVDS example
 * (cy_usb_app_vendor_opcode_t in cy_usb_app.h), which occupy:
 *   0xA0/0xA1 memory read/write, 0xB7 bootloader, 0xB8 data-xfer test,
 *   0xE0 device reset, 0xF0 MS-OS vendor code, 0xF6 get device speed.
 * Those remain available as bring-up/debug aids.
 */
typedef enum {
    QUAD_CMD_START_STREAM = 0xAA,   /* RX888 STARTFX3  */
    QUAD_CMD_STOP_STREAM  = 0xAB,   /* RX888 STOPFX3   */
    QUAD_CMD_PING         = 0xAC,   /* RX888 TESTFX3   */
    QUAD_CMD_GPIO_WRITE   = 0xAD,   /* RX888 GPIOFX3   */
    QUAD_CMD_I2C_WRITE    = 0xAE,   /* RX888 I2CWFX3   */
    QUAD_CMD_I2C_READ     = 0xAF,   /* RX888 I2CRFX3   */
    QUAD_CMD_RESET        = 0xB1,   /* RX888 RESETFX3  */
    QUAD_CMD_GPIO_READ    = 0xB3,   /* quad-added      */
    QUAD_CMD_SI5345_LOAD  = 0xC0,
    QUAD_CMD_SI5345_DELAY = 0xC1,
    QUAD_CMD_FPGA_RESET   = 0xD0,
    QUAD_CMD_FPGA_LOAD    = 0xD1,
    QUAD_CMD_FPGA_STATUS  = 0xD2,
    QUAD_CMD_ADC_CONFIG   = 0xE1,   /* moved off 0xE0 (DEVICE_RESET_CODE) */
    QUAD_CMD_ADC_TESTPAT  = 0xE2,
    QUAD_CMD_AGC_SELECT   = 0xF1,   /* moved off 0xF0 (MS_VENDOR_CODE)    */
    QUAD_CMD_AGC_GAIN     = 0xF2,
    QUAD_CMD_GET_STATUS   = 0xFE,
    QUAD_CMD_GET_VERSION  = 0xFF,
} quad_vendor_cmd_t;

/* Firmware lifecycle state (GET_STATUS.state). */
typedef enum {
    QUAD_STATE_IDLE        = 0,
    QUAD_STATE_FPGA_LOADED = 1,
    QUAD_STATE_TRAINED     = 2,
    QUAD_STATE_STREAMING   = 3,
    QUAD_STATE_ERROR       = 0xFF,
} quad_state_t;

/* Last-error codes (GET_STATUS.last_error). */
typedef enum {
    QUAD_ERR_NONE           = 0,
    QUAD_ERR_FPGA_INITB_LOW = 1,
    QUAD_ERR_FPGA_COUNT_BAD = 2,
    QUAD_ERR_TRAIN_FAIL     = 3,
    QUAD_ERR_I2C_NAK        = 4,
} quad_error_t;

/* GET_STATUS payload. Packed for a stable host-visible wire layout (24 bytes). */
typedef struct __attribute__((packed)) {
    uint32_t magic;          /* QUAD_STATUS_MAGIC ("QUAD")            */
    uint16_t fw_version;     /* QUAD_FW_VERSION_BCD                   */
    uint8_t  state;          /* quad_state_t                         */
    uint8_t  last_error;     /* quad_error_t                         */
    uint8_t  fpga_initb;     /* 1 = INIT_B high (no config error)    */
    uint8_t  link_trained;   /* 1 = link training passed             */
    uint8_t  adc_configured; /* 1 = ADC register block applied       */
    uint8_t  streaming;      /* 1 = HBDMA streaming active           */
    uint32_t fpga_bytes;     /* bytes shifted to FPGA (expect 2192012) */
    uint32_t train_result;   /* per-lane training detail (bitmap)    */
    uint32_t overrun_count;  /* DMA overruns since START_STREAM      */
} quad_status_t;

/* Accessor for the global status block; subsystems update its fields. */
quad_status_t *Cy_Quad_GetStatus(void);

/*
 * Handle a quad-specific vendor control request. Returns true if the request
 * was recognized and handled (data stage / ACK / stall already issued), or
 * false to let the caller apply its default EP0 stall.
 */
bool Cy_Quad_VendorRqtHandler(cy_stc_usb_app_ctxt_t *pAppCtxt);

#if defined(__cplusplus)
}
#endif

#endif /* _QUAD_VENDOR_H_ */
