# License Overview (plain-language summary)

> **This document is informational only.** It is a plain-language guide to help
> you understand how the licensing of this project fits together. **It creates no
> rights, terms, or obligations of its own, and it does not modify, replace, or
> override the actual license files.** The governing terms are only in those
> files:
> - `TAPR_LICENSE.TXT`
> - `licenses/INFINEON_LICENSE.txt`
> - `licenses/APACHE-LICENSE-2.0.txt`
> - `THIRD-PARTY-NOTICES.txt`
> - the license header in each individual source file
>
> If anything in this summary appears to conflict with those files, **those files
> control.** Please read them.

---

## What's in this repository

This repository holds the **source code and build inputs** for the Quad-PSWS-FPGA
receiver's EZ-USB FX10 firmware. When you build it, the result is a **compiled
firmware binary** that runs on the Infineon FX10 on the Quad receiver board.

The material here comes in a few different layers, and each layer has its own
license.

## The layers, in plain terms

**1. The project's own firmware source** — the `quad_*.c` / `quad_*.h` files.
These were written for this project and are provided under an open-source license
(see the header at the top of each of those files). You can read, build, and
modify them under the terms of that license.

**2. Third-party source from Infineon / Cypress and FreeRTOS** — most of the other
source files, the board-support package (`bsps/`), the linker scripts, and the
libraries pulled in at build time. These are **not** ours; they come from their
respective owners under their own open-source licenses (mostly the **Apache
License 2.0**, and the **MIT License** for FreeRTOS). Each such file carries its
own license header, and they are listed in `THIRD-PARTY-NOTICES.txt`. Your rights
to that source code come from those licenses, not from anything TAPR says.

**3. The compiled firmware binary** — what you get after building (or a binary that
TAPR distributes). This is covered by `TAPR_LICENSE.TXT`. In plain terms, that
license lets you install and run the firmware **on the Infineon FX10 hardware it
was written for**, but it does **not** grant you the right to run those binaries on
other (non-Infineon) hardware, and it does **not** grant you the right to
reverse-engineer or disassemble them to make them do so.

## What this is, and is not

- **TAPR is not reselling Infineon's software.** The Infineon/Cypress code remains
  under its own licenses (see `licenses/INFINEON_LICENSE.txt` and `THIRD-PARTY-NOTICES.txt`).
  TAPR is distributing firmware for an amateur-radio receiver, not repackaging or
  selling Infineon's SDK.
- **The open-source parts stay open source.** Putting a binary EULA on the compiled
  firmware does not take away any rights the Apache-2.0 / MIT / project licenses
  grant you in the corresponding **source** files.
- **Read the actual licenses.** This page is a map, not the territory. Before you
  redistribute, modify, or build products on top of this, read
  `TAPR_LICENSE.TXT`, `licenses/INFINEON_LICENSE.txt`,
  `licenses/APACHE-LICENSE-2.0.txt`, and `THIRD-PARTY-NOTICES.txt`.
