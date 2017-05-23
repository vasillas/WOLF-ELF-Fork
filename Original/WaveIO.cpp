/***************************************************************************/
/*  WaveIO.cpp:                                                            */
/*   Unit to import and export *.wav - files and similar..                 */
/*  - by DL4YHF August '2000  ...  November 2005                           */
/*  - based on the description of the WAVE-file format                     */
/*    found at http://www.wotsit.org  (excellent site with file formats!)  */
/*  - does NOT use any fancy windows gimmics, just standard file I/O.      */
/*  - has also been converted into a plain-vanilla "C" source in June 2003 */
/*     which compiles happily under Borland C for DOS, see \cproj\WaveTool */
/*-------------------------------------------------------------------------*/

#include <windows.h> // definition of data types like LONGLONG (wtypes.h) etc
#include <stdlib.h>
#include <stdio.h>
#include <io.h>          /* _rtl_open and other file routines       */
#include <fcntl.h>       /* O_RDONLY  and other file flags          */
#include <time.h>
#include "utility1.h"    // some helper routines by DL4YHF
// #include <exception>

#pragma hdrstop
#pragma warn -8004 // <var is a assigned a value that is never used> - so what

#define _I_AM_WAVE_IO_ 1
#include "WaveIO.h"  // header for this file with "single source"-variables


T_Float GuessExactFileSampleRate(T_Float dblIndicatedSRate)
{
  // Try to be smart and reconstruct a 'precise' sample rate.
  // A guess (true in 90% of all cases) :
  // Sampling rate is a standard soundcard sampling rate
  // or some "decimated" value from that.
  // Decimation factors can be any combination of 2^n * 3^m , 0..n..8
  T_Float dblStdRate[6]={ 5512.5, 8000, 11025, 22050, 44100, 48000 };

  for( int iStdRateIndex = 0; iStdRateIndex<6; ++iStdRateIndex )
   {
    int iDec2Factor = 1;
    for( int iDecBy2=0; iDecBy2<8; ++iDecBy2 )
     {
      int iDec3Factor = 1;
      for( int iDecBy3=0; iDecBy3<8; ++iDecBy3 )
       {
        T_Float dblGuessedRate = dblStdRate[ iStdRateIndex ]
                        / ( (T_Float)iDec2Factor * (T_Float)iDec3Factor );
        // If the difference between 'indicated' and 'guessed' sample rate
        // is less than 1 Hz: Bingo, we've found what we're looking for.
        T_Float d=dblIndicatedSRate - dblGuessedRate;
        if( d>-1 && d<1)
           return dblGuessedRate;  // example from VLF OpenLab experiments: 612->612.5

        iDec3Factor *= 3;
       } // end for( iDecBy3.. )
      iDec2Factor *= 2;
     } // end for( iDecBy2.. )
   }
  return dblIndicatedSRate; // nothing found, what can it be...
} // end GuessExactFileSampleRate()


/*------------- Implementation of the class  C_WaveIO ---------------------*/

/***************************************************************************/
C_WaveIO::C_WaveIO()
  /* Constructor of the Wave-IO Class.
   * Initializes all properties etc.
   */
{
   memset( &m_RiffHdr,     0 , sizeof(T_WAV_RIFF_HDR)     );
   memset( &m_ChunkHdr,    0 , sizeof(T_WAV_CHUNK_HDR)    );
   memset( &m_ChunkFormat, 0 , sizeof(T_WAV_CHUNK_FORMAT) );

   m_FileHandle       = -1;
   m_OpenedForReading = FALSE;
   m_OpenedForWriting = FALSE;
   m_dblStartTime     = 0.0;
   m_lCurrFilePos     = 0;
   m_lFileDataSize    = 0;
   m_cErrorString[0] = '\0';

   m_dbl_SampleRate = 11025.0;      // initial guess, only important for RAW files
   m_wRawFileBitsPerSample = 8;     // just a GUESS for reading RAW files,
   m_wRawFileNumberOfChannels = 1;  // should be properly set before opening such files

} // end of C_WaveIO's constructor

/***************************************************************************/
C_WaveIO::~C_WaveIO()
  /* Destructor of the Wave-IO Class.
   *  - Flushes all output files of this class (if any)
   *  - Closes all handles (if still open)
   *  - Frees all other resources that have been allocated.
   */
{


} // end C_WaveIO's destructor


/***************************************************************************/
BOOL C_WaveIO::InOpen( char *pszFileName, T_Float fltReadingSampleRate )
  /* Opens a wave file for READING it's samples.
   * Returns TRUE on success and FALSE on any error.
   */
{
 int i;
 T_WAV_CHUNK_HDR  chunk_hdr;
 struct ftime f_time;
 char *cp;
 BOOL fExtensionIsWav;
 BOOL fSubtractRecLengthFromTime = FALSE;
 BOOL ok = TRUE;
 BOOL found;

  CloseFile();   // Close "old" file if it was open

  // Look at the file extension..
  cp = strrchr(pszFileName, '.');
  if(cp)
       fExtensionIsWav = (strnicmp(cp,".wav",4)==0);
  else fExtensionIsWav = FALSE;
  m_iFileFormat = WAVE_FORMAT_UNKNOWN;  // not sure whether WAVE_FORMAT_RAW or WAVE_FORMAT_RIFF yet !


  // Erase old chunks:
  memset( &m_RiffHdr,     0 , sizeof(T_WAV_RIFF_HDR)     );
  memset( &m_ChunkHdr,    0 , sizeof(T_WAV_CHUNK_HDR)    );
  memset( &m_ChunkFormat, 0 , sizeof(T_WAV_CHUNK_FORMAT) );
  memset( m_sz255FileName,0 , 255);
  // removed from here: m_dbl_SampleRate = 0.0;  // still unknown (!)

  m_FileHandle = _rtl_open( pszFileName, O_RDONLY);
  if (m_FileHandle>0)
   {
    strncpy(m_sz255FileName, pszFileName, 255);
    // Get the "file time" as a first coarse value when the file has been
    // recorded. Note that "getftime" returns its own time structure
    // which is very different from our internal "time()"-like format.
    // "getftime" returns the date relative to 1980,
    // but SpecLab uses the Unix-style format which is
    //    "Seconds elapsed since 00:00:00 GMT(=UTC), January 1, 1970"
    // how many SECONDS between the UNIX-time(est'd 1970) and 1980 ?
    // T_Float d1,d2; BYTE *pSource;
    // pSource = "1970-01-01 00:00:00";
    // UTL_ParseFormattedDateAndTime( "YYYY-MM-DD hh:mm:ss", &d1, &pSource); // -> 0 (!)
    // pSource = "1980-01-01 00:00:00";
    // UTL_ParseFormattedDateAndTime( "YYYY-MM-DD hh:mm:ss", &d2, &pSource);
    // d2-=d1;
     // (should be something like 365*24*60*60 = 31536000, in fact it's 315532800)

    if( getftime(m_FileHandle, &f_time)==0) // "0" = "no error"
     {
       m_dblStartTime =
         UTL_ConvertYMDhmsToUnix( // parameters for this routine:
         f_time.ft_year+1980,  // int year  (FULL year, like 2002, not crap like "year-1980",
         f_time.ft_month,      // int month (1..12)
         f_time.ft_day,        // int day   (1..31)
         f_time.ft_hour,    // int hour  (0..23) !! not the nerving "AM PM" stuff please... :-)
         f_time.ft_min,     // int minute (0..59)
         2*f_time.ft_tsec); // T_Float dblSecond  (0.000...59.999)
       // By order of the Prophet, "dblStartTime" should really contain the RECORDING start time.
       // The FILE TIME is usually the time of the last write access, which will be
       // the LAST sample. So we must subtract the duration of the recording
       // as soon as we know it [but we don't know it here because the file header
       // has not been analysed yet].
       fSubtractRecLengthFromTime = TRUE;
     }
    else m_dblStartTime=0;


    // analyse the header of the WAV-file (which if POSSIBLY A RIFF-format)
    i = _rtl_read(m_FileHandle, &m_RiffHdr, sizeof(T_WAV_RIFF_HDR) );
    if( i==sizeof(T_WAV_RIFF_HDR) )
     { // reading the RIFF header successful:
      if(strncmp(m_RiffHdr.id_riff, "RIFF", 4) == 0)
        {
          m_iFileFormat = WAVE_FORMAT_RIFF;  // bingo, it's the common windoze - *.WAV-format
        }
      else
        { m_iFileFormat = WAVE_FORMAT_RAW;   // 'raw' audio samples without header..
          if( fExtensionIsWav )
            {                                // .. should NEVER have the extension "wav"
              strcpy(m_cErrorString,"no RIFF id"); // .. so this is an error !
              ok = FALSE;
            }
        } // end else < NO RIFF-header found >
      if( m_iFileFormat == WAVE_FORMAT_RIFF )
       { // if the audio file has a RIFF header, the RIFF-ID should be "WAVE" :
        if(strncmp(m_RiffHdr.wave_id, "WAVE", 4) != 0)
         { strcpy(m_cErrorString,"no WAVE id");
           ok = FALSE;
         }
       }
     }
   } // end if <file opened successfully>
  else
   { strcpy(m_cErrorString,"_open failed");
     ok = FALSE;
   }


  // For RIFF WAVE files (*.wav), analyse all CHUNKS, until "data" chunks has been found
  if( m_iFileFormat == WAVE_FORMAT_RIFF )
   {
     memset(&chunk_hdr, 0, sizeof(T_WAV_CHUNK_HDR) );
     found = FALSE;
     while( (ok) && (! found) )
      {
        // Read the next chunk HEADER from the input file:
        i = _rtl_read(m_FileHandle, &chunk_hdr, sizeof(T_WAV_CHUNK_HDR) );
        if( i!=sizeof(T_WAV_CHUNK_HDR) )
         { strcpy(m_cErrorString,"bad header size");
           ok = FALSE;
         }
        else // reading the chunk header was ok. Analyse it:
         {
           i = 0; // number of bytes read from a "known" chunk.
           if(   (strncmp(chunk_hdr.id, "fmt ", 4) == 0)
              && (chunk_hdr.chunk_size >= sizeof(T_WAV_CHUNK_FORMAT) ) )
            { // whow, it's a standard T_WAV_CHUNK_FORMAT - structure,
              //   read a T_WAV_CHUNK_FORMAT from the input file:
              i = _rtl_read(m_FileHandle, &m_ChunkFormat, sizeof(T_WAV_CHUNK_FORMAT) );
              if (i != sizeof(T_WAV_CHUNK_FORMAT) ) ok = FALSE;
              // Note that the actual chunk size may differ from what we
              // just read, though it's a "fmt "-chunk !!
            }
           else if (strncmp(chunk_hdr.id, "data", 4) == 0)
            { // bingo, it's the one and only DATA-Chunk:
              m_lFileDataSize = chunk_hdr.chunk_size;  // netto size of the samples in BYTES
              found = TRUE;
            }
           else // unknown chunk type, ignore it, it's ok !
            {
              i = 0;  // no "extra" bytes read from unknwon chunk.
            }
           // Skip the chunk, no matter if it was unknown...
           //  (except for the "data" chunk)
           // Reason: also the known chunks may be LONGER
           // in the file than we expected in our structure !
           if( (!found) && ok && ( chunk_hdr.chunk_size > (DWORD)i) )
            {
             if (lseek( m_FileHandle, chunk_hdr.chunk_size-i, SEEK_CUR ) < 0)
              { strcpy(m_cErrorString,"lseek failed");
                ok = FALSE;  // seeking the end of the unknown chunk failed !
              }
            }
         } // end if <reading a chunk header ok>
      } // end while <ok AND "data"-chunk not found>

     if (!found) ok=FALSE;
     /* Arrived here with ok=TRUE, we should have skipped all chunks. */
     /* Test the "fmt "-chunk's contents if the format is supported:  */
     if (ok)
      {
        if(m_ChunkFormat.wFormatTag != 1)
         {
          strcpy(m_cErrorString,"bad FormatTag");
          ok = FALSE;    // Format category, only 1=Microsoft PCM supported
         }
        if( (m_ChunkFormat.wChannels<1) && (m_ChunkFormat.wChannels>2) )
         {
          strcpy(m_cErrorString,"bad NrChannels");
          ok = FALSE;    // Number of channels, 1=mono, 2=stereo
         }
        if(   (m_ChunkFormat.dwSamplesPerSec < 1)
           || (m_ChunkFormat.dwSamplesPerSec > 100000) )
         {
          strcpy(m_cErrorString,"bad SamplesRate");
          ok = FALSE;    // Sampling rate not supported
         }
        else
         { // at least we know the 'nominal' sample rate..
           // Problem: RIFF WAVe header only uses integer value for sampling rate.
           if(m_dbl_SampleRate < 0.1)  // exacte rate still unknown ?
            {
              // Try to be smart and reconstruct a 'precise' sample rate.
              // A guess (true in 90% of all cases):
              // Sampling rate is a standard soundcard sampling rate
              // or some "decimated" value from that.
              // Decimation factors can be any combination of 2^n * 3^m , 0..n..8
              m_dbl_SampleRate = GuessExactFileSampleRate( m_ChunkFormat.dwSamplesPerSec );
            } // end if <exact sample rate unknown>
         }
        if( (m_ChunkFormat.wBlockAlign < 1) || (m_ChunkFormat.wBlockAlign > 4) )
         {
          strcpy(m_cErrorString,"bad BlockAlign");
          ok = FALSE;    // Data block size ?? Bytes per Sample ???
         }
        if( m_ChunkFormat.wBitsPerSample > 16)
         {
          strcpy(m_cErrorString,"bad BitsPerSample");
          ok = FALSE;    // Sample size, usually 8 or 16 bits per sample
         }
        if( (m_ChunkFormat.wBitsPerSample==16) && (m_ChunkFormat.wBlockAlign==1) )
         {  // 16 bits per sample and "block_align=1" is impossible (see formula)
            // but it occurred in a sample from G4JNT. Fix that bug HERE:
            m_ChunkFormat.wBlockAlign = (WORD)
               (m_ChunkFormat.wChannels * (m_ChunkFormat.wBitsPerSample+7) / 8);
         }
      } // end if <ok to check the "fmt "-chunk ?>

     if (ok)
      {
        // save file-internal position of 1st audio sample
        // (just in case someone wants to "Rewind" etc).
        m_lFilePos_Data = tell( m_FileHandle );
        m_lCurrFilePos  = m_lFilePos_Data;
        if (m_lFilePos_Data<=0)
         { strcpy(m_cErrorString,"ftell failed");
           ok = FALSE;
         }
      }

   } // end if( m_iFileFormat == WAVE_FORMAT_RIFF )
  else   // not WAVE_FORMAT_RIFF,  but WAVE_FORMAT_RAW = 'raw' audio samples without header
   { //------------------------------------------------------------------------------------

     m_lFileDataSize = lseek( m_FileHandle, 0, SEEK_END ); // determine the netto size in BYTES

     m_lFilePos_Data = m_lCurrFilePos = 0;  // no header to skip, jump back to the 1st byte
     lseek( m_FileHandle, m_lCurrFilePos, SEEK_SET );

     // Only a few properties should be set for successfully reading a RAW file.
     // The caller should set them before or after opening the file for reading .
     // (preferred: Tell us everything we need to know BEFORE opening the wave-file)
     UpdateRawFileParams();  // -> m_ChunkFormat.wBlockAlign, etc
   }

  // Arrived here, the file-pointer is set to the first audio sample in the file.

  if( fSubtractRecLengthFromTime
     && (m_dbl_SampleRate>0) && (m_ChunkFormat.wBlockAlign>0) )
   { // subtract recording duration from "recording start time"
     // (because up to now, only the DOS FILE TIME is known)
     m_dblStartTime -= m_lFileDataSize/*bytes*/
        / ( m_dbl_SampleRate * m_ChunkFormat.wBlockAlign );
   } // end if( fSubtractRecLengthFromTime )

  if (!ok)
   {
    if(m_cErrorString[0]=='\0')
      strcpy(m_cErrorString,"other 'open' error");
    CloseFile();  // close input file if opened but bad header
    return FALSE;
   }
  else
   {
    m_OpenedForReading = ok;
    return TRUE;
   }
} // C_WaveIO::InOpen(..)

/************************************************************************/
void C_WaveIO::UpdateRawFileParams(void)
{
 if( m_iFileFormat==WAVE_FORMAT_RAW )
  { // If the most important parameters of the RAW file have been specified
    // *after* opening the file, calculate the missing parameters now :
    if( m_wRawFileBitsPerSample<1 )
        m_wRawFileBitsPerSample = 8;
    if( m_wRawFileNumberOfChannels<1 )
        m_wRawFileNumberOfChannels=1;
    m_ChunkFormat.wBitsPerSample = (WORD) m_wRawFileBitsPerSample;
    m_ChunkFormat.wChannels   = (WORD) m_wRawFileNumberOfChannels;
    m_ChunkFormat.wBlockAlign = (WORD)( (m_wRawFileBitsPerSample+7)/8 * m_wRawFileNumberOfChannels );
  }
} // end C_WaveIO::UpdateRawFileParams()

/************************************************************************/
LONG C_WaveIO::GetTotalCountOfSamples(void)
  /* Returns the total count of sampling points in the file .
   */
{
  if( m_ChunkFormat.wBlockAlign>0 )
   { // subtract recording duration from "recording start time"
     // (because up to now, only the DOS FILE TIME is known)
     return m_lFileDataSize/*bytes*/ / m_ChunkFormat.wBlockAlign;
   } // end if( fSubtractRecLengthFromTime )
  else
     return 0;
} // C_WaveIO::GetTotalCountOfSamples()


/***************************************************************************/
LONG C_WaveIO::GetCurrentSampleIndex(void)
  /* Reads the current sample index that will be used on the next
   * READ- or WRITE- access to the audio samples in the opened file.
   * Returns a negative value on any error.
   */
{
 long l=-1;
 if ( (m_FileHandle>=0) && (m_ChunkFormat.wBlockAlign>0) )
  {
   l = tell( m_FileHandle ) - m_lFilePos_Data;
   // may also use m_lCurrFilePos to avoid the "tell()"-routine...
   l = m_lCurrFilePos - m_lFilePos_Data;
   if(l>0)
      l /= m_ChunkFormat.wBlockAlign;
  }
 return l;
} // C_WaveIO::GetCurrentSampleIndex()


/***************************************************************************/
BOOL C_WaveIO::WindToSampleIndex(long new_sample_index)
  /* Sets the current sample index that will be used on the next
   * READ- or WRITE- access to the audio samples an opened file.
   * Returns TRUE if ok ,  otherwise FALSE.
   * As long as you don't use this routine/method,
   * the audio file will be played in a strictly "sequential" way.
   */
{
 long l=-1;
 if (   (m_FileHandle>=0) && (m_ChunkFormat.wBlockAlign>0)
     && (new_sample_index>=0)  )
  {
   l = new_sample_index * m_ChunkFormat.wBlockAlign;
   if ( (l>=0) && (l<m_lFileDataSize) )
    {
      m_lCurrFilePos = m_lFilePos_Data + l;
      return( lseek( m_FileHandle, m_lCurrFilePos , SEEK_SET ) > 0);
    }
  }
 return FALSE;
} // C_WaveIO::GetCurrentSampleIndex()



/***************************************************************************/
LONG C_WaveIO::ReadSamples( BYTE* pData, LONG Length )
  /* Reads some samples from a wave file which has been opened for READING.
   * Returns the NUMBER OF AUDIO SAMPLE POINTS if successful,
   *     (not the number of bytes; one sample point may contain up to 8 bytes)
   *   or a negative value if errors occurred.
   * Notes:
   *  - "Length" is the size of the caller's buffer capacity in BYTES.
   *     The actual number of AUDIO SAMPLES will depend on the sample
   *     format. For 16-bit mono recordings, the number of samples
   *     will only be half the "Length".
   *     Generally, the   number of audio samples read   is:
   *          n_samples = Length / m_ChunkFormat.wBlockAlign .
   *  - since 11/2005, up to FOUR channels with SIXTEEN BITS each is possible
   *          ( m_ChunkFormat.wBlockAlign = 16 ) .
   */
{
 int  i;

 if( (m_ChunkFormat.wBlockAlign==0) && (m_iFileFormat==WAVE_FORMAT_RAW) )
  { // If the most important parameters of the RAW file have been specified
    // *after* opening the file, calculate the missing parameters now :
    UpdateRawFileParams();   // -> m_ChunkFormat.wBlockAlign, etc
  }

 if ( m_OpenedForReading && (m_FileHandle>=0) && (m_ChunkFormat.wBlockAlign>0) )
  {
   i = _rtl_read(m_FileHandle, pData, Length);
   if (i>0)
    {
      m_lCurrFilePos += i;
      return i / m_ChunkFormat.wBlockAlign;
    }
  }
 return -1;
} // C_WaveIO::ReadSamples(..)

/***************************************************************************/
LONG C_WaveIO::ReadSampleBlocks(
     int channel_nr,    // channel_nr for 1st destination block
     int nr_samples,    // nr_samples (or, more precisely, length of destination blocks:  pFltDest1[0..nr_samples-1] )
     T_Float *pFltDest1, // 1st destination block (~"LEFT",  sometimes "I" AND "Q" )
     T_Float *pFltDest2) // 2nd destination block (~"RIGHT")
  /* Reads some samples from a wave file which has been opened for READING
   * and converts them to <T_Float>-numbers (*) in the range of +- 32768.0
   * Input parameters:
   *   channel_nr: 0= begin with the first channel (LEFT if stereo recording)
   *               1= begin with the second channel (RIGHT if stereo, or Q if complex)
   *     The UPPER BYTE of 'channel_nr' can optionally define
   *         HOW MANY CHANNELS SHALL BE PLACED IN ONE DESTINATION BLOCK:
   *    (channel_nr>>8) : 0x00 or 0x01 = only ONE channel per destination block
   *                      0x02         = TWO channels per destination block
   *                                    (often used for complex values aka I/Q)
   *
   *   nr_samples: Max number of FLOATs which can be stored in T_Float dest[0..nr_samples-1]
   *   *pFltDestX = Pointers to destination arrays (one block per channel, sometimes "paired")
   * Return value:
   *      the NUMBER OF FLOATING-POINT VALUES placed in each destination block
   *                 (not necessarily the number of sample points!) ,
   *      or a negative value if an error occurred.
   * Note that returning less samples than you may expect is not an error
   * but indicates reaching the file's end !
   * (*) T_Float may be either SINGLE- or DOUBLE-precision float,
   *     defined in various headers, depending on SWI_FLOAT_PRECISION .
   */
{
  BYTE *temp_array;
  int  i;
  int  asize;
  int  n_samples_read;
  int  iResult;
  BOOL ok;
  BOOL fTwoChannelsPerBlock = FALSE;

   fTwoChannelsPerBlock =  ((channel_nr & WAVE_FILE_TWO_CHANNELS_PER_BLOCK) != 0)
                        && (m_ChunkFormat.wChannels>1);
   channel_nr &= 0x000F;  // remove all 'flags', leave only the start-channel number

   if ( (!m_OpenedForReading) || (m_FileHandle<0)
      || (m_ChunkFormat.wBlockAlign<=0)
      || (m_ChunkFormat.wBitsPerSample <= 1)  )
     return -1;

   if(fTwoChannelsPerBlock)
    { nr_samples /= 2;    // here: TWO destination array indices per sample point
      // so if the caller can accept 4096 floating-point values in his array (pFltDest1),
      // he will only read 2048 sampling points in one call of this subroutine !
    }

   asize = nr_samples * m_ChunkFormat.wBlockAlign;
   if (asize<=0)
     return -1;
   temp_array = new BYTE[asize];  // allocate temporary array
   if(temp_array==NULL)
     return -1;

   n_samples_read = ReadSamples( // Returns the NUMBER OF AUDIO SAMPLE POINTS!
            temp_array, /* temporary BYTE-array for conversion     */
            asize );    /* size in BYTE, not "count of samples" !! */
   if (n_samples_read > nr_samples)
       n_samples_read = nr_samples;  // emergency limit, should never happen
   ok = FALSE;
   if  (m_ChunkFormat.wBitsPerSample <= 8)
    { /* up to 8 bits per sample, we use a BYTE-pointer for addressing. */
     ok = TRUE;
     BYTE *bp = temp_array;
     if( (m_ChunkFormat.wChannels>1)  // Number of channels, 1=mono, 2=stereo
      && (channel_nr < m_ChunkFormat.wChannels) )
      { // channel_nr: 0=left, 1=right, ..
        bp += channel_nr;             // skip 'unwanted' channels
      }
     if( fTwoChannelsPerBlock )
      { // two channels (usually I+Q) per destination block (since 2005-11-01) :
        iResult = 2*n_samples_read;   // TWO destination array indices per sample point
        for(i=0; i<n_samples_read/*!*/; i++ )
         { // up to 8 bits per sample, TWO SEPARATE destination blocks:
           *pFltDest1++ = ( (T_Float)(*bp)   - 127.0 ) * (32767.0 / 127.0);  // 1st channel in file ("I")
           *pFltDest1++ = ( (T_Float)(*bp+1) - 127.0 ) * (32767.0 / 127.0);  // 2nd channel in file ("Q")
           if( (pFltDest2!=NULL) && (m_ChunkFormat.wChannels>2) )
            { *pFltDest2++ = ( (T_Float)(*(bp+2))-127.0 ) * (32767.0 / 127.0); // 3rd channel in file ("I")
              *pFltDest2++ = ( (T_Float)(*(bp+3))-127.0 ) * (32767.0 / 127.0); // 4th channel in file ("Q")
            }
           bp += m_ChunkFormat.wBlockAlign;
         }
      }
     else // ! fTwoChannelsPerBlock ..
      { iResult = n_samples_read;  // ONE destination array index per sample point
        if( (m_ChunkFormat.wChannels<2) || (pFltDest2==NULL) )
         { // only ONE channel to be copied...
           for(i=0; i<n_samples_read; ++i)
            { // up to 8 bits per sample, coding is "UNSIGNED" (crazy, isn't it)
              *pFltDest1++ = ( (T_Float)(*bp) - 127.0 ) * (32767.0 / 127.0);
              bp += m_ChunkFormat.wBlockAlign;
            }
         }
        else
         {  // TWO or more channels must be copied (and "split" into two destination blocks) :
           for(i=0; i<n_samples_read; ++i)
            { // up to 8 bits per sample, TWO SEPARATE destination blocks:
              *pFltDest1++ = ( (T_Float)(*bp)   - 127.0 ) * (32767.0 / 127.0);
              *pFltDest2++ = ( (T_Float)(*(bp+1))-127.0 ) * (32767.0 / 127.0);
              bp += m_ChunkFormat.wBlockAlign;
            }
         } // end if <two separate destination blocks>
      } // end else < not TWO channels per destination block >
   } // end if (8-bit-samples)
  else
  if( (m_ChunkFormat.wBitsPerSample <= 16) && (m_ChunkFormat.wBlockAlign>1) )
   { /* 8..16 bits per sample, we use a 16bit-int-pointer for addressing. */
     ok = TRUE;
     short *sp = (short*)temp_array; // should be even address, so what ??
     if( (m_ChunkFormat.wChannels>1) // Number of channels, 1=mono, 2=stereo
      && (channel_nr < m_ChunkFormat.wChannels) )
      { // channel_nr: 0=left, 1=right, ..
        sp += channel_nr;     // skip 'unwanted' channels
      }
     if( fTwoChannelsPerBlock )
      { // two channels (usually I+Q) per destination block (since 2005-11-01) :
        iResult = 2*n_samples_read;  // TWO destination array indices per sample point
        for(i=0; i<n_samples_read/*!*/; i++ )
         { // up to 8 bits per sample, TWO SEPARATE destination blocks:
             *pFltDest1++ = (T_Float)(*sp);        // 1st channel ("I")
             *pFltDest1++ = (T_Float)(*(sp+1));    // 2nd channel ("Q")
             if( (pFltDest2!=NULL) && (m_ChunkFormat.wChannels>2) )
              { *pFltDest2++ = (T_Float)(*(sp+2)); // 3rd channel ("I")
                *pFltDest2++ = (T_Float)(*(sp+3)); // 4th channel ("Q")
              }
             sp += (m_ChunkFormat.wBlockAlign / 2); // pointer to 16-bit-values!!
         }
      }
     else // ! fTwoChannelsPerBlock ..
      {
        iResult = n_samples_read;  // ONE destination array index per sample point
        if( (m_ChunkFormat.wChannels<2) || (pFltDest2==NULL) )
         { // only ONE channel to be copied...
           for(i=0; i<n_samples_read; ++i)
            { // up to 8 bits per sample, coding is "UNSIGNED" (crazy, isn't it)
              *pFltDest1++ = (T_Float)(*sp);
              sp += (m_ChunkFormat.wBlockAlign / 2); // pointer to 16-bit-values!!
            }
         }
        else
         {  // TWO channels must be copied (and "split" into two destination blocks) :
           for(i=0; i<n_samples_read; ++i)
            { // up to 8 bits per sample, coding is "UNSIGNED" (crazy, isn't it)
              *pFltDest1++ = (T_Float)(*sp);
              *pFltDest2++ = (T_Float)(*(sp+1));
              sp += (m_ChunkFormat.wBlockAlign / 2); // pointer to 16-bit-values!!
            }
         } // end if <two separate destination blocks>
      } // end else < not TWO channels per destination block >
    } // end if (16-bit-samples)
   else
    { // cannot handle this sample format !!
     ok = FALSE;
     iResult = -1;  // error
    }

   delete[] temp_array;
   return iResult;

} // C_WaveIO::ReadSampleBlocks(...)



/***************************************************************************/
BOOL C_WaveIO::OutOpen( char *file_name,
                        char *additional_info,
                         int file_format,
                         int bits_per_sample,
                         int channels,
                         T_Float dbl_sample_rate,
                         double dblStartTime )
  /* Creates and Opens an audio file for WRITING audio samples.
   *         Returns TRUE on success and FALSE on any error.
   * Input parameters..
   *   file_format:      WAVE_FORMAT_RAW  or  WAVE_FORMAT_RIFF
   *   additional_info:  a null-terminated text string with additional info
   *                     which will be included in the exported file.
   *                     NULL if this feature is not used.
   *                     SpecLab uses this to save date, time, precise
   *                     sample rate, and some other receiver settings
   *                     when the recording started.
   *   bits_per_sample:  must be either 8 or 16 (bits for a single channel)
   *   channels:         1=mono, 2=stereo recording etc
   *   sample_rate:      number of samples per second.
   *                     If the output format is WAVE_FORMAT_RIFF,
   *                     the sample rate should be above 8000 samples/second.
   */
{
 int i;
 T_WAV_CHUNK_HDR  chunk_hdr;
 long  i_sample_rate;

 BOOL ok = TRUE;

  CloseFile();   // Close "old" file if it was open

  // round the sample rate to a 32-bit INTEGER value (for the RIFF header)
  m_dbl_SampleRate = dbl_sample_rate;
  i_sample_rate = long(dbl_sample_rate+0.5);

  // coarse check of the arguments..
  if( (file_format!=WAVE_FORMAT_RAW) && (file_format!=WAVE_FORMAT_RIFF) )
      return FALSE;
  if( (bits_per_sample != 8) && (bits_per_sample != 16) )
      return FALSE;
  if( (channels<1) || (channels>2) )
      return FALSE;    // only mono or stereo recording supported
                       // ..sorry for those 'quattrophiles' ;-)
  if( (i_sample_rate < 1) || (i_sample_rate > 100000) )
      return FALSE;

  m_lCurrFilePos = m_lFilePos_Data = 0;  // default file position if no header
  m_lFileDataSize= 0;  // "netto" size of audio samples still zero
  m_iFileFormat = file_format;  // remember the file format when closing!

  // Try to create new file or truncate existing file to length "zero":
  m_FileHandle   = _rtl_creat( file_name, 0);
  if (m_FileHandle>0)
   {
    // Get the "start time" of this recording
    m_dblStartTime = dblStartTime; // ex: TIM_GetCurrentTime();

    if(m_iFileFormat==WAVE_FORMAT_RIFF)
     {
      // Fill and write the header of the WAV-file:
      memset( &m_RiffHdr,     0 , sizeof(T_WAV_RIFF_HDR)     );
      strncpy(m_RiffHdr.id_riff, "RIFF", 4);
      strncpy(m_RiffHdr.wave_id, "WAVE", 4);
      m_RiffHdr.len =  sizeof(T_WAV_RIFF_HDR)
                 + 2*sizeof(T_WAV_CHUNK_HDR)
                 +   sizeof(T_WAV_CHUNK_FORMAT);
       // (the length of the audio samples ("data") is still unknown,
       //  will be ADDED later before closing)

      i = _rtl_write(m_FileHandle, &m_RiffHdr, sizeof(T_WAV_RIFF_HDR) );
      if( i!=sizeof(T_WAV_RIFF_HDR) )
       { // writing the RIFF header failed:
        ok = FALSE;
       }
     } // end if <RIFF file format>
   } // end if <file opened successfully>
  else     ok = FALSE;

  // prepare a 'chunk header' in memory even if not writing to RIFF wave:
  memset(&chunk_hdr, 0, sizeof(T_WAV_CHUNK_HDR) );
  strncpy(chunk_hdr.id, "fmt ", 4);
  chunk_hdr.chunk_size = sizeof(T_WAV_CHUNK_FORMAT);
  memset( &m_ChunkFormat, 0 , sizeof(T_WAV_CHUNK_FORMAT) );
  m_ChunkFormat.wFormatTag = 1;     // Format category,    1=Microsoft PCM
  m_ChunkFormat.wChannels  = (WORD)channels; //  1=mono, 2=stereo
  m_ChunkFormat.dwSamplesPerSec     // Sampling rate     , samples per second
                 = i_sample_rate;   // ROUNDED TO 32bit-INTEGER VALUE!
  m_ChunkFormat.wBitsPerSample    // Sample size, usually 8 or 16 bits per sample
                 = (WORD)bits_per_sample;
  m_ChunkFormat.wBlockAlign       // Data block size , Bytes per Sample :
                 = (WORD)( (long)m_ChunkFormat.wChannels
                 *  ( ( m_ChunkFormat.wBitsPerSample + 7 ) / 8 ) );
  m_ChunkFormat.dwAvgBytesPerSec  // For buffer estimation
                 = m_ChunkFormat.wBlockAlign
                 * m_ChunkFormat.dwSamplesPerSec;


  if( (m_iFileFormat==WAVE_FORMAT_RIFF) && (ok) )
   {
    // Write a T_WAV_CHUNK_FORMAT to the output RIFF wave file:
    i = _rtl_write(m_FileHandle, &chunk_hdr, sizeof(T_WAV_CHUNK_HDR) );
    if (i != sizeof(T_WAV_CHUNK_HDR) ) ok = FALSE;
    i = _rtl_write(m_FileHandle, &m_ChunkFormat, sizeof(T_WAV_CHUNK_FORMAT) );
    if (i != sizeof(T_WAV_CHUNK_FORMAT) ) ok = FALSE;
   }

  if( (m_iFileFormat==WAVE_FORMAT_RIFF) && (ok) )
   {
    // Fill and write a DATA-chunk to the output file:
    memset(&chunk_hdr, 0, sizeof(T_WAV_CHUNK_HDR) );
    strncpy(chunk_hdr.id, "data", 4);
    chunk_hdr.chunk_size = 0; // still unknown, will be updated before closing.
    i = _rtl_write(m_FileHandle, &chunk_hdr, sizeof(T_WAV_CHUNK_HDR) );
    if (i != sizeof(T_WAV_CHUNK_HDR) ) ok = FALSE;
    // save file-internal position of 1st audio sample, which will also be
    // used to write the correct "data"-chunk size into the header later
    // when closing the file.
    m_lFilePos_Data = tell( m_FileHandle );
    if (m_lFilePos_Data<=0) ok = FALSE;
    m_lCurrFilePos  = m_lFilePos_Data;
   }

  if (!ok)
   {
    CloseFile();  // close output file, could not generate a header.
    return FALSE;
   }
  else
   {
    m_OpenedForWriting = ok;
    return TRUE;
   }
} // C_WaveIO::OutOpen(..)


/***************************************************************************/
LONG C_WaveIO::WriteSamples( BYTE* pData, LONG Length )
  /* Writes some samples to an audio file which has been opened for WRITING.
   * Returns the number of SAMPLES written,
   *  or a NEGATIVE value on any error.
   * Note: "Length" is the size of the caller's buffer capacity in BYTES.
   *       The actual number of AUDIO SAMPLES will depend on the sample
   *       format.
   *       For 16-bit mono recordings (which is the only supported format
   *       up 'til now), the number of samples will only be half the "Length"
   *       etc.
   */
{
  int n_written;

  /* Ensure that it's really possible to WRITE audio samples to a file now.
   */
   if ( (!m_OpenedForWriting) || (m_FileHandle<0)
      || (m_ChunkFormat.wBlockAlign<=0)
      || (m_ChunkFormat.wBitsPerSample <= 1)
      || (Length <= 0)  )
     return -1;

  /* Try to append the new samples to the wave file.
   * Note that "WindToSampleIndex()" does not work on OUTPUT files yet !!!
   */
   n_written = _rtl_write(m_FileHandle, pData, Length );
   if (n_written != Length)
    {
     /* Writing to the output file failed  !!!
      * Seems we're in trouble now, because the reason may be a full
      * harddisk and Windooze is going to crash in a few moments :-(
      */
     CloseFile();
     return -1;  // also set a breakpoint here !
    }

  /* Keep the "current file position" updated so we don't have to call
   *    the "tell()"-function too often.
   * This parameter will also be READ from the spectrum analyser main window
   * to display the actual file size.
   */
  m_lCurrFilePos += n_written;
  if(m_lCurrFilePos-m_lFilePos_Data >= m_lFileDataSize)
     m_lFileDataSize = m_lCurrFilePos-m_lFilePos_Data;
  return n_written / m_ChunkFormat.wBlockAlign;
} // end C_WaveIO::WriteSamples(..)


/************************************************************************/
LONG C_WaveIO::GetCurrentFileSize(void)
  /* Returns the current file size (if there is an opened WAVE file).
   * Useful especially when logging audio samples  to keep track
   * of the used disk size (stop before Windows is getting into trouble).
   */
{
 if (m_OpenedForWriting || m_OpenedForReading)
       return m_lCurrFilePos;
  else return 0L;
} // end C_WaveIO::GetCurrentFileSize()

/************************************************************************/
  // Some 'Get' - and 'Set' - routines for the AUDIO FILE class ..
int    C_WaveIO::GetFileFormat(void)
{ return m_iFileFormat;
}
T_Float C_WaveIO::GetSampleRate(void)  // this is the FILE's sampling rate !
{ return m_dbl_SampleRate;
}
void   C_WaveIO::SetSampleRate(T_Float dblSampleRate)
{ m_dbl_SampleRate = dblSampleRate;
}

int    C_WaveIO::GetNrChannels(void)
{ return m_ChunkFormat.wChannels;
}

void   C_WaveIO::SetNrChannels(int iNrChannels)
{ // Only applies to RAW FILES !  For RIFF WAVE files,
  //  the number of channels will be read from the file header .
  m_wRawFileNumberOfChannels = (WORD)iNrChannels;
  if(m_iFileFormat==WAVE_FORMAT_RAW)
   { UpdateRawFileParams();
   }
}

int    C_WaveIO::GetBitsPerSample(void)
{ return m_ChunkFormat.wBitsPerSample;
}

void   C_WaveIO::SetBitsPerSample(int iBitsPerSample)
{ // Only applies to RAW FILES !  For RIFF WAVE files,
  //  the number of bits per (single) sample will be read from the file header .
  m_wRawFileBitsPerSample = (WORD)iBitsPerSample;
  if(m_iFileFormat==WAVE_FORMAT_RAW)
   { UpdateRawFileParams();
   }
}

T_Float C_WaveIO::GetFreqOffset(void)
{ return 0.0;
}

T_Float C_WaveIO::GetNcoFrequency(void)
{ // only important for decimated files with I/Q-samples and complex mixing
  return 0.0;
} // end C_WaveIO::GetNcoFrequency()

BOOL   C_WaveIO::SetNcoFrequency(T_Float dblNcoFrequency)
{ // only important for decimated files with I/Q-samples and complex mixing
  dblNcoFrequency = dblNcoFrequency;
  return FALSE;  // not supported
} // end C_WaveIO::SetNcoFrequency()


double C_WaveIO::GetCurrentRecordingTime(void)
{ double d;   // no "T_Float" here, we need 64 bit floats to code the time !
  d = (long)GetCurrentSampleIndex();
  if(m_dbl_SampleRate>0)
     d /= m_dbl_SampleRate;
  return d;    // **NOT** "+ Start Time" ! ! !
}
double C_WaveIO::GetRecordingStartTime(void)
{ return m_dblStartTime;
}
void   C_WaveIO::SetRecordingStartTime(double dblStartTime)
{ m_dblStartTime = dblStartTime;
}


void C_WaveIO::GetFileName(char *pszDest, int iMaxLen)
{ strncpy( pszDest, m_sz255FileName, iMaxLen );
}


bool   C_WaveIO::AllParamsIncluded(void)
{ return FALSE;
}



/***************************************************************************/
BOOL C_WaveIO::CloseFile( void )
  /* Closes the WAV-file (if opened for READING exor WRITING.
   * Returns TRUE on success and FALSE on any error.
   */
{
 BOOL ok=TRUE;
 int  n_written;
 T_WAV_CHUNK_HDR  chunk_hdr;

  if (m_OpenedForWriting && (m_FileHandle>=0) )
   {
    /* If the file has been opened for WRITING, we may have to correct
     * some header entries (depending on the file type & size, because we did
     *  not know how long the file was going to get when writing the header).
     */
    if(  (m_iFileFormat==WAVE_FORMAT_RIFF)
      && (m_lFilePos_Data > (long)sizeof(T_WAV_CHUNK_HDR) ) )
     { /* It's a RIFF WAVE file, and the position
        * of the "data" chunk structure is known:
        * wind the file position back to 'data chunk header'...
        */
      if( lseek( m_FileHandle,
                 m_lFilePos_Data-sizeof(T_WAV_CHUNK_HDR), SEEK_SET ) > 0)
       {
        strncpy(chunk_hdr.id, "data", 4);
        // The size of the "data" chunk is known now, write it to the file:
        chunk_hdr.chunk_size = m_lFileDataSize;
        n_written = _rtl_write(m_FileHandle, &chunk_hdr, sizeof(T_WAV_CHUNK_HDR));
        if (n_written != sizeof(T_WAV_CHUNK_HDR) ) ok = FALSE;
       } /* end if <seeking the "data"-chunk length entry ok> */
      if (ok)
       { /* If all accesses up to now were ok, also update the "total" file
          * size in the RIFF header.
          */
         m_RiffHdr.len = m_lFilePos_Data + m_lFileDataSize;
         // Rewind to the beginning of the file
         //     (where the RIFF header should be :)
         if( lseek( m_FileHandle, 0L, SEEK_SET ) == 0)
          {
           n_written = _rtl_write(m_FileHandle, &m_RiffHdr, sizeof(T_WAV_RIFF_HDR) );
           if( n_written != sizeof(T_WAV_RIFF_HDR) )
            { // writing the size-corrected RIFF header failed:
              ok = FALSE;
            }
          }
       } /* end if <ok to correct the file size in the RIFF header> */
     } /* end if  <RIFF WAVE  and  m_lFilePos_Data > sizeof(..)     */

    /* Now that also the file header has been updated, close the file.
     */
    _rtl_close(m_FileHandle);
    m_FileHandle = -1;  // must forget this handle now !!
    m_OpenedForWriting = FALSE;
    return TRUE;  // Closing successful
   } // end of (..OpenedForWriting)


  ok = FALSE;
  /* Now close the file handle, no matter if it was for READING or WRITING.
   */
  if (m_FileHandle >= 0)
   {
     _rtl_close(m_FileHandle);
     m_FileHandle = -1;  // must forget this handle now !
     ok = TRUE;
   }

  m_OpenedForReading = FALSE;
  m_OpenedForWriting = FALSE;

  return ok;
} // C_WaveIO::CloseFile(..)



/* EOF <WaveIO.cpp> */

