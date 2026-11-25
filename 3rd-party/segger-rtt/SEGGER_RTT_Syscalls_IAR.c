/*********************************************************************
*                   (c) SEGGER Microcontroller GmbH                  *
*                        The Embedded Experts                        *
*                           www.segger.com                           *
**********************************************************************
*                                                                    *
*        SEGGER RTT * Real Time Transfer for embedded targets        *
*                  https://github.com/SEGGERMicro/RTT                *
*                                                                    *
**********************************************************************

---------------------------END-OF-HEADER------------------------------
Purpose : Low-level functions for using printf() via RTT in IAR.
          To use RTT for printf output, include this file in your
          application and set the Library Configuration to Normal.

----------------------------------------------------------------------
*/
#ifdef __IAR_SYSTEMS_ICC__

//
// Since IAR EWARM V8 and EWRX V4, yfuns.h is considered as deprecated and LowLevelIOInterface.h
// shall be used instead. To not break any compatibility with older compiler versions, we have a
// version check in here.
//
#if ((defined __ICCARM__) && (__VER__ >= 8000000)) || ((defined __ICCRX__)  && (__VER__ >= 400))
  #include <LowLevelIOInterface.h>
#else
  #include <yfuns.h>
#endif

#include "SEGGER_RTT.h"
#pragma module_name = "?__write"

/*********************************************************************
*
*       Function prototypes
*
**********************************************************************
*/
size_t __write(int handle, const unsigned char * buffer, size_t size);

/*********************************************************************
*
*       Global functions
*
**********************************************************************
*/
/*********************************************************************
*
*       __write()
*
* Function description
*   Low-level write function.
*   Standard library subroutines will use this system routine
*   for output to all files, including stdout.
*   Write data via RTT.
*/
size_t __write(int handle, const unsigned char * buffer, size_t size) {
  (void) handle;  /* Not used, avoid warning */
  SEGGER_RTT_Write(0, (const char*)buffer, size);
  return size;
}

/*********************************************************************
*
*       __write_buffered()
*
* Function description
*   Low-level write function.
*   Standard library subroutines will use this system routine
*   for output to all files, including stdout.
*   Write data via RTT.
*/
size_t __write_buffered(int handle, const unsigned char * buffer, size_t size) {
  (void) handle;  /* Not used, avoid warning */
  SEGGER_RTT_Write(0, (const char*)buffer, size);
  return size;
}

#endif
/****** End Of File *************************************************/
