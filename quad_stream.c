/***************************************************************************//**
* \file quad_stream.c
* \version 0.0.1
*
* START_STREAM / STOP_STREAM data-path handling for the Quad-PSWS-FPGA receiver.
*
* Sequencing (interface-contract-RESOLVED.md §4):
*   1. Assert LinkTrain (P4.3)  -> FPGA drives the 0x784B5A12 training pattern.
*   2. Bring up the LVDS PHY and run PHY + LINK training.
*   3. Wait for lock on both links (LNK0|LNK1); on timeout report TRAIN_FAIL.
*   4. Deassert LinkTrain          -> FPGA reverts to data mode.
*   5. Enable the LVDS-ingress -> USB bulk-IN (EP1) HBDMA channel; stream.
*
* The firmware moves raw 32-bit DDR words; the channel-ID (bit 14) / PPS (bit 15)
* tag bits and per-channel demux are the host's job, so the data path itself is
* sample-format agnostic.
*
*******************************************************************************/

#include "cy_pdl.h"
#include "cy_device.h"
#include "cy_usb_common.h"
#include "cy_usb_usbd.h"
#include "cy_usb_app.h"
#include "cy_debug.h"
#include "quad_stream.h"
#include "quad_vendor.h"

/* Drive the discrete LinkTrain output (P4.3). asserted -> FPGA sends training. */
static void quad_linktrain_set(bool assert)
{
    uint32_t level = assert ? QUAD_LINKTRAIN_ACTIVEHI : (QUAD_LINKTRAIN_ACTIVEHI ^ 1u);
    Cy_GPIO_Write(Cy_GPIO_PortToAddr(QUAD_LINKTRAIN_PORT), QUAD_LINKTRAIN_PIN, level);
}

/* Configure P4.3 as a strong GPIO output, initially deasserted. */
static void quad_linktrain_init(void)
{
    cy_stc_gpio_pin_config_t cfg;

    memset((void *)&cfg, 0, sizeof(cfg));
    cfg.driveMode = CY_GPIO_DM_STRONG_IN_OFF;
    cfg.hsiom     = HSIOM_SEL_GPIO;
    cfg.outVal    = (QUAD_LINKTRAIN_ACTIVEHI ^ 1u);     /* start deasserted */
    (void)Cy_GPIO_Pin_Init(Cy_GPIO_PortToAddr(QUAD_LINKTRAIN_PORT),
                           QUAD_LINKTRAIN_PIN, &cfg);
}

bool Cy_QuadStream_Start(cy_stc_usb_app_ctxt_t *pAppCtxt)
{
    quad_status_t *st = Cy_Quad_GetStatus();
    cy_stc_hbdma_channel_t *pChn =
        &(pAppCtxt->endpInDma[LVDS_STREAMING_EP].hbDmaChannel);
    uint32_t waitMs;

    DBG_APP_INFO("START_STREAM: asserting LinkTrain, starting training\r\n");

    /* Step 1: assert LinkTrain so the FPGA emits the training pattern. */
    quad_linktrain_init();
    quad_linktrain_set(true);

    /* Step 2: bring the LVDS PHY up and start PHY + LINK training afresh. */
    pAppCtxt->isPhyTrainingDone  = false;
    pAppCtxt->isLinkTrainingDone = 0u;
    InitLvdsInterface(pAppCtxt);

    /* Step 3: wait for LINK training block-detect on both links. */
    for (waitMs = 0u; waitMs < QUAD_TRAIN_TIMEOUT_MS; waitMs++) {
        if ((pAppCtxt->isLinkTrainingDone & QUAD_LINK_MASK) == QUAD_LINK_MASK) {
            break;
        }
        Cy_SysLib_Delay(1u);
    }

    st->train_result = pAppCtxt->isLinkTrainingDone;

    if ((pAppCtxt->isLinkTrainingDone & QUAD_LINK_MASK) != QUAD_LINK_MASK) {
        /* Training failed: revert LinkTrain, tear the PHY down, report. */
        DBG_APP_ERR("START_STREAM: training timeout, links=0x%x\r\n",
                    pAppCtxt->isLinkTrainingDone);
        quad_linktrain_set(false);
        Cy_Quad_LvdsDeinit();
        st->link_trained = 0u;
        st->streaming    = 0u;
        st->last_error   = QUAD_ERR_TRAIN_FAIL;
        st->state        = QUAD_STATE_ERROR;
        return false;
    }

    /* Step 4: both links locked -> deassert LinkTrain; FPGA switches to data. */
    quad_linktrain_set(false);
    st->link_trained = 1u;
    st->last_error   = QUAD_ERR_NONE;
    st->state        = QUAD_STATE_TRAINED;
    DBG_APP_INFO("START_STREAM: both links trained (0x%x)\r\n", st->train_result);

    /*
     * Step 5: (re)enable the LVDS-ingress -> EP1-IN channel. It is created and
     * enabled at enumeration (Cy_USB_AppSetupEndpDmaParamsSS); reset+enable here
     * so a START after a previous STOP restarts from a clean channel state.
     */
    Cy_HBDma_Channel_Reset(pChn);
    Cy_HBDma_Channel_Enable(pChn, 0);

    st->streaming     = 1u;
    st->overrun_count = 0u;
    st->state         = QUAD_STATE_STREAMING;
    DBG_APP_INFO("START_STREAM: streaming\r\n");
    return true;
}

void Cy_QuadStream_Stop(cy_stc_usb_app_ctxt_t *pAppCtxt)
{
    quad_status_t *st = Cy_Quad_GetStatus();
    cy_stc_hbdma_channel_t *pChn =
        &(pAppCtxt->endpInDma[LVDS_STREAMING_EP].hbDmaChannel);

    DBG_APP_INFO("STOP_STREAM: tearing down data path\r\n");

    /* Stop and flush the streaming channel / endpoint. */
    Cy_HBDma_Channel_Disable(pChn);
    Cy_USBD_FlushEndp(pAppCtxt->pUsbdCtxt, LVDS_STREAMING_EP, CY_USB_ENDP_DIR_IN);

    /* Tear the LVDS PHY down so the next START_STREAM retrains cleanly. */
    Cy_Quad_LvdsDeinit();

    /* Make sure LinkTrain is left deasserted. */
    quad_linktrain_set(false);

    st->streaming    = 0u;
    st->link_trained = 0u;
    st->state        = QUAD_STATE_FPGA_LOADED;
}
