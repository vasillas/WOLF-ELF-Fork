//-------------------------------------------------------------------------
//  Calculation of the peak amplitude and the peak frequency
//   in a given range of FFT bins ( in German: FFT-bin = Frequenz-Eimer ) .
//   Based on a suggestion by Markus Vester (DF6NM) .
//-------------------------------------------------------------------------

extern "C" int CalcFftPeakAmplAndFrequency(
        float *pfltFftBins, int iMaxNrOfBins,  // source: complex FFT bins
        int iFftBinIndex1, int iFftBinIndex2,  // search range: index
        double *pdblPeakAmplitude,  // result: peak amplitude, dB
        double *pdblPeakFrequency); // result: peak frequency offset (as multiple of bins)
   // The return value is the FFT bin index closest to the peak.

