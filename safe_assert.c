#include <assert.h>
#include <stdlib.h>

#include "safe_assert.h"

void
safe_assert(int cond) {
	assert(cond);
	if (cond == 0) {
		exit(EXIT_FAILURE);
	}
}
