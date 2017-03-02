/***************************************************************************/
/* WolfWaveThd.cpp =  Sample-Stream Input Thread for the SERIAL interface.    */
/*   - Receives continuous input from the serial interface                 */
/*       and places the sound samples in two alternating buffers.          */
/*   - based on "SndInThd" which takes audio samples from the soundcard.   */
/*   - by DL4YHF,  October 2002                                            */
/*   - VCL dependencies removed in May 2002 to keep the code compact (!)   */
/*                                                                         */
/*  The audio data are produced by a simple hardware with a PIC micro-     */
/*  controller, with the "Serial A/D"-firmware.                            */
/*  The data format is described in the firmware project directory,        */
/*   look for the file    SerialAD\SerPicAD.txt (written by W.Buescher) .  */
/***************************************************************************/

//---------------------------------------------------------------------------
#include <windows.h>   // dont use the VCL here !!
#include <stdio.h>
#include <stdlib.h>


#pragma hdrstop


#define _I_AM_SER_IN_THREAD_ 1  // for "single-source" variables of this module
#include "SerInThd.h"

#define SER_WIN_USE_OVERLAPPED_IO  1

#ifdef __WIN32__
 #pragma warn -8071 // "conversion may lose significant digits" ... no thanks !
 #pragma warn -8004 // "iBreakPoint" assigned a value that is never used .. ok
#endif


/*-------------- constants, tables ---------------------------------------*/
#define SNDTHD_EXIT_OK  1
#define SNDTHD_EXIT_BAD 2

/*-------------- Variables -----------------------------------------------*/


// Some parameters which may be 'overwritten' by command line arguments.
//  Therefore, the names are common for several modules like SndInThd, SerInThd, etc.
int     SOUND_iNrOfAudioChannels= 2;
T_Float SOUND_fltTestGeneratorFreq=0; // '0' means generator is disabled
T_Float SOUND_fltGainFactor     = 1.00;
int     SOUND_iUseSignedValues  = 0;  // 0=no 1=yes (for PIC: default=0=unsigned)
long    SOUND_lDecimationRatio  = 1;
T_Float SOUND_fltCenterFrequency= 0;
int     SOUND_fComplexOutput    = 0;     // 0=real, 1=complex output
T_Float SOUND_fltAudioPeakValue[2] = {0,0}; // crude peak detector for display purposes
int     SOUND_iChunkSize = 2048;
int     SOUND_iDcReject   = 1;
T_Float SOUND_fltDcOffset[2] = {0,0};
char    SOUND_sz80PortName[81] = "COM1";    // often overriden by command line !

/*-------------- Variables -----------------------------------------------*/



// A large "decimating" buffer where the samples from the soundcard are stored
//   (located in SndInThd.cpp)
CSoundDecimatingBuffer DecimatingAudioBuffer;


// Sample buffers used to read mono- or stereo-samples from the input
//  NOTE: The data for left+right channel are NOT separated
//        (in contrast to Spectrum Lab !) so [2*x] elements.
T_Float SerInThd_fltInputChunk[  2/*!!*/ * SOUND_MAX_CHUNK_SIZE];

// Other stuff...
int SndThd_iConnectMeterToOutput = 0; // 1=meter on output, 0=meter on input
int SndThd_iChunkSize = 2048;


typedef struct    // parameters and variables for PIC data decoder. See SerInThd_HandleRcvdData().
{ int iDecoderState;
  int iSamplesInInputChunk;
  int iTempI, iTempQ;   // temporary registers to combine "I"- and "Q" data
} T_SerInThd_PicDataDecoder;

T_SerInThd_PicDataDecoder SerInThd_PicDataDecoder;

BOOL   SndThd_fTerminateRequest;
HANDLE SndThd_hWorkerThread = 0; // Win32 API handle for the worker thread
DWORD  SndThd_dwWorkerThreadId;  // Identifier for the worker thread


//--------------------------------------------------------------------------
// Specials for serial I/O
//--------------------------------------------------------------------------
BOOL SerWin_fInitialized = FALSE;
char SerWin_sz64MyCommDeviceName[65] = "COM1";
char SerWin_sz255LastErrorText[256];
DCB  SerWin_MyDcb;  // enthaelt Baudrate, Formateinstellungen etc etc

HANDLE    SerWin_CommHandle = INVALID_HANDLE_VALUE;
COMMTIMEOUTS SerWin_CommTimeouts;
DWORD     SerWin_dwThreadLoopCounter = 0;
DWORD     SerWin_dwThreadErrorCounter= 0;
LONGLONG  SerWin_i64TimestampOfReceiveEvent=0; // value from QueryPerformanceCounter

// Um gleichzeitigen Zugriff von Worker Thread und der Applikation
// auf die COMM-Win-API-Routinen zu vermeiden (mit Enter+LeaveCriticalSection):
CRITICAL_SECTION SerWin_CriticalSection;

HANDLE SER_hStatusMessageEvent = NULL;  // Signale fuer den Worker Thread...
HANDLE SER_hThreadExitEvent    = NULL;
HANDLE SER_hTxRequestEvent     = NULL;
HANDLE SER_hTxCompleteEvent    = NULL;  // used to simulate the TX interrupt

#if(SER_WIN_USE_OVERLAPPED_IO>0)
 OVERLAPPED SerWin_osWrite = { NULL };   // "Overlapped" structure to wait for "TX complete"
 BOOL       SerWin_fWritePending = FALSE;
#endif // (SER_WIN_USE_OVERLAPPED_IO>0)


long   SerWin_lCountReceivedBytes = 0;    // for debugging purposes.


//------------- Implementation of routines and classes ----------------------


//-------- Some of the ugly red tape required in windoze app's -----------
static char *SerIn_szLastErrorCodeToString(DWORD dwError)
{
  static char sz80Result[81];

  LPVOID lpMsgBuf;

  FormatMessage(
    FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
    NULL,
    dwError,
    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
    (LPTSTR) &lpMsgBuf,
    0,
    NULL      );

  // Copy the string to the caller's buffer
  strncpy( /*dest:*/sz80Result, /*source:*/(char*)lpMsgBuf,  80/*maxlen*/ );

  // Free the buffer.
  LocalFree( lpMsgBuf );

  return sz80Result;
}



//---------------------------------------------------------------------------
void SOUND_SetDefaultParameters(void)
  // Prepare some soundcard settings (WAVEFORMATEX) required later.
  // Note: some of these settings may be overridden by the command line parser.
{
} // end SOUND_SetDefaultParameters()




//--------------------------------------------------------------------------
//  Little helpers to access the serial interface via Win API functions
//--------------------------------------------------------------------------




/***************************************************************************/
BOOL SerWin_OpenSerialPort( char *pszPortName )
   /* Opens the serial port.
    * Note that because hardware vendors and serial-device-driver writers
    * are free to name the ports for everything they like,
    * so it's NOT ok to pass just a number ! With a bit of luck, the
    * serial interface uses one of the traditional names COM1, COM2, COM3, COM4.
    *
    *  Note on WinXP: No idea why COM-port-numbers above 9 usually do NOT work.
    *      Seems to have got something to do with the "operating system" ?!?!? 
    *
    * return value:
    *     TRUE = ok
    *     FALSE= error, look at SerWin_sz255LastErrorText for info.
    */
{

   if(! SerWin_fInitialized)
    {
#if(SER_WIN_USE_OVERLAPPED_IO>0)
     SerWin_fWritePending = FALSE;

     // Create overlapped event for the "sender". Must be closed before exiting
     // to avoid a handle leak.
     SerWin_osWrite.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
#endif // SER_WIN_USE_OVERLAPPED_IO


     InitializeCriticalSection(&SerWin_CriticalSection);
     // Remarks on 'InitializeCriticalSection' from Microsoft :
     // > The threads of a single process can use a critical section object
     // > for mutual-exclusion synchronization. The process is responsible
     // > for allocating the memory used by a critical section object,
     // > which it can do by declaring a variable of type CRITICAL_SECTION.
     // > Before using a critical section, some thread of the process
     // > must call the InitializeCriticalSection function
     // > to initialize the object.
     // > Once a critical section object has been initialized, the threads
     // > of the process can specify the object in the EnterCriticalSection,
     // > TryEnterCriticalSection, or LeaveCriticalSection function
     // > to provide mutually exclusive access to a shared resource.
     // Here, the 'shared resource' is the serial interface.

     SerWin_fInitialized = TRUE;   // done
    } // end if(!SerWin_fInitialized)


   if(! SerWin_fInitialized)
       return FALSE;   // a severe application error ! SET BREAKPOINT HERE

   // Close serial port if opened before
   if (SerWin_CommHandle != INVALID_HANDLE_VALUE)
     {
       CloseHandle(SerWin_CommHandle);
       SerWin_CommHandle = INVALID_HANDLE_VALUE;
     }

   // A process uses the CreateFile function to open a handle
   // to a communications resource.
   // For example, specifying COM1 opens a handle to a serial port,
   // and LPT1 opens a handle to a parallel port.
   // If the specified resource is currently being used
   // by another process, CreateFile fails.
   // Any thread of the process can use the handle returned
   // by CreateFile to identify the resource in any of the functions
   // that access the resource.
   // When the process uses CreateFile to open a communications
   // resource, it must specify certain values for the following
   //  parameters:
   //    The fdwShareMode parameter must be zero, opening the resource
   //                     for exclusive access.
   //    The fdwCreate parameter
   //                     must specify the OPEN_EXISTING flag.
   //    The hTemplateFile parameter must be NULL.
   // Under WinXP (NT?), it seems to be impossible to do simulaneous
   //    READ and WRITE operations without the OVERLAPPED hassle.
   // Under Win95, there were no problems !!!
   SerWin_CommHandle = CreateFile(
                          pszPortName, // pointer to name of the file
         GENERIC_READ | GENERIC_WRITE, // access (read-write) mode
                                    0, // share mode
                                 NULL, // pointer to security attributes
                        OPEN_EXISTING, // how to create
#if(SER_WIN_USE_OVERLAPPED_IO>0)
                 FILE_FLAG_OVERLAPPED, // file attribute from "demo program"
#else // ! (SER_WIN_USE_OVERLAPPED_IO>0)
                FILE_ATTRIBUTE_NORMAL, // file attributes..
#endif //   SER_WIN_USE_OVERLAPPED_IO
                                 NULL  // handle to file with attributes to copy
                             );
   if(SerWin_CommHandle == INVALID_HANDLE_VALUE)
    {
     sprintf(SerWin_sz255LastErrorText, "Cannot not open port (%s)",
             SerIn_szLastErrorCodeToString(GetLastError() ) );
     return FALSE;
    }

   // To determine the initial configuration of a serial communications
   // resource, a process calls the GetCommState function,
   // which fills in a serial port DCB structure with the current
   // configuration settings.
   SerWin_MyDcb.DCBlength = sizeof(DCB);  // for compatibility checks..
   if( ! GetCommState(
                         SerWin_CommHandle, // handle of communications device
                          & SerWin_MyDcb  // address of device-control block structure
                     ) )
    {
      sprintf(SerWin_sz255LastErrorText, "Cannot read CommState (%s)",
            SerIn_szLastErrorCodeToString(GetLastError()) );
      CloseHandle(SerWin_CommHandle);
      SerWin_CommHandle = INVALID_HANDLE_VALUE;
      return FALSE;
    }

   // To modify this configuration, a process specifies a DCB structure
   // in a call to the SetCommState function.
   // Members of the DCB structure specify the configuration settings
   // such as the baud rate, the number of data bits per byte,
   // and the number of stop bits per byte. Other DCB members specify special
   // characters and enable parity checking and flow control.
   // When a process needs to modify only a few of these configuration settings,
   // it should first call GetCommState to fill in a DCB structure with the
   // current configuration. Then the process can adjust the important values
   // in the DCB structure and reconfigure the device by calling SetCommState
   // and specifying the modified DCB structure.
   //       (WB: that's exactly how it's done here.)
   // This procedure ensures that the unmodified members of the DCB structure
   // contain appropriate values. For example, a common error is to configure
   // a device with a DCB structure in which the structure's XonChar member
   // is equal to the XoffChar member. Some members of the DCB structure
   // are different from those in previous versions of Microsoft Windows.
   // In particular, the flags for controlling RTS (request-to-send)
   // and DTR (data-terminal-ready) have changed.
   //  WoBu: We set almost everything here to the driver package's "default"
   //        value   so we know quite well what's going on,
   //        regardless of the WINDOZE VERSION and System Settings on this PC !
   SerWin_MyDcb.BaudRate= 115200;                  // current baud rate (PIC with "SerialAD" firmware uses 115200 bit/second)
   SerWin_MyDcb.fBinary = TRUE;                    // binary mode, no EOF check
   SerWin_MyDcb.fParity = FALSE;                   // TRUE=enable parity checking
   SerWin_MyDcb.fOutxCtsFlow= FALSE;               // no CTS output flow control
   SerWin_MyDcb.fOutxDsrFlow= FALSE;               // DSR output flow control
   SerWin_MyDcb.fDtrControl=DTR_CONTROL_ENABLE;    // DTR flow control type: DTR_CONTROL_ENABLE=0x01="leave it on"(!!)
   SerWin_MyDcb.fDsrSensitivity=FALSE;             // DSR sensitivity

   SerWin_MyDcb.fTXContinueOnXoff=FALSE;           // XOFF continues Tx
   SerWin_MyDcb.fOutX = FALSE;                     // XON/XOFF out flow control
   SerWin_MyDcb.fInX  = FALSE;                     // XON/XOFF in flow control
   SerWin_MyDcb.fErrorChar= FALSE;                 // enable error replacement
   SerWin_MyDcb.fNull = FALSE;                     // enable null stripping (FALSE: don't throw away NULL bytes!)
   SerWin_MyDcb.fRtsControl=RTS_CONTROL_ENABLE;    // RTS flow control ..
      // RTS_CONTROL_ENABLE: Enable the RTS line when the device is opened and leave it on.
   SerWin_MyDcb.fAbortOnError=FALSE;               // abort reads/writes on error
 //  SerWin_MyDcb.fDummy2=SerWin_MyDcb.fDummy2;        // reserved
 //  SerWin_MyDcb.wReserved=SerWin_MyDcb.wReserved;    // not currently used

   SerWin_MyDcb.XonLim = 2048;    // transmit XON threshold (65535 geht unter WinXP nicht ?)
   SerWin_MyDcb.XoffLim= 2048;                    // transmit XOFF threshold
   SerWin_MyDcb.ByteSize= 8;                       // number of bits/byte, 4-8
   SerWin_MyDcb.Parity  = 0;                       // 0-4=no,odd,even,mark,space
   SerWin_MyDcb.StopBits= 0;                       // 0,1,2 = 1, 1.5, 2
   SerWin_MyDcb.XonChar = 0x11;                    // Tx and Rx XON character
   SerWin_MyDcb.XoffChar= 0x13;                    // Tx and Rx XOFF character
   SerWin_MyDcb.ErrorChar=0x00;                    // error replacement character

   SerWin_MyDcb.EofChar = 0x00;                    // end of input character
   SerWin_MyDcb.EvtChar = 0x00;                    // received event character
 //  SerWin_MyDcb.wReserved1=SerWin_MyDcb.wReserved1; // reserved; do not use


   if( ! SetCommState(
                  SerWin_CommHandle, // handle of communications device
                   & SerWin_MyDcb))  // address of device-control block structure
    {
      sprintf(SerWin_sz255LastErrorText, "Cannot set CommState (%s)",
            SerIn_szLastErrorCodeToString(GetLastError() ) );
      CloseHandle(SerWin_CommHandle);
      SerWin_CommHandle = INVALID_HANDLE_VALUE;
      return FALSE;
    }


   // Now define the RX- and TX timeouts for calls to ReadFile and WriteFile.
   // See Win32 Programmer's Reference on COMMTIMEOUTS.
   SerWin_CommTimeouts.ReadIntervalTimeout = 0;
                 // Specifies the maximum time, in milliseconds,
                 // allowed to elapse between the arrival of
                 // two characters on the communications line.

   SerWin_CommTimeouts.ReadTotalTimeoutMultiplier = 0;
                 // Specifies the multiplier, in milliseconds,
                 // used to calculate the total time-out period
                 // for read operations. For each read operation,
                 // this value is multiplied by the requested
                 // number of bytes to be read.

   SerWin_CommTimeouts.ReadTotalTimeoutConstant = 20;
                 // Specifies the constant, in milliseconds,
                 // used to calculate the total time-out period
                 // for read operations. For each read operation,
                 // this value is added to the product of the
                 // ReadTotalTimeoutMultiplier member and the
                 // requested number of bytes.

   SerWin_CommTimeouts.WriteTotalTimeoutMultiplier = 80;
                 // Specifies the multiplier, in milliseconds,
                 // used to calculate the total time-out period
                 // for write operations. For each write operation,
                 // this value is multiplied by the number of bytes
                 // to be written.
                 // WB: should be somehow baudrate-dependent !

   SerWin_CommTimeouts.WriteTotalTimeoutConstant = 100;
                 // Specifies the constant, in milliseconds,
                 // used to calculate the total time-out period
                 // for write operations. For each write operation,
                 // this value is added to the product of the
                 // WriteTotalTimeoutMultiplier member
                 // and the number of bytes to be written.

   // The SetCommTimeouts function sets the time-out parameters
   // for all read and write operations on a specified communications device.
   if( ! SetCommTimeouts( SerWin_CommHandle, &SerWin_CommTimeouts ) )
    {
      sprintf(SerWin_sz255LastErrorText, "Cannot set CommTimeouts (%s)",
            SerIn_szLastErrorCodeToString(GetLastError() ) );
      CloseHandle(SerWin_CommHandle);
      SerWin_CommHandle = INVALID_HANDLE_VALUE;
      return FALSE;
    }

   return TRUE;

} // end SerWin_OpenSerialPort(..)


/***************************************************************************/
 long SerWin_GetBaudrate(void)
   /* Reads the current baudrate from the serial port.
    * Returns a positive baudrate (bits per second) if ok,
    *         otherwise a negative value.
    */
{
   if(! SerWin_fInitialized)
      return -1;   // a severe application error ! SET BREAKPOINT HERE

   if(SerWin_CommHandle == INVALID_HANDLE_VALUE)
      return -1;
   else
    {
      switch( SerWin_MyDcb.BaudRate )
       {  // WinAPI mentions "baudrate INDEXES", not sure if that the value
          // is bits per second.
          // So here for safety, if the INDEXES are really just
          // INDEXES and no BITS PER SECOND:
        case CBR_110:    return 110;
        case CBR_300:    return 300;
        case CBR_600:    return 600;
        case CBR_1200:   return 1200;
        case CBR_2400:   return 2400;
        case CBR_4800:   return 4800;
        case CBR_9600:   return 9600;
        case CBR_14400:  return 14400;
        case CBR_19200:  return 19200;
        case CBR_38400:  return 38400;
        case CBR_56000:  return 57600;
        case CBR_57600:  return 57600;
        case CBR_115200: return 115200;
        case CBR_128000: return 128000;
        case CBR_256000: return 256000;
        default:         return SerWin_MyDcb.BaudRate;
       }
    }
}  // end SerWin_GetBaudrate()



/***************************************************************************/
 BOOL SerWin_SetBaudrate(long baudrate)
   /* Sets the baudrate for the serial port.
    * returns TRUE if ok,
    *         otherwise FALSE.
    */
{
 BOOL fResult;

   if(! SerWin_fInitialized)
      return FALSE; // a severe application error ! SET BREAKPOINT HERE

   if(SerWin_CommHandle == INVALID_HANDLE_VALUE)
      return FALSE;
   else
    { // see WinAPI help on DCB
      EnterCriticalSection(&SerWin_CriticalSection);
      SerWin_MyDcb.BaudRate = baudrate;
      if( SetCommState( SerWin_CommHandle, &SerWin_MyDcb ) )
            fResult = TRUE;
       else fResult = FALSE;
      // Read the "DCB" again to make sure we have the accurate value,
      // maybe the OS had to modify the contents:
      GetCommState( SerWin_CommHandle, &SerWin_MyDcb );
      LeaveCriticalSection(&SerWin_CriticalSection);

      return fResult;
    }
}  // end ..SetBaudrate()


/**************************************************************************/
 BOOLEAN SerWin_ReceiveSingleChar(BYTE *dest)
  /* Empfang eines Zeichens (0..255) mit Meldung ob erfolgreich.
   *  kann auch Zeichencode "Null" empfangen !
   *  Eingangsparameter:  dest = Ziel-Pointer auf empfangenes Zeichen
   *  Return-Wert: TRUE=Zeichen wurde empfangen, FALSE=kein Zeichen empfangen.
   *
   *  ACHTUNG!!!!!! Falls keine Zeichen von der seriellen Schnittstelle
   *                vorliegen, kehrt die Routine erst
   *                NACH ABLAUF EINER TIMEOUT-ZEIT ZURUECK !!!!!!!!
   *
   * Implementiert im Juni 2002 zwecks einfacher Anpassung von
   * MKT's "SER_DRV.C" auf Windows-Umgebung (z.B. im "MKT-View"-Programmiertool)
   */
{
  DWORD dwRead=0;
  ReadFile(SerWin_CommHandle,
                  dest, 1, // dest.buffer + size
                  &dwRead, NULL );
  if(dwRead>0)
     return TRUE;
  else
     return FALSE;
} // end SerWin_ReceiveSingleChar()



/***************************************************************************/
 int SerWin_Transmit( BYTE *source, int n_bytes, BOOLEAN fWaitForCompletion )
   /* Transmits a block of data via the serial interface.
    * Returns the number of bytes written (limited by timeout)
    *         or a negative error code.
    */
{
 static DWORD sdwNWritten;
 BOOL  fWriteResult= TRUE;
 BOOL  fWriteFileResult;
 DWORD dwErrorCode = NO_ERROR;
 int   dwWrittenPreviously=0;


   if(! SerWin_fInitialized)
      return -1;   // a severe application error ! SET BREAKPOINT HERE

   if(SerWin_CommHandle == INVALID_HANDLE_VALUE)
      return -1;

#if(SER_WIN_USE_OVERLAPPED_IO>0)
   if(SerWin_osWrite.hEvent == NULL)
       return -1;         // Error creating overlapped event; abort.
#endif // SER_WIN_USE_OVERLAPPED_IO

#if(SER_WIN_USE_OVERLAPPED_IO>0)
   if(SerWin_fWritePending)
     { // if we MUST wait for TX completion, check the result of the PREVIOUS write,
       // (no matter if the caller wants or not. Only ONE write-operation may pend.
       if(WaitForSingleObject( SerWin_osWrite.hEvent, 500/*ms*/ ) != WAIT_OBJECT_0)
          fWriteResult = FALSE; // ein "echter" Fehler waehrend Warten auf Write-Complete-Event
       SerWin_fWritePending = FALSE;
     }
#endif // (SER_WIN_USE_OVERLAPPED_IO>0)

   EnterCriticalSection(&SerWin_CriticalSection); // Vermeiden, dass ein anderer Thread aufs File zugreift (?!?!?)

   fWriteFileResult = WriteFile(
          SerWin_CommHandle, // handle to "file" to write to
          source,       // pointer to data to write to file
          n_bytes,      // number of bytes to write
          &sdwNWritten, // pointer to number of bytes written
#if(SER_WIN_USE_OVERLAPPED_IO>0)
         &SerWin_osWrite); // pointer to structure needed for overlapped I/O
                           // (required for Win XP, but not for Win 98 ??!?)
#else  // ! SER_WIN_USE_OVERLAPPED_IO..
          NULL );       // no pointer to "overlapped" structure
#endif // SER_WIN_USE_OVERLAPPED_IO


    LeaveCriticalSection(&SerWin_CriticalSection);

    if(!fWriteFileResult)
     {
       // Under Win98, the WriteFile function returns after the bytes are
       //   transmitted or at least put into the output buffer.
       // Under WinXP, the WriteFile function does *not* return. Bullshit.
       dwErrorCode = GetLastError();
       if(dwErrorCode != ERROR_IO_PENDING)
          fWriteResult = FALSE; // ein "echter" Fehler
       else
        {
#if(SER_WIN_USE_OVERLAPPED_IO>0)
          // Write is pending.  Wait here for completion ?
          if(!GetOverlappedResult(SerWin_CommHandle, &SerWin_osWrite,
                       &sdwNWritten, // pointer to number of bytes written
               fWaitForCompletion )) // BOOL bWait, wait flag
           {
             // GetOverlappedResult depends on the bWait-flag:
             // bWait: Specifies whether the function should wait
             //        for the pending overlapped operation to be completed.
             //    If TRUE, the function does not return until the operation
             //        has been completed.
             //    If FALSE and the operation is still pending,
             //        the function returns FALSE and the GetLastError
             //        function returns ERROR_IO_INCOMPLETE.
             if(fWaitForCompletion)
                fWriteResult = FALSE; // ein "echter" Fehler waehrend Warten..
             else // 'bWait' was FALSE in GetOverlappedResult() :
              {
               if(GetLastError()==ERROR_IO_INCOMPLETE)
                     SerWin_fWritePending = TRUE;
                else
                     fWriteResult = FALSE; // ein "echter" Fehler waehrend Warten auf WRITE
              }
           } // end if !GetOverlappedResult()
          else
           { // GetOverlappedResult() returned TRUE, everything ok, means:
             fWriteResult=TRUE;  // the "write operation" has been completed
           }
#else  // ! SER_WIN_USE_OVERLAPPED_IO...

#endif //   SER_WIN_USE_OVERLAPPED_IO
        } // end if "real" error from WriteFile (not IO pending)
     }
    else
     { // write operation completed IMMEDIATELY, no need to 'wait'
#if(SER_WIN_USE_OVERLAPPED_IO>0)
       SerWin_fWritePending = FALSE;
#endif // SER_WIN_USE_OVERLAPPED_IO
     }

    if(fWriteResult==TRUE)
     { // WriteFile successfull..
      return sdwNWritten + dwWrittenPreviously;
     }
    else // grumble... what went wrong under Windoze Icks Peeh ?
     {   // WinXP returned error code 87 while Win98 was absolutely satisfied.
         // Error Code 87 possibly means "illegal parameter" but WHERE, WHEN, and WHY ?
       sprintf(SerWin_sz255LastErrorText, "Cannot write to serial port (%s)",
            SerIn_szLastErrorCodeToString( dwErrorCode ) );
      return -1;
     }
}  // end SerWin_Transmit()


/***************************************************************************/
 BOOLEAN SerWin_IsTxBusy(void)
   /* Checks if ALL CHARACTERS have been transmitted via serial interface,
    *  including various buffers and possibly pending 'overlapped' operations.
    * Returns:
    *     TRUE  = transmitter is busy, Write still pending, buffer not empty.
    *     FALSE = transmitter is NOT busy, or module not initialized, or ....
    */
{
 DWORD dwNWritten;
 BOOLEAN fResult = FALSE;


   if(! SerWin_fInitialized)
      return FALSE;   // a severe application error ! SET BREAKPOINT HERE

   if(SerWin_CommHandle == INVALID_HANDLE_VALUE)
      return FALSE;   // a non-opened interface cannot be "busy" from this point of view !

#if(SER_WIN_USE_OVERLAPPED_IO>0)
   if(SerWin_osWrite.hEvent == NULL)
      return FALSE;   // Error creating overlapped event; abort.
   if(! SerWin_fWritePending)
      return FALSE;
#endif // (SER_WIN_USE_OVERLAPPED_IO>0)


   EnterCriticalSection(&SerWin_CriticalSection); // avoid threads stomping on each other

#if(SER_WIN_USE_OVERLAPPED_IO>0)
   if(!GetOverlappedResult(SerWin_CommHandle, &SerWin_osWrite,
                       &dwNWritten, // pointer to number of bytes written
                           FALSE )) // BOOL bWait, wait flag
    { // GetOverlappedResult depends on the bWait-flag:
      // bWait: Specifies whether the function should wait
      //        for the pending overlapped operation to be completed.
      //    If FALSE and the operation is still pending,
      //        the function returns FALSE and the GetLastError
      //        function returns ERROR_IO_INCOMPLETE.
      if(GetLastError()==ERROR_IO_INCOMPLETE)
           fResult=TRUE;   // the write operation is still pending, we are "busy" !
     }
#endif // SER_WIN_USE_OVERLAPPED_IO

    LeaveCriticalSection(&SerWin_CriticalSection);
    return fResult;
}  // end SerWin_IsTxBusy()






//----------------------------------------------------------------------------
//  Subroutines to process the serial data (from PIC to PC via "COM1","COM2")
//----------------------------------------------------------------------------


void  SerInThd_HandleRcvdData( BYTE *pbRxData, DWORD cbReceived)
{   // Called by worker thread after reception of a block of data.
  int iSource;
  WORD   w;
  double d;
  static T_Float pk0=0,pk1=0;

    // Convert the received data from the PIC's format (12 bit integer pairs)
    //    into the floating point format to feed the 'output' buffer:
    for(iSource=0; iSource<cbReceived; ++iSource)
     {
      w = pbRxData[iSource];  // get the next BYTE (!) from the serial input stream

      // Taken from PIC firmware description ( c:\pic\SerialAD\SerPicAD.txt ):
      // Byte[0] = Sync/Status
      //           Usually 0xFF which looks like a "STOP BIT" to the receiver.
      //           Used for frame sync, but also for BYTE sync if receiver
      //           turned on too late.
      //
      // Byte[1] = Least significant bits of I-channel (I7..I0).
      //
      // Byte[2] = Least significant bits of Q-channel (Q7..Q0).
      //
      // Byte[3] : Bits 3..0 = Most significant bits of I-channel (I11..I8),
      //           Bits 7..4 = Most significant bits of Q-channel (Q11..Q8),
      switch(SerInThd_PicDataDecoder.iDecoderState)
       {
         case 0:     // "I don't know what the received byte is"
              if(w==0x00FF)
                { // looks like a sync byte. This is byte[0] of a 4-byte-frame.
                  SerInThd_PicDataDecoder.iDecoderState = 1;  // waiting for byte[0]
                }
              break; // end case SerInThd_PicDataDecoder.iDecoderState==0
         case 1:     // "I expect data byte[1] from the PIC".
              SerInThd_PicDataDecoder.iTempI = (int)w;
              SerInThd_PicDataDecoder.iDecoderState = 2;  // waiting for byte[2]
              break; // end case SerInThd_PicDataDecoder.iDecoderState==1
         case 2:     // "I expect data byte[2] from the PIC".
              SerInThd_PicDataDecoder.iTempQ = (int)w;
              SerInThd_PicDataDecoder.iDecoderState = 3;  // waiting for byte[3]
              break; // end case SerInThd_PicDataDecoder.iDecoderState==2
         case 3:     // "I expect data byte[3] from the PIC".
              SerInThd_PicDataDecoder.iTempI |= ( (int)(w&0x0F) )<<8;
              SerInThd_PicDataDecoder.iTempQ |= ( (int)(w&0xF0) )<<4;
              // If necessary, flush the old buffer:
              if( (SerInThd_PicDataDecoder.iSamplesInInputChunk >= (2/*!!*/ * SOUND_MAX_CHUNK_SIZE -2) )
                ||(SerInThd_PicDataDecoder.iSamplesInInputChunk >= (SOUND_iChunkSize+2) ) )
               { DecimatingAudioBuffer.EnterSamples(
                   SerInThd_fltInputChunk, // pointer to LEFT SOURCE CHANNEL (or 'paired' data)
                   NULL,                   // pointer to RIGHT SOURCE CHANNEL (NULL for 'paired' data)
                   SerInThd_PicDataDecoder.iSamplesInInputChunk/2); // count of source SAMPLE PAIRS(!!)

                 SerInThd_PicDataDecoder.iSamplesInInputChunk = 0;
                 pk0=pk1=0;  // reset peak detector for the next chunk of samples
               }
              if(SOUND_iUseSignedValues) // 0=no 1=yes (for PIC: default=0=unsigned)
               {
                // Convert 12-bit-values (0..4095) into floating point numbers,
                //   result range = -32768...+32767 :
                d = (double)SerInThd_PicDataDecoder.iTempI * 16 - 32768.0;
                SerInThd_fltInputChunk[SerInThd_PicDataDecoder.iSamplesInInputChunk++] = d;
                // Crude peak detector for display purposes..
                if(d>0) { if(d>pk0)   pk0 = d;  }
                  else  { if(d<-pk0)  pk0 = -d; }
                // Same conversion and peak detection for the 2nd audio channel ("Q"):
                d = (double)SerInThd_PicDataDecoder.iTempQ * 16 - 32768.0;
                SerInThd_fltInputChunk[SerInThd_PicDataDecoder.iSamplesInInputChunk++] = d;
                // Crude peak detector for display purposes..
                if(d>0) { if(d>pk1)   pk1 = d;  }
                  else  { if(d<-pk1)  pk1 = -d; }
               }
              else // samples are UNSIGNED values (-> output range 0..32767)
               {
                // Convert 12-bit-values (0..4095) into floating point numbers,
                // scaled to +-32k :
                d = (double)SerInThd_PicDataDecoder.iTempI * 8;
                SerInThd_fltInputChunk[SerInThd_PicDataDecoder.iSamplesInInputChunk++] = d;
                // Crude peak detector for display purposes..
                if(d>pk0)   pk0 = d;
                // Same conversion and peak detection for the 2nd audio channel ("Q"):
                d = (double)SerInThd_PicDataDecoder.iTempQ * 8;
                SerInThd_fltInputChunk[SerInThd_PicDataDecoder.iSamplesInInputChunk++] = d;
                // Crude peak detector for display purposes..
                if(d>pk1)   pk1 = d;
               }

              SerInThd_PicDataDecoder.iDecoderState = 0;  // waiting for byte[0]
              break; // end case SerInThd_PicDataDecoder.iDecoderState==3
          default:
              SerInThd_PicDataDecoder.iDecoderState = 0;   // ?!?
              break;
       } // end switch(SerInThd_PicDataDecoder.iDecoderState)

     }  // end for <all bytes from serial data block>

  SOUND_fltAudioPeakValue[0/*iChn*/] = pk0;
  SOUND_fltAudioPeakValue[1/*iChn*/] = pk1;


} // end SerInThd_HandleRcvdData()
/*---------------------------------------------------------------------------*/


/*-------------- Implementation of the Serial Audio Input Thread ----------*/


//---------------------------------------------------------------------------
DWORD WINAPI SerInThdFunc( LPVOID lpParam )
  /* Executes the sound analyzer thread.
   *   Was once the "Execute" method of a VCL Thread object,
   *   but this unit does not use the VCL any longer since May 2002.
   */
{

  DWORD dwResult=0;
  int   iBreakPoint=0;
#if(SER_WIN_USE_OVERLAPPED_IO>0)
  #define L_RX_BUFFER_SIZE 512
#else
  #define L_RX_BUFFER_SIZE 1
#endif // SER_WIN_USE_OVERLAPPED_IO
  BYTE  bMyRxBuffer[L_RX_BUFFER_SIZE];
  BOOL  fWaitedSomewhere;



 INT i,iMax,iChn;
 INT error_code;
 double d,p;
 double dblDcFactor;

   int iSoundInputDeviceID  = -1;  // -1 means "the default audio input device"
   int iSoundOutputDeviceID = -1;

  if( SOUND_iChunkSize > SOUND_MAX_CHUNK_SIZE )
      SOUND_iChunkSize = SOUND_MAX_CHUNK_SIZE;



    // Serial Port Handling based on an article from Microsoft
    //     "Serial Communications in Win32" by Allen Denver,
    //     local copy at C:\CBProj\SerialComm\Serial...Win32.htm .
    DWORD dwRead;
    DWORD dwRes;
#if(SER_WIN_USE_OVERLAPPED_IO>0)
    BOOL  fWaitingOnRead = FALSE;
    OVERLAPPED osRead = { 0 };
#endif // (SER_WIN_USE_OVERLAPPED_IO>0)


#if(SER_WIN_USE_OVERLAPPED_IO>0)
    // Create the overlapped event. Must be closed before exiting
    // to avoid a handle leak.
    osRead.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if(osRead.hEvent == NULL)
       return -1;         // Error creating overlapped event; abort.
#endif // SER_WIN_USE_OVERLAPPED_IO



  SOUND_fltDcOffset[0]=SOUND_fltDcOffset[1]=0.0;


  // SerIn_thd_error_buf_in = SerIn_thd_error_buf_out = 0; // NO !!
  SerIn_exited_from_thread_loop = false;


  /* Almost endless thread loop begins HERE................................*/
  while( !SndThd_fTerminateRequest )
   {
       ++SerWin_dwThreadLoopCounter;

       fWaitedSomewhere = FALSE;
       if(! SerWin_fInitialized)
         { dwResult = -2; // a severe application error ! SET BREAKPOINT HERE
           break;
         }
       if(SerWin_CommHandle != INVALID_HANDLE_VALUE)
         {
         // Try to receive bytes from the communication port..
#if(SER_WIN_USE_OVERLAPPED_IO>0)
           if( !fWaitingOnRead)
            { // Issue read operation.
              if (!ReadFile(SerWin_CommHandle,
                            bMyRxBuffer, L_RX_BUFFER_SIZE, // dest.buffer + size
                            &dwRead, &osRead ) )
               {
                 if(GetLastError()!=ERROR_IO_PENDING) // read not delayed ?
                  { // error in communications; report it.
                    ++SerWin_dwThreadErrorCounter;
                  }
                 else
                    fWaitingOnRead = TRUE;
               }
              else
               { // read completed immediately
                 SerWin_lCountReceivedBytes += dwRead;
                 SerInThd_HandleRcvdData(bMyRxBuffer, dwRead);
               }
            } // end if (!fWaitingOnRead)

           // The second part of the overlapped operation is the detection
           // of its completion. The event handle in the OVERLAPPED struct
           // is passed to a Wait function, which will wait until the object
           // is signaled. Once the event is signaled, the operation is
           // complete.
           // This does not mean that it was completed successfully,
           // just that it was completed. The GetOverlappedResult function
           // reports the result of the operation. [..]
           if(fWaitingOnRead)
            {
             dwRes = WaitForSingleObject(osRead.hEvent, 50/*ms*/ );
             switch(dwRes)
              {
               // Read completed.
               case WAIT_OBJECT_0:
                    QueryPerformanceCounter( (LARGE_INTEGER*)&SerWin_i64TimestampOfReceiveEvent);
                    if (!GetOverlappedResult(SerWin_CommHandle, &osRead, &dwRead, FALSE))
                      { // Error in communications; report it.
                        ++SerWin_dwThreadErrorCounter;
                      }
                    else
                      { // Read completed successfully.
                        if(dwRead>0)
                         {
                           fWaitedSomewhere = TRUE;
                           SerWin_lCountReceivedBytes += dwRead;
                           SerInThd_HandleRcvdData(bMyRxBuffer, dwRead);
                         }
                      }

                    //  Reset flag so that another opertion can be issued.
                    fWaitingOnRead = FALSE;
                    break; // end case WAIT_OBJECT_0

               case WAIT_TIMEOUT:
                    // Operation isn't complete yet. fWaitingOnRead flag isn't
                    // changed since I'll loop back around, and I don't want
                    // to issue another read until the first one finishes.
                    //
                    // This is a good time to do some background work.
                    iBreakPoint = 1;  // <<< only used for BREAKPOINT
                    fWaitedSomewhere = TRUE;
                    break;

               default:
                    // Error in the WaitForSingleObject; abort.
                    // This indicates a problem with the OVERLAPPED structure's
                    // event handle.
                    ++SerWin_dwThreadErrorCounter;
                    break;
              } // end switch(dwRes)

            } // end if(fWaitingOnRead)

#else  // ! SER_WIN_USE_OVERLAPPED_IO...


           // Here the simplest possible way:
           //    Just issue one read-operation after the other,
           //    without overlapped handling.
           //    Whenever "ReadFile" returned something,
           //    pass it on to the application.
           // This is in fact FASTER than the more sophisticated way
           // with overlapped I/O, but it failed under WinXP - see below.
           EnterCriticalSection(&SerWin_CriticalSection); // avoid threads stomping on each other
           // ( Using a critial section was just an ugly test because
           //   under Billy's latest gimmick called WinXP
           //   the program does not work, but no problem under Win98.
           //   It worked but is EXTREEEMELY SLOW,
           //   because we slow down the receiving thread
           //   so it only RECEIVED one character per TIMEOUT! )
           if (ReadFile(SerWin_CommHandle,
                        bMyRxBuffer, L_RX_BUFFER_SIZE, // dest.buffer + size
                        &dwRead, NULL ) )
            {
               // Under Win98, the receiver thread seems to WAIT
               //   in ReadFile.
               // Under WinXP, the thread returns immediately
               //   with *NO* received bytes (dwRead=0)
               if(dwRead>0)
                {
                  SerInThd_HandleRcvdData(bMyRxBuffer, dwRead);
                } // end if <received something from the serial port>
            }
           LeaveCriticalSection(&SerWin_CriticalSection); // bloody bullshit
#endif // ! SER_WIN_USE_OVERLAPPED_IO
        } // end if <valid handle for the communication port>


   // Ugly stuff because of massive thread synchronisation problems:
   //    if(! fWaitedSomewhere )
   //     { Sleep(20);
   //       fWaitedSomewhere = TRUE;
   //     }

     }; // end while < thread loop >

#if(SER_WIN_USE_OVERLAPPED_IO>0)
    // Close the overlapped event to avoid a handle leak.
    CloseHandle(osRead.hEvent);
#endif // (SER_WIN_USE_OVERLAPPED_IO>0)


//-----


  /* if the program ever gets here, the thread is quite "polite"... */
  /*                    or someone has pulled the emergency brake ! */
  SerIn_exited_from_thread_loop = true;

  ExitThread( SNDTHD_EXIT_OK ); // exit code for this thread
     // Note: The code STILL_ACTIVE must not be used as parameter for
     // ExitThread(), but what else ? See Win32 API help !
     // in winbase.h:
     //    #define STILL_ACTIVE STATUS_PENDING
     // in winnt.h:
     //    #define STATUS_PENDING ((DWORD   )0x00000103L)
  return SNDTHD_EXIT_OK; // will this ever be reached after "ExitThread" ?
} // end SoundThdFunc()
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
BOOL SoundThd_LaunchWorkerThread(void)
   // Kick the real-time sound processing thread to life,
   //  without the help of Borland's Visual Component Library (VCL).
{
  DWORD dwThrdParam;

  // Make sure the cosine table exists, the frequency converter (NCO) may need it
  SoundTab_InitCosTable();

  // Initialize the decoder for serial data from the PIC microcontroller.
  //   See  SerInThd_HandleRcvdData() .
  memset(&SerInThd_PicDataDecoder, 0, sizeof(T_SerInThd_PicDataDecoder) );

  // Initialize a 'decimating' buffer which is used to pass the samples
  // from the audio device to the application,
  // with the option to decimate the sample rate (et al) .
  DecimatingAudioBuffer.Init(
     SOUND_MAX_CHUNK_SIZE,          // lBufferLength, max count of sample points (!), here: 1 second of data
     2500,                          // dblInputSampleRate, often 11025, 22050 or 44100, but 2500 from PIC
     SOUND_lDecimationRatio,        // lSampleRateDivisor, powers of two preferred
     SOUND_fltCenterFrequency,      // dblMixerFrequency,  "NCO" frequency in Hertz
     SOUND_fltCenterFrequency>0,    // fComplex:  TRUE = with frequency shift + complex output
     2   );                         // iNrChannels: 1=mono, 2=stereo processing


  // Try to open the serial port.
  if(! SerWin_OpenSerialPort( SOUND_sz80PortName/*"COM1".."COM9"*/ ) )
     return FALSE;

  SerWin_lCountReceivedBytes = 0;

  SndThd_fTerminateRequest = FALSE;

  //
  // Create status message event
  //
  if(SER_hStatusMessageEvent == NULL)
     SER_hStatusMessageEvent = CreateEvent(NULL, // pointer to security attributes
                                            FALSE, // flag for manual-reset event
                                            FALSE, // flag for initial state
                                            NULL); // pointer to event-object name
  if(SER_hStatusMessageEvent == NULL)
     MessageBox( NULL, "LaunchSerialThread", "CreateEvent #1 failed.", MB_OK );


  // Create synchronization event for a "Single-Byte Transmission Request"
  if(SER_hTxRequestEvent == NULL)
       SER_hTxRequestEvent = CreateEvent(NULL, // pointer to security attributes
                                        FALSE, // flag for manual-reset event
                                        TRUE , // flag for initial state
                                        NULL); // pointer to event-object name
  if(SER_hTxRequestEvent == NULL)
       MessageBox( NULL, "LaunchSerialThread", "CreateEvent (TxRequest) failed.", MB_OK );

  // Create synchronization event for the "Transmission Complete Indication"
  if(SER_hTxCompleteEvent == NULL)
       SER_hTxCompleteEvent = CreateEvent(NULL,// pointer to security attributes
                                        FALSE, // flag for manual-reset event
                                        FALSE, // flag for initial state
                                        NULL); // pointer to event-object name
  if (SER_hTxCompleteEvent == NULL)
     MessageBox( NULL, "LaunchSerialThread", "CreateEvent (TxComplete) failed.", MB_OK );


  //
  // Create thread exit event
  //
  if(SER_hThreadExitEvent == NULL)
     SER_hThreadExitEvent = CreateEvent(NULL, // pointer to security attributes
                                        TRUE, // flag for manual-reset event
                                       FALSE, // flag for initial state
                                       NULL); // pointer to event-object name
   if(SER_hThreadExitEvent == NULL)
       MessageBox( NULL, "LaunchSerialThread", "CreateEvent (ThdExit) failed.", MB_OK );


    //
    // Create the worker thread
    //
    // From "Microsoft Win32 Programmer's Reference" ("Creating Threads").
    // > The CreateThread function creates a new thread for a process.
    // > The creating thread must specify the starting address of the code
    // > that the new thread is to execute. Typically, the starting address
    // > is the name of a function defined in the program code.
    // > This function takes a single parameter and returns a DWORD value.
    // > A process can have multiple threads simultaneously
    // > executing the same function.
    // > The following example demonstrates how to create a new thread
    // > that executes the locally defined function, ThreadFunc.
    if(SndThd_hWorkerThread==NULL)
     {
      SndThd_hWorkerThread = CreateThread(
         NULL,    // LPSECURITY_ATTRIBUTES lpThreadAttributes = pointer to thread security attributes
         0,       // DWORD dwStackSize  = initial thread stack size, in bytes
         SerInThdFunc, // LPTHREAD_START_ROUTINE lpStartAddress = pointer to thread function
         0,       // LPVOID lpParameter = argument for new thread
         0,       // DWORD dwCreationFlags = creation flags
                  // zero -> the thread runs immediately after creation
         &SndThd_dwWorkerThreadId // LPDWORD lpThreadId = pointer to returned thread id
       );
      // The thread object remains in the system until the thread has terminated
      // and all handles to it have been closed through a call to CloseHandle.
     }
    if (SndThd_hWorkerThread == NULL)   // Check the return value for success.
     {
      // ex: ErrorExit( "CreateThread failed." );
      return FALSE;
     }


    // Note: The program should immediately get here,
    //       though the thread is still running.
    //       Closing the Thread's handle does not seem to terminate the thread !


    // Define the Sound Thread's priority as required.
    SetThreadPriority(
       SndThd_hWorkerThread,          // handle to the thread
       THREAD_PRIORITY_ABOVE_NORMAL); // thread priority level

  return TRUE;
} // end .._LaunchWorkerThread()


/***************************************************************************/
BOOL SoundThd_TerminateAndDeleteThread(void)
   /* Terminates the sound thread, cleans up
    * and deletes(!) the sound thread.
    */
{
  int i;
  DWORD dwExitCode;
  BOOL  fOk = TRUE;

  if(SndThd_hWorkerThread)
   {
     // First, let the sound thread 'politely' terminate itself,
     //        allowing it to clean up everything.
     // About GetExitCodeThread(HANDLE hThread,LPDWORD lpExitCode):
     //       lpExitCode: Points to a 32-bit variable
     //          to receive the thread termination status.
     //       If the function succeeds, the return value is nonzero.
     //       If the specified thread has not terminated,
     //          the termination status returned is STILL_ACTIVE.
     dwExitCode=STILL_ACTIVE;
     SndThd_fTerminateRequest = TRUE;
     // Additionally, set thread exit event here to wake up the worker thread (newer style)
     if(SER_hThreadExitEvent != NULL)
      {
        SetEvent(SER_hThreadExitEvent);  // signal for the worker thread to terminate itself
        if(WaitForSingleObject( SER_hThreadExitEvent, 100/*milliseconds*/ ) == WAIT_FAILED)
           fOk = FALSE;   // set breakpoint here for further examination !
      }

     for(i=0;
        (i<20) && (GetExitCodeThread(SndThd_hWorkerThread,&dwExitCode));
         ++i)
      {
        // MessageBeep(-1);  // drums please
        Sleep(50);  // wait a little longer for the thread to terminate itself
        if(dwExitCode==SNDTHD_EXIT_OK)
           break;   // ufb, 'EXIT_OK' is what we wanted
      } // end for

     // Before deleting the sound thread...
     if(dwExitCode==STILL_ACTIVE)
      { // The thread does not terminate itself, so be less polite and kick it out !
        // DANGEROUS termination of the sound thread (may crash the system):
        TerminateThread(SndThd_hWorkerThread, // handle to the thread
                        SNDTHD_EXIT_BAD);     // exit code for the thread
      }

     CloseHandle(SndThd_hWorkerThread);
     SndThd_hWorkerThread = NULL;
     fOk &= (dwExitCode==SNDTHD_EXIT_OK);  // TRUE = polite ending,   FALSE = something went wrong.

     // Close status message event
     if(SER_hStatusMessageEvent != NULL)
        CloseHandle(SER_hStatusMessageEvent);
     SER_hStatusMessageEvent = NULL;

     // Close the "transmit request" event
     if(SER_hTxRequestEvent != NULL)
        CloseHandle(SER_hTxRequestEvent);
     SER_hTxRequestEvent = NULL;

     // Close the "Transmission Complete" event
     if(SER_hTxCompleteEvent == NULL)
        CloseHandle(SER_hTxCompleteEvent);
     SER_hTxCompleteEvent = NULL;

     // Reset and Close the thread exit event
     if(SER_hThreadExitEvent != NULL)
      {
        ResetEvent(SER_hThreadExitEvent);
        CloseHandle(SER_hThreadExitEvent);
      }
     SER_hThreadExitEvent = NULL;



   } // end if(SndThd_hWorkerThread)


   // if still opened,   close the serial port
   if (SerWin_CommHandle != INVALID_HANDLE_VALUE)
     {
       CloseHandle(SerWin_CommHandle);
       SerWin_CommHandle = INVALID_HANDLE_VALUE;
     }


   if( SerWin_fInitialized )
     {
#if(SER_WIN_USE_OVERLAPPED_IO>0)
       if(SerWin_osWrite.hEvent != NULL)
        { CloseHandle(SerWin_osWrite.hEvent);
          SerWin_osWrite.hEvent = NULL;  // forget the handle after closing it
        }
#endif // SER_WIN_USE_OVERLAPPED_IO

       DeleteCriticalSection(&SerWin_CriticalSection); // release resources
     } // end if( SerWin_fInitialized )

   SerWin_fInitialized = FALSE;


  return fOk;
} // end ..._TerminateAndDeleteThread()
