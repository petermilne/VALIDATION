/* ------------------------------------------------------------------------- *
 * bsplit.c  		                     	                    
 * ------------------------------------------------------------------------- *
 *   Copyright (C) 2014 Peter Milne, D-TACQ Solutions Ltd                
 *                      <peter dot milne at D hyphen TACQ dot com>          
 *                         www.d-tacq.com
 *   Created on: 29 Mar 2014  
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

/**
 * @file bsplit.c split a data file into columns
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>

#include <popt.h>
#include <vector>
#include <time.h>

#define USE_STDIN	"-"

using namespace std;

namespace UI {
	const char** fnames;
	int verbose;
	int wordsize = 4;
	int nfields = 32;
};

class BSplitter {
protected:
	const int record_len;
	vector<int> fields;

	BSplitter(int _record_len) : record_len(_record_len) {
		/* default fields: all of them, starting at 1 */
		for (int ii = 1; ii <= record_len; ++ii){
			fields.push_back(ii);
		}
	}
	int offset(int ii){
		return fields[ii] - 1;
	}
public:
	virtual ~BSplitter() {}

	static BSplitter* create(int wordsize, int _record_len);

	void setFields(vector<int>* _fields){
		fields = *_fields;
	}

	virtual int split(const char* fn) = 0;
};


template <class T>
class BSplitterImpl : public BSplitter {
	FILE* fp_in;
	vector<FILE*> fp_out;
	bool using_stdin;
	T* buf;

	void onSplit01(const char* fn){
		using_stdin = strcmp(fn, USE_STDIN) == 0;
		fp_in = using_stdin? stdin: fopen(fn, "r");
		if (fp_in == 0){
			perror(fn);
			exit(1);
		}
		const char* of_root = using_stdin? "bsplit": fn;

		for (int ii = 0; ii < fields.size(); ++ii){
			char* ofn = new char[80];
			snprintf(ofn, 80, "%s.%03d", of_root, fields[ii]);
			FILE *fp = fopen(ofn, "w");
			if (fp == 0){
				perror(ofn);
				exit(1);
			}
			fp_out.push_back(fp);
		}
	}
	void onSplit99(){
		for (int ii = 0; ii < fields.size(); ++ii){
			fclose(fp_out[ii]);
		}
		if (!using_stdin){
			fclose(fp_in);
		}
	}
	void onSplitMain() {
		while(fread(buf, sizeof(T), record_len, fp_in) == record_len){
			for (int ii = 0; ii < fields.size(); ++ii){
				fwrite(&buf[offset(ii)], sizeof(T), 1, fp_out[ii]);
			}
		}
	}
public:
	BSplitterImpl(int _record_len) : BSplitter(_record_len),
		using_stdin(false),
		buf(new T[_record_len])
	{}
	virtual ~BSplitterImpl() {
		delete [] buf;
	}

	virtual int split(const char* fn) {
		onSplit01(fn);
		onSplitMain();
		onSplit99();
	}
};

BSplitter* BSplitter::create(int wordsize, int _record_len)
{
	switch(wordsize){
	case sizeof(int):
		return new BSplitterImpl<int>(_record_len);
	case sizeof(short):
		return new BSplitterImpl<short>(_record_len);
	default:
		fprintf(stderr, "ERROR: wordsize %d not supported\n", wordsize);
		exit(1);
	}
	return 0;
}
struct poptOption opt_table[] = {
	{ "verbose", 'v', POPT_ARG_INT, &UI::verbose, 0,
			"verbose" 				},
	{ "nfields", 'n', POPT_ARG_INT, &UI::nfields, 0,
			"number of fields in record" 		},
	{ "wordsize", 's', POPT_ARG_INT, &UI::wordsize, 0,
			"size of sample word"			},
	POPT_AUTOHELP
	POPT_TABLEEND
};
void ui(int argc, const char* argv[])
{
	poptContext opt_context =
			poptGetContext(argv[0], argc, argv, opt_table, 0);

	int rc;
	while ( (rc = poptGetNextOpt( opt_context )) > 0 ){
		switch(rc){
		default:
			;
		}
	}
	UI::fnames = poptGetArgs(opt_context);
}


int main(int argc, const char* argv[])
{
	ui(argc, argv);
	BSplitter* sp = BSplitter::create(UI::wordsize, UI::nfields);

	int fnum;
	for (fnum = 0; UI::fnames != NULL && UI::fnames[fnum] != NULL; ++fnum){
		sp->split(UI::fnames[fnum]);
	}
	if (fnum == 0){
		sp->split(USE_STDIN);
	}

	return 0;
}
