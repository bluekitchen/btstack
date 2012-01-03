/*=============================================================================
* (C) Copyright Albis Technologies Ltd 2011
*==============================================================================
*                STM32 Example Code
*==============================================================================
* File name:     vector_funcs.c
*
* Notes:         STM32 vector functions.
*============================================================================*/

/* Get a linker script symbol's value. */
#define GET_LDS_ULONG(symbol)                       ((unsigned long)(&symbol))

extern unsigned long _data_start_in_flash;
extern unsigned long _data_start_in_ram;
extern unsigned long _data_section_size;
extern unsigned long _bss_start;
extern unsigned long _bss_size;

extern void main(void);

/*=============================================================================
=============================================================================*/
void startup(void)
{
    unsigned long len, i;
    unsigned long *src, *dest;

    /* Zero out BSS. */
    dest = (void *)GET_LDS_ULONG(_bss_start);
    len = GET_LDS_ULONG(_bss_size);
    for(i = 0; i < len; i += 4)
    {
        *(dest++) = 0;
    }

    /* Copy the data segment initializers from Flash to RAM. */
    src = (void *)GET_LDS_ULONG(_data_start_in_flash);
    dest = (void *)GET_LDS_ULONG(_data_start_in_ram);
    len = GET_LDS_ULONG(_data_section_size);
    for(i = 0; i < len; i += 4)
    {
        *(dest++) = *(src++);
    }

    main();
}

/*=============================================================================
=============================================================================*/
void App_NMI_ISR(void)
{
}

/*=============================================================================
=============================================================================*/
void App_Fault_ISR(void)
{
    for(;;);
}

/*=============================================================================
=============================================================================*/
void App_BusFault_ISR(void)
{
    for(;;);
}

/*=============================================================================
=============================================================================*/
void App_UsageFault_ISR(void)
{
    for(;;);
}

/*=============================================================================
=============================================================================*/
void App_MemFault_ISR(void)
{
    for(;;);
}

/*=============================================================================
=============================================================================*/
void App_Spurious_ISR(void)
{
    for(;;);
}
