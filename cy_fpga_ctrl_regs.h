/***************************************************************************//**
* \file cy_fpga_ctrl_regs.h
* \version 1.0
*
* Defines the I2C based control register interface implemented by the EFinix
* TI180 FPGA.
*
*******************************************************************************
* \copyright
* (c) (2026), Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.
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
*******************************************************************************/

#ifndef _CY_FPGA_CTRL_REGS_H_
#define _CY_FPGA_CTRL_REGS_H_

#if defined(__cplusplus)
extern "C" {
#endif

/* List of control registers implemented by the FPGA's I2C slave device. */
typedef enum cy_en_fpga_i2c_regaddr_
{
    /* Common Register Info */
    FPGA_VERSION_ADDRESS                   = 0x00,              /* Version register: read-only. */
    FPGA_UVC_U3V_SELECTION_ADDRESS         = 0x01,              /* Register to select between UVC and U3V patterns. */
    FPGA_UVC_HEADER_CTRL_ADDRESS           = 0x02,              /* Register to select header addition method. */
    FPGA_LVDS_PHY_TRAINING_ADDRESS         = 0x03,              /* Register to set PHY training pattern. */
    FPGA_LVDS_LINK_TRAINING_BLK_P0_ADDRESS = 0x04,              /* Register to set LINK training pattern. */
    FPGA_LVDS_LINK_TRAINING_BLK_P1_ADDRESS = 0x05,              /* Register to set LINK training pattern. */
    FPGA_LVDS_LINK_TRAINING_BLK_P2_ADDRESS = 0x06,              /* Register to set LINK training pattern. */
    FPGA_LVDS_LINK_TRAINING_BLK_P3_ADDRESS = 0x07,              /* Register to set LINK training pattern. */
    FPGA_ACTIVE_DEVICE_MASK_ADDRESS        = 0x08,              /* Register to enable streaming devices. */
    FPGA_LOW_PWR_MODE_ADDRESS              = 0x09,              /* Register to trigger low power entry. */
    FPGA_PHY_LINK_CONTROL_ADDRESS          = 0x0A,              /* Register to control PHY and LINK training. */
    FPGA_EXT_CONTROLLER_STATUS_ADDRESS     = 0x0B,              /* Register to configure FPGA's memory controller. */

    DEVICE0_OFFSET                         = 0x20,              /* Offset for device0 control registers. */
    DEVICE1_OFFSET                         = 0x3C,              /* Offset for device1 control registers. */
    DEVICE2_OFFSET                         = 0x58,              /* Offset for device2 control registers. */
    DEVICE3_OFFSET                         = 0x74,              /* Offset for device3 control registers. */
    DEVICE4_OFFSET                         = 0x90,              /* Offset for device4 control registers. */
    DEVICE5_OFFSET                         = 0xAC,              /* Offset for device5 control registers. */
    DEVICE6_OFFSET                         = 0xC8,              /* Offset for device6 control registers. */
    DEVICE7_OFFSET                         = 0xE4               /* Offset for device7 control registers. */
} cy_en_fpga_i2c_regaddr_t;

/* List of control registers for each streaming device implemented by the FPGA's I2C slave block.
 * This bank of registers exists for each of the 8 device instances at the offsets defined in
 * \reg cy_en_fpga_i2c_regaddr_t
 */
typedef enum cy_en_strmdev_i2c_regaddr_
{
    FPGA_DEVICE_STREAM_ENABLE_ADDRESS      = 0x00,              /* Stream control register. */
    FPGA_DEVICE_STREAM_MODE_ADDRESS        = 0x01,              /* Stream mode selection. */
    DEVICE_IMAGE_HEIGHT_LSB_ADDRESS        = 0x02,              /* LSB of image height. */
    DEVICE_IMAGE_HEIGHT_MSB_ADDRESS        = 0x03,              /* MSB of image height. */
    DEVICE_IMAGE_WIDTH_LSB_ADDRESS         = 0x04,              /* LSB of image width. */
    DEVICE_IMAGE_WIDTH_MSB_ADDRESS         = 0x05,              /* MSB of image width. */
    DEVICE_FPS_ADDRESS                     = 0x06,              /* Number of frames per second to be generated. */
    DEVICE_PIXEL_WIDTH_ADDRESS             = 0x07,              /* Video pixel width setting. */
    DEVICE_SOURCE_TYPE_ADDRESS             = 0x08,              /* Stream data source selection. */
    DEVICE_FLAG_STATUS_ADDRESS             = 0x09,              /* FPGA FIFO status. */
    DEVICE_MIPI_STATUS_ADDRESS             = 0x0A,              /* MIPI status register. */
    DEVICE_HDMI_SOURCE_INFO_ADDRESS        = 0x0B,              /* HDMI source status register. */
    DEVICE_U3V_STREAM_MODE_ADDRESS         = 0x0C,              /* U3V stream mode control. */
    DEVICE_U3V_CHUNK_MODE_ADDRESS          = 0x0D,              /* U3V chunk mode control. */
    DEVICE_U3V_TRIGGER_MODE_ADDRESS        = 0x0E,              /* U3V trigger mode control. */
    DEVICE_ACTIVE_THREAD_INFO_ADDRESS      = 0x0F,              /* Number of threads active. */
    DEVICE_THREAD1_INFO_ADDRESS            = 0x10,              /* Active thread #1 */
    DEVICE_THREAD2_INFO_ADDRESS            = 0x11,              /* Active thread #2 */
    DEVICE_THREAD1_SOCKET_INFO_ADDRESS     = 0x12,              /* Active socket #1 */
    DEVICE_THREAD2_SOCKET_INFO_ADDRESS     = 0x13,              /* Active socket #2 */
    DEVICE_FLAG_INFO_ADDRESS               = 0x14,              /* Type of DMA flag selected. */
    DEVICE_COUNTER_CRC_INFO_ADDRESS        = 0x15,              /* CRC information register. */
    DEVICE_BUFFER_SIZE_LSB_ADDRESS         = 0x16,              /* LSB of DMA buffer size. */
    DEVICE_BUFFER_SIZE_MSB_ADDRESS         = 0x17,              /* MSB of DMA buffer size. */
} cy_en_strmdev_i2c_regaddr_t;

/* Field/value definitions for FPGA_UVC_U3V_SELECTION_ADDRESS */
#define FPGA_UVC_ENABLE                 (0x01)          /* FPGA generates UVC pattern. */
#define FPGA_U3V_ENABLE                 (0x00)          /* FPGA generates U3V pattern. */

/* Field/value definitions for FPGA_UVC_HEADER_CTRL_ADDRESS */
#define FPGA_UVC_HEADER_DISABLE         (0x00)          /* Header added by FX firmware. */
#define FPGA_UVC_HEADER_ENABLE          (0x01)          /* Header added by FPGA. */
#define FPGA_INMD_UVC_HEADER_ENABLE     (0x02)          /* Header added using INMD feature. */

/* Field/value definitions for FPGA_LOW_PWR_MODE_ADDRESS */
#define LVDS_L3_MODE_ENTER              (0x01)          /* Value to enter L3 mode. */

/* Field/value definitions for FPGA_PHY_LINK_CONTROL_ADDRESS */
#define FPGA_TRAINING_DISABLE           (0x00)          /* Training disabled. */
#define FPGA_PHY_CONTROL                (0x01)          /* PHY Training is required */
#define FPGA_LINK_CONTROL               (0x02)          /*  Link Training is required */
#define P0_TRAINING_DONE                (0x40)          /* Port 0 training is completed */
#define P1_TRAINING_DONE                (0x80)          /* Port 1 training is completed */

/* Field/value definitions for FPGA_EXT_CONTROLLER_STATUS_ADDRESS */
#define DMA_READY_STATUS                (0x01)          /* DMA Ready flag status */
#define DDR_CONFIG_STATUS               (0x02)          /* DDR configuration status */
#define DDR_BUSY_STATUS                 (0x04)          /* DDR Controller busy status */
#define DDR_CMD_QUEUE_FULL_STATUS       (0x08)          /* Command queue full status */
#define DATPATH_IDLE_STATUS             (0x10)          /* Datapath is idle or not */

/* Field/value definitions for FPGA_DEVICE_STREAM_ENABLE_ADDRESS */
#define CAMERA_APP_DISABLE              (0x00)          /* Streaming disabled for this device. */
#define DMA_CH_RESET                    (0x01)          /* Reset the DMA datapath. */
#define CAMERA_APP_ENABLE               (0x02)          /* Streaming enabled for this device. */
#define APP_STOP_NOTIFICATION           (0x04)          /* Stop streaming for this device. */

/* Field/value definitions for FPGA_DEVICE_STREAM_MODE_ADDRESS */
#define NO_CONVERSION                   (0x00)          /* Send data with no conversion. */
#define INTERLEAVED_MODE                (0x01)          /* Select thread interleaved mode. */
#define STILL_CAPTURE                   (0x02)          /* Send still capture data. */
#define MONO_8_CONVERSION               (0x04)          /* Convert data to MONO 8-bit format. */
#define YUV422_420_CONVERSION           (0x08)          /* Convert data from YUV422 to 420 format. */

/* Field/value definitions for DEVICE_PIXEL_WIDTH_ADDRESS */
#define PIXEL_8_BIT                     (8)             /* 8 bits per pixel. */
#define PIXEL_12_BIT                    (12)            /* 12 bits per pixel. */
#define PIXEL_16_BIT                    (16)            /* 16 bits per pixel. */
#define PIXEL_24_BIT                    (24)            /* 24 bits per pixel. */
#define PIXEL_32_BIT                    (32)            /* 32 bits per pixel. */

/* Field/value definitions for DEVICE_SOURCE_TYPE_ADDRESS */
#define INTERNAL_COLORBAR               (0x00)          /* Send internally generated colorbar. */
#define HDMI_SOURCE                     (0x01)          /* Send data received from HDMI controller. */
#define MIPI_SOURCE                     (0x02)          /* Send data received from MIPI image sensor. */

/* Field/value definitions for DEVICE_FLAG_STATUS_ADDRESS */
#define SLAVE_FIFO_ALMOST_EMPTY         (0x01)          /* Slave FIFO almost empty status */
#define INTER_MED_FIFO_EMPTY            (0x02)          /* Intermediate FIFO empty status */
#define INTER_MED_FIFO_FULL             (0x04)          /* Intermediate FIFO full status */
#define DDR_FULL_FRAME_WRITE_COMPLETE   (0x10)          /* DDR write status (Full frame write complete) */
#define DDR_FULL_FRAME_READ_COMPLETE    (0x20)          /* DDR write status (Full frame read complete) */

/* Field/value definitions for DEVICE_HDMI_SOURCE_INFO_ADDRESS */
#define HDMI_DISCONECT                  (0x00)          /* Bit 0: Source connection status */
#define THIN_MIPI                       (0x00)          /* Bit 0: Thin MIPI */
#define HDMI_CONNECT                    (0x01)          /* Bit 0: Source connection status */
#define HDMI_DUAL_CH                    (0x02)          /* Bit 1: 0 for single channel and 1 for dual channel */
#define HDMI_COLOR_FORMAT_YUV444        (0x04)          /* Bit 2: 0 for YUV 422 mode and 1 for YUV 444 mode */
#define SONY_CIS                        (0x10)          /* Bit 4: Enable CIS ISP IP (Valid for only MIPI source) */
#define SONY_MIPI                       (0x20)          /* Bit 5: Enable Crop Algorithm (Valid for only MIPI source) */

/* Field/value definitions for DEVICE_FLAG_INFO_ADDRESS */
#define FX10_READY_TO_REC_DATA          (0x08)          /* FX is ready to receive data. */
#define NEW_UVC_PACKET_START            (0x02)          /* New UVC packet to be started. */
#define NEW_FRAME_START                 (0x01)          /* New video frame to be started. */

#if defined(__cplusplus)
}
#endif

#endif /* _CY_FPGA_CTRL_REGS_H_ */

/*[]*/

