#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>

#include "safe_assert.h"

void *safe_malloc(size_t size) {
	void *ptr = malloc(size);

	safe_assert(size);

	return ptr;
}
