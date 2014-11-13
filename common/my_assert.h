#ifndef _MY_ASSERT_H
#define _MY_ASSERT_H

#include <cassert>
#define strongAssert(x) assert(x)
#ifdef HMAT_NO_ASSERT
#define myAssert(x)
#else
#define myAssert(x) assert(x)
#endif
#endif
