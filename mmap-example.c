#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <assert.h>
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

#define N_THREADS 1
#define DO_COPY 0
#define DO_MMAP 1
#define DO_READ 0

typedef struct {
	char** filenames;
	int numfiles;
} threadargs_t;

#define EXIT_MSG(...)                  \
    do {                                       \
        printf(__VA_ARGS__);           \
        _exit(-1);                 \
    } while (0)

void *do_mmap(void *args);

int
main(int argc, char **argv) {

	int i, files_per_thread, numfiles, ret;
    pthread_t *threads;
    threadargs_t *threadargs;

	/* We assume that all and only arguments we are given are file
	 * names.
	 */
	numfiles = argc - 1;
	files_per_thread = numfiles / N_THREADS;

	printf("Using %d threads and %d files\n", N_THREADS, numfiles);
	/*
	for (i = 1; i < numfiles; i++) {
		printf("File #%d is %s\n", i, argv[i]);
	}
	*/

	threads = (pthread_t*)malloc(N_THREADS * sizeof(pthread_t));
    threadargs =
        (threadargs_t*)malloc(N_THREADS * sizeof(threadargs_t));
    if (threads == NULL || threadargs == NULL)
        EXIT_MSG("Could not allocate thread array for %d threads.\n",
               N_THREADS);

	for (i = 0; i < N_THREADS; i++) {
		threadargs[i].filenames = &argv[1 + i * files_per_thread];
		threadargs[i].numfiles = files_per_thread;

		ret = pthread_create(&threads[i], NULL, do_mmap,
								 &threadargs[i]);
        if (ret != 0)
            EXIT_MSG("pthread_create for %dth thread failed: %s\n",
                   i, strerror(errno));
	}

	for (i = 0; i < N_THREADS; i++) {
        ret = pthread_join(threads[i], NULL);
        if (ret != 0)
            EXIT_MSG("Thread %d failed: %s\n", i, strerror(ret));
    }
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

void *
do_mmap(void *args) {

	int fd, i;
	threadargs_t t = *(threadargs_t*)args;
	void **dest_buffers = malloc((t.numfiles) * sizeof(void*));
	int *file_sizes = malloc(t.numfiles * sizeof(int));
	void **mapped_buffers = malloc((t.numfiles) * sizeof(void*));

	if (dest_buffers == NULL || file_sizes == NULL || mapped_buffers == NULL)
		EXIT_MSG("Could not allocate memory for destination buffers "
				 "file sizes, or mapped buffers\n");

	for (i = 0; i < t.numfiles; i++) {
		file_sizes[i] = get_filesize(t.filenames[i]);
		fd = open(t.filenames[i], O_RDONLY, 0);
		if (fd < 0)
			EXIT_MSG("Could not open file %s: %s\n", t.filenames[i], strerror(errno));

#if DO_MMAP
		mapped_buffers[i] = mmap(NULL, file_sizes[i], PROT_READ,
								 MAP_POPULATE | MAP_PRIVATE, fd, 0);
		if (mapped_buffers[i] == MAP_FAILED)
			EXIT_MSG("Failed to map file %s of size %" PRIu64 " : %s\n",
					 t.filenames[i], (uint_least64_t)file_sizes[i], strerror(errno));
#endif
#if DO_COPY
		dest_buffers[i] = malloc(file_sizes[i]);
		if (dest_buffers[i] == NULL)
			EXIT_MSG("Could not allocate memory for a destination buffer\n");
#endif
#if DO_READ
		int ret = read(fd, dest_buffers[i], file_sizes[i]);
		if (ret != file_sizes[i])
			EXIT_MSG("read failed on file %s: %s\n", t.filenames[i], strerror(errno));
#endif
		close(fd);

	}

#if DO_COPY
#if DO_MMAP
	for (i = 0; i < t.numfiles; i++) {
			memcpy(dest_buffers[i], mapped_buffers[i], file_sizes[i]);
	}
#endif
#endif
}
