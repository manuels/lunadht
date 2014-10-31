#ifndef __SAFE_ASSERT_H__
#define __SAFE_ASSERT_H__

#include <assert.h>

#define safe_assert_io(res, len, type) \
	safe_assert(res > 0 && (type) res == len)

#define safe_assert(cond) do { assert(cond); if (!(cond)) { exit(EXIT_FAILURE); } } while(0)

void *safe_malloc(size_t size);

#endif
