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

void Cy_QuadSi5345_ResetPulse(void)
{
    si5345_init_reset_pin();

    /* Active-low RSTB: drive low, hold, release. */
    Cy_GPIO_Clr(SI5345_RESET_PORT, SI5345_RESET_PIN);
    Cy_SysLib_DelayUs(10);
    Cy_GPIO_Set(SI5345_RESET_PORT, SI5345_RESET_PIN);
    Cy_SysLib_Delay(1);         /* brief settle before I2C access */

    glSi5345Page = -1;          /* device page is unknown after reset */
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
