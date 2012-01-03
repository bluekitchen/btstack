/*=============================================================================
* (C) Copyright Albis Technologies Ltd 2011
*==============================================================================
*                STM32 Example Code
*==============================================================================
* File name:     linker.lds
*
* Notes:         GNU linker script STM32F10x flash images.
*
*                CPU-Type      Flash size   SRAM size
*                                 (kByte)     (kByte)
*                 -------------------------------------
*                 STM32F101C4       16.          4.
*                 STM32F101C6       32.          6.
*                 STM32F101C8       64.         10.
*                 STM32F101CB      128.         16.
*                 STM32F101R4       16.          4.
*                 STM32F101R6       32.          6.
*                 STM32F101R8       64.         10.
*                 STM32F101RB      128.         16.
*                 STM32F101RC      256.         32.
*                 STM32F101RD      384.         48.
*                 STM32F101RE      512.         48.
*                 STM32F101T4       16.          4.
*                 STM32F101T6       32.          6.
*                 STM32F101T8       64.         10.
*                 STM32F101V8       64.         10.
*                 STM32F101VB      128.         16.
*                 STM32F101VC      256.         32.
*                 STM32F101VD      384.         48.
*                 STM32F101VE      512.         48.
*                 STM32F101ZC      256.         32.
*                 STM32F101ZD      384.         48.
*                 STM32F101ZE      512.         48.
*                 -------------------------------------
*                 STM32F102C4       16.          4.
*                 STM32F102C6       32.          6.
*                 STM32F102C8       64.         10.
*                 STM32F102CB      128.         16.
*                 STM32F102R4       16.          4.
*                 STM32F102R6       32.          6.
*                 STM32F102R8       64.         10.
*                 STM32F102RB      128.         16.
*                 -------------------------------------
*                 STM32F103C4       16.          6.
*                 STM32F103C6       32.         10.
*                 STM32F103C8       64.         20.
*                 STM32F103CB      128.         20.
*                 STM32F103R4       16.          6.
*                 STM32F103R6       32.         10.
*                 STM32F103R8       64.         20.
*                 STM32F103RB      128.         20.
*                 STM32F103RC      256.         48.
*                 STM32F103RD      284.         64.
*                 STM32F103RE      512.         64.
*                 STM32F103T4       16.          6.
*                 STM32F103T6       32.         10.
*                 STM32F103T8       64.         20.
*                 STM32F103V8       64.         20.
*                 STM32F103VB      128.         20.
*                 STM32F103VC      256.         48.
*                 STM32F103VD      384.         64.
*                 STM32F103VE      512.         64.
*                 STM32F103ZC      256.         48.
*                 STM32F103ZD      384.         64.
*                 STM32F103ZE      512.         64.
*                 -------------------------------------
*                 STM32F105R8       64.         20.
*                 STM32F105RB      128.         32.
*                 STM32F105RC      256.         64.
*                 STM32F105V8       64.         20.
*                 STM32F105VB      128.         32.
*                 STM32F105VC      256.         64.
*                 -------------------------------------
*                 STM32F107RB      128.         48.
*                 STM32F107RC      256.         64.
*                 STM32F107VB      128.         48.
*                 STM32F107VC      256.         64.
*
*                 Flash base address : 0x08000000
*                 SRAM base address  : 0x20000000
*============================================================================*/

OUTPUT_ARCH(arm)
OUTPUT_FORMAT("elf32-littlearm")
ENTRY(vectorTableBegin)

SECTIONS
{
    /* STM32 internal Flash EEPROM is remapped to 0x0 */
    . = 0x0;

    . = ALIGN(4);
    .text : { *(.text) }

    . = ALIGN(4);
    .rodata : { *(.rodata) }

    /* .data section in Flash */
    . = ALIGN(4);
    _data_start_in_flash = .;

    /* Check if Flash EEPROM full */
    ASSERT(. < 0x40000, "Error: STM32F107VC internal Flash EERPROM full!")

    /* STM32 internal SRAM start address */
    . = 0x20000000;

    /* .data section in RAM */
    . = ALIGN(4);
    _data_start_in_ram = .;
    .data : AT (_data_start_in_flash) { *(.data) }
    _data_end_in_ram = .;

    _data_section_size = _data_end_in_ram - _data_start_in_ram;

    . = ALIGN(4);
    _bss_start = .;
    .bss : { *(.bss) }
    . = ALIGN(4);
    _bss_end = .;

    _bss_size = _bss_end - _bss_start;

    /* Allocate CM3 main stack */
    . = ALIGN(4);
    . += 0x200;
    _cm3_main_stack = .;

    /* Check if RAM full */
    ASSERT(. < 0x2000fff0, "Error: STM32F107VC internal RAM full!")
}
