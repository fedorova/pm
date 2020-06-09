CC = gcc
CXXFLAGS=-g -D_GNU_SOURCE
LDDFLAGS=-lpthread

SRC_FILES := $(wildcard *.c)
OBJ_FILES := $(SRC_FILES:.c=.o)


.PHONY: all clean

all: fa memcopy

fa: file_access.o nano_time.o
	$(CC) -o  $@ $^ ${LDDFLAGS}

memcopy: memcopy.c nano_time.o
	$(CC) -o  $@ $^ ${LDDFLAGS} -lmemkind

%.o : %.c
	$(CC) $(CXXFLAGS) -c -o $@ $<

clean:
	rm *.o fa

