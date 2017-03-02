//-------------------------------------------------------------------------
// File  : wolfio.h
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


//-------------------------------------------------------------------------
//  Defines
//-------------------------------------------------------------------------

#ifndef _WINDEF_
 typedef int BOOL;  // otherwise defined in WINDEF.H
#endif

   // Transmitter / Receiver states ( WIO_iRxTxState, set by WOLF core ! )
#define WIO_STATE_OFF  0
#define WIO_STATE_RX   1  // receiving
#define WIO_STATE_TX   2  // transmitting (data, CW-ID, or tuning tone)
#define WIO_STATE_B4RX 3  // very busy during RX, for LED indicator in the GUI
#define WIO_STATE_B4TX 4  // very busy during TX, for LED indicator in the GUI

   // Transmitter sub-states (tx DATA, CW-ID, tuning tone -> WIO_iTxWhat )
#define WIO_TX_DATA    0
#define WIO_TX_CW_ID   1
#define WIO_TX_TUNE    2

   // Tuning scope modes ( -> WIO_iTuneScopeMode )
#define TSMODE_TUNING_SCOPE_OFF  0
#define TSMODE_INPUT_WATERFALL   1
#define TSMODE_INPUT_SPECTRUM    2
#define TSMODE_PLOT_DECODER_VARS 3

   // Prefilter options ( WIO_iPrefilterOptions ) :
#define WIO_PREFILTER_OFF        0
#define WIO_PREFILTER_ON         0x0001

   // Bitmasks for some "multi-consumer" flag variables
#define WIO_FLAG_4_WOLF_CORE     0x0001


//-------------------------------------------------------------------------
//  Variables
//-------------------------------------------------------------------------


// input- and output device settings...
extern char WolfThd_sz255SoundInputDevice[256];
extern char WolfThd_sz255SoundOutputDevice[256];

extern int    WIO_iRxTxState; // WIO_STATE_xxx
extern int    WIO_iTxWhat;    // WIO_TX_DATA ,  WIO_TX_CW_ID , or WIO_TX_TUNE
extern int    WIO_iQsoMode;   // 0=unlimited TX- (or RX-) period;  1=toggle between RX and TX

extern double WIO_dblCurrentTime; // Current time (for WOLF decoder), UNIX-like UTC-seconds ( fractional ! )
extern double WIO_dblStartTime;   // Start time (for WOLF decoder), UNIX-like UTC-seconds   ( fractional ! )

extern int   WIO_InstanceNr; // Instance counter. 0 = "i am the FIRST instance", etc
extern int   WIO_iComPortNr; // COM port number for RX/TX control. Must be set
                             // by GUI (possibly read from ini-file, etc )

extern int WIO_iExitCode;  // 0 = everything ok,  everything else: error, EXIT-code
extern BOOL WIO_fStopFlag; // FALSE = continue processing, TRUE=request to finish a.s.a.p.
extern int WIO_iRestartDecoderFlags;  // flags for up to 16 "consumers" to clear history buffers
extern int WIO_iUseSoundcard;  // 0 = no (use audio file),  1 = yes (use soundcard)
extern int WIO_iTuneScopeMode; // TSMODE_xxx
extern double WIO_dblTxSampleRate;       // audio sampling rate during TX (from an edit field, stored in the config)
extern double WIO_dblRxSampleRate;       // audio sampling rate during RX
extern double WIO_dblCurrSampleRate;     // real current sampling rate in Hertz
extern float WIO_fltInputLevelPercent;   // input level from soundcard, 0..100 %
extern double WIO_dblCurrCenterFreq;     // current center frequency in Hertz
extern float WIO_fltOutputAttenuation;   // output attenuation (dB)

       // To pass the received message text from the DECODER THREAD to the auto-RX/TX-machine :
extern char WIO_sz80RcvdMessage[84]; // received message text, only 15 chars for WOLF, but who knows...
extern int  WIO_iRcvdMessageCount;   // shall be incremented *AFTER* rcvd message has been set.


       // Optional FFT-based PRE-FILTER .   Just an experiment from 2005-11 !
extern int WIO_iPrefilterOptions;  // WIO_PREFILTER_xxx : 0=prefilter OFF,  1= yes (prefilter ON), ...
#ifdef FftFilterH   // DL4YHF's "FftFilter.h" included ?
 extern CFftFilter *WIO_pPrefilter; // one instance of DL4YHF's FFT-based FIR filter
#endif 


//-------------------------------------------------------------------------
//  Prototypes
//-------------------------------------------------------------------------

extern void WIO_printf(char * fmt, ... );
  /* Prints a formatted string (like printf) to the WOLF DECODER OUTPUT WINDOW.
   *  May use an internal buffer to make it thread-safe,
   *  if called from a worker thread one day !
   */

extern void WIO_exit(int status);
  /* Replacement for exit(), but gives control back to the (optional)
   *  user interface instead of terminating immediately.
   * For the console application, WIO_exit(N) does alsmost the same as exit(N)
   *  but may wait for a few seconds before closing the console window.
   */

extern BOOL WIO_MustTerminate(void);
  /* Crude solution for a POLITE thread-termination :
   *   TRUE means: "all subroutines return asap, but clean everything up" .
   */


extern int WIO_BreakOrExit(void);
  /* Checks if the WOLF processing loop should terminate itself,
   * either due to an ERROR (WIO_exit())  or a user break (ESCAPE key).
   * return value :   0 = do NOT break,     otherwise: clean up and return .
   */

extern int WIO_CheckSystemHealth(void);
  // For debugging purposes only. Called from WOLF.CPP and other modules.
  // Added 2005-04-23 to look for the "shotgun bug" which seemed to destroy
  // some global variables after switching RX-TX-TX .


extern int WIO_GetNearestNominalSampleRate( double dblTrueSampleRate );
  /* Selects the nearest "nominal" sampling rate for the soundcard .
   */

extern void WIO_SetAudioSampleRate( double dblTrueSampleRate );
  /* Sets the sampling rate for the soundcard .
   *  Input: "true" sampling rate, for example 7998.5 or 11024.4169 .
   */

extern float WIO_GetSampleFromSoundcard(void);
  /* Reads the next audio sample from the soundcard
   * (not directly, but through several stages of buffering).
   * Possibly implemented in WolfThd.cpp (where the thread handling takes place)
   */

extern void WIO_SendSampleToSoundcard(float fltSample);
  /*  Sends the next audio sample to the soundcard
   *  (not directly, but through several stages of buffering).
   */


extern void WIO_DoneFFT( double *pdblXr, double *pdblXi, int iPoints ,
                         float fltAudioCenterFreq );
  /* Called from WOLF worker thread when a new FFT has been calculated .
   * Used for the spectrum display in the GUI .
   */


// EOF < wolfio.h >