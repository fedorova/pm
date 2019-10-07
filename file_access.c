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

#define BYTES_IN_GB (1024 * 1024 * 1024)
#define DEFAULT_BLOCK_SIZE 4096
#define NANOSECONDS_IN_SECOND 1000000000

const char DEFAULT_FNAME[] = "/mnt/pmem/testfile";

uint64_t do_read_mmap_test(int fd, size_t block_size, size_t filesize,
			   off_t *offsets);
uint64_t do_read_syscall_test(int fd, size_t block_size, size_t filesize,
			      off_t *offsets);
uint64_t do_write_mmap_test(int fd, size_t block_size, size_t filesize,
			    int createfile, off_t *offsets);
uint64_t do_write_syscall_test(int fd, size_t block_size, size_t filesize,
			       off_t *offsets);
size_t   get_filesize(const char* filename);
void     print_help_message(const char* progname);

static int silent = 0;

/**
 * For certain tests, create a file prior to running them.
 * This command will give you a 4GB:
 *      $ dd < /dev/zero bs=1048576 count=4096 > testfile
 *
 * It will actually allocate space on disk.
 *
 * To clear caches, use the following command on Linux:
 *      # sync; echo 1 > /proc/sys/vm/drop_caches
 */


int main(int argc, char **argv) {

	char *fname = (char*) DEFAULT_FNAME;
	int c, fd, flags = O_RDWR, numblocks = 0, option_index;
	static int createfile = 0, randomaccess = 0,
		read_mmap = 0, read_syscall = 0,
		write_mmap = 0, write_syscall = 0;
	off_t *offsets = 0;
	size_t block_size = DEFAULT_BLOCK_SIZE, filesize, new_file_size = 0;
	uint64_t retval;

	mode_t mode = S_IRWXU | S_IRWXG;

	static struct option long_options[] =
        {
		/* These options set a flag. */
		{"createfile", no_argument,  &createfile, 1},
		{"randomaccess", no_argument,  &randomaccess, 1},
		{"readmmap", no_argument,   &read_mmap, 1},
		{"readsyscall", no_argument,  &read_syscall, 1},
		{"silent", no_argument,  &silent, 1},
		{"writemmap", no_argument,   &write_mmap, 1},
		{"writesyscall", no_argument,  &write_syscall, 1},
		/* These options take an argument. */
		{"block", required_argument, 0, 'b'},
		{"file", required_argument, 0, 'f'},
		{"help", no_argument, 0, 'h'},
		{"size", no_argument, 0, 's'},
		{0, 0, 0, 0}
	};

	/* Read long options */
	while (1) {
		c = getopt_long (argc, argv, "b:f:hs:",
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
		case 's':
			new_file_size = (size_t)(atoi(optarg)) * BYTES_IN_GB;
			break;
		default:
			break;
		}
	}

	/* If the user supplied the createfile flag, they want the test
	 * to assume that the file does not exist and create a new one.
	 * in that case, they also must supply the target file size.
	 * This option can only be supplied if we are doing write tests.
	 */
	if (createfile) {
		if (read_mmap || read_syscall) {
			printf("Invalid option: --createfile cannot be "
			       "supplied with --readmmap or -readsyscall "
			       "optons.\n");
			print_help_message(argv[0]);
			_exit(-1);
		}
		if (new_file_size == 0) {
			printf("If --createfile option is provided, you must "
			       "also provide the size of the new file as the "
			       "argument to -s option.\n");
			print_help_message(argv[0]);
			_exit(-1);
		}
		flags |= O_CREAT | O_APPEND;
	}

	if (!silent)
		printf("Using file %s\n", fname);

	if ((filesize = get_filesize(fname)) == -1) {
		if (read_mmap || read_syscall) {
			printf("Cannot obtain file size for %s: %s"
			       "File must exist prior to running read tests.\n",
			       fname, strerror(errno));
			_exit(-1);
		}
		else
			filesize = new_file_size;
	}

	fd = open((const char*)fname, flags, mode);
	if (fd < 0) {
		printf("Could not open/create file %s: %s\n",
		       fname, strerror(errno));
		_exit(-1);
	}

	if (block_size < 0 || block_size > filesize) {
		printf("Invalid block size: %" PRIu64 " for file of size "
		       "%" PRIu64 ". Block size must be greater than zero "
		       "and no greater than the file size.\n",
		       (uint_least64_t)block_size, (uint_least64_t)filesize);
		_exit(-1);
	}

	/*
	 * Generate random block numbers for random file access.
	 * Sequential for sequential access.
	 */
	numblocks = filesize / block_size;
	if (filesize % block_size > 0)
		numblocks++;

	offsets = (off_t *) malloc(numblocks * sizeof(off_t));
	if (offsets == 0) {
		printf("Failed to allocate memory: %s\n",
		       strerror(errno));
		_exit(-1);
	}
	for (int i = 0; i < numblocks; i++)
		if (randomaccess)
			offsets[i] = ((int)random() % numblocks) * block_size;
		else
			offsets[i] = i*block_size;

	if (read_mmap) {
		if (!silent)
			printf("Running readmmap test:\n");
		retval = do_read_mmap_test(fd, block_size, filesize, offsets);
		if (!silent)
			printf("\t Meaningless return token: %" PRIu64 "\n",
			       retval);
	}
	if (read_syscall) {
		if (!silent)
			printf("Running readsyscall test:\n");
		retval = do_read_syscall_test(fd, block_size, filesize,
					      offsets);
		if (!silent)
			printf("\t Meaningless return token: %" PRIu64 "\n",
			       retval);
	}
	if (write_mmap) {
		if (!silent)
			printf("Running writemmap test:\n");
		retval = do_write_mmap_test(fd, block_size, filesize,
					    createfile, offsets);
		if (!silent)
			printf("\t Meaningless return token: %" PRIu64 "\n",
			       retval);
	}
	if (write_syscall) {
		if (!silent)
			printf("Running writesyscall test:\n");
		retval = do_write_syscall_test(fd, block_size, filesize,
					       offsets);

		if (!silent)
			printf("\t Meaningless return token: %" PRIu64 "\n",
			       retval);
	}

	close(fd);

}


#define READ 1
#define WRITE 2

/**
 * SYSCALL TESTS
 *
 */
uint64_t
do_syscall_test(int fd, size_t block_size, size_t filesize, char optype,
		off_t *offsets);

uint64_t
do_read_syscall_test(int fd, size_t block_size, size_t filesize,
		     off_t *offsets) {

	return do_syscall_test(fd, block_size, filesize, READ, offsets);
}

uint64_t
do_write_syscall_test(int fd, size_t block_size, size_t filesize,
		      off_t *offsets) {

	return do_syscall_test(fd, block_size, filesize, WRITE, offsets);
}

uint64_t
do_syscall_test(int fd, size_t block_size, size_t filesize, char optype,
		 off_t *offsets) {

	bool done = false;
	char *buffer = NULL;
	int i = 0;
	size_t total_bytes_transferred = 0;
	uint64_t begin_time, end_time, ret_token = 0;

	buffer = (char*)malloc(block_size);
	if (buffer == NULL) {
		printf("Failed to allocate memory: %s\n", strerror(errno));
		return -1;
	}

	begin_time = nano_time();

	while (!done) {
		size_t bytes_transferred = 0;

		if (optype == READ)
			bytes_transferred = pread(fd, buffer,
						  block_size,
						  offsets[i++]);
		else if (optype == WRITE)
			bytes_transferred = pwrite(fd, buffer,
						   block_size,
						   offsets[i++]);
		if (bytes_transferred == 0)
			done = true;
		else if (bytes_transferred == -1) {
			printf("Failed to do I/O: %s\n", strerror(errno));
			return -1;
		}
		else {
			total_bytes_transferred +=  bytes_transferred;

			if (optype == WRITE &&
			    total_bytes_transferred == filesize)
				done = true;

			/* Pretend that we actually use the data */
			ret_token += buffer[0];
		}
		if (i*block_size >= filesize)
			done = true;
	}

	end_time = nano_time();

	if (!silent)
		printf("%s: %" PRIu64 " bytes transferred in %" PRIu64 ""
		       " ns.\n", (optype == READ)?"readsyscall":"writesyscall",
		       (uint_least64_t)total_bytes_transferred,
		       (end_time-begin_time));
	/* Print throughput in GB/second */
	printf("\t %.2f\n",
	       (double)total_bytes_transferred/(double)(end_time-begin_time)
	       * NANOSECONDS_IN_SECOND / BYTES_IN_GB);

	return ret_token;
}

/**
 * MMAP tests
 *
 */

uint64_t do_mmap_test(int fd, size_t block_size, size_t filesize, char optype,
		      int createfile, off_t *offsets);

uint64_t
do_read_mmap_test(int fd, size_t block_size, size_t filesize, off_t *offsets) {

	return do_mmap_test(fd, block_size, filesize, READ, 0, offsets);
}

uint64_t
do_write_mmap_test(int fd, size_t block_size, size_t filesize,
		   int createfile, off_t *offsets) {

	return do_mmap_test(fd, block_size, filesize, WRITE, createfile,
			    offsets);
}

uint64_t
do_mmap_test(int fd, size_t block_size, size_t filesize, char optype,
	     int createfile, off_t *offsets) {


	bool done = false;
	char *mmapped_buffer = NULL, *buffer = NULL;
	int i, j, numblocks, ret;
	uint64_t begin_time, end_time, ret_token = 0;

	if (createfile) {

		ret = ftruncate(fd, filesize);
		if (ret) {
			printf("Failed to truncate the file to size "
			       "%" PRIu64 " : %s\n",
			       (uint_least64_t)filesize, strerror(errno));
			return -1;
		}
	}

#ifdef __MACH__
	mmapped_buffer = (char *)mmap(NULL, filesize,
				      (optype==READ)?PROT_READ:PROT_WRITE,
				      MAP_PRIVATE, fd, 0);
#else /* Assumes Linux 2.6.23 or newer */
	mmapped_buffer = (char *)mmap(NULL, filesize,
				      (optype==READ)?PROT_READ:PROT_WRITE,
				      MAP_PRIVATE | MAP_POPULATE , fd, 0);
#endif
	if (mmapped_buffer == MAP_FAILED) {
		printf("Failed to mmap file of size %" PRIu64 " : %s\n",
		       (uint_least64_t)filesize, strerror(errno));
		return -1;
	}

	buffer = (char*)malloc(block_size);
	if (buffer == NULL) {
		printf("Failed to allocate memory: %s\n", strerror(errno));
		return -1;
	}
	memset((void*)buffer, 0, block_size);

	begin_time = nano_time();

	/* If we change the loop spec as follows:
	 * for (i = 0; i < filesize/block_size; i++)
	 * and then use 'i' to index the offset array
	 * sequential read throughput drops by 16x. 
	 * I don't understand why. But be careful 
	 * changing this loop.
	 */
	for (i = 0; i < filesize; i+=block_size) {
		off_t offset = offsets[i/block_size];

		if (optype == READ) {
			memcpy(buffer, &mmapped_buffer[offset],
			       block_size);
			ret_token += buffer[0];
		}
		else if (optype == WRITE) {
			memcpy(&mmapped_buffer[offset], buffer,
			       block_size);
			ret_token += mmapped_buffer[i];
		}
	}

	end_time = nano_time();

	if (!silent)
		printf("%s: %" PRIu64 " bytes read in %" PRIu64 " ns.\n",
		       (optype==READ)?"readmmap":"writemmap",
		       (uint_least64_t)filesize, (end_time-begin_time));
	/* Print throughput in GB/second */
	printf("\t %.2f\n",
	       (double)filesize/(double)(end_time-begin_time)
	       * NANOSECONDS_IN_SECOND / BYTES_IN_GB);

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
	if (retval)
		return -1;
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
	       "     For mmap tests, the size of the stride when iterating\n"
	       "     over the file.\n"
	       "     Defaults to %d.\n", DEFAULT_BLOCK_SIZE);
	printf("  -f, --file[=FILENAME]\n"
	       "     Perform all tests on this file (defaults to %s).\n",
	       DEFAULT_FNAME);
	printf("  --createfilel\n"
	       "     The test assumes that the file does not exist and will\n"
	       "     create it. This option is valid only with read tests.\n"
	       "     If using this option you must also supply the file size\n"
	       "     as the argument to -s option.\n");
	printf("  --readsyscall\n"
	       "     Perform a read test using system calls.\n");
	printf("  --readmmap\n"
	       "     Perform a read test using mmap.\n");
	printf("  --writesyscall\n"
	       "     Perform a write test using system calls.\n");
	printf("  --writemmap\n"
	       "     Perform a write test using mmap.\n");
}
