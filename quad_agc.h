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
* \file quad_agc.h
* \version 0.0.2
*
* AD8370 variable-gain-amplifier ("AGC") serial control for the Quad-PSWS-FPGA
* receiver. Four AD8370s (channels A..D) share the serial DATA and CLK lines; a
* separate active-high LATCH per channel selects which device(s) a gain word is
* transferred to.
*
* Pins (interface-contract-RESOLVED.md, all FX10 side):
*   DATA  P4.1 (shared)   CLK  P4.2 (shared)
*   LATCH A/B/C/D  P8.0 / P8.1 / P8.2 / P8.3
*
* Gain word (AD8370 data sheet, Rev. B): a single 8-bit code, MSB first.
*   bit[7]   = gain range  (0 = low range -11..+17 dB, 1 = high range +6..+34 dB)
*   bit[6:0] = 7-bit linear gain magnitude within that range
* The host computes the code; the firmware only shifts it out. Data is clocked
* into the shift register on the CLK rising edge while LATCH is low, then
* transferred to the core on the LATCH high-going edge.
*
* NOTE: MSB-first bit order and the active-high LATCH edge are taken from the
* data sheet's serial-interface description but have not been exercised on
* hardware; confirm both (and that the shared DATA/CLK reach all four parts)
* during bench bring-up.
*
*******************************************************************************/

#ifndef _QUAD_AGC_H_
#define _QUAD_AGC_H_

#include <stdint.h>
#include <stdbool.h>

#if defined(__cplusplus)
extern "C" {
#endif

/* Channel-select mask bits (AGC_SELECT wValue, AD8370 latch lines). */
#define QUAD_AGC_CH_A      (0x1u)
#define QUAD_AGC_CH_B      (0x2u)
#define QUAD_AGC_CH_C      (0x4u)
#define QUAD_AGC_CH_D      (0x8u)
#define QUAD_AGC_CH_ALL    (QUAD_AGC_CH_A | QUAD_AGC_CH_B | QUAD_AGC_CH_C | QUAD_AGC_CH_D)

/* AD8370 gain-word fields. */
#define QUAD_AGC_RANGE_HIGH (0x80u)   /* bit[7] set = high gain range */
#define QUAD_AGC_GAIN_MASK  (0x7Fu)   /* bit[6:0] = gain magnitude    */

/* Configure the serial GPIOs to their idle states (idempotent). */
void Cy_QuadAgc_InitPins(void);

/*
 * Set which channel(s) subsequent Cy_QuadAgc_SetGain() calls program. Only the
 * low four bits are used (QUAD_AGC_CH_*); default after init is QUAD_AGC_CH_ALL.
 */
void Cy_QuadAgc_Select(uint8_t mask);

/* The current channel-select mask. */
uint8_t Cy_QuadAgc_GetSelected(void);

/*
 * Shift an 8-bit AD8370 gain code out on the shared DATA/CLK lines and latch it
 * into every channel in the current select mask. All four parts receive the
 * word on the shared lines; only latched channels update their gain.
 */
void Cy_QuadAgc_SetGain(uint8_t code);

#if defined(__cplusplus)
}
#endif

#endif /* _QUAD_AGC_H_ */
