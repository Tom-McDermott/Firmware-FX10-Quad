/***************************************************************************//**
* \file cy_usb_descriptors.c
* \version 1.0
*
* Defines the USB descriptors used in the EZ-USB FX LVDS to USB Streaming Application.
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

#include "cy_pdl.h"
#include "cy_usb_common.h"
#include "cy_usbhs_cal_drv.h"
#include "cy_usb_usbd.h"
#include "cy_usb_app.h"

#define USB3_DESC_ATTRIBUTES __attribute__ ((section(".descSection"), used)) __attribute__ ((aligned (32)))

/* Standard device descriptor for USB 2.0 */
USB3_DESC_ATTRIBUTES uint8_t CyFxUSB20DeviceDscr[] =
{
    0x12,                           /* Descriptor size */
    0x01,                           /* Device descriptor type */
    0x10,0x02,                      /* USB 2.10 */
    0x00,                           /* Device class */
    0x00,                           /* Device sub-class */
    0x00,                           /* Device protocol */
    0x40,                           /* Maxpacket size for EP0 : 64 bytes */
    0xB4,0x04,                      /* Vendor ID */
#if USE_WINUSB
    0x07,0x48,                      /* Product ID */
#else
    0xF1,0x00,                      /* Product ID */
#endif /* USE_WINUSB */
    0x00,0x00,                      /* Device release number */
    0x01,                           /* Manufacture string index */
    0x02,                           /* Product string index */
    0x00,                           /* Serial number string index */
    0x01                            /* Number of configurations */
};

/* Standard device descriptor for USB 3.x */
USB3_DESC_ATTRIBUTES uint8_t CyFxUSB30DeviceDscr[] =
{
    0x12,                           /* Descriptor size */
    0x01,                           /* Device descriptor type */
    0x20,0x03,                      /* USB 3.2 */
    0x00,                           /* Device class */
    0x00,                           /* Device sub-class */
    0x00,                           /* Device protocol */
    0x09,                           /* Maxpacket size for EP0 : 2^9 */
    0xB4,0x04,                      /* Vendor ID */
#if USE_WINUSB
    0x07,0x48,                      /* Product ID */
#else
    0xF0,0x00,                      /* Product ID */
#endif /* USE_WINUSB */
    0x00,0x00,                      /* Device release number */
    0x01,                           /* Manufacture string index */
    0x02,                           /* Product string index */
    0x00,                           /* Serial number string index */
    0x01                            /* Number of configurations */
};

/* Standard full speed configuration descriptor */
USB3_DESC_ATTRIBUTES uint8_t CyFxUSBFSConfigDscr[] =
{
    /* Configuration descriptor */
    0x09,                           /* Descriptor size */
    0x02,                           /* Configuration descriptor type */
    0x19, 0x00,                     /* Length of this descriptor and all sub descriptors. */
    0x01,                           /* Number of interfaces */
    0x01,                           /* Configuration number */
    0x00,                           /* COnfiguration string index */
    0xC0,                           /* Config characteristics - self powered without remote wake support. */
    0x32,                           /* Max power consumption of device (in 2mA unit) : 100mA */

    /* Interface descriptor */
    0x09,                           /* Descriptor size */
    0x04,                           /* Interface descriptor type */
    0x00,                           /* Interface number */
    0x00,                           /* Alternate setting number */
    0x01,                           /* Number of endpoints */
    0xFF,                           /* Interface class */
    0x00,                           /* Interface sub class */
    0x00,                           /* Interface protocol code */
    0x00,                           /* Interface descriptor string index */

    /* IN endpoint descriptor */
    0x07,                           /* Descriptor size */
    0x05,                           /* Endpoint descriptor type */
    0x80 | LVDS_STREAMING_EP,       /* Endpoint number: 1-IN */
    CY_USB_ENDP_TYPE_BULK,          /* Endpoint type */
    0x40, 0x00,                     /* Maximum packet size: 64 bytes */
    0x00                            /* Polling interval for bulk EPs = 0 */
};

/* Standard high speed configuration descriptor */
USB3_DESC_ATTRIBUTES uint8_t CyFxUSBHSConfigDscr[] =
{
    /* Configuration descriptor */
    0x09,                           /* Descriptor size */
    0x02,                           /* Configuration descriptor type */
    0x19, 0x00,                     /* Length of this descriptor and all sub descriptors */
    0x01,                           /* Number of interfaces */
    0x01,                           /* Configuration number */
    0x00,                           /* Configuration string index */
    0xC0,                           /* Config characteristics - Self powered without remote wake support. */
    0x32,                           /* Max power consumption of device (in 2mA unit) : 100mA */

    /* Interface descriptor */
    0x09,                           /* Descriptor size */
    0x04,                           /* Interface descriptor type */
    0x00,                           /* Interface number */
    0x00,                           /* Alternate setting number */
    0x01,                           /* Number of endpoints */
    0xFF,                           /* Interface class */
    0x00,                           /* Interface sub class */
    0x00,                           /* Interface protocol code */
    0x00,                           /* Interface descriptor string index */

    /* IN endpoint descriptor */
    0x07,                           /* Descriptor size */
    0x05,                           /* Endpoint descriptor type */
    0x80 | LVDS_STREAMING_EP,       /* Endpoint number: 1-IN */
    CY_USB_ENDP_TYPE_BULK,          /* Endpoint type */
    0x00, 0x02,                     /* Maximum packet size: 512 bytes */
    0x00                            /* Polling interval for bulk EPs = 0 */
};

/* Standard super speed configuration descriptor */
USB3_DESC_ATTRIBUTES uint8_t CyFxUSBSSConfigDscr[] =
{
    /* Configuration descriptor */
    0x09,                           /* Descriptor size */
    0x02,                           /* Configuration descriptor type */
    0x1F, 0x00,                     /* Length of this descriptor and all sub descriptors */
    0x01,                           /* Number of interfaces */
    0x01,                           /* Configuration number */
    0x00,                           /* Configuration string index */
    0xC0,                           /* Config characteristics - Self powered without remote wake support. */
    0x32,                           /* Max power consumption of device (in 8mA unit) : 400mA */

    /* Interface descriptor */
    0x09,                           /* Descriptor size */
    0x04,                           /* Interface Descriptor type */
    0x00,                           /* Interface number */
    0x00,                           /* Alternate setting number */
    0x01,                           /* Number of end points */
    0xFF,                           /* Interface class */
    0x00,                           /* Interface sub class */
    0x00,                           /* Interface protocol code */
    0x00,                           /* Interface descriptor string index */

    /* IN endpoint descriptor */
    0x07,                           /* Descriptor size */
    0x05,                           /* Endpoint descriptor type */
    0x80 | LVDS_STREAMING_EP,       /* Endpoint number: 1-IN */
    CY_USB_ENDP_TYPE_BULK,          /* Endpoint type */
    0x00, 0x04,                     /* Maximum packet size: 1024 bytes */
    0x00,                           /* Polling interval for bulk EPs = 0 */

    /* IN endpoint SS companion descriptor */
    0x06,                           /* Descriptor size */
    0x30,                           /* SS EP companion descriptor type */
    0x0F,                           /* Maximum burst size = 16 packets */
    0x00,                           /* No streams support */
    0x00,                           /* Not a periodic endpoint */
    0x00                            /* Not a periodic endpoint */
};

/* Device qualifier descriptor. */
USB3_DESC_ATTRIBUTES uint8_t CyFxDevQualDscr[] =
{
    0x0A,                           /* Descriptor size */
    0x06,                           /* Device qualifier descriptor type */
    0x00, 0x02,                     /* USB 2.0 */
    0x00,                           /* Device class */
    0x00,                           /* Device sub-class */
    0x00,                           /* Device protocol */
    0x40,                           /* Maxpacket size for EP0 : 64 bytes */
    0x01,                           /* Number of configurations */
    0x00                            /* Reserved */
};

/* Binary Object Store (BOS) Descriptor to be used in USB 2.x connection. */
USB3_DESC_ATTRIBUTES uint8_t CyFxBOSDscr_HS[] =
{
    0x05,                           /* Descriptor size */
    0x0F,                           /* BOS descriptor. */
    0x0C,0x00,                      /* Length of this descriptor and all sub-descriptors */
    0x01,                           /* Number of device capability descriptors */

    /* USB 2.0 extension */
    0x07,                           /* Descriptor size */
    0x10,                           /* Device capability type descriptor */
    0x02,                           /* USB 2.0 extension capability type */
    0x1E,0x64,0x00,0x00,            /* Supported device level features: LPM support, BESL supported,
                                       Baseline BESL=400 us, Deep BESL=1000 us. */
};

/* Binary Object Store (BOS) Descriptor to be used in SuperSpeed connection. */
USB3_DESC_ATTRIBUTES uint8_t CyFxBOSDscr_Gen1[] =
{
    0x05,                           /* Descriptor size */
    0x0F,                           /* BOS descriptor. */
    0x16,0x00,                      /* Length of this descriptor and all sub-descriptors */
    0x02,                           /* Number of device capability descriptors */

    /* USB 2.0 extension */
    0x07,                           /* Descriptor size */
    0x10,                           /* Device capability type descriptor */
    0x02,                           /* USB 2.0 extension capability type */
    0x1E,0x64,0x00,0x00,            /* Supported device level features: LPM support, BESL supported,
                                       Baseline BESL=400 us, Deep BESL=1000 us. */

    /* SuperSpeed device capability */
    0x0A,                           /* Descriptor size */
    0x10,                           /* Device capability type descriptor */
    0x03,                           /* SuperSpeed device capability type */
    0x00,                           /* Supported device level features: Not LTM capable.  */
    0x0E,0x00,                      /* Speeds supported by the device : SS Gen1, HS and FS */
    0x03,                           /* Functionality support */
    0x0A,                           /* U1 Device Exit latency */
    0xFF,0x07                       /* U2 Device Exit latency */
};

/* Binary Object Store (BOS) Descriptor to be used in SuperSpeedPlus connection. */
USB3_DESC_ATTRIBUTES uint8_t CyFxBOSDscr_Gen2[] =
{
    0x05,                           /* Descriptor size */
    0x0F,                           /* BOS descriptor. */
    0x2A,0x00,                      /* Length of this descriptor and all sub-descriptors */
    0x03,                           /* Number of device capability descriptors */

    /* USB 2.0 extension */
    0x07,                           /* Descriptor size */
    0x10,                           /* Device capability type descriptor */
    0x02,                           /* USB 2.0 extension capability type */
    0x1E,0x64,0x00,0x00,            /* Supported device level features: LPM support, BESL supported,
                                       Baseline BESL=400 us, Deep BESL=1000 us. */

    /* SuperSpeed device capability */
    0x0A,                           /* Descriptor size */
    0x10,                           /* Device capability type descriptor */
    0x03,                           /* SuperSpeed device capability type */
    0x00,                           /* Supported device level features: Not LTM capable.  */
    0x0E,0x00,                      /* Speeds supported by the device : SS Gen1, HS and FS */
    0x03,                           /* Functionality support */
    0x0A,                           /* U1 Device Exit latency */
    0xFF,0x07,                      /* U2 Device Exit latency */

    /* SuperSpeedPlus USB device capability */
    0x14,                           /* Descriptor size */
    0x10,                           /* Device capability type descriptor */
    0x0A,                           /* SuperSpeedPlus Device capability */
    0x00,                           /* Reserved */
    0x01,0x00,0x00,0x00,            /* SSAC=1, SSIC=0 */
    0x00,0x11,                      /* SSID=0, Min. RX Lane = 1, Min. Tx Lane = 1 */
    0x00,0x00,                      /* Reserved */
    0x30,0x40,0x0A,0x00,            /* SSID=0, LSE=3(Gb/s), ST=0(Symmetric Rx), LP=1(SSPlus), LSM=10 */
    0xB0,0x40,0x0A,0x00             /* SSID=0, LSE=3(Gb/s), ST=0(Symmetric Tx), LP=1(SSPlus), LSM=10 */
};

/* Binary Object Store (BOS) Descriptor to be used in SuperSpeedPlus connection. */
USB3_DESC_ATTRIBUTES uint8_t CyFxBOSDscr_Gen1x2[] =
{
    0x05,                           /* Descriptor size */
    0x0F,                           /* BOS descriptor. */
    0x2A,0x00,                      /* Length of this descriptor and all sub-descriptors */
    0x03,                           /* Number of device capability descriptors */

    /* USB 2.0 extension */
    0x07,                           /* Descriptor size */
    0x10,                           /* Device capability type descriptor */
    0x02,                           /* USB 2.0 extension capability type */
    0x1E,0x64,0x00,0x00,            /* Supported device level features: LPM support, BESL supported,
                                       Baseline BESL=400 us, Deep BESL=1000 us. */

    /* SuperSpeed device capability */
    0x0A,                           /* Descriptor size */
    0x10,                           /* Device capability type descriptor */
    0x03,                           /* SuperSpeed device capability type */
    0x00,                           /* Supported device level features: Not LTM capable.  */
    0x0E,0x00,                      /* Speeds supported by the device : SS Gen1, HS and FS */
    0x03,                           /* Functionality support */
    0x0A,                           /* U1 Device Exit latency */
    0xFF,0x07,                      /* U2 Device Exit latency */

    /* SuperSpeedPlus USB device capability for G1X2 */
    0x14,                           /* Descriptor size */
    0x10,                           /* Device capability type descriptor */
    0x0A,                           /* SuperSpeedPlus Device capability */
    0x00,                           /* Reserved */
    0x01,0x00,0x00,0x00,            /* SSAC=1, SSIC=0 */
    0x00,0x22,                      /* SSID=0, Min. RX Lane = 2, Min. Tx Lane = 2 */
    0x00,0x00,                      /* Reserved */
    0x30,0x00,0x05,0x00,            /* SSID=0, LSE=3(Gb/s), ST=0(Symmetric Rx), LP=0(SS), LSM=5 */
    0xB0,0x00,0x05,0x00             /* SSID=0, LSE=3(Gb/s), ST=0(Symmetric Tx), LP=0(SS), LSM=5 */
};

/* Binary Object Store (BOS) Descriptor to be used in SuperSpeedPlus connection. */
USB3_DESC_ATTRIBUTES uint8_t CyFxBOSDscr_Gen2x2[] =
{
    0x05,                           /* Descriptor size */
    0x0F,                           /* BOS descriptor. */
    0x2A,0x00,                      /* Length of this descriptor and all sub-descriptors */
    0x03,                           /* Number of device capability descriptors */

    /* USB 2.0 extension */
    0x07,                           /* Descriptor size */
    0x10,                           /* Device capability type descriptor */
    0x02,                           /* USB 2.0 extension capability type */
    0x1E,0x64,0x00,0x00,            /* Supported device level features: LPM support, BESL supported,
                                       Baseline BESL=400 us, Deep BESL=1000 us. */

    /* SuperSpeed device capability */
    0x0A,                           /* Descriptor size */
    0x10,                           /* Device capability type descriptor */
    0x03,                           /* SuperSpeed device capability type */
    0x00,                           /* Supported device level features: Not LTM capable.  */
    0x0E,0x00,                      /* Speeds supported by the device : SS Gen1, HS and FS */
    0x03,                           /* Functionality support */
    0x0A,                           /* U1 Device Exit latency */
    0xFF,0x07,                      /* U2 Device Exit latency */

    /* SuperSpeedPlus USB device capability for G2X2 */
    0x14,                           /* Descriptor size */
    0x10,                           /* Device capability type descriptor */
    0x0A,                           /* SuperSpeedPlus Device capability */
    0x00,                           /* Reserved */
    0x01,0x00,0x00,0x00,            /* SSAC=1, SSIC=0 */
    0x00,0x22,                      /* SSID=0, Min. RX Lane = 2, Min. Tx Lane = 2 */
    0x00,0x00,                      /* Reserved */
    0x30,0x40,0x0A,0x00,            /* SSID=0, LSE=3(Gb/s), ST=0(Symmetric Rx), LP=1(SSPlus), LSM=10 */
    0xB0,0x40,0x0A,0x00             /* SSID=0, LSE=3(Gb/s), ST=0(Symmetric Tx), LP=1(SSPlus), LSM=10 */
};

USB3_DESC_ATTRIBUTES uint8_t CyFxLangString[] =
{
    0x04,
    0x03,
    0x09,
    0x04
};

USB3_DESC_ATTRIBUTES uint8_t CyFxMfgString[] =
{
    0x12,
    0x03,
    'I',
    0x00,
    'N',
    0x00,
    'F',
    0x00,
    'I',
    0x00,
    'N',
    0x00,
    'E',
    0x00,
    'O',
    0x00,
    'N',
    0x00
};

USB3_DESC_ATTRIBUTES uint8_t CyFxProdString[] =
{
    0x18,
    0x03,
    'E',
    0x00,
    'Z',
    0x00,
    '-',
    0x00,
    'U',
    0x00,
    'S',
    0x00,
    'B',
    0x00,
    ' ',
    0x00,
    'F',
    0x00,
    'X',
    0x00,
    '2',
    0x00,
    '0',
    0x00
};

/* MS OS String Descriptor */
USB3_DESC_ATTRIBUTES uint8_t glOsString[] =
{
    0x12, /* Length. */
    0x03, /* Type - string. */
    'M', 0x00, 'S', 0x00, 'F', 0x00, 'T', 0x00, '1', 0x00, '0', 0x00, '0', 0x00, /* Signature. */
    MS_VENDOR_CODE, /* MS vendor code. */
    0x00 /* Padding. */
};

USB3_DESC_ATTRIBUTES uint8_t glOsCompatibilityId[] =
{
    /* Header */
    0x28, 0x00, 0x00, 0x00, /* length Need to be updated based on number of interfaces. */
    0x00, 0x01, /* BCD version */
    0x04, 0x00, /* Index: 4 - compatibility ID */
    0x01, /* count. Need to be updated based on number of interfaces. */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* reserved. */
    /* First Interface */
    0x00, /* Interface number */
    0x01, /* reserved: Need to be 1. */
    0x57, 0x49, 0x4E, 0x55, 0x53, 0x42, 0x00, 0x00, /* comp ID to bind the device with WinUSB.*/
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* sub-compatibility ID - NONE. */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* reserved - needs to be zero. */
};

USB3_DESC_ATTRIBUTES uint8_t glOsFeature[] =
{
    /* Header */
    0x8E, 0x00, 0x00, 0x00, /* Length. */
    0x00, 0x01, /* BCD version. 1.0 as per MS */
    0x05, 0x00, /* Index */
    0x01, 0x00, /* count. */
    /* Property section. */
    0x84, 0x00, 0x00, 0x00, /* length */
    0x01, 0x00, 0x00, 0x00, /* dwPropertyDataType: REG_DWORD_LITTLE_ENDIAN */
    0x28, 0x00, /* wPropertyNameLength: 0x30 */

    0x44, 0x00, 0x65, 0x00, 0x76, 0x00, 0x69, 0x00, 0x63, 0x00, 0x65, 0x00, 0x49, 0x00, 0x6E, 0x00,
    0x74, 0x00, 0x65, 0x00, 0x72, 0x00, 0x66, 0x00, 0x61, 0x00, 0x63, 0x00, 0x65, 0x00, 0x47, 0x00,
    0x55, 0x00, 0x49, 0x00, 0x44, 0x00, 0x00, 0x00, /* bPropertyName: DeviceInterfaceGUID */
    0x4E, 0x00, 0x00, 0x00, /* dwPropertyDataLength: 4E */

    '{', 0x00, '0', 0x00, '1', 0x00, '2', 0x00, '3', 0x00, '4', 0x00, '5', 0x00, '6', 0x00,
    '7', 0x00, '-', 0x00, '2', 0x00, 'A', 0x00, '4', 0x00, 'F', 0x00, '-', 0x00, '4', 0x00,
    '9', 0x00, 'E', 0x00, 'E', 0x00, '-', 0x00, '8', 0x00, 'D', 0x00, 'D', 0x00, '3', 0x00,
    '-', 0x00, 'F', 0x00, 'A', 0x00, 'D', 0x00, 'E', 0x00, 'A', 0x00, '3', 0x00, '7', 0x00,
    '7', 0x00, '2', 0x00, '3', 0x00, '4', 0x00, 'A', 0x00, '}', 0x00, 0x00, 0x00
        /* bPropertyData: {01234567-2A4F-49EE-8DD3-FADEA377234A} */
};

/*******************************************************************************
 * Function name: CyApp_RegisterUsbDescriptors
 ****************************************************************************//**
 *
 * Register the USB descriptors with the USBD stack.
 *
 * \param pAppCtxt
 * Application layer context pointer.
 *
 * \param usbSpeed
 * Active USB connection speed. Used to switch descriptors where needed.
 *
 ********************************************************************************/
void CyApp_RegisterUsbDescriptors (cy_stc_usb_app_ctxt_t *pAppCtxt, cy_en_usb_speed_t usbSpeed)
{
    if ((pAppCtxt != NULL) && (pAppCtxt->pUsbdCtxt != NULL)) {
        Cy_USBD_SetDscr(pAppCtxt->pUsbdCtxt, CY_USB_SET_HS_DEVICE_DSCR, 0, (uint8_t *)CyFxUSB20DeviceDscr);
        Cy_USBD_SetDscr(pAppCtxt->pUsbdCtxt, CY_USB_SET_SS_DEVICE_DSCR, 0, (uint8_t *)CyFxUSB30DeviceDscr);
        Cy_USBD_SetDscr(pAppCtxt->pUsbdCtxt, CY_USB_SET_FS_CONFIG_DSCR, 0, (uint8_t *)CyFxUSBFSConfigDscr);
        Cy_USBD_SetDscr(pAppCtxt->pUsbdCtxt, CY_USB_SET_HS_CONFIG_DSCR, 0, (uint8_t *)CyFxUSBHSConfigDscr);
        Cy_USBD_SetDscr(pAppCtxt->pUsbdCtxt, CY_USB_SET_SS_CONFIG_DSCR, 0, (uint8_t *)CyFxUSBSSConfigDscr);
        Cy_USBD_SetDscr(pAppCtxt->pUsbdCtxt, CY_USB_SET_STRING_DSCR, 0, (uint8_t *)CyFxLangString);
        Cy_USBD_SetDscr(pAppCtxt->pUsbdCtxt, CY_USB_SET_STRING_DSCR, 1, (uint8_t *)CyFxMfgString);
        Cy_USBD_SetDscr(pAppCtxt->pUsbdCtxt, CY_USB_SET_STRING_DSCR, 2, (uint8_t *)CyFxProdString);
        Cy_USBD_SetDscr(pAppCtxt->pUsbdCtxt, CY_USB_SET_DEVICE_QUAL_DSCR, 0, (uint8_t *)CyFxDevQualDscr);
        Cy_USBD_SetDscr(pAppCtxt->pUsbdCtxt, CY_USB_SET_HS_BOS_DSCR, 0, (uint8_t *)CyFxBOSDscr_HS);

        switch (usbSpeed)
        {
            case CY_USBD_USB_DEV_SS_GEN2X2:
                Cy_USBD_SetDscr(pAppCtxt->pUsbdCtxt, CY_USB_SET_SS_BOS_DSCR, 0, (uint8_t *)CyFxBOSDscr_Gen2x2);
                break;

            case CY_USBD_USB_DEV_SS_GEN1X2:
                Cy_USBD_SetDscr(pAppCtxt->pUsbdCtxt, CY_USB_SET_SS_BOS_DSCR, 0, (uint8_t *)CyFxBOSDscr_Gen1x2);
                break;

            case CY_USBD_USB_DEV_SS_GEN2:
                Cy_USBD_SetDscr(pAppCtxt->pUsbdCtxt, CY_USB_SET_SS_BOS_DSCR, 0, (uint8_t *)CyFxBOSDscr_Gen2);
                break;

            default:
                Cy_USBD_SetDscr(pAppCtxt->pUsbdCtxt, CY_USB_SET_SS_BOS_DSCR, 0, (uint8_t *)CyFxBOSDscr_Gen1);
                break;
        }
    }
}

/*[]*/

