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
* \file quad_fpga.h
* \version 0.0.1
*
* Xilinx Artix-7 slave-serial configuration (bit-banged) for the Quad-PSWS-FPGA
* receiver firmware.
*
* Pins (per interface-contract-RESOLVED.md, all on the FX10):
*   PROGRAM_B  P9.6 (out, active low)   INIT_B  P9.7 (in, low = config error)
*   CCLK       P8.6 (out)               DIN     P8.7 (out, data)
*   DONE       not connected  -> success proxy = byte-count + INIT_B high
*   M[2:0] = 111 (slave-serial), strapped in hardware.
*
*******************************************************************************/

#ifndef _QUAD_FPGA_H_
#define _QUAD_FPGA_H_

#include <stdint.h>
#include <stdbool.h>

#if defined(__cplusplus)
extern "C" {
#endif

/* XC7A15T uncompressed bitstream size (bytes) = 17,536,096 bits / 8. */
#define QUAD_FPGA_BITSTREAM_BYTES   (2192012ul)

/* FPGA_LOAD phase, carried in the EP0 setup wValue. */
#define QUAD_FPGA_PHASE_START       (0x00u)
#define QUAD_FPGA_PHASE_CONT        (0x01u)
#define QUAD_FPGA_PHASE_END         (0x02u)

/* Configure the four config GPIOs to their idle states (idempotent). */
void Cy_QuadFpga_InitPins(void);

/*
 * Begin configuration: pulse PROGRAM_B low then high and wait for INIT_B to go
 * high (config memory cleared, ready for data). Resets the byte counter.
 * Returns true if INIT_B went high within the timeout.
 */
bool Cy_QuadFpga_Reset(void);

/* Shift len bytes MSB-first out DIN/CCLK; accumulates the byte counter. */
void Cy_QuadFpga_ShiftBytes(const uint8_t *data, uint32_t len);

/*
 * Finish configuration: clock trailing startup cycles, then evaluate success as
 * (bytes shifted == QUAD_FPGA_BITSTREAM_BYTES) AND (INIT_B still high). Updates
 * the global quad status block. Returns true on success.
 */
bool Cy_QuadFpga_Finish(void);

/* Current INIT_B level (true = high = no config error). */
bool Cy_QuadFpga_InitBHigh(void);

/* Bytes shifted to the FPGA since the last Cy_QuadFpga_Reset(). */
uint32_t Cy_QuadFpga_BytesLoaded(void);

#if defined(__cplusplus)
}
#endif

#endif /* _QUAD_FPGA_H_ */
