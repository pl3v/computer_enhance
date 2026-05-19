#include <cstdint>
#include <cstdio>
#include <cassert>

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
	u64 n_runs;
	u64 TSC_elapsed_min;
	u64 TSC_elapsed_max;
	u64 TSC_elapsed_cumul;
	u64 PFLT_min;
	u64 PFLT_max;
	u64 PFLT_cumul;
};

struct repetition_tester {
	rt_state state;
	
	/* elapsed */
	u64 TSC_elapsed;
	u64 pagefaults_elapsed;
	u64 n_bytes_processed;

	/* stop condition */
	/* NOTE: other stop conditions like elapsed time exist */
	u64 TSC_since_last_min;
	u64 TSC_threshold;

	/* stats */
	rt_stats stats;

	/* logging pagefaults */
	bool log_faults;
	FILE *log;

	/* const */
	u64 CPU_freq_s;
	u64 n_bytes_to_process;
	char *name;
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

void PrintStat(repetition_tester *rt, char *name, f64 stat, f64 stat2) {
	fprintf(stdout, "%s:\t%.f\t%.4fsec\t%.4f\t", name, stat, stat/rt->CPU_freq_s, stat2);
	PrintGigabytesPerSecond(stat, rt->CPU_freq_s, rt->n_bytes_to_process);
}

void PrintRepetitionTester(repetition_tester *rt) {
	// NOTE: Spaces are necessary as line still has previous text on it to overwrite with whitespace
	fprintf(stdout, "\r                                                                   ");
	fprintf(stdout, "\rStat\tCycles\tSeconds\tPageFaults\tThroughput");
	fprintf(stdout, "\n");

	PrintStat(rt, (char *)"Min", rt->stats.TSC_elapsed_min, rt->stats.PFLT_min);
	fprintf(stdout, "\n");
	PrintStat(rt, (char *)"Max", rt->stats.TSC_elapsed_max, rt->stats.PFLT_max);
	fprintf(stdout, "\n");
	PrintStat(rt, (char *)"Avg", (f64)rt->stats.TSC_elapsed_cumul/rt->stats.n_runs, (f64)rt->stats.PFLT_cumul/rt->stats.n_runs);
	fprintf(stdout, "\n");

	fprintf(stdout, "------------------------ runs: %lu ------------------------\n", rt->stats.n_runs);
}

void InitLogger(repetition_tester *rt, char *filename) {
	if (rt->state != RT_INIT) {
		ErrorTester(rt, (char *) "Cannot initlogger before inittester");
		return;
	}

	rt->log_faults = true;
	rt->log = fopen(filename, "w");
	if (!rt->log) {
		ErrorTester(rt, (char *) "Cannot create/write to log file");
		return;
	}

	fprintf(rt->log, "\"Number of Pagefaults\"\n");
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

	rt->n_bytes_processed = 0;
	rt->TSC_elapsed = 0;
	rt->pagefaults_elapsed = 0;

	/* Reset all stats */
	rt->stats.n_runs = 0;
	rt->stats.TSC_elapsed_min = -1;
	rt->stats.TSC_elapsed_max = 0;
	rt->stats.TSC_elapsed_cumul = 0;
	rt->stats.PFLT_min = -1;
	rt->stats.PFLT_max = 0;
	rt->stats.PFLT_cumul = 0;

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

	++ rt->stats.n_runs;

	u64 elapsed = rt->TSC_elapsed;
	rt->TSC_elapsed = 0;
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
		PrintStat(rt, (char *) "Min", (f64) elapsed, -1);
		printf("               \r");
	}
	
	u64 pflt_elapsed = rt->pagefaults_elapsed;
	rt->pagefaults_elapsed = 0;
	rt->stats.PFLT_cumul += pflt_elapsed;
	if (pflt_elapsed < rt->stats.PFLT_min) {
		rt->stats.PFLT_min = pflt_elapsed;
	}
	if (pflt_elapsed > rt->stats.PFLT_max) {
		rt->stats.PFLT_max = pflt_elapsed;
	}

	if (rt->TSC_since_last_min + rt->TSC_threshold < ReadCPUTimer()) {
		rt->state = RT_STOPPED;
		return false;
	}

	if (rt->log_faults) {
		fprintf(rt->log, "%lu\n", pflt_elapsed);
	}

	return true;
}

void BeginTime(repetition_tester *rt) {
	if (rt->state != RT_RUNNING) {
		ErrorTester(rt, (char *) "cannot time a errored repetition tester");
		return;
	}
	rt->TSC_elapsed -= ReadCPUTimer();
	u64 n;
	assert(ReadPageFaults(&n) == 0);
	rt->pagefaults_elapsed -= n;
}

void EndTime(repetition_tester *rt, u64 n_bytes_processed) {
	if (rt->state != RT_RUNNING) {
		ErrorTester(rt, (char *) "cannot time a errored repetition tester");
		return;
	}
	rt->TSC_elapsed += ReadCPUTimer();
	rt->n_bytes_processed += n_bytes_processed;
	u64 n;
	assert(ReadPageFaults(&n) == 0);
	rt->pagefaults_elapsed += n;
}
