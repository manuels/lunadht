#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>

#include "safe_assert.h"

void
safe_assert(bool cond) {
	assert(cond);
	if (!cond) {
		exit(EXIT_FAILURE);
	}
}
