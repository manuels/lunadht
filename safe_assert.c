#include <assert.h>
#include <stdlib.h>

#include "safe_assert.h"

void
safe_assert(int cond) {
	assert(cond);
	if (!cond) {
		exit(-1);
	}
}
