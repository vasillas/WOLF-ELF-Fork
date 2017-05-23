/* FFT and frequency measurement routines .          */
/* Contains some highly WOLF-specific subroutines    */
/*   ( mfreq,  corr1 )                               */


#include "stdafx.h"
#include "sigparms.h"


#include <math.h>
#include <stdlib.h>
#include <stdio.h>


#define _I_AM_FFT_C_  // trick for global vars with "single-source"-principle..
#include "fft.h"

#define PI 3.14159265358979323846


void fftsetup(int pts)
{
  int i, rev, mask;
  WolfFft_nPoints = pts;

  WolfFft_nLogPoints = 0;
  for (--pts; pts; pts>>= 1)	/* compute number of levels */
    WolfFft_nLogPoints++;
  for (i = 0; i < WolfFft_nPoints/2; ++i) { /* precompute complex exponentials */
    wr[i] = cos(2.0 * PI * i / WolfFft_nPoints);
    wi[i] = -sin(2.0 * PI * i / WolfFft_nPoints);
  }
  rev = 0;
  for (i = 0; i < WolfFft_nPoints-1; i++) {
    bitrev[i] = rev;
    mask = WolfFft_nPoints >> 1;
    while (rev >= mask) {
      rev -= mask;
      mask >>= 1;
    }
    rev += mask;
  }
  bitrev[WolfFft_nPoints-1] = WolfFft_nPoints-1;
}


/*
               0   1   2   3   4   5   6   7
  level   0
  step    1                                     0
  increm  2                                   W
  j = 0        <--->   <--->   <--->   <--->   1
  level   1
  step    2
  increm  4                                     0
  j = 0        <------->       <------->      W      1
  j = 1            <------->       <------->   2   W
  level   2                                         2
  step    4
  increm  8                                     0
  j = 0        <--------------->              W      1
  j = 1            <--------------->           3   W      2
  j = 2                <--------------->            3   W      3
  j = 3                    <--------------->             3   W
                                                              3
*/

void transform()     // inpr[] ->  xr[], xi[]
{
  int level, step, increm, i, j, lsh;
  double ur, ui, tr, ti;

  for (i = 0; i < WolfFft_nPoints; ++i)
   {
    xr[bitrev[i]] = inpr[i];
    xi[bitrev[i]] = inpi[i];
   }

  step = 1;
  for (level = 0; level < WolfFft_nLogPoints; ++level)
   {
    increm = step * 2;
    lsh = WolfFft_nLogPoints - 1 - level;
    for (j = 0; j < step; ++j)
     {
      ur = wr[j << lsh];
      ui = wi[j << lsh];
      for (i = j; i < WolfFft_nPoints; i += increm)
       {
	tr = ur * xr[i+step] - ui * xi[i+step];
	ti = ui * xr[i+step] + ur * xi[i+step];
	xr[i+step] = xr[i] - tr;
	xi[i+step] = xi[i] - ti;
	xr[i] += tr;
	xi[i] += ti;
       }
     }
    step *= 2;
   }
}


double mfreq(double tol, int apts, double dblBBSamplesPerSec)
  // Used by the WOLF core, not the FFT itself .
  // Caution: various arguments are passed in global vars
  //  (which are NOW marked as such with the 'module prefix' WolfFft_ )
{
  double p, mp, v0, v1, of, mf, f, incr, ang;
  int i, j, k, itol, mpi;

  mp = -1;
  mpi = -1;
  itol = (int)(tol * WolfFft_nPoints / dblBBSamplesPerSec );
  for (k = -1-itol; k <= itol; ++k)
   {
    i = k + WolfFft_nPoints;
    if (i >= WolfFft_nPoints)
      i -= WolfFft_nPoints;
    p = xr[i]*xr[i] + xi[i]*xi[i];
    if (p > mp) {
      mp = p;
      mpi = i;
    }
  }
  of = mpi;
  if (of > WolfFft_nPoints/2)
    of -= WolfFft_nPoints;
  of = of * dblBBSamplesPerSec / WolfFft_nPoints; /* highest power bin in Hz */
  mp = mf = -1;
  for (i = -8; i <= 8; ++i)
   {
    f = of + ((double)i * dblBBSamplesPerSec / WolfFft_nPoints / 8);
    incr = 2 * PI * f / dblBBSamplesPerSec;
    ang = 0;
    v0 = v1 = 0;
    for (j = 0; j < apts; ++j)
     {
      v0 += inpr[j] * cos(ang) + inpi[j] * sin(ang);
      v1 += inpi[j] * cos(ang) - inpr[j] * sin(ang);
      ang += incr;
     }
    p = v0 * v0 + v1 * v1;
    if (p > mp)
     {
      mp = p;
      mf = f;
     }
   }
  return mf;
}

void corr1(int nbits)    /* Correlator ? called from wolf::rcvf() */
{
  int i, j, k, l, n;
  float c1, c1m;

  c1m = -1;
  for (i = 0; i < BBPBIT; ++ i) { /* try all bit phases */
    /* generate refchn array */
    for (j = 0; j < PATLEN; ++j) {
      c1 = 0;
      for (k = BBPSYM; k < BBPBIT; ++k)
	c1 += bbdat[j * BBPBIT + i + k];
      refchn[j] = c1;
    }
    /* generate nibtbl */
    for (n = 0; n < PATLEN/4; ++n) {
      for (j = 0; j < 16; ++j) {
	nibtbl[n][j] = ((j & 8) ? refchn[n*4] : -refchn[n*4]) +
	  ((j & 4) ? refchn[n*4+1] : -refchn[n*4+1]) +
	  ((j & 2) ? refchn[n*4+2] : -refchn[n*4+2]) +
	  ((j & 1) ? refchn[n*4+3] : -refchn[n*4+3]);
      }
    }
    /* search all pattern phases with this bit phase */
    for (k = 0; k < PATWDS; ++k) /* copy pattern for rotation */
      rotpat[k] = pat[k];
    for (j = 0; j < PATLEN; ++j) { /* try all pattern phases */
      n = 0;
      c1 = 0;
      for (k = 0; k < PATWDS; ++k) { /* all words in rotated pattern */
	for (l = 28; l >= 0; l-=4) /* all nibbles in word */
	  c1 += nibtbl[n++][(rotpat[k]>>l)&017];
      }
      if (c1 > c1m || -c1 > c1m) { /* new best match */
	c1m = c1, corrp= 1;
	if (c1m < 0)
	  c1m = -c1, corrp = -1;
	corrj = j;
	corri = i;
      }
      l = rotpat[0] >> 31;
      for (k = 0; k < PATWDS-1; ++k) /* rotate pattern one bit */
	rotpat[k] += rotpat[k] + (rotpat[k+1] >> 31);
      rotpat[PATWDS-1] += rotpat[PATWDS-1] + l;
    }
  }
}
