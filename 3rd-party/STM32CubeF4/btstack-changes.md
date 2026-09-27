# BTstack changes to STM32CubeF4 V1.28.0

The baseline is the installed STM32Cube_FW_F4_V1.28.0 package. Its
`Package_license.md` is preserved as [package-license.md](package-license.md).
The top-level upstream Git release is `9e7d2aa86197059fc29cf89931d46d0ab64e6081`;
its HAL and CMSIS device gitlinks are recorded in the third-party inventory.
The installed package also contains the PDM files omitted from the corresponding
Git tree.

[`btstack-changes.patch`](btstack-changes.patch) records the content differences
in the two changed package files, with CRLF normalized to LF on both sides:

- `Drivers/BSP/STM32F4-Discovery/stm32f4_discovery_audio.c`: adapts audio clock
  selection, DMA and mono PDM input for BTstack and reports the sink frequency.
- `Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_uart.c`: avoids clearing the
  UART overrun flag immediately before enabling DMA receive because doing so
  can consume an already received byte.

## Comparison scope

On 2026-11-18, the 601 bundled files other than these tracking files were
compared against the installed package. Of these, 178 match byte for byte,
420 differ only in CRLF versus LF line endings, and the two files above have
content differences. Both PDM binary libraries are byte-identical to the
installed package; its PDM header and license differ only in line endings.

The top-level `LICENSE.md` is a separate package summary and does not
correspond to a package path; it is not represented in the patch. The package
contains additional files that BTstack does not bundle.

The package license lists CMSIS as Apache-2.0, HAL and BSP as BSD-3-Clause, and
STM32 Audio under SLA0047 (Annex 2). The older local `LICENSE.md` lists a
different audio license; use the package license and the PDM component notice
for this V1.28.0 copy.

## Verification and maintenance

To check the patch, make a temporary copy of the two files listed above from
V1.28.0, preserving their paths relative to the package root. Convert CRLF to
LF in those copies, apply the patch with `git apply --check` and `git apply`,
and compare each result with the corresponding BTstack file after the same
line-ending normalization. The patch does not describe line-ending differences,
the separate license summary, or files omitted from BTstack.

When modifying package files, compare them with the same installed release,
update this record and regenerate the patch. When changing the package version,
review and port the local changes and update the package license, component
inventory, source revisions, and comparison together.
