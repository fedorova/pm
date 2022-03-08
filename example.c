#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <gperftools/tcmalloc.h>

#define NUM_ITEMS 900*1024
#define ITEM_SIZE 900*1024

int
main(void) {

	int i, j;
	void **buf_array = tc_malloc(NUM_ITEMS * sizeof(void*));
	char data_buffer[1024];

	for(i = 0; i < NUM_ITEMS; i++) {
		buf_array[i] = tc_malloc(ITEM_SIZE);
		printf("Allocated address is: %p\n", buf_array[i]);
		for (j = 0; j < ITEM_SIZE / 1024; j++)
			memcpy((void*)(buf_array[i] + j*1024), data_buffer, 1024);
	}

	for(i = 0; i < NUM_ITEMS; i++) {
		tc_free(buf_array[i]);
	}
	tc_free(buf_array);
}
