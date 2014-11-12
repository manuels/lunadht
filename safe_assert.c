#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>

#include "safe_assert.h"

void *safe_malloc(size_t size) {
	void *ptr = malloc(size);

	safe_assert(size);
	safe_assert(ptr);

	return ptr;
}

void *safe_realloc(void *ptr, size_t count, size_t size) {
	safe_assert(SIZE_MAX / size >= count);
	safe_assert(size > 0);

	ptr = realloc(ptr, count*size);

	return ptr;
}
