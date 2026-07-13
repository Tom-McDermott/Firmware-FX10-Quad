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

/* ADS62P45 test-pattern register (interface contract §6). */
#define QUAD_ADC_REG_TESTPAT (0x16u)
#define QUAD_ADC_TESTPAT_OFF (0x00u)
#define QUAD_ADC_TESTPAT_RAMP (0x40u)

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
