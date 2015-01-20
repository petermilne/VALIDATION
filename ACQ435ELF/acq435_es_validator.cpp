#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAGIC 0xaa55f154
#define MAGIC_FOURSOME(x4) \
	((x4)[0] == MAGIC && (x4)[1] == MAGIC && (x4)[2] == MAGIC && (x4)[3] == MAGIC)

int nchannels = 32;
int errors;

int verbose;

void validate(unsigned* xx, int nchannels, int sample, int *prev_sample)
{
	if (MAGIC_FOURSOME(xx)){
		int broke_at = 0;
		for (int iquad = 8; iquad < nchannels; iquad += 8){
			if (!MAGIC_FOURSOME(xx+iquad)){
				broke_at = iquad;
				++::errors;
				break;					
			}
		}
		if (verbose > 1){
		printf("%10d %10d %10d 0x%08x 0x%08x 0x%08x 0x%08x %d %s\n",
			sample, *prev_sample, sample-*prev_sample,
			xx[4], xx[5], xx[6], xx[7], broke_at,
			broke_at? "FAIL": "PASS");
		}
		*prev_sample = sample;
	}
}

int validate(const char* fname)
{
	FILE* fp = strcmp(fname, "-") ==0 ? stdin: fopen(fname, "r");
	if (fp == 0){ 
		perror(fname); exit(1);
	}
	unsigned* xx = new unsigned[nchannels];
	int sample = 0;
	int prev_sample = 0;

	while (fread(xx, sizeof(unsigned), nchannels, fp) == nchannels){
		validate(xx, nchannels, sample, &prev_sample);
		++sample;
	}

	if (strcmp(fname, "-")){
		fclose(fp);
	}
	if (verbose){
		printf("%d/%d %s\n", ::errors, sample, fname);
	}
	return errors;
}
int main(int argc, char *argv[])
{
	if (getenv("NCHANNELS")) nchannels = atoi(getenv("NCHANNELS"));
	if (getenv("VERBOSE")) verbose = atoi(getenv("VERBOSE"));

	if (argc == 1){
		return validate("-");
	}else{
		for (int iarg = 1; iarg < argc; ++iarg){
			if (validate(argv[iarg])){
				return 1;
			}
		}
	}
	return 0;
}
