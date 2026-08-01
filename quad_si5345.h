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
* \file quad_si5345.h
* \version 0.0.1
*
* Si5345 clock synthesizer programming for the Quad-PSWS-FPGA receiver.
*
* The host parses a ClockBuilder-Pro register export into {page, reg, data}
* triplets and streams them via SI5345_LOAD. The Si5345 uses a paged 16-bit
* register map: the high address byte selects a page (written to register 0x01),
* the low byte is the register. See interface-contract-RESOLVED.md §5.
*
* I2C: 7-bit address 0x6B (A[1:0]=11), single-byte writes, on shared SCB0.
* Reset line: P9.1 (active-low RSTB).
*
*******************************************************************************/

#ifndef _QUAD_SI5345_H_
#define _QUAD_SI5345_H_

#include <stdint.h>
#include <stdbool.h>

#if defined(__cplusplus)
extern "C" {
#endif

#define QUAD_SI5345_I2C_ADDR    (0x6Bu)   /* 7-bit; PDL adds the R/W bit */
#define QUAD_SI5345_PAGE_REG    (0x01u)   /* page-select register        */
#define QUAD_SI5345_RECORD_LEN  (3u)      /* {page, reg, data} per record */

/* SI5345_LOAD phases (EP0 wValue), matching the ClockBuilder file sections. */
#define QUAD_SI5345_PHASE_PREAMBLE  (0x00u)
#define QUAD_SI5345_PHASE_MAIN      (0x01u)
#define QUAD_SI5345_PHASE_POSTAMBLE (0x02u)

/* Configure the reset GPIO and pulse RSTB (active-low) to reset the device. */
void Cy_QuadSi5345_ResetPulse(void);

/*
 * Write a block of {page, reg, data} records over I2C, managing the page-select
 * register (0x01) as the page changes. Returns true if every write ACKed.
 */
bool Cy_QuadSi5345_WriteRecords(const uint8_t *records, uint32_t len);

#if defined(__cplusplus)
}
#endif

#endif /* _QUAD_SI5345_H_ */
