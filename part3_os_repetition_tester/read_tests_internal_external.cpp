/* NOTE: read example results at bottom */

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

void TestFread(char *name, read_tester *read_t) {
	FILE *fp = fopen(read_t->filename, "r");
	if (!fp) {
		fclose(fp);
		fprintf(stderr, "File '%s' could not open\n", read_t->filename);
		return;
	}

	u8 *buffer = read_t->buffer;
	bool internal_buffer = false;
	if (buffer == NULL) {
		internal_buffer = true;
		buffer = (u8 *) malloc(read_t->bsize);
	}
	
	repetition_tester rep;
	InitTester(&rep, name, read_t->bsize);

	while (IsRunning(&rep)) {
		BeginTime(&rep);
		u64 n_bytes = (u64) fread(buffer, 1, read_t->bsize, fp);
		// fprintf(stderr, "%lu:%lu\n", n_bytes, read_t->bsize);
		EndTime(&rep, n_bytes);

		rewind(fp);
	}
	fclose(fp);
	if (internal_buffer) {
		free(buffer);
	}

	PrintRepetitionTester(&rep);
}

int main () {
	char *filename = (char *) "test.json";
	u64 file_size = UtilFileSize(filename);

	u8 *buffer = (u8 *) malloc(file_size);
	read_tester external_malloc = {.filename = filename, .buffer = buffer, .bsize = file_size};
	read_tester internal_malloc = {.filename = filename, .buffer = NULL, .bsize = file_size};
	
	while (1) {
		TestFread((char *) "fread external", &external_malloc);
		TestFread((char *) "fread internal", &internal_malloc);
	}
	
	return 0;
}


/* NOTE: example results show internal max keeps at ~1GB/s whereas
   repeated runs of external make max almost same as min
   ------------------------ fread external ------------------------
   Min:	136379200	0.0427sec	(2.4561 GB/s)
   Max:	355418496	0.1113sec	(0.9424 GB/s)
   Avg:	137734937	0.0431sec	(2.4319 GB/s)
   ------------------------ runs: 449 ------------------------
   ------------------------ fread internal ------------------------
   Min:	129229760	0.0405sec	(2.5920 GB/s)
   Max:	297096480	0.0930sec	(1.1274 GB/s)
   Avg:	130147419	0.0407sec	(2.5737 GB/s)
   ------------------------ runs: 649 ------------------------
   ------------------------ fread external ------------------------
   Min:	136421728	0.0427sec	(2.4553 GB/s)
   Max:	148662432	0.0465sec	(2.2531 GB/s)
   Avg:	137272882	0.0430sec	(2.4401 GB/s)
   ------------------------ runs: 361 ------------------------
   ------------------------ fread internal ------------------------
   Min:	139754976	0.0438sec	(2.3968 GB/s)
   Max:	313654208	0.0982sec	(1.0679 GB/s)
   Avg:	140846266	0.0441sec	(2.3782 GB/s)
   ------------------------ runs: 264 ------------------------
   ------------------------ fread external ------------------------
   Min:	136414592	0.0427sec	(2.4554 GB/s)
   Max:	148012992	0.0463sec	(2.2630 GB/s)
   Avg:	137116302	0.0429sec	(2.4429 GB/s)
   ------------------------ runs: 494 ------------------------
   ------------------------ fread internal ------------------------
   Min:	129507584	0.0405sec	(2.5864 GB/s)
   Max:	299911392	0.0939sec	(1.1169 GB/s)
   Avg:	130355973	0.0408sec	(2.5696 GB/s)
   ------------------------ runs: 708 ------------------------
*/
