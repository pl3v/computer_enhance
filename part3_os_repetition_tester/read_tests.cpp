#include <cstdlib>
#include <cstdio>

#include "repetition_tester.cpp"

typedef uint8_t u8;

u64 UtilFileSize(char *filename) {
	FILE *fp = fopen(filename, "r");
	if (!fp) {
		fclose(fp);
		fprintf(stderr, "File '%s' could not open\n", filename);
		return 0;
	}

	fseek(fp, 0L, SEEK_END);
	u64 size = (u64) ftell(fp);
	fclose(fp);

	return size;
}

struct read_tester {
	char *filename;
	u8 *buffer;
	u64 bsize;
};

void TestFread(repetition_tester *rep, read_tester *read_t) {
	FILE *fp = fopen(read_t->filename, "r");
	if (!fp) {
		fclose(fp);
		fprintf(stderr, "File '%s' could not open\n", read_t->filename);
		return;
	}

	InitTester(rep, (char *)"fread", read_t->bsize);
	while (IsRunning(rep)) {
		BeginTime(rep);
		u64 n_bytes = (u64) fread(read_t->buffer, 1, read_t->bsize, fp);
		// fprintf(stderr, "%lu:%lu\n", n_bytes, read_t->bsize);
		EndTime(rep, n_bytes);

		rewind(fp);
	}
	fclose(fp);

	PrintRepetitionTester(rep);
}

int main () {
	char *filename = (char *) "test.json";
	u64 file_size = UtilFileSize(filename);

	u8 *buffer = (u8 *) malloc(file_size);
	read_tester read_t = {.filename = filename, .buffer = buffer, .bsize = file_size};
	
	while (1) {
		repetition_tester rt = {};
		TestFread(&rt, &read_t);
	}
	
	return 0;
}
