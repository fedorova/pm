#include <sys/types.h>
#include <errno.h>
#include <memkind.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "nano_time.h"

const char DEFAULT_MEMKIND_PATH[] = "/mnt/pmem/sasha";

#define BYTES_IN_GB (1024 * 1024 * 1024)
#define DEFAULT_BLOCK_SIZE 4096
#define DEFAULT_SIZE_GB 32
#define NANOSECONDS_IN_SECOND 1000000000

void
print_help_message(const char *progname) {

    /* take only the last portion of the path */
    const char *basename = strrchr(progname, '/');
    basename = basename ? basename + 1 : progname;

    printf("usage: %s <mem_kind>\n", basename);
    printf("mem_kind can be:\n");
    printf("\t 0 -- DRAM\n");
    printf("\t 1 -- NVRAM-DEVDAX\n");
    printf("\t 2 -- NVRAM-FSDAX\n");
}


#define EXIT_HELP_MSG(...)		       \
    do {                                       \
	    printf(__VA_ARGS__);	       \
	    print_help_message(argv[0]);       \
	    _exit(-1);			       \
    } while (0)

#define EXIT_MSG(...)                          \
    do {                                       \
            printf(__VA_ARGS__);               \
            _exit(-1);                         \
    } while (0)


typedef enum state {DRAM, DAX, FSDAX} memkindname_t;

char *allocate_memory(memkindname_t kind, size_t size);
void  copy_memory(char *buf, size_t size);
void  populate_memory(char *buf, size_t size);

int main(int argc, char **argv) {

    char *buf;
    memkindname_t mem_name;
    size_t size = 0;

    if (argc < 2)
	EXIT_HELP_MSG("Missing memory kind.\n");

    switch (atoi(argv[1])) {
    case DRAM:
	mem_name = DRAM;
	break;
    case DAX:
	mem_name = DAX;
	break;
    case FSDAX:
	mem_name = FSDAX;
	break;
    default:
	EXIT_HELP_MSG("Invalid memory kind.\n");
    }

    size = (size_t)DEFAULT_SIZE_GB * (size_t)BYTES_IN_GB;
    buf = allocate_memory(mem_name, size);
    if (buf == NULL)
	EXIT_MSG("Could not allocate memory.\n");

    populate_memory(buf, size);

    for (int i = 0; i < 10; i++)
	copy_memory(buf, size);
}

char *allocate_memory(memkindname_t m_kind, size_t size) {

    int err;
    char error_message[MEMKIND_ERROR_MESSAGE_SIZE];
    char *ptr_default, *ptr_dax_kmem, *ptr_pmem;

    if (m_kind == DRAM) {
	ptr_default = (char *)memkind_malloc(MEMKIND_DEFAULT, size);
	return ptr_default;
    }
    else if (m_kind == DAX) {
	ptr_dax_kmem = (char *)memkind_malloc(MEMKIND_DAX_KMEM_ALL, size);
	return ptr_dax_kmem;
    }
    else if (m_kind == FSDAX) {

	struct memkind *pmem_kind = NULL;

	err = memkind_create_pmem(DEFAULT_MEMKIND_PATH, 0, &pmem_kind);
	if (err)
	    goto error;

	ptr_pmem = (char *)memkind_malloc(pmem_kind, size);
	return ptr_pmem;
    }

error:
    memkind_error_message(err, error_message, MEMKIND_ERROR_MESSAGE_SIZE);
    EXIT_MSG("%s\n", (char*)error_message);

    return NULL; /* keep the compiler happy */
}

/*
 * If we don't write anything into memory prior to reading it, the kernel
 * figures this out, and we get the throughput as if we are reading from
 * DRAM.
 */
void
populate_memory(char *src, size_t size) {

    char buffer[DEFAULT_BLOCK_SIZE]; /* Fix this */
    size_t i, numblocks;
    uint64_t begin_time, end_time;

    numblocks = size / DEFAULT_BLOCK_SIZE;
    printf("Data size: %ld GB\n", (numblocks * (size_t)DEFAULT_BLOCK_SIZE)/
	   (size_t)BYTES_IN_GB);
    buffer[0] = 1;

    /* Write data to memory */
    begin_time = nano_time();
    for (i = 0; i < numblocks; i++) {
        memcpy(&src[i * DEFAULT_BLOCK_SIZE], buffer, DEFAULT_BLOCK_SIZE);
    }
    end_time = nano_time();

    printf("Write throughput: %.2f GB/s \n", (double)size/
	   (double)(end_time-begin_time)
           * NANOSECONDS_IN_SECOND / BYTES_IN_GB);
}

#define BEGIN_LAT_SAMPLE			\
    if (i%LAT_SAMPL_INTERVAL == 0)		\
	    lat_begin_time = nano_time();

#define END_LAT_SAMPLE				\
    if (i%LAT_SAMPL_INTERVAL == 0) {					\
    lat_end_time = nano_time();						\
    latency_samples[i/LAT_SAMPL_INTERVAL % MAX_LAT_SAMPLES] =		\
	lat_end_time - lat_begin_time;					\
	}

void
copy_memory(char *src, size_t size) {

#define MAX_LAT_SAMPLES 10
#define LAT_SAMPL_INTERVAL 100

    char buffer[DEFAULT_BLOCK_SIZE]; /* Fix this */
    size_t i, numblocks;
    uint64_t begin_time, end_time, lat_begin_time, lat_end_time,
	meaningless_sum = 0;
    size_t latency_samples[MAX_LAT_SAMPLES];

    numblocks = size / DEFAULT_BLOCK_SIZE;
    printf("Data size: %ld GB\n", (numblocks * (size_t)DEFAULT_BLOCK_SIZE)/
	   (size_t)BYTES_IN_GB);

    /* Read data from memory */
    begin_time = nano_time();
    for (i = 0; i < numblocks; i++) {

	BEGIN_LAT_SAMPLE;
	memcpy(buffer, &src[i * DEFAULT_BLOCK_SIZE], DEFAULT_BLOCK_SIZE);
	END_LAT_SAMPLE;
	meaningless_sum += buffer[0];
    }
    end_time = nano_time();

    printf("Read throughput: %.2f GB/s \n", (double)size/
	   (double)(end_time-begin_time)
	   * NANOSECONDS_IN_SECOND / BYTES_IN_GB);

    printf("\nSample latency for %d byte block:\n", DEFAULT_BLOCK_SIZE);
    for (i = 0; i < MAX_LAT_SAMPLES; i++)
	printf("\t%ld: %ld\n", i, latency_samples[i]);

    printf("%ld\n", meaningless_sum);
}

