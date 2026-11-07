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
* \file quad_adc.h
* \version 0.0.1
*
* ADS62P45 dual-ADC serial configuration (bit-banged) for the Quad-PSWS-FPGA
* receiver. Two dual-ADCs (pairs AB and CD) share the serial DATA and CLK lines
* with a separate enable per pair; identical data can be written to both at once.
*
* Pins (interface-contract-RESOLVED.md §1.3 / §6, all FX10 side):
*   DATA  P9.4   CLK  P9.5   EN_AB  P9.2   EN_CD  P9.3   RESET  P9.0
*
* Each register write is a 16-bit frame {addr[7:0], data[7:0]} shifted MSB-first
* while the target enable(s) are asserted. Register 0x00 must be written first
* (device reset + serial-mode select). Timing per ADS62P45 datasheet Figure 6.
*
*******************************************************************************/

#ifndef _QUAD_ADC_H_
#define _QUAD_ADC_H_

#include <stdint.h>
#include <stdbool.h>

#if defined(__cplusplus)
extern "C" {
#endif

/* Device selection, carried in the ADC_CONFIG wValue. */
#define QUAD_ADC_DEV_BOTH   (0u)
#define QUAD_ADC_DEV_AB     (1u)
#define QUAD_ADC_DEV_CD     (2u)

#define QUAD_ADC_RECORD_LEN (2u)    /* {addr, data} per record */

/*
 * ADS62P45 test-pattern register (interface contract §6; ADS62P45 datasheet
 * register 0x16). Fields: D[2:0] = test pattern (100b = digital ramp),
 * D4 = data format (0 = 2's complement, 1 = straight binary). So:
 *   0x14 = ramp, straight-binary (monotonic 0..0x3FFF -- matches the streaming
 *          ramp-monotonicity check)
 *   0x04 = ramp, 2's-complement
 * (An earlier value of 0x40 was wrong -- bit 6 is not in this register's fields.
 * Caught by Tom/N5EG and confirmed against the datasheet.)
 *
 * Note: D4 also selects the NORMAL output data format, not just the ramp. So
 * TESTPAT_OFF (0x00) reverts to 2's-complement; if the data path expects
 * straight binary, turn the pattern off with 0x10 instead (and pair with the
 * straight-binary RAMP 0x14). Pick RAMP/OFF to match the intended data format.
 */
#define QUAD_ADC_REG_TESTPAT     (0x16u)
#define QUAD_ADC_TESTPAT_OFF     (0x00u)   /* no pattern, 2's-complement format */
#define QUAD_ADC_TESTPAT_RAMP    (0x14u)   /* ramp, straight-binary format      */
#define QUAD_ADC_TESTPAT_RAMP_2C (0x04u)   /* ramp, 2's-complement format       */

/* Configure the serial GPIOs to their idle states (idempotent). */
void Cy_QuadAdc_InitPins(void);

/*
 * Write a block of {addr, data} records to the selected device(s)
 * (QUAD_ADC_DEV_*). Host must place register 0x00 first.
 */
void Cy_QuadAdc_WriteRecords(uint8_t dev, const uint8_t *records, uint32_t len);

/* Convenience: set the digital test pattern on both ADCs (0 = off, 1 = ramp). */
void Cy_QuadAdc_SetTestPattern(uint8_t mode);

#if defined(__cplusplus)
}
#endif

#endif /* _QUAD_ADC_H_ */
