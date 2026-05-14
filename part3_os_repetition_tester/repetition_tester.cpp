#include <cstdint>
#include <cstdio>

#include "linux_metrics.cpp"
/* Design:
 * ## Goals
 * Test() {
 * While(RT_Testing()) { 
 *		...
 *		RT_BeginTest()
 *		...code under test...
 *		RT_EndTest()
 *	}
 * }
 *
 * Test();
 *
 * ## State machine
 * ERROR
 * INIT
 * RUNNING
 * STOPPED
 */

enum rt_state {
	RT_ERROR,
	RT_INIT,
	RT_RUNNING,
	RT_STOPPED,
};

struct rt_stats {
	u64 TSC_elapsed_min;
	u64 TSC_elapsed_max;
	u64 TSC_elapsed_cumul;
	u64 n_runs;
};

struct repetition_tester {
	char *name;
	rt_state state;

	/* timers */
	u64 CPU_freq_s;
	u64 TSC_elapsed;
	u64 n_bytes_processed;

	/* stop condition */
	/* NOTE: other stop conditions like elapsed time exist */
	u64 TSC_since_last_min;
	u64 TSC_threshold;

	/* stats */
	rt_stats stats;
	u64 n_bytes_to_process;
};

void ErrorTester(repetition_tester *rt, char *msg) {
	rt->state = RT_ERROR;
	fprintf(stderr, "%s\n", msg);
}

void PrintGigabytesPerSecond(f64 TSC_elapsed, u64 CPUFreq, u64 n_bytes_processed) {
	u64 megabyte = 1024 * 1024;
	u64 gigabyte = 1024 * megabyte;

	f64 gbps = ((f64)n_bytes_processed / gigabyte) / (TSC_elapsed / CPUFreq);
	fprintf(stdout, "(%.4f GB/s)", gbps);
}

void PrintStat(repetition_tester *rt, char *name, f64 stat) {
	fprintf(stdout, "%s:\t%.f\t%.4fsec\t", name, stat, stat/rt->CPU_freq_s);
	PrintGigabytesPerSecond(stat, rt->CPU_freq_s, rt->n_bytes_to_process);
}

void PrintRepetitionTester(repetition_tester *rt) {
	fprintf(stderr, "\r");
	PrintStat(rt, (char *)"Min", rt->stats.TSC_elapsed_min);
	fprintf(stdout, "\n");
	PrintStat(rt, (char *)"Max", rt->stats.TSC_elapsed_max);
	fprintf(stdout, "\n");
	PrintStat(rt, (char *)"Avg", (f64)rt->stats.TSC_elapsed_cumul/rt->stats.n_runs);
	fprintf(stdout, "\n");
	fprintf(stdout, "------------------------ runs: %lu ------------------------\n", rt->stats.n_runs);
}

void InitTester(repetition_tester *rt, char *name, u64 n_bytes_to_process) {
	rt->name = name;
	rt->n_bytes_to_process = n_bytes_to_process;

	rt->state = RT_INIT;
	rt->CPU_freq_s = EstimateCPUFreq(1000);
	if (rt->CPU_freq_s == 0) {
		ErrorTester(rt, (char *)"CPU frequency is estimated to 0");
		return;
	}

	rt->TSC_threshold = rt->CPU_freq_s * 10;

	/* Reset all stats */
	rt->n_bytes_processed = 0;
	rt->stats.TSC_elapsed_min = -1;
	rt->stats.TSC_elapsed_max = 0;
	rt->stats.TSC_elapsed_cumul = 0;
	rt->stats.n_runs = 0;

	fprintf(stdout, "------------------------ %s ------------------------\n", name);

	return;
}

bool IsRunning(repetition_tester *rt) {
	if (rt->state == RT_ERROR || rt->state == RT_STOPPED) {
		return false;
	} else if (rt->state == RT_INIT) {
		rt->TSC_since_last_min = ReadCPUTimer();
		rt->state = RT_RUNNING;
		return true;
	} else if (rt->state != RT_RUNNING) {
		ErrorTester(rt, (char *) "Invalid tester state has been reached");
		return false;
	}

	if (rt->n_bytes_processed != rt->n_bytes_to_process) {
		fprintf(stderr, "--%lu:%lu\n", rt->n_bytes_processed, rt->n_bytes_to_process);
		ErrorTester(rt, (char *)"did not process the right amount of bytes");
		return false;
	}
	rt->n_bytes_processed = 0;

	u64 elapsed = rt->TSC_elapsed;
	rt->TSC_elapsed = 0;
	++ rt->stats.n_runs;
	rt->stats.TSC_elapsed_cumul += elapsed;

	/* NOTE: separate if (instead of 'else if') because of first time max/min is set
	 * both need to be triggered */
	if (elapsed > rt->stats.TSC_elapsed_max) {
		rt->stats.TSC_elapsed_max = elapsed;
	}
	if (elapsed < rt->stats.TSC_elapsed_min) {
		rt->stats.TSC_elapsed_min = elapsed;
		rt->TSC_since_last_min = ReadCPUTimer();

		/* NOTE: '\r prints from start of line again */
		PrintStat(rt, (char *) "Min", (f64) elapsed);
		printf("               \r");
	}

	if (rt->TSC_since_last_min + rt->TSC_threshold < ReadCPUTimer()) {
		rt->state = RT_STOPPED;
		return false;
	}

	return true;
}

void BeginTime(repetition_tester *rt) {
	if (rt->state != RT_RUNNING) {
		ErrorTester(rt, (char *) "cannot time a errored repetition tester");
		return;
	}
	rt->TSC_elapsed -= ReadCPUTimer();
}

void EndTime(repetition_tester *rt, u64 n_bytes_processed) {
	if (rt->state != RT_RUNNING) {
		ErrorTester(rt, (char *) "cannot time a errored repetition tester");
		return;
	}
	rt->TSC_elapsed += ReadCPUTimer();
	rt->n_bytes_processed += n_bytes_processed;
}
