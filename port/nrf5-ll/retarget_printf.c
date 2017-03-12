
#include <stdint.h>
#include "uart.h"

// retarget printf
void tiny_putc(unsigned c);

// must be before stdio.h include
int putchar(int c){
	tiny_putc(c);
	return 1;
}

#include <stdio.h>

void tiny_putc(unsigned c){
	uart_tx(c);
}
void tiny_puts(const char *s){
	uart_tx_str((char*)s);
}

// from http://forum.43oh.com/topic/1289-tiny-printf-c-version/
#include "stdarg.h"

int puts(const char * s){
	tiny_puts(s);
	uart_tx('\n');
	return 1;
}

static const unsigned long dv[] = {
//  4294967296      // 32 bit unsigned max
    1000000000,     // +0
     100000000,     // +1
      10000000,     // +2
       1000000,     // +3
        100000,     // +4
//       65535      // 16 bit unsigned max     
         10000,     // +5
          1000,     // +6
           100,     // +7
            10,     // +8
             1,     // +9
};

static void xtoa(unsigned long x, const unsigned long *dp)
{
    char c;
    unsigned long d;
    if(x) {
        while(x < *dp) ++dp;
        do {
            d = *dp++;
            c = '0';
            while(x >= d) ++c, x -= d;
            tiny_putc(c);
        } while(!(d & 1));
    } else
        tiny_putc('0');
}

static void puth(unsigned n)
{
    static const char hex[16] = { '0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};
    tiny_putc(hex[n & 15]);
}
 
int printf(const char * format, ...){
    va_list argptr;
    va_start(argptr, format);
    vprintf(format, argptr);
    va_end(argptr);
}

int vprintf(const char * format, va_list a){
	int counter=0;
    char c;
    int i;
    long n;
    while(1) {
    	c = *format++;
    	if(c == 0) break;
        if(c == '%') {
        	// skip formatting
        	while (*format >= '0' && *format <= '9') format++;
            switch(c = *format++) {
                case 's':                       // String
                    tiny_puts(va_arg(a, char*));
                    break;
                case 'c':                       // Char
                    tiny_putc(va_arg(a, int));
                    break;
                case 'i':                       // 16 bit Integer
                case 'u':                       // 16 bit Unsigned
                    i = va_arg(a, int);
                    if(c == 'i' && i < 0) i = -i, tiny_putc('-');
                    xtoa((unsigned)i, dv + 5);
                    break;
                case 'l':                       // 32 bit Long
                case 'n':                       // 32 bit uNsigned loNg
                    n = va_arg(a, long);
                    if(c == 'l' &&  n < 0) n = -n, tiny_putc('-');
                    xtoa((unsigned long)n, dv);
                    break;
                case 'x':                       // 16 bit heXadecimal
                case 'X':                       // 16 bit heXadecimal
                    i = va_arg(a, int);
                    // puth(i >> 12);
                    // puth(i >> 8);
                    puth(i >> 4);
                    puth(i);
                    break;
                case 0:
                	return counter;
                default:
                	goto bad_fmt;
            }
        } else
bad_fmt:
	    	tiny_putc(c);
    }
    return counter;
}
