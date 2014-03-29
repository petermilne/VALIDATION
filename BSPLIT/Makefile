# Makefile for BSPLIT


DC=$(shell date +%y%m%d%H%M%S)

#CXXFLAGS=-g

APPS := bsplit

all: apps
	
date:
	echo $(DC)



apps: $(APPS)


clean:
	@rm -rf *.o $(APPS)
	
bsplit: bsplit.o
	$(CXX) $(CXXFLAGS) -o bsplit bsplit.o -L../lib -lpopt
	
