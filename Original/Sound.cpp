//-----------------------------------------------------------------------
// C:\cbproj\SoundUtl\Sound.cpp : implementation of the CSound class.
//
// This class implements an object that reads/writes to a soundcard.
//      Usually uses the multimedia system API (MMSYSTEM),
//      but in a few cases this module MAY work with ASIO drivers too.
//
// Author: Wolfgang Buescher (DL4YHF) 2000..2012
//         Parts for MMSYSTEM taken from "WinPSK31 by Moe Wheatley, AE4JY" .
//         ASIO support based on a sample from the Steinberg ASIO SDK .
// Original file location: C:\CBproj\SpecLab\Sound.cpp
//
//-----------------------------------------------------------------------


#include "SWITCHES.H"  // project specific compiler switches ("options")
                       // must be included before anything else !


/* Revision History (lastest entry first):
 *
 *  2012-08-14 :
 *      - Added support for Winrad-compatible ExtIO-DLLs, by virtue of
 *        DL4YHF's "Audio-I/O-Host" (AudioIO.c) which can optionally load
 *        such DLLs dynamically, in addition to the normal Audio-I/O-DLLs .
 *        Because many ExtIO-DLLs also control the VFO (HF tuning frequency),
 *        methods were added to the CSound class which can pass on this info.
 *        New: CSound::HasVFO(), GetVFOFrequency(), SetVFOFrequency(), etc.
 *
 *  2012-07-06 :
 *      - Eliminated the stupid AUDIO DEVICE NUMBERS within Spectrum Lab,
 *        because the triple-stupid windows juggles around with these numbers
 *        whenever an USB audio device is connected/disconnected, which includes
 *        stupid 'dummy' audio devices like bluetooth audio, etc etc.
 *        Thus the beloved E-MU 0202 may have been device number 3 yesterday,
 *        but number 4 today. No thanks, we don't want this kind of bullshit.
 *        SL now saves the device's "Product Name" in its configuration,
 *        which is part of microsoft's WAVEINCAPS structure.
 *        Because the windows multimedia API still uses device NUMBERS,
 *        not NAMES,
 *
 *  2012-03-25 :
 *       - Timestamp in T_ChunkInfo.ldblUnixDateAndTime is now set as close
 *          to the 'real time' in CSound::GetDateAndTimeForInput(),
 *          using the system's high-resolution timer ("QueryPerformanceCounter").
 *
 *  2008-03-29 :
 *       - Added support for the new "driver-less AUDIO-I/O" system,
 *          which is basically a DLL with circular buffer in shared memory.
 *          Can be used to send the output stream "directly" to Winamp,
 *          without a virtual audio cable or other crap in between .
 *          Modified parts can be found by searching SWI_AUDIO_IO_SUPPORTED .
 *
 *  2007-07-13 :
 *       - Output routine now accepts ARBITRARY block lengths,
 *          which was necessary when the RESAMPLING option was added in SpecLab.
 *
 *
 *  2006-04-14 :
 *       -  2 * 16-bit ASIO works, surprisingly even while the output is sent
 *          to an MMSYSTEM device using the same soundcard at the same time.
 *          Two different ASIO drivers (one for input, a different one for
 *          output) does *NOT* work, because the ASIO-wrapper (+"thiscall" fix)
 *          can only load one ASIO driver at a time .
 *  
 *  2006-03-23 :
 *       -  Checked the possibility to support ASIO drivers in this wrapper
 *          as well as the standard multimedia system, since several users
 *          reported that the 24-bit mode didn't work with their soundcards.
 *          Furthermore, Johan Bodin /SM6LKM asked for it ;-)
 *
 *  2005-04-23 :
 *       -  SOUND.CPP now used in the experimental WOLF GUI, too
 *           ( WOLF = Weak-signal Operation on Low Frequency, by KK7KA).
 *       -  Fixed a problem in InClose() / MMSYSTEM error "Still Playing" (!)
 *
 *  2004-05-27 :
 *       -  Changed the data type in InReadStereo() from SHORT to T_Float,
 *          making it easier to handle different ADC- and DAC- resolutions
 *          internally.  EVERYTHING WAS(!) SCALED TO +/-32k REGARDLESS
 *          of 8, 16, or 24 bits per sample. No problem with 32-bit floats.
 *          (One could also normalize everything to -1.0...+1.0 as well,
 *           but this would mean a lot of modifications in Spectrum Lab).
 *          This renders the CSound class incompatible with earlier versions !
 *       -  First successful experiment to read 24-bit-samples
 *          from a Soundblaster Audigy 2 ZS . Amazingly the standard multi-
 *          media system (with waveInOpen etc) also supports sampling at
 *          96 kHz with 24 bits resolution - no need to play with ASIO drivers,
 *          Direct X, and other stuff yet (other cards require ASIO for this, though)
 *       -  To achieve 24 bit resolution also for the OUTPUT (!),
 *          it was necessary to use the WAVEFORMATEXTENSIBLE structure.
 *
 *  2004-05-11 :
 *       -  Modified buffers, now dynamically allocated.
 *          Reason: Big difference between 96kHz, 2 channels, 24 bit
 *               and 11kHz, 1 channel, 16 bit on a notebook with tiny RAM !
 *
 *  2001-11-24 (YYYY-MM-DD):
 *       -  Optional STEREO processing implemented,
 *          depending on an option defined in the file SWITCHES.H .
 *
 *  2000-09-02 :
 *       -  Changed the error codes in ErrorCodes.h.
 *       -  Splitted m_ErrorCode into m_ErrorCodeIn and m_ErrorCodeOut
 *          because simultaneous input and output caused trouble
 *          before this.
 *   June 10, 2000: taken from WinPSK31 V1.2 "special edition"
 *                   "Originally written by Moe Wheatley, AE4JY"
 *                   "Modified and extended by Dave Knight, KA1DT"
 *       -  Adapted for C++Builder 4 by Wolfgang Buescher, DL4YHF
 *       -  All TAB characters from the source file removed.
 *          (never use TABS in source files !! tnx.)
 *       -  Defined a global variable of type CSound
 *          that will be used by the GENERATOR and ANALYSER
 *          (simultaneously)
 *       -  This unit will always remain "public domain"
 *          like the original work by AE4JY and KA1DT.
 *
 */

#include "SWITCHES.H"  // project specific compiler switches ("options")
                       // must be included before anything else !

#pragma alignment /* should report the alignment (if the compiler is kind enough) */
   // 2012-03-10 : BCB6 said "Die Ausrichtung beträgt 8 Byte, die enum-Größe beträgt 1 Byte"
   // This was, most likely, the reason for the malfunction of the goddamned WAVEFORMATEX .


#include <windows.h>  // try NOT to use any Borland-specific stuff here (no VCL)
#include <math.h>     // to generate test signals only (sin)
#include <stdio.h>    // no standard-IO, but sprintf used here
#pragma hdrstop

#include "Sound.h"     // Once just the soundcard interface, now multi-purpose 
#include "ErrorCodes.h"

#ifndef  SWI_UTILITY1_INCLUDED
 #define SWI_UTILITY1_INCLUDED 1
#endif
#if( SWI_UTILITY1_INCLUDED ) /* DL4YHF's "utility"-package #1 included ? */
  #include "utility1.h"  // UTL_WriteRunLogEntry(), UTL_NamedMalloc(), etc
#endif

#ifndef  SWI_AUDIO_IO_SUPPORTED
# define SWI_AUDIO_IO_SUPPORTED 0
#endif
#if( SWI_AUDIO_IO_SUPPORTED )
# include "AudioIO.h"   // located in \cbproj\AudioIO\*.*
#endif // SWI_AUDIO_IO_SUPPORTED


// local defines
#define TIMELIMIT 1000  // Time limit to WAIT on a new soundcard buffer
                        // to become ready in milliSeconds.
                        // 2004-05-10: Changed from 2000 to 1000,
                        //   because a too long delay causes a ping-pong-type
                        //   of error when running in full-duplex mode.
                        //   ( especially bad when tested with Audigy 2
                        //     and 96 kHz sampling rate, but small buffers )
                        //   On the other hand, 500ms was too short .

// A "GUID" for working with the WAVEFORMATEXTENSIBLE struct (for PCM audio stream)
static const  GUID KSDATAFORMAT_SUBTYPE_PCM = {
      0x1,0x0000,0x0010,{0x80,0x00,0x00,0xaa,0x00,0x38,0x9b,0x71} };

// removed: WAVEFORMATEX SOUND_WFXSettings;  // input- and output audio device settings

// MMSYSTEM callback functions for use in soundcard input/output
void CALLBACK WaveInCallback( HWAVEIN, UINT, CSound*, DWORD, DWORD );
void CALLBACK WaveOutCallback( HWAVEOUT, UINT, CSound*, DWORD, DWORD );

#if( SWI_ASIO_SUPPORTED )      // Support ASIO audio drivers too ?
 void CSound_AsioCallback( T_AsioWrap_DriverInfo *pDriverInfo, long index, DWORD dwInstance );
#endif

static int CountBits( DWORD dwMask )  // internal helper function
{ int n=0;
  for(int i=0; i<32; ++i)
   { if( (dwMask&1) != 0)
       ++n;
     dwMask>>=1;
   }
  return n;
}

extern BOOL SndThd_fRequestToTerminate; // Notbremse für 'gecrashte' Threads

//**************************************************************************
//    Debugging
//**************************************************************************
extern BOOL APPL_fMayUseASIO; // Since 2011-08-09 ASIO may be disabled via command line switch .
       // Located (for SpecLab) in C:\cbproj\SpecLab\SplashScreen.cpp
       // or (for the 'WOLF-GUI') in c:\cbproj\WOLF\WOLF_GUI1.cpp .
BOOL SOUND_fDumpEnumeratedDeviceNames = FALSE;  // flag for Sound_InputDeviceNameToDeviceID()


#if (SWI_ALLOW_TESTING) // unexplainable crash with corrupted "call stack" ?
 int  Sound_iSourceCodeLine = 0;  // debugging stuff: last source code line
 int  Sound_iMMSourceCodeLine=0;  // similar, for callback from the 'driver'
 int  Sound_iAsioCbkSourceCodeLine=0; // .. and yet another (incompatible) callback
 int  Sound_iUnknownWaveInCallbackMsg = -1;
 int  Sound_iUnknownWaveOutCallbackMsg = -1;
 #define DOBINI() Sound_iSourceCodeLine=__LINE__
 #define DOBINI_MM_CALLBACK() Sound_iMMSourceCodeLine=__LINE__
 #define DOBINI_ASIO_CALLBACK() Sound_iAsioCbkSourceCodeLine=__LINE__
  // Examine the most recent source code line in the debugger's "watch"-window .
  // Details about "DOBINI()" (Bavarian "here I am")  in SoundThd.cpp  !
 static void LocalBreakpoint(void)
  {
  }  // <<< set a breakpoint HERE (when running under debugger control) !
 #define LOCAL_BREAK() LocalBreakpoint()
#else
 #define DOBINI()
 #define DOBINI_MM_CALLBACK()
 #define LOCAL_BREAK()
#endif


//--------------------------------------------------------------------------
// Helper functions (there's no need to turn everything into a shiny CLASS!)
//--------------------------------------------------------------------------


/***************************************************************************/
long SOUND_GetClosestNominalStandardSamplingRate( double dblCalibratedSamplingRate )
   // Decides to which "nominal" soundcard sampling rate should be used
   //  for a certain 'calibrated' soundcard sampling rate.
{
  if( dblCalibratedSamplingRate < 5800 )
   { return 5512;
   }
  if( dblCalibratedSamplingRate < 7000 )
   { return 6000;
   }
  if( dblCalibratedSamplingRate < 9000 )
   { return 8000;
   }
  if( dblCalibratedSamplingRate < 11500 )
   { return 11025;
   }
  if( dblCalibratedSamplingRate < 14000 )
   { return 12000;           // 48k / 4
   }
  if( dblCalibratedSamplingRate < 20000 )
   { return 16000;           // 2 * 8 kHz
   }
  if( dblCalibratedSamplingRate < 25000 )
   { return 22050;           // 44k1 / 2
   }
  if( dblCalibratedSamplingRate < 38000 )
   { return 32000;           // also a standard !  4 * 8 kHz
   }
  if( dblCalibratedSamplingRate < 46000 )
   { return 44100;           // "CD"
   }
  if( dblCalibratedSamplingRate < 55000 )
   { return 48000;
   }
  if( dblCalibratedSamplingRate < 70000 )
   { return 64000;           // 8 * 8 kHz (really a standard ?)
   }
  if( dblCalibratedSamplingRate>=86000 && dblCalibratedSamplingRate<90000 )
   { return 88200; // "Sampling rate used by some professional recording equipment when the destination is CD (multiples of 44,100 Hz)."
   }
  if( dblCalibratedSamplingRate>=94000 && dblCalibratedSamplingRate<=98000 )
   { return 96000; // DVD-Audio, 2 * 48 kHz
   }
  if( dblCalibratedSamplingRate>=174000 && dblCalibratedSamplingRate<=178000 )
   { return 176400; // "used by professional applications for CD production"
   }
  if( dblCalibratedSamplingRate>=190000 && dblCalibratedSamplingRate<=194000 )
   { return 192000; // "High-Definition DVD" (and supported by E-MU 0202)
   }
  // Arrived here: No joy, simply return the ROUNDED sampling rate
  return (long)(dblCalibratedSamplingRate+0.5);
} // end Config_NominalSampleRateToCalibTableIndex()


/***************************************************************************/
char *Sound_GetAudioDeviceNameWithoutPrefix(
           char *pszNameOfAudioDeviceOrDriver,  // [in] for example "4 E-MU 0202 | USB"
           int  *piDeviceIndex )                // [out] for example 4 (parsed from the name!)
  // Use with MConfig.sz255AudioInputDeviceOrDriver !
  // The result (example: "E-MU 0202 | USB" is only valid as long as the input (string) exists !
{
  int idx=0, sign=1;
  char *cp;

  cp = strrchr(pszNameOfAudioDeviceOrDriver, '.' );
  if( (cp!=NULL) && stricmp( cp, ".dll") == 0 )
   { // It's the filename of a DLL - this cannot be a normal "audio device" !
     if( piDeviceIndex!=NULL )
      { *piDeviceIndex = C_SOUND_DEVICE_AUDIO_IO;
      }
     return pszNameOfAudioDeviceOrDriver;
   }

  cp = pszNameOfAudioDeviceOrDriver;
  if( strncmp( cp, "A: ", 3) == 0 ) // must be an ASIO DEVICE !
   { if( piDeviceIndex!=NULL )
      { *piDeviceIndex = C_SOUND_DEVICE_ASIO;
      }
     return cp+3;
   }
  // Try a few 'fixed device names' which need extra treatment :
  //    Extrawürste für SDR-IQ und Perseus.. vgl. Definitionen in CONFIG.H !
  if( strcmp( cp, "SDR-IQ / SDR-14" )==0 )
   { if( piDeviceIndex!=NULL )
      { *piDeviceIndex = C_SOUND_DEVICE_SDR_IQ;
      }
     return cp;
   }
  if( strcmp( cp, "Perseus" )==0 )
   { if( piDeviceIndex!=NULL )
      { *piDeviceIndex = C_SOUND_DEVICE_PERSEUS;
      }
     return cp;
   }
  if( *cp=='-' )                // Skip the optional sign
   { sign = -1;
     ++cp;
   }
  while( *cp>='0' && *cp<='9' ) // skip DIGITS
   { idx = 10*idx + (*cp-'0');
     ++cp;
   }
  idx *= sign;
  if( piDeviceIndex!=NULL )
   { *piDeviceIndex = idx;
   }
  while( *cp==' ' ) // skip separator between numeric prefix (audio device index) and device name
   { ++cp;
   }
  return cp;  // result: Pointer to the DEVICE NAME or the end of the string (trailing zero)
} // end Sound_GetAudioDeviceNameWithoutPrefix()


//--------------------------------------------------------------------------
int Sound_InputDeviceNameToDeviceID( char *pszNameOfAudioDevice )
  // Added 2012-07-06 for Spectrum Lab. Reasons below,
  //  as well as in C:\cbproj\SpecLab\Config.cpp .
  // Return value from Sound_InputDeviceNameToDeviceID() :
  //  <positive> : "device ID" of a standard (WMM) audio device,
  //  <negative> : one of the following constants :
  //       C_SOUND_DEVICE_SDR_IQ, C_SOUND_DEVICE_PERSEUS (SDRs with built-in support)
  //       C_SOUND_DEVICE_ASIO      (ASIO device, identified by NAME)
  //       C_SOUND_DEVICE_AUDIO_IO  (name of an Audio-I/O-DLL, or possibly ExtIO;
  //                                 in fact, any FILE+PATH which looks like a DLL)
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // Since 2012-07, Spectrum Lab doesn't use the stupid AUDIO DEVICE NUMBERS
  // anymore because the triple-stupid windows juggles around with these numbers
  // whenever an USB audio device is connected/disconnected, which includes
  // stupid 'dummy' audio devices like bluetooth audio, etc etc.
  // Thus the beloved E-MU 0202 may have been device number 3 yesterday,
  //  but number 4 today. No thanks, we don't want this kind of bullshit.
  //  SL now saves the device's "Product Name" in its configuration,
  //  which is part of microsoft's WAVEINCAPS structure.
{
 int i, n_devs, iDeviceID, iBestDeviceID;
 WAVEINCAPS  my_waveincaps;
 char *cp1, *cp2, *pszAudioDeviceNameWithoutPrefix;
 BOOL found_perfect_match = FALSE;

  pszAudioDeviceNameWithoutPrefix = Sound_GetAudioDeviceNameWithoutPrefix(
                                         pszNameOfAudioDevice, &iDeviceID );
  if(    iDeviceID==C_SOUND_DEVICE_ASIO
      || iDeviceID==C_SOUND_DEVICE_SDR_IQ
      || iDeviceID==C_SOUND_DEVICE_PERSEUS
      || iDeviceID==C_SOUND_DEVICE_AUDIO_IO /* aka "driver DLL" like in_AudioIO.dll */
    )
   { return iDeviceID;    // it's a DUMMY ID, not one of the standard multimedia device IDs !
     // (don't waste time to compare the name against the enumerated devices below)
   }
  i = strlen(pszNameOfAudioDevice);
  if( i>4 && strnicmp(pszNameOfAudioDevice+i-4,".dll",4)==0 )
   { return C_SOUND_DEVICE_AUDIO_IO; /* pseudo "device number" for DL4YHF's AudioIO-DLLs & I2PHD's ExtIO-DLLs */
   }

  //--------- check all available AUDIO INPUT DEVICES .... -----------------
  iBestDeviceID = -1;  // Use the 'default' device if none of the device names matches..
  n_devs = waveInGetNumDevs();
  if( SOUND_fDumpEnumeratedDeviceNames )
   { UTL_WriteRunLogEntry( "InputDeviceNameToDeviceID('%s') enumerating %d wave-input devices:",
                           (char*)pszNameOfAudioDevice,   (int)n_devs );
   }
  for(i=0; i<n_devs; ++i)
   {
    if( waveInGetDevCaps(     i, // UINT uDeviceID
                 &my_waveincaps, // LPWAVEINCAPS pwic,
             sizeof(WAVEINCAPS)) // UINT cbwic
       == MMSYSERR_NOERROR)
     { // The WAVEINCAPS structure describes the capabilities
       // of a waveform-audio input device.
       // Drivers for the standard multi-media system are identified by NUMBERS.
      if( (my_waveincaps.dwFormats &
           (  WAVE_FORMAT_4M16 // must support 44.1 kHz, mono, 16-bit, or ...
            | WAVE_FORMAT_4S16 // 44.1 kHz, stereo, 16 bit, or...
#ifdef WAVE_FORMAT_96S16        /* not defined in Borland's headers ! */
            | WAVE_FORMAT_96S16 // 96 kHz, stereo, 16 bit (upper end of the scale)
#else
#       error "Your windoze header files are severely outdated !"
#endif

            | WAVE_FORMAT_1M16 // 11.025 kHz, mono, 16 bit
            | WAVE_FORMAT_1M08 // 11.025 kHz, mono, 8 bit (lower end of the scale)
           )  )
       // Note: the ugly "WiNRADIO Virtual Soundcard" thingy reported dwFormats=0,
       //       so accept the device also if the "number of channels" is non-zero:
          || (my_waveincaps.wChannels > 0 )  // added 2007-03-06 for "VSC"
        )
       {
        if( SOUND_fDumpEnumeratedDeviceNames )
         { UTL_WriteRunLogEntry( "  Device #%d is '%s', formats=0x%08lX, ok",
             (int)i, (char*)my_waveincaps.szPname, (long)my_waveincaps.dwFormats );
         }
        if( strcmp( my_waveincaps.szPname, pszAudioDeviceNameWithoutPrefix ) == 0 )
         { // Bingo, this is the matching audio device name (here: INPUT) ..
           // but is it UNIQUE (same device "ID" alias device index) ?
           if( i == iDeviceID )
            { // ex: return i;
              iBestDeviceID = i;
              found_perfect_match = TRUE;
            }
           else
            { if( !found_perfect_match )
               { iBestDeviceID = i;   // use this 'second best' choice later
               }
            }
         }
        else // my_waveincaps.szPname != pszAudioDeviceNameWithoutPrefix
         { // Das schwachsinnge, dämliche "Windows 7" meldet frecherweise
           // nicht einfach nur den Namen des Gerätes (hier: "FiFiSDR Soundcard"),
           // sondern garniert den Namen mit völlig irreführendem Schrott,
           // so dass unter Windows 7 in den Auswahlboxen
           //  statt (bei XP) "FiFiSDR Soundcard" der folgende SCHWACHSINN angezeigt wird:
           //       "Mikrofon (FiFiSDR Soundcard)" .
           // HÄÄ ? ? Armes FiFi-SDR, hat man dich doch glatt zu einem dämlichen
           //  'Mikrofon' degradiert. Um diesen Windoofs-typischen Schwachsinn
           //  zumindest teilweise zu umgehen, sucht Spectrum Lab (d.h. sound.cpp)
           //  nun auch nach einem AUSDRUCK IN KLAMMERN, und vergleicht diesen
           //  mit dem gesuchten Gerätenamen.
           cp1 = strchr(  my_waveincaps.szPname, '(' );
           cp2 = strrchr( my_waveincaps.szPname, ')' );
           if( cp1!=NULL && cp2!=NULL && cp2>cp1 )
            { // The enumerated name could indeed be one of those stupidly 'decorated' names:
              ++cp1;        // skip the opening parenthesis
              *cp2 = '\0';  // isolate the real device name (strip the parenthesis)
              if( strcmp( cp1, pszAudioDeviceNameWithoutPrefix ) == 0 )
               { if( ! found_perfect_match )
                  { iBestDeviceID = i; // use this 'second best' choice later
                    // (i.e. accept "Mikrofon (FiFiSDR Soundcard)" from Vista/Win7
                    //   as well as "FiFiSDR Soundcard" from good old XP )
                  }
               }
            } // end if < windoze-7 specific decoration with a stupid prefix, and parenthesis >
         }

       }
      else
       {
        if( SOUND_fDumpEnumeratedDeviceNames )
         { UTL_WriteRunLogEntry( "  Device #%d is '%s', formats=0x%08lX, NOT SL-compatible !",
             (int)i, (char*)my_waveincaps.szPname, (long)my_waveincaps.dwFormats );
         }
       }
     }
    else  // waveInGetDevCaps failed on this device :
     {
        if( SOUND_fDumpEnumeratedDeviceNames )
         { UTL_WriteRunLogEntry( "InputDeviceNameToDeviceID: Failed to retrieve 'capabilities' for device #%d !",
             (int)i );
         }
     }
   } // end for < all windows multimedia audio INPUT devices >

 if( SOUND_fDumpEnumeratedDeviceNames )
  { if( found_perfect_match )
     { UTL_WriteRunLogEntry( "InputDeviceNameToDeviceID: Perfect match for device #%d = '%s' .",
                             (int)iBestDeviceID, (char*)pszAudioDeviceNameWithoutPrefix );
     }
    else
     { UTL_WriteRunLogEntry( "InputDeviceNameToDeviceID: No perfect match; best bet is device #%d = '%s' .",
                             (int)iBestDeviceID, (char*)pszAudioDeviceNameWithoutPrefix );
     }
  }


  SOUND_fDumpEnumeratedDeviceNames = FALSE; // no need to dump those 'device capabilities' in the debug-run-log again


  return iBestDeviceID;


} // end Sound_InputDeviceNameToDeviceNumber()


//--------------------------------------------------------------------------
int Sound_OutputDeviceNameToDeviceID( char *pszNameOfAudioDevice )
  // Added 2012-07-06 for Spectrum Lab.
  // For details, see Sound_InputDeviceNameToDeviceID() .
{
 int i, n_devs, iDeviceID, iBestDeviceID;
 BOOL found_perfect_match = FALSE;
 WAVEOUTCAPS my_waveoutcaps;
 char *pszAudioDeviceNameWithoutPrefix;

  pszAudioDeviceNameWithoutPrefix = Sound_GetAudioDeviceNameWithoutPrefix(
                                         pszNameOfAudioDevice, &iDeviceID );
  if( iDeviceID==C_SOUND_DEVICE_ASIO )
   { return iDeviceID;
   }


  //--------- check all available AUDIO OUTPUT DEVICES .... -----------------
  iBestDeviceID = -1;
  n_devs = waveOutGetNumDevs();
  if( SOUND_fDumpEnumeratedDeviceNames )
   { UTL_WriteRunLogEntry( "OutputDeviceNameToDeviceID('%s') enumerating %d wave-output devices:",
                           (char*)pszNameOfAudioDevice,  (int)n_devs );
   }
  for( i=0; i<n_devs; ++i)
   {
    if( waveOutGetDevCaps(    i, // UINT uDeviceID
                &my_waveoutcaps, // LPWAVEOUTCAPS pwoc,
            sizeof(WAVEOUTCAPS)) // UINT cbwoc
       == MMSYSERR_NOERROR)
     { // The WAVEOUTCAPS structure describes the capabilities
       // of a waveform-audio input device.
       //

      if( (my_waveoutcaps.dwFormats  // must support at least one of the following:
           &(  WAVE_FORMAT_4M16 // must support 44.1 kHz, mono, 16-bit, or ...
             | WAVE_FORMAT_4S16 // 44.1 kHz, stereo, 16 bit, or...
#ifdef WAVE_FORMAT_96S16
             | WAVE_FORMAT_96S16 // 96 kHz, stereo, 16 bit (upper end of the scale)
             | WAVE_FORMAT_48S16
#else
#       error "Your windoze header files are severely outdated !"
#endif
             | WAVE_FORMAT_1M16 // 11.025 kHz, mono, 16 bit
             | WAVE_FORMAT_1M08 // 11.025 kHz, mono, 8 bit (lower end of the scale)
           ) )
       // Note: the ugly "WiNRADIO Virtual Soundcard" thingy reported dwFormats=0,
       //       so accept the device also if the "number of channels" is non-zero:
         || (my_waveoutcaps.wChannels > 0 )  // added 2007-03-06 for "VSC"
        )
       {
        if( SOUND_fDumpEnumeratedDeviceNames )
         { UTL_WriteRunLogEntry( "  Device #%d is '%s', formats=0x%08lX, ok",
             (int)i, (char*)my_waveoutcaps.szPname, (long)my_waveoutcaps.dwFormats );
         }


        if( strcmp( my_waveoutcaps.szPname, pszAudioDeviceNameWithoutPrefix ) == 0 )
         { // Bingo, this is the matching audio device name (here: INPUT) ..
           // but is it UNIQUE (same device "ID" alias device index) ?
           if( i == iDeviceID )
            { iBestDeviceID = i;
              found_perfect_match = TRUE;
            }
           else
            { if( ! found_perfect_match )
               { iBestDeviceID = i;
               }
            }
         }
       }
      else
       {
        if( SOUND_fDumpEnumeratedDeviceNames )
         { UTL_WriteRunLogEntry( "  Device #%d is '%s', formats=0x%08lX, NOT SL-compatible !",
             (int)i, (char*)my_waveoutcaps.szPname, (long)my_waveoutcaps.dwFormats );
         }
       }
     }
    else  // waveOutGetDevCaps failed on this device :
     {
     }
   } // end for < all windows multimedia audio OUTPUT devices >

  if( SOUND_fDumpEnumeratedDeviceNames )
   { if( found_perfect_match )
      { UTL_WriteRunLogEntry( "OutputDeviceNameToDeviceID: Perfect match for device #%d = '%s' .",
                              (int)iBestDeviceID, (char*)pszAudioDeviceNameWithoutPrefix );
      }
     else
      { UTL_WriteRunLogEntry( "OutputDeviceNameToDeviceID: No perfect match; best bet is device #%d = '%s' .",
                              (int)iBestDeviceID, (char*)pszAudioDeviceNameWithoutPrefix );
      }
   }

  SOUND_fDumpEnumeratedDeviceNames = FALSE; // no need to dump those 'device capabilities' in the debug-run-log again

  return iBestDeviceID;

} // end Sound_OutputDeviceNameToDeviceID()

//--------------------------------------------------------------------------
BOOL Sound_OpenSoundcardInputControlPanel(
         char *pszNameOfAudioDevice ) // [in] reserved for future use
{
  int iResult = -1;
  if( APPL_iWindowsMajorVersion >= 6 )  // verdammt... es ist "Vista" oder späteres Zeug...
   { // Under "Vista" (eeek) and Windows 7, SNDVOL32.EXE is not available.
     // Thanks Jurgen B. for pointing out the following trick for Windows 7:
     // "C:\windows\system32\rundll32.exe" Shell32.dll,Control_RunDLL mmsys.cpl,,1
     DOBINI();  // 2008-05-21: Got stuck here when the "phantom breakpoint" occurred again
     iResult = (int)ShellExecute(  // See Win32 Programmer's Reference
        APPL_hMainWindow, // HWND hwnd = handle to parent window
              NULL,    // LPCTSTR lpOperation = pointer to string that specifies operation to perform
       "rundll32.exe", // LPCTSTR lpFile = pointer to filename or folder name string
       "Shell32.dll,Control_RunDLL mmsys.cpl,,1", // LPCTSTR lpParameters = pointer to string that specifies parameters
              NULL,    // LPCTSTR lpDirectory = pointer to string that specifies default directory
         SW_SHOWNORMAL); // .. whether file is shown when opened
        DOBINI();
   }
  else
   { // Under Windows 98, NT, 2000, XP, the trusty old SNDVOL32.EXE did the job,
     //  with the command line parameter "-r" for RECORD:
     DOBINI();  // 2008-05-21: Got stuck here when the "phantom breakpoint" occurred again
     iResult = (int)ShellExecute(  // See Win32 Programmer's Reference
        APPL_hMainWindow, // HWND hwnd = handle to parent window
              NULL,    // LPCTSTR lpOperation = pointer to string that specifies operation to perform
       "sndvol32.exe", // LPCTSTR lpFile = pointer to filename or folder name string
              "/r",    // LPCTSTR lpParameters = pointer to string that specifies parameters
              NULL,    // LPCTSTR lpDirectory = pointer to string that specifies default directory
         SW_SHOWNORMAL); // .. whether file is shown when opened
     DOBINI();
   }
  // About ShellExecute():
  // > If the function fails, the return value is an error value
  // > that is less than or equal to 32.
  return (iResult>32);
} // end Sound_OpenSoundcardInputControlPanel()

//--------------------------------------------------------------------------
BOOL Sound_OpenSoundcardOutputControlPanel(
         char *pszNameOfAudioDevice ) // [in] reserved for future use
{
  int iResult = -1;
  if( APPL_iWindowsMajorVersion >= 6 )  // verdammt... es ist "Vista" oder späteres Zeug...
   { // Under "Vista" (eeek) and Windows 7, SNDVOL32.EXE is not available.
     DOBINI();
     iResult = (int)ShellExecute(  // See Win32 Programmer's Reference
       APPL_hMainWindow, // HWND hwnd = handle to parent window
              NULL,    // LPCTSTR lpOperation = pointer to string that specifies operation to perform
         "sndvol.exe", // LPCTSTR lpFile = pointer to filename or folder name string
                "",    // LPCTSTR lpParameters = pointer to string that specifies parameters
              NULL,    // LPCTSTR lpDirectory = pointer to string that specifies default directory
       SW_SHOWNORMAL); // .. whether file is shown when opened
     DOBINI();
   }
  else  // Win98, ME, NT, XP :
   {
      DOBINI();
      iResult = (int)ShellExecute(  // See Win32 Programmer's Reference
       APPL_hMainWindow, // HWND hwnd = handle to parent window
              NULL,    // LPCTSTR lpOperation = pointer to string that specifies operation to perform
       "sndvol32.exe", // LPCTSTR lpFile = pointer to filename or folder name string
                "",    // LPCTSTR lpParameters = pointer to string that specifies parameters
              NULL,    // LPCTSTR lpDirectory = pointer to string that specifies default directory
       SW_SHOWNORMAL); // .. whether file is shown when opened
     DOBINI();
   }
  return (iResult>32);
} // end Sound_OpenSoundcardOutputControlPanel()

#if( SWI_AUDIO_IO_SUPPORTED )  // only if the 'audio-I/O-host' is supported..
//--------------------------------------------------------------------------
int ConvertErrorCode_AIO_to_SOUND( int iAIOErrorCode )
  // Converts an error code from DL4YHF's Audio-I/O-Library (see AudioIO.h)
  // into the closest match for the CSound class, defined in ?\SoundUtl\ErrorCodes.h .
{
  switch( iAIOErrorCode )
   { // "Audio-I/O" error codes from \cbproj\AudioIO\AudioIO.h :
     case AIO_GENERAL_ERROR            : return ERROR_FROM_AUDIO_IO_DLL;
     case AIO_ERROR_DLL_NOT_LOADED     : return ERROR_FROM_AUDIO_IO_DLL;
     case AIO_ERROR_DLL_NOT_COMPATIBLE : return ERROR_FROM_AUDIO_IO_DLL;
     case AIO_ERROR_NOT_IMPLEMENTED    : return ERROR_FROM_AUDIO_IO_DLL;
     case AIO_ERROR_INVALID_PARAMETER  : return ERROR_FROM_AUDIO_IO_DLL;
     case AIO_ERROR_OUT_OF_RESOURCES   : return ERROR_FROM_AUDIO_IO_DLL;
     case AIO_ERROR_FORMAT_NOT_SUPPORTED : return ERROR_FROM_AUDIO_IO_DLL;
     case AIO_ERROR_SAMPRATE_NOT_SUPPORTED: return ERROR_FROM_AUDIO_IO_DLL;
     case AIO_ERROR_FILE_NOT_FOUND     : return ERROR_FROM_AUDIO_IO_DLL;
     case AIO_ERROR_INPUT_NOT_OPEN     : return SOUNDIN_ERR_NOTOPEN;
     case AIO_ERROR_OUTPUT_NOT_OPEN    : return SOUNDOUT_ERR_NOTOPEN;
     case AIO_INVALID_HANDLE           : return ERROR_ILLEGAL_CMD_PARAMETER;
     case AIO_ERROR_CANCELLED          : return ERROR_FROM_AUDIO_IO_DLL;
     case AIO_ERROR_CANNOT_WRITE_YET   : return ERROR_FROM_AUDIO_IO_DLL;
     case AIO_ERROR_CANNOT_READ_YET    : return ERROR_FROM_AUDIO_IO_DLL;
     case AIO_ERROR_NOT_INITIALIZED    : return ERROR_FROM_AUDIO_IO_DLL;
     default: if( iAIOErrorCode<0 )      return ERROR_FROM_AUDIO_IO_DLL;
              else                       return NO_ERRORS;
   }
} // end ConvertErrorCode_AIO_to_SOUND()
#endif // ( SWI_AUDIO_IO_SUPPORTED )


//--------------------------------------------------------------------------
void Sound_InterleaveTwoBlocks( T_Float *pfltChannelA, T_Float *pfltChannelB,
                                int     nSamplePoints,
                                T_Float *pfltInterleavedOutput )
{
  while( nSamplePoints-- )
   { *pfltInterleavedOutput++  = *pfltChannelA++;
     *pfltInterleavedOutput++  = *pfltChannelB++;
   }
} // end Sound_InterleaveTwoBlocks()

#if( SWI_AUDIO_IO_SUPPORTED )
int Sound_AudioIOErrorToMyErrorCode( int iAudioIOErrorCode )
{
  switch(iAudioIOErrorCode)
   { case AIO_GENERAL_ERROR:  /* use this error code if you don't find anything better below */
     case AIO_ERROR_DLL_NOT_LOADED    :
     case AIO_ERROR_DLL_NOT_COMPATIBLE:
     case AIO_ERROR_NOT_IMPLEMENTED   :
     case AIO_ERROR_INVALID_PARAMETER :
     case AIO_ERROR_OUT_OF_RESOURCES  :
        return ERROR_FROM_AUDIO_IO_DLL;
     case AIO_ERROR_FORMAT_NOT_SUPPORTED  :
     case AIO_ERROR_SAMPRATE_NOT_SUPPORTED:
        return WAVIN_ERR_NOTSUPPORTED;
     case AIO_ERROR_FILE_NOT_FOUND    :
     case AIO_ERROR_INPUT_NOT_OPEN    : return SOUNDIN_ERR_NOTOPEN;
     case AIO_ERROR_OUTPUT_NOT_OPEN   : return SOUNDOUT_ERR_NOTOPEN;
     case AIO_INVALID_HANDLE          :
     case AIO_ERROR_CANCELLED         :
     case AIO_ERROR_CANNOT_WRITE_YET  : return SOUNDOUT_ERR_NOTOPEN;
     case AIO_ERROR_CANNOT_READ_YET   : return SOUNDIN_ERR_NOTOPEN;
     default :  return ERROR_FROM_AUDIO_IO_DLL;
   }
} // end Sound_AudioIOErrorToMyErrorCode()
#endif // ( SWI_AUDIO_IO_SUPPORTED )


// ex: CSound *SoundDev;  /* global instance for both GENERATOR and ANALYSER */

//--------------------------------------------------------------------------
// Construction/Destruction
//--------------------------------------------------------------------------
CSound::CSound()  // constructor
{
  DOBINI();  // -> Sound_iSourceCodeLine
  m_iUsingASIO = 0;  // not using ASIO driver yet
#if( SWI_ASIO_SUPPORTED )
  m_hAsio = C_ASIOWRAP_ILLEGAL_HANDLE;
#endif
#if( SWI_AUDIO_IO_SUPPORTED )
  AIO_Host_InitInstanceData( &aio_In );
  AIO_Host_InitInstanceData( &aio_Out );
#endif

  m_pfltInterleaveBuffer = NULL;  // nothing allocated yet..
  m_i32InterleaveBufferSize = 0;


  m_iInputTimeout_ms = 500; // max. number of milliseconds to wait for input data
                            // (once fixed to 500 ms; user-defineable because
                            //  the ASIO driver for 'Dante' seemed to require more)

  m_InputOpen = m_OutputOpen = FALSE;
  m_hwvin = NULL;
  m_hwvout = NULL;
  m_InEventHandle = NULL;
  m_OutEventHandle = NULL;
  m_ErrorCodeIn  = 0;
  m_ErrorCodeOut = 0;
  m_sz255LastErrorStringIn[0] = 0;
  m_sz255LastErrorStringOut[0]= 0;
  m_LostBuffersIn= 0;  // nothing "lost" yet
  m_LostBuffersOut=0;
  m_NumInputBuffers= 0;   // explained in C:\cbproj\SoundUtl\Sound.h
  m_pbInputBuffer  = NULL;
  m_pbOutputBuffer = NULL;
  m_dwInputSelector  = 0x00003; // channel selection matrix for INPUT (ASIO only)
  m_dwOutputSelector = 0x00003; // channel selection matrix for OUTPUT (ASIO only)

  memset(&m_OutFormat, 0, sizeof(WAVEFORMATEX) );
  memset(&m_InFormat , 0, sizeof(WAVEFORMATEX) );

# if(sizeof(WAVEFORMATEX) != 18 )
#   error "This stupid compiler seems to have a problem with struct alignment"
  // For obscure reasons, this check failed when compiling the same project
  // on a 'new' PC, with a fresh installed C++ Builder V6 .
  // Cure: Use a MODIFIED ?\cbuilder6\include\mmsystem.h  for Borland C++ ,
  //       with something like the following before the definition of tWAVEFORMATEX
  // force 2-byte alignment, the "Borland" way :
  //  #ifdef __BORLANDC__
  //  # pragma pack(push,2)
  //  #endif // __BORLANDC__
# endif // (sizeof(WAVEFORMATEX) != 18 ) ?


#if( CSOUND_USE_CRITICAL_SECTION )
  memset(&m_CriticalSection, 0 , sizeof(m_CriticalSection) ); // what's this "LockCount"-thingy,
    // and why does it reach incredibly large values (like 0x81D4609C) ?
  InitializeCriticalSection(&m_CriticalSection);
  // Note: A critical section is one of the fastest synchronisation objects,
  //       because it doesn't require a kernel mode switch .
  //       ( -> http://msdn.microsoft.com/en-us/library/ms810428.aspx )
#endif // CSOUND_USE_CRITICAL_SECTION

  DOBINI();  // -> Sound_iSourceCodeLine
}

CSound::~CSound()
{
  DOBINI();  // -> Sound_iSourceCodeLine

  // Some caution must be taken if the buffers are still in use at this moment:
  if(m_InputOpen)
     InClose();
  if(m_OutputOpen)
     OutClose();
#if( SWI_ASIO_SUPPORTED )
  if( m_hAsio != C_ASIOWRAP_ILLEGAL_HANDLE )
   { AsioWrap_Close( m_hAsio );
   }
#endif // SWI_ASIO_SUPPORTED
#if( SWI_AUDIO_IO_SUPPORTED )
  AIO_Host_FreeDLL( &aio_In );
  AIO_Host_FreeDLL( &aio_Out );
#endif // SWI_AUDIO_IO_SUPPORTED
  if(m_pbInputBuffer)      // free buffers ...
   { UTL_free(m_pbInputBuffer);
     m_pbInputBuffer = NULL;
   }
  if(m_pbOutputBuffer)
   { UTL_free(m_pbOutputBuffer);
     m_pbOutputBuffer = NULL;
   }
  m_i32InterleaveBufferSize = 0;
  if(m_pfltInterleaveBuffer)
   { UTL_free(m_pfltInterleaveBuffer);
     m_pfltInterleaveBuffer = NULL;
   }

#if( SWI_AUDIO_IO_SUPPORTED )
  AIO_Host_DeleteInstanceData( &aio_In ); // counterpart to AIO_Host_InitInstanceData()
  AIO_Host_DeleteInstanceData( &aio_Out);
#endif

  if(m_InEventHandle != NULL)
   { CloseHandle(m_InEventHandle);
     m_InEventHandle = NULL;    // only deleted here in the destructor !
   }
  if(m_OutEventHandle != NULL)
   { CloseHandle(m_OutEventHandle);
     m_OutEventHandle = NULL;   // only deleted here in the destructor !
   }


#if( CSOUND_USE_CRITICAL_SECTION )
  DeleteCriticalSection(&m_CriticalSection);
#endif // ( CSOUND_USE_CRITICAL_SECTION )
  DOBINI();  // -> Sound_iSourceCodeLine
} // end CSound::~CSound() [Dr. Destructo]

//--------------------------------------------------------------------------
void CSound::SetInputSelector( DWORD dwInputSelector )
  // Only works for ASIO drivers.
  // Example: dwInputChannelMask=0x0003 selects the first and the second
  //          channel for input.
{
   m_dwInputSelector = dwInputSelector;
} // end CSound::SetInputSelector()

//--------------------------------------------------------------------------
void CSound::SetOutputSelector( DWORD dwOutputSelector )
{ // similar as above, but for OUTPUT channels
   m_dwOutputSelector = dwOutputSelector;
}

//--------------------------------------------------------------------------
void CSound::SetInputTimeout( int iInputTimeout_ms )
  // Sets the max. number of milliseconds to wait for input data
  //  This was once fixed to 500 ms; now user-defineable because
  //  the ASIO driver for 'Dante' seemed to require more than that.
{
  if( iInputTimeout_ms < 100 )
   {  iInputTimeout_ms = 100;
   }
  if( iInputTimeout_ms > 5000 )
   {  iInputTimeout_ms = 5000;
   }
  m_iInputTimeout_ms = iInputTimeout_ms;
}

//--------------------------------------------------------------------------
// VFO control (added HERE 2012-08-14, because some ExtIO-DLLs need this):
//--------------------------------------------------------------------------
BOOL CSound::HasVFO(void) // -> TRUE=VFO supported, FALSE=no. Used for ExtIO-DLLs.
{
#if( SWI_AUDIO_IO_SUPPORTED )
  return AIO_Host_HasVFO( &aio_In );
#else
  return FALSE;
#endif // ( SWI_AUDIO_IO_SUPPORTED ) ?
}

//--------------------------------------------------------------------------
long CSound::GetVFOFrequency(void) // -> current VFO tuning frequency in Hertz.
{
#if( SWI_AUDIO_IO_SUPPORTED )
  return AIO_Host_GetVFOFrequency( &aio_In );
#else
  return 0;
#endif // ( SWI_AUDIO_IO_SUPPORTED ) ?

}

//--------------------------------------------------------------------------
long CSound::SetVFOFrequency( // Return value explained in AudioIO.c (!)
              long i32VFOFrequency ) // [in] new VFO frequency in Hertz
{
#if( SWI_AUDIO_IO_SUPPORTED )
  m_i32VFOFrequency = i32VFOFrequency;  // may be required when "Starting" !
  return AIO_Host_SetVFOFrequency( &aio_In, i32VFOFrequency );
#else
  return 0;
#endif // ( SWI_AUDIO_IO_SUPPORTED ) ?
}

#if( SWI_ASIO_SUPPORTED )
//--------------------------------------------------------------------------
AsioHandle CSound::GetAsioHandle(void)
{ // Just a kludge to retrieve more info about "driver-in-use" .
  // See configuration dialog in Spectrum Lab for a very ugly example.
  return m_hAsio;   // handle for DL4YHF's ASIO wrapper
}
#endif // SWI_ASIO_SUPPORTED

//--------------------------------------------------------------------------
UINT CSound::InOpen(  // In Spectrum Lab, called from SoundThd_InitializeAudioDevices() ..
           char * pszAudioInputDeviceOrDriver, // [in] name of an audio device or DL4YHF's "Audio I/O driver DLL" (full path)
           char * pszAudioInputDriverParamStr, // [in] extra parameter string for driver or Audio-I/O / ExtIO-DLL
           char * pszAudioStreamID,   // [in] 'Stream ID' for the Audio-I/O-DLL, identifies the AUDIO SOURCE (here: optional)
           char * pszDescription,     // [in] a descriptive name which identifies
           // the audio sink ("audio reader", or "consumer"). This name may appear
           // on the "patch panel" on the configuration screen of the Audio-I/O DLL .
           // For example, the 1st running instance of Spectrum Lab will
           //     identify itself as "SpectrumLab1", the 2nd as "SpectrumLab2", etc .
           long i32SamplesPerSecond, // [in] "wanted" nominal sample rate - sometimes ignored !
           int  iBitsPerSample,     // ..per channel, usually 16 (also for stereo, etc)
           int  iNumberOfChannels,  // 1 or 2 channels (mono/stereo)
           DWORD BufSize,           // number of SAMPLES per buffer
                 // (must be at least as large as the 'processing chunk size';
                 //  see caller in SoundThd.cpp : SoundThd_InitializeAudioDevices )
           DWORD SampleLimit,       // use 0 (zero) for a continuous stream
           BOOL  start)
//  Opens a Soundcard for input, using MMSYSTEM or ASIO. Sample size can be
//  1,2 or 4 bytes long depending on bits per sample and number of channels.
//( ie. a 1000 sample buffer for 16 bit stereo will be a 4000 byte buffer)
//   Sampling begins immediately so the thread must start calling
//   one of the "Read"-functions to prevent buffer overflow.
//
//  Parameters:
//      pszAudioInputDeviceOrDriver = name of an AUDIO- or ASIO-device,
//                  or AUDIO-I/O-driver.  See examples further below.
//      i32SamplesPerSecond = sampling rate in Hertz .
//                  this parameter is sometimes ignored, for example
//                  if the 'input device or driver' is actually an ExtIO-DLL
//                  connected to a certain hardware, which only supports a fixed
//                  sampling rate (which may have got nothing in common with the
//                  standard soundcard sampling rates). To be prepared for such cases,
//                  the application should call CSound::InGetNominalSamplesPerSecond()
//                  after (!) CSound::InOpen() .
//      BufSize   = DWORD specifies the soundcard buffer size number
//                  in number of samples to buffer.
//                  If this value is Zero, the soundcard is opened and
//                  then closed. This is useful   for checking the
//                  soundcard.
//      SampleLimit = limit on number of samples to be read. 0 signifies
//                  continuous input stream.
//      start:  FALSE=only open input, but don't start
//              TRUE =open input and start sampling immediately.
//   Return value:
//        0           if opened OK
//        ErrorCode   if not
//
// Examples for pszAudioInputDeviceOrDriver :
//   "4 E-MU 0202 | USB"  (device ID and name of a windows multimedia device)
//
// IMPORTANT: For the ExtIO-DLLs, the radio's VFO FREQUENCY(!) must be set
//  BEFORE opening the "driver" for input, because for some reason,
//  the VFO frequency must be passed as a function argument in "StartHW" !
//  (failing to do so caused ExtIO_RTL.DLL to open a stupid message box
//   telling something about "PLL not locked", instead of silently returning
//   with an error code, and leaving the user interaction to the HOST) .
// Thus, call CSound::SetVFOFrequency() with a VALID tuning frequency,
//   to prevent the annoying message box (from the DLL) from popping up.
//   Details in AudioIO.c :: AIO_Host_OpenAudioInput() .
{
 int i;
 int newInBufferSize/*per "part"*/, newNumInputBuffers;
 long i32TotalBufSizeInByte;
 // char *cp;
 char *pszAudioDeviceName = pszAudioInputDeviceOrDriver;
 char *pszAudioDeviceNameWithoutPrefix;
#if( SWI_ASIO_SUPPORTED )      // Support ASIO audio drivers too ?
  ASIOChannelInfo *pAsioChannelInfo;
#endif
#if( SWI_AUDIO_IO_SUPPORTED )
  int iAIOErrorCode;  // negative error codes from AudioIO.h (for audio-I/O-DLLs)
#endif

  double dblRealizedSampleRate;

  DOBINI();  // -> Sound_iSourceCodeLine
  if(m_InputOpen)
      InClose();
  m_hwvin = NULL;    // forget handle for MMSYSTEM "wave-input"
  m_InputOpen = FALSE;
  m_InWaiting  = FALSE;
  if( m_InEventHandle != NULL )
    { ResetEvent(m_InEventHandle); // let WaitForInput() wait when it needs to (!)
    }
  m_InOverflow = FALSE;
  m_iInHeadIndex  = 0;  // index for ENTERING new samples into buffer's "head"
  m_iInTailIndex  = 0;  // index for TAKING OUT samples from the buffer's "tail"
#if( SWI_AUDIO_IO_SUPPORTED )
  m_ExtIONeedsSoundcardForInput = FALSE;
#endif
  m_InBufPosition = 0;
  memset( m_InBufInfo, 0, sizeof(m_InBufInfo) );
  for(i=0; i<SOUND_MAX_INPUT_BUFFERS; ++i)
   { m_InBufInfo[i].iBufferIndex = i;
   }

  m_i64NumSamplesRead = 0;   // number of samples read by the application
  m_ldblSampleBasedInputTimestamp = 0.0; // input timestamp still unknown
  m_dblNominalInputSampleRate  // initial values for the sampling rate..
     // (the accuracy may be improved if the caller knows a 'calibrated' SR;
     //  see notes in CSound::InReadStereo )
    = m_dblMeasuredInputSampleRate = dblRealizedSampleRate
    = i32SamplesPerSecond; // initial value for the 'measured' SR (MUST ALWAYS BE VALID)
  m_nMeasuredSampleRates = 0; // no averaging for "measure" sampling rates yet
  m_dblInputSRGateCount = m_dblInputSRGateTime = 0.0;

  // ex: m_InFormat = *pWFX; // copy a lot of params from WAVEFORMATEX struct
  // WoBu didn't want to have this damned "waveformatex"-stuff in the interface,
  // so got rid of this micro$oft-specific stuff, and use it only internally :
  m_InFormat.wFormatTag = WAVE_FORMAT_PCM;
     // Since 2003-12-14, the "number of channels" for the ADC is not necessarily
     //       the same as the number of channels in the internal process !
  m_InFormat.nChannels = (WORD)iNumberOfChannels;   // only 1 or 2 so far, regardless of interleaved/non-interleaved output later
  m_InFormat.wBitsPerSample = (WORD)iBitsPerSample;  // ex 16
  m_InFormat.nSamplesPerSec = i32SamplesPerSecond; /* ex 11025 */
     // note that the WFXSettings require the "uncalibrated" sample rate !!
  m_InFormat.nBlockAlign = (WORD)( m_InFormat.nChannels *(m_InFormat.wBitsPerSample/8) );
  m_InFormat.nAvgBytesPerSec = m_InFormat.nSamplesPerSec * m_InFormat.nBlockAlign;
  m_InFormat.cbSize = 0/*!*/; // no "extra format information" appended to the end of the WAVEFORMATEX structure
  m_InBytesPerSample = (m_InFormat.wBitsPerSample/8)*m_InFormat.nChannels;  // for ASIO, this is a WISH (!)
  m_InFormat.cbSize= 0/*!*/;  // no "extra format information" appended to the end of the WAVEFORMATEX structure
  m_sz255LastErrorStringIn[0] = 0;

  if( CountBits(m_dwInputSelector) < m_InFormat.nChannels )
   { m_dwInputSelector = ( 0xFFFFFFFF >> (32-m_InFormat.nChannels) );
   }

  pszAudioDeviceNameWithoutPrefix = Sound_GetAudioDeviceNameWithoutPrefix(
                             pszAudioInputDeviceOrDriver, &m_iInputDeviceID );
  // Note: At this point, m_iInputDeviceID is simply the decimal number
  //       parsed from the begin of the device name in Sound_GetAudioDeviceNameWithoutPrefix() .
  //       Sound_InputDeviceNameToDeviceID() does a more thorough job:
  //       It ENUMERATES the available wave input devices (using a windows API function),
  //       and compares the *DEVICES NAMES* with the audio device name:
  SOUND_fDumpEnumeratedDeviceNames = TRUE;  // flag for Sound_InputDeviceNameToDeviceID()
  // later: iSoundInputDeviceID = Sound_InputDeviceNameToDeviceID( pszAudioInputDeviceOrDriver );



#if( SWI_AUDIO_IO_SUPPORTED )
  // Use DL4YHF's  "Audio-IO" for input ?  (planned..)
  if( m_iInputDeviceID == C_SOUND_DEVICE_AUDIO_IO )
   {
#if( SWI_AUDIO_IO_SUPPORTED )
     if( aio_In.h_AudioIoDLL == NULL )
      { UTL_WriteRunLogEntry( "CSound::InOpen: Trying to load DLL \"%s\"",
                              (char*)pszAudioInputDeviceOrDriver );
        iAIOErrorCode = AIO_Host_LoadDLL( &aio_In, pszAudioInputDeviceOrDriver );
        if( iAIOErrorCode < AIO_NO_ERROR ) // error codes from AudioIO.h are NEGATIVE !
         {
           m_ErrorCodeIn = ConvertErrorCode_AIO_to_SOUND(iAIOErrorCode);
           sprintf( m_sz255LastErrorStringIn, "Failed to load %s", pszAudioInputDeviceOrDriver );
           DOBINI();
           return m_ErrorCodeIn;
         }
       }
      // Problem with some ExtIO-DLLs (in AIO_Host_OpenAudioInput) :
      //  Some of these drivers need to send the VFO TUNING FREQUENCY
      //  to the 'radio' (RTL-SDR) before starting [in StartHW()], so that info must be
      //  passed to the ExtIO-DLL-host *before* calling AIO_Host_OpenAudioInput().
      // For other kinds of 'radio control drivers', it works the other way round:
      //  FIRST open the driver / "start the hardware", THEN set the VFO frequeny. Holy shit.
      AIO_Host_SetVFOFrequency( &aio_In, // set VFO tuning frequency in Hertz.
                m_i32VFOFrequency); // must have been already set because of trouble with StartHW() in certain ExtIO-DLLs !
      if( AIO_Host_OpenAudioInput( // implemented in C:\cbproj\AudioIO\AudioIO.c
                &aio_In,           // [in,out] DLL-host instance data for the INPUT
                pszAudioStreamID,  // [in] 'Stream ID' for the Audio-I/O-DLL, identifies the AUDIO SOURCE (here: optional)
                pszDescription,    // [in] a descriptive name which identifies
                i32SamplesPerSecond,     // [in(!)] "wanted" sample rate, often IGNORED (reasons in AudioIO.c..)
                &dblRealizedSampleRate,  // [out(!)] realized samples per second, dictated by hardware or driver
                iNumberOfChannels,   // [in] number of channels IN EACH SAMPLE-POINT
                200,                 // [in] iTimeout_ms, max timeout in milliseconds for THIS call
                     // (may have to wait here for a short time, for example when...
                     //   - the I/O lib needs to communicate with an external device (to open it)
                     //   - the I/O lib needs to wait until the 'writer' (=the other side)
                     //     has called AIO_Host_OpenAudioOutput() and set the sampling rate)
                AIO_OPEN_OPTION_NORMAL, // [in] bit combination of AIO_OPEN_OPTION_... flags
                NULL // [out,optional] T_ChunkInfo *pOutChunkInfo, see c:\cbproj\SoundUtl\ChunkInfo.h, MAY BE NULL (!)
            ) < 0 )
       {   // failed to open the AUDIO-I/O-DLL for "input" :
           m_ErrorCodeIn = SOUNDIN_ERR_NOTOPEN;
           strcpy( m_sz255LastErrorStringIn, "Couldn't open AUDIO_IO for input" );
           DOBINI();
           return m_ErrorCodeIn;
       }
      else // successfully opened the AUDIO-I/O-DLL for "input"... but that's not all:
       {
           if( dblRealizedSampleRate > 0.0 )
            {  m_dblMeasuredInputSampleRate = dblRealizedSampleRate;
               m_InFormat.nSamplesPerSec = (int)(dblRealizedSampleRate+0.49); // kludge for InGetNominalSamplesPerSecond()
               // With ExtIO_RTL.DLL, got here with m_dblMeasuredInputSampleRate = 2400000 (2.4 MHz!)
               //      which is more than many 'old workhorses' can handle,
               //      even though the requested SR was 9000001 Hz which,
               //      according to the RTL-SDR documentation, should be possible.
               // IS THERE NO WAY TO INSTRUCT THE SDR TO USE "OUR" SAMPLE RATE,
               // WITHOUT HAVING TO OPEN "THEIR" CONTROL PANEL ?!

            }
           m_InputOpen = TRUE;  // Note: m_InputOpen only becomes true AFTER buffer allocation was ok
           DOBINI();  // -> Sound_iSourceCodeLine
           // Added 2012-10-28 to support FiFi-SDR: Some ExtIO-DLLs (ExtIO_Si570.dll)
           //  rely on the ordinary soundcard as an input devices. Check this:
           if( aio_In.i32ExtIOHWtype == EXTIO_HWT_Soundcard /*4*/ )
            { // > The audio data are returned via the sound card managed by the host. 
              // > The external hardware just controls the LO, 
              // > and possibly a preselector, under DLL control. 
              // In this case, Sound.cpp + AudioIO.c are the 'host' (for the DLL),
              // so the ugly soundcard processing shall be performed here.
              // In Spectrum Lab, there is already a software module for the
              // soundcard-input processing (Sound.cpp) so we keep the soundcard-stuff
              // out of the 'DLL host'. Instead, the soundcard processing remains where it was:
              // in C:\cbproj\SoundUtl\Sound.cpp : CSound::InOpen() .
              m_ExtIONeedsSoundcardForInput = TRUE;  // using ExtIO to control the radio, but the soundcard to acquire samples
              // Because in this case, 'pszAudioInputDeviceOrDriver' is occupied
              // by the NAME OF THE ExtIO-DLL, it cannot be used (further below)
              // as the NAME OF THE AUDIO INPUT DEVICE aka "soundcard" device.
              // Kludge: Pass the name of the soundcard in the 'extra parameter string':
              pszAudioDeviceName = NULL;
              if( pszAudioInputDriverParamStr != NULL )
               { if( pszAudioInputDriverParamStr[0] != '\0' )
                  { pszAudioDeviceName = pszAudioInputDriverParamStr;
                    UTL_WriteRunLogEntry( "CSound::InOpen: Using ExtIO with soundcard, device name: %s",
                                          (char*)pszAudioDeviceName );
                  }
               }
              if( pszAudioDeviceName==NULL )
               { // Name of the SOUNDCARD device not specified (after the name of the ExtIO-DLL) .
                 // Make an educated guess :
                 if( stricmp( aio_In.extHWname, "SoftRock Si570") == 0 )  // ExtIO used for the FiFi-SDR = "ExtIO_Si570.dll"
                  { pszAudioDeviceName = "FiFiSDR Soundcard"; // name of FiFi-SDR's internal(!) soundcard
                    // Called much further below, Sound_InputDeviceNameToDeviceID()
                    // will find the windoze multimedia / "wave audio" device ID
                    // for this name. Storing the number anywhere is stupid,
                    // because it changes from day to day, depending on the USB
                    // port to which the FiFi-SDR is plugged.
                  }
                 else
                  { pszAudioDeviceName = "-1";  // use the standard 'wave input' device
                  }
                 UTL_WriteRunLogEntry( "CSound::InOpen: Using ExtIO with soundcard, guessed device name: %s",
                              (char*)pszAudioDeviceName );
               }
              pszAudioDeviceNameWithoutPrefix = pszAudioDeviceName;
              // NO 'return' in this case !
            } // end if( pInstData->i32ExtIOHWtype == EXTIO_HWT_Soundcard /*4*/ )
           else // In all other cases, the Audio-I/O or ExtIO-DLL provides the samples:
            { m_ExtIONeedsSoundcardForInput = FALSE;
              return 0;  // "no error" (from CSound::InOpen)
            }
       }
#endif  // SWI_AUDIO_IO_SUPPORTED
   } // end if( m_iInputDeviceID == C_SOUND_DEVICE_AUDIO_IO )
  else // do NOT use the "audio I/O DLL" for input :
   { AIO_Host_FreeDLL( &aio_In ); // ... so free it, if loaded earlier
   }
#endif // SWI_AUDIO_IO_SUPPORTED


  // use MMSYSTEM or ASIO driver for input ?
  m_iUsingASIO &= ~1;  // up to now, NOT using ASIO for input....
  if(  (m_iInputDeviceID==C_SOUND_DEVICE_ASIO) // pszAudioDriverName = name of an ASIO driver
    && APPL_fMayUseASIO )     // Since 2011-08-09, use of ASIO may be disabled through the command line
   {
#if( SWI_ASIO_SUPPORTED )      // Support ASIO audio drivers too ?
     if((m_iUsingASIO&1)==0 ) // only if ASIO driver not already in use for INPUT..
      {
       if( !AsioWrap_fInitialized )
        {  AsioWrap_Init();     // init DL4YHF's ASIO wrapper if necessary
        }
       // Prepare the most important ASIO settings :
       AsioSettings MyAsioSettings;
       AsioWrap_InitSettings( &MyAsioSettings,
                              iNumberOfChannels,
                              i32SamplesPerSecond,
                              iBitsPerSample );
       if( m_hAsio == C_ASIOWRAP_ILLEGAL_HANDLE )
        {  // Only open the ASIO driver if not open already. Reason:
           // An ASIO device always acts as IN- AND(!) OUTPUT simultaneously.
           UTL_WriteRunLogEntry( "CSound::InOpen: Trying to open ASIO-driver \"%s\"",
                              (char*)pszAudioInputDeviceOrDriver );

           m_hAsio = AsioWrap_Open( pszAudioDeviceNameWithoutPrefix, &MyAsioSettings, CSound_AsioCallback, (DWORD)this );
             // Note: AsioWrap_Open() also creates the 'buffers' in the driver !
           if( (m_hAsio==C_ASIOWRAP_ILLEGAL_HANDLE) && (AsioWrap_sz255LastErrorString[0]!=0) )
            { // immediately save error string for later:
              strncpy( m_sz255LastErrorStringIn, AsioWrap_sz255LastErrorString, 79 );
              m_sz255LastErrorStringIn[79] = '\0';
            }
        }
       if( m_hAsio != C_ASIOWRAP_ILLEGAL_HANDLE ) // ASIO wrapper willing to co-operate ?
        { m_iUsingASIO |= 1;    // now using ASIO for input...
           // BEFORE allocating the buffers, check if the ASIO driver will be kind enough
           // to deliver the samples in the format *WE* want .
           // ( some stubborn drivers don't, for example Creative's )
           pAsioChannelInfo = AsioWrap_GetChannelInfo( m_hAsio, 0/*iChnIndex*/, 1/*iForInput*/ );
           if( pAsioChannelInfo!=NULL )
            { switch(pAsioChannelInfo->type)
               { // SOO many types, so hope for the best, but expect the worst .....  see ASIO.H !
                 case ASIOSTInt16MSB   :
                      m_InFormat.wBitsPerSample = 16;
                      break;
                 case ASIOSTInt24MSB   : // used for 20 bits as well
                      m_InFormat.wBitsPerSample = 24;
                      break;
                 case ASIOSTInt32MSB   :
                      m_InFormat.wBitsPerSample = 32;
                      break;
                 case ASIOSTFloat32MSB : // IEEE 754 32 bit float
                      m_InFormat.wBitsPerSample = 32;
                      break;
                 case ASIOSTFloat64MSB : // IEEE 754 64 bit double float
                      m_InFormat.wBitsPerSample = 64;
                      break;

                 // these are used for 32 bit data buffer, with different alignment of the data inside
                 // 32 bit PCI bus systems can be more easily used with these
                 case ASIOSTInt32MSB16 : // 32 bit data with 16 bit alignment
                      m_InFormat.wBitsPerSample = 32;
                      break;
                 case ASIOSTInt32MSB18 : // 32 bit data with 18 bit alignment
                      m_InFormat.wBitsPerSample = 32;
                      break;
                 case ASIOSTInt32MSB20 : // 32 bit data with 20 bit alignment
                      m_InFormat.wBitsPerSample = 32;
                      break;
                 case ASIOSTInt32MSB24 : // 32 bit data with 24 bit alignment
                      m_InFormat.wBitsPerSample = 32;
                      break;
                 case ASIOSTInt16LSB   : // 16 bit, used by the 'ASIO Sample' driver !
                      m_InFormat.wBitsPerSample = 16;
                      break;
                 case ASIOSTInt24LSB   : // used for 20 bits as well
                      m_InFormat.wBitsPerSample = 24;
                      break;
                 case ASIOSTInt32LSB   : // This is what Creative's "updated" driver seems to use,
                      m_InFormat.wBitsPerSample = 32; // interestingly NOT one of those many 24-bit types.
                      break;
                 case ASIOSTFloat32LSB : // IEEE 754 32 bit float, as found on Intel x86 architecture
                      m_InFormat.wBitsPerSample = 32;
                      break;
                 case ASIOSTFloat64LSB : // IEEE 754 64 bit double float, as found on Intel x86 architecture
                      m_InFormat.wBitsPerSample = 64;
                      break;

                 // these are used for 32 bit data buffer, with different alignment of the data inside
                 // 32 bit PCI bus systems can more easily used with these
                 case ASIOSTInt32LSB16 : // 32 bit data with 18 bit alignment
                 case ASIOSTInt32LSB18 : // 32 bit data with 18 bit alignment
                 case ASIOSTInt32LSB20 : // 32 bit data with 20 bit alignment
                 case ASIOSTInt32LSB24 : // 32 bit data with 24 bit alignment
                      m_InFormat.wBitsPerSample = 32;
                      break;

                 // ASIO DSD format.
                 case ASIOSTDSDInt8LSB1: // DSD 1 bit data, 8 samples per byte. First sample in Least significant bit.
                 case ASIOSTDSDInt8MSB1: // DSD 1 bit data, 8 samples per byte. First sample in Most significant bit.
                 case ASIOSTDSDInt8NER8: // DSD 8 bit data, 1 sample per byte. No Endianness required.
                      m_InFormat.wBitsPerSample = 8;
                      break;
                 default:
                      strcpy( m_sz255LastErrorStringIn, "ASIO data type not supported" );
                      break;
                } // end switch(pAsioChannelInfo->type)
               // Revise the "number of bytes per sample" based on the above types:
               m_InBytesPerSample = (m_InFormat.wBitsPerSample/8) * m_InFormat.nChannels;
            } // end if < got info about this ASIO input channel >
           else
            { // got no "info" about this ASIO channel -> ignore it
            }
         } // end if < successfully opened the ASIO driver >
        else  // bad luck (happens !) :  ASIO driver could not be loaded
         { m_ErrorCodeIn = ERROR_WITH_ASIO_DRIVER;
           UTL_WriteRunLogEntry( "CSound::InOpen: Failed to open ASIO-driver !" );
           return m_ErrorCodeIn;
        }
      } // end if < ASIO driver not loaded yet >
#endif // SWI_ASIO_SUPPORTED
   } // end if < use ASIO driver instead of MMSYSTEM ? >

  // Allocate and prepare all the output buffers ....
  if( BufSize > 0 )  // only if not just a soundcard check ...
   {
     // How many buffers, and how large should each part be ?
     //    Ideally enough for 0.2 to 0.5 seconds;
     //    but for some applications we need the lowest possible latency.
     //    In those cases, only ONE of these buffers will be waiting to be filled.
     //    Spectrum Lab will ask for 'BufSize' as large as the PROCESSING CHUNKS
     //    (details on that in SoundThd.cpp :: SoundThd_InitializeAudioDevices ),
     //    Example:  96 kHz, approx. 0.5 second chunks -> BufSize=65536 [SAMPLES].
     if( BufSize < m_InFormat.nSamplesPerSec/20 )
      {  BufSize = m_InFormat.nSamplesPerSec/20;  // at least 50 ms of audio !!
      }
     // Some authors (also plaged by the obfuscated 'wave audio' API) noted that
     //    the size of a single buffer passed to waveOutWrite / waveInAddBuffer
     //    must not exceed 64 kBytes (here: m_InBufferSize) .   See
     // http://v3.juhara.com/en/articles/multimedia-programming/17-sound-playback-with-wave-api :
     // > We can send all WAV data to device driver by calling waveOutWrite once,
     // > but please note that, on some soundcards (especially the old ones),
     // > maximum buffer size can be processed is 64 KB, so if you have waveform data
     // > bigger than 64 KB, waveOutWrite must be called more than once with smaller data blocks.
     i32TotalBufSizeInByte = (long)(2*BufSize) * m_InBytesPerSample; // may be as much as a HALF MEGABYTE !
     i = (i32TotalBufSizeInByte+3) / 4/*parts*/; // -> approx size of each buffer-part, in BYTES.
     if(i<1024)  i=1024;    // at least 512 bytes(!) per buffer-part
     if(i>65536) i=65536;   // maximum  64 kByte per buffer-part
     newNumInputBuffers = (i32TotalBufSizeInByte+i-1) / i;
     if( newNumInputBuffers < 4)  // should use at least FOUR buffers, and..
         newNumInputBuffers = 4;
     if( newNumInputBuffers > SOUND_MAX_INPUT_BUFFERS ) // a maximum of SIXTEEN buffers..
         newNumInputBuffers = SOUND_MAX_INPUT_BUFFERS;
     // Now, after limiting the NUMBER OF BUFFERS, how large should each buffer-part be ?
     // For example, the caller (Spectrum Lab : SoundThd.cpp, SoundThd_InitializeAudioDevices)
     // may want to read or write samples in blocks of 65536 samples each (=BufSize) .
     // The TOTAL internal buffer size must be at least twice that value,
     // because one of those 4..16 buffer-parts is always occupied by the multimedia driver,
     // while the other parts are being filled, or already filled and waiting for in/output.
     i = (i32TotalBufSizeInByte + newNumInputBuffers/*round up*/ - 1)  / newNumInputBuffers;
     // For what it's worth, the size of each 'buffer part' should be a power of two:
     newInBufferSize = 512;  // .. but at last 512  [bytes per 'buffer part']
     while( (newInBufferSize < i) && (newInBufferSize < 65536) )
      { newInBufferSize <<= 1;
      }
     // EXAMPLES:
     //   96kHz, with 500 milliseconds per chunk: newInBufferSize=65536, newNumInputBuffers=8
     //
     //
     //
     DOBINI();  // -> Sound_iSourceCodeLine
     if(m_pbInputBuffer)
      { // free old buffer (with possibly different SIZE) ?
        if( (m_InBufferSize!=newInBufferSize) || (m_NumInputBuffers!=newNumInputBuffers) )
         { m_InBufferSize = m_NumInputBuffers = 0; // existing buffers are now INVALID
           // Sleep(10); // give other threads the chance to stop using the buffer (kludge.. now unnecessary)
           UTL_free(m_pbInputBuffer);
           m_pbInputBuffer = NULL;
         }
      }
     if( m_pbInputBuffer==NULL )
      {
         m_pbInputBuffer = (BYTE*)UTL_NamedMalloc( "TSoundIn", newNumInputBuffers * newInBufferSize);
         m_InBufferSize    = newInBufferSize/*per "part"*/;
         m_NumInputBuffers = newNumInputBuffers;
      }
     DOBINI();  // -> Sound_iSourceCodeLine
     if ( m_pbInputBuffer==NULL )
      { // bad luck, the requested memory could not be allocated !
       m_ErrorCodeIn = MEMORY_ERROR;
       return m_ErrorCodeIn;
      }
    } // end if < not just a soundcard test >

   m_InSampleLimit = SampleLimit;
   DOBINI();

   // event for callback function to notify new buffer available
   if( m_InEventHandle == NULL )
    {  m_InEventHandle = CreateEvent(NULL, FALSE,FALSE,NULL);
    }

  DOBINI();

  // Open sound card input and get handle(m_hwvin) to device,
  // depends on whether MMSYSTEM or ASIO driver shall be used :
  if( m_iUsingASIO & 1 )   // using ASIO for input :
   {
#if( SWI_ASIO_SUPPORTED )      // Support ASIO audio drivers too ?
      // Already opened the driver above, but did NOT start it then,
      // because the buffers were not allocated yet.
#endif // SWI_ASIO_SUPPORTED
   }
  else // not using ASIO driver for input but MMSYSTEM :
   { int iSoundInputDeviceID;
     //  At this point, m_iInputDeviceID is simply the decimal number
     //       parsed from the begin of the device name in Sound_GetAudioDeviceNameWithoutPrefix() .
     //       Sound_InputDeviceNameToDeviceID() does a more thorough job:
     //       It ENUMERATES the available wave input devices (using a windows API function),
     //       and compares the *DEVICES NAMES* with the audio device name:
     SOUND_fDumpEnumeratedDeviceNames = TRUE;  // flag for Sound_InputDeviceNameToDeviceID()
     iSoundInputDeviceID = Sound_InputDeviceNameToDeviceID( pszAudioDeviceName );

     if (iSoundInputDeviceID < 0)
         iSoundInputDeviceID = WAVE_MAPPER;
#  if( 1 )  // (1)=normal compilation, (0)=TEST to force using the WAVEFORMATEXTENSIBLE-
     UTL_WriteRunLogEntry( "CSound::InOpen: Trying to open \"%s\", ID #%d, %d Hz, %d bit, %d channel(s), for input.",
                              (char*)pszAudioDeviceNameWithoutPrefix,
                              (int)iSoundInputDeviceID,
                              (int)m_InFormat.nSamplesPerSec,
                              (int)m_InFormat.wBitsPerSample,
                              (int)m_InFormat.nChannels );
     // Note: It is not clear why sometimes, deep inside waveInOpen(),
     //       the Borland debugger stops on 'ntdll.DbgBreakPoint'  .
     if( (m_ErrorCodeIn = waveInOpen( &m_hwvin, iSoundInputDeviceID, &m_InFormat,
         (DWORD)WaveInCallback, (DWORD)this, CALLBACK_FUNCTION ) )
                  != MMSYSERR_NOERROR )
#endif  // (1)=normal compilation, (0)=TEST
      { // Could not open the WAVE INPUT DEVICE with the old-fashioned way.
        //    Try something new, using a WAVEFORMATEXTENSIBLE structure ...
        WAVEFORMATEXTENSIBLE wfmextensible;
#      if(sizeof(WAVEFORMATEXTENSIBLE) != 40 )
#        error "This stupid compiler seems to have a problem with struct alignment"
#      endif
        UTL_WriteRunLogEntry( "CSound::InOpen: waveInOpen failed. Trying again to open audio device for input, using WAVEFORMATEXTENSIBLE" );
        // don't know what may be in it, so clear everything (also unknown components)
        memset( &wfmextensible, 0, sizeof(WAVEFORMATEXTENSIBLE) );
        // Now set all required(?) members. Based on info found from various sources.
        // Note that the new WAVEFORMATEXTENSIBLE includes the old WAVEFORMATEX .
        wfmextensible.Format = m_InFormat;  // COPY the contents of the WAVEFORMATEX .
        wfmextensible.Format.cbSize     = sizeof(WAVEFORMATEXTENSIBLE);
        wfmextensible.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
        // wfmextensible.Format.nChannels, .wBitsPerSample, .nBlockAlign,
        //                     .nSamplesPerSec, .nAvgBytesPerSec already set !
        wfmextensible.Samples.wValidBitsPerSample = wfmextensible.Format.wBitsPerSample;
        wfmextensible.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
        // Now make a guess for the speaker configuration :
        wfmextensible.dwChannelMask = SPEAKER_FRONT_LEFT;   // MONO ...
        if(wfmextensible.Format.nChannels >= 2)             // STEREO..
           wfmextensible.dwChannelMask |= SPEAKER_FRONT_RIGHT;
        if(wfmextensible.Format.nChannels >= 3)             // ... and more ...
           wfmextensible.dwChannelMask |= SPEAKER_FRONT_CENTER;
        if(wfmextensible.Format.nChannels >= 4)
           wfmextensible.dwChannelMask |= SPEAKER_LOW_FREQUENCY;
        if(wfmextensible.Format.nChannels >= 5)
           wfmextensible.dwChannelMask |= SPEAKER_BACK_LEFT ;
        if(wfmextensible.Format.nChannels >= 6)             // typical "5.1" system
           wfmextensible.dwChannelMask |= SPEAKER_BACK_RIGHT;

        // Try again to open the audio input device, this time passing a pointer
        //  to a WAVEFORMATEXTENSIBLE struct (instead of the old WAVEFORMATEX ) :
        if( (m_ErrorCodeIn = waveInOpen( &m_hwvin, iSoundInputDeviceID,
             (WAVEFORMATEX*)&wfmextensible ,
             (DWORD)WaveInCallback, (DWORD)this, CALLBACK_FUNCTION ) )
                  != MMSYSERR_NOERROR )
         {
           // Arrived here ? waveInOpen() worked neither the OLD STYLE nor the NEW WAY.
           UTL_WriteRunLogEntry( "CSound::InOpen: Failed to open multimedia audio device for input" );

           InClose();   // m_ErrorCode = MMSYSERR_xxx
           // 2012-03-10: Got here when trying to open an E-MU 0202 for input,
           //             with 24 bits / sample, 1 channel, 11025 samples/second.
           //  Error message:  "unsupported wave format in SoundInOpen" .
           //  In an old version (V2.77 b03 from 2011_10_05), it still worked.
           //  But after the same old version was compiled with Borland C++ Builder V6,
           //  the same error ("unsupported wave format") also appeared;
           //  so obviously Borland made some changes in their "mmsystem.h"
           //  causing a serious compatibility problem ?
           //  At least, the problem could be fixed by modifying "mmsystem.h"
           //  by inserting the following before typedef struct tWAVEFORMATEX ...
           //
           // #ifdef __BORLANDC__
           // # pragma pack(push,2)
           // #endif // __BORLANDC__
           //
           //  .. and the following AFTER the structure definition:
           //
           // #ifdef __BORLANDC__
           // # pragma pack(pop)
           // #endif // __BORLANDC__
           //
           return m_ErrorCodeIn;
         }
      } // end if <could not open the WAVE INPUT DEVICE with the old-fashioned way>
   } // end else < use MMSYSTEM, not ASIO >


  if( BufSize == 0 )  // see if just a soundcard check
   {
     InClose();   // if so close the soundcard input
     return 0;
   }

  UTL_WriteRunLogEntry( "CSound::InOpen: Creating %d input buffers for 'wave' input",
                        (int)m_NumInputBuffers );

  for(i=0; i<m_NumInputBuffers; i++ )
   {
     // initialize WAVEHDR structures
     m_InputWaveHdr[i].dwBufferLength = m_InBufferSize; // number of BYTES (!)
     m_InputWaveHdr[i].dwFlags = 0;
     m_InputWaveHdr[i].dwUser = 0; // ex NULL (but this is NO pointer)
     m_InputWaveHdr[i].dwBytesRecorded = 0; // ex NULL (this is NO POINTER!)
     m_InputWaveHdr[i].lpData = (LPSTR) m_pbInputBuffer + i*m_InBufferSize;
     if( m_hwvin != NULL )
      { // only if MMSYSTEM used :
        DOBINI();
        if( (m_ErrorCodeIn = waveInPrepareHeader(m_hwvin, &m_InputWaveHdr[i],
                             sizeof(WAVEHDR))) != MMSYSERR_NOERROR )
         {
           InClose( );   // m_ErrorCodeIn = MMSYSERR_xxx
           return m_ErrorCodeIn;
         }
        // The waveInAddBuffer function sends an input buffer to the given
        // waveform-audio input device.
        // When the buffer is filled, the application is notified.
        DOBINI();
        if( (m_ErrorCodeIn = waveInAddBuffer(m_hwvin, &m_InputWaveHdr[i],
                             sizeof(WAVEHDR)) )!= MMSYSERR_NOERROR )
         {
           InClose();   // m_ErrorCodeIn = MMSYSERR_xxx
           return m_ErrorCodeIn;
         }
      } // end if( m_hwvin != NULL )

    } // end for <all input-buffers>

  DOBINI();

  /* start input capturing to buffer */
  if(start)
   {
     if( m_iUsingASIO & 1 )   // using ASIO for input :
      { // Start the ASIO driver (for BOTH INPUT AND OUTPUT !)
#if( SWI_ASIO_SUPPORTED )      // Support ASIO audio drivers too ?
        UTL_WriteRunLogEntry( "CSound::InOpen: Starting input from ASIO driver" );
        if( ! AsioWrap_StartOrStop( m_hAsio, TRUE/*fStart*/ ) )
         { if( m_sz255LastErrorStringIn[0]==0 && AsioWrap_sz255LastErrorString[0]!=0 )
            { // save error string for later:
              strncpy( m_sz255LastErrorStringIn, AsioWrap_sz255LastErrorString, 79 );
              m_sz255LastErrorStringIn[79] = '\0';
            }
           m_ErrorCodeIn = ERROR_WITH_ASIO_DRIVER;
           InClose();   // m_ErrorCodeIn = MMSYSERR_xxx
           return m_ErrorCodeIn;
         }
#endif // ( SWI_ASIO_SUPPORTED )
      }
     else // not using ASIO driver for input but MMSYSTEM :
     if( m_hwvin != NULL )
      { // only if MMSYSTEM used :
        UTL_WriteRunLogEntry( "CSound::InOpen: Starting input from multimedia driver" );
        if( (m_ErrorCodeIn = waveInStart(m_hwvin) )!= MMSYSERR_NOERROR )
         {
           InClose();   // m_ErrorCodeIn = MMSYSERR_xxx
           return m_ErrorCodeIn;
         }
      }
   } // end if(start)
  UTL_WriteRunLogEntry( "CSound::InOpen: Successfully opened audio input" );
  m_InputOpen = TRUE;  // Note: m_InputOpen only becomes true AFTER buffer allocation was ok
  DOBINI();  // -> Sound_iSourceCodeLine
  return 0;  // "no error" (from CSound::InOpen)
} // end CSound::InOpen()


//--------------------------------------------------------------------------
BOOL CSound::IsInputOpen(void)   // checks if the sound input is opened
{
  return m_InputOpen;
}

//--------------------------------------------------------------------------
BOOL CSound::WaitForInputData(int iTimeout_ms)  // does what the name says,
  //          but only waits if the input buffers are ALL empty.
  // Return value:  TRUE = "there's at least ONE buffer filled with samples
  //                        received from the soundcard";
  //                FALSE= "sorry, have been waiting for data
  //                        but nothing arrived from the soundcard" .
  //
{
  LONGLONG i64TimeToLeave, i64Tnow, i64TimerFrequency;  // added 2011-03-13
  int i;

  QueryPerformanceCounter( (LARGE_INTEGER*)&i64Tnow );
  QueryPerformanceFrequency( (LARGE_INTEGER*)&i64TimerFrequency );
  i64TimeToLeave = i64Tnow + ((LONGLONG)iTimeout_ms * i64TimerFrequency) / 1000;  // -> max. 500 (?) milliseconds to spend here

#if( CSOUND_USE_CRITICAL_SECTION )
  EnterCriticalSection(&m_CriticalSection);
#endif
  if( m_iInTailIndex == m_iInHeadIndex )  // if no buffer is filled...
   {
      // wait for mmsystem (or ASIO) to fill up a new buffer
      m_InWaiting = TRUE;
#    if( CSOUND_USE_CRITICAL_SECTION )
      LeaveCriticalSection(&m_CriticalSection);
#    endif
      DOBINI();  // -> Sound_iSourceCodeLine
      i = iTimeout_ms / 50;
      while( (i--)>0 )
       {
         DOBINI();   // -> Sound_iSourceCodeLine
         Sleep(50);  // wait a little longer for the thread to terminate itself
                     // Caution: Sleep() may actually take LONGER than expected !
         DOBINI();
         if( m_iInTailIndex != m_iInHeadIndex )
          { break;   // bingo; WaveInCallback() has sent us another buffer
          }
         if( !m_InputOpen ) // return -1 if no inputs are active
          { UTL_WriteRunLogEntry( "SOUND: Warning; input closed while waiting in 'InRead()', line %d.",(int)__LINE__ );
            return -1;
          }
         QueryPerformanceCounter( (LARGE_INTEGER*)&i64Tnow );
         if( i64Tnow > i64TimeToLeave )  // because of the stupid Sleep()-behaviour
          { DOBINI();   // -> Sound_iSourceCodeLine
            break;   // added 2011-03 because of the crippled Sleep() function
          }
       } // end for
      DOBINI();   // -> Sound_iSourceCodeLine
      if( i<=0 )
       { // if took too long, error..
         DOBINI();   // -> Sound_iSourceCodeLine
#       if( CSOUND_USE_CRITICAL_SECTION )
         EnterCriticalSection(&m_CriticalSection);
#       endif
          {
            m_ErrorCodeIn = SOUNDIN_ERR_TIMEOUT;  // YHF: code (2)113
            InClose();
            if( m_sz255LastErrorStringIn[0]==0 )
              strcpy(m_sz255LastErrorStringIn,"Timed out waiting for input");
          }
#       if( CSOUND_USE_CRITICAL_SECTION )
         LeaveCriticalSection(&m_CriticalSection);
#       endif
         DOBINI();  // -> Sound_iSourceCodeLine
       }
    }
   else
    {
#    if( CSOUND_USE_CRITICAL_SECTION )
      LeaveCriticalSection(&m_CriticalSection);
#    endif
    }
  return ( m_iInTailIndex!=m_iInHeadIndex );
} // end CSound::WaitForInputData()

//--------------------------------------------------------------------------
BYTE * CSound::WrapInBufPositionAndGetPointerToNextFragment( // .. for AUDIO INPUT
     int iOptions, // [in] one of the following:
       #define CSOUND_WRAPBUF_OPT_DONT_WAIT     0
       #define CSOUND_WRAPBUF_OPT_WAIT_FOR_DATA 1
     T_SoundBufferInfo **ppSoundBufferInfo ) // [out] address of buffer info
{
  if( m_InBufPosition >= m_InBufferSize )  // finished with an INPUT-buffer
   { // so add it back in to the *INPUT*-Queue (for the MMSYSTEM API)
     if( (m_iInTailIndex<0) || (m_iInTailIndex >= SOUND_MAX_INPUT_BUFFERS)
                          || (m_iInTailIndex >= m_NumInputBuffers)
       )
      { // added 2011-03-13 for safety ...
        m_iInTailIndex = 0;   // possibly an UNHANDLED wrap-around ?
      }
     m_InputWaveHdr[m_iInTailIndex].dwBytesRecorded = 0;
     if( m_hwvin != NULL ) // only if MMSYSTEM used (not required for ASIO) :
      { DOBINI();   // -> Sound_iSourceCodeLine
        waveInAddBuffer(m_hwvin, &m_InputWaveHdr[m_iInTailIndex], sizeof(WAVEHDR));
        DOBINI();   // -> Sound_iSourceCodeLine
      }
     m_InBufPosition = 0;
     if( ++m_iInTailIndex >= m_NumInputBuffers)   // handle wrap around
        m_iInTailIndex = 0;
   }
  else // ( m_InBufPosition < m_InBufferSize ) :
   {   // not finished with the previous *INPUT*-buffer yet !
     // No-No: return m_pbInputBuffer + m_iInTailIndex*m_InBufferSize + m_InBufPosition;
     if( ppSoundBufferInfo != NULL )
      { *ppSoundBufferInfo = &m_InBufInfo[m_iInTailIndex];
      }
     return m_pbInputBuffer + m_iInTailIndex*m_InBufferSize;
   }
  // Arrived here: there are no bytes remaining from the previous 'fragment'...
  if( m_iInTailIndex != m_iInHeadIndex )  // ok, we have AT LEAST one filled buffer !
   { if( ppSoundBufferInfo != NULL )
      { *ppSoundBufferInfo = &m_InBufInfo[m_iInTailIndex];
      }
     return m_pbInputBuffer + m_iInTailIndex*m_InBufferSize;
   }
  // Arrived here: No filled buffer available.. WAIT for the next one ?
  if( iOptions & CSOUND_WRAPBUF_OPT_WAIT_FOR_DATA )
   { if( WaitForInputData(m_iInputTimeout_ms) )
      { // arrived here: hooray, we've got at least one new fragment ("WAVEHDR") of samples..
        if( ppSoundBufferInfo != NULL )
         { *ppSoundBufferInfo = &m_InBufInfo[m_iInTailIndex];
         }
        return m_pbInputBuffer + m_iInTailIndex*m_InBufferSize;
      }
   }
  // Arrived here: No filled buffer, caller doesn't want to wait, or wait timed out:
  return NULL;
} // end CSound::WrapInBufPositionAndGetPointerToNextFragment()


//--------------------------------------------------------------------------
void CSound::GetDateAndTimeForInput(
           T_SoundBufferInfo *pSoundBufferInfo, // [in]  pSoundBufferInfo->i64HRTimerValue, etc
           T_ChunkInfo *pOutChunkInfo )         // [out] pOutChunkInfo->ldblUnixDateAndTime
  // Periodically(!!) called from the 'input reading function' to retrieve
  //  and absolute timestamp for the first sample in the current sample block.
{
  double t1,sr;
  int i, iNumSamplesPerBuffer, iBufferIndex;
  long double ldblUnixDateAndTime;
  LONGLONG i64, i64TimerFrequency;

  iNumSamplesPerBuffer = GetNumSamplesPerInputBuffer();
  if( (pSoundBufferInfo != NULL ) && (pOutChunkInfo != NULL) && iNumSamplesPerBuffer>0 )
   {
     // The conversion of the 'high resolution timer value' into a Unix date/time
     // is implemented in C:\cbproj\Timers.cpp (at least for Spectrum Lab) .
     // Under windows, the "HR-Timer" (high resolution timer) is actually
     // a value returned by QueryPerformanceCounter() [windows API function].
     if( pSoundBufferInfo->i64HRTimerValue != 0 )
      { // Only overwrite the timestamp in the 'chunk info' if it's valid.
        // Otherwise, leave the timestamp UNCHANGED, because in Spectrum Lab,
        // this value is preset with a meaningful value
        // (in SoundThd.cpp :  ) .
        ldblUnixDateAndTime = TIM_HRTimerValueToUnixDateAndTime( pSoundBufferInfo->i64HRTimerValue );

        // Measure the actual sampling rate by comparing the high-resolution-timer values
        // from this audio buffer and the previous one:
        iBufferIndex = pSoundBufferInfo->iBufferIndex-1;
        if( iBufferIndex < 0 )
         {  iBufferIndex = m_NumInputBuffers-1; // not SOUND_MAX_INPUT_BUFFERS-1 !
         }
        // high-resolution-timer difference between the CURRENT and the PREVIOUS buffer:
        i64 = pSoundBufferInfo->i64HRTimerValue - m_InBufInfo[iBufferIndex].i64HRTimerValue;
        // .. converted into a FREQUENCY ?
        QueryPerformanceFrequency( (LARGE_INTEGER*)&i64TimerFrequency );
        if( i64>0 && i64TimerFrequency>0 )
         { // Convert the number of timer ticks into seconds (t1),
           //  and calculate the momentary sample rate (sr) :
           // block_time  = number of timer ticks between two blocks / timer frequency;
           t1 = (double)i64/*timer ticks*/ / (double)i64TimerFrequency; // [seconds]
           sr = (double)iNumSamplesPerBuffer / t1;  // sampling rate measured from a SINGLE block of samples (*)
           // (*)  Tried to measure the true sample rate by comparing
           //  the 'high resolution' timer values from EACH block,
           //  but that's not accurate enough. Example:
           // Measuring interval == ONE block = 8192 / 32000 Hz = 256 ms;
           // true sampling rate (GPS-based measurement): 32399.85 Hz;
           // Sampling rates measured in 256-millisecond-intervals :
           //  32758, 32768, 31498, 32691, 31488, 32754, 32779, 31466
           //                 ^ ............ ^ ................ ^ 1 kHz off,
           // i.e. up to 3 % error, 3 % * 256 ms = ~8 ms Jitter .
           // To measure the sampling rate with less than 1 % error,
           // a 2-second measuring cycle ("gate time") was used,
           // with the following result in m_dbl16MeasuredSampleRates[] :
           //  32441, 32283, 32441, 32443, 32441, 32285, 32440, 32443, ... [Hz]
           //   (i.e. up to 32400-32283 = 0.4 % error, which is ok for this purpose)
           // 'sr' should be the soundcard sampling rate in Hertz now. Is it plausible ?
           if(   ((int)(0.9*sr) < (int)m_InFormat.nSamplesPerSec)
              && ((int)(1.1*sr) > (int)m_InFormat.nSamplesPerSec) )
            { // short-term measured sampling rate is plausible. Don't discard it.
              if( m_dblInputSRGateTime <= 0.0 ) // first block of a measuring interval..
               { m_dblInputSRGateTime = t1;
                 m_dblInputSRGateCount= iNumSamplesPerBuffer;
               }
              else  // NOT the first block of a measuring interval : "accumulate"..
               { m_dblInputSRGateTime += t1;                   // .. time in seconds
                 m_dblInputSRGateCount+= iNumSamplesPerBuffer; // .. number of samples
               }
              if( m_dblInputSRGateTime >= 2.0/*seconds*/ )
               { // Use an average of the last 16(?) measured sampling rates,
                 // because there will be a terrible jitter in the measurements.
                 // m_dbl16MeasuredSampleRates[0]=newest, [15]=oldest measured SR :
                 for(i=15;i>0;--i)
                  { m_dbl16MeasuredSampleRates[i] = m_dbl16MeasuredSampleRates[i-1];
                  }
                 m_dbl16MeasuredSampleRates[0] = m_dblInputSRGateCount / m_dblInputSRGateTime;
                 ++m_nMeasuredSampleRates;
                 if(m_nMeasuredSampleRates>16)
                  { m_nMeasuredSampleRates=16;
                  }
                 sr = 0.0;
                 for(i=0; i<m_nMeasuredSampleRates; ++i)
                  { sr += m_dbl16MeasuredSampleRates[i];
                  }
                 m_dblMeasuredInputSampleRate = sr / (double)m_nMeasuredSampleRates;
                 // 'm_dblMeasuredInputSampleRate' is the best guess for the
                 // sampling rate within this module . It is typically only
                 // a few Hertz away from the true value, so not yet accurate
                 // enough for the processing inside spectrum lab !
                 // At least, m_dblMeasuredInputSampleRate allows to adjust
                 // the timestamp further below, sufficiently accurate,
                 // and usually much better than the 'nominal' sampling rate.
                 m_dblInputSRGateTime = m_dblInputSRGateCount = 0.0;
               } // end if < m_dblInputSRGateTime >= N seconds >
            }
           else // short-term measurement of the sampling rate isn't plausible.
            {   // Discard it, and start a new gate time :
              m_dblInputSRGateTime = 0.0;
            }
         } // end if( i64>0 && ...
        // < end of sample rate "measurement" >

        // Subtract the length of ONE AUDIO BUFFER (in seconds) from the timestamp because
        // the 'timer' was queried in WaveInCallback() when the block was COMPLETE,
        // but ldblUnixDateAndTime shall apply to the FIRST sample in the block.
        if( m_dblMeasuredInputSampleRate > 0 ) // avoid div-by-zero, as usual..
         { ldblUnixDateAndTime -= (double)iNumSamplesPerBuffer
                                / m_dblMeasuredInputSampleRate;
         }

        // The timestamp (in the T_ChunkInfo) is now VALID,
        //  but it's not really PRECISE (due to windows task switching).
        // To avoid jitter in the timestamp(*), an AUDIO-SAMPLE-BASED time is used
        //  wherever possible. When checked on a windows XP system, there was
        //  considerable jitter in 'ldblUnixDateAndTime', caused by task switching
        //  and non-constant latencies, of approximately 10..100 ms.
        //  If the timestamps are 'improved later' (using a GPS sync signal),
        //  it's sufficent to keep T_ChunkInfo.ldblUnixDateAndTime
        //  within +/- 499 milliseconds of the 'correct' timestamp.
        // (*) Jitter in the timestamps must be avoided because since 2012,
        //     the timestamps are used instead of the 'sample counter'
        //     in some of Spectrum Lab's signal processing modules .
        t1 = ldblUnixDateAndTime - m_ldblSampleBasedInputTimestamp;
        if( (t1>-0.25) && (t1<0.25) )
         { // sample-based timestamp is reasonably close to the system-time:
           // Use the SAMPLE-BASED timestamp, which is almost jitter-free .
           // Slowly 'pull in' the sample-based timestamp,
           // with a maximum slew rate of 1 millisecond per call:
           t1 *= 0.05;  // only remove 1/20 th of the timestamp difference..
           if( t1<-1e-3 ) t1=-1e-3;
           if( t1> 1e-3 ) t1= 1e-3;
           // If m_ldblSampleBasedInputTimestamp lags ldblUnixDateAndTime,
           // m_ldblSampleBasedInputTimestamp is "too low" and d is positive, thus:
           m_ldblSampleBasedInputTimestamp += t1;
           // Use this 'mostly sample-counter-based' time for the chunk-info.
           // Note: As long as the flag 'CHUNK_INFO_TIMESTAMPS_PRECISE'
           //       is NOT set in T_ChunkInfo.dwValidityFlags,
           //       none of the signal-processing modules in Spectrum Lab
           //       will use the timestamp as argument (angle) to generate
           //       sinewaves, oscillator signals for frequency converters, etc.
           //   Therefore, a small amount of jitter doesn't hurt much.
           ldblUnixDateAndTime = m_ldblSampleBasedInputTimestamp;
         }
        else // difference too large -> Let's do the time-warp again..
         { m_ldblSampleBasedInputTimestamp = ldblUnixDateAndTime;
           // (this may happen periodically, if the nominal sampling rate
           //  is 'totally off' the real sampling rate,
           //  which can only be measured COARSELY here
           //  due to the timestamp jitter)
         }
        pOutChunkInfo->ldblUnixDateAndTime = ldblUnixDateAndTime;
        // At this point, we KNOW that the internally measured sampling rate,
        //   as well as the absolute timestamp set above are
        //   **NOT PRECISE** ("calibrated") but at least they are VALID now.
        // Spectrum Lab's GPS-based timing reference will do much better than this.
        pOutChunkInfo->dwValidityFlags = // see c:\cbproj\SoundUtl\ChunkInfo.h ...
          ( pOutChunkInfo->dwValidityFlags & ~(CHUNK_INFO_TIMESTAMPS_PRECISE)  )
                                             | CHUNK_INFO_TIMESTAMPS_VALID;
        // Note: the SAMPLING RATE for the chunk-info is set somewhere else !
      }
   }
} // end CSound::GetDateAndTimeForInput()


// Not used in SpectrumLab anymore, but still used in the 'WOLF GUI' (2013) :
//---------------------------------------------------------------------------
LONG CSound::InReadInt16( SHORT* pData, int Length)
  // Reads 'Length' samples of signed 16-bit integer data
  // from the opened soundcard input.
  //   returns:
  //   Length   if 'Length' samples were succesfully read
  //       0 =  if reaches the specified sample limit
  //      -1 =  if there is an error ( use GetError() to retrieve error )
{
 int i;
 BYTE *pb;
 union uBSHORT{    // byte-adressable 16-bit integer
      BYTE b[2];   // [0]=LSB, [1]=MSB (INTEL byte order)
      short i16;
   }bsTemp;
 T_SoundBufferInfo *pSoundBufferInfo;
   


   if( !m_InputOpen )      // return -1 if no inputs are active
    {
      m_ErrorCodeIn = SOUNDIN_ERR_NOTOPEN;
      return -1;
    }
   if( m_InOverflow )    // has been set by WaveInCallback()
    {
      m_ErrorCodeIn = SOUNDIN_ERR_OVERFLOW;
#if(0)
      InClose();         // happens quite often, really have to CLOSE now ??
      return -1;         // return -1 if overflow
#else
      m_InOverflow = false;
#endif
    }

  DOBINI();  // -> Sound_iSourceCodeLine


  if( m_iInTailIndex == m_iInHeadIndex )  // if no buffer is filled ...
   { if( ! WaitForInputData(m_iInputTimeout_ms) ) // ... then wait, but only for a limited time
      { DOBINI();  // -> Sound_iSourceCodeLine
        return -1;
      }
   }
  // here if there is data to retrieve
  DOBINI();  // -> Sound_iSourceCodeLine
  // ex: pb = m_pbInputBuffer + m_iInTailIndex*m_InBufferSize;  // YHF: use local pointer for speed !
  pb = WrapInBufPositionAndGetPointerToNextFragment(CSOUND_WRAPBUF_OPT_DONT_WAIT, &pSoundBufferInfo);
  if( pb==NULL ) // oops... no filled 'input' data available ?!
   { DOBINI();   // -> Sound_iSourceCodeLine
     return -1;
   }
  if( m_InFormat.wBitsPerSample == 16 )
   {
     for( i=0; i < (Length*m_InFormat.nChannels); i++ )
      {
        if (m_InBufPosition & 1)
         { // very suspicious..
           m_ErrorCodeIn = ERROR_WORD_AT_ODD_ADDRESS;
         }
        bsTemp.b[0] = pb[m_InBufPosition++];
        bsTemp.b[1] = pb[m_InBufPosition++];
        *(pData + i) = bsTemp.i16;
        if( m_InBufPosition >= m_InBufferSize )  // finished with this buffer
         {                                       // so use the next (*if* there's one)
           pb = WrapInBufPositionAndGetPointerToNextFragment(CSOUND_WRAPBUF_OPT_WAIT_FOR_DATA, &pSoundBufferInfo);
           if( pb==NULL ) // oops... no more filled 'input' data available ?!
            { DOBINI();   // -> Sound_iSourceCodeLine
              return -1;
            }
         }
      }
   }
  else // not 16 bits per sample, but most certainly 8 bits per sample:
  if( m_InFormat.wBitsPerSample == 8 )
   {
     bsTemp.i16 = 0;
     for( i=0; i < (Length*m_InFormat.nChannels); i++ )
      { bsTemp.b[1] = (BYTE)( pb[m_InBufPosition++] - 128 );
        *(pData + i) = bsTemp.i16;
      }
   }
  if( m_InBufPosition >= m_InBufferSize )  // finished with this buffer
   {                                       // so add it back in to the Queue
     m_InputWaveHdr[m_iInTailIndex].dwBytesRecorded = 0;
     if( m_hwvin != NULL )
      { // only if MMSYSTEM used : call windows API to let this buffer be filled by input device
        DOBINI();  // -> Sound_iSourceCodeLine
        waveInAddBuffer(m_hwvin, &m_InputWaveHdr[m_iInTailIndex], sizeof(WAVEHDR));
        DOBINI();
      }
     m_InBufPosition = 0;
     if( ++m_iInTailIndex >= m_NumInputBuffers)   // handle wrap around
           m_iInTailIndex = 0;
    }
  m_i64NumSamplesRead += Length;
  if( (m_InSampleLimit != 0) && (m_i64NumSamplesRead >= m_InSampleLimit) )
   {
     InClose();
     return 0;
   }
  DOBINI();
  return Length;
} // end CSound::InReadInt16()


//---------------------------------------------------------------------------
int CSound::InReadStereo(   // still used by Spectrum Lab (2012) ...
        T_Float* pLeftData,      // [out] samples for 'left' audio channel (or "I")
        T_Float *pRightData,     // [out] samples for 'right' audio channel (or "Q")
        int iNumberOfSamples,    // [in] number of samples for each of the above blocks
        int iTimeout_ms,         // [in] max timeout in milliseconds, 0=non-blocking
        int *piHaveWaited_ms,    // [out,optional] "have been waiting here for XX milliseconds"
      T_ChunkInfo *pOutChunkInfo) // [out,optional,(*)] chunk info with timestamp, GPS, calib;
                                  //       see c:\cbproj\SoundUtl\ChunkInfo.h
  // Reads 'Length' samples of floating-point samples
  // from the opened soundcard input (or audio I/O DLL), and splits the data into
  // separated blocks for LEFT + RIGHT channel (i.e. non-interlaced output).
  //   returns:
  //   Length   if 'Length' samples were succesfully read
  //       0 =  if reaches the specified sample limit
  //      -1 =  if there is an error ( use GetError() to retrieve error )
  // Notes:
  //  -  'Length' must not exceed the size of the internal buffers !
  //  -  If an audio source doesn't support the 'chunk info'
  //      (for example, the soundcard), the contents of *pOutChunkInfo
  //      will not be touched by CSound::InReadStereo() .
  //      Thus, the caller (like SL) may provide a 'meaningful default' there.
  // (*) Some of the fields in pOutChunkInfo
  //      may have been filled in by the CALLER already,
  //      indicated through pOutChunkInfo->dwValidityFlags:
  //  CHUNK_INFO_SAMPLE_RATE_VALID : pOutChunkInfo->dblPrecSamplingRate is set and VALID;
  //  CHUNK_INFO_SAMPLE_RATE_CALIBRATED : pOutChunkInfo->dblPrecSamplingRate is known to be CALIBRATED.
{
 int i,nChannels;
 BYTE *pb;
 float *pflt;
 T_Float fltScalingFactor = 1.0;
 LONG i32nSamplesRead = -1;
 T_SoundBufferInfo *pSoundBufferInfo;
 BOOL use_measured_sr;

  union uBSHORT  // byte-adressable 16-bit integer
   {  BYTE b[2];  // [0]=LSB, [1]=MSB (INTEL byte order)
      short i16;
   }bsTemp;
  union uBLONG  // byte-adressable 32-bit-integer
    { BYTE  b[4];  // b[0] = bits 7..0,  b[3] = bits 31..24 (INTEL)
      long  i32;
    }blTemp;

  if( pRightData == NULL )    // caller only wants ONE block(channel) :
   { nChannels=1;
   }
  else                        // caller wants to separate blocks
   { nChannels=2;
   }

  if( !m_InputOpen )      // return -1 if no inputs are active
   {
     DOBINI();  // -> Sound_iSourceCodeLine
     m_ErrorCodeIn = SOUNDIN_ERR_NOTOPEN;
     return -1;
   }

  if( pOutChunkInfo != NULL )
   {
     use_measured_sr = TRUE;
     // Does the CALLER know more about the soundcard's sampling rate than CSound ?
     //      Some of the fields in pOutChunkInfo
     //      may have been filled in by the CALLER already,
     //      indicated through pOutChunkInfo->dwValidityFlags:
     if( pOutChunkInfo->dwValidityFlags & CHUNK_INFO_SAMPLE_RATE_VALID )
      { // pOutChunkInfo->dblPrecSamplingRate has already been set by caller and is VALID..
        // .. is it REALLY ?
        if( (pOutChunkInfo->dblPrecSamplingRate > (0.9 * m_dblMeasuredInputSampleRate) )
          &&(pOutChunkInfo->dblPrecSamplingRate < (1.1 * m_dblMeasuredInputSampleRate) ) )
         { // Ok, the indicated sampling rate looks plausible.. USE IT !
           m_dblNominalInputSampleRate = pOutChunkInfo->dblPrecSamplingRate;
           if( pOutChunkInfo->dwValidityFlags & CHUNK_INFO_SAMPLE_RATE_CALIBRATED )
            { // The sampling rate is not just 'valid', it's also 'calibrated'
              //  (for example, using a GPS sync signal as reference, etc..)
              // In this case, DO NOT OVERWRITE THE SAMPLING RATE IN THE T_ChunkInfo
              // with the 'default' or the 'measured' sampling rate !
              use_measured_sr = FALSE;
            }
         } // end if pOutChunkInfo->dblPrecSamplingRate looks plausible
        else
         { // Ignore the *INPUT* sampling rate in T_ChunkInfo->dblPrecSamplingRate;
           // instead use the 'measured' (or, initially, the "nominal") SR :
           // pOutChunkInfo->dblPrecSamplingRate = m_dblNominalInputSampleRate;
         }
      } // end if( pOutChunkInfo->dwValidityFlags & CHUNK_INFO_SAMPLE_RATE_VALID )
     else  // The CALLER didn't fill in a valid sampling rate -> do it here:
      { // pOutChunkInfo->dblPrecSamplingRate = m_dblNominalInputSampleRate;
      }
     if( use_measured_sr && ( ( pOutChunkInfo->dwValidityFlags & CHUNK_INFO_SAMPLE_RATE_VALID )==0 ) )
      { if( (m_dblMeasuredInputSampleRate > (0.9 * m_dblNominalInputSampleRate) )
         && (m_dblMeasuredInputSampleRate < (1.1 * m_dblNominalInputSampleRate) ) )
         { pOutChunkInfo->dblPrecSamplingRate = m_dblMeasuredInputSampleRate;
         }
        else // internally 'measured' sampling rate not available :
         { pOutChunkInfo->dblPrecSamplingRate = m_dblNominalInputSampleRate;
         }
        pOutChunkInfo->dwValidityFlags |= CHUNK_INFO_SAMPLE_RATE_VALID;
      }
   } // end if( pOutChunkInfo != NULL )



   // Totally different handling (internalls) if an "Audio I/O Driver DLL" is used:
#if( SWI_AUDIO_IO_SUPPORTED )
   if( (aio_In.h_AudioIoDLL != NULL)  // using an Audio-I/O or ExtIO-DLL for *INPUT* ...
     &&( !m_ExtIONeedsSoundcardForInput ) ) // ...except for those which use the SOUNDCARD:
    { // Because the audio-I/O-DLL uses multiple channels in a SINGLE SAMPLE POINT,
      // the data must first be read into a temporary array, and then re-arranged:
      float *pfltTemp = (T_Float*)malloc( sizeof(float) * iNumberOfSamples * nChannels );
      if( pfltTemp != NULL )
       {
         i32nSamplesRead = AIO_Host_ReadInputSamplePoints( // returns the number of sample-POINTS, or a negative error code
           &aio_In,  // [in,out] T_AIO_DLLHostInstanceData *pInstData, DLL-host instance data
           pfltTemp, // [out] audio samples, as 32-bit FLOATING POINT numbers, grouped as "sample points"
           iNumberOfSamples, // [in] iNumSamplePoints; number of SAMPLE POINTS(!) to read
           nChannels, // [in] nChannelsPerSample; number of samples PER SAMPLE POINT
           200,   // [in] max timeout in milliseconds, 0 would be 'non-blocking'
                  // (must use a blocking call here, because the audio-source
                  //  usually 'sets the pace' for the entire processing chain in SL.
                  // 200 ms are sufficient for 8000 samples/second and 1024 samples per chunk)
           pOutChunkInfo,// [out,optional] T_ChunkInfo *pOutChunkInfo, see c:\cbproj\SoundUtl\ChunkInfo.h
           NULL);    // [out,optional] INT32 *piHaveWaited_ms; "have been waiting here for XX milliseconds"
         if( i32nSamplesRead < 0 )
          { m_ErrorCodeIn = ConvertErrorCode_AIO_to_SOUND( i32nSamplesRead );
          }
         else // no error -> split the sample-points into separate blocks
          { if( i32nSamplesRead>iNumberOfSamples )
             { i32nSamplesRead=iNumberOfSamples;  // should never happen, but expect the worst
             }
            pflt = pfltTemp;
            for(i=0; i<i32nSamplesRead; ++i)
             { if( pLeftData != NULL )
                  *pLeftData++ = pflt[0];
               if( pRightData != NULL )
                  *pRightData++ = pflt[1];
               pflt += nChannels;
             }
          }
         free(pfltTemp);
       }
     if( ( pOutChunkInfo != NULL ) && ( i32nSamplesRead>0 ) )
      {  pOutChunkInfo->nChannelsPerSample = nChannels;
         pOutChunkInfo->dwNrOfSamplePoints = i32nSamplesRead;
      }

     return i32nSamplesRead;
   }  // end if( aio_In.h_AudioIoDLL != NULL )
#endif // SWI_AUDIO_IO_SUPPORTED ?

   if( m_InOverflow )    // has been set by WaveInCallback()
    {
      DOBINI();  // -> Sound_iSourceCodeLine
      m_ErrorCodeIn = SOUNDIN_ERR_OVERFLOW;
      m_InOverflow = FALSE;
    }

  DOBINI();  // -> Sound_iSourceCodeLine

  if( m_iInTailIndex == m_iInHeadIndex )  // if no buffer is filled ...
   { if( ! WaitForInputData(iTimeout_ms) ) // ... then wait, but only for a limited time
      { DOBINI();  // -> Sound_iSourceCodeLine
        return -1;
      }
   }
  // here if there is data to retrieve
  DOBINI();  // -> Sound_iSourceCodeLine
  // ex: pb = m_pbInputBuffer + m_iInTailIndex*m_InBufferSize;
  pb = WrapInBufPositionAndGetPointerToNextFragment( CSOUND_WRAPBUF_OPT_DONT_WAIT, &pSoundBufferInfo );
  if( pb==NULL ) // oops... no filled 'input' data available ?!
   { DOBINI();  // -> Sound_iSourceCodeLine
     return -1;
   }
  if( pOutChunkInfo != NULL )
   { if( m_InFormat.nChannels>0 )
      { pOutChunkInfo->nChannelsPerSample = m_InFormat.nChannels; // added 2011-12-27
        // ( Even if the application specified TWO blocks ( pLeftData + pRightData ),
        //   the T_ChunkInfo struct contains the number of "inputs" from InOpen() )
      }
#   if( SWI_AUDIO_IO_SUPPORTED )  // only if the 'audio-I/O-host' is supported..
     if( aio_In.h_AudioIoDLL != NULL)  // using an ExtIO-DLL, but the SOUNDCARD for *INPUT* ...
      { // In this case, a few fields in c:\cbproj\SoundUtl\ChunkInfo.h :: T_ChunkInfo
        //    are copied from AudioIO.h::T_AIO_DLLHostInstanceData :
        pOutChunkInfo->dblRadioFrequency = aio_In.i32ExternalVfoFrequency;
      }
#   endif // SWI_AUDIO_IO_SUPPORTED ?

     GetDateAndTimeForInput(
              pSoundBufferInfo, // [in] pSoundBufferInfo->i64HRTimerValue, etc
              pOutChunkInfo ); // [out] pOutChunkInfo->ldblUnixDateAndTime
   }
  if( m_InFormat.wBitsPerSample == 16 )
   { // usually 16 BITS PER SAMPLE ...
     if( pOutChunkInfo != NULL )  // if the caller is aware of the value range,
      { pOutChunkInfo->dblSampleValueRange = 1.0;  // scale everything to +/- 1.0
        fltScalingFactor = 1.0 / 32768.0;  // 16 bit (signed)  ->    +/- 1.0
      }
     else                         // for backward compatibility:
      { fltScalingFactor = 1.0;
      }
     if (m_InBufPosition & 1)
      { // very suspicious..
        DOBINI();  // -> Sound_iSourceCodeLine
        m_ErrorCodeIn = ERROR_WORD_AT_ODD_ADDRESS;
      }
     if( (m_InFormat.nChannels==1) // 16 bit, Mono input
       ||(pRightData == NULL) )    // or caller only wants ONE channel
      { int iBufIdxInc = 2*m_InFormat.nChannels;
        if( pRightData != NULL )  // two *output* channels :
         { DOBINI();  // -> Sound_iSourceCodeLine
           for( i=0; i<iNumberOfSamples; i++ )
            {
              bsTemp.b[0] = pb[m_InBufPosition];   // LSB
              bsTemp.b[1] = pb[m_InBufPosition+1]; // MSB
              pLeftData[i] = pRightData[i] = (T_Float)bsTemp.i16 * fltScalingFactor;
              m_InBufPosition += iBufIdxInc;
              // Added 2011-03-13 for callers which need to read HUGE BLOCKS in one over:
              if( m_InBufPosition >= m_InBufferSize )  // finished with this buffer
               {                                       // so use the next (*if* there's one)
                 pb = WrapInBufPositionAndGetPointerToNextFragment(CSOUND_WRAPBUF_OPT_WAIT_FOR_DATA, &pSoundBufferInfo);
                 if( pb==NULL ) // oops... no more filled 'input' data available ?!
                  { DOBINI();   // -> Sound_iSourceCodeLine
                    return -1;
                  }
               }
            } // end for
         }
        else              // only ONE output channel :
         { DOBINI();  // -> Sound_iSourceCodeLine
           for( i=0; i<iNumberOfSamples; i++ )
            {
              bsTemp.b[0] = pb[m_InBufPosition];
              bsTemp.b[1] = pb[m_InBufPosition+1];
              pLeftData[i]= (T_Float)bsTemp.i16 * fltScalingFactor;
              m_InBufPosition += iBufIdxInc;
              // Added 2011-03-13 for callers which need to read HUGE BLOCKS in one over:
              if( m_InBufPosition >= m_InBufferSize )  // finished with this buffer
               {                                       // so use the next (*if* there's one)
                 pb = WrapInBufPositionAndGetPointerToNextFragment(CSOUND_WRAPBUF_OPT_WAIT_FOR_DATA, &pSoundBufferInfo);
                 if( pb==NULL ) // oops... no more filled 'input' data available ?!
                  { DOBINI();   // -> Sound_iSourceCodeLine
                    return -1;
                  }
               }
            } // end for
         }
       } // end if <16 bit mono>
     else
     if( (m_InFormat.nChannels==2) && (pRightData!=NULL) )
      { DOBINI();  // -> Sound_iSourceCodeLine
        for( i=0; i<iNumberOfSamples; i++ )
         {
           bsTemp.b[0] = pb[m_InBufPosition++];
           bsTemp.b[1] = pb[m_InBufPosition++];
           pLeftData[i] = (T_Float)bsTemp.i16 * fltScalingFactor;
           bsTemp.b[0] = pb[m_InBufPosition++];
           bsTemp.b[1] = pb[m_InBufPosition++];
           pRightData[i] = (T_Float)bsTemp.i16 * fltScalingFactor;
              // Added 2011-03-13 for callers which need to read HUGE BLOCKS in one over:
              if( m_InBufPosition >= m_InBufferSize )  // finished with this buffer
               {                                       // so use the next (*if* there's one)
                 pb = WrapInBufPositionAndGetPointerToNextFragment(CSOUND_WRAPBUF_OPT_WAIT_FOR_DATA, &pSoundBufferInfo);
                 if( pb==NULL ) // oops... no more filled 'input' data available ?!
                  { DOBINI();   // -> Sound_iSourceCodeLine
                    return -1;
                  }
               }

         } // end for
      } // end if <16 bit stereo>
    }
   else // not 16 bits per sample, but most certainly 8 bits per sample:
   if( m_InFormat.wBitsPerSample == 8 )
    {
      if( pOutChunkInfo != NULL )  // if the caller is aware of the value range,
       { pOutChunkInfo->dblSampleValueRange = 1.0;  // scale everything to +/- 1.0
         fltScalingFactor = 1.0 / 32768.0;  // 16 bit (signed)  ->    +/- 1.0
       }
      else                         // for backward compatibility:
       { fltScalingFactor = 1.0;
       }
      bsTemp.i16 = 0;
      if( (m_InFormat.nChannels==1) // 8 bit, Mono input
        ||(pRightData == NULL) )    // or caller only wants ONE channel
       { int iBufIdxInc = m_InFormat.nChannels;
        if( pRightData != NULL )  // two *output* channels :
         { DOBINI();  // -> Sound_iSourceCodeLine
           for( i=0; i < iNumberOfSamples; i++ )
            {
              bsTemp.b[1] = (BYTE)( pb[m_InBufPosition] - 128);
              pLeftData[i] = pRightData[i] = (T_Float)bsTemp.i16 * fltScalingFactor;
              m_InBufPosition += iBufIdxInc;
              // Added 2011-03-13 for callers which need to read HUGE BLOCKS in one over:
              if( m_InBufPosition >= m_InBufferSize )  // finished with this buffer
               {                                       // so use the next (*if* there's one)
                 pb = WrapInBufPositionAndGetPointerToNextFragment(CSOUND_WRAPBUF_OPT_WAIT_FOR_DATA, &pSoundBufferInfo);
                 if( pb==NULL ) // oops... no more filled 'input' data available ?!
                  { DOBINI();   // -> Sound_iSourceCodeLine
                    return -1;
                  }
               }

            }
         }
        else                      // only ONE output channel :
         { DOBINI();  // -> Sound_iSourceCodeLine
           for( i=0; i < iNumberOfSamples; i++ )
            {
              bsTemp.b[1] = (BYTE)( pb[m_InBufPosition] - 128);  // high byte only
              pLeftData[i] = (T_Float)bsTemp.i16 * fltScalingFactor;
              m_InBufPosition += iBufIdxInc;
              // Added 2011-03-13 for callers which need to read HUGE BLOCKS in one over:
              if( m_InBufPosition >= m_InBufferSize )  // finished with this buffer
               {                                       // so use the next (*if* there's one)
                 pb = WrapInBufPositionAndGetPointerToNextFragment(CSOUND_WRAPBUF_OPT_WAIT_FOR_DATA, &pSoundBufferInfo);
                 if( pb==NULL ) // oops... no more filled 'input' data available ?!
                  { DOBINI();   // -> Sound_iSourceCodeLine
                    return -1;
                  }
               }
            }
         }
       } // end if <8 bit mono>
      else
      if( (m_InFormat.nChannels==2) && (pRightData!=NULL) ) // 8 bit, stereo input
       { DOBINI();  // -> Sound_iSourceCodeLine
        for( i=0; i < iNumberOfSamples; i++ )
         {
           bsTemp.b[1]  = (BYTE)( pb[m_InBufPosition++] - 128 );
           pLeftData[i] = (T_Float)bsTemp.i16 * fltScalingFactor;
           bsTemp.b[1]  = (BYTE)( pb[m_InBufPosition++] - 128 );
           pRightData[i]= (T_Float)bsTemp.i16 * fltScalingFactor;
              // Added 2011-03-13 for callers which need to read HUGE BLOCKS in one over:
              if( m_InBufPosition >= m_InBufferSize )  // finished with this buffer
               {                                       // so use the next (*if* there's one)
                 pb = WrapInBufPositionAndGetPointerToNextFragment(CSOUND_WRAPBUF_OPT_WAIT_FOR_DATA, &pSoundBufferInfo);
                 if( pb==NULL ) // oops... no more filled 'input' data available ?!
                  { DOBINI();   // -> Sound_iSourceCodeLine
                    return -1;
                  }
               }
         }
       } // end if <8 bit stereo>
    } // end if < 8 bits per sample >
   else
   if( m_InFormat.wBitsPerSample == 24 )
    {
      if( pOutChunkInfo != NULL )  // if the caller is aware of the value range,
       { pOutChunkInfo->dblSampleValueRange = 1.0;  // scale everything to +/- 1.0
         fltScalingFactor = 1.0/(32768.0*65536.0); // 32 bit (signed) ->  +/- 1.0
       }
      else                         // for backward compatibility:
       { fltScalingFactor = 1.0/65536.0;  // 32 bit (signed)  ->    +/- 32768.0
       }

      blTemp.b[0] = 0;  // least significant 8 bits in 32-bit value UNUSED
      if( (m_InFormat.nChannels==1) // 24 bit, Mono input
        ||(pRightData == NULL) )    // or caller only wants ONE channel
       { int iBufIdxInc = 3 * m_InFormat.nChannels;
        if( pRightData != NULL )  // two *output* channels :
         { DOBINI();  // -> Sound_iSourceCodeLine
           for( i=0; i<iNumberOfSamples; i++ )
            {
              blTemp.b[1] = pb[m_InBufPosition];
              blTemp.b[2] = pb[m_InBufPosition+1];
              blTemp.b[3] = pb[m_InBufPosition+2];
              pLeftData[i] = pRightData[i] =  (T_Float)blTemp.i32 * fltScalingFactor;
              m_InBufPosition += iBufIdxInc;
              // Added 2011-03-13 for callers which need to read HUGE BLOCKS in one over:
              if( m_InBufPosition >= m_InBufferSize )  // finished with this buffer
               {                                       // so use the next (*if* there's one)
                 pb = WrapInBufPositionAndGetPointerToNextFragment(CSOUND_WRAPBUF_OPT_WAIT_FOR_DATA, &pSoundBufferInfo);
                 if( pb==NULL ) // oops... no more filled 'input' data available ?!
                  { DOBINI();   // -> Sound_iSourceCodeLine
                    return -1;
                  }
               }
            } // end for
         }
        else                      // only ONE output channel :
         {
           for( i=0; i<iNumberOfSamples; i++ )
            {
              blTemp.b[1] = pb[m_InBufPosition];
              blTemp.b[2] = pb[m_InBufPosition+1];
              blTemp.b[3] = pb[m_InBufPosition+2];
              pLeftData[i]= (T_Float)blTemp.i32 * fltScalingFactor;
              m_InBufPosition += iBufIdxInc;
              // Added 2011-03-13 for callers which need to read HUGE BLOCKS in one over:
              if( m_InBufPosition >= m_InBufferSize )  // finished with this buffer
               {                                       // so use the next (*if* there's one)
                 pb = WrapInBufPositionAndGetPointerToNextFragment(CSOUND_WRAPBUF_OPT_WAIT_FOR_DATA, &pSoundBufferInfo);
                 if( pb==NULL ) // oops... no more filled 'input' data available ?!
                  { DOBINI();   // -> Sound_iSourceCodeLine
                    return -1;
                  }
               }
            } // end for
         }
       } // end if <24 bit mono>
      else
      if(m_InFormat.nChannels==2) // 24 bit, Stereo
       { DOBINI();  // -> Sound_iSourceCodeLine
        for( i=0; i<iNumberOfSamples; i++ )
         {
           blTemp.b[1] = pb[m_InBufPosition++];
           blTemp.b[2] = pb[m_InBufPosition++];
           blTemp.b[3] = pb[m_InBufPosition++];
           pLeftData[i] =  (T_Float)blTemp.i32 * fltScalingFactor;

           blTemp.b[1] = pb[m_InBufPosition++];
           blTemp.b[2] = pb[m_InBufPosition++];
           blTemp.b[3] = pb[m_InBufPosition++];
           pRightData[i] =  (T_Float)blTemp.i32 * fltScalingFactor;
              // Added 2011-03-13 for callers which need to read HUGE BLOCKS in one over:
           if( m_InBufPosition >= m_InBufferSize )  // finished with this buffer
            {                                       // so use the next (*if* there's one)
              pb = WrapInBufPositionAndGetPointerToNextFragment(CSOUND_WRAPBUF_OPT_WAIT_FOR_DATA, &pSoundBufferInfo);
              if( pb==NULL ) // oops... no more filled 'input' data available ?!
               { DOBINI();   // -> Sound_iSourceCodeLine
                 return -1;
               }
            }

         } // end for
       } // end if <24 bit stereo>
    } // end if < 24 bits per sample >
   else
   if( m_InFormat.wBitsPerSample == 32 )
    {
      int iBufIdxInc = 4 * m_InFormat.nChannels;
      if( pOutChunkInfo != NULL )  // if the caller is aware of the value range,
       { pOutChunkInfo->dblSampleValueRange = 1.0;  // scale everything to +/- 1.0
         fltScalingFactor = 1.0/(32768.0*65536.0); // 32 bit (signed) ->  +/- 1.0
       }
      else                         // for backward compatibility:
       { fltScalingFactor = 1.0/65536.0;  // 32 bit (signed)  ->    +/- 32768.0
       }
      if( (m_InFormat.nChannels==1) // 32 bit, Mono input
        ||(pRightData == NULL) )    // or caller only wants ONE channel
       {
        if( pRightData != NULL )  // two *output* channels :
         { DOBINI();  // -> Sound_iSourceCodeLine
           for( i=0; i<iNumberOfSamples; i++ )
            { blTemp.i32 = *(long*)(pb+m_InBufPosition);
              pLeftData[i] = pRightData[i] =  (T_Float)blTemp.i32 * fltScalingFactor;
              m_InBufPosition += iBufIdxInc;
              // Added 2011-03-13 for callers which need to read HUGE BLOCKS in one over:
              if( m_InBufPosition >= m_InBufferSize )  // finished with this buffer
               {                                       // so use the next (*if* there's one)
                 pb = WrapInBufPositionAndGetPointerToNextFragment(CSOUND_WRAPBUF_OPT_WAIT_FOR_DATA, &pSoundBufferInfo);
                 if( pb==NULL ) // oops... no more filled 'input' data available ?!
                  { DOBINI();   // -> Sound_iSourceCodeLine
                    return -1;
                  }
               }
            }
         }
        else                      // only ONE output channel :
         { DOBINI();  // -> Sound_iSourceCodeLine
           for( i=0; i<iNumberOfSamples; i++ )
            { blTemp.i32 = *(long*)(pb+m_InBufPosition);
              pLeftData[i]= (T_Float)blTemp.i32 * fltScalingFactor;
              m_InBufPosition += iBufIdxInc;
              // Added 2011-03-13 for callers which need to read HUGE BLOCKS in one over:
              if( m_InBufPosition >= m_InBufferSize )  // finished with this buffer
               {                                       // so use the next (*if* there's one)
                 pb = WrapInBufPositionAndGetPointerToNextFragment(CSOUND_WRAPBUF_OPT_WAIT_FOR_DATA, &pSoundBufferInfo);
                 if( pb==NULL ) // oops... no more filled 'input' data available ?!
                  { DOBINI();   // -> Sound_iSourceCodeLine
                    return -1;
                  }
               }
            } // end for
         }
       } // end if <32 bit mono>
      else
      if(m_InFormat.nChannels==2) // 32 bit, Stereo
       { DOBINI();  // -> Sound_iSourceCodeLine
         for( i=0; i<iNumberOfSamples; i++ )
          { blTemp.i32 = *(long*)(pb+m_InBufPosition);
            pLeftData[i] =  (T_Float)blTemp.i32 * fltScalingFactor;
            m_InBufPosition += 4;
            blTemp.i32 = *(long*)(pb+m_InBufPosition);
            pRightData[i] =  (T_Float)blTemp.i32 * fltScalingFactor;
            m_InBufPosition += 4;
              // Added 2011-03-13 for callers which need to read HUGE BLOCKS in one over:
              if( m_InBufPosition >= m_InBufferSize )  // finished with this buffer
               {                                       // so use the next (*if* there's one)
                 pb = WrapInBufPositionAndGetPointerToNextFragment(CSOUND_WRAPBUF_OPT_WAIT_FOR_DATA, &pSoundBufferInfo);
                 if( pb==NULL ) // oops... no more filled 'input' data available ?!
                  { DOBINI();   // -> Sound_iSourceCodeLine
                    return -1;
                  }
               }
         } // end for
       } // end if <32 bit stereo>
    } // end if < 32 bits per sample >

  DOBINI();  // -> Sound_iSourceCodeLine

  if( m_InBufPosition >= m_InBufferSize )  // finished with this buffer
   {                                       // so add it back in to the Queue
     WrapInBufPositionAndGetPointerToNextFragment(CSOUND_WRAPBUF_OPT_DONT_WAIT, &pSoundBufferInfo);
   }
  m_i64NumSamplesRead += iNumberOfSamples;
  if( m_dblMeasuredInputSampleRate > 0 )
   { m_ldblSampleBasedInputTimestamp += (long double)iNumberOfSamples / m_dblMeasuredInputSampleRate;
         // (timestamp in UNIX format; used to provide a jitter-free timestamp
         //  in GetDateAndTimeForInput)
   }
  else if( m_dblNominalInputSampleRate > 0 )  // fall back to second best alternative..
   { m_ldblSampleBasedInputTimestamp += (long double)iNumberOfSamples / m_dblNominalInputSampleRate;
     // Note: this timestamp can only be accurate to a few ten milliseconds.
     //  If the application (spectrum lab) requires better timestamps, use GPS.
   }

  if( (m_InSampleLimit != 0) && (m_i64NumSamplesRead >= m_InSampleLimit) )
   {
     InClose();
     return 0;
   }
  DOBINI();   // -> Sound_iSourceCodeLine
  if( ( pOutChunkInfo != NULL ) && ( iNumberOfSamples>0 ) )
   {  pOutChunkInfo->nChannelsPerSample = nChannels;
      pOutChunkInfo->dwNrOfSamplePoints = iNumberOfSamples;
   }

  return iNumberOfSamples;
} // end CSound::InReadStereo()


#if(0)
//----------------------------------------------------------------------
int CSound::ReadInputSamplePoints( // preferred method since 2011-06
        float *pfltDest,            // [out] audio samples, as 32-bit FLOATING POINT numbers, grouped as "sample points"
        int   iNumSamplePoints,     // [in] number of SAMPLE POINTS(!) to read
        int   nChannelsPerSample,   // [in] number of samples PER SAMPLE POINT
        int   iTimeout_ms ,         // [in] max timeout in milliseconds, 0=non-blocking
        int   *piHaveWaited_ms,     // [out,optional] "have been waiting here for XX milliseconds"
        T_ChunkInfo *pOutChunkInfo) // [out,optional] chunk info, see c:\cbproj\SoundUtl\ChunkInfo.h, MAY BE NULL(!)
  // Returns the number of sample-POINTS, or a negative error code .
{
  return AIO_Host_ReadInputSamplePoints(
        &aio_In,                 // [in,out] DLL-host instance data
        pfltDest,iNumSamplePoints,nChannelsPerSample,iTimeout_ms,
        pOutChunkInfo,           // [out,optional] chunk info
        NULL ); // [in,optional] address of a callback function; not used here (NULL)
} // end CSound::AIO_ReadInputSamplePoints()
#endif


//---------------------------------------------------------------------------
void CSound::InClose()
 //  Closes the Soundcard Input if open and free up resources.
{
 int i,j;
 int result;

  DOBINI();

  m_InputOpen = FALSE;  // keep application away from accessing buffers
    // (2008-10-19: this should also prevent "starving"
    //              in ..InRead..() : WaitForSingleObject(), but apparently
    //              didn't .
  if ( m_hwvin != NULL) // used MMSYSTEM for input ?
    {
      if( (result=waveInReset(m_hwvin)) != MMSYSERR_NOERROR)
       { // waveInReset() went wrong... wonder why, only windooze knows ;-)
        if( m_ErrorCodeIn==0)   // set debugger breakpoint HERE
            m_ErrorCodeIn = result;
       }
      for( i=0; i<m_NumInputBuffers; i++ )
       {
        if( m_InputWaveHdr[i].dwFlags & WHDR_PREPARED )
         {  DOBINI();
          if( (result=waveInUnprepareHeader(m_hwvin, &m_InputWaveHdr[i],
                 sizeof (WAVEHDR)) ) != MMSYSERR_NOERROR)
           { // 'UnprepareHeader' can go wrong if MMSYSTEM still playing
             //  (yes, "playing", thats what the doc says..)
             DOBINI();
             // 2005-04-23: Got here for the WOLF GUI with result=33
             //   which means "WAVERR_STILLPLAYING" (?\cbuilder6\include\mmsystem.h).
             //   A-ha . Who's "still playing", any why ?!
             //   Win32 programmer's reference about waveInUnprepareHeader() :
             // > WAVERR_STILLPLAYING :
             // >   The buffer pointed to by the pwh parameter is still in the queue.
             // A crude fix (not even a kludge) ...
             j = 10;  // wait up to 10 * 50ms 'til MMSYSTEM finished with this buffer:
             while( (j>0) && result==WAVERR_STILLPLAYING )
              { Sleep(50);
                result=waveInUnprepareHeader(m_hwvin, &m_InputWaveHdr[i], sizeof(WAVEHDR) );
                --j;
              }
             if( result!=MMSYSERR_NOERROR)
              { if( m_ErrorCodeIn==0)   // set debugger breakpoint HERE
                    m_ErrorCodeIn = result;
              }
           }
         }
       } // end for(i..)
      DOBINI();
      if( (result=waveInClose(m_hwvin)) != MMSYSERR_NOERROR)
       { // a CLOSE-action failed... very mysterious but IT DOES HAPPEN !
         // (last occurred with result = 5 which means:  ???  )
        if( m_ErrorCodeIn==0)   // set debugger breakpoint HERE
            m_ErrorCodeIn = result;
       }
      m_hwvin = NULL;
    }

  DOBINI();

  if(m_iUsingASIO & 1)    // was using ASIO for input ?
   { m_iUsingASIO &= ~1;  // clear "using-ASIO-for-input"-flag
     if( m_iUsingASIO==0 )     // ASIO no longer in use now ?
      {
#if( SWI_ASIO_SUPPORTED )      // ASIO really support (through DL4YHF's wrapper)
        if( m_hAsio != C_ASIOWRAP_ILLEGAL_HANDLE )
         {
           DOBINI();  // -> Sound_iSourceCodeLine
           AsioWrap_Close( m_hAsio );
           DOBINI();  // -> Sound_iSourceCodeLine
           m_hAsio = C_ASIOWRAP_ILLEGAL_HANDLE;
         }
#endif // SWI_ASIO_SUPPORTED
      }
   } // end if(m_iUsingASIO & 1)


#if( SWI_AUDIO_IO_SUPPORTED )
  AIO_Host_CloseAudioInput( &aio_In );
  // 2012-10-26: Crashed above (in C:\RTL-SDR\RTLSDRGUI\ExtIO_RTLSDR.dll) ??
  // 2012-11-18: Same shit again (in C:\RTL-SDR\RTLSDRGUI\ExtIO_RTLSDR.dll) .
#endif // SWI_AUDIO_IO_SUPPORTED

  DOBINI();  // -> Sound_iSourceCodeLine
} // end InClose()

//---------------------------------------------------------------------------
long CSound::InGetNominalSamplesPerSecond(void)
{ return m_InFormat.nSamplesPerSec;
}

//---------------------------------------------------------------------------
double CSound::InGetMeasuredSamplesPerSecond(void)
  // -> INTERNALLY MEASURED sample rate; takes a few seconds to stabilize !
{ return m_dblMeasuredInputSampleRate;
}

//---------------------------------------------------------------------------
double CSound::OutGetMeasuredSamplesPerSecond(void)
  // Planned; similar as InGetMeasuredSamplesPerSecond() but for the OUTPUT (DAC).
{ return m_dblMeasuredOutputSampleRate;
}

//---------------------------------------------------------------------------
int CSound::InGetNumberOfChannels(void)
{
  return m_InFormat.nChannels;
}

//---------------------------------------------------------------------------
int CSound::InGetBitsPerSample(void)
{
  return m_InFormat.wBitsPerSample;
}



//////////////////////////////////////////////////////////////////////
//  This function opens a Soundcard for writing.
//Sample size can be 1,2 or 4 bytes long depending on bits per sample
// and number of channels.( ie. a 1000 sample buffer for 16 bit stereo
// will be a 4000 byte buffer)
//  Output does not start until at least half the buffers are filled
//  or m_OutWrite() is called with a length of '0' to flush all buffers.
//    parameters:
//      iOutputDeviceID = device ID for waveOutOpen.
//                  See Win32 Programmer's Reference for details.
//      pWFX    = WAVEFORMATEX structure with desired soundcard settings
//      BufSize = DWORD specifies the soundcard buffer size number
//                  in number of samples to buffer.
//                 If this value is Zero, the soundcard is opened and
//                  then closed. This is useful   for checking the
//                  soundcard.
//      SampleLimit = limit on number of samples to be written. 0 signifies
//                  continuous output stream.
//    returns:
//        0         if opened OK
//      ErrorCode   if not
//////////////////////////////////////////////////////////////////////
UINT CSound::OutOpen(
    //ex:int iOutputDeviceID,     // Identifies a soundcard, or a replacement like C_SOUND_DEVICE_AUDIO_IO
          // negative: one of the C_SOUND_DEVICE_... values defined in C:\cbproj\SoundUtl\Sound.h
          //  ( C_SOUND_DEVICE_DEFAULT_WAVE_INPUT_DEVICE = -1 is dictated by Windows;
          //    C_SOUND_DEVICE_SDR_IQ,   C_SOUND_DEVICE_PERSEUS,
          //    C_SOUND_DEVICE_AUDIO_IO, C_SOUND_DEVICE_ASIO  all added by DL4YHF )
        char *pszAudioOutputDeviceOrDriver, // [in] name of an audio device or DL4YHF's "Audio I/O driver DLL" (full path)
        char *pszAudioStreamID,   // [in] 'Stream ID' for the Audio-I/O-DLL, identifies the AUDIO SOURCE (here: mandatory)
        char *pszDescription,     // [in] a descriptive name which identifies
           // the audio source ("audio producer"). This name may appear
           // on a kind of "patch panel" on the configuration screen of the Audio-I/O DLL .
           // For example, Spectrum Lab will identify itself as "SpectrumLab1"
           // for the first running instance, "SpectrumLab2" for the 2nd, etc .
        long i32SamplesPerSecond, // nominal sampling rate (not the "true, calibrated value"!)
        int  iBitsPerSample,      // ..per channel, usually 16 (also for stereo, etc)
        int  iNumberOfChannels,   // 1 or 2 channels (mono/stereo)
        DWORD BufSize,      // output buffer size; measured in SAMPLE POINTS .
                 // (must be at least as large as the 'processing chunk size';
                 //  see caller in SoundThd.cpp : SoundThd_InitializeAudioDevices )
        DWORD SampleLimit ) // usually 0 = "continuous stream"
{
 int i,j;
 int newOutBufferSize/*per "part"*/, newNumOutputBuffers;
 long i32TotalBufSizeInByte;
 char *pszAudioDeviceNameWithoutPrefix;
 BYTE *pb;
#if( SWI_ASIO_SUPPORTED )      // Support ASIO audio drivers too ?
 ASIOChannelInfo *pAsioChannelInfo;
#endif

  DOBINI();

   if(m_OutputOpen)
    { OutClose();     // -> m_hwvout=NULL
    }
   m_OutputOpen = FALSE;
   m_OutWaiting = FALSE;
   m_OutUnderflow = FALSE;
   if(m_OutEventHandle != NULL)
    { ResetEvent(m_OutEventHandle); // let OutFlushBuffer() wait when it needs to (!)
      // (without this, it didn't WAIT anymore after RE-opening the output,
      //  causing the output buffers to overflow )
    }
   m_sz255LastErrorStringOut[0] = 0;
   m_iOutHeadIndex = m_iOutTailIndex = 0; // output buffer EMPTY now (explained in OutFlushBuffer)
   m_OutSampleLimit = SampleLimit;  // if only a limited number of samples is to be played
   m_OutBufPosition = 0;
   m_dblMeasuredOutputSampleRate = i32SamplesPerSecond; // INITIAL VALUE (not 'measured' yet)
   // ex: m_OutFormat = *pWFX;    // copy a lot of params from WAVEFORMATEX struct
   // WoBu didn't want to have this damned "waveformatex"-stuff in the interface,
   // so got rid of this micro$oft-specific stuff, and use it only internally :
     // Prepare some soundcard settings (WAVEFORMATEX) required for opening,
     //  ALSO FOR THE AUDIO FILE READER / WRITER (!)
   m_OutFormat.wFormatTag = WAVE_FORMAT_PCM;
     // Since 2003-12-14, the "number of channels" for the ADC is not necessarily
     //       the same as the number of channels in the internal process !
   m_OutFormat.nChannels = (WORD)iNumberOfChannels;   // only 1 or 2 so far..
   m_OutFormat.wBitsPerSample = (WORD)iBitsPerSample;  // ex 16
   m_OutFormat.nSamplesPerSec = i32SamplesPerSecond; /* ex 11025 */
     // note that the WFXSettings require the "uncalibrated" sample rate !!
   m_OutFormat.nBlockAlign = (WORD)( m_OutFormat.nChannels *(m_OutFormat.wBitsPerSample/8) );
   m_OutFormat.nAvgBytesPerSec = m_OutFormat.nSamplesPerSec * m_OutFormat.nBlockAlign;
   m_OutFormat.cbSize = 0/*!*/; // no "extra format information" appended to the end of the WAVEFORMATEX structure
   m_OutBytesPerSample = (m_OutFormat.wBitsPerSample/8)*m_OutFormat.nChannels;

   // m_dblMeasuredOutputSampleRate = i32SamplesPerSecond; // initial value for the 'measured' SR

  if( CountBits(m_dwOutputSelector) < m_OutFormat.nChannels )
   { m_dwOutputSelector = ( 0xFFFFFFFF >> (32-m_OutFormat.nChannels) );
   }

  pszAudioDeviceNameWithoutPrefix = Sound_GetAudioDeviceNameWithoutPrefix(
                           pszAudioOutputDeviceOrDriver, &m_iOutputDeviceID );


  // Use DL4YHF's  "Audio-IO" for output ?  ( something like in_AudioIO.DLL, see ?\AudioIO\*.* )
  if( m_iOutputDeviceID == C_SOUND_DEVICE_AUDIO_IO )
   {
#if( SWI_AUDIO_IO_SUPPORTED ) // host for AUDIO-I/O-DLLs, later also for Winrad ExtIO :

#else // !SWI_AUDIO_IO_SUPPORTED )
     m_ErrorCodeOut = SOUNDIN_ERR_NOTOPEN;
     return m_ErrorCodeOut;
#endif // SWI_AUDIO_IO_SUPPORTED
   } // end if( m_iOutputDeviceID == C_SOUND_DEVICE_AUDIO_IO )
  else // not AUDIO_IO, but what ? ...
   { // ....
   }


  // use MMSYSTEM or ASIO driver for OUTput ?
  if( (m_iOutputDeviceID==C_SOUND_DEVICE_ASIO)
     && APPL_fMayUseASIO // Since 2011-08-09, use of ASIO may be disabled through the command line
     )
   {
#if( SWI_ASIO_SUPPORTED )      // Support ASIO audio drivers too ?
      if((m_iUsingASIO&2)==0 ) // only if ASIO driver not already in use for OUTPUT..
       {
        if( !AsioWrap_fInitialized )
         {  AsioWrap_Init();     // init DL4YHF's ASIO wrapper if necessary
         }
        // Prepare the most important ASIO settings :
        AsioSettings MyAsioSettings;
        DOBINI();
        AsioWrap_InitSettings( &MyAsioSettings,
                               iNumberOfChannels,
                               i32SamplesPerSecond,
                               iBitsPerSample );
        DOBINI();
        MyAsioSettings.i32InputChannelBits  = m_dwInputSelector;
        MyAsioSettings.i32OutputChannelBits = m_dwOutputSelector;
        if( m_hAsio == C_ASIOWRAP_ILLEGAL_HANDLE )
         { // Only open the ASIO driver if not open already. Why ?
           // because an ASIO device always acts as IN- AND(!) OUTPUT simultaneously.
           m_hAsio = AsioWrap_Open( pszAudioDeviceNameWithoutPrefix, &MyAsioSettings, CSound_AsioCallback, (DWORD)this );
           if( (m_hAsio==C_ASIOWRAP_ILLEGAL_HANDLE) && (AsioWrap_sz255LastErrorString[0]!=0) )
            { // immediately save error string for later:
              strncpy( m_sz255LastErrorStringOut, AsioWrap_sz255LastErrorString, 79 );
              m_sz255LastErrorStringOut[79] = '\0';
            }
         }
        DOBINI();
        if( m_hAsio != C_ASIOWRAP_ILLEGAL_HANDLE ) // ASIO wrapper willing to co-operate ?
         { m_iUsingASIO |= 2;    // now using ASIO for OUTput...
           // BEFORE allocating the buffers, check if the ASIO driver will be kind enough
           // to accept the samples in the format *WE* send it ( some stubborn drivers don't...)
           pAsioChannelInfo = AsioWrap_GetChannelInfo( m_hAsio, 0/*iChnIndex*/, 0/*0= for OUTPUT*/ );
           if( pAsioChannelInfo!=NULL )
            { switch(pAsioChannelInfo->type)
               { // SOO many types, so hope for the best, but expect the worst .....  see ASIO.H !
                 case ASIOSTInt16LSB   : // the most usual format on an INTEL CPU (LSB first)
                      m_OutFormat.wBitsPerSample = 16;
                      break;
                 case ASIOSTInt24LSB   : // may be used too (but not by Creative)
                      m_OutFormat.wBitsPerSample = 24;
                      break;
                 case ASIOSTInt32LSB   : // Creative's "updated" driver usese
                                         // this format for 24(!)-bit output !
                      m_OutFormat.wBitsPerSample = 32;
                      break;

                 // a dirty dozen other formats are not supported here ! !
                 default:
                      strcpy( m_sz255LastErrorStringOut, "ASIO data type not supported" );
                      break;

                } // end switch(pAsioChannelInfo->type)
               // Revise the "number of bytes per sample" based on the above types:
               m_OutBytesPerSample = (m_OutFormat.wBitsPerSample/8)*m_OutFormat.nChannels;
            } // end if < got info about this ASIO OUTput channel >
         } // end if < successfully opened the ASIO driver >
        else  // bad luck (happens !) :  ASIO driver could not be loaded
         { m_ErrorCodeOut = ERROR_WITH_ASIO_DRIVER;
           if( m_sz255LastErrorStringOut[0] == 0 )
            { strcpy( m_sz255LastErrorStringOut, "Could not load ASIO driver" );
            }
           DOBINI();
           return m_ErrorCodeOut;
         }
       } // end if < ASIO driver not loaded yet >
#else  // name of ASIO driver specified, but ASIO not supported in compilation:
     m_ErrorCodeOut = ERROR_WITH_ASIO_DRIVER;
     if( m_sz255LastErrorStringOut[0] == 0 )
      { strcpy( m_sz255LastErrorStringOut, "Could not load ASIO driver" );
      }
     DOBINI();
     return m_ErrorCodeIn;
#endif // SWI_ASIO_SUPPORTED
   } // end if < use ASIO driver instead of MMSYSTEM ? >

  // Note: m_OutBytesPerSample may have been changed depending on ASIO driver .
  //  The following output-buffer-estimation applies to ASIO and MMSYSTEM :
  if( BufSize < m_InFormat.nSamplesPerSec/20 )
    {  BufSize = m_InFormat.nSamplesPerSec/20;  // at least 50 ms of audio !!
    }
  i32TotalBufSizeInByte = (long)(2*BufSize) * m_OutBytesPerSample; // may be as much as a HALF MEGABYTE !
  DOBINI();

  // Event for callback function to signal next buffer is free.
  // This Event will be Set in the output callback to wake up the 'writer' .
  if( m_OutEventHandle==NULL )
   {  m_OutEventHandle = CreateEvent(
           NULL,   // pointer to security attributes
           FALSE,  // flag for manual-reset event. If FALSE, Windows automatically resets the state to nonsignaled
                   //  after a single waiting thread has been released.
           FALSE,  // flag for initial state.  FALSE = "not signalled"
           NULL);  // pointer to event-object name
   }

  // open sound card output, depending on whether MMSYSTEM,
  //  ASIO driver,  or DL4YHF's AudioIO system   shall be used :
  if( m_iUsingASIO & 2 )   // using ASIO for input :
   {
#if( SWI_ASIO_SUPPORTED )      // Support ASIO audio drivers too ?
      // Already opened the driver above, but did NOT start it then,
      // because the buffers were not allocated yet.
#endif // SWI_ASIO_SUPPORTED
    }
   else if( m_iOutputDeviceID==C_SOUND_DEVICE_AUDIO_IO )
    {
#if( SWI_AUDIO_IO_SUPPORTED )
      if( aio_Out.h_AudioIoDLL == NULL )
       { // In this case, pszAudioDriverName should contain THE FULL PATH to the DLL
         // because SL must load this DLL from the very same place as Winamp...
         // ... which is, of course, Winamp's "plugins" folder !
         if( AIO_Host_LoadDLL( &aio_Out, pszAudioOutputDeviceOrDriver ) < 0 )
          {
           m_ErrorCodeOut = SOUNDOUT_ERR_NOTOPEN;
           strcpy( m_sz255LastErrorStringOut, "Couldn't load AUDIO_IO DLL" );
           DOBINI();
           return m_ErrorCodeOut;
          }
       }
      if( AIO_Host_OpenAudioOutput(
                &aio_Out,           // [in,out] T_AIO_DLLHostInstanceData *pInstData, DLL-host instance data
                pszAudioStreamID,   // [in] 'Stream ID' for the Audio-I/O-DLL, identifies the AUDIO SOURCE (here: mandatory)
                "SpectrumLab",      // [in] a descriptive name which identifies
                    // the audio source ("audio producer"). This name may appear
                    // on the "patch panel" on the configuration screen of the Audio-I/O DLL .
                i32SamplesPerSecond, // [in] samples per second, "precise" if possible
                iNumberOfChannels,   // [in] 1=mono, 2=stereo, 4=quadrophonic
                200,                 // [in] max timeout in milliseconds for THIS call
                AIO_OPEN_OPTION_NORMAL // [in] bit combination of AIO_OPEN_OPTION_... flags
           ) < 0 )
       {   // failed to open the AUDIO-I/O-DLL for "output" :
           m_ErrorCodeOut = SOUNDOUT_ERR_NOTOPEN;
           strcpy( m_sz255LastErrorStringOut, "Couldn't open AUDIO_IO for output" );
           DOBINI();
           return m_ErrorCodeOut;
       }
      else // successfully opened the AUDIO-I/O-DLL for "output"
       {
       }
#endif  // SWI_AUDIO_IO_SUPPORTED
    }
   else // neither ASIO driver nor AudioIO used for output, but MMSYSTEM :
    { int iSoundOutputDeviceID;

      SOUND_fDumpEnumeratedDeviceNames = TRUE;  // flag for Sound_OutputDeviceNameToDeviceID()
      iSoundOutputDeviceID = Sound_OutputDeviceNameToDeviceID( pszAudioOutputDeviceOrDriver );


     if ( iSoundOutputDeviceID < 0)
          iSoundOutputDeviceID = WAVE_MAPPER;

     UTL_WriteRunLogEntry( "CSound::OutOpen: Trying to open \"%s\", ID #%d, %d Hz, %d bit, %d channel(s), for output.",
                              (char*)pszAudioDeviceNameWithoutPrefix,
                              (int)iSoundOutputDeviceID,
                              (int)m_OutFormat.nSamplesPerSec,
                              (int)m_OutFormat.wBitsPerSample,
                              (int)m_OutFormat.nChannels );

     // 2008-05-04: BCB triggered a strange breakpoint deep inside waveOutOpen, why ??
     if( (m_ErrorCodeOut = waveOutOpen( &m_hwvout, iSoundOutputDeviceID, &m_OutFormat,
            (DWORD)WaveOutCallback, (DWORD)this, CALLBACK_FUNCTION ) )
         != MMSYSERR_NOERROR )
      { // Could not open the WAVE OUTPUT DEVICE with the old-fashioned way.
        //    Try something new, using a WAVEFORMATEXTENSIBLE structure ...
        WAVEFORMATEXTENSIBLE wfmextensible;
        // don't know what may be in it, so clear everything (also unknown components)
        memset( &wfmextensible, 0, sizeof(WAVEFORMATEXTENSIBLE) );
        // Now set all required(?) members. Based on info found from various sources.
        // Note that the new WAVEFORMATEXTENSIBLE includes the old WAVEFORMATEX .
        wfmextensible.Format = m_OutFormat; // COPY the contents of the WAVEFORMATEX(!)
        wfmextensible.Format.cbSize          = sizeof(WAVEFORMATEXTENSIBLE);
        wfmextensible.Format.wFormatTag      = WAVE_FORMAT_EXTENSIBLE;
        // wfmextensible.Format.nChannels, .wBitsPerSample, .nBlockAlign,
        //                     .nSamplesPerSec, .nAvgBytesPerSec already set !
        wfmextensible.Samples.wValidBitsPerSample = wfmextensible.Format.wBitsPerSample;
        wfmextensible.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
        // Now make a guess for the speaker configuration :
        wfmextensible.dwChannelMask = SPEAKER_FRONT_LEFT;   // MONO ...
        if(wfmextensible.Format.nChannels >= 2)             // STEREO..
           wfmextensible.dwChannelMask |= SPEAKER_FRONT_RIGHT;
        if(wfmextensible.Format.nChannels >= 3)             // ... and more ...
           wfmextensible.dwChannelMask |= SPEAKER_FRONT_CENTER;
        if(wfmextensible.Format.nChannels >= 4)
           wfmextensible.dwChannelMask |= SPEAKER_LOW_FREQUENCY;
        if(wfmextensible.Format.nChannels >= 5)
           wfmextensible.dwChannelMask |= SPEAKER_BACK_LEFT ;
        if(wfmextensible.Format.nChannels >= 6)             // typical "5.1" system
           wfmextensible.dwChannelMask |= SPEAKER_BACK_RIGHT;

        // Try again to open the audio output device, this time passing a pointer
        //  to a WAVEFORMATEXTENSIBLE struct (instead of the old WAVEFORMATEX ) :
        DOBINI();
        if( (m_ErrorCodeOut = waveOutOpen( &m_hwvout, iSoundOutputDeviceID,
               (WAVEFORMATEX*)&wfmextensible ,
               (DWORD)WaveOutCallback, (DWORD)this, CALLBACK_FUNCTION ) )
           != MMSYSERR_NOERROR )
         {
           // Arrived here ? waveOutOpen() worked neither the OLD STYLE nor the NEW WAY.
           OutClose();   // m_ErrorCodeOut = MMSYSERR_xxx
           DOBINI();
           return m_ErrorCodeOut;
         }
      } // end if <could not open the WAVE INPUT DEVICE with the old-fashioned way>
    } // end else < not using ASIO driver for output but MMSYSTEM >

   if( BufSize == 0 )  // see if just a soundcard check
    {
      OutClose();      // if so close the soundcard OUTput
      DOBINI();
      return 0;
    }

  // Start out paused so don't let output begin until some buffers are filled.
  //   ( m_fOutputHoldoff will be SET somewhere below for this purpose )
  if( m_hwvout != NULL )
   {  // only if mmsystem used for output ..
     DOBINI();
     if( (m_ErrorCodeOut = waveOutPause( m_hwvout ))!= MMSYSERR_NOERROR )
      {
        OutClose();   // m_ErrorCodeOut = MMSYSERR_xxx
        DOBINI();
        return m_ErrorCodeOut;
      }
   } // end if( m_hwvout != NULL )

  DOBINI();

  // Allocate and prepare all the output buffers ....
  // How many OUTPUT buffers ? Should be enough for 0.2 to 0.5 seconds :
  // Allocate and prepare all the output buffers .... same principle as in InOpen:
  i = i32TotalBufSizeInByte / 4/*parts*/; // -> approx size of each buffer-part, in BYTES.
  if(i<1024)  i=1024;    // at least 512 bytes(!) per buffer-part
  if(i>65536) i=65536;   // maximum  64 kByte per buffer-part
  newNumOutputBuffers = (i32TotalBufSizeInByte+i-1) / i;
  ++newNumOutputBuffers; // ..because one of the N buffer-parts must remain empty
  if( newNumOutputBuffers < 4)  // should use at least FOUR buffers, and..
      newNumOutputBuffers = 4;
  if( newNumOutputBuffers > SOUND_MAX_OUTPUT_BUFFERS ) // a maximum of SIXTEEN buffers..
      newNumOutputBuffers = SOUND_MAX_OUTPUT_BUFFERS;
  // How large should each buffer-part be ?
  i = (i32TotalBufSizeInByte + newNumOutputBuffers/*round up*/ - 1)  / newNumOutputBuffers;
  // For what it's worth, the size of each 'buffer part' should be a power of two:
  newOutBufferSize = 512;  // .. but at last 512  [bytes per 'buffer part']
  while( (newOutBufferSize < i) && (newOutBufferSize < 65536) )
   { newOutBufferSize <<= 1;
   }
  DOBINI();  // -> Sound_iSourceCodeLine
  if(m_pbOutputBuffer != NULL )
   { // free old buffer (with possibly different SIZE) ?
     if( (m_OutBufferSize!=newOutBufferSize) || (m_NumOutputBuffers!=newNumOutputBuffers) )
      { m_OutBufferSize = m_NumOutputBuffers = 0; // existing buffers are now INVALID
        UTL_free( m_pbOutputBuffer );
        m_pbOutputBuffer = NULL;
      }
   }
  if( m_pbOutputBuffer==NULL )
   {
     m_pbOutputBuffer = (BYTE*)UTL_NamedMalloc( "TSoundO", newNumOutputBuffers * newOutBufferSize);
     m_OutBufferSize    = newOutBufferSize/*per "part"*/;
     m_NumOutputBuffers = newNumOutputBuffers;
   }
  if ( m_pbOutputBuffer==NULL )
   { // could not allocate the sound buffer .. sh..
     m_OutBufferSize = m_NumOutputBuffers = 0; // existing buffers are INVALID
     OutClose();
     m_ErrorCodeOut = MEMORY_ERROR;
     DOBINI();
     return m_ErrorCodeOut;
   }

  // initialize output data buffer (which is in fact a SINGLE CONTIGUOUS BLOCK)
  pb = m_pbOutputBuffer;
  j  = m_OutBufferSize/*per "part"*/  *  m_NumOutputBuffers;
  while( (j--) > 0 ) // clear this sound buffer
   { *pb++ = 0;
   }

  // Prepare those strange 'wave headers' (windows multimedia API) ..
  for(i=0; i<m_NumOutputBuffers; i++ )
   {
     // initialize output data headers
     //   (they should ALL be unprepared, and NOT IN USE in this moment,
     //    which can be seen by the 'dwFlags' member of each block :
     if(  m_OutputWaveHdr[i].dwFlags  &
            (   WHDR_INQUEUE/*16*/ /* "Set by Windows to indicate that the buffer is queued for playback" */
             |  WHDR_PREPARED/*2*/ /* "... the buffer has been prepared with waveOutPrepareHeader" */
       )    )
      { LOCAL_BREAK(); // << set breakpoint HERE (or in "LocalBreakpoint" )
      }
     m_OutputWaveHdr[i].dwBufferLength = m_OutBufferSize;
     m_OutputWaveHdr[i].dwBytesRecorded= 0; // here: means "number of bytes PLAYED" from this buffer
     m_OutputWaveHdr[i].dwFlags = 0;
     m_OutputWaveHdr[i].dwUser  = 0; // ex: = NULL; but Dev-C++ complained
     m_OutputWaveHdr[i].dwLoops = 0;
     m_OutputWaveHdr[i].lpData  = (LPSTR)m_pbOutputBuffer + i*m_OutBufferSize;

     // prepare output data headers (only if MMSYSTEM is used here) :
     DOBINI();
     if( m_hwvout != NULL )
      { if(  (m_ErrorCodeOut = waveOutPrepareHeader(m_hwvout, &m_OutputWaveHdr[i],
              sizeof(WAVEHDR)) ) != MMSYSERR_NOERROR )
         {
           OutClose();   // m_ErrorCodeOut = MMSYSERR_xxx
           DOBINI();
           return m_ErrorCodeOut;
         }
      } // end if( m_hwvout != NULL )
    } // end for <all output buffers>
  DOBINI();
  m_OutputOpen = TRUE;
  m_fOutputHoldoff = TRUE;  // output starts when enough samples are collected
  DOBINI();
  return(0);  // no error
}  // end OutOpen()

//--------------------------------------------------------------------------
BOOL CSound::IsOutputOpen(void)   // checks if the sound output is opened
{
  return m_OutputOpen;
}

//--------------------------------------------------------------------------
int  CSound::GetNumInputBuffers(void)
{
  return m_NumInputBuffers;  // number of audio buffers used
}


//--------------------------------------------------------------------------
int  CSound::GetNumSamplesPerInputBuffer(void)
{
  if( m_InBytesPerSample >= 1 )
      return m_InBufferSize / m_InBytesPerSample;
  else
      return m_InBufferSize;
}

//--------------------------------------------------------------------------
int  CSound::GetInputBufferUsagePercent(void)
{ // Checks the current 'buffer usage' of the sound input in percent.
  //   0 means all buffers empty (0..30% is OK for an input buffer),
  // 100 means all buffers are occupied (80..100% is BAD for an input,
  //                  because there's the risk to lose input samples)
 int occupied_buffers;

  if(m_NumInputBuffers<=0)   // no output buffers allocated ?
     return 0;
  if( m_InWaiting ) // waiting for an input buffer to become available ?
     return 0;      //  -> all input buffers are empty ! (which is GOOD)

  occupied_buffers = m_iInHeadIndex - m_iInTailIndex;

  if(occupied_buffers<0)  // circular buffer-index wrap
     occupied_buffers+=m_NumInputBuffers;

  return (100*occupied_buffers) / m_NumInputBuffers;
}


//--------------------------------------------------------------------------
int  CSound::GetOutputBufferUsagePercent(void)
{ // Checks the current 'buffer usage' of the sound output in percent.
  //   0 means all buffers empty (which is BAD for an output buffer),
  // 100 means all buffers are occupied (50..100% is GOOD for an output).
 int occupied_buffers;

  if(m_NumOutputBuffers<=0)   // no output buffers allocated ?
     return 0;
  if( m_OutWaiting )  // waiting for an output buffer to become available ?
     return 100;      //  -> all output buffers are occupied !

  // EnterCriticalSection(&m_CriticalSection);
  occupied_buffers = m_iOutHeadIndex - m_iOutTailIndex;
  // LeaveCriticalSection(&m_CriticalSection);

  if(occupied_buffers<0)  // ringbuffer wrap
     occupied_buffers+=m_NumOutputBuffers;
  if(occupied_buffers>0)  // subtract one because ONE of the buffers..
   --occupied_buffers;    // .. must remain empty, like a classic circular, lock-free, FIFO
  // (without this, dropouts were audible though occupied_buffers was ONE)

  return (100*occupied_buffers) / m_NumOutputBuffers;
}

//--------------------------------------------------------------------------
BOOL CSound::GetInputProperties(T_SoundSampleProperties *pProperties)
{
  if(pProperties)
   { pProperties->wChannels       = m_InFormat.nChannels;
     pProperties->dwSamplesPerSec = m_InFormat.nSamplesPerSec;
     pProperties->wBitsResolution = m_InFormat.wBitsPerSample;
     return TRUE;
   }
  else
     return FALSE;
} // end GetInputProperties()

//--------------------------------------------------------------------------
BOOL CSound::GetOutputProperties(T_SoundSampleProperties *pProperties)
{
  if(pProperties)
   { pProperties->wChannels       = m_OutFormat.nChannels;
     pProperties->dwSamplesPerSec = m_OutFormat.nSamplesPerSec;
     pProperties->wBitsResolution = m_OutFormat.wBitsPerSample;
     return TRUE;
   }
  else
     return FALSE;
} // end GetOutputProperties()

//---------------------------------------------------------------------
T_Float* CSound::ResizeInterleaveBuffer( int iNumSamplePoints )
   // Returns a pointer to the 'sufficiently resized' interleave buffer.
   // NULL means "out of memory" which is unlikely to happen.
{
  if( iNumSamplePoints > m_i32InterleaveBufferSize )
   { // The interleave/de-interleave buffer is too small, or doesn't exist.
     if( m_pfltInterleaveBuffer != NULL )
      { UTL_free( m_pfltInterleaveBuffer );
      }
     m_pfltInterleaveBuffer = (T_Float*)UTL_NamedMalloc( "IleavBuf", iNumSamplePoints * 2 * sizeof( T_Float ) );
     if( m_pfltInterleaveBuffer != NULL )
      { m_i32InterleaveBufferSize = iNumSamplePoints;
      }
     else
      { m_i32InterleaveBufferSize = 0;
      }
   }
  return m_pfltInterleaveBuffer;
} // end CSound::ResizeInterleaveBuffer()


//---------------------------------------------------------------------
int CSound::GetNumberOfFreeOutputBuffers(void)
   // Result:  0 ... m_NumOutputBuffers MINUS ONE (!)
{
   int i = m_iOutHeadIndex - m_iOutTailIndex; // -> number of OCCUPIED buffers
   if( i<0 )  i += m_NumOutputBuffers;    // .. circular unwrapped..
   i = m_NumOutputBuffers - 1 - i;        // -> number of FREE buffers (available for filling)

   // Note: like in a classic circular, lock-free FIFO,
   //       ONE of the <m_NumOutputBuffers> parts of the buffer
   //       is always empty.
   // Thus, the difference between the above two buffer indices (wrapped around)
   //       tells us (here for FOUR buffers, head minus tail modulo 4 ):
   //   difference = 0   :  all four buffers ('part') are EMPTY, and THREE are free for filling
   //   difference = 1   :  one of the four buffers is FILLED,   and TWO are free for filling
   //   difference = 2   :  two buffers are filled,              and ONE is free for filling
   //   difference = 3   :  three buffers are filled, WHICH IS THE MAXIMUM (!)
   //   difference = 4   :  impossible due to the modulo-operation

  return i;
} // end CSound::GetNumberOfFreeOutputBuffers()

//---------------------------------------------------------------------
int CSound::OutFlushBuffer(int iTimeout_ms) // flushes m_OutputWaveHdr[m_iOutHeadIndex] to the output device; and WAITS if necessary
   // Called INTERNALLY if( m_OutBufPosition >= m_OutBufferSize ) ,
   //   i.e. when another part of the output buffer is full enough
   //   to be sent to the soundcard .
   // Returns the number of MILLISECONDS spent here waiting
   //  (giving the CPU to other threads, which may be important for the caller to know),
   //  or a NEGATIVE result if the caller cannot add more data because
   //  all output buffers are FULL .
   //   Return value 0 means "didn't wait, and there's a free buffer now" .
   //
   //
   // Since 2013-02-17, m_iOutHeadIndex and m_iOutTailIndex are treated like
   //                   a classic, lock-free, circular FIFO :
   // * m_iOutHeadIndex==m_iOutTailIndex means NONE of the 'output buffers'
   //                    is filled, i.e. "completely empty", which indicates a problem
   //                    (output-buffer not filled quick enough -> audio drop-out) .
   // * ( (m_iOutHeadIndex+1) % m_NumOutputBuffers) == m_iOutTailIndex
   //                    means ALL buffers are full, and the caller must wait
   //                    (or be blocked) until more samples can be filled in.
   // * It's impossible to have ALL 'm_NumOutputBuffers' occupied and queued up for output,
   //                    because (for example with FOUR buffers)
   //                    if the HEAD-INDEX would be FOUR STEPS AHEAD of the
   //                    tail index, the situation (with FOUR FULL BUFFERS) would be:
   //           m_iOutHeadIndex==(m_iOutTailIndex+4) modulo 4 == m_iOutTailIndex;
   //                    i.e. same situation as for "completly empty" buffers !
   //
   // Example: Typical situation when OutFlushBuffer() is CALLED .
   //          When RETURNING from OutFlushBuffer(), the caller expects to be able
   //          to add more samples into the next buffer (at the 'HEAD' index).
   //          The audio output driver's 'output completion callback'
   //          will (later) increment the TAIL index (m_iOutTailIndex) .
   //
   //   BUFFER INPUT SIDE:                       BUFFER OUTPUT SIDE:
   //     "Head Index"                             "Tail Index"
   //                          ____________
   // m_iOutHeadIndex --->    |Buffer[0] : | (this buffer is currently being filled
   //   |                     | filling    |       in CSound::OutWriteStereo,
   //   |                     |____________|       then passed to waveOutWrite()
   //   | (when this part of the buffer
   //   |       is COMPLETELY filled,
   //   |  OutFlushBuffer waits
   //   |    until Buffer[new_head_index]
   //   |    is not 'playing' anymore,
   //   |  then the new (incremented)
   //   |  HEAD INDEX is set in OutFlushBuffer )
   //   |
   //  \|/                      ____________
   //   - -new_head_index- -> |Buffer[1] : | <--- m_iOutTailIndex (don't touch this buffer yet
   //                         | PLAYING    |        |              because it's used by mmsystem.
   //                         |____________|        |  The TAIL index will be incremented in WaveOutCallback(),
   //                                               |  when output is "done" by the soundcard driver / MMSYSTEM !
   //                                               |
   //                          ____________        \|/
   //                         |Buffer[2] : | <- - - - - - -    (waiting to be played)
   //                         |  queued up |        .
   //                         |____________|        .
   //                                               .
   //                                               .
   //                          ____________        \./
   //                         |Buffer[3] : | <- - - - - - -    (waiting to be played)
   //                         |  queued up |
   //                         |____________|
   //
   //
   //
   //
{
  LONGLONG t1, t2;
  MMRESULT result;
  int  i, new_head_index, iWaited_ms = 0, iWaited_for_single_object = 0;
  DWORD dw, dwFlags1, dwFlags2, dwFlags3, dwFlags4;
  BOOL  must_wait = FALSE;

  if(  (m_NumOutputBuffers<=0) || (m_OutEventHandle==NULL) )
   { if( m_ErrorCodeOut == 0 )
      {  m_ErrorCodeOut = SOUNDOUT_ERR_NOTOPEN;
      }
     return -1;   // definitely 'not open for business'
   }


  dwFlags1 = m_OutputWaveHdr[m_iOutHeadIndex].dwFlags; // for diagnostics..
  dwFlags2 = dwFlags3 = dwFlags4 = 0;


  // When m_OutBufPosition reaches m_OutBufferSize, it's time to switch
  //      to the next buffer part. If no 'free' buffer is availble, wait .
  if( m_OutBufPosition >= m_OutBufferSize )
   {
     m_OutBufPosition = 0;  // sample index to fill the NEW output block (one part of the buffer)

     i = GetNumberOfFreeOutputBuffers(); // -> 0 ... m_NumOutputBuffers MINUS ONE (!)

     // If all buffers are full then need to wait.  See ASCII graphics above !
     if( i<=0 )  // according to the FIFO indices, all buffer-parts are full !
      { must_wait = TRUE;  // this is the 'normal' way to detect 'no available buffer' ..
      }
     new_head_index = (m_iOutHeadIndex+1) % m_NumOutputBuffers;
     if( m_hwvout != NULL ) // only for the standard windoze multimedia API ..
      {
        dwFlags2 = m_OutputWaveHdr[new_head_index].dwFlags; // 2nd check for a 'free' buffer:
        if( (dwFlags2 & WHDR_INQUEUE ) != 0 )  // the 'new' buffer is still PLAYING..
         { must_wait = TRUE;
           if( i>0 ) // Ooops.. buffer NOT FULL ?! ?!
            { // THIS SHOULD NOT HAPPEN .
              // 2013-02-17 : Got here ..
              //   dwFlags1        = 18
              //   dwFlags2        = 18
              //   m_NumOutputBuffers = 4
              //   m_iOutHeadIndex    =  2
              //   new_head_index     =  3
              //   m_iOutTailIndex    =  2
              //   m_OutWaiting    = TRUE
              //   m_OutputWaveHdr[0].dwFlags = 18
              //   m_OutputWaveHdr[1].dwFlags = 18
              //   m_OutputWaveHdr[2].dwFlags = 18
              //   m_OutputWaveHdr[3].dwFlags = 18  (i.e. ALL buffers "still playing")
              // (the resason for this 'illegal situation' must have been a previous call)
              //
            }
         } // end if < the 'new' (next-to-be-filled) buffer is STILL BEING PLAYED ? >
      } // end if < using the 'waveOut' API >

     if( must_wait ) // -> WAIT until a buffer gets available, or bail out (if caller doesn't want to be blocked)
      {
        if( iTimeout_ms > 0 )   // CALLER said it's ok to wait ..
         { m_OutWaiting = TRUE; // here set in OutFlushBuffer(), cleared in audio callback
           //
           // Wait for mmsystem to free up a new buffer, i.e. 'finish using' it.
           //
           // ex: m_OutWaiting = TRUE; // set in OutFlushBuffer(), cleared in audio callback
           //     2013-02-10 : m_OutWaiting now set BEFORE calling waveOutWrite() !
           // Wait until the output callback function
           //      calls  SetEvent( pCSound->m_OutEventHandle) ..
           // 2008-10-19 : Strange things happened when WaitForSingleObject
           //     was called from a thread with THREAD_PRIORITY_ABOVE_NORMAL !
           // 2013-02-17 : Does SetEvent *queue up multiple events* for the
           //     same 'consumer' (thread which calls WaitForSingleObject) ?
           //     WaitForSingleObject seemed to return immediately, even though
           //     m_OutputWaveHdr[m_iOutHeadIndex] was 'still playing' ....
           DOBINI();
           QueryPerformanceCounter( (LARGE_INTEGER*)&t1 );

           if( (dw=WaitForSingleObject( m_OutEventHandle, iTimeout_ms ) )
                                     != WAIT_OBJECT_0 )
            { DOBINI();
              if( dw == WAIT_FAILED ) // not just a timeout, but a real problem with WaitForSingleObject
               {  dw = GetLastError(); // is GetLastError aware of the calling thread ? mystery..
                  dw = dw;
               }
              m_ErrorCodeOut = SOUNDOUT_ERR_TIMEOUT;
              OutClose();
              DOBINI();
              return -1;   // ERROR from OutFlushBuffer() : "failed to wait"
            }
           else
            { // waited successfully... (?)

              // How long did WaitForSingleObject() REALLY wait ?
              QueryPerformanceCounter( (LARGE_INTEGER*)&t2 );
              t2 -= t1;
              QueryPerformanceFrequency( (LARGE_INTEGER*)&t1 ); // -> frequency in Hertz
              if( t1>0 )
               { iWaited_ms = (1000 * t2) / t1/*Hz*/;
               }
              else
               { iWaited_ms = 0;
               }
              iWaited_for_single_object = iWaited_ms; // for diagnostics, further below

              // After 'waiting successfully', the buffer should NOT be full now anymore .
              //  Check again if there really is an empty buffer entry now :
              i = GetNumberOfFreeOutputBuffers(); // -> 0 ... m_NumOutputBuffers MINUS ONE (!)
              if( i<=0 )  // still no joy... try something different (a KLUDGE, "second chance") :
               { i=i; // <<< set a breakpoint here !
                 // Since WaitForSingleObject() played a dirty trick, try something else :
                 while( iWaited_ms < iTimeout_ms )
                  { Sleep(20);
                    iWaited_ms += 20;
                    i = GetNumberOfFreeOutputBuffers();  // hope WaveOutCallback() was called during Sleep() ...
                    if( i>0 )                            // obviously YES, because the buffer is free now !
                     { break;  // "success on the second attempt", using Sleep() instead of WaitForSingleObject() !
                       // 2013-02-17 : This kludge seemed to work !
                     }
                  }
                 // 2013-02-17 : Got here with i=1 (i.e. "late success") quite often. WHY ?
                 //              total time spent waiting  = 220 ms ( iWaited_ms )
                 //              iWaited_for_single_object =   0 ms ( why ? )
                 // Calling 'ResetEvent' before 'waveOutWrite' cured this .
                 // But this "second chance" was left in the code, for what it's worth. "Defensive programming" :)
                 if( i<=0 )    // completely out of luck ?
                  { LOCAL_BREAK();
                    return -1; // no success waiting for a free output-buffer !
                  }
               } // end if < NO EMPTY BUFFER AFTER WAITING ? >

              // remove certain old errors:
              DOBINI();
              if(m_ErrorCodeOut == SOUNDOUT_ERR_UNDERFLOW)
               { m_ErrorCodeOut = NO_ERRORS;
               }
            } // end if < waited SUCCESSFULLY >
         }
        else // iTimeout_ms<=0
         { // ALL output-buffers are full, but the caller does NOT want to wait for output..
           // .. most likely, because Spectrum Lab has already been waiting for INPUT.
           return -1;  // let the caller know he cannot append more samples now !
         } // end else( iTimeout_ms > 0 )
      } // end if( must_wait )

     // Arrived here: There's at least one more buffer which may be filled,
     //        and there's one more buffer which can be queued for output.
     //        (the latter because m_OutBufPosition was >= m_OutBufferSize)
     //
     // Regardless of HOW MANY buffers are already filled,
     //   START PLAYING this part of the buffer by calling waveOutWrite() .
     //   At this point, there's at least one full buffer,
     //   and waveOutWrite should be called even if there are
     //   are other buffers already queued up for output .
     // Note:  m_iOutHeadIndex is incremented in OutFlushBuffer();
     //        m_iOutTailIndex is incremented in WaveOutCallback() !
#   if( SWI_AUDIO_IO_SUPPORTED )
     if( aio_Out.h_AudioIoDLL != NULL )
      { // send the audio stream to an AUDIO-I/O-LIBRARY (DLL);
        // using the audio-IO-DLL client (host) from C:\cbproj\AudioIO\AudioIO.c .
        // In this case, Sound.cpp calls AIO_Host_WriteOutputSamplePoints()
        // somewhere else, and does NOT perform any buffering of its own !
        // return AIO_Host_WriteOutputSamplePoints( .. ) ;
      }
#   endif // SWI_AUDIO_IO_SUPPORTED
     if( m_hwvout != NULL ) // use the standard windoze multimedia API
      { DOBINI();
        if( iTimeout_ms>0 )
         { m_OutWaiting = TRUE; // here set in OutFlushBuffer(), cleared in WaveOutCallback()
         }
        ResetEvent( m_OutEventHandle );  // added 2013-02-17: convince WaitForSingleObject() to REALLY wait
        dwFlags3 = m_OutputWaveHdr[m_iOutHeadIndex].dwFlags;  // initially 2 = only WHDR_PREPARED, not DONE; later 3=PREPARED+DONE
        result = waveOutWrite( m_hwvout,&m_OutputWaveHdr[m_iOutHeadIndex], sizeof (WAVEHDR)/*!!!*/ );
        dwFlags4 = m_OutputWaveHdr[m_iOutHeadIndex].dwFlags;  // usually 18 = WHDR_PREPARED(2) + WHDR_INQUEUE(16)
        // > The waveOutWrite function sends a data block to the given waveform-audio output device.
        // > Returns MMSYSERR_NOERROR if successful or an error otherwise (..)
        // > When the buffer is finished, the WHDR_DONE bit is set
        // > in the dwFlags member of the WAVEHDR structure.
        //    ( in fact, multiple blocks should be queued up for output,
        //      to minimize the risk of audio drop-outs.
        //      The waveOut-API calls us back (WaveOutCallback) at an UNPREDICTABLE time,
        //      and WaveOutCallback will increment m_iOutTailIndex also at an UNPREDICTABLE time.
        //      Thus m_iOutTailIndex may ONLY be set in the callback;
        //      outside the callback it is 'read-only' ! )
        //
        if( (dwFlags3!=2 && dwFlags3!=3) || (dwFlags4!=18) )
         { LOCAL_BREAK(); // << set breakpoint HERE (or in "LocalBreakpoint" )
           // 2013-02-17 : Got here with dwFlags3=18, which means 'ALREADY IN QUEUE' (application error) .
           //       dwFlags1= ,  dwFlags2= ,  dwFlags3= ,   dwFlags4=   ,
           //       m_iOutHeadIndex=2 ,  new_head_index = 3, m_iOutTailIndex = 2 ,
           //       result = 33 = 'still playing' .
           //
         }
        DOBINI();
        if( result != MMSYSERR_NOERROR/*0*/ ) // oops.. what's wrong with waveOutWrite() ?
         { LOCAL_BREAK(); // << set breakpoint HERE (or in "LocalBreakpoint" )
           m_ErrorCodeOut = result;
           // 2013-02-10: Occasionally got here with result=33 . WTF .. ?!
           // > error code 33 = ERROR_LOCK_VIOLATION
           // > The process cannot access the file because another process
           // > has locked a portion of the file.
           // After that happened: dwFlags1 = dwFlags2 = 18
           //                = WHDR_INQUEUE (16) + WHDR_PREPARED (2) ! !
           // ( constants found in C:\Programme\CBuilder6\Include\mmsystem.h )
           // Not good ! Sounds like m_iOutHeadIndex was incremented
           // too early, so that m_OutputWaveHdr[m_iOutHeadIndex] was STILL IN USE !
           //
           //
           iWaited_ms = -1;  // return value for 'no succcess'
         }
        else // waveOutWrite successfull ...
         {
           // Time to switch to the next buffer for 'filling' (left side of the ASCII graphics).
           new_head_index = (m_iOutHeadIndex+1) % m_NumOutputBuffers;
           m_iOutHeadIndex = new_head_index; // set the new (incremented) head index; here in OutFlushBuffer() AND NOWHERE ELSE
           // Note: Since the time when WaveOutCallback() is called is UNPREDICTABLE,
           //       WaveOutCallback() must only increment m_iOutTailIndex,
           //       but it must not look at m_iOutHeadIndex because the latter
           //       may, or may not, be incremented at that time .
         }

        if( m_fOutputHoldoff )   // holdoff logic doesn't start sound
         {                       // until half the buffers are full
           if( m_iOutHeadIndex >= m_NumOutputBuffers/2 )
            {
              m_fOutputHoldoff = FALSE;
              result = waveOutRestart( m_hwvout ); // buffer full enough, start output
              // > The waveOutRestart function resumes playback
              // > on a paused waveform-audio output device.
              // ("Restart" doesn't mean "re-start from the beginning" here)
            }
         }
      } // end if < use 'wave out' API >
#   if( SWI_ASIO_SUPPORTED )
     else // m_hwvout==NULL  ->  send it to ASIO driver instead ?
      {
        m_OutputWaveHdr[m_iOutHeadIndex].dwBytesRecorded = 0; // for ASIO callback !
        // here: 'dwBytesRecorded' actually means "number of bytes PLAYED" from this buffer.
        // The ASIO callback function will soon notice that
        // m_iOutHeadIndex is NOT equal to m_iOutTailIndex ,
        // and increment m_iOutTailIndex when done(!) . See CSound_AsioCallback() .
      }
#    endif //  SWI_ASIO_SUPPORTED

   } // end if( m_OutBufPosition >= m_OutBufferSize ) -> "ready to flush"

  return iWaited_ms;  // the return value may be NEGATIVE when non-blocking, and all buffers are in use !
} // end CSound::OutFlushBuffer()

//---------------------------------------------------------------------
int CSound::OutWriteInt16(  // OUTDATED !  Not used by Spectrum Lab but the "WOLF GUI"..
      SHORT *pi16Data,      // [in] points to 1st sample or SAMPLE PAIR
      int Length,           // [in] number of SAMPLING POINTS or PAIRS(!) to write
      int iTimeout_ms,      // [in] max timeout in milliseconds, 0=non-blocking
      int *piHaveWaited_ms) // [out,optional] "have been waiting here for XX milliseconds"
  // Writes 'Length' audio samples of type 'SHORT' = 16-bit integer
  // to the soundcard output.
  // If the soundcard runs in stereo mode,
  //     [Length] PAIRS(!!) are read in the caller's buffer,
  //         pi16Data[0]=left channel,  pi16Data[1]=right channel,
  //      .. pi16Data[2*Length-2]=left, pi16Data[2*Length-1]=right channel .
  //
  // Returns:
  //  'Length' if data was succesfully placed in the output buffers.
  //         0 if output has finished( reached the specified sample limit )
  //        -1 if error ( use GetError() to retrieve error )
  // CAUTION: <Length> must be an integer fraction of <BufSize>
  //          passed to OutOpen() !
{
 int i,result;
 BYTE *pb;
 int  iWaited_ms = 0;

 union uBSHORT{    // byte-adressable 16-bit integer
      BYTE b[2];
      short i16;
      WORD  u16;
   }bsTemp;

  if( piHaveWaited_ms != NULL ) // [out,optional] "have been waiting here for XX milliseconds"
   { *piHaveWaited_ms = 0;
   }

  if( !m_OutputOpen )      // return -1 if output not running.
   {
     m_ErrorCodeOut = SOUNDOUT_ERR_NOTOPEN;
     return -1;
   }

  DOBINI();
  if( Length == 0 )   // need to flush partially filled buffer and exit
   {
     OutClose();
     DOBINI();
     if(m_ErrorCodeOut == NO_ERRORS)
        return Length;
     else
        return -1;
   }
  else   // here to add new data to soundcard buffer queue
   { DOBINI();

     if( m_OutBufPosition >= m_OutBufferSize )  // flush buffer if already full
      { result = OutFlushBuffer(iTimeout_ms);  // (due to arbitrary blocksize, this may happen "anytime" !)
        if( result>=0 ) // successfully flushed, or empty output buffers available -> fill next buffer
         { iWaited_ms +=  result;
         }
        else // Negative return value from OutFlushBuffer() -> ERROR !
         { return -1; // all output buffers are still completely filled, cannot add more samples !
         }
        DOBINI();
      }
     pb = m_pbOutputBuffer + m_iOutHeadIndex*m_OutBufferSize;  // local pointer for speed
     i = Length * m_OutFormat.nChannels;
     if(m_OutFormat.wBitsPerSample == 16)
      {
        while(i--)
         {
           bsTemp.i16 = (SHORT)*pi16Data++;
           *pb++ = bsTemp.b[0]; // lsb
           *pb++ = bsTemp.b[1]; // msb
           m_OutBufPosition += 2;
           if( m_OutBufPosition >= m_OutBufferSize )  // send it if full
            { result = OutFlushBuffer(iTimeout_ms);  // (due to arbitrary blocksize, this may happen "anytime" !)
              if( result>=0 ) // successfully flushed, or empty output buffers available -> fill next buffer
               { iWaited_ms +=  result;
               }
              else // Negative return value from OutFlushBuffer() -> ERROR !
               { return -1; // all output buffers are still completely filled, cannot add more samples !
               }
              DOBINI();
              pb = m_pbOutputBuffer + m_iOutHeadIndex*m_OutBufferSize;  // new output block, new local pointer !
            }
         }
      }
     else  // not 16 bits per sample
      {    //  (must 8 bit, or what ?)
        while(i--)
         {
           bsTemp.u16 = (WORD)( (*pi16Data++) + 32767 );
           *pb++ = bsTemp.b[1];  // msb only
           ++m_OutBufPosition;
           if( m_OutBufPosition >= m_OutBufferSize )  // send it if full
            { result = OutFlushBuffer(iTimeout_ms);  // (due to arbitrary blocksize, this may happen "anytime" !)
              if( result>=0 ) // successfully flushed, or empty output buffers available -> fill next buffer
               { iWaited_ms +=  result;
               }
              else // Negative return value from OutFlushBuffer() -> ERROR !
               { return -1; // all output buffers are still completely filled, cannot add more samples !
               }
              DOBINI();
              pb = m_pbOutputBuffer + m_iOutHeadIndex*m_OutBufferSize;  // new output block, new local pointer !
            }
         }
      } // end else <not 16 bits per sample>

     // arrived here: audio samples have been written into the output buffer..
     m_OutSamplesWritten += Length;  // number of "pairs" (not multiplied with the number of channels in each pair)
     if( piHaveWaited_ms != NULL ) // [out,optional] "have been waiting here for XX milliseconds"
      { *piHaveWaited_ms = iWaited_ms;
      }

     // only if no ENDLESS playing:
     if( (m_OutSampleLimit != 0) && (m_OutSamplesWritten >= m_OutSampleLimit) )
      {  // reached the number of samples to be played. Close output.
         OutClose();
         if(m_ErrorCodeOut == NO_ERRORS )
            return 0;
         else
            return -1;
      }
     return Length;   // return number Samples accepted OK
   }
}  // end OutWriteInt16()

//---------------------------------------------------------------------
int CSound::OutWriteFloat(  // Not used by Spectrum Lab !
        T_Float* pFltSamplePoints, // [in] audio samples, single precision float
        int iNumberOfSamplePairs, // [in] number of SAMPLE POINTS(!) to write or send
        int iTimeout_ms,          // [in] max timeout in milliseconds, 0=non-blocking
        int *piHaveWaited_ms,     // [out,optional] "have been waiting here for XX milliseconds"
        T_ChunkInfo *pInChunkInfo) // [in,optional] chunk info with scaling info;
                                   //       see c:\cbproj\SoundUtl\ChunkInfo.h
  // Writes 'Length' audio samples (from a floating-point array)
  // to the soundcard output.
  //    parameters:
  //      pLeftData = pointer to block of 'Length' float's to output.
  //      Length   = Number of sample-points ("pairs") to write to the output.
  //         If the soundcard runs in stereo mode,
  //             2 * iNumberOfSamplePairs "single floating point numbers"
  //            are taken from the caller's buffer :
  //         pFltSamplePoints[0]=left channel,  pFltSamplePoints[1]=right channel,
  //      .. pFltSamplePoints[2*Length-2]=left, pFltSamplePoints[2*Length-1]=right channel .
  //      If Length is zero then the sound output is flushed and closed .
  //
  // Returns:
  //  'Length' if data was succesfully placed in the output buffers.
  //         0 if output has finished( reached the specified sample limit )
  //        -1 if error ( use GetError() to retrieve error )
{
 int i,nSingleValues;
 int result;
 BYTE *pb;
 T_Float fltTemp, fltScalingFactor = 1.0;
 int iWaited_ms = 0;


 union uBSHORT  // byte-adressable 16-bit integer
   { BYTE b[2];
     short i16;
     WORD  u16;
   } bsTemp;
 union uBLONG   // byte-adressable 32-bit-integer
   { BYTE  b[4];
     long  i32;
   } blTemp;

  if( pInChunkInfo != NULL )  // if the caller provides the value range, use it
   { if( pInChunkInfo->dblSampleValueRange > 0.0 )
      { fltScalingFactor = 32767.0 / pInChunkInfo->dblSampleValueRange;
      }
   }

   if( piHaveWaited_ms != NULL ) // [out,optional] "have been waiting here for XX milliseconds"
    { *piHaveWaited_ms = 0;
    }
   if( !m_OutputOpen )      // return -1 if output not running.
    {
      m_ErrorCodeOut = SOUNDOUT_ERR_NOTOPEN;
      return -1;
    }

   if( iNumberOfSamplePairs <= 0 )   // need to flush partially filled buffer and exit
    {
      OutClose();
      if(m_ErrorCodeOut == NO_ERRORS)
         return 0;
      else
         return -1;
    }
   else   // here to add new data to soundcard buffer queue
    {
     DOBINI();
     pb = m_pbOutputBuffer + m_iOutHeadIndex*m_OutBufferSize;  // local pointer for speed
     nSingleValues = iNumberOfSamplePairs * m_OutFormat.nChannels;
     if(m_OutFormat.wBitsPerSample == 16)
      {
       for( i=0; i < nSingleValues; i++ )
        {
            fltTemp = *pFltSamplePoints++ * fltScalingFactor;
            if(fltTemp>32767.0)  fltTemp=32767.0;
            if(fltTemp<-32768.0) fltTemp=-32768.0;
            bsTemp.i16 = (SHORT)fltTemp; // typecast because Dev-C++ complained..
            pb[m_OutBufPosition++] = bsTemp.b[0];
            pb[m_OutBufPosition++] = bsTemp.b[1];
            if( m_OutBufPosition >= m_OutBufferSize )  // send it if full
             { result = OutFlushBuffer(iTimeout_ms);  // (due to arbitrary blocksize, this may happen "anytime" !)
               if( result>=0 ) // successfully flushed, or empty output buffers available -> fill next buffer
                { iWaited_ms +=  result;
                }
               else // Negative return value from OutFlushBuffer() -> ERROR !
                { return -1; // all output buffers are still completely filled, cannot add more samples !
                }
               DOBINI();
               pb = m_pbOutputBuffer + m_iOutHeadIndex*m_OutBufferSize;  // new output block, new local pointer !
             }
        } // end for <all 16-bit output samples>
      }
     else  // not 16 bits per sample
     if(m_OutFormat.wBitsPerSample == 8)
      {
        for( i=0; i < nSingleValues; i++ )
         {
            fltTemp = *pFltSamplePoints++;
            if(fltTemp>32767.0)  fltTemp=32767.0;
            if(fltTemp<-32768.0) fltTemp=-32768.0;
            bsTemp.i16 = (SHORT)(fltTemp + 32767);
            pb[m_OutBufPosition++]=bsTemp.b[1];
            if( m_OutBufPosition >= m_OutBufferSize )  // send it if full
             { result = OutFlushBuffer(iTimeout_ms);  // (due to arbitrary blocksize, this may happen "anytime" !)
               if( result>=0 ) // successfully flushed, or empty output buffers available -> fill next buffer
                { iWaited_ms +=  result;
                }
               else // Negative return value from OutFlushBuffer() -> ERROR !
                { return -1; // all output buffers are still completely filled, cannot add more samples !
                }
               DOBINI();
               pb = m_pbOutputBuffer + m_iOutHeadIndex*m_OutBufferSize;
             }
         }
      } // end if < 8 bits per sample>
     else
     if(m_OutFormat.wBitsPerSample == 24)
      {
        // 24 bits per sample.
        // Note that the input samples are FLOATING POINT NUMBERS,
        //  but still scaled to +/- 32k for historic reasons !
        blTemp.i32 = 0;
        for( i=0; i < nSingleValues; i++ )
         {
            fltTemp = *pFltSamplePoints++;
            if(fltTemp>32767.0)  fltTemp=32767.0;
            if(fltTemp<-32768.0) fltTemp=-32768.0;
            blTemp.i32 = (long)( fltTemp * 256.0 );
            pb[m_OutBufPosition++] = blTemp.b[0];
            pb[m_OutBufPosition++] = blTemp.b[1];
            pb[m_OutBufPosition++] = blTemp.b[2];
            if( m_OutBufPosition >= m_OutBufferSize )  // send it if full
             { result = OutFlushBuffer(iTimeout_ms);  // (due to arbitrary blocksize, this may happen "anytime" !)
               if( result>=0 ) // successfully flushed, or empty output buffers available -> fill next buffer
                { iWaited_ms +=  result;
                }
               else // Negative return value from OutFlushBuffer() -> ERROR !
                { return -1; // all output buffers are still completely filled, cannot add more samples !
                }
               DOBINI();
               pb = m_pbOutputBuffer + m_iOutHeadIndex*m_OutBufferSize;  // new output block, new local pointer !
             }
         } // end for <all 24-bit output samples>
      } // end if < 24 bits per sample >
     else
     if(m_OutFormat.wBitsPerSample == 32)
      {
        // 32 bits per sample.
        // Note that the input samples are FLOATING POINT NUMBERS,
        //  but still scaled to +/- 32k for historic reasons !
        blTemp.i32 = 0;
        pb += m_OutBufPosition;  // added 2007-07-18
        for( i=0; i < nSingleValues; i++ )
          {
            fltTemp = *pFltSamplePoints++;
            if(fltTemp>32767.0)  fltTemp=32767.0;
            if(fltTemp<-32768.0) fltTemp=-32768.0;
            *((long*)pb) = (long)( fltTemp * 65536.0 );  // -> +/- 2^31
            pb+=4;  m_OutBufPosition+=4;
            if( m_OutBufPosition >= m_OutBufferSize )  // send it if full
             { result = OutFlushBuffer(iTimeout_ms);  // (due to arbitrary blocksize, this may happen "anytime" !)
               if( result>=0 ) // successfully flushed, or empty output buffers available -> fill next buffer
                { iWaited_ms +=  result;
                }
               else // Negative return value from OutFlushBuffer() -> ERROR !
                { return -1; // all output buffers are still completely filled, cannot add more samples !
                }
               DOBINI();
               pb = m_pbOutputBuffer + m_iOutHeadIndex*m_OutBufferSize;
             }
          } // end for <all 32-bit output samples>
      } // end if < 32 bits per sample >

     DOBINI();
     m_OutSamplesWritten += iNumberOfSamplePairs; // Length; ?
     if( piHaveWaited_ms != NULL ) // [out,optional] "have been waiting here for XX milliseconds"
      { *piHaveWaited_ms = iWaited_ms;
      }

     // only if no ENDLESS playing:
     if( (m_OutSampleLimit != 0) && (m_OutSamplesWritten >= m_OutSampleLimit) )
      { // reached the number of samples to be played. Close output.
        OutClose();
        DOBINI();
        if(m_ErrorCodeOut == NO_ERRORS )
           return 0;
        else
           return -1;
      }
     DOBINI();
     return iNumberOfSamplePairs;   // return number of sample-pairs accepted
   } // end if( iNumberOfSamplePairs > 0 )

}  // end OutWriteFloat()



//---------------------------------------------------------------------
int CSound::OutWriteStereo(  // Sends audio samples to the OUTPUT. Used by SpecLab.
        T_Float *pLeftData,   // [in] audio samples, 32-bit FLOATING POINT, range +/- 1.0, left channel or "I"-component
        T_Float *pRightData,  // [in] audio samples, right channel or "Q"-component
        int iNumberOfSamples, // [in] number of samples in each of the above blocks
        int iTimeout_ms,      // [in] max timeout in milliseconds, 0=non-blocking
        int *piHaveWaited_ms, // [out,optional] "have been waiting here for XX milliseconds"
      T_ChunkInfo *pInChunkInfo) // [in,optional] chunk info, see c:\cbproj\SoundUtl\ChunkInfo.h
  // Used by Spectrum Lab.  Will be outdated in future versions with MORE THAN TWO CHANNELS !
  // Writes 'Length' audio samples (from one or two floating-point arrays)
  // to the soundcard output.   This is the only function called from SpecLab.
  //    parameters:
  //      pLeftData, pRightData = pointer to block of 'Length' float's to output.
  //      iNumberOfSamples   = Number of samples to write from each pXXData-Block.
  //               If is zero then the sound output is flushed and closed.
  //      iTimeout_ms = maximum number of milliseconds to 'wait' here.
  //                    0 for 'non-blocking' (which is used by Spectrum Lab
  //                    if the audio-processing thread already waited
  //                    when READING samples from a soundcard.
  //                    See  C:\cbproj\SpecLab\SoundThd.cpp  .
  // Returns:
  //  'iNumberOfSamples' if data was succesfully placed in the output buffers.
  //         0 if output has finished( reached the specified sample limit )
  //        -1 if error ( use GetError() to retrieve error )
  //
  // Caller (in Spectrum Lab) :  TSoundThread::Execute() -> CSound::OutWriteStereo() .
  //
{
 int i, result, nChannelsPerSample;
 BYTE *pb;
 T_Float fltTemp, fltFactor, *pfltInterleaveBuffer, *pfltSource;
 int iWaited_ms = 0;


 union uBSHORT  // byte-adressable 16-bit integer
   { BYTE b[2];
     short i16;
     WORD  u16;
   } bsTemp;
 union uBLONG   // byte-adressable 32-bit-integer
   { BYTE  b[4];
     long  i32;
   } blTemp;

   if( piHaveWaited_ms != NULL ) // [out,optional] "have been waiting here for XX milliseconds"
    { *piHaveWaited_ms = 0; // set this to ZERO if we didn't wait (further below)
    }
   if( !m_OutputOpen )      // return -1 if output not running.
    {
      m_ErrorCodeOut = SOUNDOUT_ERR_NOTOPEN;
      return -1;
    }

   if( iNumberOfSamples == 0 )   // need to flush partially filled buffer and exit
    {
      OutClose();
      if(m_ErrorCodeOut == NO_ERRORS)
         return iNumberOfSamples;
      else
         return -1;
    }
   else   // here to add new data to soundcard buffer queue
    {
#  if( SWI_AUDIO_IO_SUPPORTED )
      if( aio_Out.h_AudioIoDLL != NULL )
       { // send the audio stream to an AUDIO-I/O-LIBRARY (DLL) :
         pfltSource = pLeftData;
         nChannelsPerSample = 1;
         // Because AIO_Host_WriteOutputSamplePoints() expects
         // interleaved channels (only ONE source block, with N channels per point)
         // the separated (non-interleaved) source channels may have to be
         // interleaved here:
         if( pRightData != NULL ) // multiple input channels in separate blocks ?
          { if( (pfltInterleaveBuffer = ResizeInterleaveBuffer( iNumberOfSamples )) != NULL )
             { Sound_InterleaveTwoBlocks( pLeftData, pRightData, iNumberOfSamples, pfltInterleaveBuffer );
               pfltSource = pfltInterleaveBuffer;
               nChannelsPerSample = 2;
             }
          }
         i = AIO_Host_WriteOutputSamplePoints(  // -> C:\cbproj\AudioIO\AudioIO.c
              &aio_Out,          // [in] DLL-host instance data
              pfltSource,        // [in] audio samples, as 32-bit FLOATING POINT numbers, interleaved as "sample points"
              iNumberOfSamples,  // [in] number of SAMPLE POINTS(!) to write or send
              nChannelsPerSample,// [in] number of samples PER SAMPLE POINT
              iTimeout_ms ,      // [in] max timeout in milliseconds, 0=non-blocking
              pInChunkInfo,      // [in,optional] chunk info, see c:\cbproj\SoundUtl\ChunkInfo.h, MAY BE NULL !
              piHaveWaited_ms);  // [out,optional] "have been waiting here for XX milliseconds"
         if( i>0 )   // SUCCESS (and the number of samples still unused in the recipient's buffer)
          {
            return iNumberOfSamples;  // return number Samples successfully accepted
          }
         else        // i<0 : AIO_Host_WriteOutputSamplePoints() failed,
          {          //       and 'i' is actually one of the error codes defined in AudioIO.h
            strncpy( m_sz255LastErrorStringOut, AIO_Host_ErrorCodeToString( &aio_Out, i ), 80 );
            m_ErrorCodeOut = Sound_AudioIOErrorToMyErrorCode( i );
            return -1;
          }
       }
#  endif // SWI_AUDIO_IO_SUPPORTED ?
     // Arrived here: use the standard 'multimedia API' for audio output .
     if( (m_pbOutputBuffer==0) || (m_NumOutputBuffers<=0) || (!m_OutputOpen) )
      { return 0;   // something very wrong; bail out !
      }

     // If the caller does NOT want to be blocked, make sure the output-buffer
     // is empty enough to accept the caller's number of samples (to be written):
     if( iTimeout_ms <= 0 )  // Caller does NOT want to be blocked at all :
      { // Bail out (with an error) if the output-buffer is completely full:
        if( ( (m_iOutHeadIndex+1) % m_NumOutputBuffers) == m_iOutTailIndex )
         {  // ALL of the output buffers are completely filled ->
            return 0; // bail out (caller needs to be blocked but he doesn't want to..)
            // 2013-02-09 : Occasionally got here, when the sampling rate
            //              was deliberately increased 'too much' for the output.
            //       But dropping a few output chunks is much better (faster)
            //       then periodically closing and re-opening the audio device !
         }
      } // end if( iTimeout_ms <= 0 )
     pb = m_pbOutputBuffer + m_iOutHeadIndex*m_OutBufferSize;  // local pointer for speed
     if(m_OutFormat.wBitsPerSample == 16)
      {
        fltFactor = 1.0;  // scaling factor IF the input was scaled from -32767..+32767
        if( pInChunkInfo != NULL )
         { if( pInChunkInfo->dblSampleValueRange > 0.0 )
            { // Originally the sample value range was +/-32767.0 ,
              // but since 2011-07, the preferred(!) range is +/- 1.0 ,
              // i.e. the sample blocks use floating point values normalized to -1.0 ... +1.0 .
              // Caution, some older modules may still use +/- 32k value range !
              // The T_ChunkInfo struct from c:\cbproj\SoundUtl\ChunkInfo.h
              // contains the scaling range :
              fltFactor = 32767.0 / pInChunkInfo->dblSampleValueRange;
            }
         }

       if(m_OutFormat.nChannels==1) // 16 bit, Mono output
        {
         for( i=0; i < iNumberOfSamples; i++ )
          {
            fltTemp = pLeftData[i] * fltFactor;
                 // 2009-08-06: access violation HERE,
                 // after switching the output device from the "Audio-I/O DLL"
                 // to a normal soundcard.
            if(fltTemp>32767.0)  fltTemp=32767.0;
            if(fltTemp<-32768.0) fltTemp=-32768.0;
            bsTemp.i16 = (SHORT)fltTemp; // typecast because Dev-C++ complained..
            pb[m_OutBufPosition++] = bsTemp.b[0];
            pb[m_OutBufPosition++] = bsTemp.b[1];
            if( m_OutBufPosition >= m_OutBufferSize )  // send it if full
             { result = OutFlushBuffer(iTimeout_ms);  // (due to arbitrary blocksize, this may happen "anytime" !)
               if( result>=0 ) // successfully flushed, or empty output buffers available -> fill next buffer
                { iWaited_ms +=  result;
                }
               else // Negative return value from OutFlushBuffer() -> ERROR !
                { return -1; // all output buffers are still completely filled, cannot add more samples !
                }
               DOBINI();
               pb = m_pbOutputBuffer + m_iOutHeadIndex*m_OutBufferSize;  // new output block, new local pointer !
             }
          } // end for <all 16-bit MONO output samples>
        }
       else
       if(m_OutFormat.nChannels==2) // 16 bit, Stereo output
        {
         for( i=0; i < iNumberOfSamples; i++ )
          {
            fltTemp = pLeftData[i] * fltFactor;
            // 2012-03-26 : Crashed here when changing audio params on-the-fly ?
            // 2012-11-18 : Crashed here when switching the configuration
            //              while acquiring samples from an SDR-IQ .
            if(fltTemp>32767.0)  fltTemp=32767.0;
            if(fltTemp<-32768.0) fltTemp=-32768.0;
            bsTemp.i16 = (SHORT)fltTemp;
            pb[m_OutBufPosition++] = bsTemp.b[0];
            pb[m_OutBufPosition++] = bsTemp.b[1];
            if( pRightData )
             { fltTemp = pRightData[i] * fltFactor;
               if(fltTemp>32767.0)  fltTemp=32767.0;
               if(fltTemp<-32768.0) fltTemp=-32768.0;
             }
            bsTemp.i16 = (SHORT)fltTemp;
            pb[m_OutBufPosition++] = bsTemp.b[0];
            pb[m_OutBufPosition++] = bsTemp.b[1];
            if( m_OutBufPosition >= m_OutBufferSize )  // send it if full
             { result = OutFlushBuffer(iTimeout_ms);  // (due to arbitrary blocksize, this may happen "anytime" !)
               if( result>=0 ) // successfully flushed, or empty output buffers available -> fill next buffer
                { iWaited_ms +=  result;
                }
               else // Negative return value from OutFlushBuffer() ->
                { return -1; // all output buffers are still completely filled, cannot add more samples !
                }
               DOBINI();
               pb = m_pbOutputBuffer + m_iOutHeadIndex*m_OutBufferSize;  // new output block, new local pointer !
             }
          } // end for <all 16-bit STEREO output samples>
         DOBINI();
        }
      }
     else  // not 16 bits per sample
     if(m_OutFormat.wBitsPerSample == 8)
      {
        fltFactor = 1.0;  // scaling factor IF the input was scaled from -32767..+32767
        if( pInChunkInfo != NULL )
         { if( pInChunkInfo->dblSampleValueRange > 0.0 )
            { // Originally the sample value range was +/-32767.0 ,
              // but since 2011-07, the preferred(!) range is +/- 1.0 ,
              // i.e. the sample blocks use floating point values normalized to -1.0 ... +1.0 .
              // Caution, some older modules may still use +/- 32k value range !
              // The T_ChunkInfo struct from c:\cbproj\SoundUtl\ChunkInfo.h
              // contains the scaling range :
              fltFactor = 32767.0 / pInChunkInfo->dblSampleValueRange;
            }
         }

       if(m_OutFormat.nChannels==1) // 8 bit, Mono output
        {
         for( i=0; i < iNumberOfSamples; i++ )
           {
            fltTemp = pLeftData[i] * fltFactor;
            if(fltTemp>32767.0)  fltTemp=32767.0;
            if(fltTemp<-32768.0) fltTemp=-32768.0;
            bsTemp.i16 = (SHORT)(fltTemp + 32767);
            pb[m_OutBufPosition++]=bsTemp.b[1];
            if( m_OutBufPosition >= m_OutBufferSize )  // send it if full
             { result = OutFlushBuffer(iTimeout_ms);  // (due to arbitrary blocksize, this may happen "anytime" !)
               if( result>=0 ) // successfully flushed, or empty output buffers available -> fill next buffer
                { iWaited_ms +=  result;
                }
               else // Negative return value from OutFlushBuffer() -> ERROR !
                { return -1; // all output buffers are still completely filled, cannot add more samples !
                }
               DOBINI();
               pb = m_pbOutputBuffer + m_iOutHeadIndex*m_OutBufferSize;
             }
          }
        }
       else
       if(m_OutFormat.nChannels==2) // 8 bit, Stereo output
        {
         for( i=0; i < iNumberOfSamples; i++ )
           {
            fltTemp = pLeftData[i] * fltFactor;
            if(fltTemp>32767.0)  fltTemp=32767.0;
            if(fltTemp<-32768.0) fltTemp=-32768.0;
            bsTemp.i16 = (SHORT)(fltTemp + 32767);
            pb[m_OutBufPosition++]=bsTemp.b[1];

            if( pRightData )
             { fltTemp = pRightData[i] * fltFactor;
               if(fltTemp>32767.0)  fltTemp=32767.0;
               if(fltTemp<-32768.0) fltTemp=-32768.0;
               bsTemp.i16 = (SHORT)(fltTemp + 32767);
             }
            pb[m_OutBufPosition++]=bsTemp.b[1];
            if( m_OutBufPosition >= m_OutBufferSize )  // send it if full
             { result = OutFlushBuffer(iTimeout_ms);  // (due to arbitrary blocksize, this may happen "anytime" !)
               if( result>=0 ) // successfully flushed, or empty output buffers available -> fill next buffer
                { iWaited_ms +=  result;
                }
               else // Negative return value from OutFlushBuffer() -> ERROR !
                { return -1; // all output buffers are still completely filled, cannot add more samples !
                }
               DOBINI();
               pb = m_pbOutputBuffer + m_iOutHeadIndex*m_OutBufferSize;  // new output block, new local pointer !
             }
          }
        }
      } // end if < 8 bits per sample>
     else
     if(m_OutFormat.wBitsPerSample == 24)
      {
       // 24 bits per sample.
       // Note that the input samples are FLOATING POINT NUMBERS,
       //  but -until 2011- still scaled to +/- 32k !
       fltFactor = 1.0;  // scaling factor IF the input was scaled from -32767..+32767
       if( pInChunkInfo != NULL )
        { if( pInChunkInfo->dblSampleValueRange > 0.0 )
           { fltFactor = 32767.0 / pInChunkInfo->dblSampleValueRange;
           }
        }

       blTemp.i32 = 0;
       if(m_OutFormat.nChannels==1) // 24 bit, Mono output
        {
         for( i=0; i < iNumberOfSamples; i++ )
          {
            fltTemp = pLeftData[i] * fltFactor;
            if(fltTemp>32767.0)  fltTemp=32767.0;
            if(fltTemp<-32768.0) fltTemp=-32768.0;
            blTemp.i32 = (long)( fltTemp * 256.0 );
            pb[m_OutBufPosition++] = blTemp.b[0];
            pb[m_OutBufPosition++] = blTemp.b[1];
            pb[m_OutBufPosition++] = blTemp.b[2];
            if( m_OutBufPosition >= m_OutBufferSize )  // send it if full
             { result = OutFlushBuffer(iTimeout_ms);  // (due to arbitrary blocksize, this may happen "anytime" !)
               if( result>=0 ) // successfully flushed, or empty output buffers available -> fill next buffer
                { iWaited_ms +=  result;
                }
               else // Negative return value from OutFlushBuffer() -> ERROR !
                { return -1; // all output buffers are still completely filled, cannot add more samples !
                }
               DOBINI();
               pb = m_pbOutputBuffer + m_iOutHeadIndex*m_OutBufferSize;  // new output block, new local pointer !
             }
          } // end for <all 16-bit MONO output samples>
        }
       else
       if(m_OutFormat.nChannels==2) // 24 bit, Stereo output
        {
         for( i=0; i < iNumberOfSamples; i++ )
          {
            fltTemp = pLeftData[i] * fltFactor;
            if(fltTemp>32767.0)  fltTemp=32767.0;
            if(fltTemp<-32768.0) fltTemp=-32768.0;
            blTemp.i32 = (long)( fltTemp * 256.0 );
            pb[m_OutBufPosition++] = blTemp.b[0];
            pb[m_OutBufPosition++] = blTemp.b[1];
            pb[m_OutBufPosition++] = blTemp.b[2];

            if( pRightData )
             { fltTemp = pRightData[i] * fltFactor;
               if(fltTemp>32767.0)  fltTemp=32767.0;
               if(fltTemp<-32768.0) fltTemp=-32768.0;
               blTemp.i32 = (long)( fltTemp * 256.0 );
             }
            pb[m_OutBufPosition++] = blTemp.b[0];
            pb[m_OutBufPosition++] = blTemp.b[1];
            pb[m_OutBufPosition++] = blTemp.b[2];
            if( m_OutBufPosition >= m_OutBufferSize )  // send it if full
             { result = OutFlushBuffer(iTimeout_ms);  // (due to arbitrary blocksize, this may happen "anytime" !)
               if( result>=0 ) // successfully flushed, or empty output buffers available -> fill next buffer
                { iWaited_ms +=  result;
                }
               else // Negative return value from OutFlushBuffer() -> ERROR !
                { return -1; // all output buffers are still completely filled, cannot add more samples !
                }
               DOBINI();
               pb = m_pbOutputBuffer + m_iOutHeadIndex*m_OutBufferSize;  // new output block, new local pointer !
             }
          } // end for <all 16-bit STEREO output samples>
        }
      } // end if < 24 bits per sample >
     else
     if(m_OutFormat.wBitsPerSample == 32)
      {
       // 32 bits per sample.
       // Note that the input samples are FLOATING POINT NUMBERS,
       //  but may still be scaled to +/- 32k for historic reasons !
       fltFactor = 1.0;  // scaling factor IF the input was scaled from -32767..+32767
       if( pInChunkInfo != NULL )
        { if( pInChunkInfo->dblSampleValueRange > 0.0 )
           { fltFactor = 32767.0 / pInChunkInfo->dblSampleValueRange;
           }
        }

       blTemp.i32 = 0;
       if(m_OutFormat.nChannels==1) // 24 bit, Mono output
        { pb += m_OutBufPosition;  // added 2007-07-18
         for( i=0; i < iNumberOfSamples; i++ )
          {
            fltTemp = pLeftData[i] * fltFactor;
            if(fltTemp>32767.0)  fltTemp=32767.0;
            if(fltTemp<-32768.0) fltTemp=-32768.0;
            *((long*)pb) = (long)( fltTemp * 65536.0 );  // -> +/- 2^31
            pb+=4;  m_OutBufPosition+=4;
            if( m_OutBufPosition >= m_OutBufferSize )  // send it if full
             { result = OutFlushBuffer(iTimeout_ms);  // (due to arbitrary blocksize, this may happen "anytime" !)
               if( result>=0 ) // successfully flushed, or empty output buffers available -> fill next buffer
                { iWaited_ms +=  result;
                }
               else // Negative return value from OutFlushBuffer() -> ERROR !
                { return -1; // all output buffers are still completely filled, cannot add more samples !
                }
               DOBINI();
               pb = m_pbOutputBuffer + m_iOutHeadIndex*m_OutBufferSize;
             }
          } // end for <all 32-bit MONO output samples>
        }
       else
       if(m_OutFormat.nChannels==2) // 32 bit, Stereo output
        { pb += m_OutBufPosition;  // added 2007-07-18
         for( i=0; i < iNumberOfSamples; i++ )
          {
            fltTemp = pLeftData[i] * fltFactor;
            if(fltTemp>32767.0)  fltTemp=32767.0;
            if(fltTemp<-32768.0) fltTemp=-32768.0;
            *((long*)pb) = (long)( fltTemp * 65536.0 );  // -> +/- 2^31
            pb+=4;  m_OutBufPosition+=4;

            if( pRightData )
             { fltTemp = pRightData[i] * fltFactor;
               if(fltTemp>32767.0)  fltTemp=32767.0;
               if(fltTemp<-32768.0) fltTemp=-32768.0;
             }
            *((long*)pb) = (long)( fltTemp * 65536.0 );
            pb+=4;  m_OutBufPosition+=4;
            if( m_OutBufPosition >= m_OutBufferSize )  // send it if full
             { result = OutFlushBuffer(iTimeout_ms);  // (due to arbitrary blocksize, this may happen "anytime" !)
               if( result>=0 ) // successfully flushed, or empty output buffers available -> fill next buffer
                { iWaited_ms +=  result;
                }
               else // Negative return value from OutFlushBuffer() -> ERROR !
                { return -1; // all output buffers are still completely filled, cannot add more samples !
                }
               DOBINI();
               pb = m_pbOutputBuffer + m_iOutHeadIndex*m_OutBufferSize;
             }
          } // end for <all 32-bit STEREO output samples>
        }
      } // end if < 32 bits per sample >

     DOBINI();
     m_OutSamplesWritten += iNumberOfSamples;
     if( piHaveWaited_ms != NULL ) // [out,optional] "have been waiting here for XX milliseconds"
      { *piHaveWaited_ms = iWaited_ms;
      }

     // only if no ENDLESS playing:
     if( (m_OutSampleLimit != 0) && (m_OutSamplesWritten >= m_OutSampleLimit) )
      { // reached the number of samples to be played. Close output.
        OutClose();
        DOBINI();
        if(m_ErrorCodeOut == NO_ERRORS )
           return 0;
        else
           return -1;
      }
     DOBINI();
     return iNumberOfSamples;   // return number Samples accepted OK
   }
}  // end OutWriteStereo()



//---------------------------------------------------------------------
int CSound::WriteOutputSamplePoints(  // NOT used by Spectrum Lab ! !
        float *pfltSource,        // [in] audio samples, as 32-bit FLOATING POINT numbers,
                                  //  grouped as "sample points" (N channels PER POINT),
                                  //  value range normalized to +/- 1.0 for full ADC range
        int iNumSamplePoints,     // [in] number of SAMPLE POINTS(!) to write or send
        int nChannelsPerSample,   // [in] number of samples PER SAMPLE POINT
        int iTimeout_ms,          // [in] max timeout in milliseconds, 0=non-blocking
        int *piHaveWaited_ms,     // [out,optional] "have been waiting here for XX milliseconds"
        T_ChunkInfo *pInChunkInfo) // [in,optional] chunk info, see c:\cbproj\SoundUtl\ChunkInfo.h, MAY BE NULL !
  // Return : >= 0 on success (the number indicates HOW MANY SAMPLES can be
  //                           placed in the buffer if we'd immediately
  //                           call WriteOutputSamplePoints() again ) .
  //          < 0  : one of the NEGATIVE error codes defined in AudioIO.H .
  // Unlike the older (and now discarded) 'audio output' methods, this one
  //  supports the 'T_ChunkInfo' struct defined in c:\cbproj\SoundUtl\ChunkInfo.h,
  //  including timestamp, calibrated sample rate, GPS date+time+position, etc.
  //  It also supports multi-channel audio (grouped with MULTIPLE CHANNELS per "sample point") .
{
 int i,result,chn;
 BYTE *pb;
 float fltFactor,fltTemp;
 long  i32Result = 0;
 int   iWaited_ms = 0;


 union uBSHORT  // byte-adressable 16-bit integer
   { BYTE b[2];
     short i16;
     WORD  u16;
   } bsTemp;
 union uBLONG   // byte-adressable 32-bit-integer
   { BYTE  b[4];
     long  i32;
   } blTemp;

   if(piHaveWaited_ms != NULL)
     *piHaveWaited_ms = 0;
   if( !m_OutputOpen )      // return -1 if output not running.
    {
      m_ErrorCodeOut = SOUNDOUT_ERR_NOTOPEN;
      return -1;
    }

   blTemp.i32 = 0;
   if( iNumSamplePoints == 0 )   // need to flush partially filled buffer and exit
    {
      OutClose();
      if(m_ErrorCodeOut == NO_ERRORS)
         return 0;
      else
         return -1;
    }
#if( SWI_AUDIO_IO_SUPPORTED )
   if( aio_Out.h_AudioIoDLL != NULL )
    { // send the audio stream to an AUDIO-I/O-LIBRARY (DLL) :
      return AIO_Host_WriteOutputSamplePoints(
              &aio_Out,          // [in] DLL-host instance data
              pfltSource,        // [in] audio samples, as 32-bit FLOATING POINT numbers, grouped as "sample points"
              iNumSamplePoints,  // [in] number of SAMPLE POINTS(!) to write or send
              nChannelsPerSample,// [in] number of samples PER SAMPLE POINT
              iTimeout_ms ,      // [in] max timeout in milliseconds, 0=non-blocking
              pInChunkInfo,      // [in,optional] chunk info, see c:\cbproj\SoundUtl\ChunkInfo.h, MAY BE NULL !
              piHaveWaited_ms);  // [out,optional] "have been waiting here for XX milliseconds"
    }
#endif // SWI_AUDIO_IO_SUPPORTED
   else   // here to add new data to *SOUNDCARD* buffer queue
    {
     pb = m_pbOutputBuffer + m_iOutHeadIndex*m_OutBufferSize;  // local pointer for speed
     if(m_OutFormat.wBitsPerSample == 16)
      {
        fltFactor = 32767.0;  // scaling factor IF the input was normalized ( +/- 1.0 )
        if( pInChunkInfo != NULL )
         { if( pInChunkInfo->dblSampleValueRange > 0.0 )
            { // Originally the sample value range was +/-32767.0 ,
              // but since 2011-07, the preferred(!) range is +/- 1.0 ,
              // i.e. the sample blocks use floating point values normalized to -1.0 ... +1.0 .
              // Caution, some older modules may still use +/- 32k value range !
              // The T_ChunkInfo struct from c:\cbproj\SoundUtl\ChunkInfo.h
              // contains the scaling range :
              fltFactor /= pInChunkInfo->dblSampleValueRange;
            }
         }
        for( i=0; i < iNumSamplePoints; i++ )
         {
           for(chn=0; chn<m_OutFormat.nChannels; ++chn)
            { if( chn<nChannelsPerSample )
               { fltTemp = pfltSource[chn];
               }
              else
               { fltTemp = 0.0;  // put "silence" into all unused channels
               }
              fltTemp *= fltFactor;        // scale input to 16-bit signed integer
              if( fltTemp < -32767.0 )
               {  fltTemp = -32767.0;
               }
              if( fltTemp >  32767.0 )
               {  fltTemp =  32767.0;
               }
              bsTemp.i16 = (SHORT)fltTemp; // typecast because Dev-C++ complained..
              pb[m_OutBufPosition++] = bsTemp.b[0];
              pb[m_OutBufPosition++] = bsTemp.b[1];
              if( m_OutBufPosition >= m_OutBufferSize )  // send it if full
               { result = OutFlushBuffer(iTimeout_ms);  // (due to arbitrary blocksize, this may happen "anytime" !)
                 if( result>=0 ) // successfully flushed, or empty output buffers available -> fill next buffer
                  { iWaited_ms +=  result;
                  }
                 else // Negative return value from OutFlushBuffer() -> ERROR !
                  { return -1; // all output buffers are still completely filled, cannot add more samples !
                  }
                 DOBINI();
                 pb = m_pbOutputBuffer + m_iOutHeadIndex*m_OutBufferSize;  // new output block, new local pointer !
               }
              pfltSource += nChannelsPerSample;
            } // end for < all CHANNELS within the current sample point >
         } // end for <all 16-bit output samples>
      }
     else  // not 16 bits per sample
     if(m_OutFormat.wBitsPerSample == 8)
      {
        fltFactor = 127.0;  // scaling factor IF the input was normalized ( +/- 1.0 )
        if( pInChunkInfo != NULL )
         { if( pInChunkInfo->dblSampleValueRange > 0.0 )
            { fltFactor /= pInChunkInfo->dblSampleValueRange;
            }
         }
        for( i=0; i < iNumSamplePoints; i++ )
         {
           for(chn=0; chn<m_OutFormat.nChannels; ++chn)
            { if( chn<nChannelsPerSample )
               { fltTemp = pfltSource[chn];
               }
              else
               { fltTemp = 0.0;  // put "silence" into all unused channels
               }
              fltTemp = 127.0 + (fltTemp*fltFactor);  // scale from +/- X  to  8-bit UNsigned integer (0..255)
              pb[m_OutBufPosition++] = (BYTE)(int)fltTemp;
              if( m_OutBufPosition >= m_OutBufferSize )  // send it if full
               { result = OutFlushBuffer(iTimeout_ms);  // (due to arbitrary blocksize, this may happen "anytime" !)
                 if( result>=0 ) // successfully flushed, or empty output buffers available -> fill next buffer
                  { iWaited_ms +=  result;
                  }
                 else // Negative return value from OutFlushBuffer() -> ERROR !
                  { return -1; // all output buffers are still completely filled, cannot add more samples !
                  }
                 DOBINI();
                 pb = m_pbOutputBuffer + m_iOutHeadIndex*m_OutBufferSize;
               }
              pfltSource += nChannelsPerSample;
            } // end for < all CHANNELS within the current sample point >
         } // end for <all 8-bit output samples>
      } // end if < 8 bits per sample>
     else
     if(m_OutFormat.wBitsPerSample == 24)
      {
        // 24 bits per sample.
        // Note that the input samples are FLOATING POINT NUMBERS,
        //  in most cases normalized to +/- 1.0  (since 2011-07) !
        fltFactor = 8388607.0; // factor: 2^23 - 1   IF the input was normalized
        if( pInChunkInfo != NULL )
         { if( pInChunkInfo->dblSampleValueRange > 0.0 )
            { fltFactor /= pInChunkInfo->dblSampleValueRange;
            }
         }
        for( i=0; i < iNumSamplePoints; i++ )
         {
           for(chn=0; chn<m_OutFormat.nChannels; ++chn)
            { if( chn<nChannelsPerSample )
               { fltTemp = pfltSource[chn];
               }
              else
               { fltTemp = 0.0;  // put "silence" into all unused channels
               }
              blTemp.i32 = (long)( fltTemp * fltFactor );
              pb[m_OutBufPosition++] = blTemp.b[0];
              pb[m_OutBufPosition++] = blTemp.b[1];
              pb[m_OutBufPosition++] = blTemp.b[2];
              if( m_OutBufPosition >= m_OutBufferSize )  // send it if full
               { result = OutFlushBuffer(iTimeout_ms);  // (due to arbitrary blocksize, this may happen "anytime" !)
                 if( result>=0 ) // successfully flushed, or empty output buffers available -> fill next buffer
                  { iWaited_ms +=  result;
                  }
                 else // Negative return value from OutFlushBuffer() -> ERROR !
                  { return -1; // all output buffers are still completely filled, cannot add more samples !
                  }
                 DOBINI();
                 pb = m_pbOutputBuffer + m_iOutHeadIndex*m_OutBufferSize;  // new output block, new local pointer !
               }
              pfltSource += nChannelsPerSample;
            } // end for < all CHANNELS within the current sample point >
         } // end for <all 24-bit output samples>
      } // end if < 24 bits per sample >
     else
     if(m_OutFormat.wBitsPerSample == 32)
      {
        // 32 bits per sample.
        // Note that the input samples are FLOATING POINT NUMBERS,
        //  in this case normalized to +/- 1.0  (since 2011-07) !
        // Note that the input samples are FLOATING POINT NUMBERS,
        //  in this case normalized to +/- 1.0  (since 2011-07) !
        for( i=0; i < iNumSamplePoints; i++ )
         {
           for(chn=0; chn<m_OutFormat.nChannels; ++chn)
            { if( chn<nChannelsPerSample )
               { fltTemp = pfltSource[chn];
               }
              else
               { fltTemp = 0.0;  // put "silence" into all unused channels
               }
              if(fltTemp >  1.0) fltTemp = 1.0; // clip the input (just a SAFETY PRECAUTION)
              if(fltTemp < -1.0) fltTemp =-1.0;
              // scale from +/- 1.0  to  32-bit signed integer .
              // Scaling factor is 2147483647 = 2^31 - 1 .
              *((long*)pb) = (long)( fltTemp * 2147483647.0 );  // -> +/- 2^31
              pb+=4;  m_OutBufPosition+=4;
              if( m_OutBufPosition >= m_OutBufferSize )  // send it if full
               { result = OutFlushBuffer(iTimeout_ms);  // (due to arbitrary blocksize, this may happen "anytime" !)
                 if( result>=0 ) // successfully flushed, or empty output buffers available -> fill next buffer
                  { iWaited_ms +=  result;
                  }
                 else // Negative return value from OutFlushBuffer() -> ERROR !
                  { return -1; // all output buffers are still completely filled, cannot add more samples !
                  }
                 DOBINI();
                 pb = m_pbOutputBuffer + m_iOutHeadIndex*m_OutBufferSize;
               }
              pfltSource += nChannelsPerSample;
            } // end for < all CHANNELS within the current sample point >
         } // end for <all 32-bit output samples>
      } // end if < 32 bits per sample >

     DOBINI();
     m_OutSamplesWritten += iNumSamplePoints;  // number of "sample points" (not multiplied with the number of channels in each point)
     if(piHaveWaited_ms != NULL)
       *piHaveWaited_ms = iWaited_ms;


     // only if no ENDLESS playing:
     if( (m_OutSampleLimit != 0) && (m_OutSamplesWritten >= m_OutSampleLimit) )
      { // reached the number of samples to be played. Close output.
        OutClose();
        DOBINI();
        if(m_ErrorCodeOut == NO_ERRORS )
           return 0;
        else
           return -1;
      }
     DOBINI();
   }
  return i32Result;  // returns the remaining 'free' output buffer space,
          // measured in sample points (not "bytes" or other stupidities)
} // end CSound::WriteOutputSamplePoints()


//---------------------------------------------------------------------
void CSound::OutClose()
   //  Closes the Soundcard (or similar) output if open.
{
  int i,j,result;

   m_OutputOpen = FALSE;
   if(m_hwvout != NULL)
    {
     DOBINI();
     waveOutReset(m_hwvout);  // stop and release buffers
        // > This function stops playback on a specified waveform output device
        // > and resets the current position to 0. All pending playback buffers
        // > are marked as done and returned to the application.
     DOBINI();
     for(i=0; i<m_NumOutputBuffers; i++ )
      {
        // only have to UNPREPARE sound output HEADERS if they are static,
        //           but MUST NOT 'free' them here !
        if( m_OutputWaveHdr[i].dwFlags & WHDR_PREPARED )
         { DOBINI();
           result = waveOutUnprepareHeader(m_hwvout, &m_OutputWaveHdr[i],sizeof(WAVEHDR));
           // > This function cleans up the preparation performed by waveOutPrepareHeader.
           // > The function must be called after the device driver is finished with a data block.
           // > This function complements waveOutPrepareHeader.
           // > You must call this function before freeing the buffer.
           // > After passing a buffer to the device driver with the waveOutWrite function,
           // > you must wait until the driver is finished with the buffer
           // > before calling waveOutUnprepareHeader.
           // (Naja, warum einfach wenn's auch KOMPLIZIERT geht.. ? )
           //
           // Similar as for the input, an output header' may also be
           // still in use when getting here (due to multithreading);
           // in that case result may be '33' which means "WAVERR_STILLPLAYING":
           // > WAVERR_STILLPLAYING :
           // >   The buffer pointed to by the pwh parameter is still in the queue.
           // A crude fix (similar as in InClose) :
           j = 10;  // wait up to 10 * 50ms 'til MMSYSTEM finished with this buffer:
           while( (j>0) && result==WAVERR_STILLPLAYING )
            { Sleep(50);
              result = waveOutUnprepareHeader(m_hwvout, &m_OutputWaveHdr[i],sizeof(WAVEHDR));
              --j;
            }
           if( result==MMSYSERR_NOERROR)
            { // Successfully 'unprepared' the wave-header-thing .
              // At this point, only 'WHDR_DONE' was set in dwFlags .
              // Clear that flag, too; for what it's worth..
              m_OutputWaveHdr[i].dwFlags = 0; // back to the 'initial state' for OutOpen() .
            }
           else
            { if( m_ErrorCodeOut==0)   // set debugger breakpoint HERE
                  m_ErrorCodeOut = result;
            }
         }
      }
      DOBINI();
      waveOutClose(m_hwvout);
      m_hwvout = NULL;
    } // end if(m_hwvout != NULL)

#if( SWI_AUDIO_IO_SUPPORTED )
  DOBINI();
  AIO_Host_CloseAudioOutput( &aio_Out );
#endif // SWI_AUDIO_IO_SUPPORTED

  DOBINI();
  if(m_iUsingASIO & 0x0002)    // was using ASIO for output ?
   { m_iUsingASIO &= ~0x0002;  // clear "using-ASIO-for-output"-flag
     if( m_iUsingASIO==0 )     // ASIO no longer in use now ?
      {
#if( SWI_ASIO_SUPPORTED )      // ASIO supported ? (through DL4YHF's wrapper)
        if( m_hAsio != C_ASIOWRAP_ILLEGAL_HANDLE )
         { AsioWrap_Close( m_hAsio );
           m_hAsio = C_ASIOWRAP_ILLEGAL_HANDLE;
         }
#endif // SWI_ASIO_SUPPORTED
      }
   } // end if(m_iUsingASIO & 0x0002)

  DOBINI();
} // end OutClose()


void CSound::Start(void)  // starts input and output (must be OPENED)
  // Note: Start() and Stop() seem to be MANDATORY for the ExtIO-DLLs !
{
  DOBINI();

#if ( SWI_AUDIO_IO_SUPPORTED )
  if( m_iInputDeviceID==C_SOUND_DEVICE_AUDIO_IO )
   { // Up to 2012-08, there was no extra starting. It's already complicated enough !
     // Input starts when opening the device for input,
     // output start when opening the device for output.  Basta.
     // See AudioIO.c :: AIO_Host_OpenAudioInput()  !
     // But for the ExtIO-DLLs, an extra "start" seems necessary, thus:
     AIO_Host_Start( &aio_In );
     // Note: The 'start' for ExtIO requires the EXTERNAL VFO FREQUENCY (!)
     //       already set, through CSound::SetVFOFrequency() [nnnngrrrr]
     // Because AIO_Host_Start() may take some time (during which a WAVE INPUT BUFFER
     //       did overflow; grep 2012-10-29), the 'audio-I/O' & ExtIO-host
     //       now starts the external device BEFORE starting the input from
     //       the soundcard (especially for FiFi-SDR) .
   }
#endif // SWI_AUDIO_IO_SUPPORTED ?


  if(m_InputOpen && (m_hwvin != NULL) )
   {
     waveInStart( m_hwvin);
   }
#if(0) // removed 2007-11; output only starts if buffer is full enough
  if(m_OutputOpen && (m_hwvout != NULL) && (!m_fOutputHoldoff) )
   {
     m_fOutputHoldoff = FALSE;
     waveOutRestart( m_hwvout );
   }
#endif // REMOVED 2007-11

#if( SWI_ASIO_SUPPORTED ) // ASIO supported ? (through DL4YHF's wrapper)
  if( m_iUsingASIO!=0 )   // using ASIO for in- or/and output :
   { // Start the ASIO driver (for BOTH INPUT AND OUTPUT !)
     AsioWrap_StartOrStop( m_hAsio, TRUE/*fStart*/ );
   }
#endif // SWI_ASIO_SUPPORTED


  DOBINI();
} // end CSound::Start()

void CSound::Stop(void)  // starts input and output (must be OPENED)
  // Note: Start() and Stop() seem to be MANDATORY for the ExtIO-DLLs !
{
#if ( SWI_AUDIO_IO_SUPPORTED )
  if( m_iInputDeviceID==C_SOUND_DEVICE_AUDIO_IO )
   { // Up to 2012-08, there was no extra starting / stopping.
     // But for the ExtIO-DLLs, an extra "stop" seems necessary, thus:
     AIO_Host_Stop( &aio_In );
     // 2012-11-19 : Crashed in AIO_Host_Stop() when called through
     //  SndThd_On50msTimer() -> CSound::Stop() , because the critical section
     //  was still occupied (from another thread), and some goddamned function
     //  in the ExtIO-DLL did never return. This happened over and over again.
   }
#endif // SWI_AUDIO_IO_SUPPORTED ?   
} // end CSound::Stop()


//---------------------------------------------------------------------------
void CALLBACK WaveInCallback(
        HWAVEIN m_hwvin,
        UINT uMsg,
        CSound* pCSound,
        DWORD dwParam1,  // is this the same as the "LPARAM"-thingy ?
        DWORD dwParam2 )
  //  Callback Routine called by mmsystem when a buffer becomes full.
  // > Remarks
  // > Applications should not call any system-defined functions from inside
  // > a callback function, except for EnterCriticalSection, LeaveCriticalSection,
  // > midiOutLongMsg, midiOutShortMsg, OutputDebugString, PostMessage,
  // > PostThreadMessage, SetEvent, timeGetSystemTime, timeGetTime,
  // > timeKillEvent, and timeSetEvent.
  // > Calling other wave functions will cause deadlock.
{
#define L_TEST_OVERLOAD 0
#if(L_TEST_OVERLOAD)
  static bool already_here = false;
   if(already_here)
      return;        // Set breakpoint HERE ! (should never trap)
   already_here=true;
#endif
  DOBINI_MM_CALLBACK();
  // LOCAL_BREAK(); // 2008-10-19 : Called from KERNEL32.DLL -> WINMM.DLL -> WaveInCallback(),
                    //  while InReadStereo() was waiting in the call after line "809" .

  switch( uMsg )
   {
     case WIM_OPEN:
     case WIM_CLOSE:
        DOBINI_MM_CALLBACK();
        break;           // simply ignore these messages

     case WIM_DATA:
        // > Sent .. when waveform-audio data is present in the input buffer
        // > and the buffer is being returned to the application.
        // > The message can be sent either when the buffer
        // > is full or after the waveInReset function is called.
        // > WIM_DATA Parameters:
        //    WIM_DATA :
        //        dwParam1 = (DWORD) lpwvhdr
        //      ( Pointer to a WAVEHDR structure that identifies the buffer
        //        containing the data )
        //        dwParam2 = reserved
        // > Remarks
        // >   The returned buffer might not be full. Use the dwBytesRecorded
        // >   member of the WAVEHDR structure specified by lParam to determine
        // >   the number of bytes recorded into the returned buffer.
        if( (pCSound != NULL) && (pCSound->m_NumInputBuffers>0) )
         {
           DOBINI_MM_CALLBACK();
#   if( CSOUND_USE_CRITICAL_SECTION )
           EnterCriticalSection(&pCSound->m_CriticalSection);
#   endif
            { DOBINI_MM_CALLBACK();

              // Take a snapshot of a 'high resolution timer' to reconstruct
              // a complete timestamp a later (in GetDateAndTimeForInput).
              // Note that here, inside the wave-input-callback, we do NOT
              // call any subroutine which may be "dangerous" to be called from here.
              // QueryPerformanceCounter() is assumed to be one of the "safe" functions.
              QueryPerformanceCounter( (LARGE_INTEGER*)&pCSound->m_InBufInfo[pCSound->m_iInHeadIndex].i64HRTimerValue );

              // Pass this "filled" input buffer to the application (here: CSound)
              // through the internal queue of audio input buffers:
              if( ++pCSound->m_iInHeadIndex >= pCSound->m_NumInputBuffers)  //inc ptr
               {  pCSound->m_iInHeadIndex = 0;   // handle wrap around
               }
              if( pCSound->m_iInHeadIndex == pCSound->m_iInTailIndex )   //chk for overflow
               {
                 pCSound->m_InOverflow = TRUE; // here in WaveInCallback() [windows mmsystem]
                 ++pCSound->m_LostBuffersIn;
               }
              if(pCSound->m_InWaiting)      // if user thread is waiting for buffer
               {
                 pCSound->m_InWaiting = FALSE;
                 if( pCSound->m_InEventHandle != NULL )
                  { DOBINI_MM_CALLBACK();
                    SetEvent( pCSound->m_InEventHandle); // signal it (here in MMSYSTEM callback)
                  }
               }
            } // end of critical(?) section
#   if( CSOUND_USE_CRITICAL_SECTION )
           LeaveCriticalSection(&pCSound->m_CriticalSection);
#   endif
           DOBINI_MM_CALLBACK();
         }
        else
         {
           DOBINI_MM_CALLBACK();
         }
        break; // end case MM_WIM_DATA
     default:
#     if (SWI_ALLOW_TESTING)
        Sound_iUnknownWaveInCallbackMsg = uMsg;
        DOBINI_MM_CALLBACK();
#     endif
        break;
   } // end switch( uMsg )

#if(L_TEST_OVERLOAD)
  already_here=false;
#endif

} // end WaveInCallback()


//---------------------------------------------------------------------------
void CALLBACK WaveOutCallback( HWAVEOUT m_hwvout, UINT uMsg, CSound* pCSound,
                     DWORD dwParam1, DWORD dwParam2 )
  // Callback function for the windoze 'multimedia' API.
  //    Called by mmsystem when one of the previously 'written' buffers
  //    becomes empty.  For details, see OutFlushBuffer() .
  //    No OS routines except SetEvent may be called from here.
  //
  // Call Tree (usually) :
  //    kernel32.dll -> wdmaud.drv -> .. -> WINMM.DLL -> msacm32.drv (sometimes)
  //        -> WaveOutCallback() .
  // Note: Sometimes, a "phantom breakpoint" fired from WDMAUD.D??  ?!
{
  int buffer_tail_index;
  DOBINI_MM_CALLBACK();
  switch( uMsg )
   {
     case WOM_DONE :  // "buffer empty" message
     #if( CSOUND_USE_CRITICAL_SECTION )
        EnterCriticalSection(&pCSound->m_CriticalSection);
     #endif
        buffer_tail_index = pCSound->m_iOutTailIndex + 1;
        if( buffer_tail_index >= pCSound->m_NumOutputBuffers )
         {  buffer_tail_index = 0;
         }
        pCSound->m_iOutTailIndex = buffer_tail_index;   // set the new (incremented) buffer TAIL index, here in WaveOutCallback()
        // Note: The only WRITER for m_iOutTailIndex is here, in the callback,
        //       thus there are no conflicts due to multitasking.
        //       By using a local variable to increment + wrap the index,
        //       the value of pCSound->m_iOutTailIndex is always VALID,
        //       and can be safely read anywhere .
        if(pCSound->m_OutWaiting) // if user thread is waiting for buffer
         {
           pCSound->m_OutWaiting = FALSE;
           DOBINI_MM_CALLBACK();
           SetEvent( pCSound->m_OutEventHandle);
           // 2013-20: what happens with SetEvent when there's nobody really waiting ?
           // > The SetEvent function sets the state of the specified event object to signaled.
           // > The state of an auto-reset event object remains signaled
           // > until a single waiting thread is released, at which time the system
           // > automatically sets the state to nonsignaled. If no threads are waiting,
           // > the event object's state remains signaled.
           //
         }
#      if( CSOUND_USE_CRITICAL_SECTION )
        LeaveCriticalSection(&pCSound->m_CriticalSection);
#      endif
        break; // end case WOM_DONE

     case WOM_OPEN : // ignore
     case WOM_CLOSE: // ignore
        break;
     default:
#      if (SWI_ALLOW_TESTING)
        Sound_iUnknownWaveOutCallbackMsg = uMsg;  // what's this ? set breakpoint here...
        DOBINI_MM_CALLBACK();
#      endif
        break;
   } // end switch uMsg
  DOBINI_MM_CALLBACK();
} // end WaveOutCallback()

WAVEHDR * CSound::GetInputBufHdr(int iBufNrOffset)  // used in ASIO callback
   // iBufNrOffset is the number of buffers added as 'offset'
   // to the current wave-buffer-"HEAD"-index ("m_iInHeadIndex") .
{
  if( m_NumInputBuffers>0 )  // avoid div by zero
   { iBufNrOffset = ( iBufNrOffset + m_iInHeadIndex ) % m_NumInputBuffers;
     return &m_InputWaveHdr[iBufNrOffset];
   }
  else
   { return &m_InputWaveHdr[0];
   }
}

WAVEHDR * CSound::GetOutputBufHdr(int iBufNrOffset)  // used in ASIO callback
   // iBufNrOffset is the number of buffers added as 'offset'
   // to the current wave-buffer-"HEAD"-index ("m_iOutTailIndex") .
{
  if( m_NumOutputBuffers>0 ) // avoid div by zero
   { iBufNrOffset = ( iBufNrOffset + m_iOutTailIndex ) % m_NumOutputBuffers;
     return &m_OutputWaveHdr[iBufNrOffset];
   }
  else
   { return &m_InputWaveHdr[0];
   }

}


//----------------------------------------------------------------------------------
// Callback function to process the data for ASIO driver system (in- AND output)
//   Call stack (example):
//     C:\WINDOWS\system32\emasio.dll -> AsioWrap_BufferSwitchTimeInfo0()
//          -> AsioWrap_BufferSwitchTimeInfo() -> CSound_AsioCallback()  .
#if( SWI_ASIO_SUPPORTED )
void CSound_AsioCallback(
       T_AsioWrap_DriverInfo *pDriverInfo,
            long bufferIndex,  // [in] 'index' from Steinberg's bufferSwitch(),
                               //  but INDEX OF WHAT ? .. such a stupid name.
                               //  Someone called it bufferIndex to make this clear.
                               // It's definitely NOT a SAMPLE-INDEX.
           DWORD dwInstance )
{  // The actual processing callback. Must match 'AsioDataProc' in asiowrapper.h .
   // Beware that this is normally in a seperate thread, hence be sure that you take care
   // about thread synchronization. This is omitted here for simplicity.
   // ex: static processedSamples = 0;   // static what ? INTEGER ?!?!?! baaah..
  union { long i32; WORD w[2]; BYTE b[4]; } blong;  // byte-addressable 'long'. For INTEL, b[0]=LSB
  CSound* pCSound = (CSound*)dwInstance; // pointer to "this" CSound instance
  WAVEHDR *pInBufHdr, *pOutBufHdr;
  BYTE  *pbDst, *pbSrc;
  DWORD dwBytesRecorded;    // .. in CSound's waveIn-like buffer
  DWORD dwBytesPlayed;      // .. in CSound's waveOut-like buffer
  DWORD dwInDstBytesPerSingleSample;    //  (BitsPerSample+7)/8 for Audio-INPUT(destination buffer format)
  DWORD dwInDstBytesPerSamplePoint;     // ((BitsPerSample+7)/8) * NumberOfChannels (!)
  DWORD dwOutSrcBytesPerSingleSample;   //  (BitsPerSample+7)/8 for Audio-OUTPUT(source buffer format)
  DWORD dwOutSrcBytesPerSamplePoint;    // ((BitsPerSample+7)/8) * NumberOfChannels
  DWORD dwInputChannelMask, dwOutputChannelMask;
  int  nInputChannelsRead, nOutputChannelsWritten;
  int  nInBuffersReady = 0;  // number of CSound input buffers finished in this call
  int  nOutBuffersReady= 0;  // number of CSound output buffers finished in this call

  // Buffer size in samples. This is the 3rd parameter passed
  //  to ASIOCreateBuffers(), not necessarily the "preferred" size !
  long buffSize = pDriverInfo->dwUsedBufferSize; // this is the NUMBER OF SAMPLES, not BYTES !
  // Beware: 'buffSize' can be anything; see asiowrapper.c for details .
  //          It has nothing to do with CSound's m_InBufferSize member !
  // In fact, the author's Audigy 2 ZS wished a buffer size of 4800 samples ,
  // while the CSound class always uses a power of two for convenience .

  if(pCSound == NULL)
   { // oops .. this should not happen !
     DOBINI_ASIO_CALLBACK();  // -> Sound_iAsioCbkSourceCodeLine
     return;
   }

  if(! pCSound->m_iUsingASIO)
   { // oops .. this should also not happen !
     DOBINI_ASIO_CALLBACK();  // -> Sound_iAsioCbkSourceCodeLine
     return;
   }


     DOBINI_ASIO_CALLBACK();  // -> Sound_iAsioCbkSourceCodeLine (?)
     nInputChannelsRead     = 0;  // no input channel read yet
     nOutputChannelsWritten = 0;  // no output channel written yet
     dwInputChannelMask = dwOutputChannelMask = 1;
     dwInDstBytesPerSingleSample= (pCSound->m_InFormat.wBitsPerSample+7)/8;
     dwInDstBytesPerSamplePoint = pCSound->m_InFormat.nChannels * dwInDstBytesPerSingleSample;
     dwOutSrcBytesPerSingleSample= (pCSound->m_OutFormat.wBitsPerSample+7)/8;
     dwOutSrcBytesPerSamplePoint = pCSound->m_OutFormat.nChannels * dwOutSrcBytesPerSingleSample;

     dwBytesRecorded = dwBytesPlayed = 0; // to see if loop 'did nothing'
     pInBufHdr = pOutBufHdr = NULL;

     // Loop for all ASIO-buffers begins here >>>>>>>>>>>>>>>>
     for (int i = 0; i < pDriverInfo->nInputBuffers + pDriverInfo->nOutputBuffers; i++)
      {
        if(   (pDriverInfo->bufferInfos[i].isInput )
           && (pCSound->m_InputOpen ) // <<< added 2011-03-13 for safety
          )
         { // ASIO INPUT (from ASIO to application) ---->>>
           if(   (pCSound->m_NumInputBuffers>0) && ((pCSound->m_iUsingASIO&1)!=0)
              && ((pCSound->m_dwInputSelector & dwInputChannelMask)!=0)
              && (nInputChannelsRead < pCSound->m_InFormat.nChannels )
             )
            { // It's an input (from ASIO to application),
              // and the CSound class wants to "have it" .
              DOBINI_ASIO_CALLBACK(); // -> Sound_iAsioCbkSourceCodeLine
              // Determine the DESTINATION ADDRESS for the new input data :
              nInBuffersReady = 0;   // must be cleared again for EACH ASIO-BUFFER-LOOP !
              pInBufHdr = pCSound->GetInputBufHdr(nInBuffersReady);
              dwBytesRecorded = pInBufHdr->dwBytesRecorded; // number of bytes already in buffer
              pbDst = (BYTE*)pInBufHdr->lpData + dwBytesRecorded
                              + nInputChannelsRead*dwInDstBytesPerSingleSample;
              // Copy (and most likely convert) the data from the ASIO buffer
              // into CSound's buffer .
              // Notes:
              //  - the buffer sizes may be very different (ASIO can be very strange !)
              //  - the DATA TYPES may also be very strange. Here, only the most
              //    "typical" cases (for a windows PC) will be handled !
              //  - We may fill NONE, ONE, or MANY CSound buffers here,
              //    depending on the relationship between buffSize (ASIO, number of SAMPLES)
              //    and pInBufHdr->dwBufferLength (MMSYSTEM/WAVEHDR, number of BYTES) !
              //  - The driver tells US which format WE must cope with,
              //    instead of *us* telling the driver what we want from it !
              switch (pDriverInfo->channelInfos[i].type)
               {
                 case ASIOSTInt16LSB: // 16-bit signed integer, LSB first ("Intel format")
                  { SHORT *pi16 = (SHORT*)pDriverInfo->bufferInfos[i].buffers[bufferIndex];
                    if( pCSound->m_InFormat.wBitsPerSample==16 )
                     { // no need to convert here,
                       //  it's our "native" 16-bit format so just COPY 16-bit wise:
                       for(int sample=0; sample<buffSize; ++sample)
                        { *((WORD*)pbDst) = *pi16++;  // source & dest: 16 bit, LSB first
                          pbDst += dwInDstBytesPerSamplePoint;
                          dwBytesRecorded += dwInDstBytesPerSamplePoint;
                          if( dwBytesRecorded >= pInBufHdr->dwBufferLength )
                           { // next buffer please.. but don't "emit" this one yet, because
                             // other channels may follow which will be copied into the same
                             // interlaced buffer (so we may have to access it again).
                             // NOT YET: pInBufHdr->dwBytesRecorded = pInBufHdr->dwBufferLength;
                             ++nInBuffersReady;
                             pInBufHdr = pCSound->GetInputBufHdr(nInBuffersReady); // pointer to "next" buffer !
                             dwBytesRecorded = pInBufHdr->dwBytesRecorded = 0;
                             pbDst = (BYTE*)pInBufHdr->lpData + nInputChannelsRead*dwInDstBytesPerSingleSample;
                           }
                        }
                     }
                    else  // must convert...
                     { for(int sample=0; sample<buffSize; ++sample)
                        { blong.i32 = *pi16++;  // source: 16 bit, LSB first ("Intel")
                        }
                     }
                   }
                  break;
                  case ASIOSTInt24LSB: // used for 20 bits as well
                    // Note YHF: This does NOT seem to be the usual type for 24-bit output !!!
                   {// should be the "first" channel if we can trust the spec...
                    BYTE *pb = (BYTE*)pDriverInfo->bufferInfos[i].buffers[bufferIndex];
                    for(int sample=0; sample<buffSize; ++sample)
                     { blong.i32 = 0;
                       *pb++ = blong.b[0];  // LSB first
                       *pb++ = blong.b[1];  // mid byte
                       *pb++ = blong.b[2];  // high byte (of 24-bit quantity)
                     }
                   }
                 break;
                 case ASIOSTInt32LSB:
                  { // Note: This is what Creative's "updated" driver seems to use,
                    //       interestingly NOT one of those many 24-bit types .
                    long *pi32 = (long*)pDriverInfo->bufferInfos[i].buffers[bufferIndex];
                    if( pCSound->m_InFormat.wBitsPerSample==32 )
                     { // no need to convert a lot here, it's our "native" 32-bit format
                       for(int sample=0; sample<buffSize; ++sample)
                        { *((long*)pbDst) = *pi32++;  // source & dest: 32 bit, LSB first
                          pbDst += dwInDstBytesPerSamplePoint;
                          dwBytesRecorded += dwInDstBytesPerSamplePoint; // 4 or 8
                          if( dwBytesRecorded >= pInBufHdr->dwBufferLength )
                           { ++nInBuffersReady;   // next destination buffer ...
                             pInBufHdr = pCSound->GetInputBufHdr(nInBuffersReady); // pointer to "next" buffer !
                             dwBytesRecorded = pInBufHdr->dwBytesRecorded = 0;
                             pbDst = (BYTE*)pInBufHdr->lpData + nInputChannelsRead*dwInDstBytesPerSingleSample;
                           }
                        }
                     }
                    else  // must convert...
                    if( pCSound->m_InFormat.wBitsPerSample==16 )  // ... from 32 to 16 bit
                     { for(int sample=0; sample<buffSize; ++sample)
                        { blong.i32 = *pi32++;          // source: 32 bit, LSB first, left-aligned
                          *((WORD*)pbDst) = blong.w[0]; // dest: 16 bit (upper 16 bit from source)
                          pbDst += dwInDstBytesPerSamplePoint;
                          dwBytesRecorded += dwInDstBytesPerSamplePoint;
                          if( dwBytesRecorded >= pInBufHdr->dwBufferLength )
                           { ++nInBuffersReady;   // next destination buffer ...
                             pInBufHdr = pCSound->GetInputBufHdr(nInBuffersReady); // pointer to "next" buffer !
                             dwBytesRecorded = pInBufHdr->dwBytesRecorded = 0;
                             pbDst = (BYTE*)pInBufHdr->lpData + nInputChannelsRead*dwInDstBytesPerSingleSample;
                           }
                        }
                     }
                  } break;
                 case ASIOSTFloat32LSB: // IEEE 754 32 bit float, as found on Intel x86 architecture
                    break;
                 case ASIOSTFloat64LSB: // IEEE 754 64 bit double float, as found on Intel x86 architecture
                    break;

                    // these are used for 32 bit data buffer, with different alignment of the data inside
                    // 32 bit PCI bus systems can more easily used with these .
                    // YHF: NNNNNGRRRRRR... Can we yet some more useless types please ?!?
                 case ASIOSTInt32LSB16: // 32 bit data with 18 bit alignment
                 case ASIOSTInt32LSB18: // 32 bit data with 18 bit alignment
                 case ASIOSTInt32LSB20: // 32 bit data with 20 bit alignment
                 case ASIOSTInt32LSB24: // 32 bit data with 24 bit alignment
                    break;

                 case ASIOSTInt16MSB:
                    break;
                 case ASIOSTInt24MSB: // used for 20 bits as well
                    break;
                 case ASIOSTInt32MSB:
                    break;
                 case ASIOSTFloat32MSB: // IEEE 754 32 bit float, as found on Intel x86 architecture
                    break;
                 case ASIOSTFloat64MSB: // IEEE 754 64 bit double float, as found on Intel x86 architecture
                    break;

                 // these are used for 32 bit data buffer, with different alignment of the data inside
                 // 32 bit PCI bus systems can more easily used with these
                 case ASIOSTInt32MSB16: // 32 bit data with 18 bit alignment
                 case ASIOSTInt32MSB18: // 32 bit data with 18 bit alignment
                 case ASIOSTInt32MSB20: // 32 bit data with 20 bit alignment
                 case ASIOSTInt32MSB24: // 32 bit data with 24 bit alignment
                    break;
               }
              // not yet: pInBufHdr->dwBytesRecorded = dwBytesRecorded; !
              // may have to write into the same interlaced buffer for the next channel,
              // so write 'dwBytesRecorded' back to the CSound buffer AFTER the channel-loop.
              ++nInputChannelsRead;    // count the number of PROCESSED input channels
              DOBINI_ASIO_CALLBACK(); // -> Sound_iAsioCbkSourceCodeLine
            } // end if < want this input chanel >
           else
            { // Application not ready to 'accept' this input buffer ?
              DOBINI_ASIO_CALLBACK(); // -> Sound_iAsioCbkSourceCodeLine
            }
           dwInputChannelMask <<= 1;
         } // end if < valid INPUT >
        else  // cannot accept the input at the moment, for whatever reason
         { DOBINI_ASIO_CALLBACK(); // -> Sound_iAsioCbkSourceCodeLine
         }
        if( (pDriverInfo->bufferInfos[i].isInput == false) // ASIO OUTPUT ---->>>
           && (pCSound->m_InputOpen ) // <<< added 2011-03-13 for safety
          )
         { // OK do processing for the outputs
           if(   (pCSound->m_NumOutputBuffers>0) && ((pCSound->m_iUsingASIO&2)!=0)
              && ((pCSound->m_dwOutputSelector & dwOutputChannelMask)!=0)
              && (nOutputChannelsWritten < pCSound->m_OutFormat.nChannels )
             )
            { // It's an output channel (from the application to ASIO driver ),
              // and the CSound class wants to "fill it" .
              // Determine the DESTINATION ADDRESS for the new input data :
              nOutBuffersReady = 0;   // must be cleared again for EACH ASIO-BUFFER-LOOP !
              pOutBufHdr = pCSound->GetOutputBufHdr(nOutBuffersReady);
              dwBytesPlayed = pOutBufHdr->dwBytesRecorded; // number of bytes already PLAYED (!)
              if( dwBytesPlayed>= pOutBufHdr->dwBufferLength )
               { dwBytesPlayed = 0;   // should never happen, but avoid access violation !
               }
              pbSrc = (BYTE*)pOutBufHdr->lpData + dwBytesPlayed   // source block for output
                    + nOutputChannelsWritten*dwOutSrcBytesPerSingleSample;
              switch (pDriverInfo->channelInfos[i].type)   // how to convert the OUTPUT...
               {
                 case ASIOSTInt16LSB: // into 16-bit signed integer, LSB first ("Intel format")
                  { SHORT *pi16 = (SHORT*)pDriverInfo->bufferInfos[i].buffers[bufferIndex];
                    if( pCSound->m_OutFormat.wBitsPerSample==16 )
                     { // no need to convert a lot here, it's our "native" 16-bit format:
                       for(int sample=0; sample<buffSize; ++sample)
                        {
                          *pi16++ = *((WORD*)pbSrc);
                          pbSrc += dwOutSrcBytesPerSamplePoint;
                          dwBytesPlayed += dwOutSrcBytesPerSamplePoint;
                          if( dwBytesPlayed >= pOutBufHdr->dwBufferLength )
                           { // next buffer please.. but don't increment m_iOutTailIndex now, because
                             // other channels may follow which will be filled from the same
                             // interlaced buffer (so we may have to access it again).
                             ++nOutBuffersReady;
                             pOutBufHdr = pCSound->GetOutputBufHdr(nOutBuffersReady);
                             dwBytesPlayed = pOutBufHdr->dwBytesRecorded = 0;
                             pbSrc = (BYTE*)pOutBufHdr->lpData + nOutputChannelsWritten*dwOutSrcBytesPerSingleSample;
                           }
                        } // end for
                     } // end if < buffer contains 16 bits per single sample too >
                    else
                     {
                     }
                  } break; // end case ASIOSTInt16LSB  ( on  OUTPUT )

                 case ASIOSTInt24LSB: // used for 20 bits as well
                  { // Note: This does NOT seem to be the usual type for 24-bit output
                    // (at least not for Creative's Audigy 2 ZS), but other cards
                    // may use it  - - - so support it too :
                    BYTE *pb = (BYTE*)pDriverInfo->bufferInfos[i].buffers[bufferIndex];
                    if( pCSound->m_OutFormat.wBitsPerSample==24 )
                     { // copy 24-bit-wise :
                       for(int sample=0; sample<buffSize; ++sample)
                        {
                          *pb++ = pbSrc[0];   // bits 23..16 (MSB)
                          *pb++ = pbSrc[1];   // bits 15..8
                          *pb++ = pbSrc[2];   // bits  7..0  (LSB)
                          pbSrc += dwOutSrcBytesPerSamplePoint; // increment by 3 or 6
                          dwBytesPlayed += dwOutSrcBytesPerSamplePoint;
                          if( dwBytesPlayed >= pOutBufHdr->dwBufferLength )
                           { ++nOutBuffersReady;  // next buffer please
                             pOutBufHdr = pCSound->GetOutputBufHdr(nOutBuffersReady);
                             dwBytesPlayed = pOutBufHdr->dwBytesRecorded = 0;
                             pbSrc = (BYTE*)pOutBufHdr->lpData + nOutputChannelsWritten*dwOutSrcBytesPerSingleSample;
                           }
                        } // end for
                     }
                    else // ASIO wants 24 bits per sample for output, but the source is different:
                    if( pCSound->m_OutFormat.wBitsPerSample==16 )
                     { // copy 16-bit samples into 24-bit destination :
                       for(int sample=0; sample<buffSize; ++sample)
                        {
                          *pb++ = pbSrc[0];   // bits 23..16 (MSB)
                          *pb++ = pbSrc[1];   // bits 15..8
                          *pb++ = 0;          // leave bits 7..0 zero
                          pbSrc += dwOutSrcBytesPerSamplePoint; // increment by 2 or 4
                          dwBytesPlayed += dwOutSrcBytesPerSamplePoint;
                          if( dwBytesPlayed >= pOutBufHdr->dwBufferLength )
                           { ++nOutBuffersReady;  // next buffer please
                             pOutBufHdr = pCSound->GetOutputBufHdr(nOutBuffersReady);
                             dwBytesPlayed = pOutBufHdr->dwBytesRecorded = 0;
                             pbSrc = (BYTE*)pOutBufHdr->lpData + nOutputChannelsWritten*dwOutSrcBytesPerSingleSample;
                           }
                        } // end for
                     }
                  } break; // end case ASIOSTInt24LSB  ( on  OUTPUT )

                 case ASIOSTInt32LSB:
                  { // This is what Creative's "updated" driver seems to use,
                    // interestingly they don't use one of those many 24-bit types .
                    if( pCSound->m_OutFormat.wBitsPerSample==32 )
                     { // no need to convert a lot here, it's our "native" 32-bit format:
                       long *pi32 = (long*)pDriverInfo->bufferInfos[i].buffers[bufferIndex];
                       for(int sample=0; sample<buffSize; ++sample)
                        {
                          *pi32++ = *((long*)pbSrc);
                          pbSrc += dwOutSrcBytesPerSamplePoint; // 4 or 8 bytes per "point"
                          dwBytesPlayed += dwOutSrcBytesPerSamplePoint;
                          if( dwBytesPlayed >= pOutBufHdr->dwBufferLength )
                           { ++nOutBuffersReady;  // next buffer please
                             pOutBufHdr = pCSound->GetOutputBufHdr(nOutBuffersReady);
                             dwBytesPlayed = pOutBufHdr->dwBytesRecorded = 0;
                             pbSrc = (BYTE*)pOutBufHdr->lpData + nOutputChannelsWritten*dwOutSrcBytesPerSingleSample;
                           }
                        } // end for
                     } // end if < buffer contains 32 bits per sample too >
                    else // ASIO wants 32 bits per sample for output, but source is different:
                    if( pCSound->m_OutFormat.wBitsPerSample==16 )
                     { // convert from 16 bits/sample [source]   to 32 bits/sample [destination] for output:
                       BYTE *pb = (BYTE*)pDriverInfo->bufferInfos[i].buffers[bufferIndex];
                       for(int sample=0; sample<buffSize; ++sample)
                        {
                          *pb++ = pbSrc[0]; // bits 31..24 (MSB)
                          *pb++ = pbSrc[1]; // bits 23..16
                          *pb++ = 0;        // leave bits 15..8 zero
                          *pb++ = 0;        // leave bits  7..0 zero
                          pbSrc += dwOutSrcBytesPerSamplePoint; // increment by 2 or 4
                          dwBytesPlayed += dwOutSrcBytesPerSamplePoint;
                          if( dwBytesPlayed >= pOutBufHdr->dwBufferLength )
                           { ++nOutBuffersReady;  // next buffer please
                             pOutBufHdr = pCSound->GetOutputBufHdr(nOutBuffersReady);
                             dwBytesPlayed = pOutBufHdr->dwBytesRecorded = 0;
                             pbSrc = (BYTE*)pOutBufHdr->lpData + nOutputChannelsWritten*dwOutSrcBytesPerSingleSample;
                           }
                        } // end for
                     }
                  } break; // end case ASIOSTInt32LSB  ( on  OUTPUT )

                 case ASIOSTFloat32LSB: // IEEE 754 32 bit float, as found on Intel x86 architecture
                    memset (pDriverInfo->bufferInfos[i].buffers[bufferIndex], 0, buffSize * 4);
                    break;
                 case ASIOSTFloat64LSB: // IEEE 754 64 bit double float, as found on Intel x86 architecture
                    memset (pDriverInfo->bufferInfos[i].buffers[bufferIndex], 0, buffSize * 8);
                    break;

                    // these are used for 32 bit data buffer, with different alignment of the data inside
                    // 32 bit PCI bus systems can more easily used with these .
                    // YHF: NNNNNGRRRRRR... Is it really necessary to support these ?
                 case ASIOSTInt32LSB16: // 32 bit data with 18 bit alignment
                 case ASIOSTInt32LSB18: // 32 bit data with 18 bit alignment
                 case ASIOSTInt32LSB20: // 32 bit data with 20 bit alignment
                 case ASIOSTInt32LSB24: // 32 bit data with 24 bit alignment
                    memset (pDriverInfo->bufferInfos[i].buffers[bufferIndex], 0, buffSize * 4);
                    break;

                 case ASIOSTInt16MSB:
                    memset (pDriverInfo->bufferInfos[i].buffers[bufferIndex], 0, buffSize * 2);
                    break;
                 case ASIOSTInt24MSB:  // used for 20 bits as well
                    memset (pDriverInfo->bufferInfos[i].buffers[bufferIndex], 0, buffSize * 3);
                    break;
                 case ASIOSTInt32MSB:
                    memset (pDriverInfo->bufferInfos[i].buffers[bufferIndex], 0, buffSize * 4);
                    break;
                 case ASIOSTFloat32MSB: // IEEE 754 32 bit float, as found on Intel x86 architecture
                    memset (pDriverInfo->bufferInfos[i].buffers[bufferIndex], 0, buffSize * 4);
                    break;
                 case ASIOSTFloat64MSB: // IEEE 754 64 bit double float, as found on Intel x86 architecture
                    memset (pDriverInfo->bufferInfos[i].buffers[bufferIndex], 0, buffSize * 8);
                    break;

                 // these are used for 32 bit data buffer, with different alignment of the data inside
                 // 32 bit PCI bus systems can more easily used with these
                 case ASIOSTInt32MSB16: // 32 bit data with 18 bit alignment
                 case ASIOSTInt32MSB18: // 32 bit data with 18 bit alignment
                 case ASIOSTInt32MSB20: // 32 bit data with 20 bit alignment
                 case ASIOSTInt32MSB24: // 32 bit data with 24 bit alignment
                    memset (pDriverInfo->bufferInfos[i].buffers[bufferIndex], 0, buffSize * 4);
                    break;
               }
              ++nOutputChannelsWritten;  // count the number of FILLED output channels
              // not yet: pOutBufHdr->dwBytesRecorded = dwBytesPlayed; !
              // may have to write into the same interlaced buffer for the next channel,
              // so write 'dwBytesRecorded' back to the CSound buffer AFTER the channel-loop.
            } // end if < want to fill this OUTPUT-channel ? >
           else // don't want to fill this output channel ...
            {
              // The question is these "unused output channels" must be
              //  "filled with silence" (like in Steinberg's demo from the ASIO SDK)
              // or if it's ok to leave these channels untouched .
            } // end if < want this input chanel >
           dwOutputChannelMask <<= 1;
         } // end if < valid OUTPUT >
      } // end for < all buffers the ASIO driver wants to exchange with us during this call >

     if(pInBufHdr!=NULL)
      { // AFTER processing the last input channel, set the new number of bytes
        // which have been 'recorded' in CSound's currently used input-buffer.
        //  Note: if nInBuffersReady>0, this is already the "next" buffer.
        pInBufHdr->dwBytesRecorded = dwBytesRecorded;
      }
     // Emit the new filled buffers *AFTER* the ASIO-buffer-loop.
     // Note that pCSound->m_iInHeadIndex was not modified in the above loop !
     if(nInBuffersReady>0)    // note: in rare cases, there may be MORE THAN ONE buffer filled now !
      {
#     if( CSOUND_USE_CRITICAL_SECTION )
       EnterCriticalSection(&pCSound->m_CriticalSection);  // try to live without this, it took quite long !
#     endif       
       // (remember, this ASIO callback may be called 500 times a second.. no time to waste)
       for( int i=0; i<nInBuffersReady; ++i )
        { // mark the next input buffer as "filled" ..
          pInBufHdr = &pCSound->m_InputWaveHdr[pCSound->m_iInHeadIndex];
          pInBufHdr->dwBytesRecorded = pInBufHdr->dwBufferLength;
          if( ++pCSound->m_iInHeadIndex >= pCSound->m_NumInputBuffers) // inc ptr
                pCSound->m_iInHeadIndex = 0;   // handle wrap around
          if( pCSound->m_iInHeadIndex == pCSound->m_iInTailIndex )     // chk for overflow
           {
             pCSound->m_InOverflow = TRUE;  // here in CSound_AsioCallback() [for the ASIO driver]
             ++pCSound->m_LostBuffersIn;
             // 2010-04-22: Occasionally got here at 192 kSamples/second, E-MU 0202,
             //      with pCSound->m_NumInputBuffers = 4 ,
             //           pCSound->m_InFormat.wBitsPerSample=32 ,
             //           pCSound->m_InFormat.nChannels = 1 ,
             //      and  dwBufferLength = 131072 bytes = 32768 samples;
             //      i.e. despite 0.68 seconds of (total) buffer size,
             //      samples got lost .. even though the CPU performance window
             //      in SpecLab, and the task manager, showed a CPU load of ~60 % .
           }
        } // end for
       if(pCSound->m_InWaiting)      // if user thread is waiting for buffer
        {
          pCSound->m_InWaiting = FALSE;
          if( pCSound->m_InEventHandle != NULL )
           { DOBINI_ASIO_CALLBACK();
             SetEvent( pCSound->m_InEventHandle); // signal it (here in ASIO callback)
           }
        }
#     if( CSOUND_USE_CRITICAL_SECTION )
       LeaveCriticalSection(&pCSound->m_CriticalSection);
#     endif       
      } // end if(nInBuffersReady>0)

     if(pOutBufHdr!=NULL)
      { // AFTER sending the last output channel, set the new number of bytes
        // which have been 'played' from CSound's current active output-buffer.
        //   Note: if nOutBuffersReady>0, this is already the "next" buffer.
        // The component 'dwBytesRecorded' is from a WAVEHDR struct....
        // The Win32 programmer's reference only mentions its meaning on
        //  INPUT-buffers :
        // > When the header is used in input, this member specifies how much data
        // > is in the buffer.
        // Here:
        // < The header is used for output; this member specifies how much data is already PLAYED
        // < from the buffer.
        pOutBufHdr->dwBytesRecorded/*"!"*/ = dwBytesPlayed/*!*/;
      }
     // Signal CSound if finished with another output buffer *AFTER* the ASIO-buffer-loop.
     // Note that pCSound->m_iOutTailIndex was not modified in the above loop !
     if(nOutBuffersReady>0)    // note: in rare cases, there may be MORE THAN ONE buffer filled now !
      {
#      if( CSOUND_USE_CRITICAL_SECTION )
        EnterCriticalSection(&pCSound->m_CriticalSection);  // try to save this time ?
#      endif
        for( int i=0; i<nOutBuffersReady; ++i )
         { // mark another output buffer as "sent" ..
           if( ++pCSound->m_iOutTailIndex >= pCSound->m_NumOutputBuffers) // inc ptr, here for ASIO (not MMSYS)
               pCSound->m_iOutTailIndex = 0;   // handle wrap around
           if( pCSound->m_iOutHeadIndex == pCSound->m_iOutTailIndex )
            { // chk for underflow
              pCSound->m_OutUnderflow = TRUE;
              ++pCSound->m_LostBuffersOut;  // lost another OUTPUT block (here in ASCI-callback)
              if( (pCSound->m_ErrorCodeOut == NO_ERRORS )
                ||(pCSound->m_ErrorCodeOut == SOUNDOUT_ERR_LOW_WATER) )
               {  pCSound->m_ErrorCodeOut = SOUNDOUT_ERR_UNDERFLOW;
               }
            }
         } // end for
        if(pCSound->m_OutWaiting) // if user thread is waiting for buffer..
         {
           pCSound->m_OutWaiting = FALSE;
           DOBINI_ASIO_CALLBACK();
           SetEvent( pCSound->m_OutEventHandle); // .. then kick it alive again
         }
#      if( CSOUND_USE_CRITICAL_SECTION )
        LeaveCriticalSection(&pCSound->m_CriticalSection);
#      endif
        DOBINI_ASIO_CALLBACK();
      } // end if(nOutBuffersReady>0)
     else // no output buffers ready... we simply may not use the output at the moment !
      { DOBINI_ASIO_CALLBACK(); // -> Sound_iAsioCbkSourceCodeLine
      }

} // end CSound_AsioCallback()
#endif // SWI_ASIO_SUPPORTED



/* EOF < Sound.cpp >  */

