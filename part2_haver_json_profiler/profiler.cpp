// NOTE: other profilers to checkout
// iprof
// -- callstack based zone attribution

#include <cassert>
#include <cstdio>
struct profile_anchor
{
    u64 TSCElapsedInclusive;  // includes all child times
    u64 TSCElapsedExclusive;  // excludes children (only root, even in recursive calling scenario)
    u64 HitCount;

    u64 ProcessedByteCount;

    char const *Label;
};

struct profiler
{
    profile_anchor Anchors[4096];

    u32 ParentAnchorIndex;

    u64 StartTSC;
    u64 EndTSC;
};
static profiler GlobalProfiler;

struct profile_block
{
    profile_block(char const *Label_, u32 AnchorIndex_, u64 ByteCount_)
    {
        AnchorIndex = AnchorIndex_;
        Label = Label_;
        StartTSC = READ_TIMER;

        profile_anchor *Anchor = GlobalProfiler.Anchors + AnchorIndex;
        RootTSC = Anchor->TSCElapsedExclusive;
        Anchor->ProcessedByteCount += ByteCount_;

        ParentIndex = GlobalProfiler.ParentAnchorIndex;
        GlobalProfiler.ParentAnchorIndex = AnchorIndex_;
    }
    
    ~profile_block(void)
    {
        u64 Elapsed = READ_TIMER - StartTSC;
        
        profile_anchor *Anchor = GlobalProfiler.Anchors + AnchorIndex;
        profile_anchor *Parent = GlobalProfiler.Anchors + ParentIndex;
        ++Anchor->HitCount;
        Anchor->TSCElapsedExclusive = RootTSC + Elapsed;
        Anchor->TSCElapsedInclusive += Elapsed;
        // NOTE: bogus parent at 0 so need to skip Anchor index 0 at print n such
        Parent->TSCElapsedInclusive -= Elapsed;

        GlobalProfiler.ParentAnchorIndex = ParentIndex;
        /* NOTE(casey): This write happens every time solely because there is no
           straightforward way in C++ to have the same ease-of-use. In a better programming
           language, it would be simple to have the anchor points gathered and labeled at compile
           time, and this repetitive write would be eliminated. */
        Anchor->Label = Label;
    }

    char const *Label;
    u64 StartTSC;
    u64 RootTSC;
    u32 AnchorIndex;
    u32 ParentIndex;
};

#ifndef PROFILER
#define PROFILER 0
#endif

#if PROFILER
// TODO: Find why name concat macros are split
#define NameConcat2(A, B) A##B
#define NameConcat(A, B) NameConcat2(A, B)

#define TimeBandwidth(Name, ByteCount) profile_block NameConcat(Block, __LINE__)(Name, __COUNTER__ + 1, ByteCount)
#define TimeBlock(Name) TimeBandwidth(Name, 0)
#define TimeFunction TimeBlock(__func__)
#define EndPrintProfiler EndAndPrintProfile();
#define ProfilerEndOfCompilationUnit static_assert(__COUNTER__ < ArrayCount(GlobalProfiler.Anchors), "Number of profile points exceeds size of profiler::Anchors array")

#else

#define TimeBandwidth(...)
#define TimeBlock(...)
#define TimeFunction TimeBlock(...)
#define EndPrintProfiler
#define ProfilerEndOfCompilationUnit

#endif

static void PrintTimeElapsed(u64 TotalTSCElapsed, profile_anchor *Anchor, f64 TimerFreq)
{
    u64 Elapsed = Anchor->TSCElapsedInclusive;
    f64 Percent = 100.0 * ((f64)(Elapsed) / (f64)TotalTSCElapsed);
    printf("  %s[%lu]: %lu (%.2f%%", Anchor->Label, Anchor->HitCount, Elapsed, Percent);

    if (Anchor->TSCElapsedExclusive != Elapsed) {
        f64 RootPercent = 100.0 * ((f64)Anchor->TSCElapsedExclusive / (f64)TotalTSCElapsed);
        printf(" -- with child: %.2f%%", RootPercent);
    }
    if(Anchor->ProcessedByteCount)
    {
        f64 Megabyte = 1024.0f*1024.0f;
        f64 Gigabyte = Megabyte*1024.0f;

        f64 Megabytes = (f64)Anchor->ProcessedByteCount / (f64)Megabyte;

        f64 Seconds = (f64)Anchor->TSCElapsedInclusive / TimerFreq;
        f64 BytesPerSecond = (f64)Anchor->ProcessedByteCount / Seconds;
        f64 GigabytesPerSecond = BytesPerSecond / Gigabyte;

        printf("  %.3fmb at %.2fgb/s", Megabytes, GigabytesPerSecond);
    }

    printf(")\n");
}

static void BeginProfile(void)
{
    GlobalProfiler.StartTSC = READ_TIMER;
}

void EndAndPrintProfile()
{
    GlobalProfiler.EndTSC = READ_TIMER;
    u64 TotalCPUElapsed = GlobalProfiler.EndTSC - GlobalProfiler.StartTSC;

    // NOTE: clear zero anchor as that is parent anchor for first real anchor
    GlobalProfiler.Anchors[0] = {};

    u64 TimerFreq = READ_TIMER_FREQ;
    assert(TimerFreq != 0);

    printf("\nTotal time: %0.4fms (Timer freq %lu)\n", 1000.0 * (f64)TotalCPUElapsed / (f64)TimerFreq, TimerFreq);
    
    for(u32 AnchorIndex = 0; AnchorIndex < ArrayCount(GlobalProfiler.Anchors); ++AnchorIndex)
    {
        profile_anchor *Anchor = GlobalProfiler.Anchors + AnchorIndex;
        if(Anchor->TSCElapsedInclusive)
        {
            PrintTimeElapsed(TotalCPUElapsed, Anchor, TimerFreq);
        }
    }
}
