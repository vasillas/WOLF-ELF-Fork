/***************************************************************************/
/*  WaveIO.h :                                                             */
/*   Unit to import and export *.wav - files.                              */
/*   - by DL4YHF August '2000                                              */
/*   - based on the description of the WAVE-file format                    */
/*     found at http://www.wotsit.org  (excellent site, all file formats!  */
/*-------------------------------------------------------------------------*/

#ifndef _WAVE_IO_H_
#define _WAVE_IO_H_


/*----------- Constant Definitions ----------------------------------------*/

#define WAVE_FORMAT_UNKNOWN 0  // for opening a file with auto-detection
#define WAVE_FORMAT_RAW     1  // 'raw' audio samples without header (no *.WAV - file !)
#define WAVE_FORMAT_RIFF    2  // the common windoze - *.WAV-format


// Optional flags for the parameter 'channel_nr' in ReadSampleBlocks() :
#define WAVE_FILE_TWO_CHANNELS_PER_BLOCK 0x0200  // read TWO channels per destination block



/*----------- Data Types --------------------------------------------------*/


#ifndef  SWI_FLOAT_PRECISION   // should be defined under Project..Options..Conditionals
 #define SWI_FLOAT_PRECISION 1 /* 1=single precision, 2=double precision */
#endif

#ifndef T_Float /* T_Float defined in SoundTab.h, SoundMaths.h, Sound.h, WaveIO.h, ... */
#if SWI_FLOAT_PRECISION==1
 #define T_Float float
 // 2003-07-07: "float" instead of "double" saves CPU power on stoneage notebooks
#else
 #define T_Float double
#endif
#endif // ndef T_Float

// A RIFF file has an 8-byte RIFF header, identifying the file,
// and giving the residual length after the header (i.e. file_length - 8):
typedef struct
{
  char  id_riff[4];   // identifier string = "RIFF"
  DWORD len;          // remaining length after this header
  // The riff_hdr is immediately followed by a 4-byte data type identifier.
  // For .WAV files this is "WAVE" as follows:
  char wave_id[4];    // WAVE file identifier = "WAVE"
} T_WAV_RIFF_HDR;


// RIFF Chunks
//   The entire remainder of the RIFF file is "chunks".
//   Each chunk has an 8-byte chunk header identifying the type of chunk,
//   and giving the length in bytes of the data following the chunk header,
//   as follows:

typedef struct {      // CHUNK 8-byte header
  char  id[4];        // identifier, e.g. "fmt " or "data"
  DWORD chunk_size;   // remaining chunk/junk:) length after header
                      // data bytes follow chunk header
} T_WAV_CHUNK_HDR;


// The WAVE form is defined as follows. Programs must expect(and ignore)
// any unknown chunks encountered, as with all RIFF forms.
// However, <fmt-ck> must always occur before <wave-data>, and both of these
// chunks are mandatory in a WAVE file.
//
// WAVE-form> ->
//	RIFF( 'WAVE'
//	<fmt-ck>     		// Format
//	[<fact-ck>]  		// Fact chunk
//	[<cue-ck>]  		// Cue points
//	[<playlist-ck>] 		// Playlist
//	[<assoc-data-list>] 	// Associated data list
//	<wave-data>   ) 		// Wave data
//

// WAVE Format Chunk
//  The WAVE format chunk <fmt-ck> specifies the format of the <wave-data>.
//  Since I only work with PCM format, this structure is as follows
//   (combination of <commond-fields> and <format-specific-fields>):
typedef struct {
  WORD  wFormatTag;         // Format category,    1=Microsoft PCM
  WORD  wChannels;          // Number of channels, 1=mono, 2=stereo
  DWORD dwSamplesPerSec;    // Sampling rate     , samples per second
  DWORD dwAvgBytesPerSec;   // For buffer estimation
  WORD  wBlockAlign;        // Data block size ?? Bytes per Sample ????
  WORD  wBitsPerSample;     // Sample size, usually 8 or 16 bits per sample
} T_WAV_CHUNK_FORMAT;

// The wBitsPerSample field specifies the number of bits of data used
// to represent each sample of each channel. If there are multiple channels,
// the sample size is the same for each channel.
// For PCM data, the wAvgBytesPerSec field of the `fmt' chunk should be equal
//  to the following formula rounded up to the next whole number:
//
//	                             wBitsPerSample
//	wChannels x wBitsPerSecond x --------------
//	                                   8
//
// The wBlockAlign field should be equal to the following formula,
// rounded to the next whole number:
//
//	            wBitsPerSample
//	wChannels x --------------
//	                  8
//
// Data Packing for PCM WAVE Files
//  In a single-channel WAVE file, samples are stored consecutively.
//  For stereo WAVE files, channel 0 represents the left channel,
//  and channel 1 represents the right channel. The speaker position
//  mapping for more than two channels is currently undefined.
//  In multiple-channel WAVE files, samples are interleaved.
//  The following diagrams show the data packing for a 8-bit mono
//  and stereo WAVE files:
// Data Packing for 8-Bit Mono PCM:
//
//		Sample 1	Sample 2	Sample 3	Sample 4
//		---------	---------	---------	---------
//		Channel 0	Channel 0	Channel 0	Channel 0
//
// Data Packing for 8-Bit Stereo PCM:
//
//                   Sample 1                 Sample 2
//		---------------------   ---------------------
//              Channel 0   Channel 1   Channel 0   Channel 0
//               (left)      (right)     (left)      (right)
//
// The following diagrams show the data packing for 16-bit mono
// and stereo WAVE files:
//
// Data Packing for 16-Bit Mono PCM:
//
//			Sample 1			Sample 2
//		----------------------	----------------------
//		Channel 0	Channel 0	Channel 0	Channel 0
//		low-order	high-order	low-order	high-order
//		byte 		byte		byte		byte
//
// Data Packing for 16-Bit Stereo PCM:
//
//                                 Sample 1
//               ---------------------------------------------
//               Channel 0   Channel 0    Channel 1  Channel 1
//               (left)      (left)      (right)     (right)
//               low-order   high-order  low-order   high-order
//               byte        byte        byte        byte
//
//
//
// Data Chunk
//  The Data chunk contains the actual sample frames (ie, all channels
//  of waveform data).

typedef struct {
    char   id[4];   // "data" (4 bytes)
    DWORD  chunkSize;
 // BYTE   waveformData[chunkSize-8];
} T_WAV_DATA_CHUNK;

// The ID is always data. chunkSize is the number of bytes in the chunk,
// not counting the 8 bytes used by ID and Size fields nor any possible
// pad byte needed to make the chunk an even size (ie, chunkSize is the
// number of remaining bytes in the chunk after the chunkSize field,
// not counting any trailing pad byte).
// The waveformData array contains the actual waveform data.
// The data is arranged into what are called sample frames.
// You can determine how many bytes of actual waveform data there is
// from the Data chunk's chunkSize field. The number of sample frames
// in waveformData is determined by dividing this chunkSize by the Format
// chunk's wBlockAlign.
// The Data Chunk is required. One, and only one, Data Chunk may appear
// in a WAVE.



/*----------- Variables with "C"-style single-source principle ------------*/

#undef EXTERN
#ifdef _I_AM_WAVE_IO_
 #define EXTERN
#else
 #define EXTERN extern
#endif


/*----------- Definition of the C_WaveIO  class ---------------------------*/
class C_WaveIO
{
public:
   C_WaveIO();
   virtual ~C_WaveIO();

   // Public member functions for object's users.
   /************************************************************************/
   BOOL InOpen( char *pszFileName, T_Float dblReadingSampleRate );
     /* Opens a wave file for READING it's samples.
      * Returns TRUE on success and FALSE on any error.
      */

   /************************************************************************/
   LONG GetTotalCountOfSamples(void);
     /* Returns the total count of sampling points in the file .
      */


   /************************************************************************/
   LONG GetCurrentSampleIndex(void);
     /* Reads the current sample index that will be used on the next
      * READ- or WRITE- access to the audio samples in the opened file.
      * Returns a negative value on any error.
      */


   /************************************************************************/
   BOOL WindToSampleIndex(long new_sample_index);
     /* Sets the current sample index that will be used on the next
      * READ- or WRITE- access to the audio samples an opened file.
      * Returns TRUE if ok ,  otherwise FALSE.
      * As long as you don't use this routine/method,
      * the audio file will be played in a strictly "sequential" way.
      */


   /************************************************************************/
   LONG ReadSamples( BYTE* pData, LONG Length );
     /* Reads some samples from a wave file which has been opened for READING.
      * "Length" is the size of the caller's buffer capacity in BYTES.
      * Return value: number of SAMPLE POINTS (intervals), not BYTES !!
      * More info: see WaveIO.cpp !
      */


   /************************************************************************/
   LONG ReadSampleBlocks(
         int channel_nr,    // channel_nr for 1st destination block
         int nr_samples,    // nr_samples (per destination block)
         T_Float *pFltDest1, // 1st destination block (~"LEFT")
         T_Float *pFltDest2); // 2nd destination block (~"RIGHT"), may be NULL
     /* Reads some samples from a wave file which has been opened for READING
      * and converts them to double (floats) in the range of +- 32768.0 .
      * More info: see WaveIO.cpp !
      */



   /************************************************************************/
   BOOL OutOpen( char *file_name,
                         char *additional_info,
                         int file_format,
                         int bits_per_sample,
                         int channels,
                         T_Float dbl_sample_rate,
                         double dblStartTime );
     /* Creates and Opens an audio file for WRITING audio samples.
      *         Returns TRUE on success and FALSE on any error.
      *         Detailed description of arguments in WaveIO.cpp only.
      */


   /************************************************************************/
   LONG WriteSamples( BYTE* pData, LONG Length );
     /* Writes some samples to a wave file which has been opened for WRITING.
      * Returns TRUE on success and FALSE on any error.
      * Note: "Length" is the size of the caller's buffer capacity in BYTES.
      *       The actual number of AUDIO SAMPLES will depend on the sample
      *       format. For 16-bit mono recordings, the number of samples
      *       will only be half the "Length" etc.
      */


   /************************************************************************/
   LONG GetCurrentFileSize(void);
     /* Returns the current file size (if there is an opened WAVE file).
      * Useful especially when logging audio samples  to keep track
      * of the used disk size (stop before Windows is getting into trouble).
      */

   /************************************************************************/
   // Some more 'Get' - and 'Set' - routines for the AUDIO FILE class ..
   int    GetFileFormat(void);
   void   GetFileName(char *pszDest, int iMaxLen);

   T_Float GetSampleRate(void);
   void   SetSampleRate(T_Float dblSampleRate);

   int    GetNrChannels(void);
   void   SetNrChannels(int iNrChannels);       // important to read RAW files

   int    GetBitsPerSample(void);  // per "single" sample, regardless of number of channels
   void   SetBitsPerSample(int iBitsPerSample); // important to read RAW files

   T_Float GetFreqOffset(void);

   T_Float GetNcoFrequency(void); // only important for decimated files with I/Q-samples and complex mixing
   BOOL   SetNcoFrequency(T_Float dblNcoFrequency);

   double GetCurrentRecordingTime(void); // .. depending on current sample index
   double GetRecordingStartTime(void);   // not "T_Float" here !
   void   SetRecordingStartTime(double dblStartTime);

   bool   AllParamsIncluded(void);

   /************************************************************************/
   BOOL CloseFile( void );
     /* Closes the WAV-file (if opened for READING exor WRITING.
      * Returns TRUE on success and FALSE on any error.
      */


   // Public properties for simplicity .. we know what we're doing, right ?
   T_WAV_RIFF_HDR     m_RiffHdr;
   T_WAV_CHUNK_HDR    m_ChunkHdr;
   T_WAV_CHUNK_FORMAT m_ChunkFormat;

   double m_dblStartTime;  // seconds elapsed since 00:00:00 GMT, January 1, 1970.
   int    m_iFileFormat;
   char   m_cErrorString[81];
   BOOL   m_OpenedForReading;
   BOOL   m_OpenedForWriting;
   T_Float m_dbl_SampleRate;  // will be private sooner or later..

   // Some more properties for reading RAW FILES . Should be set before opening
   // a RAW file (extension *.raw or *.dat)
   int    m_wRawFileBitsPerSample;
   int    m_wRawFileNumberOfChannels;


private:
   INT   m_FileHandle;
   LONG  m_lFilePos_Data;   // file-internal position of 1st audio sample
   LONG  m_lCurrFilePos;    // current file access position
   LONG  m_lFileDataSize;   // netto size of the samples in BYTES. Sometimes read from "data" chunk
   char  m_sz255FileName[256];

   void UpdateRawFileParams(void);


}; /* end class C_WaveIO */


#endif // _WAVE_IO_H_

/* EOF <WaveIO.h> */