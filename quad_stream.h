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
* \file quad_stream.h
* \version 0.0.1
*
* START_STREAM / STOP_STREAM handling for the Quad-PSWS-FPGA receiver firmware:
* discrete LinkTrain control, LVDS PHY+LINK training, and the LVDS-ingress ->
* USB bulk-IN data path.
*
* See ../../PLAN-streaming-and-link-training.md and
* ../../interface-contract-RESOLVED.md §3-4 for the data format and the
* link-training protocol this implements.
*
*******************************************************************************/

#ifndef _QUAD_STREAM_H_
#define _QUAD_STREAM_H_

#include <stdbool.h>
#include "cy_usb_app.h"

#if defined(__cplusplus)
extern "C" {
#endif

/*
 * LinkTrain: discrete FX10 output D32 / P4.3 (interface-contract-RESOLVED.md
 * §1.2, §4). Asserting it makes the FPGA drive the repeating 0x784B5A12 training
 * pattern on all data lanes and used control inputs; deasserting it switches the
 * FPGA to data mode. Assertion is active-high (assumption — confirm on hardware;
 * the P0.6 state-machine alternative in §1.2 is a later Verilog option).
 */
#define QUAD_LINKTRAIN_PORT      (4u)   /* GPIO port 4 */
#define QUAD_LINKTRAIN_PIN       (3u)   /* pin 3  -> P4.3 / D32 */
#define QUAD_LINKTRAIN_ACTIVEHI  (1u)   /* asserted level */

/*
 * Link-training lock is signalled per LVDS link via Cy_LVDS_PhyEventCb setting
 * bit(smNo) in pAppCtxt->isLinkTrainingDone. The 32-bit interface uses both
 * links (P0D[15:0] + P1D[15:0]), so lock requires LNK0 and LNK1: bits 0 and 1.
 */
#define QUAD_LINK_MASK           (0x03u)

/* Max time to wait for both links to lock before declaring QUAD_ERR_TRAIN_FAIL. */
#define QUAD_TRAIN_TIMEOUT_MS    (1000u)

/*
 * START_STREAM (0xAA): assert LinkTrain, bring up the LVDS PHY and run PHY+LINK
 * training, wait for lock on both links (timeout -> QUAD_ERR_TRAIN_FAIL),
 * deassert LinkTrain, then enable the LVDS-ingress -> EP1-IN HBDMA channel.
 * Updates the global status block. Returns true if streaming started.
 */
bool Cy_QuadStream_Start(cy_stc_usb_app_ctxt_t *pAppCtxt);

/*
 * STOP_STREAM (0xAB): disable/flush the streaming channel and endpoint and tear
 * the LVDS PHY down so the next START_STREAM retrains cleanly.
 */
void Cy_QuadStream_Stop(cy_stc_usb_app_ctxt_t *pAppCtxt);

/* Tear down the LVDS block (defined in main.c, where the LVDS globals live). */
void Cy_Quad_LvdsDeinit(void);

#if defined(__cplusplus)
}
#endif

#endif /* _QUAD_STREAM_H_ */
