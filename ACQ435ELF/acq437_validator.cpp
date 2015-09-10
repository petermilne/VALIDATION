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
#define MAXWORDS	66

#define ES_MAGIC 	0xaa55f151
#define NES		4


bool verbose;

unsigned long long byte_count = 0;
int ecount;

class ACQ437_Data {

protected:
	static bool monitor_spad;

	const char* def;
	const int site;
	bool spad_enabled;
	bool pmod_present;
	int nwords;
	unsigned* ids;
	unsigned* spad_cache;
	unsigned offset;
	unsigned sample;

	enum IDS {
		IDS_NOCHECK = 0,
		IDS_SAMPLE = -1,
		IDS_SPAD   = -2,
		IDS_PMOD   = -3,
	};

	unsigned cid(int ic){
		return ic;
	}

	ACQ437_Data(const char* _def, int _site) :
				def(_def), site(_site),
				nwords(16)
	{

		unsigned sid = site << 5;


		ids = new unsigned[nwords];

		if (spad_enabled && monitor_spad){
			printf("MONITOR_SPAD: enabled\n");
			spad_cache = new unsigned[nwords];
		}

		bool pmod_done = false;
		bool sample_done = false;

		for (int ic = 0; ic < nwords; ++ic){
			ids[ic] = sid | cid(ic);
		}
	}
public:
	void print() {
		printf("ACQ437_Data site:%d spad: %s\n",
				site, spad_enabled? "SPAD ENABLED": "");
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
						printf("sample:%lld\n", sample);
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

			if (verbose){
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
	static unsigned ID_MASK;

	static ACQ437_Data* create(const char* _def);
};
unsigned ACQ437_Data::ID_MASK;




bool ACQ437_Data::monitor_spad;

ACQ437_Data* ACQ437_Data::create(const char* _def){
	int site;
	char *bank_def;

	bool nosid = getenv("NOSID") != 0;

	int rc;
	static bool done_one;
#define RETERR	do { rc = __LINE__; goto parse_err; } while(0)

	if (!done_one){
		if (getenv("NOSID")){
			ID_MASK = 0x1f;
			fprintf(stderr, "ID_MASK set %02x\n", ID_MASK);
		}else{
			ID_MASK = 0xff;
		}

		if (getenv("MONITOR_SPAD") != 0){
			monitor_spad = atoi(getenv("MONITOR_SPAD"));
		}
		done_one = true;
	}

	site = atoi(_def);
	if (site < 1 || site > 6) RETERR;

	if (nosid){
		ID_MASK = 0x1f;
		fprintf(stderr, "ID_MASK set %02x\n", ID_MASK);
	}else{
		ID_MASK = 0xff;
	}
	return new ACQ437_Data(_def, site);

	parse_err:
	fprintf(stderr, "ERROR: line:%d USAGE: site", rc);
	return 0;
}



int main(int argc, char* argv[])
{
	std::vector<ACQ437_Data*> sites;
	for (int ii = 1; ii < argc; ++ii){
		ACQ437_Data* site = ACQ437_Data::create(argv[ii]);
		if (site){
			sites.push_back(site);
		}else{
			fprintf(stderr, "ERROR: failed to create site \"%s\"\n",
					argv[ii]);
			return -1;
		}
	}

	int sample_size = 0;
	for (int si = 0; si < sites.size(); ++si){
		ACQ437_Data* module = sites.at(si);
		module->setOffset(sample_size);
		module->print();
		sample_size += module->getNwords();
	}
	//ACQ435_Data::create(argv[ii])->print();

	unsigned* buf = new unsigned[sample_size];


	while(fread(buf, sizeof(unsigned), sample_size, stdin) == sample_size){
		for (int si = 0; si < sites.size(); ++si){
			ACQ437_Data* module = sites.at(si);
			if (!module->isValid(buf)){
				printf("ERROR at %lld site:%d\n",
						byte_count, si);
				++ecount;
			}
		}
		byte_count += sample_size * sizeof(unsigned);
	}

	printf("byte_count:%lld ecount:%d\n", byte_count, ecount);
}

