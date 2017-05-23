/*--------------------------------------------------------------------------*/
/*   File:   \CBproj\Wolf\SWITCHES.H                                        */
/*   Contains the project specific compilation switches.                    */
/*--------------------------------------------------------------------------*/

#define SWI_PAINTLIB_INCLUDED 0   /* JPEG support               ?          */

#define SWI_ASIO_SUPPORTED    0   /* support for Steinberg's ASIO ?        */

#define SWI_UTILITY1_INCLUDED 1   /* DL4YHF's "utility"-package #1 included ? */

#define SWI_STEREO_SUPPORTED  1   /* Stereophonic input and processing ?   */

#define SWI_SMPORT_INCLUDED   1   /* TSmPort.cpp included ? (good for XP)  */

#define SWI_FFT_RESULT_FLOAT  1   /* 0=INT16 result,   1=FLOAT result      */

#ifndef  SWI_FLOAT_PRECISION   // should be defined under Project..Options..Conditionals
 #define SWI_FLOAT_PRECISION 1 /* 1=single precision, 2=double precision */
#endif

