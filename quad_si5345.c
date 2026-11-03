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
* \file quad_si5345.c
* \version 0.0.1
*
* Si5345 clock synthesizer programming. See quad_si5345.h and
* interface-contract-RESOLVED.md §5.
*
*******************************************************************************/

#include "cy_pdl.h"
#include "cy_device.h"
#include "cy_gpio.h"
#include "cy_usb_i2c.h"
#include "quad_si5345.h"
#include "quad_vendor.h"
#include <string.h>

/* Si5345 reset line (active-low RSTB). */
#define SI5345_RESET_PORT   P9_1_PORT
#define SI5345_RESET_PIN    P9_1_PIN

/* Current device page; -1 forces a page-register write on the next access. */
static int  glSi5345Page = -1;
static bool glSi5345PinReady = false;

static void si5345_init_reset_pin(void)
{
    cy_stc_gpio_pin_config_t pinCfg;

    if (glSi5345PinReady) {
        return;
    }
    memset((void *)&pinCfg, 0, sizeof(pinCfg));
    pinCfg.driveMode = CY_GPIO_DM_STRONG_IN_OFF;
    pinCfg.hsiom     = HSIOM_SEL_GPIO;
    (void)Cy_GPIO_Pin_Init(SI5345_RESET_PORT, SI5345_RESET_PIN, &pinCfg);
    Cy_GPIO_Set(SI5345_RESET_PORT, SI5345_RESET_PIN);   /* deasserted (high) */
    glSi5345PinReady = true;
}

void Cy_QuadSi5345_BeginLoad(void)
{
    /*
     * Do NOT hard-reset the Si5345 (RSTB low) before applying the register map.
     *
     * A hard reset (RSTB pin) reloads ALL registers from NVM, and that NVM
     * download overwrites any register writes made before it completes -- the
     * Si5345 reference manual is explicit that writes issued during the download
     * are lost. The previous code pulsed RSTB and then waited only 1 ms, far
     * short of the NVM-ready time, so the ClockBuilder register map could be
     * silently clobbered: the writes ACK, the device never locks, and no clocks
     * come out (SYSINCAL/LOL stay asserted).
     *
     * The ClockBuilder register map applied here is self-contained -- its
     * postamble issues a SOFT_RST (0x001C) to apply the new configuration, which
     * does not touch NVM. So we only ensure RSTB is deasserted (driven high) and
     * invalidate the cached page; no hard reset, no NVM-ready wait.
     */
    si5345_init_reset_pin();    /* configures the pin and drives RSTB high */
    glSi5345Page = -1;          /* page-select cache is unknown */
}

bool Cy_QuadSi5345_WriteRecords(const uint8_t *records, uint32_t len)
{
    uint32_t nRec = len / QUAD_SI5345_RECORD_LEN;
    uint32_t i;
    bool     ok = true;

    for (i = 0; i < nRec; i++) {
        uint8_t page = records[(i * QUAD_SI5345_RECORD_LEN) + 0];
        uint8_t reg  = records[(i * QUAD_SI5345_RECORD_LEN) + 1];
        uint8_t data = records[(i * QUAD_SI5345_RECORD_LEN) + 2];

        /* Switch page only when it changes (single-byte writes, no burst). */
        if ((int)page != glSi5345Page) {
            if (Cy_I2C_Write(QUAD_SI5345_I2C_ADDR, QUAD_SI5345_PAGE_REG,
                             page, 1, 1) != CY_SCB_I2C_SUCCESS) {
                ok = false;
                break;
            }
            glSi5345Page = (int)page;
        }

        if (Cy_I2C_Write(QUAD_SI5345_I2C_ADDR, reg, data, 1, 1)
                != CY_SCB_I2C_SUCCESS) {
            ok = false;
            break;
        }
    }

    if (!ok) {
        Cy_Quad_GetStatus()->last_error = QUAD_ERR_I2C_NAK;
        glSi5345Page = -1;      /* resync page on the next attempt */
    }

    return ok;
}
