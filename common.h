#include <assert.h>
#include <bit>
#include <cstdlib>
#include <cstdio>
#include <algorithm>
#include <chrono>
#include <array>
#include <string>
#include <vector>
#include <atomic>
#include <cstdint>
#include <limits>
#include <xmmintrin.h>
#include <cstring>
#include <math.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <linux/mman.h>

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint32_t uint;
typedef uint16_t u16;
typedef int16_t i16;
typedef uint8_t u8;

using ms = std::chrono::milliseconds;
using Clock = std::chrono::steady_clock;

// pretty output
#define RED   "\x1b[31m"
#define BOLD  "\x1b[1m"
#define RESET "\x1b[0m"
#define GRAY  "\x1b[90m"
