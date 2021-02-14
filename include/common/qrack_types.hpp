//////////////////////////////////////////////////////////////////////////////////////
//
// (C) Daniel Strano and the Qrack contributors 2017-2021. All rights reserved.
//
// This is a multithreaded, universal quantum register simulation, allowing
// (nonphysical) register cloning and direct measurement of probability and
// phase, to leverage what advantages classical emulation of qubits can have.
//
// Licensed under the GNU Lesser General Public License V3.
// See LICENSE.md in the project root or https://www.gnu.org/licenses/lgpl-3.0.en.html
// for details.

#pragma once

#include <cfloat>
#include <complex>
#include <functional>
#include <memory>
#include <random>

#include "config.h"

#if UINTPOW < 5
#define ONE_BCI ((uint16_t)1U)
#define bitCapIntOcl uint16_t
#elif UINTPOW < 6
#define ONE_BCI 1U
#define bitCapIntOcl uint32_t
#else
#define ONE_BCI 1UL
#define bitCapIntOcl uint64_t
#endif

#if QBCAPPOW < 8
#define bitLenInt uint8_t
#elif QBCAPPOW < 16
#define bitLenInt uint16_t
#elif QBCAPPOW < 32
#define bitLenInt uint32_t
#else
#define bitLenInt uint64_t
#endif

#if QBCAPPOW < 6
#define bitsInCap 32
#define bitCapInt uint32_t
#elif QBCAPPOW < 7
#define bitsInCap 64
#define bitCapInt uint64_t
#elif QBCAPPOW < 8
#define bitsInCap 128
#ifdef BOOST_AVAILABLE
#include <boost/multiprecision/cpp_int.hpp>
#define bitCapInt boost::multiprecision::uint128_t
#else
#define bitCapInt __uint128_t
#endif
#else
#define bitsInCap (8U * (1U << QBCAPPOW))
#include <boost/multiprecision/cpp_int.hpp>
#define bitCapInt                                                                                                      \
    boost::multiprecision::number<boost::multiprecision::cpp_int_backend<1 << QBCAPPOW, 1 << QBCAPPOW,                 \
        boost::multiprecision::unsigned_magnitude, boost::multiprecision::unchecked, void>>
#endif

#define bitsInByte 8
#define qrack_rand_gen std::mt19937_64
#define qrack_rand_gen_ptr std::shared_ptr<qrack_rand_gen>
#define QRACK_ALIGN_SIZE 64

#if FPPOW < 5
namespace Qrack {
#ifdef BOOST_AVAILABLE
#include <boost/cstdfloat.hpp>
typedef std::complex<boost::float16_t> complex;
typedef boost::float16_t real1;
#else
//#include <arm_fp16.h>
typedef std::complex<__fp16> complex;
typedef __fp16 real1;
#endif
#define ZERO_R1 ((real1)0.0f)
#define ONE_R1 ((real1)1.0f)
#define PI_R1 (((real1)M_PI)
#define REAL1_DEFAULT_ARG ((real1)-999.0f)
#define REAL1_EPSILON ((real1)FLT_EPSILON)
} // namespace Qrack
#elif FPPOW < 6
namespace Qrack {
typedef std::complex<float> complex;
typedef float real1;
#define ZERO_R1 0.0f
#define ONE_R1 1.0f
#define PI_R1 ((real1)M_PI)
#define REAL1_DEFAULT_ARG -999.0f
#define REAL1_EPSILON FLT_EPSILON
} // namespace Qrack
#else
namespace Qrack {
typedef std::complex<double> complex;
typedef double real1;
#define ZERO_R1 0.0
#define ONE_R1 1.0
#define PI_R1 M_PI
#define REAL1_DEFAULT_ARG -999.0
#define REAL1_EPSILON DBL_EPSILON
} // namespace Qrack
#endif

#define ONE_CMPLX complex(ONE_R1, ZERO_R1)
#define ZERO_CMPLX complex(ZERO_R1, ZERO_R1)
#define I_CMPLX complex(ZERO_R1, ONE_R1)
#define CMPLX_DEFAULT_ARG complex(REAL1_DEFAULT_ARG, REAL1_DEFAULT_ARG)

namespace Qrack {
typedef std::shared_ptr<complex> BitOp;

/** Called once per value between begin and end. */
typedef std::function<void(const bitCapInt, const int cpu)> ParallelFunc;
typedef std::function<bitCapInt(const bitCapInt, const int cpu)> IncrementFunc;

class StateVector;
class StateVectorArray;
class StateVectorSparse;

typedef std::shared_ptr<StateVector> StateVectorPtr;
typedef std::shared_ptr<StateVectorArray> StateVectorArrayPtr;
typedef std::shared_ptr<StateVectorSparse> StateVectorSparsePtr;

// This is a buffer struct that's capable of representing controlled single bit gates and arithmetic, when subclassed.
class StateVector {
protected:
    bitCapInt capacity;

public:
    bool isReadLocked;

    StateVector(bitCapInt cap)
        : capacity(cap)
        , isReadLocked(true)
    {
    }
    virtual complex read(const bitCapInt& i) = 0;
    virtual void write(const bitCapInt& i, const complex& c) = 0;
    /// Optimized "write" that is only guaranteed to write if either amplitude is nonzero. (Useful for the result of 2x2
    /// tensor slicing.)
    virtual void write2(const bitCapInt& i1, const complex& c1, const bitCapInt& i2, const complex& c2) = 0;
    virtual void clear() = 0;
    virtual void copy_in(const complex* inArray) = 0;
    virtual void copy_in(const complex* copyIn, const bitCapInt offset, const bitCapInt length) = 0;
    virtual void copy_in(
        StateVectorPtr copyInSv, const bitCapInt srcOffset, const bitCapInt dstOffset, const bitCapInt length) = 0;
    virtual void copy_out(complex* outArray) = 0;
    virtual void copy_out(complex* copyIn, const bitCapInt offset, const bitCapInt length) = 0;
    virtual void copy(StateVectorPtr toCopy) = 0;
    virtual void shuffle(StateVectorPtr svp) = 0;
    virtual void get_probs(real1* outArray) = 0;
    virtual bool is_sparse() = 0;
};

void mul2x2(complex* left, complex* right, complex* out);
void exp2x2(complex* matrix2x2, complex* outMatrix2x2);
void log2x2(complex* matrix2x2, complex* outMatrix2x2);
} // namespace Qrack
