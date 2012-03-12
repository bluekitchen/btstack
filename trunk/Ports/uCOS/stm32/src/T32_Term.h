/*------------------------------*/
/* TRACE32 Terminal Header File */
/*------------------------------*/
/*----------------------------------------------------------------------------*/
/* History                                                                    */
/*---------                                                                   */
/*--Date-+-change---------------------------------------------------+aut+Vers+*/
/* 110508: is created for cortex (e.g. target with e: access)       :akj:1.00:*/
/*-------+----------------------------------------------------------+---+----+*/
/*       :                                                          :   :    :*/
/*-------+----------------------------------------------------------+---+----+*/
/*       :                                                          :   :    :*/
/*-------+----------------------------------------------------------+---+----+*/
/*       :                                                          :   :    :*/
/*-------+----------------------------------------------------------+---+----+*/
/*       :                                                          :   :    :*/
/*-------+----------------------------------------------------------+---+----+*/
/*       :                                                          :   :    :*/
/*-------+----------------------------------------------------------+---+----+*/
/*       :                                                          :   :    :*/
/*-------+----------------------------------------------------------+---+----+*/
/*       :                                                          :   :    :*/
/*-------+----------------------------------------------------------+---+----+*/



#ifndef  __t32_term_def_h
#define  __t32_term_def_h





#define t32_term_enable_byte 1              /* 1 = terminal Enable-Byte is used; 0= not used            */
#define t32_term_polling_nr  100            /* n pollings for transmit a byte is used (min is 1)        */
#define t32_termportaddress  0x2000fff0     /* Put = 0x20083ff0, get = 0x20083ff1, enable = 0x20083ff2  */
#define t32_termportaddress2 0x2000fff3     /* Put = 0x20083ff3, get = 0x20083ff4, enable = 0x20083ff5  */


/*----------------*/
/* Define Struct  */
/*----------------*/


typedef struct
{
    volatile unsigned char put;
    volatile unsigned char get;
#if t32_term_enable_byte > 0
    volatile unsigned char enable;
#endif
} t32_term_typedef;

#define  t32_termport   (( t32_term_typedef *) t32_termportaddress)
#define  t32_termport_2 (( t32_term_typedef *) t32_termportaddress2)



/*----------------*/
/* Define Function*/
/*----------------*/

extern void             T32_Term_Put(t32_term_typedef * port, 
                                     char               uc_value       );               /* send a character from application to the host */
unsigned char T32_Term_Get(t32_term_typedef *port                      );               /* get a character from host to application      */
extern unsigned char    T32_Term_TXStatus(t32_term_typedef *port       );               /* check terminal status                         */
extern unsigned char    T32_Term_RXStatus(t32_term_typedef *port       );               /* check terminal status                         */

#endif

/* eof */

