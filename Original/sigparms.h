/* signal parameters */

#define BBPSYM 8   /* baseband samples per symbol (doesn't depend on bitrate) */
#define BBPBIT 16  /* baseband samples per bit    (doesn't depend on bitrate) */

#ifndef BUILDING_GUI
 #define BBPSEC 80  /* baseband samples per second, depends on bitrate ! */
                    /* ( BBPSEC = SYMSEC * BBPSYM = 10 * 8)              */
 #define SYMSEC 10  /* symbols per second (data bits + PN bits)  */
 #define BITSEC 5   /* bits per second (netto, w/o sync bits)    */
#else   /* VARIABLE signal parameters for DL4YHF's GUI variant : */
 /* here, the WOLF signalling rate may be as low as 2.5 symbols/second */
 /* This makes BBPSEC, SYMSEC and BITSEC variable,                     */
 /*   but not PATLEN, SYMLEN, PATWDS, and FRMMAX .                     */
 #define SYMSEC 2   /* MINIMUM symbols per second (here: data bits + PN bits) */
                    /* (replace by SigParms_dblSymPerSec where applicable)    */
 #define BITSEC 1   /* MINIMUM bits per second (netto, w/o sync bits)         */
                    /* (replace by 0.5*SigParms_dblSymPerSec where applicable)*/
 #define BBPSEC (SigParms_dblSymPerSec*(double)BBPSYM) /* baseband samples per second */
#endif /* BUILDING_GUI */
 // signal parameters which are NOT affected by the signalling rate :
#define PATLEN 480 /* length of PN pattern            */
#define SYMLEN 960 /* number of symbols in frame      */
#define PATWDS 15  /* number of longwords to hold pattern */
#define FRMMAX 32  /* max number of frames to process */

#ifdef BUILDING_GUI  /* VARIABLE signal parameters for DL4YHF's GUI variant : */
 //  Here, some signal "parameters" are in fact VARIABLE,
 //   and calculated during run-time . They replace some of the above settings.
 extern double SigParms_dblSymPerSec; // RAW bitrate (including PN for sync), originally = 10.0)
#endif // BUILDING_GUI
