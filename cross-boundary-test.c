#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

const char DEFAULT_FNAME[] = "/Users/sasha/testfile";

/*
void mmap_thread_func(void *) {



}

void syscall_thread_func(void *) {



}
*/
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
init_data(char *buffer, char c, size_t size) {

    for (size_t i = 0; i < size; i++)
	buffer[i] = c;
}

void
dump_buffer(char *buffer, size_t offset, size_t size) {

    printf("Dumping %p starting at %" PRIu64 " for %" PRIu64 " bytes.\n",
	   (void*)buffer,  (uint64_t)offset,  (uint64_t)size);
    for (size_t i = offset; i < (offset + size); i++) {
	printf("%c ", buffer[i]);
	if ((i - offset) % 64 == 63)
	    printf("\n");
    }
    printf("\n");
}

#define WRITTEN_DATA_SIZE 4096

void
simple_test(void) {

    char *fname = (char*) DEFAULT_FNAME;
    char *mapped_buffer = NULL;
    char written_data[WRITTEN_DATA_SIZE];
    int fd;
    size_t bytes_written, filesize, offset;

    init_data((char *)written_data, 'c', WRITTEN_DATA_SIZE);

    fd = open((const char*)fname, O_RDWR, S_IRWXU | S_IRWXG);
    if (fd < 0) {
	printf("Could not open/create file %s: %s\n",
		       fname, strerror(errno));
	_exit(-1);
    }

    if ((filesize = get_filesize(fname)) == -1) {
	printf("Cannot obtain file size for %s: %s"
	       "File must exist prior to running read tests.\n",
	       fname, strerror(errno));
	_exit(-1);
    }

    mapped_buffer = (char *)mmap(NULL, filesize,
				  PROT_READ | PROT_WRITE,
				  MAP_SHARED, fd, 0);

    /*
     * Compute the offset that will begin just before the
     * end of the file.
     */
    offset = filesize - WRITTEN_DATA_SIZE / 2;
    dump_buffer(mapped_buffer, offset,  WRITTEN_DATA_SIZE / 2);

    /*
     * Issue a write that starts in the mapped region and
     * extends beyond the mapped region.
     */
    printf("Will write to offset %" PRIu64 " in %s\n", (uint64_t)offset, fname);
    bytes_written = pwrite(fd, written_data, WRITTEN_DATA_SIZE, offset);
    if (bytes_written != WRITTEN_DATA_SIZE) {
	printf("Wrote %" PRIu64 ", expected %" PRIu64 "\n",
	       (uint64_t)bytes_written, (uint64_t)WRITTEN_DATA_SIZE);
	_exit(-1);
    }

    /* Now dump the buffer again so we can see the changes */
    dump_buffer(mapped_buffer, offset,  WRITTEN_DATA_SIZE / 2);
}

int main(int argc, char **argv) {

    int ret;
    pthread_t mmap_thread, syscall_thread;

    /*
    ret = pthread_create(&mmap_thread, NULL, mmap_thread_func, NULL);
    if (ret) {
	perror(NULL);
	_exit(-1);
    }

    ret = pthread_create(&syscall_thread, NULL, syscall_thread_func, NULL);
    if (ret) {
	perror(NULL);
	_exit(-1);
    }
    */

    simple_test();

    return (0);
}
