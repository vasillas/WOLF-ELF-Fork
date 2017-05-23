//-------------------------------------------------------------------------
//  FFT.H for Stwart Nelson's WOLF implementation
//    slightly modified to compile under Borland C by Wolf DL4YHF
//
//  Last changes (YYYY-MM-DD)
//   2005-04-10  Modified to run under Borland C++ Builder
//-------------------------------------------------------------------------



//-------------------------------------------------------------------------
//  Prototypes
//-------------------------------------------------------------------------

void fftsetup(int pts);
void transform();
double mfreq(double tol, int apts, double dblBBSamplesPerSec );
void corr1(int nbits);

//-------------------------------------------------------------------------
//  Defines
//-------------------------------------------------------------------------

#define MAXPOINTS 8192   // moved to FFT.H (needed for var declarations)



//-------------------------------------------------------------------------
//  Global variables (with single-source principle)
//-------------------------------------------------------------------------
#undef EXTERN
#ifdef _I_AM_FFT_C_
 #define EXTERN
#else
 #define EXTERN extern
#endif


EXTERN double inpr[MAXPOINTS];
EXTERN double inpi[MAXPOINTS];
EXTERN double xr[MAXPOINTS];
EXTERN double xi[MAXPOINTS];
EXTERN double wr[MAXPOINTS/2];
EXTERN double wi[MAXPOINTS/2];
EXTERN int bitrev[MAXPOINTS];
EXTERN int WolfFft_nPoints, WolfFft_nLogPoints; // number of FFT points, log2(.)

  // Note: PATLEN is defined in SIGPARMS.H !
EXTERN float bbdat[(PATLEN+1)*BBPBIT];   /* baseband samples after demod */
EXTERN float refchn[PATLEN];         /* extracted symbols for ref chan */
EXTERN float nibtbl[PATLEN/4][16];
EXTERN unsigned long pat[PATWDS];    // accessed in WOLF.CPP so needed in header
EXTERN unsigned long rotpat[PATWDS];

EXTERN int corri;  /* best bit phase */
EXTERN int corrj;  /* best pattern phase */
EXTERN int corrp;  /* polarity for above */

