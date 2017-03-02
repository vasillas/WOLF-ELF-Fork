//-------------------------------------------------------------------------
//  Calculation of the peak amplitude and the peak frequency
//   in a given range of FFT bins ( in German: FFT-bin = Frequenz-Eimer ) .
//   Based on a suggestion by Markus Vester (DF6NM) ..
//
// > Da wir die FFT-Bandbreite und die Fensterfunktion
// > (bzw. ihre Fouriertransformierte) kennen, wissen wir
// >  vorab schon die Krümmung k des Spektrallinien-Scheitels.
// >  Es gelte in dessen Umgebung für die Pegel (in dB) näherungsweise
// >      A(f) = Amax - k * ((f-fmax)/df)^2
// >  mit dem FFT-Bin-Abstand df.
// >  Dieses k ist also einfach die Nachbarkanalunterdrückung,
// >  wenn Du einen Träger genau in der Mitte eines Bins platziert hättest.
// >  Damit kannst Du dann so vorgehen:
// >  1. Suche das Bin mit dem höchsten Pegel im Cursor-Fensterchen.
// >  2. Wähle zusätzlich den stärkeren der beiden Frequenznachbarn aus.
// >  3. Das wirkliche Maximum liegt zwischen diesen beiden Rasterfrequenzen
// >     f1 und f2=f1+df. Für deren Amplituden gilt nach obigem
// >     A1 = Amax-k*((fmax-f1)/df)^2 bzw. A2 = Amax-k*((f2-fmax)/df)^2.
// >  Durch Auflösen der beiden Gleichungen ergeben sich die
// >     interpolierte Frequenz des Maximums :
// >     fmax = (f1+f2)/2 + df*(A2-A1)/(2*k)
// >  und dessen Pegel
// >     Amax = (A1+A2)/2 + k/4 + (A2-A1)^2/(4*k)  .
int CalcFftPeakAmplAndFrequency(
        float *pfltFftBins, int iMaxNrOfBins,  // source: complex FFT bins
        int iFftBinIndex1, int iFftBinIndex2,  // search range: index
        double *pdblPeakAmplitude, // result: peak amplitude, dB
        double *pdblPeakFrequency) // result: peak frequency offset (as multiple of bins)
   // The return value is the FFT bin index closest to the peak.
{
  int    bin_index;
  int    iPeakIndex;
  double dblPeakAmpl, dbl_dB1, dbl_dB2;
  double dblPeakFreq, dbl_f1,  dbl_f2;
  double dbl_kdB;  

  if(iFftBinIndex1<0)
     iFftBinIndex1=0;
  if(iFftBinIndex2< iFftBinIndex1)
     iFftBinIndex2 =iFftBinIndex1;
  if(iFftBinIndex2>=iMaxNrOfBins)
     iFftBinIndex2 =iMaxNrOfBins;
  if(iFftBinIndex1 > iFftBinIndex2)
     iFftBinIndex1 = iFftBinIndex2;

  // step 1 : search the FFT bin with the highest amplitude within the range..
  iPeakIndex  = iFftBinIndex1;
  dblPeakAmpl = pfltFftBins[iPeakIndex];
  for(bin_index=iFftBinIndex1; bin_index<=iFftBinIndex2; ++bin_index)
   {
     if( pfltFftBins[bin_index] > dblPeakAmpl)
      { dblPeakAmpl = pfltFftBins[bin_index];
        iPeakIndex  = bin_index;
      }
   } // end for(i...
   dblPeakFreq = (double)iPeakIndex;

   // step 2 : Improve the accuracy by interpolation .
   // Which of the two neighbours has the higher peak ?
   dbl_f1 = dbl_f2 = dblPeakFreq;
   dbl_dB1 = dbl_dB2 = dblPeakAmpl;
   if(iPeakIndex>iFftBinIndex1)
     {
       dbl_dB1 = pfltFftBins[iPeakIndex-1];
       dbl_f1 = (double)(iPeakIndex-1);
     }
    if(iPeakIndex<iFftBinIndex2)
     {
       dbl_dB2 = pfltFftBins[iPeakIndex+1];
       dbl_f2 = (double)(iPeakIndex+1);
     }
    // Arrived here:
    //  dblPeakAmpl = amplitude of middle bin,
    //  dbldB1= left neighbour, dbldB2= right neighbour

    // Overwrite the "weaker neighbour" with the center value.
    // dbl_dB1 and dbl_dB2 will be used as input for interpolation.
    if(dbl_dB2 > dbl_dB1)
     { dbl_dB1 = dblPeakAmpl;
       dbl_f1 = dblPeakFreq;
     }
    else // dB2 is the "weaker" one..
     { dbl_dB2 = dblPeakAmpl;
       dbl_f2 = dblPeakFreq;
     }

    // Note: For the interpolation, the amplitude must be converted
    // into decibels..   here already the case !

    // A constant (for cos^2-FFT-window) is a compromise between 6.02 dB and 5.6 dB ..
    dbl_kdB = 5.9;
    // Tnx DF6NM:
    // > Diese Konstante (für cos^2-FFT-Window) ist ein Kompromiss
    // > zwischen 6.02 dB und 5.6 dB,
    // > der den Restfehler für alle möglichen Trägerlagen
    // > (mittig im Eimer bis genau zwischen zwei Stühlen) minimieren soll.
    // > Damit erfolgt die eigentliche Interpolation:
    // > frc = (f1 + f2) / 2 + (f2 - f1) * (dB2 - dB1) / (2 * kdB)
    // > dBc = (dB1 + dB2) / 2 + kdB / 4 + (dB2 - dB1) * (dB2 - dB1) / (4 * kdB)
    dblPeakFreq = (dbl_f1 + dbl_f2) / 2.0
                + (dbl_f2 - dbl_f1) * (dbl_dB2 - dbl_dB1) / (2.0 * dbl_kdB);
    dblPeakAmpl = (dbl_dB1 + dbl_dB2) / 2.0
                + dbl_kdB / 4.0 + (dbl_dB2 - dbl_dB1) * (dbl_dB2 - dbl_dB1) / (4.0 * dbl_kdB);

    // > Zuletzt noch ein Plausibilitätscheck, der bei stark
    // > abweichenden Pegeln (durch Rauschen oder am Bildschirmrand)
    // > eine Extrapolation in unsinnige Frequenz-Entfernungen verhindern soll.
    // > Eigentlich soll ja der stärkere Nachbar nie mehr als 6dB
    // > unter dem Maximum sein.
    // > IF (dB2 - dB1) > 1.5 * kdB THEN frc = f2: dBc = dB2
    // > IF (dB1 - dB2) > 1.5 * kdB THEN frc = f1: dBc = dB1
    if( (dbl_dB2 - dbl_dB1) > 1.5 * dbl_kdB)
       {  dblPeakFreq = dbl_f2; dblPeakAmpl = dbl_dB2; }
    if( (dbl_dB1 - dbl_dB2) > 1.5 * dbl_kdB)
       {  dblPeakFreq = dbl_f1; dblPeakAmpl = dbl_dB1; }

    // Write some result values back to the caller's var-space :
    if(pdblPeakAmplitude)
      *pdblPeakAmplitude = dblPeakAmpl;
    if(pdblPeakFrequency)
      *pdblPeakFrequency = dblPeakFreq;


  return iPeakIndex;
} // end CalcFftPeakAmplAndFrequency()
