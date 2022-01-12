/**
 * For certain tests, create a file prior to running them.
 * This command will give you a 4GB:
 *      $ dd < /dev/zero bs=1048576 count=4096 > testfile
 *
 * It will actually allocate space on disk.
 *
 * To clear caches, use the following command on Linux:
 *      # sync; echo 1 > /proc/sys/vm/drop_caches
 *
 * To create devdax namespace:
 * $ sudo ndctl create-namespace --force --reconfig=namespace0.0 \
 *           --mode=devdax --align=2M
 */

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

#include "nano_time.h"

#define BYTES_IN_GB (1024 * 1024 * 1024)
#define DEFAULT_BLOCK_SIZE 8192
#define DEFAULT_SIZE_DEVDAX_GB 32
#define NANOSECONDS_IN_SECOND 1000000000

const char DEFAULT_FNAME[] = "/dev/dax0.0";
static int static_size_GB = DEFAULT_SIZE_DEVDAX_GB;
const char *devdax = "/dev/dax";
const char *devraw[4] = {"/dev/nvme", "/dev/pmem", "/dev/mapper", 0};

static int
file_is_devdax(const char *filename) {
    if (strncmp(filename, devdax, strlen(devdax)) == 0)
        return 1;
    else
        return 0;
}

static int
file_is_raw(const char *filename) {

    const char *str_devraw;
    int i = 0;

    while ( (str_devraw = devraw[i++]) != 0) {
        if (strncmp(filename, str_devraw, strlen(str_devraw)) == 0)
            return 1;
    }
    return 0;
}

#define EXIT_MSG(...)                  \
    do {                                       \
        printf(__VA_ARGS__);           \
        _exit(-1);                 \
    } while (0)

#define EXIT_HELP_MSG(...)             \
    do {                                       \
        printf(__VA_ARGS__);           \
        print_help_message(argv[0]);       \
        _exit(-1);                 \
    } while (0)

#define MSG_NOT_SILENT(...)            \
    do {                                   \
        if (!silent)               \
            printf(__VA_ARGS__);   \
    } while (0)

uint64_t do_mmap_test(int fd, int tid, size_t block_size, size_t filesize, char *buf,
              char optype, off_t *offsets, uint64_t *begin, uint64_t *end);
uint64_t do_read_mmap_test(int fd, int tid, size_t block_size, size_t filesize,
               char* buf, off_t *offs, uint64_t *begin, uint64_t *end);
uint64_t do_read_syscall_test(int fd, int tid, size_t block_size, size_t filesize,
                  off_t *offsets,  uint64_t *begin, uint64_t *end);
uint64_t do_syscall_test(int fd, int tid, size_t block_size, size_t filesize,
             char optype, off_t *offsets, uint64_t *begin, uint64_t *end);
uint64_t do_write_mmap_test(int fd, int tid, size_t block_size, size_t filesize,
                char* buf, off_t *offs, uint64_t *begin, uint64_t *end);
uint64_t do_write_syscall_test(int fd, int tid, size_t block_size, size_t filesize,
                   off_t *offsets,  uint64_t *begin, uint64_t *end);
size_t   get_filesize(const char* filename);
char*    map_buffer(int fd, size_t size);
void     print_help_message(const char* progname);
void    *run_tests(void *);

static int silent = 0;

typedef struct {
    int tid;
    int fd;
    char *mapped_buffer;
    int read_mmap;
    int read_syscall;
    int write_mmap;
    int write_syscall;
    off_t *offsets;
    size_t block_size;
    size_t chunk_size;
    int retval;
    uint64_t start_time;
    uint64_t end_time;
} threadargs_t;

int main(int argc, char **argv) {

    char *fname = (char*) DEFAULT_FNAME;
    char *mapped_buffer = NULL;
    int c, fd, flags = O_RDWR, i, numthreads = 1, ret, option_index;
    static int direct_io, randomaccess = 0,
        read_mmap = 0, read_syscall = 0,
        write_mmap = 0, write_syscall = 0;
    off_t *offsets = 0;
    size_t block_size = DEFAULT_BLOCK_SIZE, filesize, numblocks,
        new_file_size = 0;
    uint64_t min_start_time, max_end_time = 0, retval;

    pthread_t *threads;
    threadargs_t *threadargs;

    mode_t mode = S_IRWXU | S_IRWXG;

    static struct option long_options[] =
        {
        /* These options set a flag. */
        {"randomaccess", no_argument,  &randomaccess, 1},
        {"readmmap", no_argument,   &read_mmap, 1},
        {"readsyscall", no_argument,  &read_syscall, 1},
        {"silent", no_argument,  &silent, 1},
        {"writemmap", no_argument,   &write_mmap, 1},
        {"writesyscall", no_argument,  &write_syscall, 1},
        /* These options may take an argument. */
        {"block", required_argument, 0, 'b'},
        {"directio", no_argument, 0, 'd'},
        {"file", required_argument, 0, 'f'},
        {"help", no_argument, 0, 'h'},
        {"size", no_argument, 0, 's'},
        {"threads", required_argument, 0, 't'},
        {0, 0, 0, 0}
    };

    /* Read long options */
    while (1) {
        c = getopt_long (argc, argv, "b:df:hs:t:",
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
        case 'd':
#ifdef __linux__
            flags |= O_DIRECT;
            MSG_NOT_SILENT("Using DIRECT_IO.\n");
#else
            printf("Direct I/O not supported.\n");
#endif
            break;
        case 'f':
            fname = optarg;
            break;
        case 'h':
            print_help_message(argv[0]);
            _exit(0);
        case 's':
            new_file_size = (size_t)atoi(optarg);
            break;
        case 't':
            numthreads = (int) (atoi(optarg));
            break;
        default:
            break;
        }
    }

    MSG_NOT_SILENT("pid: %d\n", getpid());
    MSG_NOT_SILENT("Using file %s\n", fname);

    if (new_file_size > 0)
        static_size_GB = new_file_size;

    if (file_is_devdax(fname))
        MSG_NOT_SILENT("Will map area of %dGB\n", static_size_GB);

    if ((filesize = get_filesize(fname)) == -1) {
        if (read_mmap || read_syscall)
            EXIT_MSG("Cannot obtain file size for %s: %s"
                   "File must exist prior to running read tests.\n",
                   fname, strerror(errno));
    }

    if (file_is_devdax(fname) && (read_syscall || write_syscall))
        EXIT_MSG("Dev-dax mode does not support syscall experiments\n");

    fd = open((const char*)fname, flags, mode);
    if (fd < 0) {
        printf("Could not open/create file %s: %s\n",
               fname, strerror(errno));
        _exit(-1);
    }

    if (block_size < 0 || block_size > filesize)
        EXIT_MSG("Invalid block size: %" PRIu64 " for file of size "
               "%" PRIu64 ". Block size must be greater than zero "
               "and no greater than the file size.\n",
               (uint_least64_t)block_size, (uint_least64_t)filesize);

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
    for (size_t i = 0; i < numblocks; i++) {
        if (randomaccess)
            offsets[i] = ((int)random() % numblocks) * block_size;
        else
            offsets[i] = i*block_size;
    }

    MSG_NOT_SILENT("Using %d threads\n", numthreads);

    if (numblocks % numthreads != 0)
        EXIT_MSG("We have %" PRIu64 " blocks and %d threads. "
               "Threads must evenly divide blocks. "
               "Please fix your arguments.\n",
               (uint_least64_t)numblocks, numthreads);

    if (read_mmap || write_mmap)
        assert((mapped_buffer = map_buffer(fd, filesize)) != NULL);

    threads = (pthread_t*)malloc(numthreads * sizeof(pthread_t));
    threadargs =
        (threadargs_t*)malloc(numthreads * sizeof(threadargs_t));
    if (threads == NULL || threadargs == NULL)
        EXIT_MSG("Could not allocate thread array for %d threads.\n",
               numthreads);

    for (i = 0; i < numthreads; i++) {
        threadargs[i].fd = fd;
        threadargs[i].tid = i;
        threadargs[i].block_size = block_size;
        threadargs[i].chunk_size = filesize / numthreads;
        threadargs[i].mapped_buffer = mapped_buffer;
        threadargs[i].offsets = &offsets[numblocks/numthreads * i];
        threadargs[i].read_mmap = read_mmap;
        threadargs[i].read_syscall = read_syscall;
        threadargs[i].write_mmap = write_mmap;
        threadargs[i].write_syscall = write_syscall;

        int ret = pthread_create(&threads[i], NULL, run_tests,
                     &threadargs[i]);
        if (ret != 0)
            EXIT_MSG("pthread_create for %dth thread failed: %s\n",
                   i, strerror(errno));
    }

    for (i = 0; i < numthreads; i++) {
        ret = pthread_join(threads[i], NULL);
        if (ret != 0)
            EXIT_MSG("Thread %d failed: %s\n", i, strerror(ret));
    }

    min_start_time = threadargs[0].start_time;
    max_end_time = 0;

        /*
     * Tally up the running times. Find the smallest start time and
     * the largest end time across threads.
     */
    for (i = 0; i < numthreads; i++) {
        min_start_time = (threadargs[i].start_time < min_start_time)?
            threadargs[i].start_time:min_start_time;
        max_end_time = (threadargs[i].end_time > max_end_time)?
            threadargs[i].end_time:max_end_time;
    }
    printf("%d: \t %.2f\n", numthreads,
           (double)filesize/(double)(max_end_time-min_start_time)
           * NANOSECONDS_IN_SECOND / BYTES_IN_GB);

    munmap(mapped_buffer, filesize);
    close(fd);
}

void *
run_tests(void *args) {

    uint64_t retval;
    threadargs_t t = *(threadargs_t*)args;

    if (t.read_mmap) {
        MSG_NOT_SILENT("Running readmmap test:\n");
        retval = do_read_mmap_test(t.fd, t.tid, t.block_size, t.chunk_size,
                       t.mapped_buffer, t.offsets,
                       &((threadargs_t*)args)->start_time,
                       &((threadargs_t*)args)->end_time);
    }
    if (t.read_syscall) {
        MSG_NOT_SILENT("Running readsyscall test:\n");
        retval = do_read_syscall_test(t.fd, t.tid, t.block_size,
                          t.chunk_size,
                          t.offsets,
                          &((threadargs_t*)args)->start_time,
                          &((threadargs_t*)args)->end_time);
    }
    if (t.write_mmap) {
        MSG_NOT_SILENT("Running writemmap test:\n");
        retval = do_write_mmap_test(t.fd, t.tid, t.block_size,
                        t.chunk_size,
                        t.mapped_buffer, t.offsets,
                        &((threadargs_t*)args)->start_time,
                        &((threadargs_t*)args)->end_time);
    }
    if (t.write_syscall) {
        MSG_NOT_SILENT("Running writesyscall test:\n");
        retval = do_write_syscall_test(t.fd, t.tid, t.block_size,
                           t.chunk_size, t.offsets,
                           &((threadargs_t*)args)->start_time,
                           &((threadargs_t*)args)->end_time);
    }
    return (void*) 0;
}

#define READ 1
#define WRITE 2

/**
 * SYSCALL TESTS
 *
 */
uint64_t
do_read_syscall_test(int fd, int tid, size_t block_size, size_t filesize,
             off_t *offsets, uint64_t *begin, uint64_t *end) {

    return do_syscall_test(fd, tid, block_size, filesize, READ, offsets,
                   begin, end);
}

uint64_t
do_write_syscall_test(int fd, int tid, size_t block_size, size_t filesize,
              off_t *offsets, uint64_t *begin, uint64_t *end) {

    return do_syscall_test(fd, tid, block_size, filesize, WRITE, offsets,
                   begin, end);
}

uint64_t
do_syscall_test(int fd, int tid, size_t block_size, size_t filesize, char optype,
        off_t *offsets, uint64_t *begin, uint64_t *end) {

    bool done = false;
    char *buffer = NULL;
    size_t i = 0, total_bytes_transferred = 0;
    uint64_t begin_time, end_time, ret_token = 0;

    buffer = (char*)malloc(block_size);
    if (buffer == NULL) {
        printf("Failed to allocate memory: %s\n", strerror(errno));
        return -1;
    }
    memset((void*)buffer, 0, block_size);

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

    MSG_NOT_SILENT("%s: (tid %d) %.2f GB/s "
               "(%" PRIu64 " bytes in %" PRIu64 " ns).\n",
               (optype==READ)?"readsyscall":"writesyscall", tid,
               (double)filesize/(double)(end_time-begin_time)
               * NANOSECONDS_IN_SECOND / BYTES_IN_GB,
               (uint_least64_t)filesize, (end_time-begin_time));

    *begin = begin_time;
    *end   = end_time;
    return ret_token;
}

/**
 * MMAP tests
 */

uint64_t
do_read_mmap_test(int fd, int tid, size_t block_size, size_t filesize,
          char *buf, off_t *offsets, uint64_t *begin, uint64_t *end) {

    return do_mmap_test(fd, tid, block_size, filesize, buf, READ, offsets,
            begin, end);
}

uint64_t
do_write_mmap_test(int fd, int tid, size_t block_size, size_t filesize,
           char *buf, off_t *offsets, uint64_t *begin, uint64_t *end){

    return do_mmap_test(fd, tid, block_size, filesize, buf, WRITE, offsets,
            begin, end);
}

#if SAMPLE_LATENCY
#define BEGIN_LAT_SAMPLE            \
    if (num_samples < 10 && i%LAT_SAMPL_INTERVAL == 0)  \
        lat_begin_time = nano_time();

#define END_LAT_SAMPLE              \
    if (num_samples < 10 && i%LAT_SAMPL_INTERVAL == 0) {        \
    lat_end_time = nano_time();                 \
    latency_samples[i/LAT_SAMPL_INTERVAL % MAX_LAT_SAMPLES] =   \
        lat_end_time - lat_begin_time;              \
    num_samples++;                          \
    }

#define MAX_LAT_SAMPLES 10
#define LAT_SAMPL_INTERVAL 1048576

#else

#define BEGIN_LAT_SAMPLE ;
#define END_LAT_SAMPLE

#endif

uint64_t
do_mmap_test(int fd, int tid, size_t block_size, size_t size,
         char *mmapped_buffer, char optype, off_t *offsets,
         uint64_t *begin, uint64_t *end)
{
    char *buffer = NULL;
    uint64_t i, j, numblocks, ret;
    uint64_t begin_time, end_time, ret_token = 0;
#if SAMPLE_LATENCY
    uint64_t lat_begin_time, lat_end_time;
    size_t latency_samples[MAX_LAT_SAMPLES];
    int num_samples = 0;

    memset((void*)latency_samples, 0, sizeof(latency_samples));
#endif

    buffer = (char*)malloc(block_size);
    if (buffer == NULL) {
        printf("Failed to allocate memory: %s\n", strerror(errno));
        return -1;
    }
    memset((void*)buffer, 1, block_size);

    begin_time = nano_time();

    /* If we change the loop spec as follows:
     * for (i = 0; i < size/block_size; i++)
     * and then use 'i' to index the offset array
     * sequential read throughput drops by 16x.
     * I don't understand why. But be careful
     * changing this loop.
     */
    for (i = 0; i < size; i+=block_size) {
        off_t offset = offsets[i/block_size];
        BEGIN_LAT_SAMPLE;
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
        END_LAT_SAMPLE;
    }

    end_time = nano_time();

    MSG_NOT_SILENT("%s: (tid %d) %.2f GB/s "
               "(%" PRIu64 " bytes in %" PRIu64 " ns).\n",
               (optype==READ)?"readmmap":"writemmap", tid,
               (double)size/(double)(end_time-begin_time)
               * NANOSECONDS_IN_SECOND / BYTES_IN_GB,
               (uint_least64_t)size, (end_time-begin_time));

    *begin = begin_time;
    *end   = end_time;

#if SAMPLE_LATENCY
    printf("\nSample latency for %ld byte block:\n", block_size);
    for (i = 0; i < MAX_LAT_SAMPLES; i++)
        printf("\t%ld: %ld\n", i, latency_samples[i]);
#endif

    return ret_token;
}

char *
map_buffer(int fd, size_t size) {

    char *mmapped_buffer = NULL;

#ifdef __linux__ /* Assumes Linux 2.6.23 or newer */
    mmapped_buffer = (char *)mmap(NULL, size,
                      PROT_READ | PROT_WRITE,
                      MAP_SHARED, fd, 0);
#else
    mmapped_buffer = (char *)mmap(NULL, size,
                      PROT_READ | PROT_WRITE,
                      MAP_SHARED, fd, 0);
#endif
    if (mmapped_buffer == MAP_FAILED)
        EXIT_MSG("Failed to mmap file of size %" PRIu64 " : %s\n",
               (uint_least64_t)size, strerror(errno));

    return mmapped_buffer;
}

size_t
get_filesize(const char* filename) {

    int retval;
    struct stat st;

    /* Devdax character device */
    if (file_is_devdax(filename) || file_is_raw(filename))
        return static_size_GB * (size_t)BYTES_IN_GB;
    
    /* Regular files */
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
    printf("  --readsyscall\n"
           "     Perform a read test using system calls.\n");
    printf("  --readmmap\n"
           "     Perform a read test using mmap.\n");
    printf("  --silent\n"
           "     Don't print a lot.\n");
    printf("  --size\n"
           "     Size of the area to map in devdax mode.\n"
           "     Defaults to %d GB.\n", DEFAULT_SIZE_DEVDAX_GB);
    printf("  --threads\n"
           "     The number of threads to use. Defaults to one.\n");
    printf("  --writesyscall\n"
           "     Perform a write test using system calls.\n");
    printf("  --writemmap\n"
           "     Perform a write test using mmap.\n");
}
