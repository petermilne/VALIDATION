#include <stdio.h>
#include <stdlib.h>

#define MAGIC 0xaa55f154
#define MAGIC_FOURSOME(x4) \
	((x4)[0] == MAGIC && (x4)[1] == MAGIC && (x4)[2] == MAGIC && (x4)[3] == MAGIC)

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
int main(int argc, char *argv[])
{
	int nchannels = 32;
	if (argc > 1) nchannels = atoi(argv[1]);
	if (getenv("VERBOSE")) verbose = atoi(getenv("VERBOSE"));

	int sample = 0;
	int prev_sample = 0;
	unsigned* xx = new unsigned[nchannels];

	while (fread(xx, sizeof(unsigned), nchannels, stdin) == nchannels){
		validate(xx, nchannels, sample, &prev_sample);
		++sample;
	}
	if (verbose){
		printf("%d/%d\n", ::errors, sample);
	}
	return errors? 1: 0;
}
