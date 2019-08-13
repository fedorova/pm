#include <sys/types.h>

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "nano_time.h"

#define DEFAULT_BLOCK_SIZE 4096

char filename[] = "/mnt/pm/file";

uint64_t do_read_syscall_test(int fd, size_t block_size);

int main(int argc, char **argv) {

	char *fname;
	int fd;
	size_t block_size = DEFAULT_BLOCK_SIZE;

	if (argc > 1)
		fname = argv[1];
	else
		fname = (char*) filename;


	fd = open((const char*)fname, O_RDWR | O_CREAT);
	if (fd < 0) {
		printf("Could not open file %s: %s\n", fname, strerror(errno));
		_exit(-1);
	}

	do_read_syscall_test(fd, block_size);
	//do_read_mmap_test(fd);

}

/**
 *
 *
 */
uint64_t
do_read_syscall_test(int fd, size_t block_size) {

	bool done = false;
	char *buffer = NULL;
	size_t total_bytes_read = 0;
	uint64_t begin_time, end_time, ret_token = 0;

	buffer = (char*)malloc(block_size);
	if (buffer == NULL) {
		printf("Failed to allocate a buffer: %s\n", strerror(errno));
		return -1;
	}

	begin_time = nano_time();

	while (!done) {

		size_t bytes_read = read(fd, buffer, block_size);
		if (bytes_read == 0)
			done = true;
		if (bytes_read == -1) {
			printf("Failed to read: %s\n", strerror(errno));
			return -1;
		}
		else {
			total_bytes_read +=  bytes_read;

			/* Pretend that we actually use the data */
			ret_token += buffer[0];
		}
	}

	end_time = nano_time();

	printf("read_syscall: %" PRIu64 " bytes read in %" PRIu64 " ns.\n",
	       (uint_least64_t)total_bytes_read, (end_time-begin_time));
	printf("\t %.2f bytes/second\n",
	       (double)total_bytes_read/(double)(end_time-begin_time));

	return ret_token;
}
