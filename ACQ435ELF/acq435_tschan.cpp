/* ------------------------------------------------------------------------- *
 * acq435_validator.cpp  		                     	             *
 * ------------------------------------------------------------------------- *
 *   Copyright (C) 2014 Peter Milne, D-TACQ Solutions Ltd                
 *                      <peter dot milne at D hyphen TACQ dot com>          
 *                         www.d-tacq.com
 *   Created on: 31 Jan 2014  
 *    Author: pgm                                                         
 *                                                                           *
 *  This program is free software; you can redistribute it and/or modify     *
 *  it under the terms of Version 2 of the GNU General Public License        *
 *  as published by the Free Software Foundation;                            *
 *                                                                           *
 *  This program is distributed in the hope that it will be useful,          *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of           *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            *
 *  GNU General Public License for more details.                             *
 *                                                                           *
 *  You should have received a copy of the GNU General Public License        *
 *  along with this program; if not, write to the Free Software              *
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.                */
/* ------------------------------------------------------------------------- */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <vector>
#include <time.h>

#include "acq-util.h"

#define MAXCHAN		192
#define MAXWORDS	66

#define ES_MAGIC 	0xaa55f151
#define NES		4

/**
 * Bank def :
 * ABCD : up to 4 banks of 8 channels
 * S    : scratchpad SPAD : up to 8 LW of status
 * P    : PMOD
 * lm   : bitslice LSB, MSB
 */

int verbose;

unsigned long long byte_count = 0;
class ACQ435_Data {

protected:
	static bool monitor_spad;

	const char* def;
	const int site;
	const char* banks;
	bool spad_enabled;
	bool pmod_present;
	int nwords;
	unsigned* ids;
	unsigned* spad_cache;
	int nbanks;
	char* actual_banks;
	unsigned offset;
	unsigned sample;

	enum IDS {
		IDS_NOCHECK = 0,
		IDS_SAMPLE = -1,
		IDS_SPAD   = -2,
		IDS_PMOD   = -3,
	};
	unsigned cid(int ic, bool upper, int offset) {
		unsigned rc;
		if (upper){
			rc = nbanks*8-offset-4 + ic%4;
		}else{
			rc = offset+ic%4;
		}
		//printf("cid %d %d %d => %02x\n", ic, upper, offset, rc);
		return rc;
	}
	unsigned cid(int ic){
		bool upper = ic >= nwords/2;
		int ib = ic/4;

		if (verbose > 1){
			printf("cid(%d) ib:%d switch(%c) %s\n",
				ic, ib, actual_banks[ib], actual_banks);
		}

		switch(actual_banks[ib]){
		case 'A':	return cid(ic, upper, 0);
		case 'B':	return cid(ic, upper, 4);
		case 'C':	return cid(ic, upper, 8);
		case 'D':	return cid(ic, upper,12);
		case 'S': 	
		case 'P':
		case 'l':
		case 'm':
			return 0;
		default:
			assert(0);
		}
	}
	bool bank_mask[256];   // index by ascii value


	ACQ435_Data(const char* _def, int _site,
			const char* _banks, unsigned id_mask) :
				def(_def), site(_site),
				banks(_banks),
				nwords(0), nbanks(0),
				ID_MASK(id_mask)
	{
		memset(bank_mask, 0, sizeof(bank_mask));


		for (int ii = 0; ii < strlen(banks); ++ii){
			bank_mask[banks[ii]] = 1;
			switch(banks[ii]){
			case 'A':
			case 'B':
			case 'C':
			case 'D':
				nwords += 8;
				++nbanks;
				break;
			case 'S':
				nwords += 8;
				spad_enabled = true;
			case 'P':
				nwords += 1;
				pmod_present = true;
				break;
			case 'l':
			case 'm':
				// bitslice opts: ignore
				break;
			default:
				fprintf(stderr, "ERROR invalid bank %c\n", banks[ii]);
				exit(-1);
			}
		}
		unsigned sid = site << 5;
		actual_banks = new char[nbanks*2+2+1];
		int ib = 0;

		// banks ABCD actual_banks ABCDDCBA
		for (; ib < nbanks; ++ib){
			actual_banks[ib] = banks[ib];
			actual_banks[2*nbanks-ib-1] = banks[ib];
		}
		ib = nbanks * 2;

		if (pmod_present) actual_banks[ib++] = 'P';
		if (spad_enabled) actual_banks[ib++] = 'S';
		actual_banks[ib] = '\0';


		ids = new unsigned[nwords];

		if (spad_enabled && monitor_spad){
			printf("MONITOR_SPAD: enabled\n");
			spad_cache = new unsigned[nwords];
		}

		bool pmod_done = false;
		bool sample_done = false;

		for (int ic = 0; ic < nwords; ++ic){
			if (ic < nbanks*8){
				ids[ic] = sid | cid(ic);
			}else{
				if (pmod_present && !pmod_done){
					ids[ic] = IDS_NOCHECK;
					pmod_done = true;
				}else if (spad_enabled && !sample_done){
					ids[ic] = IDS_SAMPLE;
					sample_done = true;
				}else{
					ids[ic] = monitor_spad? IDS_SPAD: IDS_NOCHECK;
				}
			}
		}
	}
public:
	virtual void print() {
		printf(
				"ACQ435_Data site:%d banks %s actual_banks %s spad: %s\n",
				site, banks, actual_banks,
				spad_enabled? "SPAD ENABLED": "");
		printf("nwords:%d\n", nwords);
		for (int ii = 0; ii < nwords; ++ii){
			printf("%02x%c", ids[ii], ii%16==15? '\n': ' ');
		}
		printf("\n");
	}
	const int getNwords() const {
		return nwords;
	}
	void setOffset(int _offset){
		offset = _offset;
	}
	bool isES(unsigned *data){
		for (int ii = 0; ii < NES; ++ii){
			if (data[ii] != ES_MAGIC ){
				return false;
			}
		}
		printf("%16lld ES detected %08x at 0x%08x, %d\n",
				byte_count, data[0], data[NES], data[NES]);
	}
	virtual bool isValid(unsigned *data){
		unsigned *mydata = data+offset;
		int errors = 0;
		bool print_spad = false;

		if (isES(data)){
			return true;
		}
		for (int ic = 0; ic < nwords; ++ic){
			bool this_error = false;


			switch(ids[ic]){
			case IDS_NOCHECK:
				break;
			case IDS_SPAD:
				if (mydata[ic] != spad_cache[ic]){
					print_spad = true;
					spad_cache[ic] = mydata[ic];
				}
				break;
			case IDS_SAMPLE:
				if (mydata[ic] == sample+1){
					++sample;
					if (sample%100000 == 0){
						printf("sample:%u\n", sample);
					}
					break;
				}else{
					printf("SEQ error wanted %08x got %08x\n",
							mydata[ic], sample+1);
					this_error = true;
					++errors;
				}
				break;    // @@todo
			default:
				if ((mydata[ic]&ID_MASK) == (ids[ic]&ID_MASK)){
					break;
				}else{
					++errors;
					this_error = true;
					//printf("%8lld ERROR at %d %08x ^ %08x\n", byte_count, ic, mydata[ic], ids[ic]);
				}
			}

			if (verbose > 1){
				printf("%8lld [%2d] %08x %08x  %s\n",
					byte_count, ic, ids[ic], mydata[ic],
					this_error? "ERROR": "OK");
			}
		}

		if (print_spad){
			for (int ic = 0; ic < nwords; ++ic){
				switch(ids[ic]){
				case IDS_SPAD:
				case IDS_SAMPLE:
					printf("%08x ", mydata[ic]);
					break;
				default:
					;
				}
			}
			printf("\n");
		}

		return errors == 0;
	}
	unsigned ID_MASK;

	static ACQ435_Data* create(const char* _def);
};

class BitCollector {
public:
	virtual unsigned collect_bits(unsigned *data, int bit) = 0;
	BitCollector(const char* _name) {
		fprintf(stderr, "%s\n", _name);
	}
};

class BitCollectorLsbFirst : public BitCollector {
public:
	virtual unsigned collect_bits(unsigned *data, int bit){
		unsigned xx = 0;
		unsigned bmask = 1<<bit;
		for (unsigned cursor = 0x1; cursor; cursor <<= 1){
			if (*data++ & bmask){
				xx |= cursor;
			}
		}

		return xx;
	}
	BitCollectorLsbFirst() : BitCollector("BitCollectorLsbFirst") {}
};

class BitCollectorMsbFirst : public BitCollector {
public:
	virtual unsigned collect_bits(unsigned *data, int bit){
		unsigned xx = 0;
		unsigned bmask = 1<<bit;
		for (unsigned cursor = 1<<31; cursor; cursor >>= 1){
			if (*data++ & bmask){
				xx |= cursor;
			}
		}

		return xx;
	}
	BitCollectorMsbFirst() : BitCollector("BitCollectorMsbFirst") {}
};

class ACQ435_DataBitslice : public ACQ435_Data {
	struct BS {
		unsigned d7, d6, d5;
		BS(): d7(0), d6(0), d5(0)
		{}
	} bs;
	time_t last_time;
	BitCollector& bc;
	bool first_sample;

public:
	bool always_valid;
	static unsigned long sample_count;

	ACQ435_DataBitslice(BitCollector& _bc, const char* _def, int _site,
			const char* _banks, bool _always_valid) :
				ACQ435_Data(_def, _site, _banks, 0x1f),
				bc(_bc),
				last_time(0),
				always_valid(_always_valid),
				first_sample(true)
	{}
	virtual bool isValid(unsigned *data){
		bool allGood = true;
		if (isES(data)){
			return true;
		}else if (!ACQ435_Data::isValid(data)){
			return false;
		}

		BS new_bs;
		new_bs.d7 = bc.collect_bits(data, 7);
		new_bs.d6 = bc.collect_bits(data, 6);
		new_bs.d5 = bc.collect_bits(data, 5);
		time_t now = time(0);

		if (new_bs.d7 == 0 && verbose){
			fprintf(stderr, "isValid new_bs=0 bs:%08x\n", bs.d7);
		}


#if 0
		if (first_sample){
			bs = new_bs;
			first_sample = false;
		}else{
			if (always_valid){
				fprintf(stderr, "%08x %08x %08x\n",
					new_bs.d7, new_bs.d6, new_bs.d5);
			}else if (new_bs.d7 != bs.d7+1){
				fprintf(stderr, "d7 error old 0x%08x new 0x%08x\n",
					bs.d7, new_bs.d7);
				return allGood = false;
			}else{
				if (now != last_time){
					fprintf(stderr, "Sample Count:%08x\n", new_bs.d7);
				}
				if (new_bs.d6 != bs.d6){
					printf("%16lld sc %d d6 %08x => %08x\n",
							byte_count, bs.d7, bs.d6, new_bs.d6);
				}
				if (new_bs.d5 != bs.d5){
					printf("%16lld sc %d d5 %08x => %08x\n",
						byte_count, bs.d7, bs.d5, new_bs.d5);
				}
				bs = new_bs;
			}
		}

		sample_count = bs.d7;
#else
		sample_count = new_bs.d7;
#endif
		last_time = now;

		return allGood;
	}
	virtual void print() {
		printf("Bitslice Frame:");
		ACQ435_Data::print();
	}
};
unsigned long ACQ435_DataBitslice::sample_count;

enum BITSLICE {
	BS_NONE,
	BS_LSB_FIRST = 'l',
	BS_MSB_FIRST = 'm'
};

enum BITSLICE isBitSlice(const char* _def)
{
	if (strchr(_def, BS_LSB_FIRST) != 0){
		return BS_LSB_FIRST;
	}else if (strchr(_def, BS_MSB_FIRST) != 0){
		return BS_MSB_FIRST;
	}else{
		const char* bsf = getenv("BITSLICE_FRAME");
		if (bsf != 0){
			if (strcmp(bsf, "LSB") == 0){
				return BS_LSB_FIRST;
			}
			if (strcmp(bsf, "MSB") == 0){
				return BS_MSB_FIRST;
			}
		}
	}
	return BS_NONE;
}
bool ACQ435_Data::monitor_spad;
ACQ435_Data* ACQ435_Data::create(const char* _def){
	int _site;
	char *bank_def;
	enum BITSLICE bitslice = isBitSlice(_def );
	bool nosid = bitslice != BS_NONE || getenv("NOSID") != 0;

	int rc;

#define RETERR	do { rc = __LINE__; goto parse_err; } while(0)

	if (sscanf(_def, "%d=%ms", &_site, &bank_def) != 2) RETERR;

	if (!(_site >= 0 && _site <= 6)) RETERR;


	if (bitslice != BS_NONE){
		BitCollector *bc;
		if (bitslice = BS_LSB_FIRST){
			bc = new BitCollectorLsbFirst;
		}else{
			bc = new BitCollectorMsbFirst;
		}
		bool always_valid = false;
		if (getenv("BITSLICE_ALWAYS_VALID")){
			always_valid = atoi(getenv("BITSLICE_ALWAYS_VALID"));
		}

		return new ACQ435_DataBitslice(*bc,
				_def, _site, bank_def, always_valid);
	}else{
		return new ACQ435_Data(_def, _site, bank_def, getenv("NOSID")? 0x1f: 0xff);
	}
	parse_err:
	fprintf(stderr, "ERROR: line:%d USAGE: site=[ABCD][S]", rc);
	return 0;
}


class ChannelMask {
	int *mask;
public:
	ChannelMask() {
		mask = new int[MAXCHAN+1];

		for (int ic = 1; ic < MAXCHAN+1; ++ic){
			mask[ic] = 1;	// default : enable all
		}
	}
	void makeMask(const char* mask_def) {
		for (int ic = 1; ic < MAXCHAN+1; ++ic){
			mask[ic] = 0;
		}
		acqMakeChannelRange(mask, MAXCHAN, mask_def);
	}

	bool operator() (int chan){
		return mask[chan];
	}
};

namespace UI {
	ChannelMask cmask;
	unsigned long maxsamples = 0;
	FILE* fout = 0;
	bool filenames_on_stdin = false;
	int two_column = 1;
};

class FileProcessor {
	/* take input file, validate and output. default output is raw */
	unsigned* buf;
	unsigned long sample_count;
	std::vector<ACQ435_Data*> sites;
protected:
	int sample_size;
	int buffer_count;

protected:
	virtual int actOnValidData(unsigned buf[], FILE* fout){
		fwrite(buf, sizeof(unsigned), sample_size, fout);
	}
public:
	FileProcessor():
		buf(0), sample_count(0), sample_size(0), buffer_count(0) {
	}

	void addModule(const char* def) {
		ACQ435_Data* module = ACQ435_Data::create(def);
		if (module){
			module->setOffset(sample_size);
			module->print();
			sample_size += module->getNwords();
			sites.push_back(module);
		}else{
			fprintf(stderr, "ERROR: failed to create site \"%s\"\n",
						def);
			exit(1);
		}
	}

	virtual int operator() (FILE* fin, FILE* fout) {

		if (!buf) buf = new unsigned[sample_size];
		unsigned samples_file = 0;

		while(fread(buf, sizeof(unsigned), sample_size, fin) == sample_size){
			for (int si = 0; si < sites.size(); ++si){
				ACQ435_Data* module = sites.at(si);
				if (!module->isValid(buf)){
					printf("ERROR at %lld site:%d offset:%d samples\n",
					byte_count, si, samples_file);
					return -1;
				}
			}

			if (fout) actOnValidData(buf, fout);

			byte_count += sample_size * sizeof(unsigned);
			++sample_count;
			++samples_file;
			if (UI::maxsamples && sample_count > UI::maxsamples){
				return 1;
			}
		}
		return 0;
	}

	static FileProcessor& instance();
};


class FileProcessorTwoColumn: public FileProcessor {

protected:
	virtual int actOnValidData(unsigned buf[], FILE* fout){
		unsigned sc = ACQ435_DataBitslice::sample_count;

		for (int iw = 0; iw != sample_size; ++iw){
			if (UI::cmask(iw+1)){
				fwrite(&sc,    sizeof(unsigned), 1, fout);
				fwrite(buf+iw, sizeof(unsigned), 1, fout);
			}
		}
	}
public:
FileProcessorTwoColumn() {}
};


FileProcessor& FileProcessor::instance()
{
	static FileProcessor* _instance;

	if (!_instance){
		if (UI::two_column){
			_instance = new FileProcessorTwoColumn;
		}else{
			_instance = new FileProcessor;
		}
	}
	return *_instance;
}
void ui(int argc, char* argv[])
{
	for (int ii = 1; ii < argc; ++ii){
		const char* this_arg = argv[ii];
		char fname[128];
		char mask_def[128];
		printf("this arg:%s\n", this_arg);
		if (sscanf(this_arg, "--outfile=%s", fname) == 1){
			UI::fout = fopen(fname, "w");
			if (!UI::fout){
				perror(fname);
			}
		}else if (sscanf(this_arg, "--two_column=%d", &UI::two_column) == 1){
			;
		}else if (sscanf(this_arg, "--mask=%s", mask_def) == 1){
			UI::cmask.makeMask(mask_def);
		}else if (sscanf(this_arg, "--maxsamples=%lu", &UI::maxsamples) == 1){
			;
		}else if (strcmp(this_arg, "--filenames") == 0){
			UI::filenames_on_stdin = true;
		}else{
			FileProcessor::instance().addModule(this_arg);
		}
	}
}

void process_filenames_stdin(FileProcessor& fp)
{
	char fname[80];
	int consecutive_errors = 0;

	while(fgets(fname, 80, stdin)){
		char* p_end;
		if ((p_end = strchr(fname, '\n'))){
			*p_end = '\0';
		}
	open_file:
		FILE* fpin = fopen(fname, "r");
		if (fpin == 0){
			perror(fname);
			exit(1);
		}
		if (verbose){
			fprintf(stderr, "process file %s\n", fname);
		}
		int rc = fp(fpin, UI::fout);
		if (rc > 0){
			fprintf(stderr, "JOB COMPLETE\n");
			return;
		}else if (rc < 0){
			char test[128];
			sprintf(test, "sha1sum %s", fname);
			if (consecutive_errors++ < 1){
				system(test);
				fprintf(stderr, "ERROR in file %s retry\n", fname);
				fclose(fpin);
				goto open_file;
			}else{
				system(test);
				fprintf(stderr, "ERROR in file %s FAILED second chance\n", fname);
				exit(1);
			}
		}else{
			consecutive_errors = 0;
		}
		fclose(fpin);
	}
}
int main(int argc, char* argv[])
{
	if (getenv("VERBOSE")){
		verbose = atoi(getenv("VERBOSE"));
	}

	ui(argc, argv);

	if (UI::filenames_on_stdin){
		process_filenames_stdin(FileProcessor::instance());
	}else{
		FileProcessor::instance()(stdin, UI::fout);
	}
}

