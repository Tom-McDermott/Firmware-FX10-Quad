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
* \file quad_fpga.c
* \version 0.0.1
*
* Xilinx Artix-7 (XC7A15T) slave-serial configuration, bit-banged over GPIO.
* See quad_fpga.h and interface-contract-RESOLVED.md for the pin map and the
* DONE-not-connected success proxy (byte count + INIT_B).
*
*******************************************************************************/

#include "cy_pdl.h"
#include "cy_device.h"
#include "cy_gpio.h"
#include "quad_fpga.h"
#include "quad_vendor.h"
#include <string.h>

/* Config pins (interface-contract-RESOLVED.md, all FX10 side). */
#define FPGA_PROG_B_PORT    P9_6_PORT
#define FPGA_PROG_B_PIN     P9_6_PIN
#define FPGA_INIT_B_PORT    P9_7_PORT
#define FPGA_INIT_B_PIN     P9_7_PIN
#define FPGA_CCLK_PORT      P8_6_PORT
#define FPGA_CCLK_PIN       P8_6_PIN
#define FPGA_DIN_PORT       P8_7_PORT
#define FPGA_DIN_PIN        P8_7_PIN

/* Extra CCLK cycles clocked after the last data bit for the startup sequence. */
#define FPGA_STARTUP_CLOCKS (128u)

/* INIT_B-high wait timeout after the PROGRAM_B pulse (milliseconds). */
#define FPGA_INITB_TIMEOUT_MS (1000u)

static bool     glFpgaPinsReady = false;
static uint32_t glFpgaBytes = 0;

/* One CCLK pulse: data must already be set on DIN (sampled on the rising edge). */
static inline void fpga_clock(void)
{
    Cy_GPIO_Set(FPGA_CCLK_PORT, FPGA_CCLK_PIN);
    Cy_GPIO_Clr(FPGA_CCLK_PORT, FPGA_CCLK_PIN);
}

void Cy_QuadFpga_InitPins(void)
{
    cy_stc_gpio_pin_config_t pinCfg;

    if (glFpgaPinsReady) {
        return;
    }

    memset((void *)&pinCfg, 0, sizeof(pinCfg));

    /* Outputs: PROGRAM_B, CCLK, DIN. */
    pinCfg.driveMode = CY_GPIO_DM_STRONG_IN_OFF;
    pinCfg.hsiom     = HSIOM_SEL_GPIO;
    (void)Cy_GPIO_Pin_Init(FPGA_PROG_B_PORT, FPGA_PROG_B_PIN, &pinCfg);
    (void)Cy_GPIO_Pin_Init(FPGA_CCLK_PORT,   FPGA_CCLK_PIN,   &pinCfg);
    (void)Cy_GPIO_Pin_Init(FPGA_DIN_PORT,    FPGA_DIN_PIN,    &pinCfg);

    /* Idle states: PROGRAM_B deasserted (high), CCLK low, DIN low. */
    Cy_GPIO_Set(FPGA_PROG_B_PORT, FPGA_PROG_B_PIN);
    Cy_GPIO_Clr(FPGA_CCLK_PORT,   FPGA_CCLK_PIN);
    Cy_GPIO_Clr(FPGA_DIN_PORT,    FPGA_DIN_PIN);

    /* Input: INIT_B (FPGA drives; high-Z on our side). */
    pinCfg.driveMode = CY_GPIO_DM_HIGHZ;
    pinCfg.hsiom     = HSIOM_SEL_GPIO;
    (void)Cy_GPIO_Pin_Init(FPGA_INIT_B_PORT, FPGA_INIT_B_PIN, &pinCfg);

    glFpgaPinsReady = true;
}

bool Cy_QuadFpga_Reset(void)
{
    uint32_t waitMs = FPGA_INITB_TIMEOUT_MS;

    Cy_QuadFpga_InitPins();

    /* Pulse PROGRAM_B low (>250 ns) to clear the configuration memory. */
    Cy_GPIO_Clr(FPGA_PROG_B_PORT, FPGA_PROG_B_PIN);
    Cy_SysLib_DelayUs(2);
    Cy_GPIO_Set(FPGA_PROG_B_PORT, FPGA_PROG_B_PIN);

    /* Wait for INIT_B to rise: config memory cleared, ready for the bitstream. */
    while ((Cy_GPIO_Read(FPGA_INIT_B_PORT, FPGA_INIT_B_PIN) == 0u) && (waitMs != 0u)) {
        Cy_SysLib_Delay(1);
        waitMs--;
    }

    glFpgaBytes = 0;
    return (Cy_GPIO_Read(FPGA_INIT_B_PORT, FPGA_INIT_B_PIN) != 0u);
}

void Cy_QuadFpga_ShiftBytes(const uint8_t *data, uint32_t len)
{
    uint32_t i;
    int      bit;

    for (i = 0; i < len; i++) {
        uint8_t b = data[i];

        /* Xilinx slave-serial clocks each byte MSB first. */
        for (bit = 7; bit >= 0; bit--) {
            if ((b >> bit) & 0x01u) {
                Cy_GPIO_Set(FPGA_DIN_PORT, FPGA_DIN_PIN);
            } else {
                Cy_GPIO_Clr(FPGA_DIN_PORT, FPGA_DIN_PIN);
            }
            fpga_clock();
        }
    }

    glFpgaBytes += len;
}

bool Cy_QuadFpga_Finish(void)
{
    quad_status_t *st = Cy_Quad_GetStatus();
    uint32_t i;
    bool     initbHigh;
    bool     countOk;
    bool     ok;

    /* Extra startup clocks (DONE-based handshake is unavailable; DIN held high). */
    Cy_GPIO_Set(FPGA_DIN_PORT, FPGA_DIN_PIN);
    for (i = 0; i < FPGA_STARTUP_CLOCKS; i++) {
        fpga_clock();
    }

    initbHigh = (Cy_GPIO_Read(FPGA_INIT_B_PORT, FPGA_INIT_B_PIN) != 0u);
    countOk   = (glFpgaBytes == QUAD_FPGA_BITSTREAM_BYTES);
    ok        = initbHigh && countOk;

    st->fpga_initb = (uint8_t)initbHigh;
    st->fpga_bytes = glFpgaBytes;
    if (ok) {
        st->state      = QUAD_STATE_FPGA_LOADED;
        st->last_error = QUAD_ERR_NONE;
    } else {
        st->state      = QUAD_STATE_ERROR;
        st->last_error = (!initbHigh) ? QUAD_ERR_FPGA_INITB_LOW
                                      : QUAD_ERR_FPGA_COUNT_BAD;
    }

    return ok;
}

bool Cy_QuadFpga_InitBHigh(void)
{
    Cy_QuadFpga_InitPins();
    return (Cy_GPIO_Read(FPGA_INIT_B_PORT, FPGA_INIT_B_PIN) != 0u);
}

uint32_t Cy_QuadFpga_BytesLoaded(void)
{
    return glFpgaBytes;
}
