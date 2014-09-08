CC=g++

LFLAGS=-std=c++11 -pthread
CFLAGS=-c -Wall $(LFLAGS)

all: file_cache_impl

file_cache_impl: main.o file_cache_impl.o 
	$(CC) main.o file_cache_impl.o -o file_cache_impl $(LFLAGS)

main.o: main.cc
	$(CC) $(CFLAGS) main.cc

file_cache_impl.o: file_cache_impl.cc
	$(CC) $(CFLAGS) file_cache_impl.cc

clean:
	rm -rf *o file1 file2 file3 file4 file_cache_impl
