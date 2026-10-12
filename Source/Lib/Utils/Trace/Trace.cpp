/*  Copyright (c) MediaArea.net SARL & Reto Kromer.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "Lib/Utils/Trace/Trace.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <mutex>
#include <string>
#include <vector>
#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
    #if defined(_M_X64) || defined(_M_IX86)
        #include <intrin.h>
        #define TRACE_CPU_CYCLES // Thread cycle counter, converted with the frequency of the time stamp counter
    #endif
#else
    #include <ctime>
#endif
//---------------------------------------------------------------------------

namespace trace
{

//---------------------------------------------------------------------------
bool IsEnabled = false;

//---------------------------------------------------------------------------
enum class type : uint8_t
{
    Duration,
    Span,
    Counter,
    Instant,
};

struct event
{
    const char*                 Name;
    uint64_t                    Begin;
    uint64_t                    End;
    size_t                      Frame;
    uint64_t                    Cpu; // Duration: CPU time, -1 if unknown, counter: value
    type                        Type;
};

struct thread_data
{
    std::vector<event>          Events;
    const char*                 Name = nullptr;
};

//---------------------------------------------------------------------------
uint64_t Now()
{
    return (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

//---------------------------------------------------------------------------
uint64_t Cpu()
{
    #if defined(TRACE_CPU_CYCLES)
        ULONG64 Cycles;
        if (!QueryThreadCycleTime(GetCurrentThread(), &Cycles))
            return 0;
        return Cycles;
    #elif defined(_WIN32)
        FILETIME Creation, Exit, Kernel, User;
        if (!GetThreadTimes(GetCurrentThread(), &Creation, &Exit, &Kernel, &User))
            return 0;
        return ((((uint64_t)Kernel.dwHighDateTime) << 32 | Kernel.dwLowDateTime) + (((uint64_t)User.dwHighDateTime) << 32 | User.dwLowDateTime)) * 100;
    #else
        timespec Time;
        if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &Time))
            return 0;
        return (uint64_t)Time.tv_sec * 1000000000 + Time.tv_nsec;
    #endif
}

//---------------------------------------------------------------------------
// Events of all threads, kept after the end of the threads
struct traces
{
    std::mutex                  Mutex;
    std::vector<thread_data*>   Threads;
    std::string                 FileName;
    uint64_t                    Start = 0;
    uint64_t                    Start_Ticks = 0;
    bool                        Written = false;

    traces()
    {
        auto Value = getenv("RAWCOOKED_TRACE");
        if (!Value || !*Value)
            return;
        FileName = Value;
        Start = Now();
        #if defined(TRACE_CPU_CYCLES)
            Start_Ticks = __rdtsc();
        #endif
        IsEnabled = true;
        atexit(trace::Write); // In case the program does not call it
    }

    void                        Write();
};
static traces& Traces = *new traces; // Not destroyed, used at exit

//---------------------------------------------------------------------------
static thread_data* Current()
{
    thread_local thread_data* Data = nullptr;
    if (!Data)
    {
        Data = new thread_data;
        Data->Events.reserve(1024);
        std::lock_guard<std::mutex> Lock(Traces.Mutex);
        Traces.Threads.push_back(Data);
    }
    return Data;
}

//---------------------------------------------------------------------------
void Event(const char* Name, uint64_t Begin, uint64_t End, size_t Frame, uint64_t Cpu)
{
    if (!IsEnabled)
        return;
    Current()->Events.push_back({ Name, Begin, End, Frame, Cpu, type::Duration });
}

//---------------------------------------------------------------------------
void Span(const char* Name, uint64_t Begin, uint64_t End, size_t Frame)
{
    if (!IsEnabled)
        return;
    Current()->Events.push_back({ Name, Begin, End, Frame, (uint64_t)-1, type::Span });
}

//---------------------------------------------------------------------------
void Counter(const char* Name, int64_t Value)
{
    if (!IsEnabled)
        return;
    auto Time = Now();
    Current()->Events.push_back({ Name, Time, Time, (size_t)-1, (uint64_t)Value, type::Counter });
}

//---------------------------------------------------------------------------
void Instant(const char* Name, size_t Frame)
{
    if (!IsEnabled)
        return;
    auto Time = Now();
    Current()->Events.push_back({ Name, Time, Time, Frame, (uint64_t)-1, type::Instant });
}

//---------------------------------------------------------------------------
void ThreadName(const char* Name)
{
    if (!IsEnabled)
        return;
    Current()->Name = Name;
}

//---------------------------------------------------------------------------
void Write()
{
    if (IsEnabled)
        Traces.Write();
}

//---------------------------------------------------------------------------
void traces::Write()
{
    std::lock_guard<std::mutex> Lock(Mutex);
    if (Written)
        return;
    Written = true;
    auto End = Now();

    // CPU time unit, in nanoseconds
    double Cpu_Unit = 1;
    #if defined(TRACE_CPU_CYCLES)
        Cpu_Unit = (double)(End - Start) / (__rdtsc() - Start_Ticks);
    #endif
    auto Cpu_Seconds = [&](uint64_t Value) { return Value * Cpu_Unit / 1000000000; };

    // Thread names, numbered if several threads have the same name
    std::map<std::string, size_t> Name_Counts, Name_Indexes;
    for (auto Thread : Threads)
        Name_Counts[Thread->Name ? Thread->Name : "Main"]++;
    std::vector<std::string> Names;
    for (auto Thread : Threads)
    {
        std::string Name = Thread->Name ? Thread->Name : "Main";
        if (Name_Counts[Name] > 1)
            Name += ' ' + std::to_string(++Name_Indexes[Name]);
        Names.push_back(Name);
    }

    // Chrome trace event format
    if (auto F = fopen(FileName.c_str(), "w"))
    {
        fputs("{\"displayTimeUnit\":\"ms\",\"traceEvents\":[\n", F);
        for (size_t t = 0; t < Threads.size(); t++)
        {
            fprintf(F, "%s{\"ph\":\"M\",\"pid\":1,\"tid\":%zu,\"name\":\"thread_name\",\"args\":{\"name\":\"%s\"}}", t ? ",\n" : "", t, Names[t].c_str());
            fprintf(F, ",\n{\"ph\":\"M\",\"pid\":1,\"tid\":%zu,\"name\":\"thread_sort_index\",\"args\":{\"sort_index\":%zu}}", t, t);
            for (const auto& Event : Threads[t]->Events)
            {
                auto Time = (double)(Event.Begin - Start) / 1000;
                switch (Event.Type)
                {
                case type::Duration:
                    fprintf(F, ",\n{\"ph\":\"X\",\"pid\":1,\"tid\":%zu,\"name\":\"%s\",\"ts\":%.3f,\"dur\":%.3f,\"args\":{", t, Event.Name, Time, (double)(Event.End - Event.Begin) / 1000);
                    if (Event.Cpu != (uint64_t)-1)
                        fprintf(F, "\"cpu_ms\":%.3f%s", Cpu_Seconds(Event.Cpu) * 1000, Event.Frame != (size_t)-1 ? "," : "");
                    if (Event.Frame != (size_t)-1)
                        fprintf(F, "\"frame\":%zu", Event.Frame);
                    fputs("}}", F);
                    break;
                case type::Span:
                    fprintf(F, ",\n{\"ph\":\"b\",\"cat\":\"span\",\"id\":%zu,\"pid\":1,\"tid\":%zu,\"name\":\"%s\",\"ts\":%.3f,\"args\":{\"frame\":%zu}}", Event.Frame, t, Event.Name, Time, Event.Frame);
                    fprintf(F, ",\n{\"ph\":\"e\",\"cat\":\"span\",\"id\":%zu,\"pid\":1,\"tid\":%zu,\"name\":\"%s\",\"ts\":%.3f}", Event.Frame, t, Event.Name, (double)(Event.End - Start) / 1000);
                    break;
                case type::Counter:
                    fprintf(F, ",\n{\"ph\":\"C\",\"pid\":1,\"tid\":%zu,\"name\":\"%s\",\"ts\":%.3f,\"args\":{\"value\":%lld}}", t, Event.Name, Time, (long long)Event.Cpu);
                    break;
                case type::Instant:
                    fprintf(F, ",\n{\"ph\":\"i\",\"s\":\"t\",\"pid\":1,\"tid\":%zu,\"name\":\"%s\",\"ts\":%.3f", t, Event.Name, Time);
                    if (Event.Frame != (size_t)-1)
                        fprintf(F, ",\"args\":{\"frame\":%zu}", Event.Frame);
                    fputs("}", F);
                    break;
                }
            }
        }
        fputs("\n]}\n", F);
        fclose(F);
    }

    // Summary: stages and instant events
    struct stage
    {
        size_t                  Count = 0;
        uint64_t                Total = 0;
        uint64_t                Cpu = 0;
        uint64_t                Max = 0;
    };
    std::map<std::string, stage> Stages;
    std::map<std::string, size_t> Instants;
    for (auto Thread : Threads)
        for (const auto& Event : Thread->Events)
            switch (Event.Type)
            {
            case type::Duration:
            case type::Span:
            {
                auto& Stage = Stages[Event.Name];
                auto Duration = Event.End - Event.Begin;
                Stage.Count++;
                Stage.Total += Duration;
                if (Event.Cpu != (uint64_t)-1)
                    Stage.Cpu += Event.Cpu;
                Stage.Max = std::max(Stage.Max, Duration);
                break;
            }
            case type::Instant:
                Instants[Event.Name]++;
                break;
            default:;
            }
    fprintf(stderr, "\nTrace: %.3f s, written to %s\n", (double)(End - Start) / 1000000000, FileName.c_str());
    fprintf(stderr, "%-32s %8s %10s %10s %10s %10s\n", "Stage", "Count", "Total (s)", "CPU (s)", "Mean (ms)", "Max (ms)");
    for (const auto& Stage : Stages)
        fprintf(stderr, "%-32s %8zu %10.3f %10.3f %10.3f %10.3f\n", Stage.first.c_str(), Stage.second.Count, (double)Stage.second.Total / 1000000000, Cpu_Seconds(Stage.second.Cpu), (double)Stage.second.Total / Stage.second.Count / 1000000, (double)Stage.second.Max / 1000000);
    for (const auto& Instant : Instants)
        fprintf(stderr, "%-32s %8zu\n", Instant.first.c_str(), Instant.second);

    // Summary: busy time of threads (union of their stages) and CPU time during the stages (outermost stages)
    fprintf(stderr, "%-32s %10s %8s %10s\n", "Thread", "Busy (s)", "Busy (%)", "CPU (s)");
    for (size_t t = 0; t < Threads.size(); t++)
    {
        std::vector<const event*> Durations;
        for (const auto& Event : Threads[t]->Events)
            if (Event.Type == type::Duration)
                Durations.push_back(&Event);
        if (Durations.empty())
            continue;
        std::sort(Durations.begin(), Durations.end(), [](const event* a, const event* b) { return a->Begin < b->Begin || (a->Begin == b->Begin && a->End > b->End); });
        uint64_t Busy = 0, Cpu = 0;
        uint64_t Current_Begin = Durations[0]->Begin, Current_End = Durations[0]->Begin;
        for (auto Event : Durations)
        {
            if (Event->Begin >= Current_End)
            {
                // Outermost stage
                Busy += Current_End - Current_Begin;
                Current_Begin = Event->Begin;
                if (Event->Cpu != (uint64_t)-1)
                    Cpu += Event->Cpu;
            }
            Current_End = std::max(Current_End, Event->End);
        }
        Busy += Current_End - Current_Begin;
        fprintf(stderr, "%-32s %10.3f %8.1f %10.3f\n", Names[t].c_str(), (double)Busy / 1000000000, (double)Busy / (End - Start) * 100, Cpu_Seconds(Cpu));
    }
}

} // namespace trace
