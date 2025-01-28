/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2013-2024 OpenMV, LLC.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * CMSIS support.
 */
#ifndef _ARM_MATH_H
#define _ARM_MATH_H

#include <stdint.h>
#include <math.h>

#ifndef PI
#define PI               3.14159265358979f
#endif

#ifndef M_PI
#define M_PI                     3.14159265f
#define M_PI_2                   1.57079632f
#define M_PI_4                   0.78539816f
#endif


#if defined(__GNUC__) || defined(__clang__)
#define __forceinline inline __attribute__((always_inline))
#else
#define __forceinline inline
#endif

#define __STATIC_FORCEINLINE static __forceinline


/**
 * @brief 8-bit fractional data type in 1.7 format.
 */
typedef int8_t q7_t;

/**
 * @brief 16-bit fractional data type in 1.15 format.
 */
typedef int16_t q15_t;

/**
 * @brief 32-bit fractional data type in 1.31 format.
 */
typedef int32_t q31_t;

/**
 * @brief 64-bit fractional data type in 1.63 format.
 */
typedef int64_t q63_t;


#define isnanf isnan
#define isinff isinf


__STATIC_FORCEINLINE uint8_t __CLZ(uint32_t data) {
    if (data == 0U) {
        return 32U;
    }

    uint32_t count = 0U;
    uint32_t mask = 0x80000000U;

    while ((data & mask) == 0U) {
        count += 1U;
        mask = mask >> 1U;
    }
    return count;
}

__STATIC_FORCEINLINE int32_t __SSAT(int32_t val, uint32_t sat) {
    if ((sat >= 1U) && (sat <= 32U)) {
        const int32_t max = (int32_t) ((1U << (sat - 1U)) - 1U);
        const int32_t min = -1 - max;
        if (val > max) {
            return max;
        } else if (val < min) {
            return min;
        }
    }
    return val;
}

__STATIC_FORCEINLINE uint32_t __USAT(int32_t val, uint32_t sat) {
    if (sat <= 31U) {
        const uint32_t max = ((1U << sat) - 1U);
        if (val > (int32_t) max) {
            return max;
        } else if (val < 0) {
            return 0U;
        }
    }
    return (uint32_t) val;
}

/**
   \brief   Rotate Right in unsigned value (32 bit)
   \details Rotate Right (immediate) provides the value of the contents of a register rotated by a variable number of bits.
   \param [in]    op1  Value to rotate
   \param [in]    op2  Number of Bits to rotate
   \return               Rotated value
 */
__STATIC_FORCEINLINE uint32_t __ROR(uint32_t op1, uint32_t op2) {
    op2 %= 32U;
    if (op2 == 0U) {
        return op1;
    }
    return (op1 >> op2) | (op1 << (32U - op2));
}

/**
   \brief   Reverse bit order of value
   \details Reverses the bit order of the given value.
   \param [in]    value  Value to reverse
   \return               Reversed value
 */
__STATIC_FORCEINLINE uint32_t __RBIT(uint32_t value) {
    uint32_t result;

    uint32_t s = (4U /*sizeof(v)*/ * 8U) - 1U; /* extra shift needed at end */

    result = value;                    /* r will be reversed bits of v; first get LSB of v */
    for (value >>= 1U; value != 0U; value >>= 1U) {
        result <<= 1U;
        result |= value & 1U;
        s--;
    }
    result <<= s;                      /* shift when v's highest bits are zero */
    return result;
}


/**
 * @brief Clips Q63 to Q31 values.
 */
__STATIC_FORCEINLINE q31_t clip_q63_to_q31(
    q63_t x) {
    return ((q31_t) (x >> 32) != ((q31_t) x >> 31)) ?
           ((0x7FFFFFFF ^ ((q31_t) (x >> 63)))) : (q31_t) x;
}


/**
 * @brief Clips Q31 to Q15 values.
 */
__STATIC_FORCEINLINE q15_t clip_q31_to_q15(
    q31_t x) {
    return ((q31_t) (x >> 16) != ((q31_t) x >> 15)) ?
           ((0x7FFF ^ ((q15_t) (x >> 31)))) : (q15_t) x;
}

/**
 * @brief Multiplies 32 X 64 and returns 32 bit result in 2.30 format.
 */
__STATIC_FORCEINLINE q63_t mult32x64(
    q63_t x,
    q31_t y) {
    return ((((q63_t) (x & 0x00000000FFFFFFFF) * y) >> 32) +
            (((q63_t) (x >> 32) * y)      )  );
}

/* SMMLAR */
#define multAcc_32x32_keep32_R(a, x, y) \
    a = (q31_t) (((((q63_t) a) << 32) + ((q63_t) x * y) + 0x80000000LL) >> 32)

/* SMMLSR */
#define multSub_32x32_keep32_R(a, x, y) \
    a = (q31_t) (((((q63_t) a) << 32) - ((q63_t) x * y) + 0x80000000LL) >> 32)

/* SMMULR */
#define mult_32x32_keep32_R(a, x, y) \
    a = (q31_t) (((q63_t) x * y + 0x80000000LL) >> 32)

/* SMMLA */
#define multAcc_32x32_keep32(a, x, y) \
    a += (q31_t) (((q63_t) x * y) >> 32)

/* SMMLS */
#define multSub_32x32_keep32(a, x, y) \
    a -= (q31_t) (((q63_t) x * y) >> 32)

/* SMMUL */
#define mult_32x32_keep32(a, x, y) \
    a = (q31_t) (((q63_t) x * y) >> 32)


/**
 * @brief definition to pack two 16 bit values.
 */
#define __PKHBT(ARG1, ARG2, ARG3) ( (((int32_t) (ARG1) << 0) & (int32_t) 0x0000FFFF) | \
                                    (((int32_t) (ARG2) << ARG3) & (int32_t) 0xFFFF0000)  )
#define __PKHTB(ARG1, ARG2, ARG3) ( (((int32_t) (ARG1) << 0) & (int32_t) 0xFFFF0000) | \
                                    (((int32_t) (ARG2) >> ARG3) & (int32_t) 0x0000FFFF)  )


/**
 * @brief definition to pack four 8 bit values.
 */
#ifndef ARM_MATH_BIG_ENDIAN
#define __PACKq7(v0, v1, v2, v3) ( (((int32_t) (v0) << 0) & (int32_t) 0x000000FF) |  \
                                   (((int32_t) (v1) << 8) & (int32_t) 0x0000FF00) |  \
                                   (((int32_t) (v2) << 16) & (int32_t) 0x00FF0000) | \
                                   (((int32_t) (v3) << 24) & (int32_t) 0xFF000000)  )
#else
#define __PACKq7(v0, v1, v2, v3) ( (((int32_t) (v3) << 0) & (int32_t) 0x000000FF) |  \
                                   (((int32_t) (v2) << 8) & (int32_t) 0x0000FF00) |  \
                                   (((int32_t) (v1) << 16) & (int32_t) 0x00FF0000) | \
                                   (((int32_t) (v0) << 24) & (int32_t) 0xFF000000)  )
#endif




/*
 * @brief C custom defined intrinsic functions
 */

/*
 * @brief C custom defined QADD8
 */
__STATIC_FORCEINLINE uint32_t __QADD8(
    uint32_t x,
    uint32_t y) {
    q31_t r, s, t, u;

    r = __SSAT(((((q31_t) x << 24) >> 24) + (((q31_t) y << 24) >> 24)), 8) & (int32_t) 0x000000FF;
    s = __SSAT(((((q31_t) x << 16) >> 24) + (((q31_t) y << 16) >> 24)), 8) & (int32_t) 0x000000FF;
    t = __SSAT(((((q31_t) x << 8) >> 24) + (((q31_t) y << 8) >> 24)), 8) & (int32_t) 0x000000FF;
    u = __SSAT(((((q31_t) x) >> 24) + (((q31_t) y) >> 24)), 8) & (int32_t) 0x000000FF;

    return ((uint32_t) ((u << 24) | (t << 16) | (s << 8) | (r)));
}


/*
 * @brief C custom defined QSUB8
 */
__STATIC_FORCEINLINE uint32_t __QSUB8(
    uint32_t x,
    uint32_t y) {
    q31_t r, s, t, u;

    r = __SSAT(((((q31_t) x << 24) >> 24) - (((q31_t) y << 24) >> 24)), 8) & (int32_t) 0x000000FF;
    s = __SSAT(((((q31_t) x << 16) >> 24) - (((q31_t) y << 16) >> 24)), 8) & (int32_t) 0x000000FF;
    t = __SSAT(((((q31_t) x << 8) >> 24) - (((q31_t) y << 8) >> 24)), 8) & (int32_t) 0x000000FF;
    u = __SSAT(((((q31_t) x) >> 24) - (((q31_t) y) >> 24)), 8) & (int32_t) 0x000000FF;

    return ((uint32_t) ((u << 24) | (t << 16) | (s << 8) | (r)));
}


/*
 * @brief C custom defined QADD16
 */
__STATIC_FORCEINLINE uint32_t __QADD16(
    uint32_t x,
    uint32_t y) {
/*  q31_t r,     s;  without initialisation 'arm_offset_q15 test' fails  but 'intrinsic' tests pass! for armCC */
    q31_t r = 0, s = 0;

    r = __SSAT(((((q31_t) x << 16) >> 16) + (((q31_t) y << 16) >> 16)), 16) & (int32_t) 0x0000FFFF;
    s = __SSAT(((((q31_t) x) >> 16) + (((q31_t) y) >> 16)), 16) & (int32_t) 0x0000FFFF;

    return ((uint32_t) ((s << 16) | (r)));
}


/*
 * @brief C custom defined SHADD16
 */
__STATIC_FORCEINLINE uint32_t __SHADD16(
    uint32_t x,
    uint32_t y) {
    q31_t r, s;

    r = (((((q31_t) x << 16) >> 16) + (((q31_t) y << 16) >> 16)) >> 1) & (int32_t) 0x0000FFFF;
    s = (((((q31_t) x) >> 16) + (((q31_t) y) >> 16)) >> 1) & (int32_t) 0x0000FFFF;

    return ((uint32_t) ((s << 16) | (r)));
}


/*
 * @brief C custom defined QSUB16
 */
__STATIC_FORCEINLINE uint32_t __QSUB16(
    uint32_t x,
    uint32_t y) {
    q31_t r, s;

    r = __SSAT(((((q31_t) x << 16) >> 16) - (((q31_t) y << 16) >> 16)), 16) & (int32_t) 0x0000FFFF;
    s = __SSAT(((((q31_t) x) >> 16) - (((q31_t) y) >> 16)), 16) & (int32_t) 0x0000FFFF;

    return ((uint32_t) ((s << 16) | (r)));
}


/*
 * @brief C custom defined SHSUB16
 */
__STATIC_FORCEINLINE uint32_t __SHSUB16(
    uint32_t x,
    uint32_t y) {
    q31_t r, s;

    r = (((((q31_t) x << 16) >> 16) - (((q31_t) y << 16) >> 16)) >> 1) & (int32_t) 0x0000FFFF;
    s = (((((q31_t) x) >> 16) - (((q31_t) y) >> 16)) >> 1) & (int32_t) 0x0000FFFF;

    return ((uint32_t) ((s << 16) | (r)));
}


/*
 * @brief C custom defined QASX
 */
__STATIC_FORCEINLINE uint32_t __QASX(
    uint32_t x,
    uint32_t y) {
    q31_t r, s;

    r = __SSAT(((((q31_t) x << 16) >> 16) - (((q31_t) y) >> 16)), 16) & (int32_t) 0x0000FFFF;
    s = __SSAT(((((q31_t) x) >> 16) + (((q31_t) y << 16) >> 16)), 16) & (int32_t) 0x0000FFFF;

    return ((uint32_t) ((s << 16) | (r)));
}


/*
 * @brief C custom defined SHASX
 */
__STATIC_FORCEINLINE uint32_t __SHASX(
    uint32_t x,
    uint32_t y) {
    q31_t r, s;

    r = (((((q31_t) x << 16) >> 16) - (((q31_t) y) >> 16)) >> 1) & (int32_t) 0x0000FFFF;
    s = (((((q31_t) x) >> 16) + (((q31_t) y << 16) >> 16)) >> 1) & (int32_t) 0x0000FFFF;

    return ((uint32_t) ((s << 16) | (r)));
}


/*
 * @brief C custom defined QSAX
 */
__STATIC_FORCEINLINE uint32_t __QSAX(
    uint32_t x,
    uint32_t y) {
    q31_t r, s;

    r = __SSAT(((((q31_t) x << 16) >> 16) + (((q31_t) y) >> 16)), 16) & (int32_t) 0x0000FFFF;
    s = __SSAT(((((q31_t) x) >> 16) - (((q31_t) y << 16) >> 16)), 16) & (int32_t) 0x0000FFFF;

    return ((uint32_t) ((s << 16) | (r)));
}


/*
 * @brief C custom defined SHSAX
 */
__STATIC_FORCEINLINE uint32_t __SHSAX(
    uint32_t x,
    uint32_t y) {
    q31_t r, s;

    r = (((((q31_t) x << 16) >> 16) + (((q31_t) y) >> 16)) >> 1) & (int32_t) 0x0000FFFF;
    s = (((((q31_t) x) >> 16) - (((q31_t) y << 16) >> 16)) >> 1) & (int32_t) 0x0000FFFF;

    return ((uint32_t) ((s << 16) | (r)));
}


/*
 * @brief C custom defined SMUSDX
 */
__STATIC_FORCEINLINE uint32_t __SMUSDX(
    uint32_t x,
    uint32_t y) {
    return ((uint32_t) (((((q31_t) x << 16) >> 16) * (((q31_t) y) >> 16)) -
                        ((((q31_t) x) >> 16) * (((q31_t) y << 16) >> 16))   ));
}

/*
 * @brief C custom defined SMUADX
 */
__STATIC_FORCEINLINE uint32_t __SMUADX(
    uint32_t x,
    uint32_t y) {
    return ((uint32_t) (((((q31_t) x << 16) >> 16) * (((q31_t) y) >> 16)) +
                        ((((q31_t) x) >> 16) * (((q31_t) y << 16) >> 16))   ));
}


/*
 * @brief C custom defined QADD
 */
__STATIC_FORCEINLINE int32_t __QADD(
    int32_t x,
    int32_t y) {
    return ((int32_t) (clip_q63_to_q31((q63_t) x + (q31_t) y)));
}


/*
 * @brief C custom defined QSUB
 */
__STATIC_FORCEINLINE int32_t __QSUB(
    int32_t x,
    int32_t y) {
    return ((int32_t) (clip_q63_to_q31((q63_t) x - (q31_t) y)));
}


/*
 * @brief C custom defined SMLAD
 */
__STATIC_FORCEINLINE uint32_t __SMLAD(
    uint32_t x,
    uint32_t y,
    uint32_t sum) {
    return ((uint32_t) (((((q31_t) x << 16) >> 16) * (((q31_t) y << 16) >> 16)) +
                        ((((q31_t) x) >> 16) * (((q31_t) y) >> 16)) +
                        ( ((q31_t) sum)                                  )   ));
}


/*
 * @brief C custom defined SMLADX
 */
__STATIC_FORCEINLINE uint32_t __SMLADX(
    uint32_t x,
    uint32_t y,
    uint32_t sum) {
    return ((uint32_t) (((((q31_t) x << 16) >> 16) * (((q31_t) y) >> 16)) +
                        ((((q31_t) x) >> 16) * (((q31_t) y << 16) >> 16)) +
                        ( ((q31_t) sum)                                  )   ));
}


/*
 * @brief C custom defined SMLSDX
 */
__STATIC_FORCEINLINE uint32_t __SMLSDX(
    uint32_t x,
    uint32_t y,
    uint32_t sum) {
    return ((uint32_t) (((((q31_t) x << 16) >> 16) * (((q31_t) y) >> 16)) -
                        ((((q31_t) x) >> 16) * (((q31_t) y << 16) >> 16)) +
                        ( ((q31_t) sum)                                  )   ));
}


/*
 * @brief C custom defined SMLALD
 */
__STATIC_FORCEINLINE uint64_t __SMLALD(
    uint32_t x,
    uint32_t y,
    uint64_t sum) {
/*  return (sum + ((q15_t) (x >> 16) * (q15_t) (y >> 16)) + ((q15_t) x * (q15_t) y)); */
    return ((uint64_t) (((((q31_t) x << 16) >> 16) * (((q31_t) y << 16) >> 16)) +
                        ((((q31_t) x) >> 16) * (((q31_t) y) >> 16)) +
                        ( ((q63_t) sum)                                  )   ));
}


/*
 * @brief C custom defined SMLALDX
 */
__STATIC_FORCEINLINE uint64_t __SMLALDX(
    uint32_t x,
    uint32_t y,
    uint64_t sum) {
/*  return (sum + ((q15_t) (x >> 16) * (q15_t) y)) + ((q15_t) x * (q15_t) (y >> 16)); */
    return ((uint64_t) (((((q31_t) x << 16) >> 16) * (((q31_t) y) >> 16)) +
                        ((((q31_t) x) >> 16) * (((q31_t) y << 16) >> 16)) +
                        ( ((q63_t) sum)                                  )   ));
}


/*
 * @brief C custom defined SMUAD
 */
__STATIC_FORCEINLINE uint32_t __SMUAD(
    uint32_t x,
    uint32_t y) {
    return ((uint32_t) (((((q31_t) x << 16) >> 16) * (((q31_t) y << 16) >> 16)) +
                        ((((q31_t) x) >> 16) * (((q31_t) y) >> 16))   ));
}


/*
 * @brief C custom defined SMUSD
 */
__STATIC_FORCEINLINE uint32_t __SMUSD(
    uint32_t x,
    uint32_t y) {
    return ((uint32_t) (((((q31_t) x << 16) >> 16) * (((q31_t) y << 16) >> 16)) -
                        ((((q31_t) x) >> 16) * (((q31_t) y) >> 16))   ));
}


/*
 * @brief C custom defined SXTB16
 */
__STATIC_FORCEINLINE uint32_t __SXTB16(
    uint32_t x) {
    return ((uint32_t) (((((q31_t) x << 24) >> 24) & (q31_t) 0x0000FFFF) |
                        ((((q31_t) x << 8) >> 8) & (q31_t) 0xFFFF0000)  ));
}

/*
 * @brief C custom defined SMMLA
 */
__STATIC_FORCEINLINE int32_t __SMMLA(
    int32_t x,
    int32_t y,
    int32_t sum) {
    return (sum + (int32_t) (((int64_t) x * y) >> 32));
}


#endif /* _ARM_MATH_H */
