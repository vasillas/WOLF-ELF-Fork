//-------------------------------------------------------------------------
// File  : wolf.h
// Authors: Stewart Nelson,    KK7KA  (did the hard work and wrote the WOLF CORE)
//          Wolfgang Buescher, DL4YHF (tried to write a graphic user interface)
// Date  : 2005-04-10
// Purpose: Definitions, prototypes, and a few global variables for the WOLF CORE.
//
// Modifications (with ISO8601 date format = YYYY-MM-DD)  :
//   2005-04-10, DL4YHF:
//     - Wrote WOLF.H as a common file for WOLF.CPP (=the core)
//                                      and WOLF_GUIx.cpp (=the GUI) .
//-------------------------------------------------------------------------


//-------------------------------------------------------------------------
//  Defines
//-------------------------------------------------------------------------


#define VERSION "0.62"     /* program version number */
#define SIGLVL 0.57     /* level of test signal */
#define INTLV 1         /* 1 for interleave on */
#define KLUDGE 0     /* slew freq */


#define PI 3.14159265358979323846

#define MSGLEN 15    /* source message len in chars */
#define MSGLENW 5    /* radix 40 message len in 16-bit words */
#define WLEN 16         /* bits in 3-character coded group */
#define MSGLENB 80      /* message length in bits */

#define RRATE 6         /* reciprocal of code rate */
#define SRBITS 14    /* number of bits in encode SR */
#define HSTATE 8192     /* half number of states in encode SR */
#define NSTATE 16384    /* number of states in encode SR */
#define FBITS 40     /* number of bits to decode in front of msg */
#define BBITS 40     /* number of bits to decode in back of msg */
#define TBITS 160    /* total number of bits to decode (incl msg) */
#define TLONGW 5     /* number of longwords for above */

/* option flags in WOLF_binopts */
#define USE1P 001    /* use single best position for all frames */
#define USE1F 002    /* use single best frequency for all frames */
#define USEMF 004    /* use (approx) matched filter */
#define USEISI 010      /* use ISI correction */

#define SHOISI 0100     /* show intersymbol interference */
#define ALLOPTS (USE1P|USE1F|USEMF|USEISI|SHOISI) /* all valid options this version */


//-------------------------------------------------------------------------
//  Data types
//-------------------------------------------------------------------------

typedef unsigned short ushort;
typedef unsigned long ulong;

struct wif {
  char name[256]; // ex: char *name; but must be a STRING BUFFER since 2005-04 
  FILE *fp;
  int ct;
  int eightb;
  int eof;
};



//-------------------------------------------------------------------------
//  Global variables
//-------------------------------------------------------------------------

extern int binopts;   // WOLF options as bit combination, like USE1P

extern int nameflg;   /* explicit file name supplied ? */
extern struct wif noisef;  // NOISE file parameters
extern struct wif sigf;    // SIGNAL file parameters

extern int WolfOpt_iShowTimeOfDay;  // 1=show time of day, 0=show seconds since start

//-------------------------------------------------------------------------
//  Prototypes
//-------------------------------------------------------------------------

int  WOLF_Main(int argc, char* argv[]); // ex: main(), may be called from GUI now
void WOLF_InitDecoder(void);            // called from GUI to prepare ANOTHER decoder run
