CXX=g++

ifdef RISCV
CXX	:= /home/greghas/riscv/bin/riscv64-unknown-linux-gnu-g++
endif

#add/replace link flags
CXXLFLAGS = -std=c++17 -Wall -Wextra -Wconversion -Wshadow -pedantic -g -O3 
#add/replace compile flags
CXXFLAGS += $(CXXLFLAGS)
CXXFLAGS += -Wcast-qual


ifdef RISCV
CXXFLAGS += -march=rv64gcv -mabi=lp64d
endif


LDFLAGS = -lm

.PHONY: all 

all:test-main

test-main: test-main.o
	$(CXX) $(CXXFLAGS) -o $@ $+ $(LDFLAGS)

test-main.o: main.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<


.PHONY: clean

clean:
	rm -r test-main test-main.d *.o

 