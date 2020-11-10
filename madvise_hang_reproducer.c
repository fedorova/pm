#include <sys/types.h>
#include <errno.h>
#include <memkind.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

const char DEFAULT_MEMKIND_PATH[] = "/mnt/pmem/sasha";

#define DEFAULT_ALLOC_SIZE_GB 32
#define DEFAULT_BLOCK_SIZE_KB 32

#define MAX_CAPACITY_GB 220

#define KB 1024
#define GB 1024 * 1024 * 1024

int main(int argc, char **argv) {

    struct memkind *pmem_kind = NULL;
    int err;
    uint64_t block_size, num_blocks, total_size;
    void **allocated_addresses;

    if (argc == 2) {
	block_size = atoi(argv[1]) * KB;
	total_size = atoi(argv[2]) * GB;

	if (block_size <= 0 || block_size / GB > MAX_CAPACITY_GB) {
	    printf("usage: %s <block size in KB> <total memory in GB>\n"),
		argv[0]);
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
    if (allocated_address == NULL) {
	printf("Could not allocate memory for pointer addresses.\n");
	_exit(-1);
    }
    bzero(allocated_addresses, num_blocks * sizeof(void*));

    for (uint64_t i = 0; i < num_blocks; i++) {
	printf("Allocating block % " PRIu64 " of " PRIu64 ".\n",
	       i, num_blocks);
	allocated_addresses[i] = (void*) memkind_malloc(pmem_kind, block_size);
	if (allocated_addresses[i] == NULL)
	    printf("Could not allocate memory for block %" PRIu64 ".\n", i);
    }

    for (uint64_t i = 0; i < num_blocks; i++) {
	printf("Freeing block % " PRIu64 " of " PRIu64 ".\n",
	       i, num_blocks);
	if (allocated_addresses[i] != NULL)
	    memkind_free(pmem_kind, allocated_addresses[i]);
    }

}
