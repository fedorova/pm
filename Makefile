CC = gcc
CXXFLAGS=-g -D_GNU_SOURCE
#LDDFLAGS=-lpthread
LDDFLAGS=-lpthread -lmemkind

SRC_FILES := $(wildcard *.c)
OBJ_FILES := $(SRC_FILES:.c=.o)

# 	$(CC) -o  $@ $^ ${LDDFLAGS} -I ${HOME}/PMDK/memkind-1.10.1/jemalloc/include -L ${HOME}/PMDK/memkind-1.10.1/jemalloc/lib -ljemalloc

.PHONY: all clean

all: fa ht hang

fa: file_access.o nano_time.o
	$(CC) -o  $@ $^ ${LDDFLAGS}

ht: hash_table.o nano_time.o
	$(CC) -o  $@ $^ ${LDDFLAGS}

memcopy: memcopy.c nano_time.o
	$(CC) -o  $@ $^ ${LDDFLAGS}

hang: madvise_hang_reproducer.c
	$(CC) -o  $@ $^ ${LDDFLAGS} -I ${HOME}/PMDK/memkind-1.10.1/jemalloc/include

%.o : %.c
	$(CC) $(CXXFLAGS) -c -o $@ $<

clean:
	rm *.o fa ht hang memcopy

