//-----------------------------------------------------------------------
// c:\cbproj\WOLF\WOLF_GUI1.cpp : Graphic User Interface for WOLF
//             ( Weak-signal Operation on Low Frequency,
//               WOLF-core written by Stewart Nelson, KK7KA )
//
// Author  : Wolfgang Buescher (DL4YHF) 04/2005
// Location: <dl4yhf>C:\CBproj\WOLF\WOLF_GUI1.cpp
// Compiler: Borland C++Builder V4 (Borland-specific stuff only used here in the GUI)
//
//-----------------------------------------------------------------------

#include "switches.h"   // project specific 'switches' for compilation

//---------------------------------------------------------------------------
#include <vcl.h>        // sorry, this is BCB-specific stuff,
#include <IniFiles.hpp> //        cannot be compiled with MS C !
#include <ShellAPI.h>   // using ShellExecute() to launch the soundcard volume control
#include <mmsystem.h>   // .. to enumerate all audio devices
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#pragma hdrstop
#include "ErrorCodes.h"
#include "SoundMaths.h"   // original location: <DL4YHF>c:\CBproj\SoundUtl
#include "SoundTab.h"     // large cosine table, and some filter coefficients
#include "CalcFftPeak.h"  // DF6NM's peak-frequency-detector (using FFT bins)

#ifndef  SWI_UTILITY1_INCLUDED
 #define SWI_UTILITY1_INCLUDED 0
#endif
#if( SWI_UTILITY1_INCLUDED ) /* DL4YHF's "utility"-package #1 included ? */
  #include "utility1.h"  // uses UTL_malloc instead of malloc (for debugging)
#endif

#include "sound.h"     // soundcard stuff and related 'little helpers'

#include "wolfio.h"    // WOLF input/output routines (general header, no VCL in it)
#include "wolf.h"      // WOLF core algorithms, options, etc
#include "WolfThd.h"   // various WOLF WORKER THREADS to send and receive
#include "sigparms.h"  // a few WOLF signal parameters ( FRMMAX, .. )

#include "AAMachine.h"   // Automatic Answering Machine (auto-RX/TX)
#include "StringEditU.h" // string- and number editing dialog (uses VCL)
#include "WOLF_GUI1.h"   // header for THIS module (WOLF Graphic User Interface)

//-------------------------------------------------------------------------
// External references
//-------------------------------------------------------------------------
extern DWORD LinradColors[256];  // default color palette for the waterfall display
extern DWORD SunriseColors[256]; // alternative color palette for the waterfall display

const int crHollowCross=5; // ID for special mouse pointer in MyOwnRes.res (from SpecLab)

//-------------------------------------------------------------------------
// Internal defines and types
//-------------------------------------------------------------------------
#define FREQSCALE_HEIGHT  8      // height of the frequency scale [pixel]
#define AMPLSCALE_WIDTH   8      // width of the amplitude scale [pixel]
#define TSCOPE_MAX_FFT_LENGTH 32768   // max FFT length for the tuning scope

#define POS_TOP       1    // draw a scale on TOP of a diagram
#define POS_BOTTOM    2    // draw a scale on the BOTTOM of a diagram
#define POS_LEFT      4    // draw a scale on the LEFT side of a diagram
#define POS_RIGHT     8    // draw a scale on the RIGHT side of a diagram

  // Columns in the "RX / TX Message" grid
#define SEQ_COL_LINE   0
#define SEQ_COL_RX_MSG 1
#define SEQ_COL_TX_MSG 2
#define SEQ_COL_ACTION 3

//-------------------------------------------------------------------------
// Constants
//-------------------------------------------------------------------------
const char *C_szDefaultAutoSeqMsg[] =
{
   "CQ dxcall K" ,   "dxcall mycall K", "tc=3",
   "mycall dxcall K","dc mycall OO K",  "tc=3",
   "mycall dc oo K", "dc mc RR OO K",   "tc=3",
   "mc dxcall oo K", "dc mc RR OO K",   "tc=3",
   "mc dc RR OO K",  "dc mc TU 73 SK",  "tc=1",
   "mc dc RR 73 K",  "dc mc TU 73 SK",  "tc=1",
   "mycall dc 73 K", "dc mc GL 73 SK",  "tc=1",
   "mc dxcall 73 K", "dc mc GL 73 SK",  "tc=1",
   "mc dc RR 73 K",  "dc mc GL 73 SK",  "tc=1",
   "mc dc TU 73 SK", "73 mycall SK",    "tc=1",
   "mc dc GL 73 SK", "73 mycall SK",    "tc=1",
   "","","", "","","", "","","", "","","",  "","","", "","","", "","","", "","","",
   "","","", "","","", "","","", "","","",  "","","", "","","", "","","", "","","",
};

//-------------------------------------------------------------------------
// Class for the main form (main window) using Borland's VCL
//-------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TWolfMainForm *WolfMainForm;   // a VCL-object: the "main form"




//-------------------------------------------------------------------------
// Variables
//-------------------------------------------------------------------------
    // 'APPL_' = global variables exposed by the main application.
    //           In SpectrumLab, located in the 'splash screen'
    //             (which is executed long before the VCL stuff).
    //           In the 'WOLF GUI', located in WOLF_GUI1.cpp
    //             (because that's the heart of the 'main APPLication')
    //
BOOL APPL_fMayUseASIO = TRUE; // dare to use ASIO ? TRUE=yes, FALSE=no
int  APPL_iWindowsMajorVersion=0; // 0=unknown, 5=Win2003 or XP, 6=Vista or Win7 (!)
int  APPL_iMainFormCreated=0;
int  APPL_iTerminating = 0;  // set by main form when "going out of business"
char APPL_sz255CurrentDirectory[256]="";  // to defeat file selector boxes changing the CWD
char APPL_sz255PathToDataFiles[256] ="";  // something like C:\Data\Spectrum  (no trailing backslash) !
HWND APPL_hMainWindow = NULL; // used for global message processing (also for sockets)
HINSTANCE APPL_hInstance= 0; // handle to current instance, set in WinMain() or similar


double SigParms_dblSymPerSec=10.0; // RAW bitrate (including PN + data, originally = 10.0)
int  WIO_iUseSoundcard = 0; // 0 = no (use audio file),  1 = yes (use soundcard)
int  WIO_iTuneScopeMode= TSMODE_INPUT_WATERFALL;
int  WIO_iOldTuneScopeMode = -1;
float WIO_fltInputLevelPercent=0;    // input level from soundcard, 0..100 %
int  WIO_iRxTxState= WIO_STATE_OFF;  // WIO_STATE_xxx
int  WIO_iTxWhat   = WIO_TX_DATA;    // WIO_TX_DATA ,  WIO_TX_CW_ID , or WIO_TX_TUNE
int  WIO_iOldRxTxState = -1;         // for LED indicator, polled in GUI via timer
int  WIO_iQsoMode;  // 0=unlimited TX- (or RX-) period
char WIO_sz2kDisplayLineBuffer[8][2048];
WORD WIO_wDispBufHead=0, WIO_wDispBufTail=0;  // thread-safe FIFO for message lines
int  WIO_iExitCode = 0; // 0 = everything ok,  everything else: error, EXIT-code
BOOL WIO_fStopFlag = FALSE; // FALSE = continue processing, TRUE=request to finish a.s.a.p.

       // To pass the received message text from the DECODER THREAD to the auto-RX/TX-machine :
char WIO_sz80RcvdMessage[84]; // received message text, only 15 chars for WOLF, but who knows...
int  WIO_iRcvdMessageCount;   // shall be incremented *AFTER* rcvd message has been set.

  // Some 'global' vars for the user interface:
double WGUI_dbldBmin = -100;  // min. magnitude for the tuning scope
double WGUI_dbldBmax = 0;     // max. magnitude for the tuning scope
double WGUI_dblCurrPeakFreq=0;
int    WGUI_iScopeRes= 1;     // 0=low, 1=medium, 2=high frequency resolution
int    WGUI_iColorPalNr=0;    // 0=use LINRAD, 1=SUNRISE, 2=SUNSET color palette

  // An instance of the "Automatic Answering Machine" :
T_AAMachine MyAnsweringMachine;


//-------------------------------------------------------------------------
// Character-based output functions for the WOLF decoder :
//  (must not use any Borland-specific stuff in the interface,
//   the VCL [Visual Component Library] is only used "internally" .
//-------------------------------------------------------------------------

/***************************************************************************/
void WIO_printf(char * fmt, ... )
  /* Prints a formatted string (like printf) to the WOLF DECODER OUTPUT WINDOW.
   *  May use an internal buffer to make it thread-safe,
   *  because this function may be called from a worker thread !
   */
{
 va_list params;
 char c, sz255Temp[256], *pszSrc, *pszDst;
 va_start( params, fmt );
 vsprintf( sz255Temp, fmt, params );
 // in console: printf(sz255Temp);
 // Here: Append the characters to the RichText control on the "main screen".
 // Caution: WIO_printf produces no visible output until the line is COMPLETE,
 //          which means the new line character.
 //          Furthermore, this routine may be called from a WORKER THREAD
 //          so we must not call anything from the user interface,
 //          and should avoid VCL (because the VCL is not thread-safe).
 pszSrc = sz255Temp;
 WIO_wDispBufHead &= 0x0007;  // keep buffer index in legal range
 pszDst = WIO_sz2kDisplayLineBuffer[WIO_wDispBufHead];  // thread-safe FIFO for message lines
 pszDst += strlen(pszDst);
 while( (c=*pszSrc++)!=0 )
  { if(c=='\n') // next output line complete -> flush it to the RichEdit control
     { WIO_wDispBufHead = (WORD)((WIO_wDispBufHead+1) & 0x0007);
       pszDst = WIO_sz2kDisplayLineBuffer[WIO_wDispBufHead];
       *pszDst = '\0';
     }
    else
     { *pszDst++ = c;
     }
    *pszDst = '\0';
  }

} // end WIO_printf()

/***************************************************************************/
void ShowStatus(char * fmt, ... )
{
 va_list params;
 char sz255Temp[256];
 va_start( params, fmt );
 vsprintf( sz255Temp, fmt, params );
 WolfMainForm->Lab_Status->Caption = sz255Temp;
 WolfMainForm->Update();
}


/***************************************************************************/
void WIO_exit(int status)
  /* Replacement for exit(), but gives control back to the (optional)
   *  user interface instead of terminating immediately.
   * For the console application, WIO_exit(N) does alsmost the same as exit(N)
   *  but may wait for a few seconds before closing the console window.
   */
{
  Sleep( 2000 );    // suspends execution for N milliseconds

  if( WIO_iExitCode == 0 )
      WIO_iExitCode = status;

  // ... insert additional "cleanup" code for the user interface here !

  WIO_fStopFlag = TRUE;  // flag for WIO_MustTerminate(), to break from all loops and subroutines !

  // exit( status );
} // end WIO_exit()


/***************************************************************************/
BOOL WIO_MustTerminate(void)
  /* Crude solution for a POLITE thread-termination :
   *   TRUE means: "all subroutines return asap, but clean everything up" .
   */
{
  return WolfThd_fTerminateRequest || WIO_fStopFlag ;
} // end WIO_MustTerminate()



//--------------------------------------------------------------------------
//  End of NON-VCL-STUFF .
//  Next: Helper routines, possibly using Borland-specific things ..  :-(
//--------------------------------------------------------------------------


/****************** Routines outside TDebugForm *****************************/
extern "C" void DEBUG_EnterErrorHistory(
       int  debug_level,   // importance of the message (to be entered or not)
        int  options,       // reserved for future extensions
     double time_of_occurrence,  // fractional GMT SECONDS since Jan.1970 ,
                           // may be UTL_USE_CURRENT_TIME
       char *message_fmt,  // pointer to null-terminated format string (source)
                   ... )   // optional argument list (printf-style)
  // CAUTION ! THIS ROUTINE MAY BE CALLED FROM SOME "WORKER THREAD" !
  // (thus it must not use VCL-stuff, because Borland's VCL isn't thread-safe)
{ // Just implemented as a DUMMY (in contrast to Spectrum Lab..)
} // end EnterErrorHistory

/***************************************************************************/
extern "C" void DEBUG_Breakpoint( void ) // common breakpoint for all "self detected errors"
  // Only called through macro DEBUGGER_BREAK() in 'beta releases' !
  // Replaces assert()-stuff .  When running under debugger control,
  // set a breakpoint ON THE LAST CURLY BRACE of the function body .
  // Then use "return to caller", if possible .
{
  // ex: geninterrupt(0x03);      // int 3   ->  debugger break (always)
  // removed; geninterrupt(3) crashed the application when running w/o debugger
} // end DEBUG_Breakpoint() .  ALWAYS SET A BREAKPOINT HERE ! <<<<<<<<<<<<<<<


//---------------------------------------------------------------------------
int LimitInteger( int iValue, int iMin, int iMax )
{
  if( iValue < iMin ) return iMin;
  if( iValue > iMax ) return iMax;
  return iValue;
}

/***************************************************************************/
int FirstStrToIntDef( AnsiString s, int deflt )
  /* Similar to "StrToIntDef()".
   * Ignores any junk after the first token.
   * Very useful for combo boxes with leading numerical values.
   */
{
 int ret;
 if( s.Length()>0 )
   {
     // Note on SubString: Beware of the bloody index of "first char" -
     //    hey folks, this is C and not PASCAL ....
     s = s.SubString(  1,  s.AnsiPos(" ")-1 );
     ret = StrToIntDef( s, deflt);
   } else ret = deflt;
  return ret;
}


//---------------------------------------------------------------------------
BOOL PhysValueToScreen(
        T_ScaleParams * pScaleParams, // drawing area + frequency range
        double dblValue, int *piScreenCoord )
{
  double d;  int i;
  if( pScaleParams->iPosition==POS_RIGHT || pScaleParams->iPosition==POS_LEFT )
   { // vertical scale   ( Y=0 for MAX value)
     d = (dblValue - pScaleParams->dblValMin) * (double)(pScaleParams->iBottom - pScaleParams->iTop)
       / ( pScaleParams->dblValMax - pScaleParams->dblValMin );
     i = pScaleParams->iBottom - (int)(d + 0.5);
     if( i > pScaleParams->iBottom )
      { *piScreenCoord = pScaleParams->iBottom;
        return FALSE;  // clipped !
      }
     else if ( i < pScaleParams->iTop )
      { *piScreenCoord = pScaleParams->iTop;
        return FALSE;  // clipped !
      }
     else
      { *piScreenCoord = i;
        return TRUE;
      }
   }
  else // horizontal scale (X=0 for MIN value)
   { d = (dblValue - pScaleParams->dblValMin) * (double)(pScaleParams->iRight - pScaleParams->iLeft)
       / ( pScaleParams->dblValMax - pScaleParams->dblValMin );
     i = pScaleParams->iLeft + (int)(d + 0.5);
     if( i < pScaleParams->iLeft )
      { *piScreenCoord = pScaleParams->iLeft;
        return FALSE;  // clipped !
      }
     else if ( i > pScaleParams->iRight )
      { *piScreenCoord = pScaleParams->iRight;
        return FALSE;  // clipped !
      }
     else
      { *piScreenCoord = i;
        return TRUE;
      }
   }
}

//---------------------------------------------------------------------------
BOOL ScreenPosToPhysValue(
        T_ScaleParams * pScaleParams, // drawing area + frequency range
        int iScreenCoord/*X or Y*/ ,  double *pdblValue )
{
  double d;
  if( pScaleParams->iPosition==POS_RIGHT || pScaleParams->iPosition==POS_LEFT )
   { // vertical scale   ( Y=0 for MAX value)
     d = pScaleParams->dblValMax - (double)(iScreenCoord - pScaleParams->iTop)
           * (pScaleParams->dblValMax - pScaleParams->dblValMin)
           / (double)(pScaleParams->iBottom - pScaleParams->iTop);
   }
  else
   { // horizontal scale ( X=0 for MIN value)
     d = pScaleParams->dblValMin + (double)(iScreenCoord - pScaleParams->iLeft)
           * (pScaleParams->dblValMax - pScaleParams->dblValMin)
           / (double)(pScaleParams->iRight - pScaleParams->iLeft);
   }

  if( pdblValue )
     *pdblValue = d;
  return ( d>=pScaleParams->dblValMin && d<=pScaleParams->dblValMax );
}


//---------------------------------------------------------------------------
void ScrollBitmapDown( Graphics::TBitmap *pBmp,
                          RECT *rc, int by_n_lines,long lBackgndColor )
   // Scrolls the "Plotter" bitmap  down  by a few pixels .
   //     The scrolled region is
   //     between x1=rc->left and x1=rc->right ,
   //         and y1=rc->top  and y2=rc->bottom  (INCLUSIVE!).
   //     After scrolling, a strip at the right border
   //     will be erased (between x = rc->right - by_n_lines
   //                      and    x = rc->right )
{
 TRect src_rect, dst_rect;
 TCanvas *canvas = pBmp->Canvas;

  // Copy lines from source to destination shifted left by n lines.
  // Note: TCanvas::CopyRect() uses a funny kind of
  //    "bounding rectangle", so use 1 pixel more than obvious !
  src_rect.Left = rc->left;
  src_rect.Right= rc->right + 1;  // because of "bounding rectangle"
  src_rect.Top  = rc->top;
  src_rect.Bottom= rc->bottom + 1 - by_n_lines;
  dst_rect.Left = src_rect.Left;
  dst_rect.Right= src_rect.Right;
  dst_rect.Top  = src_rect.Top + by_n_lines;
  dst_rect.Bottom=rc->bottom + 1;
  canvas->CopyMode = cmSrcCopy;
  canvas->CopyRect( dst_rect, canvas, src_rect );
  // Erase the new horizontal strip up to, and including, x=rc->top .
  // From help on "FillRect" :
  //  > Use FillRect to fill a rectangular region using the current brush.
  //  > The region is filled including the top and left sides of the rectangle,
  //  > but excluding the bottom and left(?!?!right!) edges.
  // Better Info from the Win32SDK help about "FillRect",
  //      which possibly behaves similar...
  // > Remarks
  // > When RECT is passed to the FillRect function, the rectangle
  // > is filled up to, but not including, the right (!a-ha!) column and
  // > bottom row of pixels.
  // So is this what Borland's "bounding rect" is all about ?
  canvas->Brush->Color = (TColor)lBackgndColor;
  dst_rect = Rect( rc->left,  rc->top ,   // top
                   rc->right+1,           // right,  INCLUDING(!!) x2
                   rc->top+by_n_lines);   // bottom, INCLUDING(!!) y2
  canvas->FillRect(dst_rect);
} // end ScrollBitmapDown()

//---------------------------------------------------------------------------
void BitmapFade( Graphics::TBitmap *pBmp, int i3RGBFadeDelays[3],
                 int xmin, int ymin, int xmax, int ymax )
  // Make all pixels about 10% darker than they were before.
  // Borrowed from Spectrum Lab's tuning scope .
{
 int x,y,c,i;
 BYTE *bp;

  // For speed reasons, FORGET THE OBVIOUS "Pixels[][]"-property !
  switch( pBmp->PixelFormat )
   {
     case pf24bit:
        for (y = ymin; y <= ymax; y++)
         { bp = (BYTE *)pBmp->ScanLine[y] + 3*xmin;
           for (x = xmin; x<= xmax; ++x)
            { for(c=0; c<3; ++c) // 3 byte per pixel
               {
                 i = (int)(*bp);
                 i = i - i/i3RGBFadeDelays[c] - 1;    // quasi-exponential decay (not infinite)
                 if(i<0) i=0;
                 *(bp++) = (BYTE)i;
               }
            }
         }
        break;
     case pf32bit:
        for (y = ymin; y <= ymax; y++)
         {
           bp = (BYTE *)pBmp->ScanLine[y] + 4*xmin;
           for (x = xmin; x<= xmax; ++x)
            { for(c=0; c<3; ++c) // 4 byte per pixel, but only 3 colour components
               {
                 i = (int)(*bp);
                 i = i - i/i3RGBFadeDelays[c] - 1;    // quasi-exponential decay (not infinite)
                 if(i<0) i=0;
                 *(bp++) = (BYTE)i;
               }
              ++bp;  // skip the "alpha" channel (or whatever this 4th byte is for)
            }
         }
        break;
     default:    // bad luck, unsupported "pixel format" !
        break;
   } // end switch( offscreen_bmp->PixelFormat )
}

//---------------------------------------------------------------------------
void ComplexFftBinsToDecibels(
        int iNrOfBinsInFft, float *pfltXr, float *pfltXi,  // source: FFT bins
        float *pfltDecibels, int iNrOfBinsToConvert ) // destination buffer (may overlap source array)
{
  double f;

  // Conversion of FFT frequency bins into into power dB :
  // A pure input sin wave ... Asin(wt)... will produce an fft output
  //   peak of (N*A/4)^2  where N is FFT_SIZE.
  //   [ Furthermore, it depends on the FFT windowing function ! ]
  double fltFftMaxAmpl = (float)iNrOfBinsInFft * 32768.0;

  for(int iBinIndex=0; iBinIndex<iNrOfBinsToConvert; ++iBinIndex)
   {
     f = sqrt(  pfltXr[iBinIndex] * pfltXr[iBinIndex]
              + pfltXi[iBinIndex] * pfltXi[iBinIndex] ); // -> VOLTAGE
     if( f>0 )  // convert into dB (logarithmic intensity scale)
          f = 20 * log10(  f / fltFftMaxAmpl );  // -> dB, 0=clipping point
     else f = -200;
     pfltDecibels[iBinIndex] = f;
   }
} // end


//---------------------------------------------------------------------------
void DrawWaterfallLine(
        Graphics::TBitmap *pBmp,       // pointer to destination bitmap
        int iMaxNrOfValues, float *pfltDecibels ) // source: decibel values
{
  int  iWidth = pBmp->Width;
  BYTE bRed,bGreen,bBlue;
  BYTE *pbRGB;    // pointer to destination (8 bit per R,G,B component)
  int   iPixelIndex;
  float f;
  DWORD dw, *pdwColorPalette;

  switch( WGUI_iColorPalNr )
   { case 1:  pdwColorPalette = SunriseColors;
              break;
     default: pdwColorPalette = LinradColors;
              break;
   }

  // Emergency brake for the display range :
  if( WGUI_dbldBmin >= WGUI_dbldBmax )
   {  WGUI_dbldBmin=-100; WGUI_dbldBmax=0; }

  // Accessing the bitmap's pixels via ScanLine[] is decades faster
  //         than using the old "Pixel[][]"-property of TCanvas.
  //  Here, only the 32-bit pixel format is supported  !
  if( pBmp->PixelFormat != pf32bit )
     return;
  pbRGB = (BYTE*) pBmp->ScanLine[0];
  for( iPixelIndex=0; iPixelIndex<iWidth ; iPixelIndex++ )
   {
     if( iPixelIndex < iMaxNrOfValues )
      {
        // Scale the dB-value into 0...255 as index into the colour palette :
        f = 255.0 * ( pfltDecibels[iPixelIndex] - WGUI_dbldBmin ) / ( WGUI_dbldBmax - WGUI_dbldBmin );
        if( f<0 )  f=0;
        if( f>255) f=255;
        dw = pdwColorPalette[(int)f];
        bRed  = (BYTE) dw;
        bGreen= (BYTE)(dw>>8);
        bBlue = (BYTE)(dw>>16);
      }
     else
      { bRed=bGreen=bBlue=0;
      }

     *pbRGB++ = bBlue;
     *pbRGB++ = bGreen;
     *pbRGB++ = bRed;
     *pbRGB++ = 0;

   } /* end for(iPixelIndex..) */

} /* end DrawWaterfallLine() */
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
void DrawSpectrumGraph(
        Graphics::TBitmap *pBmp,      // pointer to destination bitmap
        T_ScaleParams *pFreqScale,    // horz. drawing area + frequency range
        T_ScaleParams *pAmplScale,    // vert. drawing area + amplitude range
        int iMaxNrOfValues, float *pfltDecibels ) // source: decibel values
{
  int  iScreenX, iScreenY;
  int  iWidth = pBmp->Width;
  int   iPixelIndex;

  pBmp->Canvas->Brush->Color = clBlack;
  pBmp->Canvas->Pen->Color   = clWhite;

  for( iPixelIndex=0; iPixelIndex<iWidth && iPixelIndex<iMaxNrOfValues ; iPixelIndex++ )
   { iScreenX = iPixelIndex + pFreqScale->iLeft;

     // Scale the dB-value into a screen position within the drawing area:
     PhysValueToScreen( pAmplScale, pfltDecibels[iPixelIndex], &iScreenY );
     if( iPixelIndex<= 0 )
          pBmp->Canvas->MoveTo( iScreenX, iScreenY  );
     else pBmp->Canvas->LineTo( iScreenX, iScreenY  );
     
   } /* end for(iPixelIndex..) */

} /* end DrawSpectrumGraph() */
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
void DrawLine(Graphics::TBitmap *pBmp, int x1, int y1, int x2, int y2 )
{
  pBmp->Canvas->MoveTo( x1, y1 );
  pBmp->Canvas->LineTo( x2, y2 );
}

//---------------------------------------------------------------------------
void DrawScale(  // draws a scale, vertical or horizontal, on any side of a diagram
              Graphics::TBitmap *pBmp, // pointer to destination bitmap
              T_ScaleParams *pSP,      // drawing area + physical value range
              double *pdblMarkerValues, int iNrOfMarkers )  // values to be marked on the scale
{
  double dblValue, dblValueStep;
  int i, iScreenCoord, iSize;
  int iXLeft, iXRight, iYTop, iYBot;
  BOOL fHorScale;

  pBmp->Canvas->Brush->Color = clBlack;
  pBmp->Canvas->Pen->Color = clWhite;

  // erase old stuff (which may include frequency markers, etc)
  pBmp->Canvas->FillRect( Rect(pSP->iLeft, pSP->iTop, pSP->iRight+1/*!*/, pSP->iBottom+1/*!*/ ) );

  switch( pSP->iPosition ) // draw baseline and get a few other params..
   { case POS_TOP :
          iSize = pSP->iBottom - pSP->iTop;
          iYTop = LimitInteger( pSP->iBottom - 2 - iSize/4/*best*/, pSP->iTop+2/*min*/, pSP->iBottom/*max*/ );
          iYBot = pSP->iBottom;
          DrawLine(pBmp, pSP->iLeft, pSP->iBottom, pSP->iRight, pSP->iBottom );
          fHorScale = TRUE;
          break;
     case POS_BOTTOM:
          iSize = pSP->iBottom - pSP->iTop;
          iYTop = pSP->iTop;
          iYBot = LimitInteger( pSP->iTop + 2 + iSize/4/*best*/, pSP->iTop+2/*min*/, pSP->iBottom/*max*/ );
          DrawLine(pBmp, pSP->iLeft, pSP->iTop, pSP->iRight, pSP->iTop );
          fHorScale = TRUE;
          break;
     case POS_LEFT :
          iSize = pSP->iRight - pSP->iLeft;
          iXLeft = LimitInteger( pSP->iRight - 2 - iSize/4, pSP->iLeft+1, pSP->iRight-1);
          iXRight= pSP->iRight;
          DrawLine(pBmp, iXRight, pSP->iTop, iXRight, pSP->iBottom );
          fHorScale = FALSE;
          break;
     case POS_RIGHT:
          iSize = pSP->iRight - pSP->iLeft;
          iXLeft = pSP->iLeft;
          iXRight= LimitInteger( pSP->iLeft + 2 + iSize/4, pSP->iLeft+1, pSP->iRight-1);
          DrawLine(pBmp, iXLeft, pSP->iTop, iXLeft, pSP->iBottom );
          fHorScale = FALSE;
          break;
   }

  // Draw a few ticks... approx 10...50 pixel per step
  dblValueStep = ( pSP->dblValMax - pSP->dblValMin );
  dblValueStep = 10.0;   // cries for improvement !
  dblValue = pSP->dblValMin;
  dblValue -= fmod( dblValue, dblValueStep );
  while(dblValue<=pSP->dblValMax)
   { if( PhysValueToScreen( pSP, dblValue, &iScreenCoord ) )
      { if( fHorScale )     DrawLine( pBmp,  iScreenCoord, iYTop, iScreenCoord, iYBot+1/*!*/ );
        else /*VertScale*/  DrawLine( pBmp,  iXLeft, iScreenCoord, iXRight+1/*!*/, iScreenCoord );
      }
     dblValue += 10.0;
   }

  // draw extra frequency marker (if visible) :
  for( i=0; i<iNrOfMarkers; ++i)
   {
    if( PhysValueToScreen( pSP, pdblMarkerValues[i], &iScreenCoord ) )
     {
       if( fHorScale )     DrawLine( pBmp,  iScreenCoord, pSP->iTop, iScreenCoord, pSP->iBottom+1/*!*/ );
       else /*VertScale*/  DrawLine( pBmp,  pSP->iLeft, iScreenCoord, pSP->iRight+1/*!*/, iScreenCoord );
     }
   }

} // end DrawScale()




//--------------------------------------------------------------------------
//  Implementation of the user interface, uses lots of Borland-specific things
//--------------------------------------------------------------------------



//---------------------------------------------------------------------------
__fastcall TWolfMainForm::TWolfMainForm(TComponent* Owner)
        : TForm(Owner)
{

  // Code to detect other instances of this program.
  // A mutex object is used because it can be detected by all applications
  //  (it's a very "global" object). The system closes its handle
  // automatically when the process terminates.
  // The mutex object is destroyed when its last handle has been closed.
  // Must be executed before any other child window is created....
  WIO_InstanceNr = 0;  // Instance counter. 0 = "i am the FIRST instance" etc
  ::CreateMutex( NULL, TRUE, "WOLFgui1" );
  if( GetLastError() == ERROR_ALREADY_EXISTS)
    {
      WIO_InstanceNr = 1;  // 1 = "I AM THE SECOND INSTANCE" (array index!!)
      ::CreateMutex( NULL, TRUE, "WOLFgui2" );
      if( GetLastError() == ERROR_ALREADY_EXISTS)
        WIO_InstanceNr = 2; // 2 = I AM THE THIRD OR ANY OTHER INSTANCE !
    } // end if <mutex of the FIRST instance of SpecLab already exists>



}

//---------------------------------------------------------------------------
void __fastcall TWolfMainForm::MI_QuitClick(TObject *Sender)
{
  Close();
}


//---------------------------------------------------------------------------
void __fastcall TWolfMainForm::UpdateConfig(void)
{
  WAVEINCAPS my_waveincaps;
  WAVEOUTCAPS my_waveoutcaps;
  int i,n_devs,n_good_devs;

  //--------- build a list of available AUDIO INPUT DEVICES ------------------
  n_devs = waveInGetNumDevs();
  CB_SoundInputDevice->Items->Clear();   // "A windows API function failed" ?! (2005-08-09, Win98)
  CB_SoundInputDevice->ItemIndex = -1;
  for(n_good_devs=0,i=0; i<n_devs; ++i)
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
            | WAVE_FORMAT_48M16
            | WAVE_FORMAT_48S16
#ifdef WAVE_FORMAT_96S16        /* not defined in Borland's headers ! */
            | WAVE_FORMAT_96S16 // 96 kHz, stereo, 16 bit (upper end of the scale)
#endif
            | WAVE_FORMAT_1M08 // 11.025 kHz, mono, 8 bit (lower end of the scale)
           )  )
       // Note: the ugly "WiNRADIO Virtual Soundcard" thingy reported dwFormats=0,
       //       so accept the device also if the "number of channels" is non-zero:
          || (my_waveincaps.wChannels > 0 )  // added 2007-03-06 for "VSC"
        )
       {
        ++n_good_devs;
        CB_SoundInputDevice->Items->Add( my_waveincaps.szPname );
        if( strcmp( WolfThd_sz255SoundInputDevice, my_waveincaps.szPname )==0 )
         { CB_SoundInputDevice->ItemIndex=CB_SoundInputDevice->Items->Count-1;
         }
       }
     }
    else  // waveInGetDevCaps failed on this device :
     {
      CB_SoundInputDevice->Items->Add("#"+IntToStr((int)i)+": ????");
     }
   }
  CB_SoundInputDevice->Items->Add("-1 (use default WAVE input)");
  if(CB_SoundInputDevice->ItemIndex < 0  )
   { CB_SoundInputDevice->ItemIndex = CB_SoundInputDevice->Items->Count-1;
   }

  //--------- Build a list of available AUDIO OUTPUT DEVICES -----------------
  n_devs = waveOutGetNumDevs();
  CB_SoundOutputDevice->Items->Clear();
  CB_SoundOutputDevice->ItemIndex = -1;
  for(n_good_devs=0,i=0; i<n_devs; ++i)
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
#endif
             | WAVE_FORMAT_1M08 // 11.025 kHz, mono, 8 bit (lower end of the scale)
           ) )
       // Note: the ugly "WiNRADIO Virtual Soundcard" thingy reported dwFormats=0,
       //       so accept the device also if the "number of channels" is non-zero:
         || (my_waveincaps.wChannels > 0 )  // added 2007-03-06 for "VSC"
        )
       {
        ++n_good_devs;
        CB_SoundOutputDevice->Items->Add( my_waveoutcaps.szPname );
        if( strcmp( WolfThd_sz255SoundOutputDevice, my_waveoutcaps.szPname )==0 )
         { CB_SoundOutputDevice->ItemIndex=CB_SoundOutputDevice->Items->Count-1;
         }
       }
     }
    else  // waveInGetDevCaps failed on this device :
     {
      CB_SoundOutputDevice->Items->Add("#"+IntToStr((int)i)+": ????");
     }
   }
  CB_SoundOutputDevice->Items->Add("-1 (use default WAVE output)");
  if(CB_SoundOutputDevice->ItemIndex < 0)
   { CB_SoundOutputDevice->ItemIndex=CB_SoundOutputDevice->Items->Count-1;
   }


} // end TWolfMainForm::UpdateConfig()

//---------------------------------------------------------------------------
void __fastcall TWolfMainForm::ApplyConfig(int iRxTxState)
{
 int i;

  WIO_fltOutputAttenuation = strtod( Ed_OutputAttenuation->Text.c_str(), NULL);
  if( WIO_fltOutputAttenuation < 0 )
      WIO_fltOutputAttenuation = -WIO_fltOutputAttenuation;
  WIO_dblTxSampleRate = strtod( Ed_TxSampleRate->Text.c_str(), NULL);
  WIO_dblRxSampleRate = strtod( Ed_RxSampleRate->Text.c_str(), NULL);
  if( iRxTxState == WIO_STATE_TX )
   {
     WIO_dblCurrCenterFreq = strtod( Ed_TxFcenter->Text.c_str(), NULL);
     WIO_dblCurrSampleRate = WIO_dblTxSampleRate;
   }
  else // WIO_STATE_RX  ..
   {
     WIO_dblCurrCenterFreq = strtod( Ed_RxFcenter->Text.c_str(), NULL);
     WIO_dblCurrSampleRate = WIO_dblRxSampleRate;
   }
  WIO_SetAudioSampleRate( WIO_dblCurrSampleRate );
  if( WIO_iComPortNr != CB_ComPortNr->ItemIndex)
   {  WIO_iComPortNr = CB_ComPortNr->ItemIndex;
      WIO_CloseComPort();   // close "old" COM port (if opened)
      if( WIO_iComPortNr > 0 )
          WIO_OpenComPort(WIO_iComPortNr);
   } // end if < COM port number has changed >


  // Test/Debugging stuff for the WOLF implementation :
  WIO_iAddRxNoiseSwitch  = Chk_AddNoiseOnRx->Checked;
  WIO_dblAddRxNoiseLevel = strtod( Ed_AddedNoiseLevel->Text.c_str(), NULL );

  // Strings defined for the "answering machine" :
  AAM_SetTxText( &MyAnsweringMachine, Ed_TxString->Text.c_str() );
  AAM_SetMacroText( &MyAnsweringMachine, "mycall", Ed_MyCall->Text.c_str(), NULL, NULL );
  AAM_SetMacroText( &MyAnsweringMachine, "dxcall", Ed_DxCall->Text.c_str(), NULL, NULL );
  for(i=0; (i+1)<SG_RxTxMsgs->RowCount && i<AAM_MAX_MESSAGES; ++i)
   {
     strncpy(MyAnsweringMachine.defs[i].sz80RxMsg, SG_RxTxMsgs->Cells[SEQ_COL_RX_MSG][i+1].c_str(), 80 );
     strncpy(MyAnsweringMachine.defs[i].sz80TxMsg, SG_RxTxMsgs->Cells[SEQ_COL_TX_MSG][i+1].c_str(), 80 );
     strncpy(MyAnsweringMachine.defs[i].sz80Action,SG_RxTxMsgs->Cells[SEQ_COL_ACTION][i+1].c_str(), 80 );          
   }
  UpdateCaption();     // show new baudrate in title bar
}


//---------------------------------------------------------------------------
void __fastcall TWolfMainForm::MI_AboutClick(TObject *Sender)
{
 char sz2kTemp[2048];

 sprintf(sz2kTemp,
   "WOLF : Weak-signal Operation on Low Frequency\n\n"\
   "WOLF Version %s by Stewart Nelson, KK7KA\n"\
   "  http://www.scgroup.com/ham/wolf.html\n\n"\
   "User Interface by Wolfgang Büscher, DL4YHF\n"\
   "  http://www.qsl.net/d/dl4yhf/wolf/index.html\n"\
   "Compiled: %s,  %s\n\n"\
   "Preliminary manual in html\\index.html .\n",
   (char*)VERSION, (char*)__DATE__, (char*)__TIME__ );
 MessageBox(
    Handle,         // handle of owner window
    sz2kTemp,       // lpText = address of text in message box
    "About WOLF..", // lpCaption = address of title of message box
    MB_OK /* | MB_ICONINFORMATION */ );    // uType = style of message box
}


//---------------------------------------------------------------------------
void __fastcall TWolfMainForm::BuildRxCommandLine( char *pszWaveFileName )
    // Build an 'internal' command line for the WOLF CORE  to RECEIVE
    // Example: wolf -q xmgr.wav -b 14000 -l -f 805.495 -t .1 -w -.000046 -r 8000.06
{
 int  i;

   WolfThd_iArgCnt = 0;   // here in BuildRxCommandLine() ...
   for(i=0; i<WOLF_MAX_ARGS; ++i)   // make an array of "pointers to strings":
    { WolfThd_szArg[i][0] = '\0';
      WolfThd_pszArgs[i] = WolfThd_szArg[i];
    }

   strcpy(WolfThd_szArg[WolfThd_iArgCnt++], "wolf" ); // dummy for application line in argv[0]
   if( pszWaveFileName )
    { strcpy(WolfThd_szArg[WolfThd_iArgCnt++], "-q" );
      strcpy(WolfThd_szArg[WolfThd_iArgCnt++], pszWaveFileName );
    }
   strcpy(WolfThd_szArg[WolfThd_iArgCnt++], "-r" );
   strcpy(WolfThd_szArg[WolfThd_iArgCnt++], Ed_RxSampleRate->Text.c_str() );
   strcpy(WolfThd_szArg[WolfThd_iArgCnt++], "-f" );
   strcpy(WolfThd_szArg[WolfThd_iArgCnt++], Ed_RxFcenter->Text.c_str() );
   if( Ed_TxPhaseRevTime->Text != "" )
    { strcpy(WolfThd_szArg[WolfThd_iArgCnt++], "-t" );  // here: -t = TOLERANCE
      strcpy(WolfThd_szArg[WolfThd_iArgCnt++], Ed_FreqToler->Text.c_str() );
    }
   if( UD_NrRxFrames->Position != FRMMAX ) // max number of frames to process ( makes sense for soundcard ! )
    { strcpy(WolfThd_szArg[WolfThd_iArgCnt++], "-d" );
      strcpy(WolfThd_szArg[WolfThd_iArgCnt++], IntToStr(UD_NrRxFrames->Position).c_str() );
    }
   if( Ed_SlewRate->Text != "" )
    { strcpy(WolfThd_szArg[WolfThd_iArgCnt++], "-w" );
      strcpy(WolfThd_szArg[WolfThd_iArgCnt++], Ed_SlewRate->Text.c_str() );
    }
   i = ApplySpecialOptions();
   if(i != 0)
    { strcpy(WolfThd_szArg[WolfThd_iArgCnt++], "-o" );
      sprintf(WolfThd_szArg[WolfThd_iArgCnt++], "%o", (int)i );  /* octal !! */
    }
   if( CHK_VerboseMsgs->Checked )
    { strcpy(WolfThd_szArg[WolfThd_iArgCnt++], "-v" );
    }
   if( CHK_ShowTimeOfDay->Checked )
    { strcpy(WolfThd_szArg[WolfThd_iArgCnt++], "-ut" );
    }

} // end TWolfMainForm::BuildRxCommandLine()

//---------------------------------------------------------------------------
void __fastcall TWolfMainForm::BeginReception( void )
{
   // build command line and launch WOLF-thread(s)
   ShowBusyLed(WIO_STATE_B4RX);  // show YELLOW LED while WOLF is thinking (GUI blocked)
   WolfThd_TerminateAndDeleteThreads(); // terminate worker threads (if any)
   WIO_SetComRTS( FALSE );       // Turn off RequestToSend - here used as PTT (sic!)
   ApplyConfig( WIO_STATE_RX );  // -> WIO_dblCurrCenterFreq, WIO_dblCurrSampleRate, ...
   PageControl1->ActivePage = TS_MainScreen;  // switch to main screen with "decoder" output
   WolfThd_fExitedThread = WolfThd_fExitedThread;  // TEST for debugger
   WIO_iUseSoundcard = 1;        // let WOLF.CPP use the soundcard routines, not a wave-file
   BuildRxCommandLine( NULL );   // NO wave file as input -> use soundcard
   if( WolfThd_LaunchWorkerThreads(WOLFTHD_AUDIO_RX) )
    { ShowStatus( "Decoding audio from %s", WolfThd_sz255SoundInputDevice );
      WIO_iRxTxState = WIO_STATE_RX;
    }
   else
    { ShowStatus( "Error: Couldn't launch WOLF WORKER THREAD");
      WIO_iRxTxState = WIO_STATE_OFF;
    }
   WolfThd_fExitedThread = WolfThd_fExitedThread;  // TEST for debugger
} // end TWolfMainForm::BeginReception()


//---------------------------------------------------------------------------
void __fastcall TWolfMainForm::BuildTxCommandLine( char *pszWaveFileName )
    // Build an 'internal' command line for the WOLF CORE  to TRANSMIT

{
 int  i;

   WolfThd_iArgCnt = 0; // here in BuildTxCommandLine() ...
   for(i=0; i<WOLF_MAX_ARGS; ++i)   // make an array of "pointers to strings":
    { WolfThd_szArg[i][0] = '\0';
      WolfThd_pszArgs[i] = WolfThd_szArg[i];
    }

   // > In transmit mode, there are some other options you may wish to use. For example:
   // > wolf -x "QUICK BROWN FOX" -e -r 7999.876 -f 812.345 -a 0 -t 1
   // >   The 'e' option causes the file to be written with 8 bit samples.
   // >   The 'r' option specifies the actual sample rate of the transmitting
   // >        sound card. This may be needed for on the air testing, because
   // >        the program expects the bit rates to match within a few PPM.
   // >   The 'f' option specifies the center frequency of the output.
   // >        The program alters the sample rate and/or frequency slightly,
   // >        so that there are an integral number of samples and carrier
   // >        cycles in a frame.
   // >   The 'a' option (on transmit only) specifies the output level
   // >        attenuation, in dB, relative to full scale. If not used,
   // >        the level is -8 dB normally, or 0 dB for '-k' mode.
   // >   The 't' option (on transmit only) specifies the transition time
   // >        for a phase reversal, in bit times. The value must be between
   // >        zero and one, inclusive. If not used, a transition takes
   // >        about 0.1 bit times.
   // >        Larger 't' values greatly reduce the required spectrum,
   // >        at some expense in received S/N. For the same PEP,
   // >        't 1' is about 1.5 dB worse than 't 0'.
   strcpy(WolfThd_szArg[WolfThd_iArgCnt++], "wolf" ); // dummy for application line in argv[0]
   if( pszWaveFileName )
    { strcpy(WolfThd_szArg[WolfThd_iArgCnt++], "-q" );
      strcpy(WolfThd_szArg[WolfThd_iArgCnt++], pszWaveFileName );
    }
   strcpy(WolfThd_szArg[WolfThd_iArgCnt++], "-x" );
   strcpy(WolfThd_szArg[WolfThd_iArgCnt++], Ed_TxString->Text.c_str() );
   if( RB_8BitWave->Checked )
       strcpy(WolfThd_szArg[WolfThd_iArgCnt++], "-e" );
   strcpy(WolfThd_szArg[WolfThd_iArgCnt++], "-r" );
   strcpy(WolfThd_szArg[WolfThd_iArgCnt++], Ed_TxSampleRate->Text.c_str() );
   strcpy(WolfThd_szArg[WolfThd_iArgCnt++], "-f" );
   strcpy(WolfThd_szArg[WolfThd_iArgCnt++], Ed_TxFcenter->Text.c_str() );
   strcpy(WolfThd_szArg[WolfThd_iArgCnt++], "-a" );
   strcpy(WolfThd_szArg[WolfThd_iArgCnt++], Ed_OutputAttenuation->Text.c_str() );
   if( Ed_TxPhaseRevTime->Text != "" )
    { strcpy(WolfThd_szArg[WolfThd_iArgCnt++], "-t" );
      strcpy(WolfThd_szArg[WolfThd_iArgCnt++], Ed_TxPhaseRevTime->Text.c_str() );
    }
   if( CHK_VerboseMsgs->Checked )
    { strcpy(WolfThd_szArg[WolfThd_iArgCnt++], "-v" );
    }

} // end TWolfMainForm::BuildTxCommandLine()

//---------------------------------------------------------------------------
void __fastcall TWolfMainForm::BeginTransmission( void )
{
   ShowBusyLed(WIO_STATE_B4TX);         // show YELLOW LED while "preparing"
   ShowStatus( "Generating WOLF message.." );
   WolfThd_TerminateAndDeleteThreads(); // terminate worker threads (if any)
   WolfThd_fExitedThread = false;
   ApplyConfig( WIO_STATE_TX );  // -> WIO_dblCurrCenterFreq, WIO_dblCurrSampleRate, ...
   WOLF_InitDecoder();    // prepare another decoder run (reset all vars to defaults)
   WIO_iUseSoundcard = 1; // let WOLF.CPP use the soundcard routines, not a wave-file
   BuildTxCommandLine( NULL );   // NO wave file as input -> use soundcard
     // A bit different than other digital transmission modes :
     //  - WOLF only calculates a single block of samples ONCE .
     //  - The worker thread then sends the audio block in an endless loop.
     // So: First let WOLF calculate the audio samples...
   WIO_ClearAudioBuffer();
   WIO_dblStartTime = time(NULL); // Start time (for WOLF decoder), UNIX-like UTC-seconds
   WOLF_Main( WolfThd_iArgCnt, WolfThd_pszArgs ); // produce audio samples for SOUNDCARD (-buffer)
     // WIO_printf("Filled TX-buffer for soundcard, %6.3f samples/sec., %6.3f Hz\n", samprate, freq);
   WIO_printf("Filled TX-buffer for soundcard, %ld samples, SR=%.1lf Hz, BufUsage=%3.1lf %%\n",
       (long)WolfThd_dwAudioBufHeadIndex, (double)WIO_dblCurrSampleRate,
       (double)100.0 * ( (double)WolfThd_dwAudioBufHeadIndex / (double)WolfThd_dwAudioBufferSize)
     );
     // After generating the samples, let a worker thread play them in a loop.
     // The number of samples produced depends on WOLF's baudrate, see WOLF.C .
     // For debugging: watch the number of audio samples in WolfThd_dwAudioBufHeadIndex .
     //   For "normal" WOLF (10 sym/sec), it should be 96sec*8000samp/sec = 768000 .
     //   For "slow" WOLF (5 sym/sec), it should be 2*96sec*8000samp/sec = 1536000 .
     //   Furthermore, WolfThd_dwAudioBufferSize must be > WolfThd_dwAudioBufHeadIndex .
     //   The sample buffer was allocated in WolfThd_AdjustAudioBufferSizeIfRequired(),
     //   depending on sample rate and other parameters .
     //
   WIO_SetComRTS( TRUE );         // set RequestToSend == activate PTT (sic!)
   if( WolfThd_LaunchWorkerThreads(WOLFTHD_AUDIO_TX_LOOP) )
    { ShowStatus( "Sending audio to %s", WolfThd_sz255SoundOutputDevice );
      WIO_iRxTxState = WIO_STATE_TX;
      WIO_iTxWhat    = WIO_TX_DATA;
    }
   else
    { ShowStatus( "Error: Couldn't launch WOLF WORKER THREAD");
      WIO_SetComRTS( FALSE );   // turn PTT off again
      WIO_iRxTxState = WIO_STATE_OFF;
    }
}
//---------------------------------------------------------------------------



//---------------------------------------------------------------------------
void __fastcall TWolfMainForm::MI_DecodeWAVClick(TObject *Sender)
{

  WIO_SetComRTS( FALSE );      // turn PTT off

  /* Ask for the filename ( see help on Borland's TOpenDialog) */
  OpenDialog1->Title    = "Decode WAVE file";
    // To defeat the silly default directory "Own Files"
    // where Billy wants us to put everything :
  OpenDialog1->InitialDir = ExtractFilePath( Application->ExeName );
  OpenDialog1->DefaultExt = "WAV";
  OpenDialog1->FileName = sigf.name;  // often "wolfr.wav"
  OpenDialog1->Filter = "Wave audio files (*.wav)|*.WAV";
  OpenDialog1->Options.Clear();
  OpenDialog1->Options << ofFileMustExist << ofHideReadOnly << ofNoChangeDir
                       << ofShowHelp;
  // OpenDialog1->HelpContext = HELPID_DIGIMODE_SETTINGS;
  if (OpenDialog1->Execute())
   {
     ShowBusyLed(WIO_STATE_B4RX); // show YELLOW LED while WOLF is thinking (with GUI blocked)

     // Because WOLF uses several global variables,
     //  stop the worker thread before telling it 'what to do next' :
     WolfThd_TerminateAndDeleteThreads();
     ApplyConfig( WIO_STATE_RX ); // -> WIO_dblCurrCenterFreq, WIO_dblCurrSampleRate, ...

     // Build an "internal" command line for the former main()-function in WOLF.CPP :
     BuildRxCommandLine( OpenDialog1->FileName.c_str() );

     PageControl1->ActivePage = TS_MainScreen;  // switch to main screen with "decoder" output
     if( WolfThd_LaunchWorkerThreads(WOLFTHD_WAVE_ANALYZER) )
      {    ShowStatus( "Decoding WAVE file - press ESCAPE to stop");
           WIO_iRxTxState = WIO_STATE_RX;
      }
     else
      {    ShowStatus( "Error: Couldn't launch WOLF WORKER THREAD");
           WIO_iRxTxState = WIO_STATE_OFF;
      }
   }
}

//---------------------------------------------------------------------------
void __fastcall TWolfMainForm::MI_EncodeWAVClick(TObject *Sender)
{
  WIO_SetComRTS( FALSE );      // turn PTT off

  /* Get the filename. See help on TSaveDialog */
  SaveDialog1->Title    = "Encode WAVE file";
  SaveDialog1->InitialDir = ExtractFilePath( Application->ExeName );
  SaveDialog1->DefaultExt = "WAV";
  SaveDialog1->FileName = "wolfx.wav";
  SaveDialog1->Filter = "Wave audio files (*.wav)|*.WAV";
  SaveDialog1->Options.Clear();
  SaveDialog1->Options << ofNoChangeDir << ofShowHelp;
  // SaveDialog1->HelpContext = HELPID_DIGIMODE_SETTINGS;
  if( SaveDialog1->Execute() )
   {
     ShowBusyLed(WIO_STATE_B4TX); // show YELLOW LED while WOLF is thinking (with GUI blocked)

     // Because WOLF uses several global variables,
     //  stop the worker thread before telling it 'what to do next' :
     WolfThd_TerminateAndDeleteThreads();
     ApplyConfig( WIO_STATE_TX ); // -> WIO_dblCurrCenterFreq, WIO_dblCurrSampleRate, ...

     // Build an 'internal' command line...
     WIO_iUseSoundcard = 0; // let WOLF.CPP produce a wave-file
     BuildTxCommandLine( SaveDialog1->FileName.c_str() );

     // Switch to the "decoder" tab before starting :
     PageControl1->ActivePage = TS_MainScreen;
     if( WolfThd_LaunchWorkerThreads(WOLFTHD_WAVE_PRODUCER) )
      {    ShowStatus( "Generating WAVE file - press ESCAPE to stop");
           WIO_iRxTxState = WIO_STATE_TX;
           WIO_iTxWhat    = WIO_TX_DATA;
      }
     else
      {    ShowStatus( "Error: Couldn't launch WOLF WORKER THREAD");
           WIO_iRxTxState = WIO_STATE_OFF;
      }
   }
}
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
void __fastcall TWolfMainForm::UpdateLevelIndicator(float fltInputLevelPercent)
{
 Graphics::TBitmap *pBmp = Img_InputLevel->Picture->Bitmap;
 int iLevelY;
  pBmp->Height = Img_InputLevel->Height;
  pBmp->Width  = Img_InputLevel->Width;
  if( fltInputLevelPercent > 100.0 )
      fltInputLevelPercent = 100.0;
  if( fltInputLevelPercent < 0.0 )
      fltInputLevelPercent = 0.0;
  iLevelY = pBmp->Height - (int)( (fltInputLevelPercent * (float)pBmp->Height-1) / 100.0 );
  pBmp->Canvas->Brush->Color = clGreen;
  pBmp->Canvas->FillRect((Rect(0,iLevelY,pBmp->Width/*not "-1" !*/, pBmp->Height ) ) );
  pBmp->Canvas->Brush->Color = clGray;
  pBmp->Canvas->FillRect((Rect(0,0, pBmp->Width, iLevelY ) ) );
} // end UpdateLevelIndicator()

//---------------------------------------------------------------------------
void __fastcall TWolfMainForm::ClearTuningScope(void)
{
 Graphics::TBitmap *pBmp = Img_Tuning->Picture->Bitmap;
  pBmp->Height = Img_Tuning->Height;
  pBmp->Width  = Img_Tuning->Width;
  pBmp->Canvas->Brush->Color = clBlack;
  pBmp->Canvas->FillRect((Rect(0,0,pBmp->Width/*not "-1" !*/, pBmp->Height ) ) );
  if( OffscreenBitmap )
   { pBmp = OffscreenBitmap;
     pBmp->Canvas->Brush->Color = clBlack;
     pBmp->Canvas->FillRect((Rect(0,0,pBmp->Width/*not "-1" !*/, pBmp->Height ) ) );
   }
}

//---------------------------------------------------------------------------
void __fastcall TWolfMainForm::GetTuningScopeParams( int *piNrFFTBins, int *piFirstFFTBinIndex, T_ScaleParams *pFSParams, T_ScaleParams *pASParams )
   // Decides where to put the frequency scale, and sets the transformation parameters
   // Pointers may be NULL if parameters are not required by the caller .
{
 int iNrOfBins;  // number of FFT frequency bins for the tuning scope
 int iFreqScaleWidth, iFirstBinIndex;
 double d, dblBinWidthHz;  // width of the 2nd, 3rd, etc FFT frequency bin in Hertz(!)
 T_ScaleParams f_scale_params, a_scale_params;

  switch( WGUI_iScopeRes )
   { case 2:  iNrOfBins = TSCOPE_MAX_FFT_LENGTH / 2; break;
     case 1:  iNrOfBins = TSCOPE_MAX_FFT_LENGTH / 4; break;
     case 0:
     default: iNrOfBins = TSCOPE_MAX_FFT_LENGTH / 8; break;
   }
  if( piNrFFTBins )
     *piNrFFTBins = iNrOfBins;

  // Define the graphic area for the frequency scale:
  iFreqScaleWidth = Img_Tuning->Width;  // width in pixel (for waterfall; limited for graph below)
  f_scale_params.iScaleLengthPixels = iFreqScaleWidth;
  f_scale_params.iLeft = 0;
  f_scale_params.iTop  = 0;
  f_scale_params.iRight  = iFreqScaleWidth-1;
  f_scale_params.iBottom = FREQSCALE_HEIGHT-1;
  f_scale_params.iPosition = POS_TOP;    // for waterfall: scale on TOP of the diagram
  if( WIO_iTuneScopeMode == TSMODE_INPUT_SPECTRUM )
   { // leave some space for the AMPLITUDE scale on the right side:
     iFreqScaleWidth = Img_Tuning->Width - AMPLSCALE_WIDTH;
     f_scale_params.iScaleLengthPixels = iFreqScaleWidth;
     f_scale_params.iRight  = iFreqScaleWidth-1;
     f_scale_params.iPosition = POS_BOTTOM;  // for waterfall: scale on TOP of the diagram
     f_scale_params.iTop    = Img_Tuning->Height - FREQSCALE_HEIGHT;
     f_scale_params.iBottom = Img_Tuning->Height - 1;
   }


  // Get the 1st FFT bin index to center the display .
  //   The WOLF center frequency shall be in the middle (if possible ! ) .
  // Center the display, so the WOLF center frequency is in the middle
  //   of the tuning scope :
  dblBinWidthHz = WIO_dblCurrSampleRate / (2.0 * (double)iNrOfBins);
  if( dblBinWidthHz < 1e-30 )
      dblBinWidthHz = 8000.0 / (2.0 * (double)iNrOfBins);
  d = WIO_dblCurrCenterFreq - 0.5*((double)iFreqScaleWidth * dblBinWidthHz);  // freq1 for display
  iFirstBinIndex = (int)( d / dblBinWidthHz );
  if( iFirstBinIndex > (iNrOfBins-iFreqScaleWidth) )
      iFirstBinIndex = (iNrOfBins-iFreqScaleWidth);
  if( iFirstBinIndex < 0 )
      iFirstBinIndex = 0;
  // Init frequency-scale transformation params:
  f_scale_params.dblValMin = ((double)iFirstBinIndex-0.5)*dblBinWidthHz;
  f_scale_params.dblValMax = ((double)(iFirstBinIndex+iFreqScaleWidth)-0.5)*dblBinWidthHz;
     // "-0.5" because the 1st FFT bin is only half as wide as the rest

  if( piFirstFFTBinIndex )
     *piFirstFFTBinIndex = iFirstBinIndex;

  //--- now for the AMPLITUDE scale ...
  // Define the graphic area for the AMPLITUDE scale:
  a_scale_params.iPosition = POS_RIGHT;
  a_scale_params.iLeft = Img_Tuning->Width - AMPLSCALE_WIDTH;
  a_scale_params.iRight= Img_Tuning->Width - 1;
  if( f_scale_params.iPosition == POS_TOP )
   { // if freq-scale on TOP of the diagram, ampl scale moves lower
     a_scale_params.iTop    = f_scale_params.iBottom+1;
     a_scale_params.iBottom = Img_Tuning->Height-1;
   }
  else
   { // freq-scale below of the diagram, ampl-scale moves higher
     a_scale_params.iTop    = 0;
     a_scale_params.iBottom = f_scale_params.iTop-1;
   }
  a_scale_params.iScaleLengthPixels = 1 + a_scale_params.iBottom - a_scale_params.iTop;
  a_scale_params.dblValMin = WGUI_dbldBmin;  // min. magnitude for the tuning scope
  a_scale_params.dblValMax = WGUI_dbldBmax;  // max. magnitude for the tuning scope

  if(pFSParams)  *pFSParams = f_scale_params;
  if(pASParams)  *pASParams = a_scale_params;

} // end GetTuningScopeParams()


//---------------------------------------------------------------------------
void __fastcall TWolfMainForm::UpdateTuningScope(
        float *pfltXr, float *pfltXi ) // source: complex FFT bins
{
 Graphics::TBitmap *pBmp = Img_Tuning->Picture->Bitmap;
 int  iNrFFTBins, iFirstBinIndex;
 TRect rcSrc, rcDst;
 T_ScaleParams fscale, ascale;  // frequency- and amplitude scale params
 double dblBinWidthHz, dblPeakAmplitude, dblPeakFrequency;


  pBmp->Height = Img_Tuning->Height;
  pBmp->Width  = Img_Tuning->Width;
  pBmp->Canvas->Brush->Color = clBlack;

  // Get a few scaling parameters for the tuning scope :
  GetTuningScopeParams( &iNrFFTBins, &iFirstBinIndex, &fscale, &ascale );
  if( iNrFFTBins<=0 ) return;
  dblBinWidthHz = WIO_dblCurrSampleRate / (2.0 * (double)iNrFFTBins);

  rcDst = Rect(0,0, pBmp->Width, pBmp->Height ); // default TARGET drawing area
  if( WIO_iOldTuneScopeMode != WIO_iTuneScopeMode )
   { // redraw "everything" because the display mode was changed
     WIO_iOldTuneScopeMode = WIO_iTuneScopeMode;
     pBmp->Canvas->FillRect( rcDst );
   }

  // Convert the COMPLEX spectrum into "power" decibels (only as many as required)
  ComplexFftBinsToDecibels(
        iNrFFTBins, pfltXr+iFirstBinIndex, pfltXi+iFirstBinIndex, // source: total number of FFT bins, complex input
        pfltXr,   // destination: dB values in an array of FLOATs
        fscale.iScaleLengthPixels); // number of frequency bins to convert (1 pixel per bin !)

  // Detect the peak frequency and -amplitude, using DF6NM's smart subroutine..
  CalcFftPeakAmplAndFrequency(
        pfltXr, fscale.iScaleLengthPixels, // source: complex FFT bins, max nr of bins(!)
        0, fscale.iScaleLengthPixels-1,    // search range: index
        &dblPeakAmplitude, &dblPeakFrequency); // -> peak in dB,  peak freq offset (bins)
  dblPeakFrequency = (dblPeakFrequency + (double)iFirstBinIndex) * dblBinWidthHz;
  WGUI_dblCurrPeakFreq = dblPeakFrequency; // save as default for the SR calibrator
  Lab_PkInfo1->Caption = FormatFloat("###0.000 'Hz'", dblPeakFrequency );
  Lab_PkInfo2->Caption = FormatFloat( "##0.0 'dB'",   dblPeakAmplitude );

  // Draw the tuning scope (mode-dependent)
  switch( WIO_iTuneScopeMode )
   {
     case TSMODE_INPUT_WATERFALL :
      { // Use an "offscreen"-bitmap instead of drawing into the visible bitmap:
        OffscreenBitmap->Height= 1;
        OffscreenBitmap->Width = pBmp->Width;

        // Draw the frequency scale (on top of the waterfall)
        DrawScale( pBmp, &fscale, &WIO_dblCurrCenterFreq,1 );

        // Scroll the waterfall area down by one pixel .
        rcDst = Rect(0, FREQSCALE_HEIGHT, pBmp->Width-1, pBmp->Height-1 );
        ScrollBitmapDown(pBmp, &rcDst, 1/*1 pixel*/,  0x000000/*color*/ ) ;

        // Draw a single waterfall line into an "offscreen" bitmap
        DrawWaterfallLine( OffscreenBitmap, // pointer to destination bitmap
             fscale.iScaleLengthPixels, pfltXr ); // source: decibel values
        rcDst = Rect(0,FREQSCALE_HEIGHT, OffscreenBitmap->Width, FREQSCALE_HEIGHT+1 ); // TARGET drawing area

        // Copy the off-screen bitmap into the visible bitmap (part or complete)
        // Note:  TCanvas::CopyRect() uses a funny kind of "bounding rectangle",
        //        so use "1 pixel more than obvious" !
        rcSrc = Rect(0,0, OffscreenBitmap->Width, OffscreenBitmap->Height );
        pBmp->Canvas->CopyMode = cmSrcCopy;
        pBmp->Canvas->CopyRect( rcDst, OffscreenBitmap->Canvas, rcSrc );
     } break; // end case TSMODE_INPUT_WATERFALL
    case TSMODE_INPUT_SPECTRUM :
     {
        // Use an "offscreen"-bitmap instead of drawing into the visible bitmap:
        OffscreenBitmap->Height= pBmp->Height;
        OffscreenBitmap->Width = pBmp->Width;
        OffscreenBitmap->Canvas->Brush->Color = pBmp->Canvas->Brush->Color;

        // Copy the previous graph into the off-screen-bitmap for the fading effect .
        //   Beware of the ugly "bounding rectangle" (!)
        rcSrc = Rect(fscale.iLeft, ascale.iTop, fscale.iRight+1/*!*/, ascale.iBottom+1/*!*/ );
        // ex: OffscreenBitmap->Canvas->CopyMode = cmSrcCopy;
        // ex: OffscreenBitmap->Canvas->CopyRect( rcSrc, pBmp->Canvas, rcSrc );
        int i3BGRFadeDelays[3] = { 64, 10, 10 };  // let BLUE fade slower than GREEN and RED..
        BitmapFade( OffscreenBitmap, i3BGRFadeDelays, fscale.iLeft, ascale.iTop, fscale.iRight, ascale.iBottom );

        // Draw the spectrum graph (white curve over blue-fading history) :
        DrawSpectrumGraph( OffscreenBitmap, &fscale, &ascale,
             fscale.iScaleLengthPixels, pfltXr ); // source: decibel values

        // Erase the small "dead rectangle" in the lower right corner :
        OffscreenBitmap->Canvas->FillRect( Rect(  // beware the bounding rectangle (!) ..
           fscale.iRight+1, ascale.iBottom+1, OffscreenBitmap->Width/*!*/, OffscreenBitmap->Height/*!*/ ) );

        // Draw frequency- and amplitude scale :
        DrawScale( OffscreenBitmap, &fscale, &WIO_dblCurrCenterFreq,1 );
        DrawScale( OffscreenBitmap, &ascale, NULL,0 );

        // Finally copy the new plot into the visible bitmap:
        rcSrc = Rect(0,0, OffscreenBitmap->Width/*!*/, OffscreenBitmap->Height/*!*/ );
        pBmp->Canvas->CopyMode = cmSrcCopy;
        pBmp->Canvas->CopyRect( rcSrc, OffscreenBitmap->Canvas, rcSrc );
     }
   } // end switch( WIO_iTuneScopeMode )

} // end UpdateTuningScope()

void __fastcall TWolfMainForm::Img_TuningMouseMove(TObject *Sender,
      TShiftState Shift, int X, int Y)
{
  int iNrFFTBins, iFirstBinIndex;
  T_ScaleParams fscale, ascale;
  double dblFreq, dblAmpl;
  AnsiString s1="", s2="";

  if( WIO_iTuneScopeMode==TSMODE_INPUT_WATERFALL || WIO_iTuneScopeMode==TSMODE_INPUT_SPECTRUM  )
   { // Get the scaling parameters for the tuning scope :
     GetTuningScopeParams( &iNrFFTBins, &iFirstBinIndex, &fscale, &ascale );
     if( ScreenPosToPhysValue( &fscale, X, &dblFreq ) )
         s1 = FormatFloat("###0.0 'Hz'", dblFreq );
     //  s1 = s1 + " x="+IntToStr(X)+" y="+IntToStr(Y);

     if( WIO_iTuneScopeMode==TSMODE_INPUT_SPECTRUM )
      { if( ScreenPosToPhysValue( &ascale, Y, &dblAmpl ) )
            s2 = FormatFloat("##0.0 'dB'", dblAmpl );
      }
   }

  if( s1 != "" )
   { Lab_CursorInfo1->Caption = s1;
     Lab_CursorInfo2->Caption = s2;
   }
  else
   { Lab_CursorInfo1->Caption = s2;
     Lab_CursorInfo2->Caption = "";
   }
}
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
void __fastcall TWolfMainForm::UpdateCaption(void)
{
  AnsiString s = "WOLF";
  // If TWO (or more) instances of this program running at the same time,
  //  show who was the 2nd (or 3rd). "Caption" is the text in the window title.
  if( WIO_InstanceNr>0 )
     s = s + " ("+IntToStr(WIO_InstanceNr+1) + ")";
  Application->Title = s; // "Application->Title" is shown in the task bar

  // For the title bar of the WOLF gui, also add the speed :
  if( SigParms_dblSymPerSec==10.0 )
   { s = s + " - normal speed (WOLF 10)";
   }
  else if( SigParms_dblSymPerSec==5.0 )
   { s = s + " - half speed (WOLF 5)";
   }
  else if( SigParms_dblSymPerSec==2.5 )
   { s = s + " - quarter speed (WOLF 2.5)";
   }
  else if( SigParms_dblSymPerSec==20.0 )
   { s = s + " - double speed (WOLF 20)";
   }
  else if( SigParms_dblSymPerSec==40.0 )
   { s = s + " - fourfold speed (WOLF 40)";
   }
  else
   { s = s + " - " + FormatFloat("##0.#",SigParms_dblSymPerSec) + "sym/sec";
   }
  Caption = s;
}

//---------------------------------------------------------------------------
void __fastcall TWolfMainForm::FormCreate(TObject *Sender)
{
  OSVERSIONINFO windoze_version_info;

  m_iUpdating = 1;

  // Must find out the windoze version (MAJOR version, at least)
  // because of a zillion of subtle and annoying differences
  //  (especially between XP and "Vista"-yucc-makes me vomit- and Windows 7)
  // We don't want to have dozens of GetVersionEx-calls all over the place.
  windoze_version_info.dwOSVersionInfoSize = sizeof( OSVERSIONINFO );
  if( GetVersionEx( &windoze_version_info ) )
   { APPL_iWindowsMajorVersion = windoze_version_info.dwMajorVersion;
     if( windoze_version_info.dwMajorVersion >= 5 )
      { // Win2000, Win XP, etc..  , but definitely NOT win98:

      }
     if( windoze_version_info.dwMajorVersion >= 6 )
      { // Vista (igitt..) oder Windows 7 ?
        // Warum verwenden die Vollpfosten für Beides die gleiche "Major Version" ?
        // Elender Schwachsinn ! Windows "Vista" ist wohl Windows V6.0,
        //                       und "Windows 7" ist wohl Windows V6.1 .
        // Vermutlich ist "Windows 8" dann Windows V7, oder wie oder was ?!
      }
   }

  if( APPL_sz255CurrentDirectory[0] == '\0' )
   { // only if not set yet : save the "current working directory"
     // before one of those f***d fileselector dialog windows screws it up ;-)
    GetCurrentDirectory(  // Win32 API function (no VCL)
       255, // size, in characters, of directory buffer
       APPL_sz255CurrentDirectory ); // address of buffer for current directory
   }

  APPL_hMainWindow = Handle; // used for global message processing (also for sockets)




#if( SWI_UTILITY1_INCLUDED ) /* DL4YHF's "utility"-package #1 included ? */
  UTL_Init(); // should be called "EARLY" ON STARTUP
#endif

  SoundTab_InitCosTable(); // should be called ONCE in the initialization

  // A Borland speciality (a member of each TWinControl):
  // > Ist DoubleBuffered true, zeichnet sich das fensterorientierte Steuerelement
  // > in ein Speicher-Bitmap, das dann zum Zeichnen des Fensters verwendet wird.
  // > Durch die Doppelpufferung tritt weniger Flimmern beim Neuzeichnen des Steuerelements auf.
  // > Auf der anderen Seite wird jedoch mehr Speicher benötigt.
  DoubleBuffered = TRUE;    // this didn't cure the flickering level indicator
  // Img_InputLevel->DoubleBuffered = TRUE; // a 'TImage' doesn't have this..
  SCB_Tx->DoubleBuffered = TRUE;            // but THIS seemed to do the trick


  // A long time ago, some blockhead decided it would be nice
  //  to use a rotten decimal COMMA instead of the decimal DOT in Germany...
  //  WE use the decimal DOT, even on German PC's, right ?
  DecimalSeparator = '.'; // don't want to use that rotten decimal COMMA !
  ThousandSeparator= 0;   // don't want to use the boring separator as well

  OffscreenBitmap = new Graphics::TBitmap();
  OffscreenBitmap->PixelFormat = pf32bit;
  OffscreenBitmap->Height= 1;
  OffscreenBitmap->Width = 1;

  // UpdateCaption();   // too early, must load the config before this

  PageControl1->ActivePage = TS_MainScreen;

  AAM_Init( &MyAnsweringMachine );  // init the Automatic Answering Machine

  // ex: WolfThd_Init();    // Initialize all variables of the WOLF-THREAD module.
           // Since 2005-11, this must be done *AFTER* loading the config !

 APPL_iMainFormCreated = 1;           
 m_iUpdating = 0;
}
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
void __fastcall TWolfMainForm::FormShow(TObject *Sender)
{
  CLB_SpecialOpts->Clear();
  CLB_SpecialOpts->Items->Add("001 Force all frames to use the same bit timing");
  CLB_SpecialOpts->Items->Add("002 Force all frames to use the same frequency");
  CLB_SpecialOpts->Items->Add("004 Crude matched filter (center of bit cell weighted more heavily)");
  CLB_SpecialOpts->Items->Add("010 Crude intersymbol interference (ISI) compensation");
  CLB_SpecialOpts->Items->Add("100 Display ISI info (after compensation, if any)");

}
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
void __fastcall TWolfMainForm::ShowBusyLed(int iTempRxTxState)
{
  Shp_RxTxLed->Brush->Color = clYellow;
  Lab_RxTxInfo1->Caption = "busy";
  WIO_iOldRxTxState = iTempRxTxState;
  Update();
}


//---------------------------------------------------------------------------
void __fastcall TWolfMainForm::Timer1Timer(TObject *Sender)
{
 int i;
 long i32;
 double dbl;
 AnsiString s;
 char sz80Temp[84]; char *cp;
  static int siQsoPeriodNr=0;
  static int siInitCountdown = 10;
  static int siOldSoundcardError = 0;
  static int siOldRcvdMsgCount=0;
  static int siOldQsoMode = -1;
  static int siTxCountRanOff = 0;
  static DWORD sdwOldLostAudioSamples=0;
  static DWORD dwOldAudioBufSourceIndex;

  WIO_dblCurrentTime = time(NULL);  // replacement for "future", more accurate time-sources !

  if( siInitCountdown )
   {
    switch( siInitCountdown-- )
     {
     case 3:   // Read some important settings from an old-style INI file (!)
      { AnsiString s, section;
        s = "wolfcfg.ini";   // We MAY have several WOLF instances with different settings...
        if( WIO_InstanceNr > 0) // Instance counter. 0 = "i am the FIRST instance"
          s = "wolfcfg"+IntToStr(WIO_InstanceNr+1) + ".ini";
        TIniFile *pIniFile = new TIniFile( ExtractFilePath(Application->ExeName) + s );
        ShowStatus("Loading config \"%s\"...", s.c_str() );

        section = "main_window";
        Left = pIniFile->ReadInteger(section,"Left", Left);
        Top  = pIniFile->ReadInteger(section,"Top",  Top);
        Width= pIniFile->ReadInteger(section,"Width",Width);
        Height=pIniFile->ReadInteger(section,"Height",Height);
        SCB_Tx->Height=pIniFile->ReadInteger(section,"VSplit1",SCB_Tx->Height);

        section = "WOLF_config";
        Ed_TxString->Text  = pIniFile->ReadString(section,"TxMessage",Ed_TxString->Text);
        Ed_TxSampleRate->Text = pIniFile->ReadString(section,"TxSampRate",Ed_TxSampleRate->Text);
        Ed_RxSampleRate->Text = pIniFile->ReadString(section,"RxSampRate",Ed_RxSampleRate->Text);
        Ed_TxFcenter->Text = pIniFile->ReadString(section,"TxFcenter",Ed_TxFcenter->Text);
        Ed_RxFcenter->Text = pIniFile->ReadString(section,"RxFcenter",Ed_RxFcenter->Text);
        Ed_OutputAttenuation->Text = pIniFile->ReadString(section,"TxAttenuation",Ed_OutputAttenuation->Text);
        Ed_TxPhaseRevTime->Text = pIniFile->ReadString(section,"TxPhaseRevTime",Ed_TxPhaseRevTime->Text);
        Ed_SlewRate->Text = pIniFile->ReadString(section,"SlewRate",Ed_SlewRate->Text);
        Ed_FreqMeasIntv->Text = pIniFile->ReadString(section,"FreqMeasIntv",Ed_FreqMeasIntv->Text);
        UD_NrRxFrames->Position = (short)pIniFile->ReadInteger(section,"FramesPerRxPeriod", 32);
        Ed_FreqToler->Text= pIniFile->ReadString(section,"FreqToler",Ed_FreqToler->Text);
        CHK_ShowTimeOfDay->Checked=pIniFile->ReadInteger(section,"ShowTimeOfDay", 0 );
        CHK_VerboseMsgs->Checked = pIniFile->ReadInteger(section,"VerboseMsgs", 0 );
        RB_8BitWave->Checked = pIniFile->ReadInteger(section,"8BitWaveFile", 0 );
        RB_16BitWave->Checked = ! RB_8BitWave->Checked;
        s = pIniFile->ReadString(section,"BitsPerSecond", "10" );
        SigParms_dblSymPerSec = strtod( s.c_str(), NULL);
        if( SigParms_dblSymPerSec<1 || SigParms_dblSymPerSec>100 )
            SigParms_dblSymPerSec = 10.0;  // default bitrate for WOLF
        Chk_AddNoiseOnRx->Checked= pIniFile->ReadInteger(section,"AddRxNoiseSwitch", 0 );
        Ed_AddedNoiseLevel->Text = pIniFile->ReadString(section,"AddRxNoiseLevel",Ed_AddedNoiseLevel->Text);

        section = "WOLF_gui_settings";
        s = pIniFile->ReadString(section,"InputDevice", "-1" );
        strncpy( WolfThd_sz255SoundInputDevice, s.c_str(), 255 );
        s = pIniFile->ReadString(section,"OutputDevice", "-1" );
        strncpy( WolfThd_sz255SoundOutputDevice, s.c_str(), 255 );
        RB_TxFirst->Checked = pIniFile->ReadInteger(section,"TxFirst", 0 );
        RB_TxLast->Checked = !RB_TxFirst->Checked;
        Ed_PeriodMins->Text = pIniFile->ReadString(section,"PeriodMinutes", Ed_PeriodMins->Text );
        WIO_iTuneScopeMode= pIniFile->ReadInteger(section,"TuningScopeMode", WIO_iTuneScopeMode );
        WGUI_iScopeRes = pIniFile->ReadInteger(section,"ScopeFreqRes", 1 );
        WGUI_dbldBmin  = pIniFile->ReadInteger(section,"Scope_dBmin",-100);
        WGUI_dbldBmax  = pIniFile->ReadInteger(section,"Scope_dBmax",  0 );
        if( WGUI_dbldBmin>=WGUI_dbldBmax )
          { WGUI_dbldBmin=-100; WGUI_dbldBmax=0; }
        WGUI_iColorPalNr = pIniFile->ReadInteger(section,"Scope_ColorPalNr", 0 );
        CHK_RestartDecoder->Checked = pIniFile->ReadInteger(section,"RestartDecoder", 1 );
        WIO_iComPortNr = pIniFile->ReadInteger(section, "COM_PortNr", 0 );
        if(WIO_iComPortNr<0) WIO_iComPortNr=0;  // 0 = "don't use the COM port"
        if(WIO_iComPortNr>8) WIO_iComPortNr=0;  // COM port numbers above 8 don't work !
        CB_ComPortNr->ItemIndex = WIO_iComPortNr;

        section = "Auto_RX_TX";
        Chk_AAMachineEnabled->Checked = pIniFile->ReadInteger(section,"AutoAnswer",0 );
        Ed_MyCall->Text = pIniFile->ReadString(section,"MyCall","" );
        Ed_DxCall->Text = pIniFile->ReadString(section,"DxCall","" );
        SG_RxTxMsgs->Cells[SEQ_COL_LINE][0] = "Nr";
        SG_RxTxMsgs->Cells[SEQ_COL_RX_MSG][0] = "On Reception of..";
        SG_RxTxMsgs->Cells[SEQ_COL_TX_MSG][0] = "...send message :";
        SG_RxTxMsgs->Cells[SEQ_COL_ACTION][0] = "+ exec cmd";
        for(int i=1; i<SG_RxTxMsgs->RowCount; ++i)
         { s = IntToStr(i);
           SG_RxTxMsgs->Cells[SEQ_COL_LINE][i] = s;
           SG_RxTxMsgs->Cells[SEQ_COL_RX_MSG][i] = pIniFile->ReadString(section,"RxMsg"+s,
               C_szDefaultAutoSeqMsg[3*(i-1)] );
           SG_RxTxMsgs->Cells[SEQ_COL_TX_MSG][i] = pIniFile->ReadString(section,"TxMsg"+s,
               C_szDefaultAutoSeqMsg[3*(i-1)+1] );
           SG_RxTxMsgs->Cells[SEQ_COL_ACTION][i] = pIniFile->ReadString(section,"Action"+s,
               C_szDefaultAutoSeqMsg[3*(i-1)+2] );
         }

        delete pIniFile;  // closes the file (but doesn't delete it)
        ApplyConfig( WIO_STATE_RX ); // -> WIO_dblCurrCenterFreq, WIO_dblXXSampleRate, ...
        UpdateConfig();

        WolfThd_Init();   // Initialize all variables of the WOLF-THREAD module.
          // Since 2005-11, this must be done *AFTER* loading the config,
          // because the size of the audio buffer now depends on the WOLF baudrate.

        if( WIO_iComPortNr > 0 )     // transmitter control through serial port ?
         {  WIO_OpenComPort(WIO_iComPortNr);
            WIO_SetComRTS( FALSE );  // turn off RequestToSend - here used as PTT (!)
            WIO_SetComDTR( FALSE );  // turn alarm bell off too
         }

        // see help on TCursor , TScreen::Cursors
        Screen->Cursors[crHollowCross] = LoadCursor(HInstance, "S_HOLLOW");
        Img_Tuning->Cursor  = TCursor(crHollowCross); // from MyOwnRes.RES


      } break;  // end case 3
     case 2:   // Finished the basic initialisation
        ShowStatus("");
        UpdateLevelIndicator(0); // init "input level indicator"
        ClearTuningScope();
        break;  // end case 2
     case 1:   // Last init-step..
      { // If a command line has been passed to the WOLF GUI,
        //  pass it on to the WOLF CORE...
        int i;
        WolfThd_iArgCnt = ParamCount(); // same meaning as 'argc' in main()..
        for(i=0; i<16; ++i)       // make an array of "pointers to strings":
         { WolfThd_szArg[i][0] = '\0';
           WolfThd_pszArgs[i] = WolfThd_szArg[i];
         }
        if( WolfThd_iArgCnt>0 )   // Pass the 'external' command line to the WOLF core:
         { // Caution: the damned ParamCount() function returns TWO (!)
           //  when THREE arguments are passed, the first being the program name.
           ++WolfThd_iArgCnt;
           if( WolfThd_iArgCnt > 15 )
               WolfThd_iArgCnt = 15;
           for(i=0; i<=WolfThd_iArgCnt; ++i)       // make an array of "pointers to strings"..
            { strcpy( WolfThd_szArg[i], ParamStr(i).c_str() );
            }
           // Switch to the "decoder" tab before starting :
           PageControl1->ActivePage = TS_MainScreen;
           if( WolfThd_LaunchWorkerThreads(WOLFTHD_WAVE_ANALYZER) )
                 ShowStatus( "Running in Batch mode - press && hold ESCAPE to stop");
           else  ShowStatus( "Error: Couldn't launch WOLF WORKER THREAD");
         } // end if( WolfThd_iArgCnt>0 )
        UpdateCaption();         
        break;
      }
     default:
        break;
     } // end switch( siInitCountdown-- )
   }
  else  // not INITIALISING..
   {
     if( (WIO_iRestartDecoderFlags  & WIO_FLAG_4_WOLF_CORE) == 0 )
          Btn_RestartDecoder->Enabled = TRUE;  // WOLF core seems to have handled this flag
     else Btn_RestartDecoder->Enabled = TRUE;  // "restart request" still pending

     // only if audio buffer was successfully allocated:
     if( (WolfThd_pi16AudioBuffer!=NULL) && (WolfThd_dwAudioBufferSize>0) )
      {
        UpdateLevelIndicator( WIO_fltInputLevelPercent );
        Lab_StatusRight->Caption = "BufUsage "
          + FormatFloat("##0.0", WIO_GetAudioBufferUsagePercent() ) + "%";

        switch( WIO_iTuneScopeMode )
         {
          case TSMODE_TUNING_SCOPE_OFF:
               break;
          case TSMODE_INPUT_WATERFALL :
          case TSMODE_INPUT_SPECTRUM  :
            {  DWORD dwAudioBufSourceIndex;
               int iNrFFTBins;
               GetTuningScopeParams( &iNrFFTBins, NULL, NULL, NULL );
               if( WIO_iRxTxState == WIO_STATE_TX )
                { // while TRANSMITTING, use the buffer's TAIL index :
                  dwAudioBufSourceIndex = WolfThd_dwAudioBufTailIndex;
                }
               else
                { // while RECEIVING, use the buffer's HEAD index :
                  dwAudioBufSourceIndex = WolfThd_dwAudioBufHeadIndex;
                }
               if( dwOldAudioBufSourceIndex != dwAudioBufSourceIndex)
                 { int i;
                   long i32BufIndex;
                   int  iFftLength = 2*iNrFFTBins;
                   float fltRe[TSCOPE_MAX_FFT_LENGTH], fltIm[TSCOPE_MAX_FFT_LENGTH];
                   dwOldAudioBufSourceIndex = dwAudioBufSourceIndex;
                   if ( iFftLength>TSCOPE_MAX_FFT_LENGTH )
                        iFftLength=TSCOPE_MAX_FFT_LENGTH;

                   // Get the "latest" samples from the circular audio input buffer :
                   i32BufIndex = dwAudioBufSourceIndex - iFftLength;
                   while(i32BufIndex<0) i32BufIndex+=WolfThd_dwAudioBufferSize;
                   for(i=0; i<iFftLength; ++i)
                    { fltRe[i] = WolfThd_pi16AudioBuffer[ i32BufIndex++ ];
                      if( i32BufIndex>= (long)WolfThd_dwAudioBufferSize )
                          i32BufIndex = 0;
                    }

                // Apply HAMMING window to the time domain samples
                SndMat_MultiplyHanningWindow( fltRe, iFftLength );

                // Calculate FFT. fltRe is input AND result; fltIm is only result
                SndMat_CalcRealFft( iFftLength, fltRe, fltIm );
                // -> fltRe[0..(TSCOPE_FFT_LENGTH/2)] , fltIm[0..(TSCOPE_FFT_LENGTH/2)]

                //

                UpdateTuningScope( fltRe, fltIm );
                }
            }  break; // end case spectrum graph or waterfall display

          case TSMODE_PLOT_DECODER_VARS:
            {
            } break;

          default:
              break;

         } // end switch( WIO_iTuneScopeMode )
      } // end if( WolfThd_pi16AudioBuffer )


     //---- Automatic RX / TX - Changeover in "QSO Mode" -----------------
     if( WIO_iQsoMode )
      { int iNewRxTxState;
        i32 = StrToIntDef( Ed_PeriodMins->Text, 0 );
        if( i32>=1 )  // at least ONE MINUTE PERIODS (2 for WOLF10) ...
         {
           // Get the current "period number" from the PC's local clock :
           dbl = fmod( WIO_dblCurrentTime, 24*60*60 ); // -> time of day
           i32 = (long)dbl / (i32 * 60);
           siQsoPeriodNr = 1 + (i32 & 1);
           if(RB_TxFirst->Checked) // shall "I" send even(1st) or odd(2nd) periods?
                iNewRxTxState = (siQsoPeriodNr==1) ? WIO_STATE_TX : WIO_STATE_RX;
           else iNewRxTxState = (siQsoPeriodNr==2) ? WIO_STATE_TX : WIO_STATE_RX;

           if( iNewRxTxState==WIO_STATE_TX &&  WIO_iRxTxState!=WIO_STATE_TX )
            { // suppress transmission in QSO-mode if the TX-string is empty (1st)
              strncpy(sz80Temp, Ed_TxString->Text.c_str(), 80 );
              sz80Temp[80]='\0';  cp=sz80Temp;
              while( *cp==' ' ) ++cp;
              if(*cp=='\0')  // TX-string is empty (or contains only spaces) :
                iNewRxTxState = WIO_STATE_RX;
            } // end if < going to SWITCH FROM RX TO TX ? >

           if(iNewRxTxState==WIO_STATE_TX && WIO_iRxTxState!=WIO_STATE_TX  )
            { // suppress BEGIN OF a transmission if count of TX-overs runs off (2nd)
              MyAnsweringMachine.iTxOversLeft = StrToIntDef( Ed_TxOversLeft->Text, MyAnsweringMachine.iTxOversLeft );
              if(MyAnsweringMachine.iTxOversLeft>0)
               { --MyAnsweringMachine.iTxOversLeft;
                 siTxCountRanOff = 0;
               }
              else
               { iNewRxTxState = WIO_STATE_RX;
                 if( !siTxCountRanOff )
                  { siTxCountRanOff = 1;
                    WIO_printf( "QSO mode: Cannot transmit because tc=0 (see WOLF Config) !\n" );
                  }
               }
              Ed_TxOversLeft->Text=IntToStr(MyAnsweringMachine.iTxOversLeft);
            }

           if( iNewRxTxState != WIO_iRxTxState )
            { // time to change from RX to TX (or vice versa) ?
              if( iNewRxTxState==WIO_STATE_RX )
                   BeginReception();
              else BeginTransmission();
            }
         } // end if < period time valid >
      } // end if( WIO_iQsoMode )


   } // end else < not 'initializing' >

  i32 = WIO_dblCurrentTime;
  sprintf(sz80Temp, "%02d:%02d:%02d", (int)((i32/3600)%24), (int)((i32/60)%60), (int)(i32%60) );
  // ex: MI_Time->Caption = sz80Temp;  // no problem with BCB V4,
  // but with BCB V6, the main menu flickered like hell when periodically
  // modifying this 'top level' menu item (MI_Time->Caption) .
  Lab_StatusMiddle->Caption = sz80Temp; // cure: use a PANEL instead (in the status line)

#if(0) // FOR DEBUGGING PURPOSES ONLY :
  // Poll the ESCAPE key, to stop any worker thread politely...
  if ( GetAsyncKeyState( VK_ESCAPE) )
   { //  Return Values of GetAsyncKeyState() :
     //     If the function succeeds, the return value specifies whether the
     //     key was pressed since the last call to GetAsyncKeyState,
     //     and whether the key is currently up or down.
     //  If the most significant bit is set, the key is down,
     //  and if the least significant bit is set, the key was pressed
     //  after the previous call to GetAsyncKeyState.
     //  The return value is zero if a window in another thread or process
     //  currently has the keyboard focus.
     WIO_fStopFlag = TRUE;  // request to finish worker thread a.s.a.p.
     if( WIO_iExitCode==0 )
         WIO_iExitCode = -1; // flag for "terminated by user"
   }
#endif // for debugging: stop all worker threads per ESCAPE key ?

  // Check the most common errors :
  if( sdwOldLostAudioSamples != WolfThd_dwAudioSamplesLost )
   {  sdwOldLostAudioSamples  = WolfThd_dwAudioSamplesLost;
      if( sdwOldLostAudioSamples != 0 )
       { ShowStatus( "Buffer overflow, lost %ld audio samples", (long)sdwOldLostAudioSamples );
       }
   }

  if( WolfThd_fExitedThread )
   { // one of the worker threads has terminated itself ( is "done" ) ...
     ShowStatus( "Worker thread finished, exit code = %d", (int)WIO_iExitCode );
     WolfThd_TerminateAndDeleteThreads();  // release all unused thread handles if necessary
     WolfThd_fExitedThread = FALSE; // handled !
     // start the decoder again after processing <FRMMAX> frames from the soundcard ?
     if( CHK_RestartDecoder->Checked && WIO_iRxTxState==WIO_STATE_RX && WIO_iUseSoundcard )
      { MI_ReceiveClick(Sender);  // happens every 51 minutes, with FRMMAX=32, and 96 seconds per frame
      }
     else
      { WIO_iRxTxState = WIO_STATE_OFF;
      }
   }

  // When running in SOUNDCARD mode, show errors from soundcard / multimedia system
  i = WolfThd_GetSoundcardErrorStatus();
  if( siOldSoundcardError != i )
   {  siOldSoundcardError  = i;
      if( siOldSoundcardError != NO_ERRORS )
        ShowStatus( "Soundcard error: %s", (char*)ErrorCodeToString(siOldSoundcardError) );
   }

  // Update the "RX" / "TX" / "OFF" indicator if necessary :
  if( WIO_iOldRxTxState!= WIO_iRxTxState || siOldQsoMode!=WIO_iQsoMode )
   {  WIO_iOldRxTxState = WIO_iRxTxState;
      siOldQsoMode = WIO_iQsoMode;
      switch( WIO_iOldRxTxState )
       { case WIO_STATE_OFF:
         default:
              Shp_RxTxLed->Brush->Color = clGray;
              Lab_RxTxInfo1->Caption = "OFF";
              Lab_RxTxInfo2->Caption = "F9:RX";
              break;
         case WIO_STATE_RX :
              Shp_RxTxLed->Brush->Color = clLime;
              Lab_RxTxInfo1->Caption = "RX";
              if( WIO_iQsoMode )
                   Lab_RxTxInfo2->Caption = "QSO p"+IntToStr(siQsoPeriodNr);
              else Lab_RxTxInfo2->Caption = "F10:TX";
              break;
         case WIO_STATE_TX :
              Shp_RxTxLed->Brush->Color = clRed;
              Lab_RxTxInfo1->Caption = "TX";
              if( WIO_iQsoMode )
                   Lab_RxTxInfo2->Caption = "QSO p"+IntToStr(siQsoPeriodNr);
              else Lab_RxTxInfo2->Caption = "F9:RX";
              break;
       }
   }

  // Process all received messages which may have poured out of the decoder.
  //  ( The Automatic Answering Machine is in another module ("AAM") without VCL )
  if( siOldRcvdMsgCount != WIO_iRcvdMessageCount )
   {  siOldRcvdMsgCount = WIO_iRcvdMessageCount;
      MyAnsweringMachine.iTxOversLeft = StrToIntDef( Ed_TxOversLeft->Text, MyAnsweringMachine.iTxOversLeft );
      if( AAM_HandleReceivedText( &MyAnsweringMachine, WIO_sz80RcvdMessage ) )
       { Ed_DxCall->Text = AAM_GetMacroText( &MyAnsweringMachine, "dxcall", NULL );
         Ed_TxString->Text= AAM_GetTxText( &MyAnsweringMachine );
         if( CHK_AlarmOnReception->Checked )
          {
            WIO_SetComDTR( TRUE/*high*/ );  // set DTR signal to ring the bell
          }
         if( Chk_AAMachineEnabled->Checked )
          {  // only change to TRANSMIT if the auto-answer function is enabled:

          }
       }
      Ed_TxOversLeft->Text = IntToStr( MyAnsweringMachine.iTxOversLeft );
   }

  // Get all decoder output lines from the thread-safe buffer
  // and flush them into the DECODER OUTPUT WINDOW :
  while( WIO_wDispBufTail != WIO_wDispBufHead )
   {
     WIO_wDispBufTail &= 0x0007;  // keep buffer index in legal range
     REd_Decoder->Lines->Append( WIO_sz2kDisplayLineBuffer[WIO_wDispBufTail] );
     WIO_wDispBufTail = (WORD)( (WIO_wDispBufTail+1) & 0x0007 );
     // to keep current output position 'in sight' (by scrolling the text) :
     REd_Decoder->HideSelection = false;
   }

} // end Timer1Timer()
//---------------------------------------------------------------------------


void __fastcall TWolfMainForm::MI_WOLFsettingsClick(TObject *Sender)
{
   PageControl1->ActivePage = TS_WolfConfig;
}
//---------------------------------------------------------------------------


int  __fastcall TWolfMainForm::ApplySpecialOptions(void)
{
 int i;
 int iBinOpts = 0;
 char sz3OctalValue[8];
  for(i=0; i<CLB_SpecialOpts->Items->Count; ++i)
   { if( CLB_SpecialOpts->Checked[i] )
      { strncpy( sz3OctalValue,CLB_SpecialOpts->Items->Strings[i].c_str(), 3);
        iBinOpts |= strtol( sz3OctalValue, NULL, 8);
      }
   }
  return iBinOpts;
}

void __fastcall TWolfMainForm::Btn_ClearDecoderOutputClick(TObject *Sender)
{
   REd_Decoder->Clear();
}
//---------------------------------------------------------------------------



void __fastcall TWolfMainForm::FormClose(TObject *Sender,
      TCloseAction &Action)
{
 AnsiString s, section;

  if( WolfThd_hWorkerThread || WolfThd_hAudioThread  )
   { ShowStatus("Terminating worker threads...");
     WolfThd_TerminateAndDeleteThreads();  // here in FormClose()
   }
  ApplyConfig(WIO_iRxTxState); // apply settings before saving
  WIO_Exit();          // close COM port if opened, etc

  s = "wolfcfg.ini";   // We MAY have several WOLF instances with different settings...
  if( WIO_InstanceNr > 0) // Instance counter. 0 = "i am the FIRST instance"
     s = "wolfcfg"+IntToStr(WIO_InstanceNr+1) + ".ini";
  ShowStatus("Saving config \"%s\"...", s.c_str() );
  TIniFile *pIniFile = new TIniFile( ExtractFilePath(Application->ExeName) + s );
  section = "main_window";
  pIniFile->WriteInteger(section,"Left", Left);
  pIniFile->WriteInteger(section,"Top",  Top);
  pIniFile->WriteInteger(section,"Width",Width);
  pIniFile->WriteInteger(section,"Height",Height);
  pIniFile->WriteInteger(section,"VSplit1",SCB_Tx->Height);


  section = "WOLF_config";
  pIniFile->WriteString( section, "TxMessage", Ed_TxString->Text);
  pIniFile->WriteString( section, "TxSampRate",Ed_TxSampleRate->Text);
  pIniFile->WriteString( section, "RxSampRate",Ed_RxSampleRate->Text);
  pIniFile->WriteString( section, "TxFcenter", Ed_TxFcenter->Text);
  pIniFile->WriteString( section, "RxFcenter", Ed_RxFcenter->Text);
  pIniFile->WriteString( section, "TxAttenuation",Ed_OutputAttenuation->Text);
  pIniFile->WriteString( section, "TxPhaseRevTime",Ed_TxPhaseRevTime->Text);
  pIniFile->WriteString( section, "SlewRate",    Ed_SlewRate->Text);
  pIniFile->WriteInteger(section,"FramesPerRxPeriod",UD_NrRxFrames->Position);
  pIniFile->WriteString( section, "FreqMeasIntv",Ed_FreqMeasIntv->Text);
  pIniFile->WriteString( section, "FreqToler",   Ed_FreqToler->Text);
  pIniFile->WriteInteger(section,"ShowTimeOfDay",CHK_ShowTimeOfDay->Checked );
  pIniFile->WriteInteger(section,"VerboseMsgs",  CHK_VerboseMsgs->Checked );
  pIniFile->WriteInteger(section,"8BitWaveFile", RB_8BitWave->Checked  );
  pIniFile->WriteString(section,"BitsPerSecond", FormatFloat("##0.0",SigParms_dblSymPerSec) );
  pIniFile->WriteInteger(section,"AddRxNoiseSwitch", Chk_AddNoiseOnRx->Checked );
  pIniFile->WriteString(section,"AddRxNoiseLevel",Ed_AddedNoiseLevel->Text);

  section = "WOLF_gui_settings";
  pIniFile->WriteString(section,"InputDevice", WolfThd_sz255SoundInputDevice );
  pIniFile->WriteString(section,"OutputDevice",WolfThd_sz255SoundOutputDevice );
  pIniFile->WriteInteger(section,"TxFirst",      RB_TxFirst->Checked  );
  pIniFile->WriteString(section, "PeriodMinutes",Ed_PeriodMins->Text );
  pIniFile->WriteInteger(section,"TuningScopeMode", WIO_iTuneScopeMode );
  pIniFile->WriteInteger(section,"ScopeFreqRes",    WGUI_iScopeRes );
  pIniFile->WriteInteger(section,"Scope_dBmin", (int)WGUI_dbldBmin );
  pIniFile->WriteInteger(section,"Scope_dBmax", (int)WGUI_dbldBmax );
  pIniFile->WriteInteger(section,"Scope_ColorPalNr", WGUI_iColorPalNr );
  pIniFile->WriteInteger(section,"RestartDecoder",  CHK_RestartDecoder->Checked );
  pIniFile->WriteInteger(section, "COM_PortNr", WIO_iComPortNr );

  section = "Auto_RX_TX";
  pIniFile->WriteInteger(section,"AutoAnswer", Chk_AAMachineEnabled->Checked );
  pIniFile->WriteString(section,"MyCall", Ed_MyCall->Text );
  pIniFile->WriteString(section,"DxCall", Ed_DxCall->Text );
  for(int i=1; i<SG_RxTxMsgs->RowCount; ++i)
   { s = IntToStr(i);
     pIniFile->WriteString(section,"RxMsg"+s,SG_RxTxMsgs->Cells[SEQ_COL_RX_MSG][i] );
     pIniFile->WriteString(section,"TxMsg"+s,SG_RxTxMsgs->Cells[SEQ_COL_TX_MSG][i] );
     pIniFile->WriteString(section,"Action"+s,SG_RxTxMsgs->Cells[SEQ_COL_ACTION][i] );
   }


  delete pIniFile;  // closes the file (but doesn't delete it)

  ShowStatus("Closing main form...");

}
//---------------------------------------------------------------------------

void __fastcall TWolfMainForm::MI_ReceiveClick(TObject *Sender)
{
   WIO_iQsoMode = 0;   // no 2-way QSO, instead RX ONLY
   BeginReception();   // build command line and launch WOLF-thread(s)
}
//---------------------------------------------------------------------------

void __fastcall TWolfMainForm::MI_TxBeaconClick(TObject *Sender)
{
  WIO_iQsoMode = 0;    // no 2-way QSO, instead BEACONING ...
  BeginTransmission(); // transmit without period control ("beacon mode")
}
//---------------------------------------------------------------------------

void __fastcall TWolfMainForm::MI_QsoModeClick(TObject *Sender)
{
  WIO_iQsoMode = 1;    // no more BEACON, but attempting a 2-way QSO
  PageControl1->ActivePage = TS_MainScreen;  // switch to main screen with "decoder" output  
  // (the rest is done in the TIMER routine)
}
//---------------------------------------------------------------------------

void __fastcall TWolfMainForm::MI_QSOModeParamsClick(TObject *Sender)
{
   PageControl1->ActivePage = TS_WolfConfig; // switch to main screen with "decoder" output
   Ed_PeriodMins->SetFocus();
}
//---------------------------------------------------------------------------


void __fastcall TWolfMainForm::MI_SendTuneToneClick(TObject *Sender)
{
  double dbl, dblAmpl, dblIndexToRad, dblPhaseAccu;
  long  i32, i32NrPeriods, i32NrSamples;

   ShowBusyLed(WIO_STATE_B4TX);         // show YELLOW LED while "preparing"
   WIO_iQsoMode = 0;      // stop the "QSO machine" (if it was active)
   WolfThd_TerminateAndDeleteThreads(); // terminate worker threads (if any)
   ApplyConfig( WIO_STATE_TX );  // -> WIO_dblCurrCenterFreq, WIO_dblCurrSampleRate, ...
   WIO_iUseSoundcard = 1;        // use the soundcard for output
   WIO_ClearAudioBuffer();

   // Fill the buffer with a pure sine wave, using an integer number of periods.
   dbl/*number of sinewave periods*/ = (double)WIO_GetMaxAudioBufferSize() * WIO_dblCurrCenterFreq / WIO_dblCurrSampleRate;
   i32NrPeriods = (long)dbl;  // truncated, but the lookup table is long enough so don't worry..
   i32NrSamples = (long)( (double)i32NrPeriods * WIO_dblCurrSampleRate / WIO_dblCurrCenterFreq);
   dblIndexToRad = 2.0 * PI * WIO_dblCurrCenterFreq / WIO_dblCurrSampleRate; // index -> radians
   dblPhaseAccu  = 0.0;
   dblAmpl = 32767.0 * pow(10, -WIO_fltOutputAttenuation/20.0 );
   for(i32=0; i32<i32NrSamples; ++i32)
    { WIO_SendSampleToSoundcard( dblAmpl * cos( dblPhaseAccu ) );
      dblPhaseAccu += dblIndexToRad;
      if( dblPhaseAccu >  (2.0*PI) )
       {  dblPhaseAccu -= (2.0*PI);  // don't let the angle for cos() get too large
       }
      // 2013-02: Where did those 'weak sidebands' at 550 and 750 Hz,
      //          symmetrically around the carrier at 650 Hz, come from ?
    }
   WIO_SetComRTS( TRUE );         // set RequestToSend = activate PTT

   // Launch the audio-output thread in 'loop'-mode :
   if( WolfThd_LaunchWorkerThreads(WOLFTHD_AUDIO_TX_LOOP) )
    { ShowStatus( "Sending %.1lf Hz sine to %s",
               (double)WIO_dblCurrCenterFreq, WolfThd_sz255SoundOutputDevice );
      WIO_iRxTxState = WIO_STATE_TX;
      WIO_iTxWhat    = WIO_TX_TUNE;
    }
   else
    { ShowStatus( "Error: Couldn't launch WOLF WORKER THREAD");
      WIO_iRxTxState = WIO_STATE_OFF;
    }

}
//---------------------------------------------------------------------------


void __fastcall TWolfMainForm::MI_RxFreqCheckClick(TObject *Sender)
{
   WIO_iQsoMode = 0;      // stop the "QSO machine" (if it was active)
   WolfThd_TerminateAndDeleteThreads(); // terminate worker threads (if any)
   ApplyConfig( WIO_STATE_RX );  // -> WIO_dblCurrCenterFreq, WIO_dblCurrSampleRate, ...
   WIO_SetComRTS( FALSE );  // turn PTT off
   PageControl1->ActivePage = TS_MainScreen;  // switch to main screen with "decoder" output
   WolfThd_fExitedThread = WolfThd_fExitedThread;  // TEST for debugger
   WIO_iUseSoundcard = 1; // let WOLF.CPP use the soundcard routines, not a wave-file
   WolfThd_iArgCnt = 0;   // here in BuildRxCommandLine() ...
   for(int i=0; i<WOLF_MAX_ARGS; ++i)   // make an array of "pointers to strings":
    { WolfThd_szArg[i][0] = '\0';
      WolfThd_pszArgs[i] = WolfThd_szArg[i];
    }
   strcpy(WolfThd_szArg[WolfThd_iArgCnt++], "wolf" ); // dummy for application line in argv[0]
   strcpy(WolfThd_szArg[WolfThd_iArgCnt++], "-r" );
   strcpy(WolfThd_szArg[WolfThd_iArgCnt++], Ed_RxSampleRate->Text.c_str() );
   strcpy(WolfThd_szArg[WolfThd_iArgCnt++], "-f" );
   strcpy(WolfThd_szArg[WolfThd_iArgCnt++], Ed_RxFcenter->Text.c_str()    );
   // The 'm' command displays frequency, angle, and power
   //     at the interval specified (from 10 to 100 seconds).
   strcpy(WolfThd_szArg[WolfThd_iArgCnt++], "-m" );
   strcpy(WolfThd_szArg[WolfThd_iArgCnt++], Ed_FreqMeasIntv->Text.c_str() );
   if( Ed_SlewRate->Text != "" )
    { strcpy(WolfThd_szArg[WolfThd_iArgCnt++], "-w" );
      strcpy(WolfThd_szArg[WolfThd_iArgCnt++], Ed_SlewRate->Text.c_str() );
    }
   if( WolfThd_LaunchWorkerThreads(WOLFTHD_AUDIO_RX) )
    { ShowStatus( "Frequency measurement using soundcard");
      WIO_iRxTxState = WIO_STATE_RX;
    }
   else
    { ShowStatus( "Error: Couldn't launch WOLF WORKER THREAD");
      WIO_iRxTxState = WIO_STATE_OFF;
    }
   WolfThd_fExitedThread = WolfThd_fExitedThread;  // TEST for debugger
}
//---------------------------------------------------------------------------


void __fastcall TWolfMainForm::RxTxStopClick(TObject *Sender,
      TMouseButton Button, TShiftState Shift, int X, int Y)
{
   switch( WIO_iRxTxState )  // sequence:  OFF -> RX -> TX -> RX -> TX ...
    {
      case WIO_STATE_RX:
           if( WIO_iQsoMode ) // ?
            { MI_StopClick(Sender); // assume user wants to TERMINATE QSO-MODE
            }
           else
            { BeginTransmission(); // transmit without period control ("beacon mode")
            }
           break;
      case WIO_STATE_TX:
      default:
           MI_ReceiveClick( Sender );
           break;
    }
}
//---------------------------------------------------------------------------

void __fastcall TWolfMainForm::MI_StopClick(TObject *Sender)
{
   ShowBusyLed(WIO_STATE_B4RX);  // show YELLOW LED while terminating thread
   WIO_iQsoMode = 0;        // stop the "QSO machine"
   WIO_fStopFlag = TRUE;    // request to finish worker thread a.s.a.p.
   WolfThd_TerminateAndDeleteThreads(); // terminate worker threads
   WIO_SetComRTS( FALSE );  // turn PTT off (now, when AUDIO IS OFF)
   WIO_SetComDTR( FALSE );  // turn alarm bell off too
   WIO_iRxTxState = WIO_STATE_OFF;
}
//---------------------------------------------------------------------------

void __fastcall TWolfMainForm::MI_RXVolCtrlClick(TObject *Sender)
{
#if(0) // removed 2013, because this nice simple utility doesn't exist on windows 7 anymore:
  ShellExecute(  // See Win32 Programmer's Reference ...
         Handle,    // HWND hwnd = handle to parent window
           NULL,    // LPCTSTR lpOperation = pointer to string that specifies operation to perform
    "sndvol32.exe", // LPCTSTR lpFile = pointer to filename or folder name string
           "/r",    // LPCTSTR lpParameters = pointer to string that specifies executable-file parameters
           NULL,    // LPCTSTR lpDirectory = pointer to string that specifies default directory
    SW_SHOWNORMAL); // .. whether file is shown when opened
#else
  // Things get terribly obfuscated for Windows 7 ... see SOUND.CPP :
  Sound_OpenSoundcardInputControlPanel( WolfThd_sz255SoundInputDevice );
#endif
}
//---------------------------------------------------------------------------

void __fastcall TWolfMainForm::MI_TXVolCtrlClick(TObject *Sender)
{
#if(0) // removed 2013, because this nice simple utility doesn't exist on windows 7 anymore:
  ShellExecute( Handle, NULL, "sndvol32.exe", "", NULL, SW_SHOWNORMAL);
#else  // also here, must fry an 'extra sausage' for Vista and Win7 .. eek:
  Sound_OpenSoundcardOutputControlPanel( WolfThd_sz255SoundOutputDevice );
#endif
}
//---------------------------------------------------------------------------

void __fastcall TWolfMainForm::MI_ManualClick(TObject *Sender)
{ // poor man's help system, using the default HTML browser :
  ShellExecute( Handle, "open", "html\\wolf_gui_manual.html", "", NULL, SW_SHOWNORMAL);
}
//---------------------------------------------------------------------------


void __fastcall TWolfMainForm::FormResize(TObject *Sender)
{ // Called when the size of the main window has changed
  UpdateLevelIndicator( WIO_fltInputLevelPercent );
}
//---------------------------------------------------------------------------

void __fastcall TWolfMainForm::MI_OptionsClick(TObject *Sender)
{
  // Update some menu entries with variable contents,
  //  before opening this menu :
  MI_UsePrefilter->Checked = (WIO_iPrefilterOptions & WIO_PREFILTER_ON)!=0;
}
//---------------------------------------------------------------------------

void __fastcall TWolfMainForm::MI_ModeClick(TObject *Sender)
{
  char sz80Temp[84];
  sprintf( sz80Temp, "&QSO mode (%s, %s min periods)",
        (char*)( RB_TxFirst->Checked ? "TX 1st, RX 2nd" : "RX 1st, TX 2nd" ),
        (char*)  Ed_PeriodMins->Text.c_str()  );
  MI_QsoMode->Caption = sz80Temp;
  MI_QsoMode->Checked = WIO_iQsoMode;
  MI_TxBeacon->Checked = (!WIO_iQsoMode) && (WIO_iRxTxState==WIO_STATE_TX) && (WIO_iTxWhat==WIO_TX_DATA);
  MI_Receive->Checked  = (!WIO_iQsoMode) &&  WolfThd_fWolfCoreThdRunning && (WIO_iRxTxState==WIO_STATE_RX);
  MI_Stop->Checked = (!WolfThd_fWolfCoreThdRunning) && (WIO_iRxTxState==WIO_STATE_OFF);
  MI_SendTuneTone->Checked = (WIO_iRxTxState==WIO_STATE_TX) && (WIO_iTxWhat==WIO_TX_TUNE);
  MI_RxFreqCheck->Checked = FALSE;
  sprintf(sz80Temp, "WOLF %lg [Symbols/sec]", (double)SigParms_dblSymPerSec );
  MI_WolfSpeed->Caption   = sz80Temp;
  MI_NormalSpeed->Checked = (SigParms_dblSymPerSec == 10.0);
  MI_HalfSpeed->Checked   = (SigParms_dblSymPerSec == 5.0 );
  MI_QuarterSpeed->Checked= (SigParms_dblSymPerSec == 2.5 );
  MI_DoubleSpeed->Checked = (SigParms_dblSymPerSec == 20.0);
  MI_FourfoldSpeed->Checked=(SigParms_dblSymPerSec == 40.0);
}
//---------------------------------------------------------------------------


void __fastcall TWolfMainForm::MI_TuningScopeClick(TObject *Sender)
{
 char *scope_res[] =  { "low","medium","high","??" };
  MI_TScope_Off->Checked = (WIO_iTuneScopeMode==TSMODE_TUNING_SCOPE_OFF);
  MI_TScope_Waterfall->Checked   = (WIO_iTuneScopeMode==TSMODE_INPUT_WATERFALL);
  MI_TScope_SpecGraph->Checked   = (WIO_iTuneScopeMode==TSMODE_INPUT_SPECTRUM);
  MI_TScope_DecoderVars->Checked = (WIO_iTuneScopeMode==TSMODE_PLOT_DECODER_VARS);
  MI_ScopeRes->Caption = "Frequency resolution: "+AnsiString(scope_res[WGUI_iScopeRes&0x003]);
  MI_ScopeAmplRange->Caption= "Scope amplitude range: "
          + IntToStr((int)WGUI_dbldBmin)+" ... "
          + IntToStr((int)WGUI_dbldBmax)+" dB";
  MI_CP_Linrad->Checked = ( WGUI_iColorPalNr == 0 );
  MI_CP_Sunrise->Checked= ( WGUI_iColorPalNr == 1 );
}
//---------------------------------------------------------------------------


void __fastcall TWolfMainForm::MI_TScope_WaterfallClick(TObject *Sender)
{ WIO_iTuneScopeMode = TSMODE_INPUT_WATERFALL;
}
//---------------------------------------------------------------------------

void __fastcall TWolfMainForm::MI_TScope_SpecGraphClick(TObject *Sender)
{ WIO_iTuneScopeMode = TSMODE_INPUT_SPECTRUM;
}
//---------------------------------------------------------------------------

void __fastcall TWolfMainForm::MI_TScope_OffClick(TObject *Sender)
{ WIO_iTuneScopeMode = TSMODE_TUNING_SCOPE_OFF;
}
//---------------------------------------------------------------------------

void __fastcall TWolfMainForm::MI_TScope_DecoderVarsClick(TObject *Sender)
{ WIO_iTuneScopeMode = TSMODE_PLOT_DECODER_VARS;
}
//---------------------------------------------------------------------------

void __fastcall TWolfMainForm::MI_ScopeAmplRangeClick(TObject *Sender)
{
 BOOL fModified = FALSE;
 int i;
  i = (int)WGUI_dbldBmin;  // min. magnitude for the tuning scope
  RunIntegerInputDialog(   // a MODAL dialog; returns a button code like mrOk (!)
       "WOLF Tuning Scope", "Enter display range minimum [dB]",
       "Default for min value = -100 dB",
        &i,  EDIT_NORMAL, 0/*iHelpContext*/ );
  fModified |= ((int)WGUI_dbldBmin != i);
  WGUI_dbldBmin = i;
  i = (int)WGUI_dbldBmax;  // min. magnitude for the tuning scope
  RunIntegerInputDialog(   // a MODAL dialog; returns a button code like mrOk (!)
       "WOLF Tuning Scope", "Enter display range maxmum [dB]",
       "Default: 0 dB = clipping point",
        &i,  EDIT_NORMAL, 0/*iHelpContext*/ );
  fModified |= ((int)WGUI_dbldBmax != i);
  WGUI_dbldBmax = i;
  if( WGUI_dbldBmin > WGUI_dbldBmax )
   { WGUI_dbldBmax = WGUI_dbldBmin;
     WGUI_dbldBmin = i;
   }
  if( fModified ) WIO_iOldTuneScopeMode = -1; // force redraw a.s.a.p.
}
//---------------------------------------------------------------------------


void __fastcall TWolfMainForm::Btn_RestartDecoderClick(TObject *Sender)
{
  if( WIO_iRxTxState == WIO_STATE_OFF )
   {   // decoder isn't running at all - guess the user wants to START it ?
     MI_ReceiveClick(Sender);
   }
  else
   {   //  decoder already running ..
     // Here, instead of killing the decoder process,
     //  only set a SIGNAL for the decoder.
     // Background: Audio processing shall continue undisturbed
     //             in a future version (??) with GPS- or DCF77-decoder .
     WIO_iRestartDecoderFlags = 0xFFFF;  // flags for up to 16 "consumers" to clear history buffers
     Btn_RestartDecoder->Enabled = FALSE;
   }
}
//---------------------------------------------------------------------------

void __fastcall TWolfMainForm::Btn_CopyDecoderOutputClick(TObject *Sender)
{
  if( REd_Decoder->SelLength < 2 )  // if nothing selected..
      REd_Decoder->SelectAll();       // .. select ALL !
  REd_Decoder->CopyToClipboard();     // otherwise copy selected text only
}
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
//  Automated RX / TX Sequence
//---------------------------------------------------------------------------

void __fastcall TWolfMainForm::SG_RxTxMsgsClick(TObject *Sender)
{ // Called when user selected a cell (row or column) in the "Auto RX/TX grid" .
  if( m_iUpdating )
    return;
}
//---------------------------------------------------------------------------


void __fastcall TWolfMainForm::MI_ScopeResLowClick(TObject *Sender)
{
  WGUI_iScopeRes = 0;
}
//---------------------------------------------------------------------------

void __fastcall TWolfMainForm::MI_ScopeResMedClick(TObject *Sender)
{
  WGUI_iScopeRes = 1;
}
//---------------------------------------------------------------------------

void __fastcall TWolfMainForm::MI_ScopeResHighClick(TObject *Sender)
{
  WGUI_iScopeRes = 2;
}
//---------------------------------------------------------------------------


void __fastcall TWolfMainForm::Chk_TestPTTClick(TObject *Sender)
{
  int iNewState = Chk_TestPTT->Checked;
  ApplyConfig(WIO_iRxTxState);
  WIO_SetComRTS( iNewState );
}
//---------------------------------------------------------------------------

void __fastcall TWolfMainForm::Chk_TestAlarmBellClick(TObject *Sender)
{
  int iNewState = Chk_TestAlarmBell->Checked;
  ApplyConfig(WIO_iRxTxState);
  WIO_SetComDTR( iNewState );
}
//---------------------------------------------------------------------------

void __fastcall TWolfMainForm::MI_CP_LinradClick(TObject *Sender)
{
  WGUI_iColorPalNr = 0;
}
//---------------------------------------------------------------------------

void __fastcall TWolfMainForm::MI_CP_SunriseClick(TObject *Sender)
{
  WGUI_iColorPalNr = 1;
}
//---------------------------------------------------------------------------


void __fastcall TWolfMainForm::Btn_CalRxSrClick(TObject *Sender)
{
  char   sz255Temp[256];
  double dblFcarr,dblFref, dblFdisp;
  double dblFsOld,dblFsNew;

  MI_StopClick(Sender);  // force restart of decoder !

  dblFcarr= dblFref = strtod(Ed_RxFcenter->Text.c_str(), NULL);
  dblFsOld= strtod(Ed_RxSampleRate->Text.c_str(), NULL);
  if(  RunFloatInputDialog( "RX Sample Rate Calibration",
           "Enter the KNOWN REFERENCE frequency",
           " (audio frequency in Hertz)", &dblFref,
           EDIT_NORMAL,  0/*iHelpContext*/ )  )
   { if(    WGUI_dblCurrPeakFreq >= (dblFref * 0.9)
         && WGUI_dblCurrPeakFreq <= (dblFref * 1.1)  )
          dblFdisp = WGUI_dblCurrPeakFreq;
     else dblFdisp = dblFref;
     if( RunFloatInputDialog( "RX Sample Rate Calibration",
           "Enter the DISPLAYED audio frequency in Hertz,",
           " or the last \"f:\"-value from the WOLF decoder .", &dblFdisp,
           EDIT_NORMAL,  0/*iHelpContext*/ )  )
      { // Calculate the new, "calibrated" sample rate ...
        if( dblFdisp>=-10.0 && dblFdisp<=10.0 )
         { // doesn't look like an absolute AUDIO FREQUENCY,
           // but more like an OFFSET which must be ADDED
           // to the carrier-frequency ...
           dblFdisp += dblFcarr;
         }

        if( dblFref>0 && dblFdisp>0 )
         { // if sample rate is too low, "displayed" freq will be too high !
           dblFsNew = dblFsOld * dblFref / dblFdisp;
           sprintf(sz255Temp,"The new sampling rate would be %8.3lf Hz.\r\n"\
                             "Does this sound reasonable ?", dblFsNew );
           if( IDYES == MessageBox( Handle, // handle of owner window
                               sz255Temp,  // text in message box
               "Sample Rate Calibration",  // title of message box
               MB_YESNOCANCEL | MB_DEFBUTTON2 ) ) // style of message box
            {
              sprintf(sz255Temp,"%8.3lf", dblFsNew );
              Ed_RxSampleRate->Text = sz255Temp;
            }
         }
      }
   }
}
//---------------------------------------------------------------------------

void __fastcall TWolfMainForm::MI_NormalSpeedClick(TObject *Sender)
{
  SigParms_dblSymPerSec = 10.0;
  ApplyConfig(WIO_iRxTxState);
}
//---------------------------------------------------------------------------

void __fastcall TWolfMainForm::MI_HalfSpeedClick(TObject *Sender)
{
  SigParms_dblSymPerSec = 5.0;
  ApplyConfig(WIO_iRxTxState);
}
//---------------------------------------------------------------------------

void __fastcall TWolfMainForm::MI_QuarterSpeedClick(TObject *Sender)
{
  SigParms_dblSymPerSec = 2.5;
  ApplyConfig(WIO_iRxTxState);
}
//---------------------------------------------------------------------------

void __fastcall TWolfMainForm::MI_DoubleSpeedClick(TObject *Sender)
{
  SigParms_dblSymPerSec = 20.0;
  ApplyConfig(WIO_iRxTxState);
}
//---------------------------------------------------------------------------

void __fastcall TWolfMainForm::MI_FourfoldSpeedClick(TObject *Sender)
{
  SigParms_dblSymPerSec = 40.0;
  ApplyConfig(WIO_iRxTxState);
}
//---------------------------------------------------------------------------

void __fastcall TWolfMainForm::ApplySpecialOptions(TObject *Sender)
{

  // Output formatting options:
  WolfOpt_iShowTimeOfDay = CHK_ShowTimeOfDay->Checked; // 1=show time of day, 0=show seconds since start

  // Test/Debugging stuff for the WOLF implementation :
  WIO_iAddRxNoiseSwitch  = Chk_AddNoiseOnRx->Checked;
  WIO_dblAddRxNoiseLevel = strtod( Ed_AddedNoiseLevel->Text.c_str(), NULL );

}
//---------------------------------------------------------------------------

void __fastcall TWolfMainForm::MI_UsePrefilterClick(TObject *Sender)
{  // toggle prefilter on/off ( "highly experimental" or even "questionable" ! )
   WIO_iPrefilterOptions ^= WIO_PREFILTER_ON;
}
//---------------------------------------------------------------------------

void __fastcall TWolfMainForm::ApplyIOConfig(TObject *Sender)
{
  strncpy( WolfThd_sz255SoundInputDevice, CB_SoundInputDevice->Text.c_str(), 255 );
  strncpy( WolfThd_sz255SoundOutputDevice,CB_SoundOutputDevice->Text.c_str(),255 );  
}
//---------------------------------------------------------------------------

