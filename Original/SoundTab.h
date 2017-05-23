//---------------------------------------------------------------------------
// File  :  SoundTab.cpp
// Date  :  2002-05-05   (yyyy-mm-dd)
// Author:  Wolfgang Buescher  (DL4YHF)
//
// Description:
//     A few TABLES for audio processing.
//
// Revision history :  See *.cpp. Here only the latest mods:
//   2003-07-07  Optional selection of the internal data type
//               via compiler switch "SWI_FLOAT_PRECISION"
//               (with BCB-IDE: Project..Options..Conditionals)
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#ifndef SoundTabH
#define SoundTabH

#ifndef  SWI_FLOAT_PRECISION   // should be defined under Project..Options..Conditionals
 #define SWI_FLOAT_PRECISION 1 /* 1=single precision, 2=double precision */
#endif

#ifndef T_Float  /* T_Float defined in SoundTab.h,  Sound.h, ... */
#if SWI_FLOAT_PRECISION==1
 #define T_Float float
 // 2003-07-07: "float" instead of "double" saves CPU power on stoneage notebooks
#else
 #define T_Float double
#endif
#endif // ndef T_Float



//----------------------- Constants ---------------------------------------

// Length for every DECIMATING FIR filter, used for anti-aliasing
#define SoundTab_DEC_FIR_LENGTH             25

// Size of the cosine table, used for signal generators and frequency converters.
//  A table size of 32768 provides a difference between carrier and
//  "impurities" of roughly 90dB = 20*log10(32768).  Correct..
#define SOUND_COS_TABLE_LEN  32768


// Other 'often needed' stuff..
#ifndef  C_PI
 #define C_PI  (3.1415926535897932384626)
#endif


// 'Complexity' of I/Q- and/or real-value processing routines ....
#define REAL_IN_REAL_OUT      0
#define REAL_IN_COMLEX_OUT    1
#define COMPLEX_IN_REAL_OUT   2
#define COMPLEX_IN_COMLEX_OUT 3



//----------------------- Data Types --------------------------------------

#ifndef _T_COMPLEX_
#define _T_COMPLEX_ 2
typedef struct
{  // T_Float = either 'float' or 'double'
   T_Float re;  // real part or "I" ("in-phase")
   T_Float im;  // imaginary part or "Q" ("quadrature-phase")
} T_Complex;
#endif // _T_COMPLEX_



//------------------------ Global Variables -------------------------------

// Coefficients for a decimate-by-2 and -by-3 - filter...
extern const T_Float SoundTab_Dec2FilterCoeffs[SoundTab_DEC_FIR_LENGTH];

extern const T_Float SoundTab_Dec3FilterCoeffs[SoundTab_DEC_FIR_LENGTH];

// Coefficients for a sharp lowpass with cutoff frequency at f_sample/4, IIR!!
// Used for frequency shifter with WEAVER method for SSB generation.
extern const T_Float SoundTab_HalfbandIIR20Lowpass[2 + 2*20];


// A reasonably long cosine table with a length of ONE period.
//  Used for test signal generators,  frequency converters etc etc.
// Filled with :
//  SOUND_cos_table[i] = cos((T_Float)i * 2.0 * PI / (T_Float)SOUND_COS_TABLE_LEN);
// Note: SOUND_COS_TABLE_LEN is now fixed to 32768, because the same table is
//  used in various "DSP plugins" now - and changing the table size would
//  render those plugins incompatible with their hosts !
extern float SoundTab_fltCosTable[SOUND_COS_TABLE_LEN];


//**************************************************************************
//    Prototypes (no class methods)
//**************************************************************************

void SoundTab_InitCosTable(void); // should be called ONCE in the initialization

void SoundTab_GetNcoParams(
      T_Float dblNcoFreqDivSampleRate, // input : NCO frequency divided by sampling rate
      int    *piSineTableOffset,      // output: offset from cosine- to sine table index
      T_Float *pdblPhzInc );           // output: phase increment value (floating point!!)






#endif // SoundTabH
