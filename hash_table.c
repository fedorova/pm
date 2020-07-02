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

/* Use macros, so we can easily replace allocators */
#define FSDAX 1

#if FSDAX
struct memkind *pmem_kind = NULL;

const char DEFAULT_MEMKIND_PATH[] = "/mnt/pmem/sasha";
#define BYTES_IN_GB (1024 * 1024 * 1024)
#define DEFAULT_SIZE_GB 32

#define ALLOC_DATA(size) memkind_malloc(pmem_kind, size)
#define ALLOC_METADATA(size) memkind_malloc(pmem_kind, size)
#define FREE_DATA(ptr) memkind_free(pmem_kind, ptr)
#define FREE_METADATA(ptr) memkind_free(pmem_kind, ptr)

#else

#define ALLOC_DATA(size) malloc(size)
#define ALLOC_METADATA(size) malloc(size)
#define FREE(ptr) free(ptr)
#define FREE_METADATA(ptr) free(ptr)

#endif


typedef struct ht_bucket {
    size_t htb_key;
    size_t htb_value_size;
    void*  htb_value_address;
    struct ht_bucket *htb_next;
} ht_bucket_t;

typedef struct {
    size_t ht_size;
    ht_bucket_t *ht_buckets;
} hashtable_t;

hashtable_t *ht_allocate(size_t num_items) {

    hashtable_t *ht_ptr;
    ht_bucket_t *ht_buckets;

    ht_buckets = (ht_bucket_t *)ALLOC_METADATA(sizeof(ht_bucket_t) * num_items);
    if (ht_buckets == NULL)
	return NULL;

    memset(ht_buckets, 0, sizeof(ht_bucket_t) * num_items);

    ht_ptr = (hashtable_t *)ALLOC_METADATA(sizeof(hashtable_t));
    if (ht_ptr == NULL) {
	FREE_METADATA(ht_buckets);
	return NULL;
    }

    ht_ptr->ht_size = num_items;
    ht_ptr->ht_buckets = ht_buckets;

    return ht_ptr;
}

void *ht_get(hashtable_t *ht_ptr, size_t key, size_t *size) {

    ht_bucket_t *htb;

    htb = &ht_ptr->ht_buckets[key % ht_ptr->ht_size];

    /*
     * Traverse the linked list in the bucket until we find the
     * items with the requested key.
     */
    while(htb != NULL) {
	if (htb->htb_key == key) {
	    *size = htb->htb_value_size;
	    return htb->htb_value_address;
	}
	htb = htb->htb_next;
    }
    *size = 0;
    return NULL;
}

void *ht_put(hashtable_t *ht_ptr, size_t key, size_t size) {

    ht_bucket_t *htb, *htb_prev = NULL, *htb_new;

    htb = &ht_ptr->ht_buckets[key % ht_ptr->ht_size];

    /*
     * Find the empty bucket in the hashtable
     */
    while(htb != NULL) {
	if (htb->htb_key == key) {
	    /* We don't allow duplicate keys for now */
	    return NULL;
	}
	else if(htb->htb_key == 0) {
	    /* Allocate space for the value */
	    htb->htb_value_address = ALLOC_DATA(size);
	    if (htb->htb_value_address == NULL)
		EXIT_MSG("Could not allocate %ld bytes: %s\n",
			 size, strerror(errno));

	    htb->htb_key = key;
	    htb->htb_value_size = size;
	    return htb->htb_value_address;
	}
	else {
	    htb_prev = htb;
	    htb = htb->htb_next;
	}
    }

    /* Allocate a new bucket in the chain */
    htb_new = (ht_bucket_t*)ALLOC_METADATA(sizeof(ht_bucket_t));
    if (htb_new == NULL)
	return NULL;

    htb_new->htb_value_address = ALLOC_DATA(size);
    if (htb_new->htb_value_address == NULL) {
	FREE_METADATA(htb_new);
	return NULL;
    }
    htb_new->htb_key = key;
    htb_new->htb_value_size = size;
    htb_new->htb_next = NULL;

    htb_prev->htb_next = htb_new;

    return htb_new->htb_value_address;
}

void ht_remove(hashtable_t *ht_ptr, size_t key, size_t size) {

    ht_bucket_t *htb, *htb_prev = NULL, *htb_new;

    htb = &ht_ptr->ht_buckets[key % ht_ptr->ht_size];

    while(htb != NULL) {
	if (htb->htb_key == key) {
	    if (htb->htb_value_size != size)
		EXIT_MSG("Found key, unmatched size: key %zu, "
			 "hashbtable size: %zu, new item size: %zu\n",
			 key, htb->htb_value_size, size);
	    else{ /* Remove */
		FREE_DATA(htb->htb_value_address);
		if (htb_prev == NULL) { /* first item in chain */
		    htb->htb_key = 0;
		    htb->htb_value_size = 0;
		    htb->htb_value_address = 0;
		}
		else {
		    htb_prev->htb_next = htb->htb_next;
		    FREE_METADATA(htb);
		}
		return;
	    }
	}
	htb_prev = htb;
	htb = htb->htb_next;
    }
    printf("Remove can't find requested item: key %zu, size %zu\n",
	   key, size);
}

void ht_bucket_print(ht_bucket_t *htb, size_t idx) {

    printf("Bucket %ld: \n", idx);
    while (htb != NULL) {
	printf("\t Key = %ld, value_address = %p, size = %ld\n",
	       htb->htb_key, htb->htb_value_address, htb->htb_value_size);
     	htb = htb->htb_next;
    }
    printf("\n");
}

void hashtable_print(hashtable_t *ht_ptr) {

    size_t i;

    for (i = 0; i < ht_ptr->ht_size; i++)
	ht_bucket_print(&ht_ptr->ht_buckets[i], i);
}


#define MAX_KEY 1000000
#define MAX_SIZE 32768

typedef struct {
    size_t key;
    size_t size;
} ht_item_t;

int main(void) {

    ht_item_t *items_put;
    int i, num_items = 1024;
    size_t key, size, ret_size;
    void *addr;
    uint64_t begin_time, end_time;

#if FSDAX
    if (memkind_create_pmem(DEFAULT_MEMKIND_PATH, 0, &pmem_kind) != 0)
	EXIT_MSG("Could not create pmem device: %s\n", strerror(errno));
#endif
    /*
     * Allocate the space to remember the keys and sizes we add
     * to the hashtable.
     */
    items_put = (ht_item_t *)malloc(num_items * sizeof(ht_item_t));
    if (items_put == NULL)
	EXIT_MSG("Could not allocate items array of %d items.\n", num_items);

    hashtable_t *ht = ht_allocate(num_items);
    if (ht == NULL)
	EXIT_MSG("Could not allocate hash table of %d items.\n", num_items);

    /* Put a bunch of items */
    printf("Putting the following items:\n");
    begin_time = nano_time();
    for (i = 0; i < num_items; i++) {
	key = random() % MAX_KEY;
	size = random() % MAX_SIZE;

	addr = ht_put(ht, key, size);
	/* Duplicate key not allowed. Try again */
	if (addr == NULL) {
	    i--;
	    continue;
	}

	items_put[i].key = key;
	items_put[i].size = size;
    }
    end_time = nano_time();
    printf("Put time for %d items is %ld ns\n", num_items,
	   (end_time - begin_time));

    /* Check that they are there */
    for (i = 0; i < num_items; i++) {
	addr = ht_get(ht, items_put[i].key, &ret_size);
	printf("\t [%d] key: %ld, size: %ld\n", i, items_put[i].key,
	       items_put[i].size);
	if (addr == NULL || (items_put[i].size != ret_size))
	    EXIT_MSG("Expected item %ld of size %ld not found.\n",
		     items_put[i].key, items_put[i].size);
    }

    printf("\n\nHASHTABLE:\n");
    hashtable_print(ht);

    /* Remove all items */
    for (i = 0; i < num_items; i++) {
	ht_remove(ht, items_put[i].key, items_put[i].size);

	if (ht_get(ht, items_put[i].key, &ret_size) != NULL)
	    printf("Removed item (key: %ld, size: %ld) was found.\n",
		   items_put[i].key, items_put[i].size);
    }

    printf("\n\nHASHTABLE:\n");
    hashtable_print(ht);
}
