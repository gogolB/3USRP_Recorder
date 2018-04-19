cc=gcc
cpp=g++

all:
	$(cpp) -Wall -std=c++11  main.c -o datarec -luhd

clean:
	rm datarec
