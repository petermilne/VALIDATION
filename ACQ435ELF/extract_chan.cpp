/*
 * extract_chan.cpp
 *
 *  Created on: 13 Sep 2015
 *      Author: pgm
 */


/* extract single channel from data set */

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

int NCHAN=96;
int NCOLS=2;		// =2 TWO col data, skip first



int extract_chan1(int chan, FILE* fpin, FILE* fpout, int* buf)
{
	while(fread(buf, sizeof(int), NCHAN, fpin) == NCHAN){
		fwrite(&buf[chan-1], sizeof(int), 1, fpout);
	}
}
int extract_chan2(int chan, FILE* fpin, FILE* fpout, int* buf)
{
	while(fread(buf, sizeof(int), NCHAN*NCOLS, fpin) == NCHAN*NCOLS){
		fwrite(&buf[(chan-1)*NCOLS+1], sizeof(int), 1, fpout);
	}
}

int extract_chan(int chan, char* fromfile, char* tofile)
{
	FILE* fpin = fopen(fromfile, "r");
	assert(chan >= 1 && chan <= NCHAN);
	if (fpin == 0){
		perror(fromfile); return 1;
	}
	FILE* fpout = fopen(tofile, "w");
	if (fpout == 0){
		perror(tofile); return 1;
	}
	switch(NCOLS){
	case 2:
		return extract_chan2(chan, fpin, fpout, new int[NCHAN*NCOLS]);
	case 1:
		return extract_chan1(chan, fpin, fpout, new int[NCHAN]);
	default:
		fprintf(stderr, "case NCOLS=%d NOT SUPPORTED\n", NCOLS);
	}

}

int main(int argc, char* argv[])
{
	int chan;

	if (getenv("NCHAN")) NCHAN = atoi(getenv("NCHAN"));
	if (getenv("NCOLS")) NCOLS = atoi(getenv("NCOLS"));
	if (argc == 4){
		return extract_chan(atoi(argv[1]), argv[2], argv[3]);
	}else{
		fprintf(stderr, "USAGE: extract_chan CH from-file to-file\n");
		return 1;
	}
}




