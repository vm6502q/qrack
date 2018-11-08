#define bitLenInt uint8_t
#define bitCapInt uint64_t
#define bitsInByte 8

#include "config.h"

#if ENABLE_COMPLEX8
#include <complex>
#define complex std::complex<float>
#define real1 float
#define ZERO_R1 0.0f
#define ONE_R1 1.0f
#define PI_R1 (real1) M_PI
#define min_norm 1e-9f
#define polar(A, B) std::polar(A, B)
#else
#include "common/complex16simd.hpp"
#define complex Complex16Simd
#define real1 double
#define ZERO_R1 0.0
#define ONE_R1 1.0
#define PI_R1 M_PI
#define min_norm 1e-15
#endif
