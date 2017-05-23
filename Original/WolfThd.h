/***************************************************************************/
/* WolfThd.h  =  Header file for several "WOLF worker threads"             */
/*   - based on DL4YHF's "audio utilities" which process audio samples     */
/*     from the soundcard.                                                 */
/*   - by DL4YHF,  April 2005                                              */
/*   - NO BORLAND-SPECIFIC STUFF (like VCL) in this module ! ! !           */
/*                                                                         */
/***************************************************************************/


//---------------------------------------------------------------------------
#ifndef WolfThdH
#define WolfThdH

//---------------------------------------------------------------------------
#include <Classes.hpp>
//---------------------------------------------------------------------------

/* manually included INCLUDES here: */


//#include "Sound.h"     // Soundcard access helper
//#include "ErrorCodes.h"
//#include "SoundDec.h"    // Decimating buffer and a few tables, etc

/*-------------- constants, tables ---------------------------------------*/
#define WOLFTHD_EXIT_OK       1
#define WOLFTHD_EXIT_ERROR    2
#define WOLFTHD_STILL_ACTIVE  0x00000103L  /* same value as STATUS_PENDING in winnt.h ! */

#ifndef  PI
 #define PI 3.14159265358979323846
#endif

// Possible "connections" for SAVED samples:
#define SOUND_SAVE_OFF        0 // do NOT save anything
#define SOUND_SAVE_INPUT      1 // save INPUT samples
#define SOUND_SAVE_OUTPUT     2 // save OUTPUT samples
#define SOUND_SAVE_IQ_SAMPLES 3 // save decimated I/Q samples from FFT input

// states of the 'sound save buffers' (see T_SOUND_SAVE_BUFFER)
#define SOUND_SAVEBUF_EMPTY   0 // signal from main application to sound thread
#define SOUND_SAVEBUF_FILLING 1
#define SOUND_SAVEBUF_FILLED  2 // signal from sound thread to main application

// ID's for the WOLF WORKER THREADS:
#define WOLFTHD_WAVE_ANALYZER 0  // Thread to analyze a WAVE file
#define WOLFTHD_WAVE_PRODUCER 1  // Thread to produce a WAVE file
#define WOLFTHD_AUDIO_RX 2 // Thread to read samples from an audio device and analyze them in real time
#define WOLFTHD_AUDIO_TX_LOOP 3 // Thread to send samples to an audio device in a loop


#ifndef  K_PI
 #define K_PI  (3.1415926535897932384626)
#endif


/*----------------------- Data Types --------------------------------------*/



/*--------------- Declaration of the Sound Analyzer Thread ----------------*/




//---------------------------------------------------------------------------


/*----------------------- Variables  --------------------------------------*/
#undef EXTERN
#ifdef _I_AM_WOLF_THREAD_
 #define EXTERN
#else
 #define EXTERN extern
#endif


// Audio buffer used to read samples from the input (or send to output) .
// Buffer size estimation: The WOLF DECODER sometimes 'thinks' for up to
//  SIXTY seconds (15 seconds on a 1.7 GHZ P4, much more on 300 MHz K6),
//  so we need 60 seconds * 8000 samples/sec =~  480 kSamples in the buffer,
//  with some safety margin (to avoid loss of samples): ~512 k (4096*128) .
// BUT: Because this buffer is also used for TRANSMISSION,
//   it must be large for a complete WOLF "frame" :
//   8000 samples/second (SAMPR) * 960 bits (TXLENB) / 10 (SYMSEC) = 768 kSamples .
// Samples are read from the soundcard in chunks of 4096 samples, so :
#define WOLFTHD_AUDIO_CHUNK_SIZE       4096
// ex: #define WOLFTHD_AUDIO_BUFFER_SIZE   (300*WOLFTHD_AUDIO_CHUNK_SIZE)
// Since 2005-11, Slower WOLF modes are possible. So the buffer must now
//     be FOUR times larger than before (see SIGPARMS.H, SYMSEC=2 now )
// Since the sample rate may be 11025 Hz now, the buffer may have to be
// even larger: 11025 * 960 / 10(SYMSEC) = 258.4 * 4096, so using 300*4096 samples now
// Since 2005-11, we may need up to  11024*960/2SymPerSec  = 5.3 megasamples,
//  which sounds like a big waste of memory if we don't run WOLF at slow speed.
//  -> the AUDIO BUFFER SIZE is not fixed any longer, instead it's calculated
//     during run-time !
// ex: #define WOLFTHD_AUDIO_BUFFER_SIZE  (4*300*WOLFTHD_AUDIO_CHUNK_SIZE)
extern DWORD WolfThd_dwAudioBufferSize; // ex: WOLFTHD_AUDIO_BUFFER_SIZE
extern SHORT *WolfThd_pi16AudioBuffer;  // ex: [ WOLFTHD_AUDIO_BUFFER_SIZE + 4];
EXTERN DWORD WolfThd_dwAudioBufHeadIndex, WolfThd_dwAudioBufTailIndex;
EXTERN BOOL  AudioOutThd_fPlayLoop;   // TRUE : Play buffer contents in endless loop


// Some -unfortunately global- variables to let WOLF run in an extra thread :
EXTERN BOOL   WolfThd_fWolfCoreThdRunning; // Flag for "WOLF core thread running"
EXTERN BOOL   WolfThd_fAudioInThdRunning;  // Flag for "Audio input thread running"
EXTERN BOOL   WolfThd_fAudioOutThdRunning; // Flag for "Audio output thread running"
EXTERN BOOL   WolfThd_fTerminateRequest; // Flag for ALL(!) threads to terminate themselves
EXTERN HANDLE WolfThd_hWorkerThread;     // Win32 API handle for the WOLF worker thread
EXTERN DWORD  WolfThd_dwWorkerThreadId;  // Identifier for the worker thread
EXTERN HANDLE WolfThd_hAudioThread;      // Win32 API handle for the audio-collector thread
EXTERN DWORD  WolfThd_dwAudioThreadId;   // Identifier for the worker thread


#define WOLF_MAX_ARGS 16
EXTERN int   WolfThd_iArgCnt;        // number of arguments passed in command line, like 'argc'
EXTERN char *WolfThd_pszArgs[WOLF_MAX_ARGS];    // argument vector (array of pointers to C-strings), like 'argv'
EXTERN char  WolfThd_szArg[WOLF_MAX_ARGS][256]; // String memories for arguments passed to main()-function

EXTERN int WolfThd_InstanceNr;  // Instance counter.  Possible values:
             // 0 = "I am the FIRST instance and most likely a SERVER"
             // 1 = "I am the SECOND instance and most likely a CLIENT"
             // 2 = "I am the THIRD or any other instance" (which is bad!)
             // Some of SpecLab's child windows use this information
             //  to change their window title accordingly, therefore GLOBAL.
             //  The instance number is detected somewhere during
             //  the initialization of the main form (using a MUTEX).

EXTERN BOOL WolfThd_fBreakThread;  // signal to worker thread to terminate itself
EXTERN BOOL WolfThd_fExitedThread; // flag set when thread DID terminate

EXTERN DWORD WolfThd_dwAudioSamplesLost;  // ideally ALWAYS zero

  // Test/Debugging stuff:
extern long   WIO_i32RandomNumber;
extern int    WIO_iAddRxNoiseSwitch;
extern double WIO_dblAddRxNoiseLevel;


/*-----------------------------------------------------------------------*/


/* debugging stuff.  WARNING: Modified in a thread, be carefull--   */
EXTERN int   WolfThd_iErrorCode;


/*--------------- Old-fashioned "C"-style prototypes :-) ------------------*/


  // Prefix 'WolfThd_' : something to do with multi-threading ...
void  WolfThd_Init(void);
BOOL  WolfThd_LaunchWorkerThreads(int iWhichThread);
BOOL  WolfThd_TerminateAndDeleteThreads(void);
DWORD WolfThd_GetSoundcardErrorStatus(void);

  // Prefix 'WIO_'     : WOLF-I/O, may be called from WOLF.CPP ...
float WIO_GetAudioBufferUsagePercent(void);
long  WIO_GetMaxAudioBufferSize(void);
void  WIO_ClearAudioBuffer(void);
void  WIO_SendSampleToSoundcard(float fltSample);
float WIO_GetSampleFromSoundcard(void);


//--------------------------------------------------------------------------
// COM Port Access.
//  Here: Used to control the transmitter, and to ring the alarm bell
//  Called from the WOLF GUI (which controls RX / TX changeover)
//--------------------------------------------------------------------------
int  WIO_OpenComPort( int iComPortNr );
int  WIO_GetComCTS( void );
int  WIO_SetComRTS( int high );   // RequestToSend - best used as PTT (!)
int  WIO_SetComDTR( int high );   // DataTerminalReady - used for ALARM BELL
int  WIO_CloseComPort( void );

void  WIO_Exit(void);

#endif // #ifndef WolfThdH
