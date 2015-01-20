#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAGIC 0xaa55f154


/*
#define IS_MAGIC(x)	((x)==MAGIC)
*/
#define MAGIC_MASK	0xFFFFFFF0
#define IS_MAGIC(x)	(((x)&MAGIC_MASK)==(MAGIC&MAGIC_MASK))

#define MAGIC_FOURSOME(x4)	(IS_MAGIC((x4)[0]) && IS_MAGIC((x4)[1]) && IS_MAGIC((x4)[2]) && IS_MAGIC((x4)[3]))

int nchannels = 32;
int errors;

int verbose;

int validate(unsigned* xx, int nchannels, int sample, int *prev_sample)
/* -1 ES_ERR, 0 : no ES, 1: ESGOOD */
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
		return broke_at? -1: 1;
	}else{
		return 0;
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
		if (validate(xx, nchannels, sample, &prev_sample) == 0){
			/* check for second half ES */
			validate(xx+nchannels/2, nchannels, sample, &prev_sample);
		}
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
