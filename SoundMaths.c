//---------------------------------------------------------------------------
// File  :  SoundMaths.c
// Date  :  2003-06-23   (ISO 8601,  YYYY-MM-DD)
// Author:  Wolfgang Buescher  (DL4YHF)
//
// Description:
//     Some mathmatical subroutines which are frequently used
//     in DL4YHF's "Sound Utilities".
//
// Revision history (YYYY-MM-DD):
//   2003-06-23  Reorganized some modules of Spectrum Lab (result: this module)
//   2005-04-16  Now used in the WOLF GUI for the 'tuning scope' .  
//
// Literature:
//   [SGDSP] = Steven W. Smith,
//          "The Scientists and Engineer's Guide to Digital Signal Processing",
//          Chapter 12, "The Fast Fourier Transform".    www.DSPguide.com .
//          Locally saved by W.B. under c:\Literat1\dspguide\*.pdf .
//
//---------------------------------------------------------------------------

#include <windows.h>
#include <math.h>

#pragma hdrstop   // no precompiled headers after this point

#include "SoundMaths.h"

// #pragma warn -8017
#pragma warn -8004  // .. is assigned a value that is never used (well..)
#pragma warn -8080  // .. is declared but never used (.. so what ?)

/***************************************************************************/
void SndMat_ResampleFloatArray(T_Float *pfltArray, int iSourceLength, int iDestLength )
  // Stretches or shrinks an array.  Originally used for the FFT-based filter,
  // to adapt the frequency response curve when changing the FFT size .
  // Neither iSourceLength nor iDestLength may be zero or negative !
{
  float fltStretchFactor = (float)iSourceLength / (float)iDestLength;
  float fltSourceIndex, fltSrcLeft, fltSrcRight, fltTemp;
  int iDstIdx, iSrcIdx, iStartIdx, iEndIdx, iStep;

  if( iDestLength > iSourceLength  )  // "stretching" (array gets LARGER) :
   {  // begin at the END of the array to avoid overwriting values
      iStartIdx = iDestLength-1;
      iEndIdx   = 0;
      iStep     = -1;
   }
  else // ( iDestLength < iSourceLength ) -> "shrinking" (array gets SMALLER) :
   {  // begin at the START of the array ...
      iStartIdx = 0;
      iEndIdx   = iDestLength-1;
      iStep     = +1;
   }
  for(iDstIdx=iStartIdx; iDstIdx>=0 && iDstIdx<iDestLength; iDstIdx+=iStep)
   { fltSourceIndex = (float)iDstIdx * fltStretchFactor;
     if( fltSourceIndex < 0.0 )
         fltSourceIndex = 0.0;
     if( fltSourceIndex >= iSourceLength )
         fltSourceIndex = iSourceLength-1;
     iSrcIdx = (int)fltSourceIndex;
     fltSrcLeft = pfltArray[iSrcIdx];
     if( (iSrcIdx+1) < iSourceLength)
          fltSrcRight = pfltArray[iSrcIdx+1];
     else fltSrcRight = fltSrcLeft;
     // Interpolate between "left" and "right" value :
     fltTemp = fltSourceIndex - (float)iSrcIdx;  // -> fractional index, 0 .. 0.999999
     fltTemp = fltSrcLeft * (1.0-fltTemp) + fltSrcRight * fltTemp;
     pfltArray[iDstIdx] = fltTemp;
   } // end for(iDstIdx ..

} // end SndMat_ResampleCurve()



/***************************************************************************/
double SndMat_CalculateAngle(double re, double im)
  // Four-quadrant conversion of a complex pair ("I/Q")
  //   into an phase value (in radians, but explained in degrees here).
  // A positive real value gives an angle of zero, etc.
  // Returned value range is -180° .. +180° =  -pi .. pi .
  // If both real and imaginary part are zero, the returned value
  // is zero.
  // Revision history:
  //   Jan 13, 2002, by DL4YHF:
  //       Implemented for a phase-sensitive spectrum analyzer
  //   March 15, 2002:
  //       Copied into the sound thread module
  //   Sept  15, 2002:
  //       Copied into AM_FM_DeMod.cpp (sound utilities)
{
#define ANGLE_RANGE_PLUS_MINUS_180_DEGREES 1  // 1: result_range = -pi..pi = -180°..+180°
     // (-180°..+180° is preferred because angles tend to be +-0.x degrees,
     //  and it looks ugly if the display jumps from "0.1" to "359.9" and back)
  if(im > 0.0)
   {  // first or second quadrant
     if( re > 0.0 )
      { // first quadrant (0..90 degees)
        return atan(im/re);
      }
     else
     if( re < 0.0 )
      { // second quadrant (90..180 degrees)
        return atan(im/re) + C_PI;
      }
     else // re=0, im>0
      {
        return 0.5 * C_PI;
      }
   }
  else // ! im>0
  if(im < 0.0)
   {  // third or fourth quadrant
     if( re < 0.0 )
      { // third quadrant
#if(ANGLE_RANGE_PLUS_MINUS_180_DEGREES)
        return atan(im/re) - C_PI;     // for result range -180..-90°
#else
        return atan(im/re) + C_PI;     // for result range 180..270°
#endif
      }
     else
     if( re > 0.0 )
      { // fourth quadrant
#if(ANGLE_RANGE_PLUS_MINUS_180_DEGREES)
        return atan(im/re);           // for result range -90..0°
#else
        return atan(im/re) + 2*C_PI;  // for result range 270..360°
#endif
      }
     else // re=0, im<0  -> 270 degrees
      {
#if(ANGLE_RANGE_PLUS_MINUS_180_DEGREES)
        return -0.5 * C_PI;
#else
        return 1.5 *  C_PI;
#endif
      }
   }
  else   // im=0, a "real" number
   {
     if(re>=0)
        return 0;
     else
        return C_PI;    // negative -> 180 degrees
   }
} // end ..CalculateAngle()


/***************************************************************************/
T_FAST_FLOAT SndMat_CalculateAngleFast(T_FAST_FLOAT x, T_FAST_FLOAT y)
{ // Fast atan2 calculation with self normalization.
  // Returned value range is  -pi..pi =  -180° .. +180° .
  //
  // Based on an article by Jim Shima, found at
  //       http://www.dspguru.com/comp.dsp/tricks/alg/fxdatan2.htm .
  // The Trick:
  //  compute a self-normalizing ratio depending on the quadrant
  //  that the complex number resides in.
  //  For a complex number z, let x = Re(z) and y = Im(z).
  //
  //  For a complex number in quadrant I (0<=theta<=pi/4), compute the ratio:
  //
  //     x-y
  // r = ---     (1)
  //     x+y
  //
  // To get the phase angle, compute:
  //
  // theta1 = pi/4 - pi/4*r (2)
  //
  // Likewise, if the complex number resides in quadrant II (pi/4<=theta<=3*pi/4),
  // compute the ratio:
  //
  //     x+y
  // r = ---     (3)
  //     y-x
  //
  // And to get the quadrant II phase angle, compute:
  //
  // theta2 = 3*pi/4 - pi/4*r (4)
  //
  // If it turns out that the complex number was really in quad IV
  //  instead of quad I, just negate the answer resulting from (2).
  //
  // Likewise, do the same if the number was in quad III
  // instead of quad II. By doing this, you have a 4-quadrant arctan function.
  //
  // The max error using equations (2) or (4) is a little less than 0.07 rads
  // (only at a few angles though). The accuracy of the estimator is actually
  // quite good considering using a 1st-order polynomial to estimate the phase angle.
  //
  // If you use a higher degree polynomial, it turns out that the even powers
  // of the poly will disappear (due to the odd function), thus relaxing some
  // of the computational load.
  //
#define ATAN2_HIGH_ACCURACY 1
  // FOR BETTER ACCURACY:
  //   To obtain better accuracy (a max error of .01 rads =~ 0.6 degrees),
  //   one can replace equations (2) and (4) with:
  //       theta1 = 0.1963 * r^3 - 0.9817 * r + pi/4   (2a)
  //       theta2 = 0.1963 * r^3 - 0.9817 * r + 3*pi/4 (4a)
  //
  //  Equations (2a) or (4a) can be computed using 2 MACs on a DSP,  // (YHF: hw^3 ?)
  //  which does not involve much more computation for a 7x increase
  //  in accuracy.
  //
  // C code using equations (1)-(4):
  //-----------------------------------------------
  // Fast arctan2
  static T_FAST_FLOAT coeff_1 = C_PI/4;
  static T_FAST_FLOAT coeff_2 = 3*C_PI/4;
  T_FAST_FLOAT r,angle;
 // ex:  T_FAST_FLOAT abs_y = fabs(y)+1e-10;    // kludge to prevent 0/0 condition
  T_FAST_FLOAT abs_y = fabs(y)+1e-30;    // kludge to prevent 0/0 condition, more accurate result

  if (x>=0)
   {
      r = (x - abs_y) / (x + abs_y);         // (equation 1)
#if(ATAN2_HIGH_ACCURACY)
      angle = coeff_1 - 0.9817 * r + 0.1963 * r*r*r; // (2a)
#else
      angle = coeff_1 - coeff_1 * r;         // (equation 2)
#endif // (ATAN2_HIGH_ACCURACY)
   }
  else
   {
      r = (x + abs_y) / (abs_y - x);         // (equation 3)
#if(ATAN2_HIGH_ACCURACY)
      angle = coeff_2 - 0.9817 * r + 0.1963 * r*r*r; // (4a)
#else
      angle = coeff_2 - coeff_1 * r;         // (equation 4)
#endif // (ATAN2_HIGH_ACCURACY)
   }

  if (y < 0)
     return(-angle);     // negate if in quad III or IV
  else
     return(angle);
} // end SndMat_CalculateAngleFast()



/***************************************************************************/
void SndMat_RunComplexFIR( // complex FIR-filter (usually a low pass)
      int       iNrCoeffs,   // Length of filter queue + count of coeffs
      T_Float   *pfltCoeffs, // pointer to filter coefficients   [iNrCoeffs]
      T_Complex *pcpxQueue,  // pointer to filter queue (memory) [iNrCoeffs]
      int       *piQueueIdx, // index for circular filter queue, 0..iNrCoeffs-1
      T_Complex *pcplxValue) // reference to in- and output value
{
 T_Complex acc;
 T_Complex *pQueueEnd = pcpxQueue + iNrCoeffs;
 T_Complex *qptr;
 int j;

   --*piQueueIdx;
   if(*piQueueIdx<0)       // deal with FIR pointer wrap
      *piQueueIdx = iNrCoeffs-1;
   qptr = pcpxQueue/*array*/ + *piQueueIdx/*index*/ ;
   *qptr = *pcplxValue;    // place filter "input"  in circular Queue

   acc.re = 0.0;           // prepare accumulation
   acc.im = 0.0;
   for(j=0; j<iNrCoeffs; ++j )   // do the complex MAC's
    {
     acc.re += ( (qptr->re)*(*pfltCoeffs) );
     acc.im += ( (qptr->im)*(*pfltCoeffs++) );
     if( (++qptr) >= pQueueEnd ) // deal with wraparound
            qptr  =  pcpxQueue;
    }
   // filter output now in acc .
   *pcplxValue = acc;             // re+im back to the caller

} // end SndMat_RunComplexLowpass(..)

/***************************************************************************/
void SndMat_MultiplyHanningWindow( float *pfltArray, int iLength )
{ int i;
  float fltAngle, fltAngleIncr;
   fltAngle = 0.0;
   fltAngleIncr = (2.0 * C_PI) / (float)(iLength-1) ;

   for(i=0; i<iLength; i++) // fill table with FFT WINDOW FUNCTION ..
    {
     pfltArray[i] *= ( .5 - .5*cos(fltAngle) );
     fltAngle += fltAngleIncr;
    }
} // end SndMat_MultiplyHanningWindow()

/***************************************************************************/
void SndMat_CalcComplexFft(
          int iNrOfPoints,    // N =  number of points in the DFT
          float *pfltRe,      // REX[] = real parts of input and output
          float *pfltIm )     // IMX[] = imaginary parts of input and output
 //  THE FAST FOURIER TRANSFORM  - inspired by [SGDSP] TABLE 12-3 or -4 .
 //  Upon entry, N contains the number of points in the DFT, REX[ ] and
 //   IMX[ ] contain the real and imaginary parts of the input.
 //   All signals run from 0 to N-1.
 //  Upon return, REX[ ] & IMX[ ] contain the DFT output.
 //  No cluttered classes, global vars, windowing, averaging and whatsoever.
{
  int I,J,JM1,K,L,M,LE,LE2, IP;
  int NM1 = iNrOfPoints - 1;
  int ND2 = iNrOfPoints / 2;
  float UR, UI, SR, SI, TR, TI;

  // ex: m = CINT(LOG(N%)/LOG(2))
  M = 0; I=iNrOfPoints; while(I>1){ ++M; I = (I>>1); }   // -> m = log2( n )
  J = ND2;

  for(I=1; I<NM1; ++I)                  // Bit reversal sorting
   {
     if(I<J) // 1120   IF I% >= J% THEN GOTO 1190
      { TR = pfltRe[J];
        TI = pfltIm[J];
        pfltRe[J] = pfltRe[I];
        pfltIm[J] = pfltIm[I];
        pfltRe[I] = TR;
       pfltIm[I] = TI;
      }
     K = ND2;    // 1190

     while(K<=J) // 1200   IF K% > J% THEN GOTO 1240
      { J = J - K;
        K = K / 2;
      }          // 1230  GOTO 1200
     J += K;     // 1240   J% = J%+K%
   }             // 1250 NEXT I%

  for( L=1; L<=M; ++L)             // 1270 Loop for each stage
   {
     LE = 1<<L;   // 1280  LE% = CINT(2^L%)
     LE2 = LE/2;  // 1290   LE2% = LE%/2
     UR = 1;
     UI = 0;
     SR = cos(C_PI/(float)LE2);   // Calculate sine & cosine values
     SI = -sin(C_PI/(float)LE2);
     for(J=1; J<=LE2; ++J)        // 1340 Loop for each sub DFT
      { JM1 = J-1;
        for(I=JM1; I<=NM1; I+=LE) // 1360 Loop for each butterfly
         { IP = I+LE2;
           TR = pfltRe[IP]*UR - pfltIm[IP]*UI;  // Butterfly calculation
           TI = pfltRe[IP]*UI + pfltIm[IP]*UR;
           pfltRe[IP] = pfltRe[I]-TR;
           pfltIm[IP] = pfltIm[I]-TI;
           pfltRe[I]  = pfltRe[I]+TR;
           pfltIm[I]  = pfltIm[I]+TI;
         } // NEXT I
        TR = UR;                  // 1450
        UR = TR*SR - UI*SI;
        UI = TR*SI + UI*SR;
      } // NEXT J
   } // NEXT L

} // end SndMat_CalcComplexFft()


/***************************************************************************/
void SndMat_CalcComplexInverseFft(
       int iNrOfPoints, // N  number of points in the IDFT (?) .. IN THE TIME DOMAIN
       float *pfltRe,   // REX[] = input: real parts of frequency domain, result: re(time domain)
       float *pfltIm )  // IMX[] = input: imag. part of frequency domain, result: im(time domain)
 //  INVERSE FFT FOR COMPLEX SIGNALS  - inspired by [SGDSP] TABLE 12-5 .
 //  Upon entry, N contains the number of points in the IDFT, REX[ ] & IMX[]
 //  contain the real & imaginary parts of the complex frequency domain.
 //   Upon return, REX[ ] and IMX[ ] contain the complex time domain.
 //   All signals run from 0 to N-1.
{
  int i;
  float fltFactor;

  for(i=0; i<iNrOfPoints; ++i) //  Change the sign of IMX[ ]
   { pfltIm[i] = -pfltIm[i];
   }

  SndMat_CalcComplexFft(    // Calculate forward FFT
              iNrOfPoints,  // N =  number of points in the DFT
              pfltRe,       // REX[] = real parts of input and output
              pfltIm );     // IMX[] = imaginary parts of input and output

  // Divide the time domain by N and change the sign of IMX[ ] :
  fltFactor = 1.0 / (float)iNrOfPoints;
  for(i=0; i<iNrOfPoints; ++i)
   { pfltRe[i] =  pfltRe[i] * fltFactor;
     pfltIm[i] = -pfltIm[i] * fltFactor;
   }
} // end SndMat_CalcComplexInverseFft()


/***************************************************************************/
void SndMat_CalcRealFft(
          int iNrOfPoints, // number of points in the time domain (input)
          float *pfltRe,   // the real input signal, also used as result (real part)
          float *pfltIm )  // output, imaginary part
 //  FFT FOR REAL SIGNALS  - inspired by [SGDSP] TABLE 12-7 / .
 //  Upon entry, iNrOfPoints contains the number of points in the "DFT" (oh really?!),
 //              pfltRe[0..iNrOfPoints-1] contains the real input signal,
 //              while values in pfltIm[ ] are ignored.
 //              The INPUT signals run from 0 to iNrOfPoints-1  .
 // Upon return, pfltRe[ ] & pfltIm[ ] contain the DFT output.
 //  Note: The output signals run from  0...iNrOfPoints/2 !
 //        A "1024 point REAL FFT" produces 513(!) POINTS in pfltRe[]
 //                                and 513(!) POINTS in pfltIm[] .
 // In contrast to the cluttered 'Cfft' class with its ugly
 // averaging, logarithm, etc,  this is a beautifully simple routine !
{
  int I, IM, IP, IP2, IPM, J,JM1, LE, LE2, NH, NM1, ND2, N4;
  float UR, UI, SR, SI, TR, TI;

  NH = iNrOfPoints/2-1;       // Separate even and odd points
  for(I=0; I<=NH; ++I)
   { pfltRe[I] = pfltRe[2*I];
     pfltIm[I] = pfltRe[2*I+1];
   }

     // N% = N%/2                      'Calculate N%/2 point FFT
     // GOSUB 1000                     '(GOSUB 1000 is the FFT in Table 12-3)
     // N% = N%*2
   SndMat_CalcComplexFft(
             iNrOfPoints / 2, // N =  number of points in the DFT
             pfltRe,          // REX[] = real parts of input and output
             pfltIm );        // IMX[] = imaginary parts of input and output

  NM1 = iNrOfPoints-1 ;      // 3150 Even/odd frequency domain decomposition
  ND2 = iNrOfPoints/2 ;
  N4  = iNrOfPoints/4-1;
  for(I=1; I<=N4; ++I)
   { IM = ND2-I;
     IP2 = I+ND2;
     IPM = IM+ND2;
     pfltRe[IP2] = (pfltIm[I] + pfltIm[IM]) * 0.5;
     pfltRe[IPM] =  pfltRe[IP2];
     pfltIm[IP2] = -(pfltRe[I] - pfltRe[IM]) * 0.5;
     pfltIm[IPM] = -pfltIm[IP2];
     pfltRe[I]   = (pfltRe[I] + pfltRe[IM]) * 0.5;
     pfltRe[IM]  =  pfltRe[I];
     pfltIm[I]   = (pfltIm[I] - pfltIm[IM]) * 0.5;
     pfltIm[IM]  = -pfltIm[I];
   } // 3300 NEXT I%
  pfltRe[iNrOfPoints*3/4] = pfltIm[iNrOfPoints/4];
  pfltRe[ND2] = pfltIm[0];
  pfltIm[iNrOfPoints*3/4] = 0;
  pfltIm[ND2] = 0;
  pfltIm[iNrOfPoints/4] = 0;
  pfltIm[0]   = 0;

  // 3380 : Complete the last FFT stage
    // L  = CINT(LOG(N)/LOG(2));
  LE = 0; I=iNrOfPoints; while(I>1){ ++LE; I=(I>>1); } // -> LE = log2( N )
  LE = 1<<LE;  // LE = CINT(2^LE);

  LE2= LE/2;
  UR = 1;
  UI = 0;
  SR =  cos(C_PI/(float)LE2); // only once per calculation.. no need for an array
  SI = -sin(C_PI/(float)LE2);
  for(J=1; J<=LE2; ++J)
   { JM1 = J-1;
     for( I=JM1; I<=NM1; I+=LE )
      { IP = I+LE2;
        TR = pfltRe[IP]*UR - pfltIm[IP]*UI;
        TI = pfltRe[IP]*UI + pfltIm[IP]*UR;
        pfltRe[IP] = pfltRe[I]-TR;
        pfltIm[IP] = pfltIm[I]-TI;
        pfltRe[I]  = pfltRe[I]+TR;
        pfltIm[I]  = pfltIm[I]+TI;
      } // 3560   NEXT I%
     TR = UR;
     UR = TR*SR - UI*SI;
     UI = TR*SI + UI*SR;
   } // NEXT J%

} // end SndMat_CalcRealFft()


/***************************************************************************/
void SndMat_CalcRealInverseFft(
          int iNrOfPoints, // N  number of points in the IDFT (?!?) .. IN THE TIME DOMAIN
          float *pfltRe,   // REX[] = real parts of frequency domain, AND result
          float *pfltIm )  // IMX[] = imaginary parts of frequency domain
 //  INVERSE FFT FOR REAL SIGNALS  - inspired by [SGDSP] TABLE 12-6 .
 //  Upon entry, N contains the number of points in the IDFT ("time domain" ?) ,
 //  REX[ ] and IMX[ ] contain the real & imaginary parts of the frequency domain
 //  running from index 0 to N%/2.  The remaining samples in REX[] and IMX[]
 //  are ignored. Upon return, REX[ ] contains the real time domain, IMX[ ]
 //  contains zeros. (SET to zero, cannot be used to check the algorithm !)
{
  int K;
  float fltFactor;


  for(K=(iNrOfPoints/2+1); K<iNrOfPoints; ++K)  // Make frequency domain symmetrical
   { pfltRe[K] =  pfltRe[iNrOfPoints-K];        // (as in Table 12-1)
     pfltIm[K] = -pfltIm[iNrOfPoints-K];
   }

  for(K=0; K<iNrOfPoints; ++K)       // Add real and imaginary parts together
   { pfltRe[K] =  pfltRe[K]+pfltIm[K];
   }

  // Calculate forward real DFT (TABLE 12-6, ex: "GOSUB 3000" )
  SndMat_CalcRealFft( // Calculate the REAL FFT ..
          iNrOfPoints, // N  number of points in the DFT (for example 1024 points)
          pfltRe,      // REX[] = the real input signal, also used as result
          pfltIm );    // IMX[] = output, imaginary part (for example 513(!) points)


  // Add real and imaginary parts together and divide the time domain by N
  fltFactor = 1.0 / (float)iNrOfPoints;
  for(K=0; K<iNrOfPoints; ++K)  // see: iNrOfPoints are the number of samples IN THE TIME DOMAIN again !
   {
     pfltRe[K] = (pfltRe[K]+pfltIm[K]) * fltFactor;
     pfltIm[K] = 0; // set IMAGINARY part to zero for the sake of "mathematical correctness"
   }

} // end SndMat_CalcRealInverseFft()

