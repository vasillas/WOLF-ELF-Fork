/***************************************************************************/
/* WolfThd.cpp = Several "worker threads" for WOLF                         */
/*   - based on DL4YHF's "audio utilities" which process audio samples     */
/*     from the soundcard.                                                 */
/*   - by DL4YHF,  April 2005                                              */
/*   - NO BORLAND-SPECIFIC STUFF (like VCL) in this module ! ! !           */
/*                                                                         */
/***************************************************************************/

//---------------------------------------------------------------------------
#include <windows.h>   // dont use the VCL here !!
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#pragma hdrstop        // Borland-specific: no precompiled headers after this..

#include "Sound.h"      // CSound class, used to access the soundcard
#include "ErrorCodes.h" // some MMSYSTEM- and other error codes
#include "FftFilter.h"  // CFftFilter class, here used as an experimental PRE-FILTER 

#include "sigparms.h"  // WOLF Signal Parameters (once fixed, now variable)
#include "wolfio.h"    // WOLF input/output routines (general header, no VCL in it)
#include "wolf.h"      // WOLF core algorithms, options, etc

#define _I_AM_WOLF_THREAD_ 1  // for "single-source" variables of this module :
#include "WolfThd.h"

#ifdef __WIN32__
 #pragma warn -8071 // "conversion may lose significant digits" ... no thanks !
 #pragma warn -8004 // "iBreakPoint" assigned a value that is never used .. ok
#endif


/*-------------- Variables -----------------------------------------------*/

// input- and output device settings...
char WolfThd_sz255SoundInputDevice[256]  = "-1";  // -1 = "use default audio device"
char WolfThd_sz255SoundOutputDevice[256] = "-1";

int  WolfThd_nSamplesPerSec; // nominal sampling rate (not the "true, calibrated value"!)
int  WolfThd_nBitsPerSample; // ..per channel, usually 16 (also for stereo, etc)
int  WolfThd_nAudioChannels; // 1 or 2 channels (mono/stereo)

DWORD  WolfThd_dwAudioBufferSize=0;  // ex: WOLFTHD_AUDIO_BUFFER_SIZE
SHORT *WolfThd_pi16AudioBuffer=NULL; // ex: [ WOLFTHD_AUDIO_BUFFER_SIZE + 4];
double WIO_dblTxSampleRate;       // audio sampling rate during TX
double WIO_dblRxSampleRate;       // audio sampling rate during RX
double WIO_dblCurrSampleRate;     // current sampling rate in Hertz
double WIO_dblCurrCenterFreq;     // current center frequency in Hertz
float  WIO_fltOutputAttenuation;  // output attenuation (dB)
int    WIO_InstanceNr = 0; // Instance counter. 0 = "i am the FIRST instance", etc
double WIO_dblStartTime;   // Start time (for WOLF decoder), UNIX-like UTC-seconds, fractional !
double WIO_dblCurrentTime; // Current time (for WOLF decoder), UNIX-like UTC-seconds
int    WIO_iRestartDecoderFlags;  // flags for up to 16 "consumers" to clear history buffers

// An instance of the CSound class to access the soundcards...
CSound SoundDev;  // almost entirely used in a WORKER THREAD !

// Optional FFT-based PRE-FILTER .   Just an experiment from 2005-11 !
int WIO_iPrefilterOptions = 0;     // WIO_PREFILTER_xxx : 0=prefilter OFF,  1= yes (prefilter ON), ...
CFftFilter *WIO_pPrefilter = NULL; // one instance of DL4YHF's FFT-based FIR filter


// Some parameters which may be 'overwritten' by command line arguments.
//  Therefore, the names are common for several modules like SndInThd, SerInThd, etc.
int     SOUND_iNrOfAudioChannels= 2;
float SOUND_fltTestGeneratorFreq=0; // '0' means generator is disabled
float SOUND_fltGainFactor     = 1.00;
int     SOUND_iUseSignedValues  = 0;  // 0=no 1=yes (for PIC: default=0=unsigned)
long    SOUND_lDecimationRatio  = 1;
float SOUND_fltCenterFrequency= 0;
int     SOUND_fComplexOutput    = 0;     // 0=real, 1=complex output
float SOUND_fltAudioPeakValue[2] = {0,0}; // crude peak detector for display purposes
int     SOUND_iChunkSize = 2048;
int     SOUND_iDcReject   = 1;
float SOUND_fltDcOffset[2] = {0,0};
char    SOUND_sz80PortName[81] = "COM1";    // often overriden by command line !

  // Test/Debugging stuff:
long   WIO_i32RandomNumber = 12345;
int    WIO_iAddRxNoiseSwitch = 0;
double WIO_dblAddRxNoiseLevel= -50.0;


//--------------------------------------------------------------------------
void WolfThd_AdjustAudioBufferSizeIfRequired(void)
  // IN:  WIO_dblRxSampleRate, WIO_dblTxSampleRate, SigParms_dblSymPerSec .
  // OUT: WolfThd_pi16AudioBuffer, WolfThd_dwAudioBufferSize .
{
  double d;
  DWORD dwNewAudioBufferSize;
  double dblMaxAudioSampleRate;

  // Get audio sample rate for "worst-case" (largest value, for buffer size)
  dblMaxAudioSampleRate = WIO_dblTxSampleRate;
  if( WIO_dblRxSampleRate > dblMaxAudioSampleRate )
      dblMaxAudioSampleRate = WIO_dblRxSampleRate;
  if( dblMaxAudioSampleRate < 8000.0 ) // stay within rasonable limits..
      dblMaxAudioSampleRate = 8000.0;
  if( SigParms_dblSymPerSec < 1.0 )  // emergency brake to avoid div by zero
      SigParms_dblSymPerSec = 1.0;

  // Since 2005-11, Slower WOLF modes are possible. So the buffer must now
  //     be FOUR times larger than before (see SIGPARMS.H, SYMSEC=2 now )
  // Since the sample rate may be 11025 Hz now, the buffer may have to be
  // even larger: 11025 * 960 / 10(SYMSEC) = 258.4 * 4096, so using 300*4096 samples now
  // Since 2005-11, we may need up to  11024*960/2SymPerSec  = 5.3 megasamples,
  //  which sounds like a big waste of memory if we don't run WOLF at slow speed.
  //  -> the AUDIO BUFFER SIZE is not fixed any longer, instead it's calculated
  //     during run-time !
  // ex: #define WOLFTHD_AUDIO_BUFFER_SIZE  (4*300*WOLFTHD_AUDIO_CHUNK_SIZE)
  d =( dblMaxAudioSampleRate * (double)SYMLEN/* 960 symbols in frame */ )
     / SigParms_dblSymPerSec;
  dwNewAudioBufferSize = (DWORD)(d+1.0) + (DWORD)WOLFTHD_AUDIO_CHUNK_SIZE;
  // Add one chunk size for safety (the WOLF encoder may decide to produce
  //  a few more samples than we guess) :
  dwNewAudioBufferSize += WOLFTHD_AUDIO_CHUNK_SIZE;
  dwNewAudioBufferSize -= (dwNewAudioBufferSize % WOLFTHD_AUDIO_CHUNK_SIZE);
  // dwNewAudioBufferSize should be a multiple of WOLFTHD_AUDIO_CHUNK_SIZE.
  // Some examples (for WOLFTHD_AUDIO_CHUNK_SIZE=4096) :
  // 10   symbols/second, fSample=8000 Hz:   AudioBufferSize= 768000
  // 2.5  symbols/second, fSample=11025 Hz:  AudioBufferSize=4233600
  if( dwNewAudioBufferSize > WolfThd_dwAudioBufferSize )
   { if( WolfThd_pi16AudioBuffer != NULL )
      {  free( WolfThd_pi16AudioBuffer );
         WolfThd_pi16AudioBuffer = NULL;
      }
   } // end if < size of audio buffer must be increased >
  if( ( WolfThd_pi16AudioBuffer==NULL ) && ( dwNewAudioBufferSize>0 ) )
   {
     WolfThd_pi16AudioBuffer = (SHORT *)malloc( (dwNewAudioBufferSize+4) * sizeof(SHORT) );
     WolfThd_dwAudioBufferSize = dwNewAudioBufferSize;
     if( WolfThd_pi16AudioBuffer == NULL )
       exit(0);  // this should NEVER happen under WIN32 !
   }

} // end WolfThd_AdjustAudioBufferSize()


//--------------------------------------------------------------------------
void WolfThd_Init(void)
   // Must be called ONCE (- ONLY ONCE -) before calling any other subroutine of this module.
   // Sets a few things to their default values.
{
  WIO_iRestartDecoderFlags = 0;    // polled in WOLF.CPP (GUI variant only)
  WolfThd_dwAudioBufHeadIndex = WolfThd_dwAudioBufTailIndex = 0;
  WolfThd_dwAudioSamplesLost = 0;
  WolfThd_fTerminateRequest= FALSE; // here in WolfThd_Init()
  WolfThd_hWorkerThread    = NULL; // Win32 API handle for the worker thread
  WolfThd_dwWorkerThreadId = 0;    // Identifier for the worker thread
  WolfThd_hAudioThread     = NULL;
  WolfThd_dwAudioThreadId  = 0;
  WolfThd_fWolfCoreThdRunning = FALSE;
  WolfThd_fAudioInThdRunning  = FALSE;
  WolfThd_fAudioOutThdRunning = FALSE;

  // Preset some soundcard settings (WAVEFORMATEX) required for opening...
  WolfThd_nAudioChannels = 1;
  WolfThd_nBitsPerSample = 16;  // ALWAYS 16 BIT, EVEN WHEN EXPORTING 8-BIT-VALUES(!!)
  WolfThd_nSamplesPerSec = SOUND_GetClosestNominalStandardSamplingRate( WIO_dblTxSampleRate );

  // No optional PRE-FILTER allocated yet . This was just an experiment from 2005-11
  WIO_pPrefilter = NULL;


  WolfThd_AdjustAudioBufferSizeIfRequired();  // since 2005-11, the audio buffer size is no longer "fixed"


} // end WolfThd_Init()

//--------------------------------------------------------------------------
int WIO_CheckSystemHealth(void)
  // For debugging purposes only. Called from WOLF.CPP and other modules.
  // Added 2005-04-23 to trace down the "shotgun bug", which seemed to destroy
  // "some" global variables after switching RX-TX-TX .
{
  static int iTrapCount = 0;

  if( iTrapCount>0 )
     return FALSE;

  if( WolfThd_dwAudioBufHeadIndex > WolfThd_dwAudioBufferSize )
   { WolfThd_dwAudioBufHeadIndex = 0;
     ++iTrapCount;
     return FALSE; // SET BREAKPOINT HERE !
     // (trapped 2005-04-23, after RX-TX-RX, caller:rcvf()  )
   }

  if( WolfThd_dwAudioBufTailIndex >= WolfThd_dwAudioBufferSize )
   { WolfThd_dwAudioBufTailIndex = 0;
     ++iTrapCount;
     return FALSE; // SET BREAKPOINT HERE !
   }
  return TRUE;
} // end WIO_CheckSystemHealth()


//--------------------------------------------------------------------------
DWORD WolfThd_GetSoundcardErrorStatus(void)   // see ErrorCodes.h for possible return values
{
  if( SoundDev.m_ErrorCodeIn )
     return SoundDev.m_ErrorCodeIn;
  return 0;
}

//--------------------------------------------------------------------------
float WIO_GetSampleFromSoundcard(void)
  /* Called from WOLF.CPP .
   *  Reads the next audio sample from the soundcard
   *  (not directly, but through several stages of buffering).
   * BLOCKS THE CALLING THREAD until samples are available !
   */
{
  int emgcy_brake = 0;
  float fltResult, fltRectified;

  if( ! WolfThd_pi16AudioBuffer )
    return 0.0;     // buffer allocation failed, can only return a "dummy" :(

  // Avoid illegal buffer index ..
  if( WolfThd_dwAudioBufTailIndex >= WolfThd_dwAudioBufferSize )
      WolfThd_dwAudioBufTailIndex = 0;

  // If necessary, wait until samples are available in the audio buffer.
  //  As long as HEAD INDEX == TAIL INDEX,  the buffer is empty,
  //  which means the caller is 'fast enough' and we can give the CPU
  //  to another thread (or even another task) here :
  while( WolfThd_dwAudioBufTailIndex == WolfThd_dwAudioBufHeadIndex )
   {  // no samples are available in the buffer ..
      if( WolfThd_fTerminateRequest )
        return 0.0;  // don't block the caller, HE SHALL TERMINATE a.s.a.p. !
      Sleep(100);   // WAIT for 100 milliseconds, then try again..
      if( ++emgcy_brake > 20 )
       { // waited 2 seconds for audio samples, but nothing happened..
         return 0.0;  // give up, there must be something wrong with the soundcard
       }
   }

  // Arrived here: At least one audio sample is available in the buffer ...
  if( WolfThd_dwAudioBufTailIndex >= WolfThd_dwAudioBufferSize )
      WolfThd_dwAudioBufTailIndex = 0; // buffer wrap on 'get from buffer' (tail)
  fltResult = WolfThd_pi16AudioBuffer[ WolfThd_dwAudioBufTailIndex++ ];


  // Peak-detecting input-level detector ...
  //  ( broadband, used as soundcard OVERLOAD indicator )
  fltRectified = fltResult>=0 ? fltResult : -fltResult; // no "fabs" - this is faster
  fltRectified *= 100.0/32767.0;
  if( fltRectified > WIO_fltInputLevelPercent )
      WIO_fltInputLevelPercent = fltRectified;
  else
      WIO_fltInputLevelPercent = 0.99 * WIO_fltInputLevelPercent + 0.01 * fltRectified;

  return fltResult;

} // end WIO_GetSampleFromSoundcard()

//--------------------------------------------------------------------------
void WIO_SendSampleToSoundcard(float fltSample)
  /* Called from WOLF.CPP .
   *  Sends the next audio sample to the soundcard
   *  ( or, more precisely, puts the sample in a buffer
   *    from which it will be sent to the soundcard some time later ).
   */
{
 float fltRectified;

  if( ! WolfThd_pi16AudioBuffer )
    return;     // buffer allocation failed, avoid access violation


  if( WolfThd_dwAudioBufHeadIndex >= WolfThd_dwAudioBufferSize )
      WolfThd_dwAudioBufHeadIndex = 0;
  WolfThd_pi16AudioBuffer[ WolfThd_dwAudioBufHeadIndex++ ] = fltSample;
  if( WolfThd_dwAudioBufHeadIndex >= WolfThd_dwAudioBufferSize )
      WolfThd_dwAudioBufHeadIndex = 0;  // set breakpoint here; shouldn't trap

  // Peak-detecting input-level detector ...
  //  ( broadband, used as soundcard OVERLOAD indicator )
  fltRectified = fltSample>=0 ? fltSample : -fltSample; // no "fabs" - this is faster
  fltRectified *= 100.0/32767.0;
  if( fltRectified > WIO_fltInputLevelPercent )
      WIO_fltInputLevelPercent = fltRectified;
  else
      WIO_fltInputLevelPercent = 0.99 * WIO_fltInputLevelPercent + 0.01 * fltRectified;

} // end WIO_SendSampleToSoundcard()

//--------------------------------------------------------------------------
void WIO_ClearAudioBuffer(void)
{
  WolfThd_AdjustAudioBufferSizeIfRequired();  // f(WIO_dblRxSampleRate,WIO_dblTxSampleRate,SigParms_dblSymPerSec)
  WolfThd_dwAudioBufTailIndex = WolfThd_dwAudioBufHeadIndex = 0; // empty buffer
  WolfThd_dwAudioSamplesLost = 0;
}

//--------------------------------------------------------------------------
long WIO_GetMaxAudioBufferSize(void)
{
  return  WolfThd_dwAudioBufferSize;  // may have to be VARIABLE one day..
}

//--------------------------------------------------------------------------
float WIO_GetAudioBufferUsagePercent(void)
   // Detects the current usage in the audio input buffer.
   // May be used one day by the worker thread to decide if the CPU is too slow,
   // and to leave some time-consuming calculations away to prevent buffer overflow.
   // Ideally, the buffer is as empty as possible ( < 10 % ) .
   // If the buffer usage is above 50% , the CPU is too slow .
{
  float f = (float)WolfThd_dwAudioBufHeadIndex - (float)WolfThd_dwAudioBufTailIndex;
  if( f < 0 )
      f += WolfThd_dwAudioBufferSize;
  if( WolfThd_dwAudioBufferSize > 0 )
       f = (100.0 * f) / (float)WolfThd_dwAudioBufferSize;
  else f = 0.0;  // no buffer, no "usage" :-)
  return f;
} // end WIO_GetAudioBufferUsagePercent()


//--------------------------------------------------------------------------
int WIO_GetNearestNominalSampleRate( double dblTrueSampleRate )
{
  return SOUND_GetClosestNominalStandardSamplingRate( dblTrueSampleRate );
} // end WIO_GetNearestNominalSampleRate()

//--------------------------------------------------------------------------
void WIO_SetAudioSampleRate( double dblTrueSampleRate )
{
  WIO_dblCurrSampleRate  = dblTrueSampleRate;
  WolfThd_nSamplesPerSec = WIO_GetNearestNominalSampleRate(dblTrueSampleRate);
} // end WIO_SetAudioSampleRate()



/***************************************************************************/
void WIO_UpdatePrefilterParams(void)
  // Creates and initializes an FFT-base FIR filter,
  //   which may be used as an optional pre-filter in front of the decoder.
  //   This was just an experiment from 2005-11 .
  // Inputs: WIO_dblCurrCenterFreq, WIO_dblCurrSampleRate,
  //         SigParms_dblSymPerSec,  (?) ..
{
  int i, iSlope;
  int iBinIndexOfFC;   // FFT bin index for center frequency
  int iBinCountOfHBW;  // count of FFT bins for half bandwidth
  int iBinCountOfHSW;  // count of FFT bins for half slope width (transition bandwidth)
  int iResponseLength = 16385;  // compromise for average PC CPU speed, should be (2^n + 1)
  float flt;
  float *pfltResponse;


//if( WIO_iPrefilterOptions & WIO_PREFILTER_ON )
// {
    if( ! WIO_pPrefilter ) // No optional PRE-FILTER allocated yet ? Create such an object
     { WIO_pPrefilter = new( CFftFilter ); // Create one instance of DL4YHF's FFT-based FIR filter
     }
// } // end if < pre-filter enabled >
//else // pre-filter not enabled, free resources..
// {
//  if( WIO_pPrefilter )  // prefilter had been in use; kick it out now..
//   { delete WIO_pPrefilter;
//     WIO_pPrefilter = NULL;
//   }
// }

  pfltResponse = (float*)malloc( sizeof(float) * iResponseLength );

  // Update the FFT-based FIR "pre-filter" for the current WOLF settings:
  if( WIO_pPrefilter && pfltResponse && ( WIO_dblCurrSampleRate>0 ) )
   {
     iBinIndexOfFC  = 2.0 * (double)iResponseLength * WIO_dblCurrCenterFreq / WIO_dblCurrSampleRate;
     iBinCountOfHBW = (double)iResponseLength * SigParms_dblSymPerSec * 1.4 / WIO_dblCurrSampleRate;
     iBinCountOfHSW = (double)iResponseLength * SigParms_dblSymPerSec * 0.5 / WIO_dblCurrSampleRate;

     // Create frequency response for a BANDPASS suitable as a pre-filter ...
     for(i=0; i<iResponseLength; ++i)
      { flt = 0.0;
        if(i<iBinIndexOfFC)
         { iSlope = i - (iBinIndexOfFC-iBinCountOfHBW);
           if(i>iBinIndexOfFC-iBinCountOfHBW)
              flt = 1.0;
         }
        else
         { iSlope = (iBinIndexOfFC+iBinCountOfHBW) - i;
           if(i<iBinIndexOfFC+iBinCountOfHBW)
              flt = 1.0;
         }
        if( iBinCountOfHSW>0 && iSlope>=-iBinCountOfHSW && iSlope<=iBinCountOfHSW )
         { // inside the slope ... a simple cos function
           flt = 0.5 * (1.0+sin( C_PI*0.5*iSlope / (float)iBinCountOfHSW) );
         }
        pfltResponse[i] = flt;
      } // end for

     WIO_pPrefilter->SetRealFrequencyResponse(
        pfltResponse,      // float *pfltResponse, contains frequency response of the filter
        iResponseLength);  // int  iFreqResponseLength = length of pfltResponse
     WIO_pPrefilter->SetOptions( C_FftFilt_OPTIONS_NONE );  // no special options (yet)

#if(0) // something for the future... limit all FFT bins to certain thresholds
     WIO_pPrefilter->SetThresholdValues(   // set limiter thresholds..
        pParams->fltLimitThreshold, // one threshold value per FFT bin (!)
        pParams->iResponseLength ); // length of pfltThresholdValues[]
#endif

   } // end  if( WIO_pPrefilter )

  free ( pfltResponse );

} // end WIO_UpdatePrefilterParams()




//--------------------------------------------------------------------------
// COM Port Access.
//  Here: Used to control the transmitter, and to ring the alarm bell
//--------------------------------------------------------------------------

 /*var*/ int     WIO_iComPortNr = 0;  // must be set by GUI (read from ini-file)
         HANDLE  WIO_hComPort = INVALID_HANDLE_VALUE;

/***************************************************************************/
int WIO_CloseComPort(void)
{
  DCB dcb;
 if( WIO_hComPort != INVALID_HANDLE_VALUE )
  {
    dcb.DCBlength = sizeof( DCB );
    GetCommState( WIO_hComPort, &dcb );
    dcb.fDtrControl = DTR_CONTROL_DISABLE;
    dcb.fRtsControl = RTS_CONTROL_DISABLE;
    SetCommState( WIO_hComPort, &dcb );
    CloseHandle( WIO_hComPort );
    WIO_hComPort = INVALID_HANDLE_VALUE;
    return true;
  }
 return false;
}

/***************************************************************************/
int WIO_OpenComPort( int iComPortNr )
  /* Opens the serial interface and keeps it 'occupied'
   * for this application.
   *  Input parameters:  COM port number, valid: 1 to 8 .
   */
{
 char szPort[10];

 if( WIO_hComPort != INVALID_HANDLE_VALUE )
  { // if a COM-port has already been opened; close it (may be different now)
    WIO_CloseComPort();
  }

 // if not yet done, open COM port:
 if( WIO_hComPort == INVALID_HANDLE_VALUE )
   {
     if( (iComPortNr>=1) && (iComPortNr<=8) )
      {
       wsprintf( szPort, "COM%d", iComPortNr );
       WIO_hComPort = CreateFile( szPort, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
      }
     else // PTT control not on a serial port:
      {

      }
   }

 return (WIO_hComPort != INVALID_HANDLE_VALUE);
}


/***************************************************************************/
int WIO_GetComCTS(void)
{
  return 0;  // not required yet :)
} // end WIO_GetComCTS()


/***************************************************************************/
int WIO_SetComRTS( int high )
{
 // A "well-behaving" (but slow) way to set the signal level on RTS :
 DCB dcb;
 if( WIO_hComPort == INVALID_HANDLE_VALUE )
   return false;
 dcb.DCBlength = sizeof( DCB );
 if (! GetCommState( WIO_hComPort, &dcb ) )
   return false;
 if( high  )   dcb.fRtsControl = RTS_CONTROL_ENABLE;
       else    dcb.fRtsControl = RTS_CONTROL_DISABLE;
 return SetCommState( WIO_hComPort, &dcb );

} // end WIO_SetComRTS()


/***************************************************************************/
int WIO_SetComDTR( int high )
{
 // A "well-behaving" (but slow) way to set the signal level on DTR :
 DCB dcb;
 if( WIO_hComPort == INVALID_HANDLE_VALUE )
   return false;
 dcb.DCBlength = sizeof( DCB );
 if (! GetCommState( WIO_hComPort, &dcb ) )
   return false;
 if( high  )    dcb.fDtrControl = DTR_CONTROL_ENABLE;
        else    dcb.fDtrControl = DTR_CONTROL_DISABLE;
 return SetCommState( WIO_hComPort, &dcb );
} // end WIO_SetComDTR()


void WIO_AddNoiseToChunk( double dblNoiseLevel, /* dB relative to "clipping" point */
                          short *pi16Samples, int iNrOfSamples )
{
  float fltNoiseFactor=
        pow(10.0,dblNoiseLevel/20.0)  // convert dB -> "factor"
        * sqrt(WIO_dblRxSampleRate)   // consider bandwidth
        * 1.41E-5;  // correction factor found out by crude trial and error
  while( iNrOfSamples-- )
   {
     // random number generator taken from the beautiful old Atari ST :
     WIO_i32RandomNumber = (WIO_i32RandomNumber * 3141592621) + 1;
     *pi16Samples += fltNoiseFactor * (float)WIO_i32RandomNumber;
     ++pi16Samples;
   }
} // end WIO_AddNoiseToChunk()


static int OpenSoundcardForInput(void)
{
  return SoundDev.InOpen(
       WolfThd_sz255SoundInputDevice, // [in] pszAudioInputDeviceOrDriver, name of an audio device or DL4YHF's "Audio I/O driver DLL" (full path)
       "",  // [in] pszAudioInputDriverParamStr, extra parameter string for driver or Audio-I/O / ExtIO-DLL
       "",  // [in] pszAudioStreamID,  'Stream ID' for the Audio-I/O-DLL, identifies the AUDIO SOURCE (here: optional)
       "Wolfie", // char *pszDescription, a descriptive name which identifies
             //   the audio sink ("audio reader", or "consumer"). This name may appear
             //   on a kind of "patch panel" on the configuration screen of the Audio-I/O DLL .
             //   For example, Spectrum Lab will identify itself as "SpectrumLab1"
             //   for the first running instance, "SpectrumLab2" for the 2nd, etc .
        SOUND_GetClosestNominalStandardSamplingRate( WIO_dblRxSampleRate ), // [in] nominal sampling rate (not the "true, calibrated value"!)
        WolfThd_nBitsPerSample,    // [in] BitsPerSample, per channel, usually 16 (also for stereo, etc)
        WolfThd_nAudioChannels,    // [in] NumberOfChannels, 1 or 2 channels (mono/stereo)
        WOLFTHD_AUDIO_CHUNK_SIZE,  // DWORD BufSize
        0,       // sample limit, 0 signifies continuous input stream
        FALSE ); // don't start immediately

} // end OpenSoundcardForInput()



//--------------------------------------------------------------------------
//  Thread handling .
//   Using Borland's TThread (etc) would make things easier,
//   but DL4YHF decided to use as little Borland-specific stuff as possibe.
//   All thread handling uses 'pure Windows API routines' to simplify
//   migration from BORLAND C++BUILDER to other compilers .
//--------------------------------------------------------------------------

/*--------- Implementation of SOUNDCARD-handling threads ------------------*/

//---------------------------------------------------------------------------
DWORD WINAPI AudioInThdFunc( LPVOID )
  /* Collects audio samples from the soundcard and places them in a buffer,
   *    from which they can be read by 'WOLF' for processing .
   *
   * NO-VCL SPECIFIC STUFF IN HERE, no calls of the user interface,
   * no dynamic memory handling (see notes about "small memory leaks"
   * in the description of ExitThread() in Win32 programmer's reference ! ).
   *
   * Note: this thread is COMPLETELY SEPARATED FROM THE WOLF DECODER THREAD !
   *       (the audio-input-thread must actually have a HIGHER PRIORITY
   *        than the WOLF decoder, to interrupt it whenever necessary)
   */
{
 int i, error_code;
 T_ChunkInfo ChunkInfo;


  ChunkInfo_Init( &ChunkInfo );

  WolfThd_fAudioInThdRunning  = TRUE ;

  // Select the nearest "nominal" sampling rate for the soundcard ...
  WIO_SetAudioSampleRate( WIO_dblCurrSampleRate );

  // Open Sound Device for input
  error_code = OpenSoundcardForInput(); // here: called from the audio-input-thread !
  if( error_code != NO_ERROR )
   { // Failed to open the 'soundcard' (or similar) for input .....
     ExitThread( WOLFTHD_EXIT_ERROR ); // exit code for this thread
   }

  SoundDev.Start();      // start input from audio device (soundcard)

  // Remove old audio samples from the buffer :
  WolfThd_dwAudioBufTailIndex = WolfThd_dwAudioBufHeadIndex = WolfThd_dwAudioSamplesLost = 0;

  // AUDIO INPUT thread loop:
  //  - Wait for samples from soundcard (in chunks of XXXX samples)
  //  - Place the samples in a circular buffer
  //  - Leave thread loop when signalled via flag
  while( ! WolfThd_fTerminateRequest )
   {

     ChunkInfo.dwNrOfSamplePoints  = WOLFTHD_AUDIO_CHUNK_SIZE;
     ChunkInfo.nChannelsPerSample  = SOUND_iNrOfAudioChannels; // same as for ....InOpen
     ChunkInfo.dblPrecSamplingRate = WIO_dblCurrSampleRate;


     if( (!WolfThd_pi16AudioBuffer) || (WolfThd_dwAudioBufferSize<=0) )
      { // oops... the audio sample buffer has not been allocated !
        Sleep(100);
      }
     else
     if( SoundDev.IsInputOpen() )
      {
       // Get input data from soundcard object.
       if( SoundDev.InReadInt16(
            &WolfThd_pi16AudioBuffer[ WolfThd_dwAudioBufHeadIndex ] ,
             WOLFTHD_AUDIO_CHUNK_SIZE ) != WOLFTHD_AUDIO_CHUNK_SIZE )
        {
         error_code = SoundDev.m_ErrorCodeIn;
         switch(error_code)
          {
          /* waveform audio error return values from MMSYSTEM:    */
          case WAVERR_BADFORMAT   :   /* unsupported wave format  */
          case WAVERR_STILLPLAYING:   /* still something playing  */
          case WAVERR_UNPREPARED  :   /* header not prepared      */
          case WAVERR_SYNC        :   /* device is synchronous    */
            //SoundDev.InClose();  // what an ugly solution: close & open again..
            //error_code=SoundDev.InOpen(..
            break;
          /* error codes from ErrorCodes.H :                      */
          case NO_ERRORS:
            break;     // hmmm... so why did InRead() fail ??
          case SOUNDIN_ERR_NOTOPEN:
             // someone must have closed the soundcard's input ?!?!?
            OpenSoundcardForInput();  // open it again (same params)
            SoundDev.Start(); // start input from audio device (soundcard)
            break;
          case SOUNDIN_ERR_OVERFLOW:
          case SOUNDIN_ERR_TIMEOUT:
            // CPU couldn't keep up so try again: Close and re-open Sound Device.
            // Note: this should NEVER happen in this simple high-priority thread,
            //       but it happens (especially if the harddisk falls asleep..)
            SoundDev.InClose();
            OpenSoundcardForInput();  // open it again (same params)
            SoundDev.Start(); // start input from audio device (soundcard)
            break;
          default:
            // don't know how to handle this error, set breakpoint here:
            break;
          } // end switch(error_code)
        } /* end if (..InRead(..) != ..BUF_SIZE) */
       else
        { /* Successfully read audio samples from soundcard : */
         long i32FreeBufferSpace;
         i32FreeBufferSpace = (long)WolfThd_dwAudioBufferSize - (long)WolfThd_dwAudioBufHeadIndex + (long)WolfThd_dwAudioBufTailIndex - 1;
         if( i32FreeBufferSpace < 0 )
             i32FreeBufferSpace += WolfThd_dwAudioBufferSize;
         if( i32FreeBufferSpace < WOLFTHD_AUDIO_CHUNK_SIZE )
          { // bad luck, the large audio buffer just wasn't big enough !
            WolfThd_dwAudioSamplesLost += WOLFTHD_AUDIO_CHUNK_SIZE;
          }
         else
          { // enough space in buffer -> do some optional processing on the input,
            // then advance the buffer HEAD index .

            if( WIO_iAddRxNoiseSwitch ) // add artificial noise to the digitized input ?
             {
               WIO_AddNoiseToChunk( WIO_dblAddRxNoiseLevel,
                  &WolfThd_pi16AudioBuffer[ WolfThd_dwAudioBufHeadIndex ],
                  WOLFTHD_AUDIO_CHUNK_SIZE );
             } // end if( WIO_iAddRxNoiseSwitch )

            if( (WIO_pPrefilter!=NULL) && (WIO_iPrefilterOptions & WIO_PREFILTER_ON) )
             {  // use the experimental PRE-FILTER (really just an experiment,
                // besides that it's relatively foolish to run the audio samples
                // through the pre-filter at this early stage . )
                T_Float fltChunk[WOLFTHD_AUDIO_CHUNK_SIZE];
                SHORT *pi16 = &WolfThd_pi16AudioBuffer[WolfThd_dwAudioBufHeadIndex];
                for(i=0; i<WOLFTHD_AUDIO_CHUNK_SIZE; ++i)  // INT16 -> T_Float
                   fltChunk[i] = (T_Float)*(pi16++);
                WIO_pPrefilter->ProcessSamples(
                    fltChunk, // float *pfltSource, real INPUT (or I-component) in the time domain
                    NULL,     // float *pfltSourceQ, quadrature component for input (optional)
                    fltChunk, // float *pfltDest,  real OUTPUT (or I-component) in the time domain
                    NULL,     // float *pfltDestQ, quadrature component for output (optional)
                    &ChunkInfo );  //  number of samples, etc, etc, etc, etc
                pi16 = &WolfThd_pi16AudioBuffer[WolfThd_dwAudioBufHeadIndex];
                for(i=0; i<WOLFTHD_AUDIO_CHUNK_SIZE; ++i)  // T_Float -> INT16
                   *(pi16++) = fltChunk[i];
             } // end if < run audio samples through the prefilter ON INPUT ? >


            WolfThd_dwAudioBufHeadIndex += WOLFTHD_AUDIO_CHUNK_SIZE;
            if( WolfThd_dwAudioBufHeadIndex >= WolfThd_dwAudioBufferSize )
                WolfThd_dwAudioBufHeadIndex = 0; // buffer wrap on 'input' (head)
            // Now WIO_GetSampleFromSoundcard() may be called
            //  to read another sample from this large audio buffer.
          }

         error_code = SoundDev.m_ErrorCodeIn; // hmm..
         if (error_code!=NO_ERRORS) // reading ok, but something is wrong ?!
          { SoundDev.m_ErrorCodeIn = NO_ERRORS;  // ouch !
          }
        } /* end if <sampling of input data ok> */
      } // end if( SoundDev.IsInputOpen() )
     else // soundcard NOT open for input ...
      {   // Re-Open it for input !
        error_code = OpenSoundcardForInput();
        if( error_code != NO_ERROR )
         {
           ExitThread( WOLFTHD_EXIT_ERROR ); // exit code for this thread
         }
      } // end if <sound device NOT open for input (?!)
   } // end while < almost endless thread-loop >

  // Close the soundcard for input, we don't read from it any longer !
  SoundDev.InClose();

  WolfThd_fAudioInThdRunning  = FALSE;

  return WOLFTHD_EXIT_OK; // set exit code for this thread
  // ( returning from a thread results in an implicit call of ExitThread() )

} // end AudioInThdFunc()


//---------------------------------------------------------------------------
static int OpenSoundcardForOutput(void)
{
  // Open Sound Device for output
  return SoundDev.OutOpen(
     WolfThd_sz255SoundOutputDevice,// [in] pszAudioOutputDeviceOrDriver, name of an audio device or DL4YHF's "Audio I/O driver DLL" (full path)
     "",  // [in] pszAudioStreamID, 'Stream ID' for the Audio-I/O-DLL, identifies the AUDIO SOURCE (here: mandatory)
     "",  // [in] pszDescription,  a descriptive name which identifies
           // the audio source ("audio producer"). This name may appear
           // on a kind of "patch panel" on the configuration screen of the Audio-I/O DLL .
           // For example, Spectrum Lab will identify itself as "SpectrumLab1"
           // for the first running instance, "SpectrumLab2" for the 2nd, etc .
     WolfThd_nSamplesPerSec, // [in] i32SamplesPerSecond, nominal sampling rate (not the "true, calibrated value"!)
     WolfThd_nBitsPerSample, // ..per channel, usually 16 (also for stereo, etc)
     WolfThd_nAudioChannels, // 1 or 2 channels (mono/stereo)
     WOLFTHD_AUDIO_CHUNK_SIZE,           // output buffer size; see caller in SoundThd.cpp
     0 ); // sample limit, 0 signifies continuous input stream
} // end OpenSoundcardForOutput(void)

//---------------------------------------------------------------------------
DWORD WINAPI AudioOutThdFunc( LPVOID )
  /* Sends audio samples from a buffer to the soundcard .
   *  Must have higher priority than the rest to avoid dropouts in the audio .
   *  See notes in 'AudioInThdFunc' too !
   * If AudioOutThd_fPlayLoop is TRUE ,
   *  the data in WolfThd_pi16AudioBuffer[ 0 ] ..
   *           to WolfThd_pi16AudioBuffer[ WolfThd_dwAudioBufHeadIndex-1 ]
   *       are repeated over and over .
   *  The current writing index is always WolfThd_dwAudioBufTailIndex .
   */
{
 int i, error_code, ms_waited;
 SHORT i16TxChunk[ WOLFTHD_AUDIO_CHUNK_SIZE ];

  WolfThd_fAudioOutThdRunning  = TRUE ;

  error_code = OpenSoundcardForOutput();
  if( error_code != NO_ERROR )
   {
     WolfThd_fAudioOutThdRunning  = FALSE;
     return WOLFTHD_EXIT_ERROR; // exit code for this thread
   }


  SoundDev.Start();   // start output ...

  // Here: do *NOT* remove samples in the audio buffer !


  // AUDIO INPUT thread loop:
  //  - Take the samples from a circular buffer
  //  - Send them to soundcard (in chunks of XXXX samples)
  //  - Leave thread loop when signalled via flag a.s.a.p.
  while( !WolfThd_fTerminateRequest && !WIO_fStopFlag )
   {
     if( (!WolfThd_pi16AudioBuffer) || (WolfThd_dwAudioBufferSize<=0) )
      { // oops... the giant audio sample buffer has not been allocated !
        Sleep(100);
      }
     else
     // Send data from the 'large' audio buffer to soundcard.
     //  If soundcard's buffer is already full, thread will WAIT here .
     //  If the buffer is empty ("underflow"), send the same again !
     if( SoundDev.IsOutputOpen() )
      {
       // Send output data to the soundcard object.
       // Because the source buffer may contain less samples than we must send,
       // use a local buffer to send <WOLFTHD_AUDIO_CHUNK_SIZE>  samples
       // to the soundcard !
       if( AudioOutThd_fPlayLoop )  // for "loop playing mode" :
        { // send WolfThd_pi16AudioBuffer[ 0 ... WolfThd_dwAudioBufHeadIndex-1 ] over and over !
          for(i=0; i<WOLFTHD_AUDIO_CHUNK_SIZE; ++i)
           { i16TxChunk[ i ] = WolfThd_pi16AudioBuffer[ WolfThd_dwAudioBufTailIndex++ ];
             if( WolfThd_dwAudioBufTailIndex >= WolfThd_dwAudioBufHeadIndex )
                 WolfThd_dwAudioBufTailIndex = 0;
           }
        }
       else  // no loop mode ... use the complete buffer !
        {
          for(i=0; i<WOLFTHD_AUDIO_CHUNK_SIZE; ++i)
           { i16TxChunk[ i ] = WolfThd_pi16AudioBuffer[ WolfThd_dwAudioBufTailIndex++ ];
             if( WolfThd_dwAudioBufTailIndex >= WolfThd_dwAudioBufferSize )
                 WolfThd_dwAudioBufTailIndex = 0;
             if( WolfThd_dwAudioBufTailIndex == WolfThd_dwAudioBufHeadIndex )
              {  // soft buffer underrun !
              }
           }
        }

#if(1) // (1)=normal compilation, (0)=test without writing to soundcard
       // NOTE: THE SECOND PARAMETER FOR OutWriteFloat
       //      IS THE COUNT OF SAMPLE PAIRS, NOT SIMPLE NUMBERS.
       //      Therefore, the "chunk" must have 2*SndThd_iChunkSize elements.
       if( SoundDev.OutWriteInt16(
              i16TxChunk,  // [in] pi16Data, points to 1st sample or SAMPLE PAIR
              WOLFTHD_AUDIO_CHUNK_SIZE, // [in] int Length, number of SAMPLING POINTS or PAIRS(!) to write
              500,         // [in] iTimeout_ms, max timeout in milliseconds, 0 would be 'non-blocking'
              &ms_waited ) // [out] int *piHaveWaited_ms, "have been waiting here for XX milliseconds"
           != WOLFTHD_AUDIO_CHUNK_SIZE )
        {
         error_code = SoundDev.m_ErrorCodeOut; // See ErrorCodes.h !
         switch(error_code) // Only handle errors resulting from the OUTPUT
          {
           case SOUNDOUT_ERR_NOTOPEN:
             // someone must have closed the soundcard's output. ?!?!?
             break;

           case SOUNDOUT_ERR_UNDERFLOW:
             // ignore, dont close & reopen
             break;  // end case < OUTPUT UNDERFLOW

           case SOUNDOUT_ERR_TIMEOUT:
             // cpu couldn't keep up.. better luck next time
             break; //  end case < OUTPUT UNDERFLOW or OUTPUT TIMEOUT >

           default: // all other sound errors (like INPUT errors):
             break; // IGNORE input errors, don't close the output !!!
          } // end switch(error_code)
        } // end if < output to soundcard NOT successful >
       else
        { // ok, samples sent to soundcard
         error_code = SoundDev.m_ErrorCodeOut;
         if (error_code!=NO_ERRORS) // reading ok, but something is wrong...
          {
           // 2013-02-16 : Got here with error_code=33 . Details in SOUND.CPP .
           SoundDev.m_ErrorCodeOut = NO_ERRORS;  // ouch !!!!!!!!

          }
        } /* end if <sampling of input data ok> */
#else   // do NOT sent output to the soundcard ?!
       Sleep(100);
#endif
      } // end if( SoundDev.IsOutputOpen() )
     else
      {  // Re-Open Sound Device for input
        error_code = OpenSoundcardForOutput();
        if( error_code != NO_ERROR )
         { WolfThd_fAudioOutThdRunning  = FALSE;
           return WOLFTHD_EXIT_ERROR; // exit code for this thread
         }
      } // end if < soundcard NOT open for output >

   }  /* end <almost endless thread loop> */

  // Close the soundcard for output, we don't feed it any longer !
  SoundDev.OutClose();

  // Special case for AUDIO TX LOOP :
  //  The WOLF core has already finished, and there is no other worker thread.
  //  So the AUDIO TX THRAD must inform the application about the 'termination' :
  WolfThd_fExitedThread = true;
  WolfThd_fAudioOutThdRunning = FALSE;
  return WOLFTHD_EXIT_OK; // set exit code for this thread

} // end AudioOutThdFunc()



/*--------- Implementation of the WOLF worker threads ---------------------*/


//---------------------------------------------------------------------------
DWORD WINAPI WolfWaveThdFunc( LPVOID )
  /* Lets WOLF process a WAVE file in a "background thread",
   *   to keep the user interface operational .
   * NO-VCL SPECIFIC STUFF IN HERE, and no direct calls of the user interface !
   */
{
  WolfThd_fWolfCoreThdRunning  = TRUE;

  WIO_iExitCode = 0;      // forget old error-code
  WIO_iUseSoundcard = 0;
  WIO_fStopFlag = FALSE;
  WOLF_InitDecoder(); // prepare ANOTHER decoder run (reset all vars to defaults)
  WOLF_Main( WolfThd_iArgCnt, WolfThd_pszArgs ); // let WOLF run to produce or decode a WAVE file

  /* if we get here, the thread was "polite" and terminated ITSELF, */
  /*     or someone has pulled the emergency brake !                */
  WolfThd_fExitedThread = true;

  WolfThd_fWolfCoreThdRunning  = FALSE;

  return WOLFTHD_EXIT_OK; // will this ever be reached after "ExitThread" ?
} // end WolfWaveThdFunc()
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
DWORD WINAPI WolfRxThdFunc( LPVOID )
  /* Executes WOLF to receive data from the soundcard
   *  and processes them in 'real time', in a separate thread .
   * NO-VCL SPECIFIC STUFF IN HERE, and no direct calls of the user interface !
   */
{
  WolfThd_fWolfCoreThdRunning  = TRUE;

  WIO_iUseSoundcard = 1; // let WOLF.CPP use the soundcard routines, not a wave-file

  WolfThd_dwAudioBufTailIndex = WolfThd_dwAudioBufHeadIndex; // discard old audio samples in buffer
  WolfThd_dwAudioSamplesLost = 0;

  // SOUND_thd_error_buf_in = SOUND_thd_error_buf_out = 0; // NO !!
  WolfThd_fExitedThread = false;

  // let WOLF run .. here in WolfRxThdFunc() :
  WOLF_InitDecoder(); // prepare ANOTHER decoder run (reset all vars to defaults)
  WOLF_Main( WolfThd_iArgCnt, WolfThd_pszArgs ); // let WOLF process input from SOUNDCARD

  // if the program ever gets here, the thread is quite "polite"...
  //                    or someone has pulled the emergency brake !
  WolfThd_fExitedThread = true;
  WolfThd_fWolfCoreThdRunning  = FALSE;

  return WOLFTHD_EXIT_OK; // 'return' instead of 'ExitThread()' !
} // end WolfRxThdFunc()
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
BOOL WolfThd_LaunchAudioThread( LPTHREAD_START_ROUTINE lpStartAddress )
   // Launches the audio-input thread
   // (reads samples from the soundcard, and places them in a large buffer) .
{
   if(WolfThd_hAudioThread==NULL)
    {
      WolfThd_hAudioThread = CreateThread(
         NULL,    // LPSECURITY_ATTRIBUTES lpThreadAttributes = pointer to thread security attributes
         0,       // DWORD dwStackSize  = initial thread stack size, in bytes
         lpStartAddress, // LPTHREAD_START_ROUTINE lpStartAddress = pointer to thread function
         0,       // LPVOID lpParameter = argument for new thread
         0,       // DWORD dwCreationFlags = creation flags
                  // zero -> the thread runs immediately after creation
         &WolfThd_dwAudioThreadId // LPDWORD lpThreadId = pointer to returned thread id
       );
      // The thread object remains in the system until the thread has terminated
      // and all handles to it have been closed through a call to CloseHandle.
     }

  if(WolfThd_hAudioThread!=NULL)
   { // Define the AUDIO INPUT thread's priority as required.
     // Especially when playing with the soundcard,
     // set the thread priority ABOVE NORMAL to avoid loss of audio samples !
     SetThreadPriority(
       WolfThd_hAudioThread,    // handle to the thread
       THREAD_PRIORITY_NORMAL); // thread priority level
    }
  return (WolfThd_hAudioThread!=NULL);
} // end WolfThd_LaunchAudioThread()


//---------------------------------------------------------------------------
BOOL WolfThd_LaunchWorkerThreads(int iWhichThread)
   // Launches the real-time processing thread(s) ,
   //  without the aid of Borland's Visual Component Library (VCL) !
{

   // If "old" worker thread still running, terminate and delete it .
   //  Calling ..TerminateAndDeleteThreads() if no extra thread is running
   //  won't harm !
   WolfThd_TerminateAndDeleteThreads(); // here in Launch(!)WorkerThreads()

   WolfThd_fTerminateRequest = WolfThd_fExitedThread = FALSE; // here in WolfThd_LaunchWorkerThreads()
   WIO_fStopFlag = FALSE;
   WIO_dblStartTime = time(NULL); // Start time (for WOLF decoder), UNIX-like UTC-seconds

   LPTHREAD_START_ROUTINE lpStartAddress; // pointer to thread function
   switch( iWhichThread )    // which kind of thread shall be launched ?
    {
     case WOLFTHD_WAVE_ANALYZER:  // Launch a thread to analyze a WAVE file
     case WOLFTHD_WAVE_PRODUCER:  // Launch a thread to produce a WAVE file
          lpStartAddress = WolfWaveThdFunc;
          break;

     case WOLFTHD_AUDIO_RX     :  // Launch a thread to read samples from an audio device and analyze them in real time
          AudioOutThd_fPlayLoop = FALSE; // no loop-mode now
          WIO_iUseSoundcard = 1; // let WOLF.CPP use the soundcard routines, not a wave-file
          WIO_UpdatePrefilterParams();   // prepare FFT-based FIR "pre-filter" for the current WOLF settings

          // Launch the thread for collecting samples from the audio device..
          if( ! WolfThd_LaunchAudioThread( AudioInThdFunc ) )
             return FALSE;       // bad luck .. don't try to launch the worker thread in this case !

          lpStartAddress = WolfRxThdFunc; // the WOLF decoder thread will be launched below
          break;

     case WOLFTHD_AUDIO_TX_LOOP:  // Launch a thread to generate samples and send them to an audio device in real time
          WIO_iUseSoundcard = 1; // let WOLF.CPP use the soundcard routines, not a wave-file
          WolfThd_fExitedThread = false;

          // Launch the AUDIO PLAYBACK THREAD as a "loop player" :
          AudioOutThd_fPlayLoop = TRUE;
          return WolfThd_LaunchAudioThread( AudioOutThdFunc );

     default: // nothing - or not implemented yet !
          return FALSE;
    }
   if(WolfThd_hWorkerThread==NULL)
    {
      // Create the worker thread(s)
      //
      // From "Microsoft Win32 Programmer's Reference" ("Creating Threads").
      // > The CreateThread function creates a new thread for a process.
      // > The creating thread must specify the starting address of the code
      // > that the new thread is to execute. Typically, the starting address
      // > is the name of a function defined in the program code.
      WolfThd_hWorkerThread = CreateThread(
         NULL,    // LPSECURITY_ATTRIBUTES lpThreadAttributes = pointer to thread security attributes
         0,       // DWORD dwStackSize  = initial thread stack size, in bytes
         lpStartAddress, // LPTHREAD_START_ROUTINE lpStartAddress = pointer to thread function
         0,       // LPVOID lpParameter = argument for new thread
         0,       // DWORD dwCreationFlags = creation flags
                  // zero -> the thread runs immediately after creation
         &WolfThd_dwWorkerThreadId // LPDWORD lpThreadId = pointer to returned thread id
       );
      // The thread object remains in the system until the thread has terminated
      // and all handles to it have been closed through a call to CloseHandle.
     }
    if (WolfThd_hWorkerThread == NULL)   // Check the return value for success.
     {
      // ex: ErrorExit( "CreateThread failed." );
      return FALSE;
     }

    // Note: The program should immediately get here,
    //       though the thread is still running.
    //       Closing the Thread's handle doesn't seem to terminate the thread !

    // Define the WOLF-thread's priority as required.
    // Note: The WOLF thread must have lower priority than the
    //       AUDIO-input thread, because no samples must get lost.
    //     If the samples are *processed* a few milliseconds sooner or later
    //     doesn't matter, because the audio buffer is large enough .
    SetThreadPriority(
       WolfThd_hWorkerThread,   // handle to the thread
       THREAD_PRIORITY_NORMAL); // thread priority level

  return TRUE;
} // end .._LaunchWorkerThread()


/***************************************************************************/
BOOL WolfThd_TerminateAndDeleteThreads(void)
   /* Terminates the worker thread(s) and cleans up .
    */
{
  int i;
  DWORD dwExitCode;
  BOOL  fOk = TRUE;


  // First attempt for a "polite" termination of all theads:
  //   Set a global flag to let threds terminate (if running)
  //   and wait until none of the background threads declares itself "still running" :
  WolfThd_fTerminateRequest = TRUE;  // flag to terminate all worker threads
  for(i=0; (i<200) // 200 * 50 ms ? Gosh !
        && (WolfThd_fWolfCoreThdRunning || WolfThd_fAudioInThdRunning || WolfThd_fAudioOutThdRunning );
         ++i)
   { Sleep(50);  // wait a little longer for the threads to terminate themselves
     // (Caution: especially the WOLF core may take sometime to terminate politely,
     //  since WOLF.CPP was not designed to run in a background thread ! )
   }


  if(WolfThd_hWorkerThread)  // keep an eye on the WOLF-"worker"-thread ?
   {
     // First, let the WOLF WORKER THREAD 'politely' terminate itself,
     //        allowing it to clean up everything.
     // About GetExitCodeThread(HANDLE hThread,LPDWORD lpExitCode):
     //       lpExitCode: Points to a 32-bit variable
     //          to receive the thread termination status.
     //       If the function succeeds, the return value is nonzero.
     //       If the specified thread has not terminated,
     //          the termination status returned is STILL_ACTIVE.
     dwExitCode=STILL_ACTIVE;

     // Now keep fingers crossed that the threads terminate themselves (politely)
     //   because only the thread code will be able to clean everything up !
     // ( Because the WOLF thread sometimes 'meditates' for some seconds,
     //   make the 'max waiting time' long enough ! )
     for(i=0;
        (i<200) // 200 * 50 ms ? Gosh !
           && (GetExitCodeThread(WolfThd_hWorkerThread,&dwExitCode));
         ++i)
      {
        // MessageBeep(-1);  // drums please
        Sleep(50);  // wait a little longer for the thread to terminate itself
        if(dwExitCode!=STILL_ACTIVE)
           break;   // ok, this is what we wanted : the thread terminated (politely)
      } // end for

     // Before deleting the sound thread...
     if(dwExitCode==STILL_ACTIVE)
      { // The thread does not terminate itself, so be less polite and kick it out !
        WIO_printf( "Error: Worker thread refused to terminate !\n" );
        // DANGEROUS termination of the sound thread (may crash the system):
        TerminateThread(WolfThd_hWorkerThread, // handle to the thread
                        WOLFTHD_EXIT_ERROR);   // exit code for the thread
      }

     CloseHandle(WolfThd_hWorkerThread);
     WolfThd_hWorkerThread = NULL;
     fOk &= (dwExitCode==WOLFTHD_EXIT_OK);  // TRUE = polite ending,   FALSE = something went wrong.

   } // end if(WolfThd_hWorkerThread)

  if(WolfThd_hAudioThread)
   {
     // Second, let the AUDIO BUFFER THREAD 'politely' terminate itself,
     //         allowing it to clean up everything.
     dwExitCode=STILL_ACTIVE;

     // Now keep fingers crossed that the threads terminate themselves (politely)
     //   because only the thread code will be able to clean everything up !
     // ( Because the WOLF thread sometimes 'meditates' for some seconds,
     //   make the 'max waiting time' long enough ! )
     for(i=0;
        (i<200) // 200 * 50 ms ? Gosh !
           && (GetExitCodeThread(WolfThd_hAudioThread,&dwExitCode));
         ++i)
      {
        // MessageBeep(-1);  // drums please
        Sleep(50);  // wait a little longer for the thread to terminate itself
        if(dwExitCode!=STILL_ACTIVE)
           break;   // ok, this is what we wanted : the thread terminated (politely)
      } // end for

     // Before deleting the sound thread...
     if(dwExitCode==STILL_ACTIVE)  // if still not terminated, kick it out :
      { WIO_printf( "Error: Audio thread refused to terminate !\n" );
        TerminateThread(WolfThd_hAudioThread, // handle to the thread
                       WOLFTHD_EXIT_ERROR);   // exit code for the thread
      }

     CloseHandle(WolfThd_hAudioThread);
     WolfThd_hAudioThread = NULL;
     fOk &= (dwExitCode==WOLFTHD_EXIT_OK);  // TRUE = polite ending,   FALSE = something went wrong.

   } // end if(WolfThd_hAudioThread)


  return fOk;
} // end ..._TerminateAndDeleteThread()


/***************************************************************************/
void WIO_Exit(void)
  /* De-Initializes the IO-access...
   *    - closes the COM port if necessary
   */
{
 if( WIO_hComPort != INVALID_HANDLE_VALUE )
     WIO_CloseComPort();
}



// EOF < WolfThd.cpp >
