#define PROFILER 1

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// #include "haver.h"
typedef uint8_t u8;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int64_t s64;

typedef double f64;

#define READ_TIMER ReadCPUTimer()
#define READ_TIMER_FREQ EstimateCPUFreq(1000)
#include "haver_json.h"

#include "linux_metrics.cpp"
#include "profiler.cpp"
#include "haver_json.cpp"
#include "listing_0065_haversine_formula.cpp"


/* NOTE: OS timer has significant overhead!!!
 * ----- OS Timer:
 * Total time: 8946.8480ms (Timer freq 1000000)
 * ParseJsonSep[1]: 43 (0.00% -- with child: 26.70%)
 * Tokenise[1]: 2009084 (22.46%)
 * Parse[1]: 379841 (4.25%)
 * ReferenceHaversine[1000000]: 1708363 (19.09%)
 * Calculate Loop[1000000]: 3226731 (36.07% -- with child: 55.16%)
 * Free json[1]: 17254 (0.19%)
 * ----- RDTSC timer:
 * Total time: 2566.3383ms (Timer freq 3192781824)
 * ParseJsonSep[1]: 209376 (0.00% -- with child: 93.25%)
 * Tokenise[1]: 6533591424 (79.74%)
 * Parse[1]: 1106765056 (13.51%)
 * ReferenceHaversine[1000000]: 294830848 (3.60%)
 * Calculate Loop[1000000]: 123651520 (1.51% -- with child: 5.11%)
 * Free json[1]: 78867776 (0.96%)
 * Example 2:
 * ----- OS timer:
 * Total time: 2541.2410ms (Timer freq 1000000)
 * ParseJsonSep[1]: 37 (0.00% -- with child: 94.35%)
 * Tokenise[1]: 2016778 (79.36%)
 * Parse[1]: 380928 (14.99%)
 * Calculate Loop[1]: 116642 (4.59%)
 * Free json[1]: 26811 (1.06%)
 * ----- RDTSC timer:
 * Total time: 2552.3242ms (Timer freq 3195088100)
 * ParseJsonSep[1]: 230592 (0.00% -- with child: 94.33%)
 * Tokenise[1]: 6564577952 (80.50%)
 * Parse[1]: 1127945984 (13.83%)
 * Calculate Loop[1]: 375397312 (4.60%)
 * Free json[1]: 86617568 (1.06%)
 */
// #define READ_TIMER ReadOSTimer()
// #define READ_TIMER_FREQ GetOSTimerFreq()


static f64 JsonHaver(char *filename, u64 file_len)
{
    Json json;
    {
    TimeBandwidth("Parse json", file_len);
    ParseJsonSep(&json, filename);
    }

    u32 arr_head_off = GetArrayHead(&json, FindValue(&json, (char *)"pairs"));
    ArrayHead *ah = (ArrayHead *) OffsetToPtr(&json.a, arr_head_off);

    Json view;
    view.a = json.a;

    f64 coef = 1.0 / (f64)ah->size;
    u32 arr_off = ah->array_offset;

    f64 sum;

    Array *a;
    Value *v;
    f64 x0, y0, x1, y1;
    {
    TimeBandwidth("Calculate Loop", ah->size*4*8);
    while (arr_off != 0) {

        a = (Array *) OffsetToPtr(&json.a, arr_off);
        v = (Value *) OffsetToPtr(&json.a, a->value_offset);
        assert(v->type == ValueObject);
        view.head_offset = v->object_offset;
        v = ((Value *) OffsetToPtr(&json.a, FindValue(&view, (char *)"x0")));
        x0 = v->fraction;
        v = ((Value *) OffsetToPtr(&json.a, FindValue(&view, (char *)"y0")));
        y0 = v->fraction;
        v = ((Value *) OffsetToPtr(&json.a, FindValue(&view, (char *)"x1")));
        x1 = v->fraction;
        v = ((Value *) OffsetToPtr(&json.a, FindValue(&view, (char *)"y1")));
        y1 = v->fraction;


        // fprintf(stdout, "%lf, %lf, %lf, %lf\n", x0, y0, x1, y1);
        f64 EarthRadius = 6372.8;
        f64 HaversineDistance = ReferenceHaversine(x0, y0, x1, y1, EarthRadius);
        sum += HaversineDistance*coef;


        arr_off = a->next_offset;
    }
    }

    fprintf(stdout, "Pair count: %lu\n", ah->size);
    fprintf(stdout, "Expected sum: %.16f\n", sum);

    {
        // TimeBlock("Free json");
        AOFree(&json.a);
    }

    return sum;
}

static f64 BinaryHaver(char * filename)
{
    f64 res;

    FILE *fp = fopen(filename, "rb");
    while (fread(&res, sizeof(f64), 1, fp));
    fclose(fp);

    return res;
}

int main (int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stdout, "usage: haver.out [data_x_flex.json] [data_x_haveranswer.f64] \n");
        exit(EXIT_FAILURE);
    }

    fprintf(stdout, "Json haversine:\n");
    FILE *fp = fopen(argv[1], "r");
    fseek(fp, 0, SEEK_END);
    u64 file_len = ftell(fp);
    fclose(fp);

    BeginProfile();
    f64 json_sum = JsonHaver(argv[1], file_len);
    EndPrintProfiler;

    f64 bin_sum = BinaryHaver(argv[2]);

    fprintf(stdout, "\nValidation:\n");
    fprintf(stdout, "Reference sum: %.16f\n", bin_sum);
    fprintf(stdout, "Difference: %.16f\n", bin_sum - json_sum);

    return 0;
}

ProfilerEndOfCompilationUnit;
