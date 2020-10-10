#pragma once

#include <x86intrin.h>

// cache line flush
#if __CLWB__
#define flush _mm_clwb
#define FLUSH_METHOD  "_mm_clwb"
#elif __CLFLUSHOPT__
#define flush _mm_clflushopt
#define FLUSH_METHOD  "_mm_clflushopt"
#elif __CLFLUSH__
#define flush _mm_clflush
#define FLUSH_METHOD  "_mm_clflush"
#else
static_assert(0, "cache line flush not supported!");
#endif

// memory fence
#define fence _mm_sfence
#define FENCE_METHOD  "_mm_sfence"