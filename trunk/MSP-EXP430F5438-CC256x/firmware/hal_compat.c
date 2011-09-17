/**
 * various functions to deal with flaws and portability issues
 *
 * @author Matthias Ringwald
 */

#include "hal_compat.h"
#include <msp430x54x.h>

// __delay_cycles is limited
void waitAboutOneSecond(void){
    int i;
    for (i=0;i<1000;i++) __delay_cycles(16000);
}

// access far text for MSP430X platform
#if defined(__GNUC__) && (__MSP430X__ > 0)

uint8_t FlashReadByte (uint32_t addr){
    
    uint8_t result;
    uint32_t register sr, flash;
    
    __asm__ __volatile__ (
                          "mov    r2  , %1   \n"
                          "bic    %3  , r2   \n"
                          "nop               \n"
                          "movx.a %4  , %2   \n"
                          "movx.b @%2, %0    \n"
                          "mov    %1 , r2    \n"
                          :"=X"(result),"=r"(sr),"=r"(flash)
                          :"i"(GIE),"m"(addr));
    
    return result;
}

// argument order matches memcpy
void FlashReadBlock(uint8_t *buffer, uint32_t addr,  uint16_t len){
    while (len){
        *buffer++ = FlashReadByte(addr++);
        len--;
    }
}

#endif
