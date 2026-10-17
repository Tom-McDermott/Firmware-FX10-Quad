/*
 * Copyright (c) 2026 TAPR (Tucson Amateur Packet Radio Corporation)
 *
 * This software was created by a human being with assistance from Claude.ai.
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
* \file quad_agc.c
* \version 0.0.2
*
* AD8370 VGA serial control, bit-banged over GPIO. See quad_agc.h and
* interface-contract-RESOLVED.md.
*
* The four AD8370s share DATA (P4.1) and CLK (P4.2); each has its own active-high
* LATCH (P8.0..P8.3). A gain code is clocked MSB-first into every part's shift
* register on the shared lines, then transferred to the core of the selected
* channel(s) by a LATCH high pulse. Timing per AD8370 data sheet Rev. B; confirm
* bit order / latch polarity on hardware (see quad_agc.h).
*
*******************************************************************************/

#include "cy_pdl.h"
#include "cy_device.h"
#include "cy_gpio.h"
#include "quad_agc.h"
#include <string.h>

/* Serial pins (interface-contract-RESOLVED.md), all FX10 side. */
#define AGC_DATA_PORT    P4_1_PORT
#define AGC_DATA_PIN     P4_1_PIN
#define AGC_CLK_PORT     P4_2_PORT
#define AGC_CLK_PIN      P4_2_PIN

/* Per-channel latch pins, indexed A..D. */
static GPIO_PRT_Type *const glAgcLatchPort[4] = {
    P8_0_PORT, P8_1_PORT, P8_2_PORT, P8_3_PORT,
};
static const uint32_t glAgcLatchPin[4] = {
    P8_0_PIN, P8_1_PIN, P8_2_PIN, P8_3_PIN,
};

static bool    glAgcPinsReady = false;
static uint8_t glAgcSelected   = QUAD_AGC_CH_ALL;

void Cy_QuadAgc_InitPins(void)
{
    cy_stc_gpio_pin_config_t pinCfg;
    int ch;

    if (glAgcPinsReady) {
        return;
    }
    memset((void *)&pinCfg, 0, sizeof(pinCfg));
    pinCfg.driveMode = CY_GPIO_DM_STRONG_IN_OFF;
    pinCfg.hsiom     = HSIOM_SEL_GPIO;
    (void)Cy_GPIO_Pin_Init(AGC_DATA_PORT, AGC_DATA_PIN, &pinCfg);
    (void)Cy_GPIO_Pin_Init(AGC_CLK_PORT,  AGC_CLK_PIN,  &pinCfg);
    for (ch = 0; ch < 4; ch++) {
        (void)Cy_GPIO_Pin_Init(glAgcLatchPort[ch], glAgcLatchPin[ch], &pinCfg);
    }

    /* Idle: CLK low, DATA low, all latches low (inactive). */
    Cy_GPIO_Clr(AGC_CLK_PORT,  AGC_CLK_PIN);
    Cy_GPIO_Clr(AGC_DATA_PORT, AGC_DATA_PIN);
    for (ch = 0; ch < 4; ch++) {
        Cy_GPIO_Clr(glAgcLatchPort[ch], glAgcLatchPin[ch]);
    }

    glAgcPinsReady = true;
}

void Cy_QuadAgc_Select(uint8_t mask)
{
    glAgcSelected = mask & QUAD_AGC_CH_ALL;
}

uint8_t Cy_QuadAgc_GetSelected(void)
{
    return glAgcSelected;
}

/* Shift an 8-bit word MSB-first; DATA set while CLK low, sampled on rising edge. */
static void agc_shift8(uint8_t word)
{
    int bit;

    for (bit = 7; bit >= 0; bit--) {
        if ((word >> bit) & 0x1u) {
            Cy_GPIO_Set(AGC_DATA_PORT, AGC_DATA_PIN);
        } else {
            Cy_GPIO_Clr(AGC_DATA_PORT, AGC_DATA_PIN);
        }
        Cy_GPIO_Set(AGC_CLK_PORT, AGC_CLK_PIN);
        Cy_GPIO_Clr(AGC_CLK_PORT, AGC_CLK_PIN);
    }
}

/* Pulse one channel's latch high then low to transfer its shift register. */
static void agc_latch_pulse(int ch)
{
    Cy_GPIO_Set(glAgcLatchPort[ch], glAgcLatchPin[ch]);
    Cy_SysLib_DelayUs(1);
    Cy_GPIO_Clr(glAgcLatchPort[ch], glAgcLatchPin[ch]);
}

void Cy_QuadAgc_SetGain(uint8_t code)
{
    int ch;

    Cy_QuadAgc_InitPins();

    /* Load the word into all four shift registers over the shared lines. */
    agc_shift8(code);
    Cy_SysLib_DelayUs(1);                       /* last-bit hold before latch */

    /* Transfer to the core of each selected channel. */
    for (ch = 0; ch < 4; ch++) {
        if (glAgcSelected & (1u << ch)) {
            agc_latch_pulse(ch);
        }
    }
}
