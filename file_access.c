#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "nano_time.h"

#define DEFAULT_BLOCK_SIZE 4096

const char DEFAULT_FNAME[] = "/mnt/pmem/testfile";

uint64_t do_read_syscall_test(int fd, size_t block_size);
uint64_t do_read_mmap_test(int fd, size_t block_size, size_t filesize);
size_t   get_filesize(const char* filename);
void     print_help_message(const char* progname);

static int silent = 0;

/**
 * For read tests, create a 4GB file prior to running tests using this command:
 *      $ dd < /dev/zero bs=1048576 count=4096 > testfile
 *
 * It will actually allocate space on disk.
 *
 * To clear caches, use the following command on Linux:
 *      # sync; echo 1 > /proc/sys/vm/drop_caches
 */


int main(int argc, char **argv) {

	char *fname = (char*) DEFAULT_FNAME;
	int c, fd, option_index;
	static int read_mmap = 0, read_syscall = 0;
	size_t block_size = DEFAULT_BLOCK_SIZE, filesize;
	uint64_t retval;

	static struct option long_options[] =
        {
		/* These options set a flag. */
		{"readmmap", no_argument,   &read_mmap, 1},
		{"readsyscall", no_argument,  &read_syscall, 1},
		{"silent", no_argument,  &silent, 1},
		/* These options take an argument. */
		{"block", required_argument, 0, 'b'},
		{"file", required_argument, 0, 'f'},
		{"help", no_argument, 0, 'h'},
		{0, 0, 0, 0}
	};

	/* Read long options */
	while (1) {
		c = getopt_long (argc, argv, "b:f:h",
				 long_options, &option_index);

		/* Detect the end of the options. */
		if (c == -1)
			break;

		switch (c)
		{
		case 0:
			/* If this option set a flag, do nothing else now. */
			break;
		case 'b':
			block_size = atoi(optarg);
			break;
		case 'f':
			fname = optarg;
			break;
		case 'h':
			print_help_message(argv[0]);
			_exit(0);
		default:
			break;
		}
	}

	fd = open((const char*)fname, O_RDWR | O_CREAT);
	if (fd < 0) {
		printf("Could not open file %s: %s\n", fname, strerror(errno));
		_exit(-1);
	}
	else
		if (!silent)
			printf("Using file %s\n", fname);

	if ((filesize = get_filesize(fname)) == -1)
		_exit(-1);

	if (block_size < 0 || block_size > filesize) {
		printf("Invalid block size: %" PRIu64 " for file of size "
		       "%" PRIu64 ". Block size must be greater than zero "
		       "and no greater than the file size.\n",
		       (uint_least64_t)block_size, (uint_least64_t)filesize);
		_exit(-1);
	}

	if (read_syscall) {
		if (!silent)
			printf("Running readsyscall test:\n");
		retval = do_read_syscall_test(fd, block_size);
		if (!silent)
			printf("\t Meaningless return token: %" PRIu64 "\n",
			       retval);
	}
	if (read_mmap) {
		if (!silent)
			printf("Running readmmap test:\n");
		retval = do_read_mmap_test(fd, block_size, filesize);
		if (!silent)
			printf("\t Meaningless return token: %" PRIu64 "\n",
			       retval);
	}

	close(fd);

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

	if (!silent)
		printf("read_syscall: %" PRIu64 " bytes read in %" PRIu64 ""
		       "ns.\n", (uint_least64_t)total_bytes_read,
		       (end_time-begin_time));
	printf("\t %.2f bytes/second\n",
	       (double)total_bytes_read/(double)(end_time-begin_time));

	return ret_token;
}

/**
 *
 *
 */
uint64_t
do_read_mmap_test(int fd, size_t block_size, size_t filesize) {

	bool done = false;
	char *mmapped_buffer = NULL, *buffer = NULL;
	int i, ret;
	uint64_t begin_time, end_time, ret_token = 0;

#ifdef __MACH__
	mmapped_buffer = (char *)mmap(NULL, filesize, PROT_READ,
				      MAP_PRIVATE, fd, 0);
#else /* Assumes Linux 2.6.23 or newer */
	mmapped_buffer = (char *)mmap(NULL, filesize, PROT_READ,
				      MAP_PRIVATE | MAP_POPULATE, fd, 0);
#endif
	if (mmapped_buffer == MAP_FAILED) {
		printf("Failed to mmap file of size %" PRIu64 " : %s\n",
		       (uint_least64_t)filesize, strerror(errno));
		return -1;
	}

	buffer = (char*)malloc(block_size);
	if (buffer == NULL) {
		printf("Failed to allocate a buffer: %s\n", strerror(errno));
		return -1;
	}

	begin_time = nano_time();

	for (i = 0; i < filesize; i += block_size) {
		memcpy(buffer, &mmapped_buffer[i], block_size);
		ret_token += buffer[i];
	}

	end_time = nano_time();

	if (!silent)
		printf("read_mmap: %" PRIu64 " bytes read in %" PRIu64 " ns.\n",
		       (uint_least64_t)filesize, (end_time-begin_time));
	printf("\t %.2f bytes/second\n",
	       (double)filesize/(double)(end_time-begin_time));

	ret = munmap(mmapped_buffer, filesize);
	if (ret)
		printf("Error unmapping file: %s\n", strerror(errno));

	return ret_token;
}

size_t
get_filesize(const char* filename) {

	int retval;

	struct stat st;
	retval = stat(filename, &st);
	if (retval) {
		printf("Failed to find the size of file %s:"
		       "%s\n", filename, strerror(errno));
		return -1;
	}
	else
		return st.st_size;
}

void
print_help_message(const char *progname) {

	/* take only the last portion of the path */
	const char *basename = strrchr(progname, '/');
	basename = basename ? basename + 1 : progname;

	printf("usage: %s [OPTION]\n", basename);
	printf("  -h, --help\n"
	       "     Print this help and exit.\n");
	printf("  -b, --block[=BLOCKSIZE]\n"
	       "     Block size used for read system calls.\n"
	       "     For mmap tests, the size of the stride when iterating over the file.\n"
	       "     Defaults to %d.\n", DEFAULT_BLOCK_SIZE);
	printf("  -f, --file[=FILENAME]\n"
	       "     Perform all tests on this file (defaults to %s).\n",
	       DEFAULT_FNAME);
	printf("  --readsyscall\n"
	       "     Perform a read test using system calls.\n");
	printf("  --readmmap\n"
	       "     Perform a read test using mmap.\n");
}
