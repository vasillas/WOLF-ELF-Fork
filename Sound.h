//---------------------------------------------------------------------------
// File:    C:\cbproj\SoundUtl\Sound.h
// Date:    2012-08-14 (ISO8601, YYYY-MM-DD)
// Author : Wolfgang Buescher, DL4YHF
// Purpose: interface for the CSound class.
//
//---------------------------------------------------------------------------


#if !defined(_INCLUDE_SOUND_H_)
#define _INCLUDE_SOUND_H_


// #pragma alignment  /* emits a warning telling us the current struct alignment */

#include <mmsystem.h>  // BEWARE OF WRONG STRUCT ALIGNMENT IN BORLAND C++BUILDER V6 !


#include "SWITCHES.H"    // application specific compilation switches
                         // located in some PROJECT-SPECIFIC subdirectory !

#ifndef  SWI_FLOAT_PRECISION   // may be defined under Project..Options..Conditionals
 #define SWI_FLOAT_PRECISION 1 // 1=single precision, 2=double precision
#endif
#ifndef  SWI_ASIO_SUPPORTED    // Support ASIO audio drivers too ?
 #define SWI_ASIO_SUPPORTED  1 // 1=yes,    0=no (MMSYSTEM only)
#endif
#if( SWI_ASIO_SUPPORTED )      // Support ASIO audio drivers too ?
 #include "asiowrapper.h"      // DL4YHF's wrapper for Steinberg's ASIO driver,
 // located in \cbproj\SoundUtl\myasio\asiowrapper.h  .
#endif

#if( SWI_AUDIO_IO_SUPPORTED )
 #include "AudioIO.h"   // \cbproj\AudioIO\AudioIO.h
#endif // SWI_AUDIO_IO_SUPPORTED

#ifndef _CHUNK_INFO_H_
 #include "ChunkInfo.h"  // T_ChunkInfo def'd in c:\cbproj\SoundUtl\ChunkInfo.h
#endif

extern int APPL_iWindowsMajorVersion; // 5=Win2003 or XP, 6=Vista or Win7 (!)
extern HWND APPL_hMainWindow; // used in Sound.cpp to launch other applications

CPROT  double TIM_HRTimerValueToUnixDateAndTime(LONGLONG i64HRTimerValue);

#define  C_SOUND_DEVICE_DEFAULT_WAVE_INPUT_DEVICE -1 /* dictated by Windows ! */
#define  C_SOUND_DEVICE_SDR_IQ   -2 /* used in CfgDlgU.cpp since 2011-06 */
#define  C_SOUND_DEVICE_PERSEUS  -3 /* used in CfgDlgU.cpp since 2011-06 */
#define  C_SOUND_DEVICE_AUDIO_IO -4 /* "device number" for DL4YHF's AudioIO-DLL(s) */
#define  C_SOUND_DEVICE_ASIO     -5 /* dummy for ASIO (in this case, the NAME devices which device to use) */


#ifndef GUID_DEFINED
#define GUID_DEFINED
typedef struct _GUID
{                        // taken from BASETYPS.H - just in case its missing !
    unsigned long Data1;
    unsigned short Data2;
    unsigned short Data3;
    unsigned char Data4[8];
} GUID;
#endif /* GUID_DEFINED */


#ifndef _MY_WAVEFORMATEXTENSIBLE_
#define _MY_WAVEFORMATEXTENSIBLE_
typedef struct
{
  WAVEFORMATEX  Format;    /* 18 bytes  [DL4YHF] */
  // Format
  //   Specifies the stream's wave-data format. This member is a structure
  //   of type WAVEFORMATEX.
  // The wFormat member of WAVEFORMATEX should be set to WAVE_FORMAT_EXTENSIBLE.
  // The wBitsPerSample member of WAVEFORMATEX is defined unambiguously
  //   as the size of the container for each sample.
  //   Sample containers are always byte-aligned, and wBitsPerSample
  //   must be a multiple of eight.

  union
  {
    WORD  wValidBitsPerSample;
    WORD  wSamplesPerBlock;
    WORD  wReserved;
  } Samples;         /* + 2 bytes  = 20  [DL4YHF] */
  // Samples
  //    A union containing one of the following three members:
  //    wValidBitsPerSample, wSamplesPerBlock, or wReserved.
  //    These three members are described in the following text.
  //  wValidBitsPerSample
  //    Specifies the precision of the sample in bits.
  //    The value of this member should be less than or equal
  //    to the container size specified in the Format.wBitsPerSample member.
  //    See the following Comments section.
  //  wSamplesPerBlock
  //    Specifies the number of samples contained in one compressed block.
  //    This value is useful for estimating buffer requirements
  //    for compressed formats that have a fixed number of samples
  //    within each block. Set this member to zero if each block
  //    of compressed audio data contains a variable number of samples.
  //    In this case, buffer-estimation and buffer-position information
  //    must be obtained in other ways.
  //  wReserved
  //    Reserved for internal use by operating system. Initialize to zero.

  DWORD  dwChannelMask;  /* + 4 bytes  = 24  [DL4YHF] */
  // dwChannelMask
  //    Specifies the assignment of channels in the multichannel stream
  //    to speaker positions. The encoding is the same as that used
  //    for the ActiveSpeakerPositions member of the KSAUDIO_CHANNEL_CONFIG
  //    structure.
  //    The dwChannelMask member contains a mask indicating which channels
  //    are present in the multichannel stream. The least-significant bit
  //    represents the front-left speaker, the next bit corresponds
  //    to the front-right speaker, and so on.
  //    The following flag bits are defined in the header file ksmedia.h.
  //    ( see SPEAKER_xxxx - defs below ).
  //    The channels that are specified in dwChannelMask should be present
  //    in the order shown in the preceding table, beginning at the top.
  //    For example, if only front-left and front-center are specified,
  //    then front-left and front-center should be in channels 0 and 1,
  //    respectively, of the interleaved stream.


  GUID  SubFormat;    /* + 16 bytes  = 40  [DL4YHF] */
  // SubFormat
  //    Specifies the subformat.
  //    The SubFormat member contains a GUID that specifies the subformat.
  //    The subformat information is similar to that provided by the wave-format
  //    tag in the WAVEFORMATEX structure's wFormatTag member.
  //    The following table shows some typical SubFormat GUIDs
  //    and their corresponding wave-format tags.
  //     SubFormat GUID                          Wave-Format Tag
  //     KSDATAFORMAT_SUBTYPE_PCM                WAVE_FORMAT_PCM
  //     KSDATAFORMAT_SUBTYPE_IEEE_FLOAT         WAVE_FORMAT_IEEE_FLOAT
  //   A common "GUID" example:
  //     static const  GUID KSDATAFORMAT_SUBTYPE_PCM = {
  //     	0x1,0x0000,0x0010,{0x80,0x00,0x00,0xaa,0x00,0x38,0x9b,0x71}

} WAVEFORMATEXTENSIBLE, *PWAVEFORMATEXTENSIBLE;

// Some additional defines which are often necessary when playing with
// "WAVEFORMATEXTENSIBLE" :

#define WAVE_FORMAT_EXTENSIBLE          0xFFFE

#define SPEAKER_FRONT_LEFT              0x00001
#define SPEAKER_FRONT_RIGHT             0x00002
#define SPEAKER_FRONT_CENTER            0x00004
#define SPEAKER_LOW_FREQUENCY           0x00008
#define SPEAKER_BACK_LEFT               0x00010
#define SPEAKER_BACK_RIGHT              0x00020
#define SPEAKER_FRONT_LEFT_OF_CENTER    0x00040
#define SPEAKER_FRONT_RIGHT_OF_CENTER   0x00080
#define SPEAKER_BACK_CENTER             0x00100
#define SPEAKER_SIDE_LEFT               0x00200
#define SPEAKER_SIDE_RIGHT              0x00400
#define SPEAKER_TOP_CENTER              0x00800
#define SPEAKER_TOP_FRONT_LEFT          0x01000
#define SPEAKER_TOP_FRONT_CENTER        0x02000
#define SPEAKER_TOP_FRONT_RIGHT         0x04000
#define SPEAKER_TOP_BACK_LEFT           0x08000
#define SPEAKER_TOP_BACK_CENTER         0x10000
#define SPEAKER_TOP_BACK_RIGHT          0x20000


#endif // ndef _WAVEFORMATEXTENSIBLE_

#ifndef T_Float  /* T_Float defined in SoundTab.h, SoundMaths.h, Sound.h, WaveIO.h, ... */
#if SWI_FLOAT_PRECISION==1
 #define T_Float float
 // 2003-07-07: "float" instead of "double" saves CPU power on stoneage notebooks
#else
 #define T_Float double
#endif
#endif // ndef T_Float



/*----------- Internal Constants & Definitions ----------------------------*/
#define SOUND_MAX_INPUT_BUFFERS  16 // max number of sound input buffers to allocate
#define SOUND_MAX_OUTPUT_BUFFERS 16 // max number of sound output buffers to allocate
  // 2006-01-22: The Soundblaster Audigy2 ZS seemed to cause occasional
  //             "drop-outs" under certain conditions.
  //     Increasing the number of buffers from 3 to 5 did NOT help that much,
  //     in fact the buffer size (8192 at that time) seemed to be too large.
  //     A smaller BUFFER SIZE (4096), but a larger NUMBER OF BUFFERS seemd to help.
  // 2006-02-17: Increased the number of buffers for higher sampling rates,
  //     when occasional loss of samples occurred at 192 kSamples/second .
  //     The number of buffers actually used is now calculated in CSound::InOpen().


//+++++++++++++   WAVEFORMATEX  member variables     +++++++++++++++++++
//typedef struct {
//    WORD  wFormatTag;
//    WORD  nChannels;
//    DWORD nSamplesPerSec;
//    DWORD nAvgBytesPerSec;
//    WORD  nBlockAlign;
//    WORD  wBitsPerSample;
//    WORD  cbSize;
//} WAVEFORMATEX;
//
// For devices with more channels and higher resolution (bits/sample),
//  use a structure named WAVEFORMATEXTENSIBLE .
//  The old WAVEFORMATEX structure is *A PART OF* the newer
//  WAVEFORMATEXTENSIBLE structure .   See definition above .
//


typedef struct // T_SoundSampleProperties
{
   WORD  wChannels;         // 1=mono, 2=stereo; everything else is unlikely
   DWORD dwSamplesPerSec;   // sampling rate, often something like 44100 (Hz)
   WORD  wBitsResolution;   // internal resolution, usually 8, 16, or maybe 24 bits
} T_SoundSampleProperties;

typedef struct // T_SoundBufferInfo
{
   LONGLONG i64HRTimerValue; // .. to determine the timestamp later
   int iBufferIndex;  // index of THIS buffer (0 ... SOUND_MAX_INPUT_BUFFERS-1);
                      // used to find the predecessor in CSound.m_InBufInfo[]
} T_SoundBufferInfo;

class CSound
{
public:
   CSound();
   virtual ~CSound();
   // Public member functions :
   UINT InOpen(
       //ex:int iInputDeviceID,      // Identifies a soundcard, or a replacement like C_SOUND_DEVICE_AUDIO_IO
           char *pszAudioInputDeviceOrDriver, // [in] name of an audio device or DL4YHF's "Audio I/O driver DLL" (full path)
           char *pszAudioInputDriverParamStr, // [in] extra parameter string for driver or Audio-I/O / ExtIO-DLL
           char *pszAudioStreamID,   // [in] 'Stream ID' for the Audio-I/O-DLL, identifies the AUDIO SOURCE (here: optional)
           char *pszDescription,     // [in] a descriptive name which identifies
           // the audio sink ("audio reader", or "consumer"). This name may appear
           // on a kind of "patch panel" on the configuration screen of the Audio-I/O DLL .
           // For example, Spectrum Lab will identify itself as "SpectrumLab1"
           // for the first running instance, "SpectrumLab2" for the 2nd, etc .
           long i32SamplesPerSecond, // nominal sampling rate (not the "true, calibrated value"!)
           int  iBitsPerSample,      // ..per channel, usually 16 (also for stereo, etc)
           int  iNumberOfChannels,   // 1 or 2 channels (mono/stereo)
           DWORD BufSize, DWORD SampleLimit, BOOL start);
   void SetInputSelector( DWORD dwInputSelector );
   void SetOutputSelector(DWORD dwOutputSelector);
   void SetInputTimeout( int iInputTimeout_ms );

   // VFO control (added HERE 2012-08-14, because some ExtIO-DLLs need this):
   BOOL HasVFO(void); // -> TRUE=VFO supported, FALSE=no. Used for ExtIO-DLLs.
   long GetVFOFrequency(void); // -> current VFO tuning frequency in Hertz.
   long SetVFOFrequency( // Return value explained in AudioIO.c !
               long i32VFOFrequency ); // [in] new VFO frequency in Hertz
        // SetVFOFrequency() must be called 'early' - before InOpen() -
        //  because of trouble with StartHW() in certain ExtIO-DLLs !

#if(SWI_ASIO_SUPPORTED)
   AsioHandle GetAsioHandle(void);   // kludge to retrieve more info about "driver-in-use"
#endif // (SWI_ASIO_SUPPORTED)
   BOOL IsInputOpen(void);
   LONG InReadInt16( SHORT* pData, int Length ); // still used by the 'WOLF GUI' (2013)
   int  InReadStereo(
        T_Float* pLeftData,      // [out] samples for 'left' audio channel (or "I")
        T_Float *pRightData,     // [out] samples for 'right' audio channel (or "Q")
        int iNumberOfSamples,    // [in] number of samples for each of the above blocks
        int   iTimeout_ms ,      // [in] max timeout in milliseconds, 0=non-blocking
        int *piHaveWaited_ms,    // [out,optional] "have been waiting here for XX milliseconds"
        T_ChunkInfo *pOutChunkInfo); // [out,optional,(*)] chunk info with timestamp, GPS, calib;
   int  ReadInputSamplePoints( // preferred method since 2011-06
        // Returns the number of sample-POINTS, or a negative error code
        float *pfltDest,            // [out] audio samples, as 32-bit FLOATING POINT numbers, grouped as "sample points"
        int   iNumSamplePoints,     // [in] number of SAMPLE POINTS(!) to read
        int   nChannelsPerSample,   // [in] number of samples PER SAMPLE POINT
        int   iTimeout_ms ,         // [in] max timeout in milliseconds, 0=non-blocking
        int *piHaveWaited_ms,  // [out,optional] "have been waiting here for XX milliseconds"
        T_ChunkInfo *pOutChunkInfo);// [out,optional] chunk info, see c:\cbproj\SoundUtl\ChunkInfo.h, MAY BE NULL(!)

   void InClose(void);
   long   InGetNominalSamplesPerSecond(void);  // -> NOMINAL value (as set in InOpen)
   double InGetMeasuredSamplesPerSecond(void); // -> INTERNALLY MEASURED value (takes a few seconds to stabilize)
   double OutGetMeasuredSamplesPerSecond(void); // similar for the audio-OUTPUT
   int  InGetNumberOfChannels(void);
   int  InGetBitsPerSample(void);

   UINT OutOpen(
       //ex:int iOutputDeviceID,     // Identifies a soundcard, or a replacement like C_SOUND_DEVICE_AUDIO_IO
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
           DWORD BufSize,            // output buffer size; see caller in SoundThd.cpp
           DWORD SampleLimit );
   BOOL IsOutputOpen(void);
   int  GetNumInputBuffers(void);
   int  GetNumSamplesPerInputBuffer(void);
   int  GetInputBufferUsagePercent(void);
   int  GetNumberOfFreeOutputBuffers(void);  // -> 0 ... m_NumOutputBuffers MINUS ONE (!)
   int  GetOutputBufferUsagePercent(void);
   int  OutWriteInt16( SHORT *pi16Data, int Length,
        int iTimeout_ms,      // [in] max timeout in milliseconds, 0=non-blocking
        int *piHaveWaited_ms); // [out,optional] "have been waiting here for XX milliseconds"
   int  OutWriteStereo(    // THIS is the output-function used by Spectrum Lab
        T_Float* pLeftData,   // [in] audio samples, 32-bit FLOATING POINT, range +/- 1.0, left channel or "I"-component
        T_Float *pRightData,  // [in] audio samples, right channel or "Q"-component
        int Length,           // [in] number of samples in each of the above blocks
        int iTimeout_ms,      // [in] max timeout in milliseconds, 0=non-blocking
        int *piHaveWaited_ms,  // [out,optional] "have been waiting here for XX milliseconds"
        T_ChunkInfo *pInChunkInfo); // [in,optional] chunk info with scaling info;
                                    //       see c:\cbproj\SoundUtl\ChunkInfo.h
   int OutWriteFloat( T_Float* pFltSamplePoints,
        int iNumberOfSamplePairs, // [in] number of SAMPLE POINTS(!) to write or send
        int iTimeout_ms,          // [in] max timeout in milliseconds, 0=non-blocking
        int *piHaveWaited_ms,     // [out,optional] "have been waiting here for XX milliseconds"
        T_ChunkInfo *pInChunkInfo); // [in,optional] chunk info with scaling info;
                                    //       see c:\cbproj\SoundUtl\ChunkInfo.h

   int  WriteOutputSamplePoints(  // NOT used by Spectrum Lab
        float *pfltSource,        // [in] audio samples, as 32-bit FLOATING POINT numbers,
                                  //  grouped as "sample points" (N channels PER POINT),
                                  //  value range normalized to +/- 1.0 for full ADC range
        int iNumSamplePoints,     // [in] number of SAMPLE POINTS(!) to write or send
        int nChannelsPerSample,   // [in] number of samples PER SAMPLE POINT
        int iTimeout_ms,          // [in] max timeout in milliseconds, 0=non-blocking
        int *piHaveWaited_ms,     // [out,optional] "have been waiting here for XX milliseconds"
        T_ChunkInfo *pInChunkInfo); // [in,optional] chunk info, see c:\cbproj\SoundUtl\ChunkInfo.h, MAY BE NULL !


   void OutClose(void);

   BOOL GetInputProperties(T_SoundSampleProperties *pProperties);
   BOOL GetOutputProperties(T_SoundSampleProperties *pProperties);

   void Start(void);  // starts input (output only starts if buffer is full enough)
   void Stop(void);   // stops  input and (if active) output
        // Note: Start() and Stop() seem to be MANDATORY for the ExtIO-DLLs !

#if( SWI_AUDIO_IO_SUPPORTED )
   T_AIO_DLLHostInstanceData aio_In, aio_Out;
   BOOL m_ExtIONeedsSoundcardForInput; // TRUE when using ExtIO only to control the radio, but the soundcard to acquire samples
   long m_i32VFOFrequency; // must be set "early" because of trouble with StartHW() in certain ExtIO-DLLs !
#endif // SWI_AUDIO_IO_SUPPORTED

   // Public variables to speed up error handling. Introduced September 2000.
   //        A little dangerous, but timing is the most critical issue here.
   UINT m_ErrorCodeIn;  // These "error codes" can be converted...
   UINT m_ErrorCodeOut; // .. into strings using ErrorCodes::ErrorCodeToString()
   UINT m_LostBuffersIn;  // YHF: Counter for "lost input buffers"..
   UINT m_LostBuffersOut; //      values not important for this class.
   char m_sz255LastErrorStringIn[256];  // introduced 2006-04-16 to debug ASIO calls
   char m_sz255LastErrorStringOut[256];
   int  m_iInputDeviceID, m_iOutputDeviceID; // contains a non-negative device ID (win mm), or C_SOUND_DEVICE_AUDIO_IO, etc etc

   WAVEFORMATEX m_OutFormat;
   WAVEFORMATEX m_InFormat; // now 'public' because needed in ASIO callback (read-only there)
   // Since a WAVEHDR structure is just a few
   //   bytes large (it does NOT CONTAIN THE ACTUAL WAVE BUFFER),
   //   it is useless to allocate them on every OPEN/CLOSE action.
   WAVEHDR m_InputWaveHdr[SOUND_MAX_INPUT_BUFFERS];   // buffer HEADERS..
   WAVEHDR m_OutputWaveHdr[SOUND_MAX_OUTPUT_BUFFERS];
   int  m_NumInputBuffers;  // number of audio buffers allocated for INPUT
   int  m_NumOutputBuffers; // number of audio buffers allocated for OUTPUT
   int  m_iInHeadIndex,   m_iInTailIndex;   // INDICES into m_InputWaveHdr[ 0 .. m_NumInputBuffers-1 ]
      //    m_iInHeadIndex==m_iInTailIndex means NONE of the 'input buffers' is filled.
      // ( (m_iInHeadIndex+1) % m_NumInputBuffers) == m_NumInputBuffers means ALL buffers are full
      //    which indicates a problem (input-buffer is not drained fast enough) .
   int  m_iOutHeadIndex, m_iOutTailIndex;  // INDICES into m_OutputWaveHdr[0 .. m_NumOutputBuffers-1]
      //  Explained in sound.cpp::OutFlushBuffer() !
   BOOL m_InputOpen;       // for user: READ ONLY  (internally used for thread-sync when stopping)
   BOOL m_OutputOpen;      // for user: READ ONLY
   T_SoundBufferInfo m_InBufInfo[SOUND_MAX_INPUT_BUFFERS]; // added 2012-03-17 for timestamped input
   double m_dblInputSRGateTime, m_dblInputSRGateCount;
   double m_dblMeasuredInputSampleRate, m_dblNominalInputSampleRate;
   double m_dblMeasuredOutputSampleRate;
   double m_dbl16MeasuredSampleRates[16]; // non-circular FIFO for averaging
   int    m_nMeasuredSampleRates; // number of valid entries in m_dbl16MeasuredSampleRates[]
   HANDLE m_InEventHandle;
   HANDLE m_OutEventHandle;
   int  m_iUsingASIO;     // 0=not using ASIO, 1=for input, 2=for output, 3=both
   BOOL m_InWaiting;
   BOOL m_InOverflow;
   BOOL m_OutWaiting; // TRUE=waiting for another output buffer to become available (for filling)
   BOOL m_OutUnderflow;
   DWORD m_dwInputSelector;  // channel selection matrix for INPUT (ASIO only)
   DWORD m_dwOutputSelector; // channel selection matrix for OUTPUT (ASIO only)


   // semi-public functions for callback functions only.
   WAVEHDR * GetInputBufHdr(int iBufNrOffset);  // used in ASIO callback
   WAVEHDR * GetOutputBufHdr(int iBufNrOffset);


#define CSOUND_USE_CRITICAL_SECTION 0
#if( CSOUND_USE_CRITICAL_SECTION )
   CRITICAL_SECTION m_CriticalSection; // use *carefully* to prevent threads
#endif                                 // stomping on each other

private:
   BOOL OutFlushBuffer(int iTimeout_ms);
   BOOL WaitForInputData(int iTimeout_ms);
   BYTE *WrapInBufPositionAndGetPointerToNextFragment(int iOptions, T_SoundBufferInfo **ppSoundBufferInfo);
   void GetDateAndTimeForInput(T_SoundBufferInfo *pSoundBufferInfo/*in*/, T_ChunkInfo *pOutChunkInfo/*out*/);
   T_Float *ResizeInterleaveBuffer( int iNumSamplePoints );

#if( SWI_ASIO_SUPPORTED )
   AsioHandle m_hAsio;     // handle for DL4YHF's ASIO wrapper
#endif
#if( SWI_AUDIO_IO_SUPPORTED )
   // ex: BOOL m_fAudioIoDllLoaded;
   // ex: long m_iAudioIoInputHandle, m_iAudioIoOutputHandle;
#endif
   BOOL m_fOutputHoldoff;  // keeps output off until half the buffers are full
   int  m_InBytesTotal;
   int  m_InBytesPerSample;
   int  m_OutBytesPerSample;
   int  m_iInputTimeout_ms; // max. number of milliseconds to wait for input data
                            // (once fixed to 500 ms; user-defineable because
                            //  the ASIO driver for 'Dante' seemed to require more)
   LONG m_InBufferSize;    // bytes in each buffer
   LONG m_OutBufferSize;   // bytes in each buffer
   LONG m_InBufPosition;   // a tiny comment would be very helpful here - WB
           // actually, m_InBufPosition runs from 0 to m_InBufferSize-1
           // so it must be an index into one of these 'buffer fragments'
           // which windoze fills one-by-one through the stupid WAVEHDR-array.
           // (a single, simple, circular FIFO with one writer
           //  and possibly multiple readers would have been MUCH TO EASY..)
           // This 'm_InBufPosition'-thing is INCREMENTED while READING data
           // from the input buffer, more or less byte-wise,
           // so obviously it's not a SAMPLE-INDEX but a BYTE-INDEX .
   LONG m_OutBufPosition;
   DWORD m_InSampleLimit;
   // ex: DWORD m_InSamplesRead;  // wraps around after 6 hours at 192 kS/sec
   LONGLONG    m_i64NumSamplesRead;    // number of samples read by the application
   long double m_ldblSampleBasedInputTimestamp; // in UNIX format; used to provide a jitter-free timestamp
   // double   m_dblMeasuredInputSampleRate, m_dblMeasuredOutputSampleRate;
   // (useless or at least not worth the effort because the 'system time'
   //  may be as bad / jittery as the soundcard's sampling rate itself)

   DWORD m_OutSampleLimit;
   DWORD m_OutSamplesWritten;
   HWAVEIN m_hwvin;
   HWAVEOUT m_hwvout;

   // ex: WAVEFORMATEX m_OutFormat;
   // ex: WAVEFORMATEX m_InFormat;    // now 'public' because needed in ASIO callback !
   // ex: WAVEHDR m_InputWaveHdr[SOUND_MAX_INPUT_BUFFERS];   // buffer HEADERS..
   // ex: WAVEHDR m_OutputWaveHdr[SOUND_MAX_OUTPUT_BUFFERS];
   // ex: BYTE m_InputBuffer[SOUND_NR_INPUT_BUFFERS][SWI_SOUND_MAX_BUFFFER_SIZE];
   // ex: BYTE m_OutputBuffer[SOUND_NR_OUTPUT_BUFFERS][SWI_SOUND_MAX_BUFFFER_SIZE];
   // Switched from the above 'static' buffers to dynamic ones in 2004-05-11
   // for reasons explained somewhere else (something to do with 96kHz sampling rate)
   BYTE *m_pbInputBuffer;  // dynamically allocated for [SOUND_MAX_INPUT_BUFFERS * m_InBufferSize]
                           // Contains INTERLACED audio samples ("grouped" for one sampling point,
                           // which is contrary to the ASIO driver system) .
   BYTE *m_pbOutputBuffer; // dynamically allocated for [SOUND_NR_OUTPUT_BUFFERS * m_OutBufferSize]
                           // Same principle as m_pbInputBuffer .


   T_Float *m_pfltInterleaveBuffer;    // added 2012-06-09 to interleave/de-interleave samples
   long     m_i32InterleaveBufferSize;

}; /* end class CSound */

extern BOOL SOUND_fDumpEnumeratedDeviceNames;  // flag for Sound_InputDeviceNameToDeviceID()

CPROT long SOUND_GetClosestNominalStandardSamplingRate( double dblCalibratedSamplingRate );
CPROT char *Sound_GetAudioDeviceNameWithoutPrefix(
           char *pszNameOfAudioDeviceOrDriver,  // [in] for example "4 E-MU 0202 | USB"
           int  *piDeviceIndex );               // [out] for example 4
CPROT int Sound_InputDeviceNameToDeviceID( char *pszNameOfAudioDevice );
CPROT int Sound_OutputDeviceNameToDeviceID( char *pszNameOfAudioDevice );
CPROT BOOL Sound_OpenSoundcardInputControlPanel( char *pszNameOfAudioDevice );
CPROT BOOL Sound_OpenSoundcardOutputControlPanel( char *pszNameOfAudioDevice );
      // Opens a "volume control panel" for the soundcard, or similar stuff
      // (OS-specific; "SNDVOL32.EXE", "SNDVOL.EXE", or whatever comes next):


#endif /* !defined(_INCLUDE_SOUND_H_) */
