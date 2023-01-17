DIRS=bin

all: caduinfo cadupack caduunpack cadurandomise caduhead cadutail

caduinfo: src/caduinfo.cpp include/libcadu/cadu_constants.h
	g++ -static --std=c++20 -o bin/caduinfo -Wl,-rpath=/usr/local/lib -I ./include/ -g src/caduinfo.cpp -lfec

cadupack: src/cadupack.cpp include/libcadu/cadu_constants.h
	g++ -static --std=c++20 -o bin/cadupack -Wl,-rpath=/usr/local/lib -I ./include/ -I ../libccsds/include/ -I ../seqiter/include/ -g src/cadupack.cpp -lfec

caduunpack: src/caduunpack.cpp include/libcadu/cadu_constants.h
	g++ -static --std=c++20 -o bin/caduunpack -Wl,-rpath=/usr/local/lib -I ./include/ -g src/caduunpack.cpp -lfec

cadurandomise: src/cadurandomise.cpp include/libcadu/cadu_constants.h
	g++ -static --std=c++20 -o bin/cadurandomise -Wl,-rpath=/usr/local/lib -I ./include/ -g src/cadurandomise.cpp -lfec

caduhead: src/caduhead.cpp
	g++ -static --std=c++20 -o bin/caduhead -Wl,-rpath=/usr/local/lib -I ./include/ -g src/caduhead.cpp -lfec

cadutail: src/cadutail.cpp
	g++ -static --std=c++20 -o bin/cadutail -Wl,-rpath=/usr/local/lib -I ./include/ -g src/cadutail.cpp -lfec

.PHONY: install
install:
	install -Dm 755 -t /usr/local/include/libcadu/ ./include/libcadu/*
	install -D -m 755 bin/caduinfo /usr/local/bin/
	install -D -m 755 bin/cadupack /usr/local/bin/
	install -D -m 755 bin/caduunpack /usr/local/bin/
	install -D -m 755 bin/cadurandomise /usr/local/bin/
	install -D -m 755 bin/caduhead /usr/local/bin/
	install -D -m 755 bin/cadutail /usr/local/bin/

$(shell mkdir -p $(DIRS))