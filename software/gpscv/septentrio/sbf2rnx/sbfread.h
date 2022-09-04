/*
 * sbfread.h: Declarations of SBF (Septentrio Binary
 * Format) reading and parsing functions.
 *
 * Septentrio grants permission to use, copy, modify, and/or distribute
 * this software for any purpose with or without fee.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND SEPTENTRIO DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * SEPTENTRIO BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef SBFREAD_H
#define SBFREAD_H 1

#include <stdio.h>

#include <stdint.h>

#include <sys/types.h>

#include "measepoch.h"
#include "sbfdef.h"
#include "sbfsigtypes.h"
#include "sbfsvid.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 64 bit versions for fseek and ftell are different for the different compilers */
/* STD C version */
#if (defined(__rtems__))
#define ssnfseek fseek
#define ssnftell ftell
/* C99-ready compilers (including gcc, excluding g++) */
#elif (defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)) || defined (__MINGW64__)
#define ssnfseek fseeko
#define ssnftell ftello
#define ssnOff_t off_t
/* Microsoft compiler MSVC  */
#elif defined (_MSC_VER)
#define ssnfseek _fseeki64
#define ssnftell _ftelli64
#define ssnOff_t __int64
/* g++ compilers */
#elif (defined(_POSIX_C_SOURCE) && (_POSIX_C_SOURCE >= 200112L))
#define ssnfseek fseeko
#define ssnftell ftello
#define ssnOff_t off_t
/* fall back to 32 bit version */
#else
#define ssnfseek fseek
#define ssnftell ftell
#define ssnOff_t off_t
#endif

#define BLOCKNUMBER_ALL      (0xffff)

#define START_POS_FIELD   0x03LU
#define START_POS_CURRENT 0x00LU
#define START_POS_SET     0x01LU

#define END_POS_FIELD         0x0cLU
#define END_POS_AFTER_BLOCK   0x00LU
#define END_POS_DONT_CHANGE   0x04LU

#ifndef M_PI
#define M_PI        3.14159265358979323846  /* pi */
#endif

#define c84   299792458.             /* WGS-84 value for lightspeed */

#define E5FREQ         1191.795e6
#define E5aFREQ        1176.45e6
#define E5bFREQ        1207.14e6
#define E6FREQ         1278.75e6
#define L1FREQ         1575.42e6
#define L2FREQ         1227.60e6
#define L5FREQ         1176.45e6
#define E2FREQ         1561.098e6
#define L1GLOFREQ      1602.00e6
#define L2GLOFREQ      1246.00e6
#define L3GLOFREQ      1202.025e6
#define B3FREQ         1268.52e6
#define S1FREQ         2492.028e6

#define E5WAVELENGTH   (c84/E5FREQ)
#define E5aWAVELENGTH  (c84/E5aFREQ)
#define E5bWAVELENGTH  (c84/E5bFREQ)
#define E6WAVELENGTH   (c84/E6FREQ)
#define L1WAVELENGTH   (c84/L1FREQ)
#define L2WAVELENGTH   (c84/L2FREQ)
#define L5WAVELENGTH   (c84/L5FREQ)
#define E2WAVELENGTH   (c84/E2FREQ)
#define L3WAVELENGTH   (c84/L3GLOFREQ)
#define B3WAVELENGTH   (c84/B3FREQ)
#define S1WAVELENGTH   (c84/S1FREQ)

#define FIRSTEPOCHms_DONTCARE  (-1LL)
#define LASTEPOCHms_DONTCARE   (1LL<<62)
#define INTERVALms_DONTCARE    (1)


/* structure to keep the data from the last reference epoch when
   decoding Meas3 blocks. */
typedef struct
{
    uint8_t           SigIdx[MEAS3_SYS_MAX][MEAS3_SAT_MAX][MEAS3_SIG_MAX];
    MeasSet_t         MeasSet[MEAS3_SYS_MAX][MEAS3_SAT_MAX][MEAS3_SIG_MAX];
    uint32_t          SlaveSigMask[MEAS3_SYS_MAX][MEAS3_SAT_MAX];
    int16_t           PRRate_64mm_s[MEAS3_SYS_MAX][MEAS3_SAT_MAX];
    uint32_t          TOW_ms;

    uint8_t           M3SatDataCopy[MEAS3_SYS_MAX][32];
} sbfread_Meas3_RefEpoch_t;


typedef struct
{
    uint64_t  type;           /* decryption type */
    uint8_t   key[32];        /* decryption key */
    bool      has_key;        /* true if decryption key has been set */
    uint64_t  nonce;          /* decryption nonce */
    bool      has_nonce;      /* true if decryption nonce has been set */
    char      rxserialnr[20]; /* receiver serial number */
    bool      has_rxserialnr; /* true if serial number has been set */
    bool      receiver_key_file_read_attempted; /* true if receiver key file has been scanned (or an attempt to do it in case the file is not there) */
    bool      session_key_file_read_attempted;  /* true if session key file has been scanned (or an attempt to do it in case the file is not there) */
} SBFDecrypt_t;


#define MEASCOLLECT_SEEN_MEAS3RANGES         (1<<0)
#define MEASCOLLECT_SEEN_MEAS3DOPPLER        (1<<1)
#define MEASCOLLECT_SEEN_MEAS3CN0HIRES       (1<<2)
#define MEASCOLLECT_SEEN_MEAS3PP             (1<<3)
#define MEASCOLLECT_SEEN_MEAS3MP             (1<<4)
#define MEASCOLLECT_SEEN_MEASEPOCH           (1<<5)
#define MEASCOLLECT_SEEN_MEASEXTRA           (1<<6)
#define MEASCOLLECT_SEEN_MEASFULLRANGE       (1<<7)
#define MEASCOLLECT_NR_OF_MEASBLOCKS         8

typedef struct
{
    FILE*               F;       /* handle to the file */
    SBFDecrypt_t        decrypt; /* SBF decryption context */

    /* the following fields are used to collect and decode the measurement
       blocks */
    sbfread_Meas3_RefEpoch_t RefEpoch[NR_OF_ANTENNAS];
    Meas3Ranges_1_t     Meas3Ranges[NR_OF_ANTENNAS];
    Meas3Doppler_1_t    Meas3Doppler[NR_OF_ANTENNAS];
    Meas3CN0HiRes_1_t   Meas3CN0HiRes[NR_OF_ANTENNAS];
    Meas3PP_1_t         Meas3PP[NR_OF_ANTENNAS];
    Meas3MP_1_t         Meas3MP[NR_OF_ANTENNAS];
    MeasEpoch_2_t       MeasEpoch;
    MeasExtra_1_t       MeasExtra;
    MeasFullRange_1_t   MeasFullRange;
    uint32_t            MeasCollect_CurrentTOW;
    uint32_t            MeasCollect_BlocksSeenAtLastEpoch;
    uint32_t            MeasCollect_BlocksSeenAtThisEpoch;
    uint32_t            TOWAtLastMeasEpoch;
    bool                MeasCollect_PredictEndOfEpoch;
} SBFData_t;

void AlignSubBlockSize(void*  SBFBlock,
                       unsigned int    N,
                       size_t SourceSBSize,
                       size_t TargetSBSize);

int32_t CheckBlock(SBFData_t* SBFData, uint8_t* Buffer);

int32_t GetNextBlock(SBFData_t* SBFData,
                     void*      SBFBlock,
                     uint16_t   BlockNumber1, uint16_t BlockNumber2,
                     uint32_t   FilePos);

int32_t GetNextBlockWithEscape(SBFData_t* SBFData,
                               void*      SBFBlock,
                               uint16_t BlockNumber1, uint16_t BlockNumber2,
                               uint32_t FilePos,
                               volatile bool* const Escape);

ssnOff_t GetSBFFilePos(SBFData_t* SBFData);

ssnOff_t GetSBFFileLength(SBFData_t* SBFData);

void InitializeSBFDecoding(char* FileName,
                           SBFData_t* SBFData);

void InitializeSBFDecodingWithExistingFile(FILE* file,
        SBFData_t* SBFData);

void CloseSBFFile(SBFData_t* SBFData);

bool IsTimeValid(void* SBFBlock);

int GetCRCErrors();

#define SBFREAD_MEAS3_ENABLED      0x1
#define SBFREAD_MEASEPOCH_ENABLED  0x2
#define SBFREAD_ALLMEAS_ENABLED    0x3

/* sbfread_MeasCollectAndDecode() collects and decodes GNSS
   measurements from SBF blocks.  That function should be called for
   all SBF blocks in the file.  It collects all information from the
   measurement-related SBF blocks (MeasEpoch, MeasExtra, Meas3Ranges,
   Meas3Doppler,...) and it returns true when a complete measurement
   epoch is available, i.e. when all SBF blocks pertaining to the
   current epoch have been collected or we are at the end of the
   file. The decoded measurement epoch containing all observables from
   all satellites is provided in the MeasEpoch_t structure.

   Arguments:

   * SBFData: a pointer to the SBFData_t structure initialized with
     InitializeSBFDecoding()

   * SBFBlock: a pointer to the SBFBlock read from the file (see also
     GetNextBlock() function)

   * MeasEpoch: a pointer to the decoded measurement epoch.  MeasEpoch
     contains a valid measurement epoch only when the function returns
     true.

   * EnabledMeasType: defines which types of blocks are considered.
     It can be one of:
     SBFREAD_MEAS3_ENABLED: collect only the Meas3 blocks, ignoring MeasEpoch and MeasExtra
     SBFREAD_MEASEPOCH_ENABLED: collect only the MeasEpoch+MeasExtra blocks
     SBFREAD_ALLMEAS_ENABLED: collects whatever is available (recommended).
*/
bool sbfread_MeasCollectAndDecode(
    SBFData_t*                      SBFData,
    void*                           SBFBlock,
    MeasEpoch_t*                    MeasEpoch,
    uint32_t                        EnabledMeasTypes);

/*  sbfread_FlushMeasEpoch() forces the measurement decoder to
    process all available data from the current epoch, even if not
    all measurement SBF blocks from that epoch have been received */
bool sbfread_FlushMeasEpoch(SBFData_t*   SBFData,
                            MeasEpoch_t* MeasEpoch,
                            uint32_t     EnabledMeasTypes);


void sbfread_MeasEpoch_Decode(MeasEpoch_2_t* sbfMeasEpoch,
                              MeasEpoch_t*   MeasEpoch);

void sbfread_MeasExtra_Decode(MeasExtra_1_t* sbfMeasExtra,
                              MeasEpoch_t*   measEpoch);


double GetWavelength_m(SignalType_t SignalType, int GLOfn);


MeasEpochChannelType1_t
* GetFirstType1SubBlock(const MeasEpoch_2_t* MeasEpoch,
                        int* Type1Counter);

MeasEpochChannelType1_t
* GetNextType1SubBlock(const MeasEpoch_2_t*     MeasEpoch,
                       MeasEpochChannelType1_t* CurrentType1SubBlock,
                       int* Type1Counter);

bool IncludeThisEpoch(void*   SBFBlock,
                      int64_t ForcedFirstEpoch_ms,
                      int64_t ForcedLastEpoch_ms,
                      int     ForcedInterval_ms,
                      bool    AcceptInvalidTimeStamp);

void SBFDecryptBlock(SBFDecrypt_t* decrypt,
                     VoidBlock_t*  block);

#define TerminateProgram {char S[255];   \
  (void)snprintf(S, sizeof(S), "Abnormal program termination (%s,%i)", \
             __FILE__, __LINE__); \
  S[254] = '\0'; \
  perror(S); \
  exit(EXIT_FAILURE); \
  }

#ifdef __cplusplus
}
#endif

#endif
/* End of "ifndef SBFREAD_H" */
