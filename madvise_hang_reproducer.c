#include <sys/types.h>
#include <errno.h>
#include <inttypes.h>
#include <jemalloc/jemalloc.h>
#include <memkind.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

const char DEFAULT_MEMKIND_PATH[] = "/mnt/pmem/sasha";

#define DEFAULT_ALLOC_SIZE_GB 230
#define DEFAULT_BLOCK_SIZE_KB 28

#define MAX_CAPACITY_GB 220

static const u_int64_t KB = 1024;
static const u_int64_t GB = 1024 * 1024 * 1024;

#if 0
void printit(void *ctx, const char *str) {
    printf("%s",str);
}

int jemalloc_info() {
    printf("\n");
    jemk_malloc_stats_print(printit, NULL, NULL);
    return 0;
}
#endif

int main(int argc, char **argv) {

    struct memkind *pmem_kind = NULL;
    char error_message[MEMKIND_ERROR_MESSAGE_SIZE];
    int err;
    size_t metadata = 0, sz = sizeof(size_t);
    u_int64_t block_size, i, num_blocks, total_size;
    void **allocated_addresses;

    if (argc == 3) {
	block_size = atoi(argv[1]) * KB;
	total_size = atoi(argv[2]) * GB;

	if (block_size <= 0 || block_size / GB > MAX_CAPACITY_GB) {
	    printf("usage: %s <block size in KB> <total memory in GB>\n",
		argv[0]);
	    _exit(-1);
	}
    }
    else {
	block_size = DEFAULT_BLOCK_SIZE_KB * KB;
	total_size = DEFAULT_ALLOC_SIZE_GB * GB;
    }

    if(memkind_create_pmem(DEFAULT_MEMKIND_PATH, 0, &pmem_kind)) {
	memkind_error_message(err, error_message, MEMKIND_ERROR_MESSAGE_SIZE);
	_exit(-1);
    }

    num_blocks = total_size / block_size;

    allocated_addresses = malloc(num_blocks * sizeof(void*));
    if (allocated_addresses == NULL) {
	printf("Could not allocate memory for pointer addresses.\n");
	_exit(-1);
    }
    bzero(allocated_addresses, num_blocks * sizeof(void*));

    for (i = 0; i < num_blocks; i++) {
	if ( i % 1024 == 0)
	    printf("Allocating block %" PRIu64 " of %" PRIu64 ".\n",
		   i, num_blocks);
	allocated_addresses[i] = (void*) memkind_malloc(pmem_kind, block_size);
	if (allocated_addresses[i] == NULL) {
	    printf("Ran out of memory after %zu blocks (%zu bytes).\n", i, i*block_size);
	    break;
	    //printf("Could not allocate memory for block %" PRIu64 ".\n", i);
	}
    }

//    jemalloc_info();
//    jemk_mallctl("stats.metadata", &metadata, &sz, NULL, 0);
//    printf("metadata=%zu\n", metadata);
    while(1);

    for (i = 0; i < num_blocks; i++) {
	if ( i % 1024 == 0)
	    printf("Freeing block %" PRIu64 " of %" PRIu64 ".\n",
		   i, num_blocks);
	if (allocated_addresses[i] != NULL)
	    memkind_free(pmem_kind, allocated_addresses[i]);
    }

}
