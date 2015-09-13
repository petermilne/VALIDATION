all: acq435_validator acq437_validator acq435_tschan extract_chan
acq435_tschan: acq435_tschan.o acq-util.o
	$(CXX) -o $@ $^

install: all
	sudo cp acq435_tschan extract_chan /usr/local/bin

