//-------------------------------------------------------------------------
// File  : wolfio_console.cpp
// Author: W.Buescher, DL4YHF
// Date  : 2005-04-10 (YYYY-MM-DD)
// Purpose:
//   Use this module to compile Stewart's WOLF
//       (Weak-signal Operation on Low Frequency)
//   into a CONSOLE APPLICATION, like the original implementation .
//   For more info on WOLF visit http://www.scgroup.com/ham/wolf.html .
//
//  Characters go to the standard output, etc .
//  For this purpose, all printf() calls etc were replaced by WIO_printf(), etc
//
//   Module code "WIO" (short for "WOLFIO") means "WOLF input / output" .
//   This will make it easier to switch to the planned user interface later.
//
//-------------------------------------------------------------------------

#include <windows.h>  // uses Sleep()
#include <stdio.h>
#include <stdarg.h>



#include "wolfio.h"  // header for WOLF input/output routines

//-------------------------------------------------------------------------
// Character-based output functions for the WOLF decoder :
//-------------------------------------------------------------------------

/***************************************************************************/
void WIO_printf(char * fmt, ... )
  /* Prints a formatted string (like printf) to the WOLF DECODER OUTPUT WINDOW.
   *  May use an internal buffer to make it thread-safe,
   *  if called from a worker thread one day !
   */
{
 va_list params;              /* Parameter-Liste fuer VA_... Macros */
 char sz255Temp[256];            /* Puffer fuer formatierten String */
 va_start( params, fmt );                   /* Parameter umwandeln, */
 vsprintf( sz255Temp, fmt, params );                /* formatieren, */
 printf(sz255Temp);                                   /* ausgeben . */
} // end WIO_printf()


/***************************************************************************/
extern void WIO_exit(int status)
  /* Replacement for exit(), but gives control back to the (optional)
   *  user interface instead of terminating immediately.
   * For the console application, WIO_exit(N) does alsmost the same as exit(N)
   *  but may wait for a few seconds before closing the console window.
   */
{
  Sleep( 2000 );    // suspends execution for N milliseconds

  // ... insert additional "cleanup" code for the user interface here !

  exit( status );
} // end WIO_exit()



// EOF < wolfio_console.cpp >
