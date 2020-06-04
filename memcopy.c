#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "nano_time.h"

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


const char DEFAULT_MEMKIND_DIR[] = "/mnt/pmem";


#define BYTES_IN_GB (1024 * 1024 * 1024)
#define DEFAULT_BLOCK_SIZE 4096
#define DEFAULT_SIZE_DEVDAX_GB 32
#define NANOSECONDS_IN_SECOND 1000000000

typedef enum state {DRAM, DAX, FSDAX} memkind_t;

int main(int argc, char **argv) {

    memkind_t memknd;

    if (argc < 2)
	EXIT_HELP_MSG("Missing memory kind.\n");

    switch (atoi(argv[1])) {
    case DRAM:
	memknd = DRAM;
	break;
    case DAX:
	memknd = DAX;
	break;
    case FSDAX:
	memknd = FSDAX;
	break;
    default:
	EXIT_HELP_MSG("Invalid memory kind.\n");
    }
	
}


