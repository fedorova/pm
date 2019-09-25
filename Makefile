CC = gcc
CXXFLAGS=-g

SRC_FILES := $(wildcard *.c)
OBJ_FILES := $(SRC_FILES:.c=.o)


.PHONY: all clean

all: fa

fa: file_access.o nano_time.o
	$(CC) -o  $@ $^

%.o : %.c
	$(CC) $(CXXFLAGS) -c -o $@ $<

clean:
	rm *.o fa

