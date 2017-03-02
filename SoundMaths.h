//---------------------------------------------------------------------------
// File  :  SoundMaths.h
// Date  :  2003-06-23   (ISO 8601,  YYYY-MM-DD)
// Author:  Wolfgang Buescher  (DL4YHF)
//
// Description:
//     Some mathmatical subroutines which are frequently used
//     in DL4YHF's "Sound Utilities".
//
// Revision history (YYYY-MM-DD):
//   2003-06-23  Reorganized some modules of Spectrum Lab (result: this module)
//---------------------------------------------------------------------------

#ifndef  C_PI
 #define C_PI  (3.1415926535897932384626)
#endif

#define T_FAST_FLOAT float

#ifndef  SWI_FLOAT_PRECISION   /* should be defined under Project..Options..Conditionals */
 #define SWI_FLOAT_PRECISION 1 /* 1=single precision, 2=double precision */
#endif

#ifndef T_Float    /* T_Float defined in SoundTab.h, SoundMaths.h, Sound.h, WaveIO.h, ... */
#if SWI_FLOAT_PRECISION==1 /* "float" instead of "double" saves CPU power on stoneage PCs */
 #define T_Float float
#else
 #define T_Float double
#endif
#endif // ndef T_Float


#ifndef _T_COMPLEX_
#define _T_COMPLEX_ 1
typedef struct
{  // T_Float = either 'float' or 'double'
   T_Float re;  // real part or "I" ("in-phase")
   T_Float im;  // imaginary part or "Q" ("quadrature-phase")
} T_Complex;
#endif // _T_COMPLEX_





//**************************************************************************
//    Prototypes (no class methods, no C++, just good old "C")
//**************************************************************************

#ifndef CPROT
// Note on "extern": do not trust the lousy example from Borland !!
//   >extern "c" = wrong (not recognized by Borland C++ V4.0)
//   >extern "C" = right (though the help system says something different)
#ifdef __cplusplus
 #define CPROT extern "C"
#else
 #define CPROT
#endif  /* ! "cplusplus" */
#endif

/***************************************************************************/
CPROT void SndMat_ResampleFloatArray(T_Float *pfltArray, int iSourceLength, int iDestLength );
  // Stretches or shrinks an array.  Originally used for the FFT-based filter,
  // to adapt the frequency response curve when changing the FFT size .
  // Neither iSourceLength nor iDestLength may be zero or negative !



/***************************************************************************/
CPROT double SndMat_CalculateAngle(double re, double im);
  // Precise four-quadrant conversion of a complex pair ("I/Q")
  //   into an phase value (in radians, but explained in degrees here).
  // A positive real value gives an angle of zero, etc.
  // Returned value range is -180° .. +180° =  -pi .. pi .
  // If both real and imaginary part are zero, the returned value
  // is zero.


/***************************************************************************/
CPROT T_FAST_FLOAT SndMat_CalculateAngleFast(T_FAST_FLOAT x, T_FAST_FLOAT y);
  // Fast atan2 calculation with self normalization.
  // Returned value range is  -pi..pi =  -180° .. +180° .
  // Detailed explanation and discussion of accuracy in *.cpp !


/***************************************************************************/
CPROT void SndMat_RunComplexFIR( // complex FIR-filter (usually a low pass)
      int       iNrCoeffs,    // Length of filter queue + count of coeffs
      T_Float   *pfltCoeffs,  // pointer to filter coefficients   [iNrCoeffs]
      T_Complex *pcpxQueue,   // pointer to filter queue (memory) [iNrCoeffs]
      int       *piQueueIdx,  // index for circular filter queue, 0..iNrCoeffs-1
      T_Complex *pcplxValue); // reference to in- and output value

/***************************************************************************/
CPROT void SndMat_MultiplyHanningWindow( float *pfltArray, int iLength );

/***************************************************************************/
CPROT void SndMat_CalcComplexFft(
          int iNrOfPoints,    // N =  number of points in the DFT
          float *pfltRe,      // REX[] = real parts of input and output
          float *pfltIm );    // IMX[] = imaginary parts of input and output
 //  THE FAST FOURIER TRANSFORM
 //  Upon entry, N contains the number of points in the DFT, REX[ ] and
 //   IMX[ ] contain the real and imaginary parts of the input.
 //   All signals run from 0 to N-1.
 //  Upon return, REX[ ] & IMX[ ] contain the DFT output.

/***************************************************************************/
CPROT void SndMat_CalcComplexInverseFft(
       int iNrOfPoints, // N  number of points in the IDFT (?) .. IN THE TIME DOMAIN
       float *pfltRe,   // REX[] = input: real parts of frequency domain, result: re(time domain)
       float *pfltIm ); // IMX[] = input: imag. part of frequency domain, result: im(time domain)
 //  INVERSE FFT FOR COMPLEX SIGNALS  - inspired by [SGDSP] TABLE 12-5 .
 //  Upon entry, N contains the number of points in the IDFT, REX[ ] & IMX[]
 //  contain the real & imaginary parts of the complex frequency domain.
 //   Upon return, REX[ ] and IMX[ ] contain the complex time domain.
 //   All signals run from 0 to N-1.



/***************************************************************************/
CPROT void SndMat_CalcRealFft(
          int iNrOfPoints,    // N  number of points in the DFT
          float *pfltRe,      // REX[] = the real input signal, also used as result
          float *pfltIm );    // IMX[] = output, imaginary part
 //  FFT FOR REAL SIGNALS  - inspired by [SGDSP] TABLE 12-7 .
 //  Upon entry, iNrOfPoints contains the number of points in the DFT,
 //              REX[ ] contains the real input signal,
 //              while values in IMX[ ] are ignored.
 //              These signals run from 0 to iNrOfPoints-1  .
 // Upon return, REX[ ] & IMX[ ] contain the DFT output.
 // ( YHF: The output signals run from  0...iNrOfPoints/2 !
 //   A "1024 point REAL FFT" produces 513(!) POINTS in pfltRe[]
 //                                and 513(!) POINTS in pfltIm[] . )


/***************************************************************************/
CPROT void SndMat_CalcRealInverseFft(
          int iNrOfPoints, // N  number of points in the IDFT (?!?) .. IN THE TIME DOMAIN !
          float *pfltRe,   // REX[] = real parts of frequency domain, AND result
          float *pfltIm ); // IMX[] = imaginary parts of frequency domain
 //  INVERSE FFT FOR REAL SIGNALS  - inspired by [SGDSP] TABLE 12-6 .
 //  Upon entry, N contains the number of points in the IDFT,
 //  REX[ ] and IMX[ ] contain the real & imaginary parts of the frequency domain
 //  running from index 0 to N%/2.  The remaining samples in REX[] and IMX[]
 //  are ignored. Upon return, REX[ ] contains the real time domain, IMX[ ]
 //  contains zeros.


