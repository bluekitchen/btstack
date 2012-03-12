
/*------------------------------*/
/* TRACE32 Terminal Function    */
/*------------------------------*/
/*
   this terminal Function are for SingeE Access
   on Devices with Dualport Access only.

   use on the Trace32 Driver the command
   Terminal Setup:
   
     TERM.Reset
     TERM.METHODE.SingleE 
     TERM.MODE Ascii| String | RAW | HEX | VT100

   After this, start your Window Definition. 
   This can containe ONE terminal.view command with the SAME
   configuration addresses.

     TERM e:TermOutAddress1 e:TermInAddress1
     TERM e:TermOutAddress2 e:TermInAddress2
     .....


   If you use the Enable functionality, then you can 
   enable (or disable) the T32-Terimanl Driver on the fly 
   by the command

     Data.Set e:TermOutAddress+2 %Byte -1      ; for enable
   or
     Data.Set e:TermOutAddress+2 %Byte 0       ; for disable

 note
   if the "t32_term_pen_port" is located in the zero-section, then the 
   terminal is automatically disabled by default.


   In the example, T32OutAddress means the Out-Byte of the Applycation view
          and the  T32InAddress means the In-Byte of the Applycation view    
          

*/
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


#include "T32_Term.h"



/*----------------------------------------------------------------------------*/
/* Put a character to T32-Terminal                                            */
/*----------------------------------------------------------------------------*/
/*
  Abstract  
            this funktion puts a character to the 3 (or 2) byte port 
            the port is a 3 cell memory part.

            struct(3 bytes, [0] unsigned char put (unsigned 8 bits),
                            [1] unsigned char get (unsigned 8 bits),
                            [2] unsigned char enable (unsigned 8 bits))

            the last byte is optional and controller by the 
            - T32_term_enable_byte in the T32_Term.h file.
            if this is >0, the the byte is used.

            The enable byte must be set to >0, otherwise, the terminal
            functions are disabled in the applycation. 
    --------------------------------------------------------------------------
            The Put Funtion first checks the Enable Byte. If it is 0x0,
            the the function is returned.

            If the Terminal is enabled, the Put Function checks the put Byte
            if no Terminal is opened in the Trace32, then the last Character 
            is not readed and the Put Function will polling up to n parts.
            n = t32_term_polling_nr defined in the T32_Term.h file. 

    --------------------------------------------------------------------------

  Parameter
            -1-
            struct t32_term_typedef *Address

            pointer to the Port-Structure. It is defined in the 
            T32_Term.h file. The selectable Port-Channel is for
            Multi-Terminal mode. 
            Normaly use -> t32_termport <- for this paramter always.

            -2-
            unsigned char 
            
            8 Bit Value for transmit to the Terminal. 
            Don't send a 0x00 value, it will not transmit.

            
            the terminal can interprete a subset of VT100 syntax.

 return
            none
            
    --------------------------------------------------------------------------

Example:

    include "T32_Term.h"

    T32_Term_Put(t32_termport,Character);

------------------------------------------------------------------------------*/

void T32_Term_Put(t32_term_typedef *port, char uc_value)
{
unsigned int polling_loop_ctr;


#if t32_term_enable_byte > 0                
    if (port->enable == 0)
        return;
#endif

    polling_loop_ctr = t32_term_polling_nr;
    while ((port->put!=0) && (polling_loop_ctr !=0)) {
        polling_loop_ctr--;
    }

    if (port->put !=0)
        return;

    port->put = (unsigned char)uc_value; 

    return;



}


/*----------------------------------------------------------------------------*/
/* Get a character from T32-Terminal                                          */
/*----------------------------------------------------------------------------*/
/*
  Abstract  
            this funktion read's a character from the 3 (or 2) byte port 
            the port is a 3 cell memory part.

            struct(3 bytes, [0] unsigned char put (unsigned 8 bits),
                            [1] unsigned char get (unsigned 8 bits),
                            [2] unsigned char enable (unsigned 8 bits))

            the last byte is optional and controller by the 
            - T32_term_enable_byte in the T32_Term.h file.
            if this is >0, the the byte is used.

            The enable byte must be set to > 0, otherwise, the terminal
            functions are disabled in the applycation. 
    --------------------------------------------------------------------------
            The Get Funtion first checks the Enable Byte. If it is 0x0,
            the the function is returned with 0x00.

            If the Terminal is enabled, the Get Function reads the Value from
            get Byte and then writes a 0x00 to the get byte for signaling to 
            Trace32, that the charcater is taked. 

    --------------------------------------------------------------------------

  Parameter
            -1-
            struct t32_term_typedef *Address
            
            pointer to the Port-Structure. It is defined in the 
            T32_Term.h file. The selectable Port-Channel is for
            Multi-Terminal mode. 
            Normaly use -> t32_termport <- for this paramter always.


 return     unsigned char

            - 0x00 for no character is present
            > 0x00 as Terminal Value.
            
    --------------------------------------------------------------------------

Example:

    include "T32_Term.h"

    unsigned char uc_terminal_char;
    
    uc_terminal_char = T32_Term_Get(t32_termport);

------------------------------------------------------------------------------*/

unsigned char T32_Term_Get(t32_term_typedef *port)
{
    unsigned char uc_val;

#if t32_term_enable_byte > 0                
    if (port->enable == 0)
        return (unsigned char)0x0;
#endif

    uc_val = (unsigned char)port->get;      /* read port         */
    if (uc_val > 0)
        port->get   =   0;                  /* write handshake   */

    return uc_val;
}



/*----------------------------------------------------------------------------*/
/* RX-Status T32-Terminal                                                     */
/*----------------------------------------------------------------------------*/
/*
  Abstract  see Put and Get Function
    --------------------------------------------------------------------------

  Parameter
            -1-
            struct t32_term_typedef *Address
            
            pointer to the Port-Structure. It is defined in the 
            T32_Term.h file. At this time, only one Terminal can
            be used in the Trace32. The selectable Port-Channel is for
            future. 
            Normaly use -> t32_termport <- for this paramter always.


 return     unsigned char

            - 0x00 for no character is present
              0xFF a Character is ready for get it
            
    --------------------------------------------------------------------------

Example:

    include "T32_Term.h"

    unsigned char uc_terminal_status;
    
    uc_terminal_status = T32_Term_RXStatus(t32_termport);

------------------------------------------------------------------------------*/

unsigned char T32_Term_RXStatus(t32_term_typedef *port)
{
    unsigned char uc_val;

#if t32_term_enable_byte > 0                
    if (port->enable == 0)
        return (unsigned char)0x0;
#endif

    uc_val = port->get;                     /* read port         */
    if (uc_val > 0)
        return (unsigned char)0x0;
    else
        return (unsigned char)0xff;

}

/*----------------------------------------------------------------------------*/
/* TX-Status T32-Terminal                                                     */
/*----------------------------------------------------------------------------*/
/*
  Abstract  see Put and Get Function
    --------------------------------------------------------------------------

  Parameter
            -1-
            struct t32_term_typedef *Address
            
            pointer to the Port-Structure. It is defined in the 
            T32_Term.h file. At this time, only one Terminal can
            be used in the Trace32. The selectable Port-Channel is for
            future. 
            Normaly use -> t32_termport <- for this paramter always.


 return     unsigned char

            - 0x00 TX Buffer is empty
              0xFF TX Buffer is not ready for transmit
            
    --------------------------------------------------------------------------

Example:

    include "T32_Term.h"

    unsigned char uc_terminal_status;
    
    uc_terminal_status = T32_Term_TXStatus(t32_termport);

------------------------------------------------------------------------------*/

unsigned char T32_Term_TXStatus(t32_term_typedef *port)
{
    unsigned char uc_val;

#if t32_term_enable_byte > 0                
    if (port->enable == 0)
        return (unsigned char)0x0;
#endif

    uc_val = port->put;
    if (uc_val > 0)
        return (unsigned char)0xff;
    else
        return (unsigned char)0x00;
}

/* eof */

