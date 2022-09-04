/*
 * sviddef.h: Septentrio specific GNSS satellite numbering.
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

#ifndef SVIDDEF_H
#define SVIDDEF_H 1

/* Septentrio-specific internal SVID numbering (this numbering is
   different from the numbering in the SBF blocks):

    1- 37     : GPS
   38- 68     : GLONASS
   71-106     : GALILEO
  107-119     : L-Band (MSS)
  120-158     : SBAS, Regional Augmentation (RA)
  161-223     : BeiDou/Compass
  224-230     : QZSS
  231-244     : IRNSS (NAVIC)
*/


/* GPS */
#define gpMINPRN             1U
#define gpMAXPRN            37U
#define gpNBRPRN            ((gpMAXPRN-gpMINPRN)+1U)

/* GLONASS */
/* At cold boot, the relation between the slot numbers and the freq NR
   is unknown. The PRN number glPRNUNKWN is reserved to indicate that
   the actual slot number is unknown. */
#define glMINPRN            38U
#define glMAXPRN            67U
#define glNBRPRN            ((glMAXPRN-glMINPRN)+1U)
#define glPRNUNKWN          (glMAXPRN+1U)
#define glNBRFN             14U  /**< number of frequency numbers (from -7 to +6)*/

/* GALILEO */
#define galMINPRN           71U
#define galMAXPRN           106U
#define galNBRPRN           ((galMAXPRN-galMINPRN)+1U)

/* L-Band (Inmarsat) satellites */
#define lbMINPRN            107U
#define lbMAXPRN            117U
#define lbNBRPRN            ((lbMAXPRN-lbMINPRN)+1U)
#define lbPRNUNKWN1         119U /* unknown lbr PRN for channel 1 */
#define lbPRNUNKWN2         118U /* unknown lbr PRN for channel 2 */

/* SBAS (regional augmentation) */
#define raMINPRN            120U
#define raMAXPRN            158U
#define raNBRPRN            ((raMAXPRN-raMINPRN)+1U)

/* BeiDou */
#define cmpMINPRN           161U
#define cmpMAXPRN           223U
#define cmpNBRPRN           ((cmpMAXPRN-cmpMINPRN)+1U)

/* QZSS */
#define qzMINPRN            224U
#define qzMAXPRN            230U
#define qzNBRPRN            ((qzMAXPRN-qzMINPRN)+1U)

/* IRNSS */
#define irMINPRN            231U
#define irMAXPRN            244U
#define irNBRPRN            ((irMAXPRN-irMINPRN)+1U)


// maximum number of PRNs in a single constellation
#define MAX_NBR_PRN  cmpNBRPRN  // BDS has the largest constellation (63)


#define agIsGLONASS(prn) (((unsigned long)prn >= glMINPRN) &&  \
                          ((unsigned long)prn <= glPRNUNKWN))

#define agIsGLOKWN(prn)  (((unsigned long)prn >= glMINPRN) && \
                          ((unsigned long)prn <= glMAXPRN))

#define agIsGPS(prn)     (((unsigned long)prn >= gpMINPRN) && \
                          ((unsigned long)prn <= gpMAXPRN))

#define agIsRA(prn)      (((unsigned long)prn >= raMINPRN) && \
                          ((unsigned long)prn <= raMAXPRN))

#define agIsGAL(prn)     (((unsigned long)prn >= galMINPRN) && \
                          ((unsigned long)prn <= galMAXPRN))

#define agIsCOMPASS(prn) (((unsigned long)prn >= cmpMINPRN) && \
                          ((unsigned long)prn <= cmpMAXPRN))

#define agIsLBR(prn)    (((unsigned long)prn >= lbMINPRN) && \
                         ((unsigned long)prn <= lbPRNUNKWN1))

#define agIsQZSS(prn)    (((unsigned long)prn >= qzMINPRN) && \
                          ((unsigned long)prn <= qzMAXPRN))

#define agIsIRNSS(prn)   (((unsigned long)prn >= irMINPRN) && \
                          ((unsigned long)prn <= irMAXPRN))

#define agIsValid(prn) (agIsGPS(prn)     || \
                        agIsGLONASS(prn) || \
                        agIsGAL(prn)     || \
                        agIsCOMPASS(prn) || \
                        agIsLBR(prn)     || \
                        agIsQZSS(prn)    || \
                        agIsIRNSS(prn)   || \
                        agIsRA(prn) )

#endif

