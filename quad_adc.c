/***************************************************************************//**
* \file quad_adc.c
* \version 0.0.1
*
* ADS62P45 dual-ADC serial configuration, bit-banged over GPIO.
* See quad_adc.h and interface-contract-RESOLVED.md §6.
*
* NOTE: SEN is treated as active-low and SDATA is sampled on the SCLK rising
* edge (MSB first); the hardware RESET pin (P9.0) is treated as active-high and
* left inactive (the register-0x00 write performs the reset). Confirm these
* against ADS62P45 datasheet Figure 6 during hardware bring-up.
*
*******************************************************************************/

#include "cy_pdl.h"
#include "cy_device.h"
#include "cy_gpio.h"
#include "quad_adc.h"
#include "quad_vendor.h"
#include <string.h>

/* Serial pins (interface-contract-RESOLVED.md), all FX10 side. */
#define ADC_DATA_PORT   P9_4_PORT
#define ADC_DATA_PIN    P9_4_PIN
#define ADC_CLK_PORT    P9_5_PORT
#define ADC_CLK_PIN     P9_5_PIN
#define ADC_EN_AB_PORT  P9_2_PORT
#define ADC_EN_AB_PIN   P9_2_PIN
#define ADC_EN_CD_PORT  P9_3_PORT
#define ADC_EN_CD_PIN   P9_3_PIN
#define ADC_RESET_PORT  P9_0_PORT
#define ADC_RESET_PIN   P9_0_PIN

/* Enable-mask bits. */
#define ADC_EN_AB_BIT   (0x1u)
#define ADC_EN_CD_BIT   (0x2u)

static bool glAdcPinsReady = false;

static uint8_t adc_dev_to_mask(uint8_t dev)
{
    switch (dev) {
        case QUAD_ADC_DEV_AB: return ADC_EN_AB_BIT;
        case QUAD_ADC_DEV_CD: return ADC_EN_CD_BIT;
        case QUAD_ADC_DEV_BOTH:
        default:              return ADC_EN_AB_BIT | ADC_EN_CD_BIT;
    }
}

void Cy_QuadAdc_InitPins(void)
{
    cy_stc_gpio_pin_config_t pinCfg;

    if (glAdcPinsReady) {
        return;
    }
    memset((void *)&pinCfg, 0, sizeof(pinCfg));
    pinCfg.driveMode = CY_GPIO_DM_STRONG_IN_OFF;
    pinCfg.hsiom     = HSIOM_SEL_GPIO;
    (void)Cy_GPIO_Pin_Init(ADC_DATA_PORT,  ADC_DATA_PIN,  &pinCfg);
    (void)Cy_GPIO_Pin_Init(ADC_CLK_PORT,   ADC_CLK_PIN,   &pinCfg);
    (void)Cy_GPIO_Pin_Init(ADC_EN_AB_PORT, ADC_EN_AB_PIN, &pinCfg);
    (void)Cy_GPIO_Pin_Init(ADC_EN_CD_PORT, ADC_EN_CD_PIN, &pinCfg);
    (void)Cy_GPIO_Pin_Init(ADC_RESET_PORT, ADC_RESET_PIN, &pinCfg);

    /* Idle: CLK low, DATA low, enables deasserted (high), RESET inactive (low). */
    Cy_GPIO_Clr(ADC_CLK_PORT,   ADC_CLK_PIN);
    Cy_GPIO_Clr(ADC_DATA_PORT,  ADC_DATA_PIN);
    Cy_GPIO_Set(ADC_EN_AB_PORT, ADC_EN_AB_PIN);
    Cy_GPIO_Set(ADC_EN_CD_PORT, ADC_EN_CD_PIN);
    Cy_GPIO_Clr(ADC_RESET_PORT, ADC_RESET_PIN);

    glAdcPinsReady = true;
}

/* Assert the selected enable(s) (active-low). */
static void adc_enable(uint8_t mask, bool assert)
{
    /* assert -> drive low; deassert -> drive high. */
    if (mask & ADC_EN_AB_BIT) {
        if (assert) Cy_GPIO_Clr(ADC_EN_AB_PORT, ADC_EN_AB_PIN);
        else        Cy_GPIO_Set(ADC_EN_AB_PORT, ADC_EN_AB_PIN);
    }
    if (mask & ADC_EN_CD_BIT) {
        if (assert) Cy_GPIO_Clr(ADC_EN_CD_PORT, ADC_EN_CD_PIN);
        else        Cy_GPIO_Set(ADC_EN_CD_PORT, ADC_EN_CD_PIN);
    }
}

/* Shift a 16-bit word MSB-first; SDATA set while CLK low, sampled on rising edge. */
static void adc_shift16(uint16_t word)
{
    int bit;

    for (bit = 15; bit >= 0; bit--) {
        if ((word >> bit) & 0x1u) {
            Cy_GPIO_Set(ADC_DATA_PORT, ADC_DATA_PIN);
        } else {
            Cy_GPIO_Clr(ADC_DATA_PORT, ADC_DATA_PIN);
        }
        Cy_GPIO_Set(ADC_CLK_PORT, ADC_CLK_PIN);
        Cy_GPIO_Clr(ADC_CLK_PORT, ADC_CLK_PIN);
    }
}

/* Write one {addr, data} register to the device(s) in mask. */
static void adc_write_reg(uint8_t mask, uint8_t addr, uint8_t data)
{
    adc_enable(mask, true);
    Cy_SysLib_DelayUs(1);                       /* SEN setup */
    adc_shift16(((uint16_t)addr << 8) | data);
    Cy_SysLib_DelayUs(1);                       /* last-bit hold before latch */
    adc_enable(mask, false);                    /* deassert latches the write */
    Cy_SysLib_DelayUs(1);
}

void Cy_QuadAdc_WriteRecords(uint8_t dev, const uint8_t *records, uint32_t len)
{
    uint8_t  mask = adc_dev_to_mask(dev);
    uint32_t nRec = len / QUAD_ADC_RECORD_LEN;
    uint32_t i;

    Cy_QuadAdc_InitPins();

    for (i = 0; i < nRec; i++) {
        uint8_t addr = records[(i * QUAD_ADC_RECORD_LEN) + 0];
        uint8_t data = records[(i * QUAD_ADC_RECORD_LEN) + 1];
        adc_write_reg(mask, addr, data);
    }
}

void Cy_QuadAdc_SetTestPattern(uint8_t mode)
{
    uint8_t val = (mode != 0u) ? QUAD_ADC_TESTPAT_RAMP : QUAD_ADC_TESTPAT_OFF;

    Cy_QuadAdc_InitPins();
    adc_write_reg(ADC_EN_AB_BIT | ADC_EN_CD_BIT, QUAD_ADC_REG_TESTPAT, val);
}
