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
#define DEFAULT_BUFFER_SIZE 4096
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

int main(int argc, char **argv) {

    char *buf;
    memkindname_t mem_name;
    
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

    buf = allocate_memory(mem_name, DEFAULT_SIZE_GB);
    if (buf == NULL)
	EXIT_MSG("Could not allocate memory.\n");
      
    copy_memory(buf, DEFAULT_SIZE_GB);
}

char *allocate_memory(memkindname_t m_kind, size_t size) {

    int err;
    char error_message[MEMKIND_ERROR_MESSAGE_SIZE];
    char *ptr_default, *ptr_dax_kmem, *ptr_pmem;

    if (m_kind == DRAM) {
	ptr_default = (char *)memkind_malloc(MEMKIND_DEFAULT, size);
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

void
copy_memory(char *buf, size_t size) {



}
    
