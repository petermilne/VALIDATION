/* ------------------------------------------------------------------------- *
 * acq435_validator.cpp  		                     	                    
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
#define MAXWORDS	32

class ACQ435_Data {

protected:
	const char* def;
	const int site;
	const char* banks;
	bool spad_enabled;
	const int nwords;
	unsigned* ids;
	char* actual_banks;
	unsigned offset;
	unsigned sample;

	enum IDS {
		IDS_NOCHECK = 0,
		IDS_SAMPLE = 0xff000000
	};
	int nbanks() {
		return strlen(banks);
	}
	unsigned cid(int ic, bool upper, int offset) {
		unsigned rc;
		if (upper){
			rc = MAXWORDS-offset-4 + ic%4;
		}else{
			rc = offset+ic%4;
		}
		//printf("cid %d %d %d => %02x\n", ic, upper, offset, rc);
		return rc;
	}
	unsigned cid(int ic){
		bool upper = ic >= nwords/2;
		int ib = ic/4;
		switch(actual_banks[ib]){
		case 'A':	return cid(ic, upper, 0);
		case 'B':	return cid(ic, upper, 4);
		case 'C':	return cid(ic, upper, 8);
		case 'D':	return cid(ic, upper,12);
		case 'S': 	return 0;
		default:
				assert(0);
		}
	}

	ACQ435_Data(const char* _def, int _site,
			const char* _banks, bool _spad_enabled) :
		def(_def), site(_site),
		banks(_banks), spad_enabled(_spad_enabled),
		nwords(strlen(banks)*8)
	{
		ids = new unsigned[nwords];
		unsigned sid = site << 5;
		int ispad = spad_enabled? nbanks()*8: 999;

		actual_banks = new char[2*nbanks()+1];

		// banks ABCD actual_banks ABCDDCBA
		for (int ib = 0; ib < nbanks(); ++ib){
			actual_banks[ib] = banks[ib];
			actual_banks[2*nbanks()-ib-1] = banks[ib];
		}
		actual_banks[2*nbanks()] = '\0';


		for (int ic = 0; ic < nwords; ++ic){
			if (ic >= ispad){
				ids[ic] = ic == ispad? IDS_SAMPLE: IDS_NOCHECK;
			}else{
				ids[ic] = sid | cid(ic);
			}
		}
	}
public:
	void print() {
		printf(
	"ACQ435_Data site:%d banks %s actual_banks %s spad: %s\n",
			site, banks, actual_banks, 
			spad_enabled? "SPAD ENABLED": "");
		for (int ii = 0; ii < nwords; ++ii){
			printf("%02x ", ids[ii]);
		}
		printf("\n");
	}
	const int getNwords() const {
		return nwords;
	}
	void setOffset(int _offset){
		offset = _offset;
	}
	virtual bool isValid(unsigned *data){
		unsigned *mydata = data+offset;
		int errors = 0;

		for (int ic = 0; ic < nwords; ++ic){
			switch(ids[ic]){
			case IDS_NOCHECK: break;
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
					++errors;
				}
				break;    // @@todo
			default:
				if ((mydata[ic]&ID_MASK) == (ids[ic]&ID_MASK)){
					break;
				}else{
					++errors;
					printf("ERROR at %d\n", ic);
				}
			}
		}
		return errors == 0;
	}
	static unsigned ID_MASK;

	static ACQ435_Data* create(const char* _def);
};
unsigned ACQ435_Data::ID_MASK;

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

public:
	bool always_valid;

	ACQ435_DataBitslice(BitCollector& _bc, const char* _def, int _site,
				const char* _banks, bool _spad_enabled,
				bool _always_valid) :
		ACQ435_Data(_def, _site, _banks, _spad_enabled),
		bc(_bc),
		last_time(0),
		always_valid(_always_valid)
	{}
	virtual bool isValid(unsigned *data){
		if (!ACQ435_Data::isValid(data)){
			return false;
		}
		BS new_bs;
		new_bs.d7 = bc.collect_bits(data, 7);
		new_bs.d6 = bc.collect_bits(data, 6);
		new_bs.d5 = bc.collect_bits(data, 5);

		if (always_valid){
			fprintf(stderr, "%08x %08x %08x\n",
					new_bs.d7, new_bs.d6, new_bs.d5);
			return true;
		}
		if (new_bs.d7 != bs.d7+1){
			fprintf(stderr, "d7 error 0x%08x 0x%08x\n",
					bs.d7, new_bs.d7);
			return false;
		}
		time_t now = time(0);
		if (now != last_time){
			fprintf(stderr, "Sample Count:%08x\n", new_bs.d7);
		}
		if (new_bs.d6 != bs.d6){
			fprintf(stderr, "d6 %08x => %08x\n", bs.d6, new_bs.d6);
		}
		if (new_bs.d5 != bs.d5){
			fprintf(stderr, "d5 %08x => %08x\n", bs.d5, new_bs.d5);
		}
		bs = new_bs;
		last_time = now;

		return true;
	}
};
ACQ435_Data* ACQ435_Data::create(const char* _def){
	int _site;
	bool _spad_enabled = false;
	char *bank_def;
	bool bitslice = getenv("BITSLICE_FRAME") != 0;

	bool nosid = bitslice || getenv("NOSID") != 0;

	int rc;
#define RETERR	do { rc = __LINE__; goto parse_err; } while(0)

	if (getenv("NOSID")){
		ID_MASK = 0x1f;
		fprintf(stderr, "ID_MASK set %02x\n", ID_MASK);
	}else{
		ID_MASK = 0xff;
	}
	if (sscanf(_def, "%d=%ms", &_site, &bank_def) != 2) RETERR;

	if (!(_site >= 0 && _site <= 6)) RETERR;
	for (int ii = 0; ii < strlen(bank_def); ++ii){
		switch(bank_def[ii]){
		case 'A':
		case 'B':
		case 'C':
		case 'D':
			break;
		case 'S':
			_spad_enabled = true;
			break;
		}
	}

	if (nosid){
		ID_MASK = 0x1f;
		fprintf(stderr, "ID_MASK set %02x\n", ID_MASK);
	}else{
		ID_MASK = 0xff;
	}
	if (bitslice){
		BitCollector *bc;
		if (strcmp(getenv("BITSLICE_FRAME"), "LSB") == 0){
			bc = new BitCollectorLsbFirst;
		}else{
			bc = new BitCollectorMsbFirst;
		}
		bool always_valid = false;
		if (getenv("BITSLICE_ALWAYS_VALID")){
			always_valid = atoi(getenv("BITSLICE_ALWAYS_VALID"));
		}

		return new ACQ435_DataBitslice(*bc,
				_def, _site, bank_def, _spad_enabled,
				always_valid);
	}else{
		return new ACQ435_Data(_def, _site, bank_def, _spad_enabled);
	}
parse_err:
	fprintf(stderr, "ERROR: line:%d USAGE: site=[ABCD][S]", rc);
	return 0;
}



int main(int argc, char* argv[])
{
	std::vector<ACQ435_Data*> sites;
	for (int ii = 1; ii < argc; ++ii){
		ACQ435_Data* site = ACQ435_Data::create(argv[ii]);
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
		ACQ435_Data* module = sites.at(si);
		module->setOffset(sample_size);
		module->print();
		sample_size += module->getNwords();
	}
	//ACQ435_Data::create(argv[ii])->print();

	unsigned* buf = new unsigned[sample_size];
	unsigned long long byte_count = 0;

	while(fread(buf, sizeof(unsigned), sample_size, stdin) == sample_size){
		for (int si = 0; si < sites.size(); ++si){
			ACQ435_Data* module = sites.at(si);
			if (!module->isValid(buf)){
				printf("ERROR at %lld site:%d\n",
						byte_count, si);
			}
		}
		byte_count += sample_size * sizeof(unsigned);
	}
}
