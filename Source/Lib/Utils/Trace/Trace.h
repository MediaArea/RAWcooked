/*  Copyright (c) MediaArea.net SARL & Reto Kromer.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#ifndef TraceH
#define TraceH
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include <cstddef>
#include <cstdint>
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// Instrumentation: timeline of the processing stages in each thread, for performance analysis
// Enabled if the RAWCOOKED_TRACE environment variable is set: Chrome trace event format (JSON, e.g. for https://ui.perfetto.dev) written to the file named by the variable
// at the end, and a summary (count, wall and CPU time of each stage, busy and CPU time of each thread, count of each instant event) written to stderr
// Disabled: each instrumentation point costs a test of a global boolean
// Names are string literals (pointers are kept)
namespace trace
{
    extern bool                 IsEnabled;
    inline bool                 Enabled() { return IsEnabled; }
    uint64_t                    Now(); // Nanoseconds
    uint64_t                    Cpu(); // CPU time used by the current thread, in units converted at the end

    void                        Event(const char* Name, uint64_t Begin, uint64_t End, size_t Frame = (size_t)-1, uint64_t Cpu = (uint64_t)-1); // Stage of Frame (or no frame) from Begin to End, with the CPU time used by the thread during the stage (from Cpu()) if known
    void                        Span(const char* Name, uint64_t Begin, uint64_t End, size_t Frame); // Stage of Frame across threads (e.g. decoding of a frame by several threads), stages may overlap
    void                        Counter(const char* Name, int64_t Value); // Value of a counter (e.g. count of frames in a queue) from now
    void                        Instant(const char* Name, size_t Frame = (size_t)-1); // Instant event (e.g. a decision)
    void                        ThreadName(const char* Name); // Name of the current thread in the timeline (threads with the same name are numbered)
    void                        Write(); // Writes the trace and the summary (once, also called at exit)

    // Stage from the creation to the destruction of the object (or to End())
    class scope
    {
    public:
        scope(const char* Name_, size_t Frame_ = (size_t)-1) : Name(Name_), Frame(Frame_), Begin(Enabled() ? Now() : 0), Cpu_Begin(Begin ? Cpu() : 0) {}
        ~scope() { End(); }
        void                    End() { if (Begin) { Event(Name, Begin, Now(), Frame, Cpu() - Cpu_Begin); Begin = 0; } }

    private:
        const char*             Name;
        size_t                  Frame;
        uint64_t                Begin;
        uint64_t                Cpu_Begin;
    };
}

#define TRACE_CONCAT2(A, B) A##B
#define TRACE_CONCAT(A, B) TRACE_CONCAT2(A, B)
#define TRACE_SCOPE(...) trace::scope TRACE_CONCAT(Trace_Scope_, __LINE__)(__VA_ARGS__)

//---------------------------------------------------------------------------
#endif
