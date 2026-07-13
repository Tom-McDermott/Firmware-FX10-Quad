# EZ-USB&trade; FX20: LVDS USB streaming application 1.0.1

## What's Included?

Refer to the [README.md](./README.md).

## Feature Updates

* Added support for firmware-based LVDS PHY training
* Updated data pipeline used in USB-HS connection to avoid LVDS Adapter errors
* Changed rate of data generation from FPGA to about 7.6 Gbps for compatibility with all FPGA configurations

## Defect Fixes

* Corrected link loopback datapath to achieve expected data throughput

## Supported Software and Tools

This version of the application is compatible with the following software and tools:

| Software and Tools                                       | Version |
| :---                                                     | :----:  |
| ModusToolbox&trade; software environment                 | 3.5.0   |
| CAT1A Peripheral Driver Library                          | 3.19.0  |
| USBFXStack Middleware Library                            | 1.3.2   |
| FreeRTOS&trade; for Infineon MCUs                        | 10.5.004|
| GNU Arm&reg; Embedded Compiler                           | 14.2.1  |
| Arm&reg; Compiler                                        | 6.22    |

## More information

For more information, refer to the following documents:

* [EZ-USB&trade; FX20: LVDS USB streaming application README.md](./README.md)
* [ModusToolbox Software Environment, Quick Start Guide, Documentation, and Videos](https://www.infineon.com/cms/en/design-support/tools/sdk/modustoolbox-software)
* [Infineon Technologies AG](https://www.infineon.com)

---
© 2026, Cypress Semiconductor Corporation (an Infineon company) or an affiliate of Cypress Semiconductor Corporation.
