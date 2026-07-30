THIRD-PARTY-NOTICES.txt


 --- From Local AI Search ---


I’m scanning the repository for SPDX headers and non-Infineon/Cypress copyright notices so I can list the matching source files precisely.

Searched for text `SPDX|Copyright|copyright` (`**/*.{c,h,cpp,cc,hpp,md,txt,json,mak,mk,ld,sct,py,sh,yml,yaml}`), 169 results

The scan is nearly complete; I’m trimming it to the actual source-like files and classifying each one by whether it has an SPDX header or a non-Infineon/Cypress copyright notice.


I found 63 qualifying source-like files in the workspace. The table below lists each one and the reason it matched.

### Top-level and application sources

| File | Match |
|---|---|
| app_version.h | SPDX header |
| cm0_code.c | SPDX header |
| cy_fpga_ctrl_regs.h | SPDX header |
| cy_usb_app.c | SPDX header |
| cy_usb_app.h | SPDX header |
| cy_usb_descriptors.c | SPDX header |
| cy_usb_i2c.c | SPDX header |
| cy_usb_i2c.h | SPDX header |
| cy_usb_qspi.c | SPDX header |
| cy_usb_qspi.h | SPDX header |
| main.c | SPDX header |
| FreeRTOSConfig.h | Amazon copyright notice |

### Linker scripts

| File | Match |
|---|---|
| fx_cm0plus.ld | SPDX header |
| fx_cm0plus.sct | SPDX header |
| fx_cm0plus_loadable.ld | SPDX header |
| fx_cm0plus_loadable.sct | SPDX header |
| fx_cm4.ld | SPDX header |
| fx_cm4_dual.sct | SPDX header |
| fx_cm4_loadable.ld | SPDX header |
| fx_cm4_loadable.sct | SPDX header |
| fx_cm0plus.ld | SPDX header |
| fx_cm0plus.sct | SPDX header |
| fx_cm0plus_loadable.ld | SPDX header |
| fx_cm0plus_loadable.sct | SPDX header |
| fx_cm4.ld | SPDX header |
| fx_cm4_dual.sct | SPDX header |
| fx_cm4_loadable.ld | SPDX header |
| fx_cm4_loadable.sct | SPDX header |
| fx_cm0plus.ld | SPDX header |
| fx_cm0plus.sct | SPDX header |
| fx_cm0plus_loadable.ld | SPDX header |
| fx_cm0plus_loadable.sct | SPDX header |
| fx_cm4.ld | SPDX header |
| fx_cm4_dual.sct | SPDX header |
| fx_cm4_loadable.ld | SPDX header |
| fx_cm4_loadable.sct | SPDX header |

### BSP and generated source files

| File | Match |
|---|---|
| bsp.mk | SPDX header |
| system_psoc6_cm0plus.c | SPDX header |
| linker.sct | SPDX header |
| linker.ld | SPDX header |
| system_psoc6_cm4.c | SPDX header |
| linker.sct | SPDX header |
| linker.mk | SPDX header |
| linker.ld | SPDX header |
| cycfg.c | SPDX header |
| cycfg.h | SPDX header |
| cycfg_clocks.c | SPDX header |
| cycfg_clocks.h | SPDX header |
| cycfg_notices.h | SPDX header |
| cycfg_peripherals.c | SPDX header |
| cycfg_peripherals.h | SPDX header |
| cycfg_pins.c | SPDX header |
| cycfg_pins.h | SPDX header |
| cycfg_qspi_memslot.c | SPDX header |
| cycfg_qspi_memslot.h | SPDX header |
| cycfg_routing.h | SPDX header |
| cycfg_system.c | SPDX header |
| cycfg_system.h | SPDX header |
| cybsp.c | SPDX header |
| cybsp.h | SPDX header |
| cybsp_doc.h | SPDX header |
| cybsp_hw_config.h | SPDX header |
| cybsp_types.h | SPDX header |
| system_psoc6.h | SPDX header |



Asked AI  
List all source files that contain Copyright Infineon or contain Copyright Cypress that lack any open-source license text (such as SPDX, Apache, GNU, or other).

3:13 PM
I’m scanning the source tree for files that mention Infineon or Cypress copyright and then checking whether they have any obvious open-source license text.

I checked the repository for source-like files that mention “Copyright Infineon” or “Copyright Cypress” but do not contain any obvious open-source license marker such as SPDX, Apache, GNU, BSD, MIT, GPL/LGPL, or similar license text.

Result: 0 files matched that criterion.
