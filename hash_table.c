#include <sys/types.h>
#include <errno.h>
#ifdef __linux__
#include <memkind.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "nano_time.h"

#define BYTES_IN_GB (1024 * 1024 * 1024)
#define DEFAULT_BLOCK_SIZE 4096
#define DEFAULT_CACHE_SIZE_GB 32
#define HASHTABLE_NUM_BUCKETS 4*1024*1024
#define NANOSECONDS_IN_SECOND 1000000000

size_t ht_size;

void
print_help_message(const char *progname) {

    /* take only the last portion of the path */
    const char *basename = strrchr(progname, '/');
    basename = basename ? basename + 1 : progname;

    printf("usage: %s\n", basename);

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


typedef struct ht_bucket {
    size_t  htb_offset;
    size_t htb_size;
    void*  htb_memory_address;
    struct ht_bucket *htb_next;
} ht_bucket_t;

typedef struct {
    size_t ht_size;
    ht_bucket_t *ht_buckets;
} hashtable_t;

hashtable_t *ht_allocate(size_t num_items) {

    hashtable_t *ht_ptr;
    ht_bucket_t *htb_ptr;

    htb_ptr = (ht_bucket_t *)malloc(sizeof(ht_bucket_t) * num_items);
    if (htb_ptr == NULL)
	return NULL;

    memset(htb_ptr, 0, sizeof(ht_bucket_t) * num_items);

    ht_ptr = (hashtable_t *)malloc(sizeof(hashtable_t));
    if (ht_ptr == NULL) {
	free(htb_ptr);
	return NULL;
    }

    ht_ptr->ht_size = num_items;
    ht_ptr->ht_buckets = htb_ptr;

    return ht_ptr;
}

void *ht_get(hashtable_t *ht_ptr, size_t offset, size_t *size) {

    ht_bucket_t *htb;
    size_t size;

    size = ht_ptr->ht_size;
    htb = ht_ptr->ht_buckets[offset % size];

    /*
     * Traverse the linked list in the bucket until we find the
     * items with the requested offset.
     */
    while(htb != NULL) {
	if (htb->offset == offset) {
	    *size = htb->size;
	    return htb->htb_memory_address;
	}
	htb = htb->next;
    }

    *size = 0;
    return NULL;
}

void *ht_put(hashtable_t *ht_ptr, off_t offset, size_t size) {

    ht_bucket_t *htb, *htb_prev = NULL, *htb_new;

    htb = ht_ptr->ht_buckets[offset % ht_ptr->size];

    /*
     * Find the empty bucket in the hashtable
     */
    while(htb != NULL) {
	if (htb->offset == offset) {
	    if (htb->size == size) {
		printf("Duplicate item: offset %lld, size %lld\n", offset, size);
		return htb->htb_memory_address;
	    }
	    else
		EXIT_MSG("Duplicate offset, unmatched size: offset %lld, "
			 "hashbtable size: %lld, new item size: %lld\n",
			 offset, htb->size, size);
	    return htb->htb_memory_address;
	}
	else if(htb->offset == 0) {
	    htb->htb_memory_address = malloc(size);
	    if (htb->htb_memory_address == NULL)
		EXIT_MSG("Could not allocate %lld bytes of memory\n", size);
	    htb->htb_offset = offset;
	    htb->htb_size = size;
	    return htb->htb_memory_address;
	}
	else {
	    htb_prev = htb;
	    htb = htb->next;
	}
    }

    /* Allocate a new item */
    htb_new = (ht_bucket_t*)malloc(sizeof(ht_bucket_t));
    if (htb_new == NULL)
	EXIT_MSG("Could not allocate %lld bytes of memory\n", sizeof(ht_bucket_t));

    htb_new->htb_memory_address = malloc(size);
    if (htb_new->htb_memory_address == NULL) {
	    free(htb_new);
	    return NULL;
    }
    htb_new->offset = offset;
    htb_new->size = size;
    htb_new->htb_next = NULL;

    htb_prev->htb_next = htb_new;

    return htb_new->htb_memory_address;
}

void ht_remove(size_t offset, size_t size) {

}

void hashtable_print() {

}
