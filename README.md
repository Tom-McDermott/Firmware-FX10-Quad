# Quad-PSWS-FPGA Receiver: EZ-USB&trade; FX10 firmware

This is the EZ-USB&trade; FX10 (`CYUSB4014-FCAXI`) firmware for the **Quad-PSWS-FPGA
receiver** — a four-channel HF SDR built around a Xilinx Artix-7 FPGA, two
ADS62P45 dual ADCs, a Si5345 clock synthesizer, and four AD8370 AGC amplifiers.
The FX10 is the USB bridge and system controller: it loads the FPGA, programs the
clock and ADCs over I2C/serial, runs LVDS/LVCMOS link training, and streams the
combined 32-bit sample bus to a USB bulk-IN endpoint.

The host controls every subsystem through a vendor-specific **EP0 command set**;
the high-rate sample data flows out **bulk endpoint EP1-IN**.

> **Heritage:** This project began as Infineon's
> [mtb-example-fx20-lvds-usb-stream](https://github.com/Infineon/mtb-example-fx20-lvds-usb-stream)
> example and reuses its USBFX stack, HBDMA plumbing, and LVDS/LVCMOS driver. It
> has been retargeted FX20&rarr;FX10 and substantially rewritten for the Quad
> receiver hardware: the Efinix-Ti180 QSPI/I2C data source is gone, replaced by a
> Xilinx slave-serial loader, a Si5345/ADS62P45 control path, and a 32-bit LVCMOS
> DDR data path with discrete-pin link training.


## Requirements

- [ModusToolbox&trade;](https://www.infineon.com/modustoolbox) v3.4 or later (built with v3.8)
- Programming language: C
- Associated hardware: the Quad-PSWS-FPGA receiver board (FX10 + Artix-7 + Si5345 + 2&times;ADS62P45 + 4&times;AD8370)


## Supported toolchains (make variable 'TOOLCHAIN')

- GNU Arm&reg; Embedded Compiler (`GCC_ARM`) — default


## Supported device

- **EZ-USB&trade; FX10 `CYUSB4014-FCAXI`** (FX3G2 die; 512 KB flash, 1024 KB buffer RAM) — the board's part.

Retargeting to another FX10/FX20/FX5 MPN is done with the **BSP Assistant**
(`bsp-assistant change_mcu <bsp-dir> <MPN>`) plus the `DEVICE ?=` default in the
*Makefile*.


## Hardware setup

The FX10 connects to the host over USB. The Artix-7, ADCs, clock, and AGC live on
the same board and are reached entirely through the FX10 — there is **no separate
FPGA control connector or FPGA I2C interface**. Key nets (FX10 `port.bit`, see the
companion `interface-contract-RESOLVED.md` for the full map):

**Table 1. Principal FX10 signals**

| Function | FX10 pin | Dir | Notes |
|----------|----------|-----|-------|
| FPGA `PROGRAM_B` | P9.6 | OUT | active-low config pulse |
| FPGA `INIT_B` | P9.7 | IN | low during config = CRC/frame error |
| FPGA `CCLK` / `DIN` | P8.6 / P8.7 | OUT | bit-banged slave-serial config |
| 32-bit LVCMOS data `D[31:0]` | P1D[15:0] + P0D[15:0] | IN | two 16-bit ports |
| DDR data clock | P1CLK + P0CLK | IN | 125.000 MHz, DDR (both edges) |
| `LinkTrain` | P4.3 / D32 | OUT | assert &rarr; FPGA drives training pattern |
| I2C SDA / SCL | P10.1 / P10.2 | — | Si5345, PCF8574s |
| Si5345 reset | P9.1 | OUT | |
| ADC serial DATA/CLK | P9.4 / P9.5 | OUT | shared to both ADS62P45 |
| ADC enable AB / CD | P9.2 / P9.3 | OUT | per-pair select |
| ADC reset | P9.0 | OUT | both devices |

> **Note:** `DONE` is not wired on this board; FPGA-load success is inferred from
> byte-count + `INIT_B` (see [FPGA configuration](#fpga-configuration)).


## Using the code example

This is a standard ModusToolbox&trade; application; build it from the command
line (or import it into the Eclipse IDE / VS Code as usual).

```
cd quad-fx10-firmware
source /opt/infineon/settings.sh        # export CY_TOOLS_PATHS for the MTB tools
make build TOOLCHAIN=GCC_ARM CY_COMPILER_GCC_ARM_DIR=/usr OBJCOPY_DIR=/usr/arm-none-eabi/bin
# -> build/APP_QUAD_FX10/Release/quad-fx10-firmware.{elf,hex}
```

Program the resulting `.hex`/`.elf` onto the FX10 with the EZ-USB&trade; FX
Control Center (USB bootloader) or a KitProg3/MiniProg over SWD.

> **Note:** This offline tree builds against a sibling `../mtb_shared` (a symlink
> to the pre-fetched SDK) rather than running `make getlibs`. The generated BSP,
> linker scripts, and `libs/` are kept locally but are `.gitignore`d — a full
> ModusToolbox checkout regenerates them.


## Operation (typical host sequence)

Everything is driven from the host over EP0. The order below brings the receiver
from power-up to a running ramp-test stream:

```
GET_VERSION                          # confirm comms (returns the version string)
FPGA_RESET                           # pulse PROGRAM_B, wait INIT_B high
FPGA_LOAD(start)…(cont)…(end)        # push top.bin (2,192,012 bytes)
FPGA_STATUS                          # verify bytes==2192012 && INIT_B high
SI5345_LOAD(preamble); sleep >300ms; SI5345_LOAD(main); SI5345_LOAD(post)
ADC_CONFIG(block, dev=both)          # register 0x00 first; both ADS62P45
ADC_TESTPAT(ramp)                    # internal digital ramp
START_STREAM                         # link-train both links, then start HBDMA
   … read 32-bit words from EP1-IN; use bit[14] to find ChA; mask bits 14/15;
     verify the 14-bit ramp is monotonic; check overrun_count == 0 …
STOP_STREAM
GET_STATUS                           # poll state / last_error at any time
```

`START_STREAM` always ACKs the control transfer, even on training failure — always
follow it with `GET_STATUS` and check `state` / `last_error` / `train_result`.


## Debugging

By default debug logs go out the **USBFS port** (`USBFS_LOGS_ENABLE=1`). Set that
macro to `0u` in the *Makefile* to route logs to **UART on SCB1 / P8.1** at
921600 baud, 8N1. The `DBG_APP_*` macros trace each subsystem (FPGA load, Si5345,
ADC, LVDS bring-up, training block-detect, streaming).


## Design and implementation


### Features

- USB 2.0 High-Speed and USB 3.2 Gen2/Gen1 (default `CY_USBD_USB_DEV_SS_GEN2X2`).
- Host-driven **EP0 vendor command set** for all board subsystems.
- **Xilinx slave-serial** FPGA loader (bit-banged, host-streamed bitstream).
- **Si5345** paged-I2C clock loader and **ADS62P45** serial ADC configuration.
- **32-bit LVCMOS DDR** ingress with discrete-pin **link training**, streamed to
  bulk EP1-IN via high-bandwidth DMA.


### USB identity

- **VID `0x04B4`**, **PID `0x00F7`** — identical at High-Speed and SuperSpeed.


### LVCMOS data interface

The receiver presents a **32-bit LVCMOS DDR** bus: two 16-bit ports
(`P1D[15:0]` + `P0D[15:0]`) clocked by `P0CLK`/`P1CLK` at **125.000 MHz on both
edges**. The FX10 PHY is **clock-slave** (it takes the bit clock from the FPGA)
and runs in **WideLink LVCMOS, 32 lanes, 2:1 gearing (DDR)** — configured in
`cy_gpif_header_lvds.h`. (This replaces the base example's differential-LVDS
148.5 MHz / 8:1 configuration.)


### Sample / data format

Per DDR beat the 32-bit bus carries **2 channels &times; 16 bits**:

```
Rising  clock edge : { ChA[15:0] , ChB[15:0] }   (P1D , P0D)
Falling clock edge : { ChC[15:0] , ChD[15:0] }   (P1D , P0D)
```

Each 16-bit channel word:

```
 bit 15 | bit 14 |            bits 13..0
  PPS   |  ChID  |   14-bit sample (2's complement, bit 13 = sign)
```

- **bit[13:0]** — 14-bit ADC sample, two's-complement.
- **bit[14]** — channel-ID: **1 only on ChA** (a frame/channel sync).
- **bit[15]** — PPS: set for one sample on all channels at the PPS edge.

The firmware moves **raw 32-bit words**; masking bits 14/15, per-channel demux,
and PPS handling are all the host's job, so the data path is format-agnostic.


### EP0 command set

All are **vendor** requests to the **device**: writes `bmRequestType = 0x40`,
reads `0xC0`. Base low-level codes reuse the RX888/SDDC numbering. See
`ep0-command-set-v0.0.1.md` for full encodings.

**Table 2. Command map (v0.0.1)**

| Code | Name | Dir | Purpose |
|-----:|------|:---:|---------|
| `0xAA` | START_STREAM | out | Link-train, then start HBDMA streaming |
| `0xAB` | STOP_STREAM | out | Stop streaming, quiesce the data path |
| `0xAC` | PING | in | 4-byte liveness heartbeat |
| `0xAD` | GPIO_WRITE | out | Set a pin: `wIndex=(port<<8)\|pin`, `wValue` bit0=level |
| `0xB3` | GPIO_READ | in | Read a pin level |
| `0xAE` | I2C_WRITE | out | `wValue`=7-bit addr, `wIndex=(addrWidth<<8)\|reg` |
| `0xAF` | I2C_READ | in | I2C read (register-based) |
| `0xB1` | RESET | out | ACK, wait `wValue+1` ms, `NVIC_SystemReset()` |
| `0xC0` | SI5345_LOAD | out | Paged register records; `wValue` 0=pre/1=main/2=post |
| `0xC1` | SI5345_DELAY | out | Optional firmware delay (host sleep preferred) |
| `0xD0` | FPGA_RESET | out | Pulse `PROGRAM_B`, wait `INIT_B` high |
| `0xD1` | FPGA_LOAD | out | Slave-serial config bytes; `wValue`=phase |
| `0xD2` | FPGA_STATUS | in | 8 B: INIT_B + bytes-loaded + loader state |
| `0xE1` | ADC_CONFIG | out | `{addr,data}` records; `wValue` 0=both/1=AB/2=CD |
| `0xE2` | ADC_TESTPAT | out | Test pattern: `wValue` 0=off / 1=ramp |
| `0xF1` | AGC_SELECT | out | Select AD8370(s) *(v0.0.2)* |
| `0xF2` | AGC_GAIN | out | Set AD8370 gain *(v0.0.2)* |
| `0xFE` | GET_STATUS | in | Structured status/error block (below) |
| `0xFF` | GET_VERSION | in | Firmware/build string |

Debug commands inherited from the base example (`0xA0`/`0xA1` mem r/w, `0xB7`
bootloader, `0xB8` data-test, `0xE0` reset, `0xF0` MS-OS, `0xF6` device-speed) are
retained as bring-up aids — which is why the ADC/AGC codes avoid `0xE0`/`0xF0`.


### GET_STATUS structure

`GET_STATUS` (0xFE) returns a packed 24-byte, little-endian block:

```c
typedef struct __attribute__((packed)) {
    uint32_t magic;          /* 0x51554144 = "QUAD" */
    uint16_t fw_version;     /* BCD, 0x0001 = v0.0.1 */
    uint8_t  state;          /* 0 IDLE, 1 FPGA_LOADED, 2 TRAINED, 3 STREAMING, 0xFF ERROR */
    uint8_t  last_error;     /* 0 NONE, 1 FPGA_INITB_LOW, 2 FPGA_COUNT_BAD, 3 TRAIN_FAIL, 4 I2C_NAK */
    uint8_t  fpga_initb;     /* 1 = INIT_B high (no config error) */
    uint8_t  link_trained;   /* 1 = both links trained */
    uint8_t  adc_configured; /* 1 = ADC block applied */
    uint8_t  streaming;      /* 1 = HBDMA streaming active */
    uint32_t fpga_bytes;     /* bytes shifted to FPGA (expect 2192012) */
    uint32_t train_result;   /* bit0 = LNK0 locked, bit1 = LNK1 locked */
    uint32_t overrun_count;  /* DMA overruns since START_STREAM (want 0) */
} quad_status_t;
```


### Link-training protocol

Link training is deferred out of boot into `START_STREAM`, because it needs the
FPGA loaded and the 125 MHz clock running first:

1. FX10 asserts **`LinkTrain`** (discrete `P4.3`). The FPGA responds by driving
   the repeating 32-bit word **`0x784B5A12`** on all data lanes and used control
   inputs. (This is the FX10 LINK-training pattern `0x125A4B78` byte-reversed.)
2. The FX10 brings up the LVCMOS PHY and runs PHY + LINK training, declaring
   success when **both links (LNK0 + LNK1)** report block-detect (1 s timeout &rarr;
   `QUAD_ERR_TRAIN_FAIL`).
3. FX10 **deasserts `LinkTrain`**; the FPGA reverts to data mode.
4. The LVDS-ingress &rarr; EP1-IN HBDMA channel is enabled and data flows.

There is **no FPGA I2C handshake** in this flow (the base example's I2C
training coordination was removed — our FPGA has no I2C control interface).


### Streaming data path

- EP1-IN is a **bulk** endpoint (1024-byte max packet; 16-packet burst on
  USB 3.2). The device receives on **LVDS Adapter 0 / Socket 0**.
- **Three HBDMA buffers of 64512 (0xFC00) bytes** hold data en route to USB.
- On USB 3.x an event-driven HBDMA channel forwards data with no per-buffer
  firmware involvement; on USB 2.x, DataWire callbacks drive the transfers.


### Application workflow

**Initialization (boot).** Data structures, clocks, HBDMA manager, and the USBD /
USB driver layers are initialized; descriptors and event callbacks are
registered; the device connects. The LVDS interface is **not** touched at boot.

**Enumeration.** The host reads descriptors and issues `SET_CONFIGURATION`; the
app enables EP1-IN and creates its HBDMA channel.

**Command phase.** The host loads the FPGA, programs the Si5345 and ADCs, and
issues `START_STREAM` — which performs link training and starts the data path as
described above. `STOP_STREAM` tears the channel and PHY down.


## Compile-time configurations

Set in the *Makefile* `DEFINES` (or via `make` CLI). Quad defaults:

**Table 3. Key macros**

| Macro | Quad default | Meaning |
|-------|--------------|---------|
| `USB_CONN_TYPE` | `CY_USBD_USB_DEV_SS_GEN2X2` | USB speed (Gen2x2 … HS/FS) |
| `FPGA_CONFIG_EN` | `0` | Efinix QSPI passive-serial config — **off** (we use host slave-serial) |
| `CUSTOM_TRAIN_ENABLE` | `0` | Firmware custom PHY training — off (hardware training) |
| `LVDS_LB_EN` | `0` | Internal link loopback — off (external FPGA source) |
| `PORT0_THREAD_INTLV` | `0` | Port-0 thread interleave |
| `USBFS_LOGS_ENABLE` | `1` | `1` logs over USBFS, `0` over UART (SCB1/P8.1) |

`make build BLENABLE=no` produces a standalone (non-bootloader) binary for SWD
programming.


## FPGA configuration

The Artix-7 is configured in **slave-serial** mode by the FX10, which streams the
host-provided bitstream out bit-banged `CCLK`/`DIN`:

1. Host issues `FPGA_RESET` — FX10 pulses `PROGRAM_B` and waits for `INIT_B` high.
2. Host streams **`top.bin` (2,192,012 bytes)** via `FPGA_LOAD` chunks (`wValue`
   = phase: 0 start / 1 cont / 2 end; the entire data stage is raw bitstream — no
   in-band header). The FX10 shifts each byte MSB-first.
3. On the END phase the FX10 clocks the startup cycles and verifies **byte-count
   == 2,192,012 AND `INIT_B` still high**. `DONE` is not wired, so this is the
   success proxy.

`FPGA_STATUS` (0xD2) returns `INIT_B`, bytes-loaded, and loader state for polling.


## Application files

**Table 4. Application file description**

| File | Description |
|------|-------------|
| *main.c* | Device init, ISRs, LVDS bring-up/teardown, boot flow |
| *quad_vendor.c/.h* | EP0 vendor-command dispatch and the `GET_STATUS` block |
| *quad_fpga.c/.h* | Xilinx slave-serial FPGA loader (bit-banged) |
| *quad_si5345.c/.h* | Si5345 paged-I2C clock loader |
| *quad_adc.c/.h* | ADS62P45 serial config + ramp test pattern |
| *quad_stream.c/.h* | START/STOP_STREAM: LinkTrain (P4.3) + PHY training + HBDMA |
| *cy_usb_app.c/.h* | Data-transfer logic, HBDMA channel + endpoint DMA setup |
| *cy_usb_descriptors.c* | USB descriptors (VID/PID, endpoints) |
| *cy_gpif_header_lvds.h* | LVCMOS/GPIF PHY config (32-bit DDR, clock-slave) |
| *cy_usb_i2c.c/.h* | SCB0 I2C master helpers |
| *cm0_code.c* | CM0+ initialization code |
| *app_version.h* | Version information |
| *FreeRTOSConfig.h* | FreeRTOS kernel configuration |
| *Makefile* | GNU make build script |

> **Legacy (unused, retained from the base example):** *cy_usb_qspi.c/.h* (SMIF /
> QSPI FPGA config for the Efinix path) and *cy_fpga_ctrl_regs.h* (Efinix I2C
> control-register definitions). These are not exercised by the Quad firmware and
> can be removed in a future cleanup.


## Related documentation

Project design docs (maintained alongside the firmware):

- *interface-contract-RESOLVED.md* — authoritative FX10 &harr; Artix-7 pin/signal
  map, data format, and link-training protocol.
- *ep0-command-set-v0.0.1.md* — full EP0 command encodings.
- *PLAN-streaming-and-link-training.md* — streaming/link-training design + status.
- *BRINGUP-PLAN-FOR-TOM-streaming.md* — bench bring-up runbook.

Infineon references: [EZ-USB&trade; FX20/FX10 SDK](https://www.infineon.com/fx20),
[usbfxstack](https://github.com/Infineon/usbfxstack),
[mtb-pdl-cat1](https://github.com/Infineon/mtb-pdl-cat1).


## Status

v0.0.1 — the full EP0 command set and the streaming/link-training path are
implemented and build clean, and are **compile-verified**. Hardware bring-up
(especially link training and the LVCMOS DDR PHY geometry) is pending; see
`BRINGUP-PLAN-FOR-TOM-streaming.md`.
