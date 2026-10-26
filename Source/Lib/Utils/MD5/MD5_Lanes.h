/*  Copyright (c) MediaArea.net SARL & Reto Kromer.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#ifndef MD5_LanesH
#define MD5_LanesH
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include <cstddef>
#include <cstdint>
extern "C"
{
#include "md5.h"
}
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// Same as MD5_Update() of each part, computed in a SIMD lane in parallel with the parts given by other threads at the same time (e.g. several files)
// Parts are computed in lanes only if there are enough parts from other threads for a lower CPU usage than MD5_Update() (count measured on the CPU, 1 if lanes are faster than MD5_Update()),
// else with MD5_Update() in the thread of the caller, so it is not slower than MD5_Update() and the CPU usage is lower when several threads call it at the same time
// Batch: count of callers giving parts at the same time (e.g. MD5_Lanes_Count() if possible), the computing may wait for parts from this count of callers (lower CPU usage), also when other parts computed with them are done, during a limited time, or until MD5_Lanes_Flush(true) is called; 0 if the caller does not accept to wait
struct md5_part
{
    const uint8_t*              Data;
    size_t                      Size;
};
void MD5_Update_Lanes(MD5_CTX* Ctx, const md5_part* Parts, size_t Count, size_t Batch = 0);
size_t MD5_Lanes_Count(); // Count of callers computed together in the SIMD lanes of an engine, 1 if SIMD lanes are not available
void MD5_Lanes_Flush(bool Begin); // No wait for other parts from MD5_Lanes_Flush(true) to MD5_Lanes_Flush(false), e.g. when a caller can not give more parts before the end of pending computings
const size_t MD5_Lanes_Batch_Min = 3; // Count of jobs computed together if jobs accept to wait for other jobs, batch is useful only with at least this count of threads (else each job waits)

//---------------------------------------------------------------------------
#endif
