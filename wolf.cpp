//-------------------------------------------------------------------------
// File   : wolf.cpp
// Authors: Stewart Nelson,    KK7KA  (developed WOLF)
//          Wolfgang Buescher, DL4YHF (some modifications for the GUI variant)
// Revs   : 2001-12-15 WOLF core algorithms V0.62
//          2005-04-11 graphic user interface
//          2005-04-15 rx from soundcard
//          2005-04-24 bugs fixed, float instead of short in the matching table
//
// Purpose:     ET3 encode / decode
//     WOLF (Weak-signal Operation on Low Frequency) is a proposed
//     new signal format and protocol designed specifically for the LF bands.
//     It can be used for beacons and for two way communication.
//     For more info, visit http://www.scgroup.com/ham/wolf.html .
//
// Modifications (with ISO8601 date, YYYY-MM-DD)  :
//   2005-04-10, DL4YHF:
//     - Modified a few headers to compile WOLF with Borland C .
//     - added a few typecasts (ushort)+(char) to avoid warnings like
//        "[C++ Warning]: W8071 Conversion may lose significant digits."
//     - Replaced printf() by WIO_printf() to simplify the task of running
//       WOLF in a windows-style user interface .
//     - Added module WOLFIO_CONSOLE to the project to compile WOLF into
//       a simple console application (for compatibility and easier testing)
//     - Wrote a preliminary "graphic user interface" for WOLF using BCB V4,
//       which supports batch- and interactive mode .
//   2005-04-15, DL4YHF:
//     - real-time operation using the soundcard (and some heavy buffers)
//-------------------------------------------------------------------------



#include "stdafx.h"
#include "sigparms.h"  // YHF: include SIGPARMS before FFT.H !
#include "fft.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef BUILDING_GUI
 #include <time.h>  // using gmtime to split unix date into YYYY-MM-DD hh:mm:ss
#endif

#include "wolfio.h" // YHF: replacement for WIO_printf, etc (WOLF-Input/Output)


#define _I_AM_WOLF_C_  // trick for global vars with "single-source"-principle..
#include "wolf.h"

// 2005-04-09: Most defines and some variables moved into header WOLF.H,
//             because we may need these values in the user interface too.

   // data type for the matching tables (originally 16-bit unsigned, but may be float now)
#define USE_FLOATS_FOR_MATCHTABLE 1   /* 0=use WORD,  1=use float */


//-------------------------------------------------------------------------
//  Global variables
//-------------------------------------------------------------------------

int binopts = 0;
int WolfOpt_iShowTimeOfDay = 0;   // time-format in decoder output lines

FILE *ouf;
int wavhdr[] = {0x4952, 0x4646, 0x0000, 0x0000, 0x4157, 0x4556, 0x6d66, 0x2074,
      0x0010, 0x0000, 0x0001, 0x0001, 0x1f40, 0x0000, 0x3e80, 0x0000,
      0x0002, 0x0010, 0x6164, 0x6174, 0, 0};

int wavhdi[22];

char rxdmsg[MSGLEN+1];     /* received data in ASCII */
ushort txdr40[MSGLENW];    /* transmit data in radix 40 */
ushort rxdr40[MSGLENW];    /* received data in radix 40 */
char txdbbd[MSGLENB*RRATE];   /* transmit baseband data (0 or 1) in order sent */
char txdref[MSGLENB*RRATE];   /* transmit reference bit stream */
float rxdbbd[MSGLENB*RRATE];  /* received baseband data in order sent */
float refbbd[MSGLENB*RRATE];  /* received baseband reference in order sent */
int rxdqnt[MSGLENB*RRATE]; /* quantized received data (0 to 64) */
int verbose = 0;     /* flag for verbose output */

int frm_max = FRMMAX;     /* number of frames to decode (in soundcard mode) */

const char pnpat[] = {
0,0,1,1,1,1,1,0,1,0,1,0,1,1,0,0,0,0,1,1,0,1,0,1,1,0,0,1,0,1,0,0,1,0,0,0,1,0,0,1,
1,0,1,0,0,0,0,0,1,1,1,0,1,0,1,0,1,1,1,0,0,0,1,0,1,1,0,0,0,1,0,1,1,1,1,0,1,0,1,1,
1,1,0,0,1,0,0,0,1,0,0,1,1,1,0,1,0,1,0,0,0,1,0,1,1,0,0,0,1,0,1,0,1,1,0,1,1,0,1,0,
1,0,1,1,0,0,0,0,0,1,0,1,1,0,1,1,0,1,0,0,0,1,0,1,1,1,0,0,0,1,1,1,1,0,1,1,1,0,0,1,
1,0,1,1,0,1,0,0,1,1,1,0,1,0,0,1,1,0,1,0,1,0,0,1,0,0,0,1,1,1,0,1,0,0,1,0,0,0,1,1,
0,0,1,0,1,0,1,1,1,1,0,1,0,1,0,0,0,0,0,0,0,0,0,0,1,0,1,1,1,1,1,1,0,1,1,0,0,1,1,1,
1,1,0,1,0,0,1,0,1,0,1,0,1,1,0,1,0,0,1,0,1,0,0,0,1,0,1,0,1,1,0,0,0,0,0,0,0,0,0,0,
1,1,1,1,1,1,0,1,0,1,1,0,0,1,0,0,1,1,0,1,0,1,1,0,1,1,1,0,0,1,0,1,0,1,0,1,1,1,1,0,
1,1,0,0,1,0,0,0,0,0,0,1,1,1,1,1,0,0,1,1,0,0,0,1,1,1,1,1,0,1,0,0,0,0,0,0,1,0,0,1,
0,1,0,0,0,1,1,0,0,1,0,1,0,1,0,0,1,1,0,1,1,1,0,0,1,1,1,0,0,1,1,1,1,1,0,1,1,0,1,0,
1,0,0,1,1,1,0,0,0,0,0,1,0,0,1,0,1,0,0,0,0,1,1,0,0,0,0,1,1,0,1,0,1,1,1,0,1,0,1,1,
1,0,0,0,1,1,1,1,1,0,0,0,1,1,0,1,0,0,1,1,1,1,1,1,0,1,0,0,1,0,0,1,0,1,0,0,0,1,1,1,
};


#ifdef BUILDING_GUI  // free dynamically allocated arrays.. may call this more than once !
//-------------------------------------------------------------------------
//  Forward declarations (only required for GUI variant)
//-------------------------------------------------------------------------
 void WOLF_FreeMatchtbl(void);
 void WOLF_RestartDecoder(void);
#endif // BUILDING_GUI 


//-------------------------------------------------------------------------
//  Auxiliary routines, not WOLF-specific, but used in this module ...
//-------------------------------------------------------------------------

char *FloatToStr5( double dblValue )
  // Formats a floating-point number into a STRING with 5 characters.
  // Here: Used to display the 'pm'-value, which was an integer once.
{
  static char szResult[8];  // used as return value, so must be STATIC !

  // Better ideas how to achieve this without several if's ?  ;-)
  //  (there is one, passing the "prec"-param in a variable, but ... )
  if( dblValue < 0.0 )
   { if( dblValue > -10.0 )   sprintf(szResult, "%5.2lf", dblValue );
     else if(dblValue>-100.0) sprintf(szResult, "%5.1lf", dblValue );
     else                     sprintf(szResult, "%5.0lf", dblValue );
   }
  else // positive values :
   { if( dblValue < 10.0 )    sprintf(szResult, "%5.3lf", dblValue );
     else if(dblValue<100.0)  sprintf(szResult, "%5.2lf", dblValue );
     else if(dblValue<1000.0) sprintf(szResult, "%5.1lf", dblValue );
     else                     sprintf(szResult, "%5.0lf", dblValue );
   }
  return szResult;

} // end FloatToString5()



//-------------------------------------------------------------------------
//  WOLF core by KK7KA follows ...
//-------------------------------------------------------------------------


/* in: ASCII string, up to 15 chars
   out: array of 16 bit words, radix-40 encoded */
void encr40(char *in, ushort out[])
{
  int ch, fill, val, i;
  fill = 0;
  val = 0;
  for (i = 0; i < MSGLEN; ++i) {
    if (fill)
      ch = 0;
#pragma warn -pia            // Borland moaned "possibly incorect assignment", but
    else if (!(ch = *in++))  // indeed an assignment !
      fill = 1;
    else if (ch >= 'A' && ch <= 'Z')
      ch = ch - 'A' + 1;
    else if (ch >= 'a' && ch <= 'z')
      ch = ch - 'a' + 1;
    else if (ch >= '0' && ch <= '9')
      ch = ch - '0' + 27;
    else if (ch == ' ')
      ch = 0;
    else if (ch == '.')
      ch = 37;
    else if (ch == '/')
      ch = 38;
    else
      ch = 39;
    val = val * 40 + ch;
    if (i % 3 == 2) {
      *out++ = (ushort)val;
      val = 0;
    }
  }
}
int cnvcod[] = {042631, 047245, 073363, 056507, 077267, 064537};

/* tail-biting encoder
   in: array of radix-40 encoded 16 bit words
   out: baseband signal in xmit order, array of floats */
void enccnv(ushort in[], char out[])
{
  int sr, i, j, k, b, t;
  sr = in[MSGLENW-1];      /* init sr to last 15 bits */
  for (i = 0; i < MSGLENW; ++i) {
    for (j = 0; j < WLEN; ++j) {
      // Borland: "suggested parenthesis to clarify precedence", so ..
      // ex:  sr = (sr << 1) | (in[i] >> (15-j)) & 1;
      sr = (sr << 1) | ( (in[i] >> (15-j)) & 1);
      for (k = 0; k < RRATE; ++k) {
   b = 0;
   for (t = sr & cnvcod[k]; t; t &= t-1)
     b ^= 1;
   /* store */
#if INTLV
   out[k*MSGLENB + (j&7)*10 + ((j&8)>>3) + i*2] = (ushort)b;
#else
   out[k*MSGLENB + j + i*WLEN] = (ushort)b;
#endif
      }
    }
  }
}

char dect[] = {' ', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I',
          'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S',
          'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '0', '1', '2',
          '3', '4', '5', '6', '7', '8', '9', '.', '/', '*'};

/* decode radix-40 message, opposite of encr40 */
void decr40(ushort *in, char *out)
{
  int val, i;
  for (i = 0; i < MSGLENW; ++i) {
    val = *in++;
    if (val >= 64000) {
      *out++ = '?';
      *out++ = '?';
      *out++ = '?';
    }
    else {
      *out++ = dect[val/1600];
      *out++ = dect[(val%1600)/40];
      *out++ = dect[val%40];
    }
  }
  *out = 0;
}

/* Viterbi decoder data */
int esig0[NSTATE];      /* vector of 6 xmit bits for state if next msg bit 0 */
int esig1[NSTATE];      /* vector of 6 xmit bits for state if next msg bit 1 */
int errsofara[NSTATE];     /* minimum error for path to this state */
int errsofarb[NSTATE];     /* as above, alternating between two copies */
ulong prvstate[NSTATE][TLONGW];  /* bit table for best predecessor */
int errv[1<<RRATE];     /* penalty for actual rcvd signal versus given vector */

/* Viterbi decoder inner loop */
void viterbi(int erra[], int errb[], int biw, ulong bib)
{
  register int ins, outs, erp0, erp1;

  for (ins = 0; ins < HSTATE; ++ins) {
    outs = ins << 1; /* possible output state if 0 sent */
    erp0 = erra[ins] + errv[esig0[ins]];
    erp1 = erra[ins+HSTATE] + errv[esig0[ins+HSTATE]];
    if (erp0 < erp1) {
      errb[outs] = erp0;
    } else {
      errb[outs] = erp1;
      prvstate[outs][biw] |= bib;
    }
    erp0 = erra[ins] + errv[esig1[ins]];
    erp1 = erra[ins+HSTATE] + errv[esig1[ins+HSTATE]];
    if (erp0 < erp1) {
      errb[outs+1] = erp0;
    } else {
      errb[outs+1] = erp1;
      prvstate[outs+1][biw] |= bib;
    }
  }
}

/* tail-biting decoder
   in: array of floats in "received" order - first xmitted bit first
   out: most likely message, radix-40 coded */
int deccnv(float in[], ushort out[])
{
  int i, j, k, bi, t, sigv, biw;
  int state1, state2, statef;
  ulong bib;

  for (i = 0; i < NSTATE; ++i) {
      for (j = 0; j < TLONGW; ++j) /* clear prev states */
      prvstate[i][j] = 0;
    errsofara[i] = 0;      /* clear initial error vector */
    sigv = 0;        /* build esig0, 1 arrays */
    for (bi = 0; bi < RRATE; ++bi) {
      for (t = (i << 1) & cnvcod[bi]; t; t &= t-1)
   sigv ^= 1 << bi;
    }
    esig0[i] = sigv;
    esig1[i] = sigv ^ ((1 << RRATE) - 1);
  }

  /* setup quantized rcvd data */
  for (i = 0; i < MSGLENB * RRATE; ++i) {
    t = (int) (in[i] * 32 + 2048.5);
    if (t > 4096)
      t = 4096;
    if (t < 0)
      t = 0;
    rxdqnt[i] = t;
  }
  for (bi = 0; bi < TBITS; ++bi) { /* loop thru msg bits (incl overlap) */
    biw = bi >> 5;      /* longword number for this depth */
    bib = 1 << (bi & 037); /* bit mask for this depth */
    /* setup errv for this bit */
    k = (bi + MSGLENB - FBITS) % MSGLENB;
#if INTLV
    k = (k & 7) * 10 + (k >> 3);
#endif
    for (i = 0; i < (1 << RRATE); ++i) {
      t = 0;
      for (j = 0; j < RRATE; ++j) {
   if (i & (1 << j)) /* one would be xmit */
     t += 4096 - rxdqnt[k + j * MSGLENB];
   else
     t += rxdqnt[k + j * MSGLENB];
      }
      errv[i] = t;
    }
    /* update all states for this bit */
    if (!(bi & 1))      /* even numbered bit */
      viterbi(errsofara, errsofarb, biw, bib);
    else          /* odd numbered bit */
      viterbi(errsofarb, errsofara, biw, bib);
  }
  /* find most likely end state */
  t = errsofara[j = 0];
  for (i = 1; i < NSTATE; ++i)
    if (errsofara[i] < t)
      t = errsofara[j = i];
  statef = j;
  /* trace prev states to recover message */
  for (bi = TBITS-1; bi >= FBITS; --bi) {
    if ((biw = (bi - FBITS) >> 4) < MSGLENW) {
      out[biw] = (ushort)( (out[biw] >> 1) | ((j & 1) << (WLEN - 1)) );
    }
    t = (prvstate[j][bi >> 5] >> (bi & 037)) & 1;
    j = (j >> 1) | (t << (SRBITS-1));
    if (bi == FBITS + MSGLENB)
      state1 = j;
    if (bi == FBITS)
      state2 = j;
  }
  statef = statef;   // YHF, 'cos Borland moaned "statef never used"
  return(state1 != state2);
}

double drand48()
{
  return((double)(rand()*(RAND_MAX+1)+rand())) / ((RAND_MAX+1)*(RAND_MAX+1));
}

double rconst1 = 2.0;
double rconst2 = 0.5;
double gstore;
int ready = 0;

double gauss(void)
{
  double v1, v2, r, fac;
  if (!ready) {
    do {
      v1 = (drand48() - rconst2) * rconst1;
      v2 = (drand48() - rconst2) * rconst1;
      r = v1*v1+v2*v2;
    } while (r > 1.0);
    fac = sqrt(-2.0*log(r)/r);
    gstore = v1 * fac;
    ready = 1;
    return(v2 * fac);
  }
  else {
    ready = 0;
    return(gstore);
  }
}

void wrtwd(int w)   /* writes a 16-bit word to the output file */
{
  putc( (char)w,      ouf);
  putc( (char)(w>>8), ouf);
}

#define TXLENB 960      /* bits (incl ref) in complete transmission */
#define SAMPR 8000      /* nominal sample rate */
int samptrans = 40;     /* samples in transition (each end) */
double freq = 800.0;            /* audio center frequency in Hz */
char *txdmsg = NULL;    /* message to send (ASCII) */
double lvl = 13045;     /* 8 dB below full scale sine */
double samprate = SAMPR;   /* actual sample rate of sound card */
int sampflg = 0;     /* flag for sample rate supplied with -r */
int eightbit = 0;    /* flag for 8-bit samples */
int keyed = 0;       /* flag for keyed mode */
int sercnt = 0;         /* number of frames for output via serial port */

double atten = 1.0;
int attflg = 0;
double toler = 1.0;
int tolflg = 0;

int nameflg = 0; /* explicit file name supplied */
struct wif noisef = {"noise.wav", 0, 0, 0, 0};
struct wif sigf   = {"wolfr.wav", 0, 0, 0, 0};


void xmitser()
{
  int i, j, t;
  if (!nameflg)
    strcpy( sigf.name, "wolfx.txt");
  if (!(ouf = fopen(sigf.name, "wb"))) {
    WIO_printf("?File %s not writable\n", sigf.name);
    WIO_exit(1);
#ifdef BUILDING_GUI
    return; // required for GUI version, where "exit" doesn't quit
#endif
  }
  for (i = sercnt; i; --i) { /* repeat for desired number of frames */
    for (j = 0; j < TXLENB; ++j) { /* repeat for each bit of frame */
      if (j & 1) /* sending ref bit */
   t = txdref[j>>1];
      else /* sending data bit */
   t = txdbbd[j>>1];
      if (t)
   t = 0377;
      putc((char)t, ouf); /* write 3 chars for 1/10 sec at 300 bps */
      putc((char)t, ouf);
      putc((char)t, ouf);
    }
  }
  if (fclose(ouf)) {
    WIO_printf("error closing %s\n", sigf.name);
    WIO_exit(1);
#ifdef BUILDING_GUI
    return; // required for GUI version, where "exit" doesn't quit
#endif    
  }
  WIO_printf("Wrote %s, %d bytes\n", sigf.name, sercnt*TXLENB*3);
}

void xmiteep()
{
  int i, j, t;
  if (!nameflg)
    strcpy(sigf.name, "wolfx.bin");
  if (!(ouf = fopen(sigf.name, "wb"))) {
    WIO_printf("?File %s not writable\n", sigf.name);
    WIO_exit(1);
#ifdef BUILDING_GUI
    return; // required for GUI version, where "exit" doesn't quit
#endif
  }
  for (i = sercnt; i; ++i) { /* repeat for desired number of frames */
    for (j = 0; j < TXLENB; ++j) { /* repeat for each bit of frame */
      if (j & 1) /* sending ref bit */
   t = txdref[j>>1];
      else /* sending data bit */
   t = txdbbd[j>>1];
      if (t)
   t = 1;
      putc((char)t, ouf);
    }
  }
  putc(2, ouf);
  if (fclose(ouf)) {
    WIO_printf("error closing %s\n", sigf.name);
    WIO_exit(1);
#ifdef BUILDING_GUI
    return; // required for GUI version, where "exit" doesn't quit
#endif
  }
  WIO_printf("Wrote %s, %d bytes\n", sigf.name, 1-sercnt*TXLENB);
}

void xmit()
{
  int bit, prev, next, i, j, spf, cpf, sampbit;
  double ang, incr, env;
#ifdef BUILDING_GUI
  #define DEBUG_ENVELOPE_GLITCH
  double prev_env = 0.0;  // to find the 'envelope glitch' (2005-11-20)
#endif

  /* adjust sample rate for integer samples per frame */
#ifndef BUILDING_GUI  /* original code by KK7KA :  */
  spf = (int) (samprate * TXLENB / SYMSEC + 0.5);  // samples per frame / per file
  samprate = ((double) spf) * SYMSEC / TXLENB;

  /* adjust frequency for integer cycles per frame */
  cpf = (int) (freq * TXLENB / SYMSEC + 0.5);
  freq = ((double) cpf) * SYMSEC / TXLENB;

#else /* in the WOLF GUI, the symbol rate is variable (see SIGPAMS.H) : */
  spf = (int) (samprate * TXLENB / SigParms_dblSymPerSec + 0.5);  // samples per frame
  // Examples for 'spf' :
  // WOLF10:  SigParms_dblSymPerSec=10.0 : 96sec * 8000 Hz =  768000 samples
  // WOLF5:   SigParms_dblSymPerSec=5.0 : 2*96sec* 8000 Hz = 1536000 samples
  // WOLF2.5: SigParms_dblSymPerSec=2.5 : 4*96sec* 8000 Hz = 3072000 samples
  /* adjust sample rate for integer samples per frame : */
  samprate = ((double) spf) * SigParms_dblSymPerSec / TXLENB;

  /* adjust frequency for integer cycles per frame */
  cpf = (int) (freq * TXLENB / SigParms_dblSymPerSec + 0.5);
  // for WOLF2.5: cpf = round(1700Hz * 960bits / 2.5bps) = 652800 audio cycles
  freq = ((double) cpf) * SigParms_dblSymPerSec / TXLENB;
  // 'freq' is the REALIZEABLE center frequency now. Ideally unchanged .

#endif /* BUILDING_GUI */

  encr40(txdmsg, txdr40); /* convert to radix 40 */
  enccnv(txdr40, txdbbd);  /* form convolutional */
  for (i = 0; i < MSGLENB*RRATE; ++i)
    txdref[i] = pnpat[i];     /* bit from pn pattern */
  if (sercnt > 0) { /* want output for serial port */
    xmitser();
    return;
  }
  if (sercnt < 0) { /* want output for EEPROM */
    xmiteep();
    return;
  }
  if (!nameflg)
    strcpy(sigf.name, "wolfx.wav" );

#ifdef BUILDING_GUI
  if( ! WIO_iUseSoundcard )
#endif
   {
    if (!(ouf = fopen(sigf.name, "wb")))
     {
       WIO_printf("?File %s not writable\n", sigf.name);
       WIO_exit(1);
#ifdef BUILDING_GUI
       return; // required for GUI variant, where "WIO_exit" doesn't quit
#endif
     }
     i = spf;
     if (!eightbit)
       i *= 2;       /* if 16-bit samples */
     wavhdr[20] = i;    /* put size into wave header */
     wavhdr[21] = i >> 16;
     i += 36;        /* add size of header (except cnt and RIFF) */
     wavhdr[2] = i;
     wavhdr[3] = i >> 16;
#ifdef BUILDING_GUI
     i = WIO_GetNearestNominalSampleRate( samprate );
#else
     i = SAMPR;         /* bytes per second if 8-bit */
#endif
     wavhdr[12] = i;            // dwSamplesPerSec, low
     wavhdr[13] = i>>16;        // dwSamplesPerSec, high
     if (!eightbit)
        i *= 2;         /* bytes per second */
     wavhdr[14] = i;            // dwAvgBytesPerSec, low
     wavhdr[15] = i >> 16;      // dwAvgBytesPerSec, high
     i = eightbit ? 0x80001 : 0x100002; /* sample format */
     wavhdr[16] = i;            // (wBlockAlign)
     wavhdr[17] = i >> 16;      // (wBitsPerSample)
     for (i = 0; i < 22; ++i) /* put out wave header */
       wrtwd(wavhdr[i]);
   } // end if( ! WIO_iUseSoundcard )
  if (attflg)        /* overriding default level */
    lvl = atten * 256 * 127;
  else if (keyed)
    lvl = 127*256;      /* max amplitude */
  if (tolflg)        /* overriding default transition time */
    samptrans = (int)(spf * toler / (TXLENB * 2));
  else if (keyed)    /* keyed mode */
    samptrans = 0;      /* instant transitions */
  if( eightbit
#ifdef BUILDING_GUI
      && ! WIO_iUseSoundcard   // ignore eight-bit-flag when using soundcard
#endif
    )
   { lvl /= 256;                /* adjust scale for 8-bit samples */
   }
  ang = 0;
  incr = 2 * PI * freq / samprate;
  bit = txdbbd[0];
  prev = bit ^ 1;
  next = txdref[0];
  for (i = 0; i < TXLENB; ++i)   /* each bit to be sent */
   {
    /* compute number of samples for this bit */
    // ex: sampbit = (i+1)*spf/TXLENB - i*spf/TXLENB;  // fails for "WOLF 2.5"
    // 2005-11-20:
    //    There were occasional bugs (glitches) in the envelope at slow modes.
    //    For WOLF2.5, it happened after 74% of the frame.
    //    See Output_Error_26percent_WOLF2_5.gif .
    //    'sampbit' was 3200, and 'samptrans' was 1600 then .
    // The error happened at i=700 and j=0, spf was 307200 .
    // Here we go: 3072000*701  =: 2.15347e+09
    //                    2^31  =: 2.14748e+09   .  RING RING !
    //    Too much for a poor little 32-bit integer value
    //    (in the calculation of sampbit) !   Cure: use 64-bit integer.
#ifndef LONGLONG
 #define LONGLONG __int64
#endif
    sampbit = (int)(   ( (LONGLONG)(i+1)*(LONGLONG)spf/(LONGLONG)TXLENB )
                     - ( (LONGLONG) i   *(LONGLONG)spf/(LONGLONG)TXLENB )
                   );  
    for (j = 0; j < sampbit; ++j)  /* each sample to be sent (within "bit") */
     {
      if (prev != bit && j < samptrans)
   env = sin(j * (PI / (2 * samptrans)));
      else if (bit != next && j > sampbit-samptrans)
   env = sin((sampbit-j) * (PI / (2 * samptrans)));
      else
   env = 1.0;
#ifdef DEBUG_ENVELOPE_GLITCH // to find the 'envelope glitch' (2005-11-20)
      if( (fabs( env-prev_env ) > 0.1) && (i>0) && (i<TXLENB-1) )
       {
        WIO_printf("Envelope glitch at bit #%d, j=%d : env=%lg, prev=%lg\n",
            (int)i, (int)j, (double)env, (double)prev_env );
       }
      prev_env = env;
#endif // DEBUG_ENVELOPE_GLITCH

      if (bit)
   env = -env;
      else if (keyed)
   env = 0;
#ifdef BUILDING_GUI
      if( WIO_iUseSoundcard )
          WIO_SendSampleToSoundcard( lvl * env * sin(ang) );
      else  // output not to SOUNDCARD, but WAVE FILE :
#endif
       { if (eightbit)
           putc((char)(int)(128.5 + lvl * env * sin(ang)), ouf);  // print TO FILE
         else
           wrtwd((int)(lvl * env * sin(ang)));
       }
      ang += incr;
      if( ang > (double)2.0*PI )
          ang -= 2.0*PI;   // avoid losing accuracy by too large argument
     } // end for <each sample to send>
    prev = bit;
    bit = next;
    if (i & 1)                  /* sent ref bit */
     {
      if (i == TXLENB-1)   /* last bit */
   next = bit ^ 1;
      else
   next = txdref[(i+1)>>1];
     }
    else       /* sent msg bit */
     { next = txdbbd[(i+2)>>1];
     }

#if( 0 )   // FOR TESTING ONLY !
    if(i >= (TXLENB-25) )  /* deliberately kill the last N bits */
       next = 0;  /* 10 seconds @ 2.5 bps "unmodulated carrier" for buffer test */
#endif // TEST
  }
#ifdef BUILDING_GUI
  if( WIO_iUseSoundcard )
   {   // ... soundcard managed by calling application !
   }
  else
#endif // BUILDING_GUI
   {
     if (fclose(ouf))
      {
        WIO_printf("error closing %s\n", sigf.name);
        WIO_exit(1);
#ifdef BUILDING_GUI
        return; // required for GUI version, where "exit" doesn't quit
#endif
      }
     i = wavhdr[20] + 44;
     WIO_printf("Wrote %s, %d bytes, at %6.3f samples/sec., %6.3f Hz\n", sigf.name, i, samprate, freq);
  }
}

short bbufr[(PATLEN*(FRMMAX+1)+1) * BBPBIT/*16*/ ];
short bbufi[(PATLEN*(FRMMAX+1)+1) * BBPBIT/*16*/ ];

int rdwif(struct wif *fil)
{
  int c, i;
  if (!fil->ct) {
    if (fil->eof)  /* already saw eof */
      return 0;
    if (fil->fp) { /* read this file at least once */
      if (!attflg) { /* will not read in loop */
   ++fil->eof; /* mark eof seen */
   return 0;
      }
      fclose(fil->fp);
    }
    if (!(fil->fp = fopen(fil->name, "rb"))) {
      WIO_printf("?File %s not found\n", fil->name);
      WIO_exit(1);
#ifdef BUILDING_GUI
      return 0;
#endif
    }
    for (i = 0; i < 18; ++i) {   /* get wave header, except DATA chunk */
      wavhdi[i] = getc(fil->fp);
      wavhdi[i] += getc(fil->fp) << 8;
    }
    if (wavhdi[0] != wavhdr[0]) {

      WIO_printf("?File %s unsupported format\n", fil->name);
      WIO_exit(1);
#ifdef BUILDING_GUI
      return 0; // required for GUI version, where "exit" doesn't quit
#endif
    }
    c = (wavhdi[13] << 16) + wavhdi[12]; /* nominal sample rate */
#ifdef BUILDING_GUI
    if( c != (int)samprate )
#else
    if (c != SAMPR)
#endif
     {         /* not the expected 8000 (or 11025) ? */
      WIO_printf("File recorded at nominal sample rate of %d.\n", c);
      if (!sampflg)
           samprate = c;
     }
    for (; wavhdi[8] > 16; --wavhdi[8]) /* skip any extra FMT chunk bytes */
      getc(fil->fp);
    while (1) { /* read DATA or other chunk */
      for (i = 18; i < 22; ++i) {   /* get DATA chunk */
   wavhdi[i] = getc(fil->fp);
   wavhdi[i] += getc(fil->fp) << 8;
      }
      if (wavhdi[18] == 0x6164 && wavhdi[19] == 0x6174) /* DATA chunk */
   break;
      while (wavhdi[20]--)
   getc(fil->fp); /* skip rest of unknown chunk */
    }
    c = (wavhdi[21] << 16) + wavhdi[20]; /* data length in bytes */
    if (wavhdi[16] == 1) {  /* 8-bit file */
      fil->ct = c;
      fil->eightb = 1;
      if (verbose)
   WIO_printf("Reading %d 8-bit samples.\n", fil->ct);
    }
    else if (wavhdi[16] == 2) { /* 16-bit file */
      fil->ct = c >> 1;
      fil->eightb = 0;
      if (verbose)
   WIO_printf("Reading %d 16-bit samples.\n", fil->ct);
    }
    else {
      WIO_printf("?File %s unsupported sample format\n", fil->name);
      WIO_exit(1);
#ifdef BUILDING_GUI
      return 0; // required for GUI version, where "exit" doesn't quit
#endif
    }
  }
  --fil->ct;
  if (fil->eightb) {
    c = (getc(fil->fp) - 128) << 8;
  }
  else {
    c = getc(fil->fp);
    c += getc(fil->fp) << 8;
    if (c >= 32768)
      c -= 65536;
  }
  return c;
}

#define SMBB 0 /* seems to make things worse when on */
#define NBLNK 1
#if NBLNK
int nbwid = 5;
int nbct = 5;
int nbi = 0;     // noise blanker index ?
double nbuf[10];
double nblim = 0;
double nbfac = 1.0;
#endif

double slew = 0;
void rcvbb(int pt,   // number baseband samples processed earlier
           int npts, // number of baseband samples to process now
           double dblBBSamplesPerSec )  // param: baseband samples per second
{ // WB: First processing stage for reception. Multiplies received audio samples
  // with NCO, and accumulates the I+Q output in an integrate-and-dump
  // buffer.
  // Called from rcvs() for each new frame ,
  //  immediately after starting the decoder, returns after 24 seconds (WOLF 10) .
  //    1st call : pt= 0  , npts= 1936  (immediately, t=0)
  //    2nd call : pt=1936, npts= 1920  ( t=24 s )
  //    3rd call : pt=3856, npts= 3840  ( t=48 s )
  //    4th call : pt=7696, npts= 7680  ( t=96 s, now called from rcvs() )
  // Notes WB (thinking about possible improvements) :
  //  This is a simple form of decimation, but does it give the max possible
  //  rejection of signals which are just outside the WOLF passband ?
  // ToDo: Check if a chain of half band filters (like in SpecLab's PSK decoder)
  //       would improve this, especially make it more immune against LORAN-like
  //       carriers which are always "very near".
  int i, j, sampn;
#if NBLNK
  double ss;
#endif
  double ang, incr, s, accr, acci;
#if SMBB
  double fac, accr0, acci0, accr1, acci1, accrs, accis;
  accrs = accis = 0;
#endif
  double dblAudioSamplesPerBasebandSample = samprate / dblBBSamplesPerSec ;
   // -> dblAudioSamplesPerBasebandSample will typically be 100, but not always integer.
   // Note: this was once 'samprate/BBPSEC', see definition of this macro in sigparms.h.

  incr = 2 * PI * freq / samprate;  // increment step for 'local oscillator'
  ang = incr * (int) ( pt * dblAudioSamplesPerBasebandSample ); // L.O. phase angle for next sample
  for (i = pt; i < pt+npts; ++i)
   {
    accr = acci = 0;
#if SMBB
    accr1 = acci1 = 0;
#endif
    sampn = (int) ( (double)(i + 1) * dblAudioSamplesPerBasebandSample ) - (int) ( (double)i * dblAudioSamplesPerBasebandSample);
    // 'sampn' is the number of audio samples processed in one integration cycle.
    //  sampn = circa samprate/BBSEC = 8000/80 +/- X = 100 +/- 1 .
    for (j = 0; j < sampn; ++j)
     {
      // Get the next sample...
#ifdef BUILDING_GUI // building the GUI : ...from WAVE FILE or SOUNDCARD !
      if(WIO_MustTerminate() || ((WIO_iRestartDecoderFlags & WIO_FLAG_4_WOLF_CORE)!=0) )
          return; // stop after user pressed ESCAPE, or clicked "Restart Decoder"
      if(WIO_iUseSoundcard)
          s = WIO_GetSampleFromSoundcard() * atten;
      else
#endif // ! BUILDING_GUI
        { s = ((double)rdwif(&sigf)) * atten;
        }

      if(attflg      // attenuated "input", add noise for testing ?
#ifdef BUILDING_GUI
         && (! WIO_iUseSoundcard)
#endif
        )
       {
#if 1
   s += (double) rdwif(&noisef);
#else
   s += gauss()*9663;
#endif
       } // end if (attflg)
#if NBLNK
      if (nblim) {
   if (nbct < 0)
     ss = nbuf[nbi] * nbfac;
   else
     ss = 0;
   nbuf[nbi++] = s;
   if (nbi >= nbwid)
     nbi = 0;
   if (s > nblim || s < -nblim)
     nbct = nbwid * 2;
   else
     nbct--;
   s = ss;
      }
#endif
#if SMBB
      fac = 0.5 * (1.0 - cos(PI * j  / sampn));
      accr0 = s * cos(ang);
      acci0 = s * sin(ang);
      accr += accr0 * (1-fac);
      acci += acci0 * (1-fac);
      accr1 += accr0 * fac;
      acci1 += acci0 * fac;
#else
      accr += s * cos(ang);
      acci -= s * sin(ang);
#endif
      ang += incr;
    } // end for (j.. )
#if SMBB
    accr = .01 * (accr + accrs);
    acci = .01 * (acci + accis);
    accrs = accr1;
    accis = acci1;
#else
    accr *= .01;
    acci *= .01;
#endif
//    WIO_printf("%10.3f %10.3f\n", accr, acci);

    bbufr[i] = (short) accr;
    bbufi[i] = (short) acci;
    freq += slew / dblBBSamplesPerSec; /* slew frequency to compensate for drifting rcvr */

   } // end for < i >

} // end rcvbb()


#ifdef BUILDING_GUI
static char * WOLF_TimeToString(int time_sec)
{
  long i32;
  struct tm *utc;
  static char sz20Result[24];

  if( WolfOpt_iShowTimeOfDay ) // 1=show time of day, 0=show seconds since start
   {
     i32 = (long)WIO_dblStartTime + time_sec;
     utc = gmtime(&i32);
     sprintf(sz20Result,"%02d:%02d:%02d",   // use ISO 8601 date format !
      (int)utc->tm_hour,      (int)utc->tm_min,   (int)utc->tm_sec );
   }
  else
   { sprintf(sz20Result,"t:%4d", (int)time_sec );
   }
  return sz20Result;

} // end WOLF_TimeToString()
#endif // def BUILDING_GUI

void rcvf(int nbits, double dblBBSamplesPerSec )
  // Note: iBBSamplesPerSec replaces BBPSEC now, it's not a constant any longer
{
  int i, t, j, err, tbits;
  double sr, si, f, ang, pow;

  tbits = nbits +1; /* includes extra bit to allow for bit phasing */
  /* square signal to get carrier */
  for (i = 0; i < tbits * BBPBIT; ++i)
   {
    inpr[i] = bbufr[i] * bbufr[i] - bbufi[i] * bbufi[i];
    inpi[i] = bbufr[i] * bbufi[i] * 2;
   }

  /* clear rest of FFT input buffer */
  for (; i < WolfFft_nPoints; ++i)
    inpr[i] = inpi[i] = 0;


  /* do FFT ( inpr[] ->  xr[], xi[] ) */
  transform();

  /* measure 2 x carrier frequency */
  f = mfreq(toler*2, tbits * BBPBIT/*16*/ , dblBBSamplesPerSec  );
  /* determine 2 x carrier phase */
  sr = si = ang = 0;
  for (i = 0; i < tbits * BBPBIT; ++i)
   {
    sr += inpr[i] * cos(ang) + inpi[i] * sin(ang);
    si += inpi[i] * cos(ang) - inpr[i] * sin(ang);
    ang += 2 * PI * f / dblBBSamplesPerSec; // ex:   ang += 2 * PI * f / BBPSEC;
   }
  if(fabs(sr)<1e-30)  // YHF: prevent DOMAIN error in atan2 when sr almost zero:
     ang = 0.0;  // (happend after restarting the decoder, when "all" was zero)
  else
     ang = atan2(si, sr)/2; /* phase of carrier */
  f /= 2;  /* freq of carrier */
  pow = (sr * sr + si * si) / ( (float)tbits * (float)tbits * BBPBIT/*16*/ * BBPBIT );
#ifndef BUILDING_GUI
  t = nbits / BITSEC;
#else
  t = (int)((double)nbits / (0.5*SigParms_dblSymPerSec) );
#endif
     // ( add 'epsilon' in log10() to prevent DOMAIN error when there is no signal )
  WIO_printf("%s f:%6.3f a:%4.1f dp:%5.1f ",WOLF_TimeToString(t),
                 f,      ang,  10*log10(pow+1e-30));

  /* demodulate to real baseband */
  pow = 0;
  for (i = 0; i < tbits * BBPBIT; ++i) {
    bbdat[i] = (float) (bbufr[i] * cos(ang) + bbufi[i] * sin(ang));
    pow += bbdat[i] * bbdat[i];
    ang += 2 * PI * f / dblBBSamplesPerSec; // ex: ang += 2 * PI * f / BBPSEC;
  }
  pow /= tbits * BBPBIT;
  /* match with pattern */
  corr1(nbits);
  WIO_printf("ci:%2d cj:%3d ", corri, corrj);
  /* get data symbol values into rxdbbd */
  for (i = 0; i < PATLEN; ++i)
    rxdbbd[i] = 0;
  for (i = 0; i < nbits; ++i) {
    sr = 0;
    for (j = 0; j < BBPSYM; ++j)
      sr += bbdat[i * BBPBIT + j + corri];
    rxdbbd[(i + corrj) % PATLEN] = (float)(sr * corrp * 0.0005);
  }


  /* decode message */
  err = deccnv(rxdbbd, rxdr40); /* attempt to decode */
  decr40(rxdr40, rxdmsg);  /* back to ASCII */
  WIO_printf("%s %s\n",rxdmsg, err ? "?" : "-"); /* display result */
#ifdef BUILDING_GUI // for GUI variant only :
  if( ! err )  // pass message text from DECODER THREAD to auto-RX/TX-machine if valid
   { strcpy( WIO_sz80RcvdMessage, rxdmsg );
     ++WIO_iRcvdMessageCount;   // increment *AFTER* WIO_sz80RcvdMessage has been set
   }
#endif // BUILDING_GUI
}

#if ( USE_FLOATS_FOR_MATCHTABLE )
# define MATCHTBL_TYPE float
#else
# define MATCHTBL_TYPE unsigned short
#endif
MATCHTBL_TYPE *matchtbl[SYMLEN];  /* dynamically allocated arrays */
   // SYMLEN = number of symbols in frame, originally 960
float bbdatr[(PATLEN+1)*BBPBIT];   /* baseband samples after demod */
float bbdati[(PATLEN+1)*BBPBIT];   /* baseband samples after demod */
int refj;
double refp, reff, refa;
#define MBINSOFF 7         /* number of 1/8 bins offset +/- to try */
double refsr[FRMMAX][MBINSOFF*2+1][BBPBIT+1]; /* ref sig re per frame per freq per pos */
double refsi[FRMMAX][MBINSOFF*2+1][BBPBIT+1]; /* ref sig im per frame per freq per pos */
double refpw[FRMMAX][MBINSOFF*2+1][BBPBIT+1]; /* ref sig pow per frame per freq per pos */

void refine(int fnum, int idx, double oldf)
{
  int i, j, k, l, m;
  float sr, si, tr, ti;
  double ang, f, pow;
  double dblBBSamplesPerSec = /*baseband samples per second (once BBPSEC from sigparms.h) */
   SigParms_dblSymPerSec * (double)BBPSYM;  /* .. now depends on variable signalling rate */


  if( idx < 0 )
      idx = 0;  // set breakpoint here (should never trap)

  refp = -1;
  for (i = -MBINSOFF; i <= MBINSOFF; ++i)
   { /* try freqs in 1/8 bin intervals */
    m = idx;
    f = oldf + ((double)dblBBSamplesPerSec * i / WolfFft_nPoints / 8);
    ang = 2 * PI * f * m / dblBBSamplesPerSec;
    for (k = 0; k < (PATLEN+1) * BBPBIT; ++k)
     { // 2005-04-18: crashed here under win98, m=-40501248
      bbdatr[k] = (float) (bbufr[m] * cos(ang) + bbufi[m] * sin(ang));
      bbdati[k] = (float) (bbufi[m] * cos(ang) - bbufr[m] * sin(ang));
      ang += 2 * PI * f / dblBBSamplesPerSec;
      ++m;
     }
    for (j = 0; j <= BBPBIT; ++j) { /* try various bit phases */
      sr = si = 0;
      m = j;
      for (k = 0; k < PATLEN; ++k) { /* add all ref bits */
   m += BBPSYM; /* skip over data bit */
   tr = ti = 0;
   for (l = 0; l < BBPSYM; ++l) { /* add all bb for ref sym */
          tr += bbdatr[m];
     ti += bbdati[m++];
   }
   if (pnpat[k])
     sr += tr, si += ti;
   else
     sr -= tr, si -= ti;
      }
      pow = sr*sr+si*si;
      refsr[fnum][i+MBINSOFF][j] = sr;
      refsi[fnum][i+MBINSOFF][j] = si;
      refpw[fnum][i+MBINSOFF][j] = pow;
    }
  }
}

int bestfi = 0;
int bestj = 8;
int avgfrm = 1;

void refine1(int nfrm)
{
  int i, fi, j;
  double p, p1, pmax;


  pmax = 0;
  switch (binopts & (USE1F|USE1P)) {
  case 0: /* no special constraints */
    break;

  case 1: /* use one position */
    for (j = 0; j < BBPBIT; ++j) {
      p1 = 0;
      for (i = 0; i < nfrm; ++i) {
   p = 0;
   for (fi = 0; fi <= MBINSOFF*2; ++fi)
     if (refpw[i][fi][j] > p)
       p = refpw[i][fi][j];
   p1 += p;
      }
      if (p1 > pmax)
   pmax = p1, bestj = j;
    }
    break;

  case 2: /* use one frequency */
    for (fi = 0; fi <= MBINSOFF*2; ++fi) {
      p1 = 0;
      for (i = 0; i < nfrm; ++i) {
   p = 0;
   for (j = 0; j < BBPBIT; ++j)
     if (refpw[i][fi][j] > p)
       p = refpw[i][fi][j];
   p1 += p;
      }
      if (p1 > pmax)
   pmax = p1, bestfi = fi;
    }
    break;

  case 3: /* use one frequency and position */
    for (fi = 0; fi <= MBINSOFF*2; ++fi) {
      for (j = 0; j < BBPBIT; ++j) {
   p = 0;
   for (i = 0; i < nfrm; ++i)
     p += refpw[i][fi][j];
   if (p > pmax)
     pmax = p, bestfi = fi, bestj = j;
      }
    }
    break;
  }
}

void refine2(int fnum, int nfrm, double oldf)
{
  int i, j, fi, imin, imax;
  double sr, si, p;
  double dblBBSamplesPerSec = /*baseband samples per second (once BBPSEC from sigparms.h) */
   SigParms_dblSymPerSec * (double)BBPSYM;  /* .. now depends on variable signalling rate */



  /* compute range of frames for phase measurement */
#if 0
  if (avgfrm > nfrm) /* want to avg over more than we have - use all */
    imin = 0, imax = nfrm;
  else {
    imin = fnum - (avgfrm >> 1);
    imax = fnum + ((avgfrm + 1) >> 1);
    if (imin < 0)
      imax += -imin, imin = 0;
    if (imax > nfrm)
      imin -= imax - nfrm, imax = nfrm;
  }
#else
  imin = fnum - (avgfrm >> 1);
  imax = fnum + ((avgfrm + 1) >> 1);
  if (imin < 0)
    imin = 0;
  if (imax > nfrm)
    imax = nfrm;
#endif

  refp = 0;
  for (fi = 0; fi <= MBINSOFF*2; ++fi) { /* try freqs corresponding to 1/8 bin intervals */
    if (binopts & USE1F && fi != bestfi) /* not to use this freq */
      continue;
    for (j = 0; j <= BBPBIT; ++j) {
      if (binopts & USE1P && j != bestj) /* not to use this position */
   continue;
      sr = si = 0;
      for (i = imin; i < imax; ++i) { /* sum selected frames this freq and pos */
   sr += refsr[i][fi][j];
   si += refsi[i][fi][j];
      }
      p = sr * sr + si * si;
      if (p > refp) { /* new best match */
   refp = p;
   reff = oldf + (dblBBSamplesPerSec * (fi - MBINSOFF) / WolfFft_nPoints / 8); /* save freq */
   refj = j; /* bit phase */
   refa = atan2(si, sr); /* and carrier phase */
      }
    }
  }
  if (verbose)
   { 
    WIO_printf("fn=%d of=%6.3f p=%4.1f f=%6.3f a=%4.1f j=%d ", fnum, oldf, 10*log10(refp+1e-30), reff, refa, refj);
   }
}

#if KLUDGE
int kptr = 0;    // used in rcvs() 
double ktbl[] = {.366, .288, .201, .152, .093, .063, .036, .013, -.024, -.032, -.050, -.053, -.034,
-.043, -.061, -.094, -.121, -.143, -.152 -.152};
#endif

int lastflg = 0; /* take position and freq from last frame only */

/* display S/N of received reference or message */
void qualprt(float bbd[], const char std[])
{
  int i;
  double d, sig, pow;

  sig = 0;
  pow = 0;
  for (i = 0; i < MSGLENB*RRATE; ++i) {
    if (std[i])
      sig += bbd[i];
    else
      sig -= bbd[i];
    pow += bbd[i] * bbd[i];
  }
  sig = sig * sig / (MSGLENB*RRATE); /* total signal power */
  pow -= sig;             /* total noise power */
  if(sig>1e-30 && pow>1e-30) // prevent DOMAIN error in log10()
       d = 10*log10(sig/pow);
  else d = -99.9;
  WIO_printf("%5.1f", d ); // S/N calculation ok
}


int isicnt[8];
double isisum[8];

void isicorr()
{
  int i, j;
  double sig, adj;

  sig = 0;
  for (i = 0; i < PATLEN; ++i) {
    if (pnpat[i])
      sig += refbbd[i];
    else
      sig -= refbbd[i];
  }
  sig /= PATLEN;

  for (i = 0; i < PATLEN; ++i) {
    j = i + 1;
    if (j == PATLEN)
      j = 0;
    adj = sig * 0.12;
    if (pnpat[i])
      adj = -adj;
    rxdbbd[i] += (float)adj;
    rxdbbd[j] += (float)adj;
  }
}

void isianal()
{
  int i, j, k;

  for (i = 0; i < 8; ++i)
    isisum[i] = isicnt[i] = 0;
  for (i = 0; i < PATLEN; ++i) {
    k = (i ? i-1 : PATLEN-1);
    // Borland moaned "Suggest parentheses to clarify precedence", so :
    // ex: j = pnpat[k] << 2 | txdbbd[i] << 1 | pnpat[i];
    j = (pnpat[k] << 2) | (txdbbd[i] << 1) | pnpat[i];
    ++isicnt[j];
    isisum[j] += rxdbbd[i];
  }
  for (i = 0; i < 8; ++i)
    WIO_printf("%d %4.1f\n", isicnt[i], (isicnt[i] ? isisum[i] / isicnt[i] : 0));

}

double fact[] = {0.6, 1.0, 1.2, 1.2, 1.2, 1.2, 1.0, 0.6};

void rcvs(double dblBBSamplesPerSec ) // WOLF decoder (called after 96 sec, doesn't return until finished! )
{
  int i, j, k, l, itol, itoln, p, jm, km, err;
#if ( USE_FLOATS_FOR_MATCHTABLE )
  float  pw, pwm;
#else
  int    pw, pwm;
#endif
  double fac, sr, rr, rrt, f, ang, inf;


  fac = 0.05 / (WolfFft_nPoints * WolfFft_nPoints);
  /* allocate and clear memory */
  itol = (int)(toler * WolfFft_nPoints / dblBBSamplesPerSec);
  itoln = itol * 2 + 3; /* number of bins to keep (something like itoln=411) */
  for (i = 0; i < SYMLEN; ++i)  // in sigparms.h: SYMLEN=960 (symbols in frame)
   { // Matching-table size estimation when using floats, tolerance = 2 Hz :
     //     960*411*4 byte = roughly 1.5 MByte (acceptable these days)
    matchtbl[i] = (MATCHTBL_TYPE *)malloc(itoln * sizeof(MATCHTBL_TYPE));
    if (matchtbl[i] == NULL)
     {
      WIO_printf("insufficient memory - try smaller freq tolerance\n");
      WIO_exit(1);
#ifdef BUILDING_GUI
      return; // required for GUI version, where "exit" doesn't quit
#endif
     }
    for (j = 0; j < itoln; ++j)
      matchtbl[i][j] = 0;
   }

  if(frm_max<1 || frm_max>FRMMAX) // added by YHF for soundcard mode (-d option)
     frm_max = FRMMAX;


  /* for each new frame */        // YHF: here at t=72 ...
  for (i = 0; i < frm_max; ++i)
   {
    /* read it in */
#if KLUDGE
    freq += ktbl[kptr+1] - ktbl[kptr];
    ++kptr;
#endif
    rcvbb( (PATLEN*(i+1)+1)*BBPBIT, PATLEN*BBPBIT, dblBBSamplesPerSec );
    /* do spreads and FFTs */
    for (j = 0; j < SYMLEN; ++j)
     {
#ifdef BUILDING_GUI // required for GUI version which may be multi-threaded :
      if(WIO_MustTerminate() || ((WIO_iRestartDecoderFlags & WIO_FLAG_4_WOLF_CORE)!=0) )
         return;
#endif

      /* despread to get carrier */
      l = i * PATLEN * BBPBIT + j * BBPSYM + (BBPSYM*3/2);
      for (k = 0; k < PATLEN * BBPBIT; ++k) {
   p = pnpat[k>>4] ? 1 : -1;
   inpr[k] = bbufr[l] * p;
   inpi[k] = bbufi[l++] * p;
      }
      /* clear rest of FFT input buffer */
      for (; k < WolfFft_nPoints; ++k)
   inpr[k] = inpi[k] = 0;

      /* do FFT ( inpr[] ->  xr[], xi[] ) */
      transform();           // 2005-04-23 23:10 no crash here until FFT #j=213

      /* scan relevant output bins */
      for (k = 0; k < itoln; ++k) {
   l = k-1-itol;
   if (l < 0)
     l += WolfFft_nPoints;
#if ( USE_FLOATS_FOR_MATCHTABLE )
   pw = (xr[l] * xr[l] + xi[l] * xi[l]) * fac;
#else
   pw = (int) ((xr[l] * xr[l] + xi[l] * xi[l]) * fac);
   if (pw > 4095)
     pw = 4095;
#endif // USE_FLOATS_FOR_MATCHTABLE
   if (pw < 0) {
     WIO_printf("%d\n", pw);
     WIO_exit(1);
   }
   if (lastflg)
     matchtbl[j][k] = (MATCHTBL_TYPE)pw;
   else
     matchtbl[j][k] += (MATCHTBL_TYPE)pw;
      }
    }
    /* find best freq and pos */
    pwm = 0;
    jm=km=0; // added by DL4YHF, to avoid crash due to negative array index
             // in refine() etc . Local vars are not initialized by default .
    for (j = 0; j < SYMLEN; ++j)
     {
       for (k = 0; k < itoln; ++k)
        {
          if (matchtbl[j][k] > pwm)
           {
             pwm = matchtbl[j][k];
             jm = j;
             km = k;
           }
        }
     }
    inf = (float)(km-1-itol) * dblBBSamplesPerSec / WolfFft_nPoints;
    // The 'jm:' value gives the number of bits (0 to 959)
    //  to the best matched frame start. If three or more
    //  identical values appear in 'l' mode, there is probably a signal.
    // The steady growth of 'pm' values shows that a valid reference
    //  is being received. If pm increases by 4000 or more in one frame,
    //  WOLF is being overloaded and the signal level should be reduced.
    //  The 'pm:' values can give clues to times of best propagation,
    //  and you can then use the 's' option to focus in on that data.
#if ( USE_FLOATS_FOR_MATCHTABLE )
    WIO_printf("%s f:%6.3f pm:%s jm:%3d ",
      WOLF_TimeToString( (int)( (double)(i+2)*(double)SYMLEN/SigParms_dblSymPerSec) ),
                     inf, FloatToStr5(pwm), jm);
#else // old variant with 16-bit INTEGERS in the matching table:
    WIO_printf("t:%4d f:%6.3f pm:%5d jm:%3d ",(i+2)*SYMLEN/SYMSEC, inf, (int)pwm, jm);
#endif
    if (verbose)
      WIO_printf("\n");
    /* clear out rxdbbd */
    for (j = 0; j < PATLEN; ++j) {
      rxdbbd[j] = 0;
      refbbd[j] = 0;
    }
    for (j = 0; j < i+1; ++j) {  /* for each frame in current group */
      /* refine frequency and bit phase */
      l = j*PATLEN*BBPBIT + jm*BBPSYM; /* initial index to search for framing */
      refine(j, l, inf); /* compute ref sig for various freqs and positions */
      // YHF: 'l' must not be negative, reson for crash ?
    }

    refine1(i+1); /* apply any constraints on frequency or position */
    for (j = 0; j < i+1; ++j) {  /* for each frame in current group */
      refine2(j, i+1, inf); /* get freq, phase, and position to use */
      /* demod to real baseband */
      f = reff;
      l = j*PATLEN*BBPBIT + jm*BBPSYM + refj; /* initial index for frame */
      ang = refa + 2 * PI * f * l / dblBBSamplesPerSec;
      for (k = 0; k < PATLEN * BBPBIT; ++k) {
   bbdat[k] = (float) (bbufr[l] * cos(ang) + bbufi[l] * sin(ang));
   ++l;
   ang += 2 * PI * f / dblBBSamplesPerSec;
      }
      /* add symbol values into rxdbbd */
      rrt = 0;
      for (k = 0; k < PATLEN; ++k) {
   sr = 0;
   rr = 0;
   p = pnpat[k] ? 1 : -1;
   if (binopts & USEMF) {
     for (l = 0; l < BBPSYM; ++l) {
       sr += bbdat[k * BBPBIT + l] * fact[l];
       rr += bbdat[k * BBPBIT + l + BBPSYM] * fact[l];
     }
   }
   else {
     for (l = 0; l < BBPSYM; ++l) {
       sr += bbdat[k * BBPBIT + l];
       rr += bbdat[k * BBPBIT + l + BBPSYM];
     }
   }
   rxdbbd[k] += (float)(sr * 0.0005);
   refbbd[k] += (float)(rr * 0.0005);
   rrt += rr * p;
      }
      if (verbose)
   WIO_printf("r=%6.0f\n", rrt * 0.0005);
    }

    /* decode message */
    if (binopts & USEISI)
      isicorr();
    err = deccnv(rxdbbd, rxdr40); /* attempt to decode */
    enccnv(rxdr40, txdbbd);     /* reencode for signal quality test */
    WIO_printf("q:");
    qualprt(refbbd, pnpat);     /* reference quality */
    qualprt(rxdbbd, txdbbd);    /* mesaage quality */
    decr40(rxdr40, rxdmsg);   /* back to ASCII */
#if KLUDGE
    WIO_printf("f=%6.3f ", freq);
#endif
    WIO_printf(" %s %s\n",rxdmsg, err ? "?" : "-"); /* display result */
    if (binopts & SHOISI)    /* display ISI info */
      isianal();
    if (sigf.eof)    /* stop if out of data */
      break;
#ifdef BUILDING_GUI // required for GUI version which may be multi-threaded :
    if( ! err )  // pass message text from DECODER THREAD to auto-RX/TX-machine if valid
     { strcpy( WIO_sz80RcvdMessage, rxdmsg );
       ++WIO_iRcvdMessageCount;   // increment *AFTER* WIO_sz80RcvdMessage has been set
     }
    if(WIO_MustTerminate() || ((WIO_iRestartDecoderFlags & WIO_FLAG_4_WOLF_CORE)!=0) )
      return;
#endif
   } // end for (i = 0; i < frm_max; ++i)
}


int mflag = 0;    // 'mflag' isn't just a flag, it's the number of seconds for something

void measure()    // frequency- and drift measurement
{
  int i, time;
  double sr, si, f, ang, pow;
  int iBBSamplesPerSec = 80;  // baseband samples per second .
        // 80 is the original 'BBPSEC'-value from sigparms.h ,
        // which is now called "WOLF 10" (10 symbols per second) .
        // 'mflag' seems to be the number of seconds per measuring interval !
  int iBBSamplesPerBlock = mflag * iBBSamplesPerSec;

  time = 0;
  while (1)  /* do to end of file (or end of real-time processing..) */
   {
    rcvbb(0, iBBSamplesPerBlock,  iBBSamplesPerSec );
    /* copy carrier . */
    for (i = 0; i < iBBSamplesPerBlock; ++i)
     {
      inpr[i] = bbufr[i];
      inpi[i] = bbufi[i];
     }
    /* clear rest of FFT input buffer */
    for (; i < WolfFft_nPoints; ++i)
      inpr[i] = inpi[i] = 0;

    /* do FFT  ( inpr[] ->  xr[], xi[] ) */
    transform();

    /* measure carrier frequency */
    f = mfreq(toler, iBBSamplesPerBlock, iBBSamplesPerSec );  // in fft.cpp

    /* determine carrier amp, phase */
    sr = si = ang = 0;
    for (i = 0; i < iBBSamplesPerBlock; ++i)
     {
      sr += inpr[i] * cos(ang) + inpi[i] * sin(ang);
      si += inpi[i] * cos(ang) - inpr[i] * sin(ang);
      ang += 2 * PI * f / (double)iBBSamplesPerSec;
     }
    if(fabs(sr)<1e-30) // YHF: prevent DOMAIN error in atan2 when sr almost zero
       ang = 0.0;
    else
       ang = atan2(si, sr); /* phase of carrier */
    pow = (sr * sr + si * si) / (float)( iBBSamplesPerBlock * iBBSamplesPerBlock );
    time += mflag;
    WIO_printf("%s f:%6.3f a:%4.1f dp:%5.1f\n", WOLF_TimeToString(time),
                     f, ang, 10*log10(pow+1e-30) );
    if (sigf.eof)
      return;
#ifdef BUILDING_GUI // required for GUI version which may be multi-threaded :
    if(WIO_MustTerminate() || ((WIO_iRestartDecoderFlags & WIO_FLAG_4_WOLF_CORE)!=0) )
      return;
#endif
  }
}

/* generate complex waveform of carrier */
#define BITPCH 80 /* bits per chunk */
#define CHPFRM 6  /* chunks per frame */
void monitor()    /* carrier monitor (-m option with negative parameter?) */
{
  int i, k, km, kmax, l, m, n, nfrm, ch, off, offm, pp, itol, itoln;
  double p, p0, pm, tr, ti, sr, si;
  double dblBBSamplesPerSec = /*baseband samples per second (once BBPSEC from sigparms.h) */
   SigParms_dblSymPerSec * (double)BBPSYM;  /* .. now depends on variable signalling rate */


  /* read in file (or receive from soundcards, decimated into "baseband samples") */
  rcvbb(0, (PATLEN+1)*BBPBIT , dblBBSamplesPerSec ); /* get first frame */

  for (i = 0; i < FRMMAX; ++i)
   {
    if (sigf.eof)
      break;
    rcvbb((PATLEN*(i+1)+1)*BBPBIT, PATLEN*BBPBIT, dblBBSamplesPerSec );
   }
  nfrm = i;
  WIO_printf("processing %d frames\n", nfrm);
  if (nfrm == 0)
    return;

  if (txdmsg) {
    encr40(txdmsg, txdr40); /* convert to radix 40 */
    enccnv(txdr40, txdbbd);   /* form convolutional */
  }

  fftsetup(8192);       // setup the WOLF-specific(!) FFT module
  itol = (int)(toler * WolfFft_nPoints / BITSEC);
  itoln = itol * 2 + 3; /* number of bins to keep */

  /* locate ref */
  pm = 0;
  for (off = 0; off < PATLEN*BBPBIT; ++off) {
    p = 0;
    m = off;
    pp = 0;
    n = 0;
    for (ch = 0; ch < nfrm * CHPFRM; ++ch) { /* do all chunks */
      sr = si = 0;
      for (k = 0; k < BITPCH; ++k) { /* add all ref bits in chunk */
   tr = ti = 0;
   if (txdmsg) {
     if (txdbbd[pp]) {
       for (l = 0; l < BBPSYM; ++l) { /* add all bb for data sym */
         tr += bbufr[m];
         ti += bbufi[m++];
       }
     }
     else {
       for (l = 0; l < BBPSYM; ++l) { /* add all bb for data sym */
         tr -= bbufr[m];
         ti -= bbufi[m++];
       }
     }
   }
   else
     m += BBPSYM; /* skip over data bit */
   if (pnpat[pp++]) {
     for (l = 0; l < BBPSYM; ++l) { /* add all bb for ref sym */
       tr += bbufr[m];
       ti += bbufi[m++];
     }
   }
   else {
     for (l = 0; l < BBPSYM; ++l) { /* add all bb for ref sym */
       tr -= bbufr[m];
       ti -= bbufi[m++];
     }
   }
   sr += tr, si += ti;
   if (n < 8192) {
     inpr[n] = tr; /* store for poss fft */
     inpi[n++] = ti;
   }
      }
      p += sr * sr + si * si;
      if (pp >= PATLEN)
   pp = 0;
    }
    if (mflag < -1) { /* using FFT mode */
      /* clear rest of FFT input buffer */
      for (; n < WolfFft_nPoints; ++n)
   inpr[n] = inpi[n] = 0;

      /* do FFT  ( inpr[] ->  xr[], xi[] )  */
      transform();

      /* scan relevant output bins */
      p = -1;
      for (k = 0; k < itoln; ++k) {
   l = k-1-itol;
   if (l < 0)
     l += WolfFft_nPoints;
   p0 = xr[l] * xr[l] + xi[l] * xi[l];
   if (++l >= WolfFft_nPoints)
     l = 0;
   p0 += xr[l] * xr[l] + xi[l] * xi[l];
   if (p0 > p) {
     p = p0;
     km = k;
   }
      }
    }
    if (p > pm) { /* new max */
      pm = p;
      offm = off;
      kmax = km;
    }
  }
  WIO_printf("offset = %d, pmax = %f, kmax = %d\n", offm, pm, kmax);

  /* demod */
  strcpy(sigf.name, "wolfm.txt");
  if (!(ouf = fopen(sigf.name, "w"))) {
    WIO_printf("?File %s not writable\n", sigf.name);
    WIO_exit(1);
#ifdef BUILDING_GUI
    return; // required for GUI version, where "exit" doesn't quit
#endif
  }
  m = offm;
  pp = 0;
  for (ch = 0; ch < nfrm * CHPFRM; ++ch) { /* do all chunks */
    sr = si = 0;
    for (k = 0; k < BITPCH; ++k) { /* add all ref bits in chunk */
      tr = ti = 0;
      if (txdmsg) {
   if (txdbbd[pp]) {
     for (l = 0; l < BBPSYM; ++l) { /* add all bb for data sym */
       tr += bbufr[m];
       ti += bbufi[m++];
     }
   }
   else {
     for (l = 0; l < BBPSYM; ++l) { /* add all bb for data sym */
       tr -= bbufr[m];
       ti -= bbufi[m++];
     }
   }
      }
      else
   m += BBPSYM; /* skip over data bit */
      if (pnpat[pp++]) {
   for (l = 0; l < BBPSYM; ++l) { /* add all bb for ref sym */
     tr += bbufr[m];
     ti += bbufi[m++];
   }
      }
      else {
   for (l = 0; l < BBPSYM; ++l) { /* add all bb for ref sym */
     tr -= bbufr[m];
     ti -= bbufi[m++];
   }
      }
      sr += tr, si += ti;
    }
    fprintf(ouf, "%d\t%4.1f\t%4.1f\n", ch, sr*.001, si*.001);
    if (pp >= PATLEN)
      pp = 0;
  }
  if (fclose(ouf)) {
    WIO_printf("error closing %s\n", sigf.name);
    WIO_exit(1);
#ifdef BUILDING_GUI
    return; // required for GUI version, where "exit" doesn't quit
#endif
  }
}


int skip = 0;
void rcv()  // decoder function, called from main()
{
  int i, j, k;
  unsigned long p;
  double dblBBSamplesPerSec = /*baseband samples per second (once BBPSEC from sigparms.h) */
   SigParms_dblSymPerSec * (double)BBPSYM;  /* .. now depends on variable signalling rate */


  for (i = 0; i < MSGLENB*RRATE; ++i)
    rxdbbd[i] = 0;
  fftsetup(8192);       // setup the WOLF-specific(!) FFT module

  /* setup packed pat for correlator */
  k = 0;
  p = 0;
  for (i = 0; i < PATWDS; ++i)
   {
    for (j = 0; j < 32; ++j)
      p = p + p + pnpat[k++];
    pat[i] = p;
   }
  noisef.ct = sigf.ct = 0;
  noisef.fp = sigf.fp = 0;
  if (attflg && !nameflg)
    strcpy(sigf.name,"wolfx.wav"); /* read own Tx output */

#ifdef BUILDING_GUI // building the GUI : ...from WAVE FILE or SOUNDCARD !
  if(! WIO_iUseSoundcard) // skip samples only when processing WAVE FILE..
#endif
   {
    while (--skip >= 0)
     { rdwif(&sigf);
#ifdef BUILDING_GUI // required for GUI version which may be multi-threaded :
       if(WIO_MustTerminate() )
          return;
#endif
     }
   }
  if (mflag > 0) { /* doing freq measurement */
    measure();
    return;
  }
  if (mflag < 0) { /* doing carrier monitor */
    monitor();
    return;
  }
  for (i = 0; i < 3; ++i)
   {
    if (!i) /* first time */
      rcvbb(0, (PATLEN/4+1)*BBPBIT, dblBBSamplesPerSec );
    else
      rcvbb((PATLEN/4*i+1)*BBPBIT, PATLEN/4*i*BBPBIT, dblBBSamplesPerSec );
    rcvf((PATLEN/4)<<i, dblBBSamplesPerSec ); /* rcv 1/4, 1/2, whole */
    /* YHF: crashed in rcvf(), atan2(), when running for 2nd time */
    if (sigf.eof)
      return;
#ifdef BUILDING_GUI // required for GUI version which may be multi-threaded :
    if(WIO_MustTerminate() || ((WIO_iRestartDecoderFlags & WIO_FLAG_4_WOLF_CORE)!=0) )
      return;
#endif
   }
  rcvs(dblBBSamplesPerSec);  // 2005-04-23: called at t=96. Crashed here after RX-TX-RX ?

#if 0
  /* determine carrier phase */
  sr = si = ang = 0;
  for (i = 0; i < PATLEN * BBPBIT; ++i) {
    sr += inpr[i] * cos(ang) + inpi[i] * sin(ang);
    si += inpi[i] * cos(ang) - inpr[i] * sin(ang);
    ang += 2 * PI * f / dblBBSamplesPerSec;
  }
  ang = atan2(si, sr); /* phase of carrier */
  pw = (sr * sr + si * si) / (PATLEN*BBPBIT);
  WIO_printf("f:%6.3f a:%4.1f p:%5.1f\n", f, ang, 10*log10(pw+1e-30));

  for (i = 0; i < PATLEN; ++i)
    rxdbbd[i] = 0;
  rcvbb((PATLEN+1)*BBPBIT, PATLEN*BBPBIT*15, dblBBSamplesPerSec );
  for (i = 0; i < 16; ++i) { /* try 16 frames */
    for (j = 0; j < PATLEN; ++j) {
      sr = 0;
      for (k = 0; k < BBPSYM; ++k)
   sr += bbufi[(i*PATLEN+j)*BBPBIT+k];
      rxdbbd[j] += (float)(sr * .0005);
    }
    if (!(i & 1))
      continue;
    err = deccnv(rxdbbd, rxdr40); /* attempt to decode */
    decr40(rxdr40, rxdmsg);   /* back to ASCII */
    WIO_printf("%d %s %x\n", i+1, rxdmsg, err); /* display result */
#ifdef BUILDING_GUI // required for GUI version which may be multi-threaded :
    if( ! err )  // pass message text from DECODER THREAD to auto-RX/TX-machine if valid
     { strcpy( WIO_sz80RcvdMessage, rxdmsg );
       ++WIO_iRcvdMessageCount;   // increment *AFTER* WIO_sz80RcvdMessage has been set
     }
    if(WIO_MustTerminate() || ((WIO_iRestartDecoderFlags & WIO_FLAG_4_WOLF_CORE)!=0) )
      return;
#endif // BUILDING_GUI
  } // end for < 16 frames >
#endif
}

#if (defined __BORLANDC__) || (defined BUILDING_GUI)
 int WOLF_Main(int argc, char* argv[])  // here: main function called from GUI
#else
 int main(int argc, char* argv[])       // original code: main function in WOLF
#endif
{
  int ap;
#ifdef BUILDING_GUI
  long i32;
  struct tm *utc;  
#endif


#ifndef BUILDING_GUI
  WIO_printf("WOLF version %s\n", VERSION);
#endif

  // Set default values for some settings, since the GUI may call
  // WOLF_Main() more than once !
  eightbit = attflg = keyed = nameflg = sampflg = sercnt = mflag = tolflg = 0;
  samptrans = 40; freq = 800.0; txdmsg = NULL;
  lvl = 13045;
#ifdef BUILDING_GUI
  samprate = SAMPR;
#else
  samprate = SAMPR;
#endif
  atten = 1.0;  toler = 1.0;
  verbose = 0;
#ifdef BUILDING_GUI
  for(int i = 0; i < SYMLEN; ++i)  // need initialized pointers here ...
   { matchtbl[i] = NULL;   // to avoid illegal "cleanup" at the end of main()
   }
#endif


#ifdef BUILDING_GUI
  i32 = (long)WIO_dblStartTime;
  utc = gmtime(&i32);
  // 2005-11-20: Show the speed in this pseudo-command line too:
  WIO_printf("%04d-%02d-%02d %02d:%02d:%02d >WOLF%lg ",   // use ISO 8601 date format !
      (int)utc->tm_year+1900, (int)utc->tm_mon+1, (int)utc->tm_mday,
      (int)utc->tm_hour,      (int)utc->tm_min,   (int)utc->tm_sec ,
      (double)SigParms_dblSymPerSec );   // WOLF2.5 WOLF5 WOLF10 WOLF20 or WOLF40
#endif

  for (ap = 1; ap < argc; ++ap)
   {
#ifdef BUILDING_GUI
    WIO_printf(" %s", argv[ap]);  // echo command line arguments
#endif
    if (argv[ap][0] != '-')
      {
        WIO_printf("bad option %s\n", argv[ap]);
        return(1);
      }
    switch (argv[ap][1]) {
    case 'a':        /* set attenuation of test data below noise */
      if (++ap >= argc) {
   WIO_printf("missing attenuation value\n");
   return(1);
      }
#ifdef BUILDING_GUI
      WIO_printf(" %s", argv[ap]);
#endif
      atten = strtod(argv[ap], NULL);
      if (atten < 0)
   atten = - atten;
      atten = pow(10, -atten/20.0);
      ++attflg;
      break;
    case 'b':           /* set noise blanker */
      if (++ap >= argc) {
   WIO_printf("missing blanker threshold\n");
   return(1);
      }
#ifdef BUILDING_GUI
      WIO_printf(" %s", argv[ap]);
#endif
      nblim = strtod(argv[ap], NULL);
      nbfac = 32000.0 / nblim;
      break;
    case 'c':           /* set coherent averaging */
      if (++ap >= argc) {
   WIO_printf("missing frame count\n");
   return(1);
      }
#ifdef BUILDING_GUI
      WIO_printf(" %s", argv[ap]);
#endif
      avgfrm = strtol(argv[ap], NULL, 0);
      if (avgfrm > FRMMAX)
          avgfrm = FRMMAX;
      break;

    case 'd':   /* set decoder frame count (number of frames for soundcard mode) */
      if (++ap >= argc)
       { WIO_printf("missing decoder frame count\n");
         return(1);
       }
#ifdef BUILDING_GUI
      WIO_printf(" %s", argv[ap]);
#endif
      frm_max = strtol(argv[ap], NULL, 0);
      if (frm_max<1 || frm_max>FRMMAX)
          frm_max = FRMMAX;
      break;

    case 'e':           /* set 8-bit mode */
      ++eightbit;
      break;
    case 'f':           /* set frequency */
      if (++ap >= argc) {
   WIO_printf("missing frequency\n");
   return(1);
      }
#ifdef BUILDING_GUI
      WIO_printf(" %s", argv[ap]);
#endif
      freq = strtod(argv[ap], NULL);
      break;
    case 'k':           /* set keyed (xor gate) mode */
      ++keyed;
      break;
    case 'l':           /* look at last frame only for freq and pos */
      ++lastflg;
      break;
    case 'm':           /* set measurement interval */
      if (++ap >= argc) {
   WIO_printf("missing interval\n");
   return(1);
      }
#ifdef BUILDING_GUI
      WIO_printf(" %s", argv[ap]);
#endif
      mflag = strtol(argv[ap], NULL, 0);
      if (mflag > 100)   // not just a flag, but a time interval in SECONDS
          mflag = 100;
      break;
    case 'n':    /* set frame count (serial mode) */
      if (++ap >= argc) {
   WIO_printf("missing frame count\n");
   return(1);
      }
#ifdef BUILDING_GUI
      WIO_printf(" %s", argv[ap]);
#endif
      sercnt = strtol(argv[ap], NULL, 0);
      if (sercnt > 1000)
   sercnt = 1000;
      if (sercnt < -16)
   sercnt = -16;
      break;
    case 'o':           /* set binary options */
      if (++ap >= argc) {
   WIO_printf("missing options value\n");
   return(1);
      }
#ifdef BUILDING_GUI
      WIO_printf(" %s", argv[ap]);
#endif
      binopts = strtol(argv[ap], NULL, 8);
      if (binopts & ~ALLOPTS) {
   WIO_printf("invalid options value\n");
   return(1);
      }
      break;
    case 'q':           /* specify filename */
      if (++ap >= argc) {
   WIO_printf("missing filename\n");
   return(1);
      }
#ifdef BUILDING_GUI
      WIO_printf(" %s", argv[ap]);
#endif
      strcpy(sigf.name, argv[ap] );
      ++nameflg;
      break;
    case 'r':           /* specify actual sample rate */
      if (++ap >= argc) {
   WIO_printf("missing sample rate\n");
   return(1);
      }
#ifdef BUILDING_GUI
      WIO_printf(" %s", argv[ap]);
#endif
      samprate = strtod(argv[ap], NULL);
      ++sampflg;
      break;
    case 's':           /* specify number of samples to skip */
      if (++ap >= argc) {
   WIO_printf("missing skip count\n");
   return(1);
      }
#ifdef BUILDING_GUI
      WIO_printf(" %s", argv[ap]);
#endif
      skip = strtol(argv[ap], NULL, 0);
      break;

    case 't':           /* specify frequency tolerance */
      if (++ap >= argc) {
   WIO_printf("missing tolerance\n");  /* (or phase TRANSITION time for tx) */
   return(1);
      }
#ifdef BUILDING_GUI
      WIO_printf(" %s", argv[ap]);
#endif
      toler = strtod(argv[ap], NULL);
      ++tolflg;
      break;
    case 'u': /* set user-defined / alternate display formats */
      break;
    case 'v':           /* set verbose mode */
      ++verbose;
      break;
    case 'w':           /* set slew */
      if (++ap >= argc) {
   WIO_printf("missing slew rate\n");
   return(1);
      }
#ifdef BUILDING_GUI
      WIO_printf(" %s", argv[ap]);
#endif
      slew = strtod(argv[ap], NULL);
      break;
    case 'x':           /* specify message to xmit */
      if (++ap >= argc) {
   WIO_printf("missing message\n");
   return(1);
      }
#ifdef BUILDING_GUI
      WIO_printf(" \"%s\"", argv[ap]);
#endif
      txdmsg = argv[ap];
      break;
    default:
      WIO_printf("bad option %s\n", argv[ap]);
      return(1);
    }
  }
#ifdef BUILDING_GUI
  if(argc>0)
     WIO_printf("\n");
#endif

  if (txdmsg && !mflag)
    xmit();         // produce audio samples for TX
  else
   {
#ifdef BUILDING_GUI // required for GUI version which may be multi-threaded :
    do   // here we may run in a THREAD LOOP !
     {
      rcv();
      if( ((WIO_iRestartDecoderFlags & WIO_FLAG_4_WOLF_CORE)!=0) )
       {  // got here because the operator wants to "restart" the decoder,
          // maybe because he QSY'ed or thinks the decoder got helplessly lost:
         WIO_iRestartDecoderFlags &= ~WIO_FLAG_4_WOLF_CORE;
         WOLF_FreeMatchtbl();
         WOLF_RestartDecoder();
         i32 = (long)WIO_dblCurrentTime;  // may be GPS-disciplined one fine day !
         WIO_printf("%02d:%02d:%02d >WOLF RESTARTED\n", (int)((i32/3600)%24), (int)((i32/60)%60), (int)(i32%60) );
       }
      else
        break;
     }while( ! WIO_MustTerminate() );
#else  // not building the GUI :
    rcv();
#endif
   } // end else


#ifdef BUILDING_GUI  // free dynamically allocated arrays.. may call this more than once !
  WOLF_FreeMatchtbl();
#endif

  return(0);
} // end WOLF_Main()

#ifdef BUILDING_GUI  // free dynamically allocated arrays.. may call this more than once !
//--------------------------------------------------------------------------
void WOLF_FreeMatchtbl(void)
{
  for(int i = 0; i < SYMLEN; ++i)
   { if( matchtbl[i] != NULL )
      { free( matchtbl[i] );
        matchtbl[i] = NULL;
      }
   }
}
#endif // BUILDING_GUI

#ifdef BUILDING_GUI  // only for the GUI variant, not required for CONSOLE
//--------------------------------------------------------------------------
void WOLF_RestartDecoder(void)
{
  int i,j,k;

  for( i=0; i<MSGLEN; ++i)
    rxdmsg[i] = 0;
  for( i=0; i<MSGLENW; ++i)
    txdr40[i] = rxdr40[i] = 0;
  for( i=0; i<MSGLENB*RRATE; ++i)
   { txdbbd[i] = txdref[i] = '\0';
     rxdbbd[i] = refbbd[i] = 0.0;
     rxdqnt[i] = 0;
   }

  /* init Viterbi decoder data */
  for( i=0; i<NSTATE; ++i)
    esig0[i] = esig1[i] = errsofara[i] = errsofarb[i] = 0;
  for( i=0; i<NSTATE; ++i)
    for( j=0; j<TLONGW; ++j)
      prvstate[i][j] = 0;
  for( i=0; i<(1<<RRATE); ++i)
    errv[i] = 0;

  /* refine(), refine(1), refine(2), ...    */
  for(i=0; i<((PATLEN+1)*BBPBIT); ++i)
      bbdatr[i] = bbdati[i] = 0;
  refj = refp = reff = refa = 0;
  for(i=0; i<FRMMAX; ++i)
    for(j=0; j<(MBINSOFF*2+1); ++j)
      for(k=0; k<(BBPBIT+1); ++k)
       { refsr[i][j][k] = 0.0;
         refsi[i][j][k] = 0.0;
         refpw[i][j][k] = 0.0;
       }
  bestfi = 0;
  bestj  = 8;
  avgfrm = 1;

  /* rcvs(), ... */
#if KLUDGE
  kptr = 0;
#endif

  /* rcvbb()    */
  //  nbwid = 5;    // constant parameter, not variable
  nbct = 5;         // variable, not constant
  nbi = 0;          // noise blanker index ?
  for(i=0; i<10; ++i)
    nbuf[i] = 0;
  //  nblim = 0;        // set through command line argument, don't clear here
  nbfac = 1.0;

  /* isicorr(), ... */
  for(i=0; i<8; ++i)
   { isicnt[i] = isisum[i] = 0;
   }

} // end WOLF_RestartDecoder()
#endif // BUILDING_GUI



#ifdef BUILDING_GUI  // only for the GUI variant, not required for CONSOLE
//--------------------------------------------------------------------------
void WOLF_InitDecoder(void)
   // Set most variables to the 'reset' values to prepare a new decoder run.
   // Added by DL4YHF 2005-04-09 because the user interface
   //       may launch WOLF more than once .
   // Called BEFORE parsing the command line !
{

  /* init "wave file" info.. but don't forget the FILE NAMES : */
  noisef.fp = NULL;
  noisef.ct = noisef.eightb = noisef.eof = 0;
  sigf.fp = NULL;
  sigf.ct = sigf.eightb = sigf.eof = 0;


  /* random generator */
  ready = 0;

  /* rcv()      */
  skip = 0;

  /* rcvbb()    */
  nbwid = 5;    // constant parameter, not variable
  nblim = 0;    // set through command line argument (-b)

  /* measure()  */
  mflag = 0;    // option, set via command line

  /* misc....   */
  lastflg = 0;


  WOLF_RestartDecoder();  // <<< all "variables" are initialized here

} // end WOLF_InitDecoder()
#endif // BUILDING_GUI

