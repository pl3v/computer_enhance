#include <cstdlib>
#include <cstdio>

#include "repetition_tester.cpp"

typedef uint8_t u8;

struct write_tester {
	char *log_filename;
	u64 bsize;
};

void TestLinearWrite(char *name, write_tester *params) {
	repetition_tester rep;
	InitTester(&rep, name, params->bsize);
	InitLogger(&rep, params->log_filename);

	while (IsRunning(&rep)) {
		char *buffer = (char *) malloc(params->bsize);

		BeginTime(&rep);
		for (u64 i = 0; i < params->bsize; i++) {
			buffer[i] = (u8) i;
		}
		EndTime(&rep, params->bsize);
		
		free(buffer);
	}

	PrintRepetitionTester(&rep);
}

int main () {
	u64 buffer_size = 1024 * 1024 * 100;
	write_tester external_malloc = {.log_filename = (char *) "linear_pagefaults.csv", .bsize = buffer_size};
	
	TestLinearWrite((char *) "linear write", &external_malloc);
	
	return 0;
}
