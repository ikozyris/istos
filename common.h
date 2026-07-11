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

typedef unsigned long u64; // 64-bit bitboard: bit i set means square i is occupied
typedef unsigned int u32;
typedef unsigned int uint;
typedef unsigned short u16;
typedef unsigned char u8;

using ms = std::chrono::milliseconds;
using Clock = std::chrono::steady_clock;

// pretty output
#define RED   "\x1b[31m"
#define BOLD  "\x1b[1m"
#define RESET "\x1b[0m"
#define GRAY  "\x1b[90m"
