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
#ifndef __CMSIS_EXTENSION_H
#define __CMSIS_EXTENSION_H
#include "arm_math.h"

/**
   \brief   Signed Saturate
   \details Saturates a signed value.
   \param [in]  value  Value to be saturated
   \param [in]    sat  Bit position to saturate to (1..32)
   \param [in]  shift  Right shift (0..31)
   \return             Saturated value
 */
__STATIC_FORCEINLINE int32_t __SSAT_ASR(int32_t val, uint32_t sat, uint32_t shift) {
    val >>= shift & 0x1F;

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

/**
   \brief   Signed Saturate
   \details Saturates two signed values.
   \param [in]  value  Values to be saturated
   \param [in]    sat  Bit position to saturate to (1..16)
   \return             Saturated value
 */
__STATIC_FORCEINLINE int32_t __SSAT16(int32_t val, uint32_t sat) {
    if ((sat >= 1U) && (sat <= 32U)) {
        const int32_t max = (int32_t) ((1U << (sat - 1U)) - 1U);
        const int32_t min = -1 - max;
        int32_t valHi = val >> 16;
        if (valHi > max) {
            valHi = max;
        } else if (valHi < min) {
            valHi = min;
        }
        int32_t valLo = (val << 16) >> 16;
        if (valLo > max) {
            valLo = max;
        } else if (valLo < min) {
            valLo = min;
        }
        return (valHi << 16) | (valLo & 0xFFFF);
    }
    return val;
}

/**
   \brief   Unsigned Saturate
   \details Saturates an unsigned value.
   \param [in]  value  Value to be saturated
   \param [in]    sat  Bit position to saturate to (0..31)
   \param [in]  shift  Right shift (0..31)
   \return             Saturated value
 */
__STATIC_FORCEINLINE uint32_t __USAT_ASR(int32_t val, uint32_t sat, uint32_t shift) {
    val >>= shift & 0x1F;

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
   \brief   Unsigned Saturate
   \details Saturates two unsigned values.
   \param [in]  value  Values to be saturated
   \param [in]    sat  Bit position to saturate to (0..15)
   \return             Saturated value
 */
__STATIC_FORCEINLINE uint32_t __USAT16(int32_t val, uint32_t sat) {
    if (sat <= 15U) {
        const uint32_t max = ((1U << sat) - 1U);
        int32_t valHi = val >> 16;
        if (valHi > (int32_t) max) {
            valHi = max;
        } else if (valHi < 0) {
            valHi = 0U;
        }
        int32_t valLo = (val << 16) >> 16;
        if (valLo > (int32_t) max) {
            valLo = max;
        } else if (valLo < 0) {
            valLo = 0U;
        }
        return (valHi << 16) | valLo;
    }
    return (uint32_t) val;
}

__STATIC_FORCEINLINE uint32_t __UXTB(uint32_t op1) {
    return op1 & 0xFF;
}

__STATIC_FORCEINLINE uint32_t __UXTB_RORn(uint32_t op1, uint32_t rotate) {
    return (op1 >> rotate) & 0xFF;
}

__STATIC_FORCEINLINE uint32_t __SSUB16(uint32_t op1, uint32_t op2) {
    return ((op1 & 0xFFFF0000) - (op2 & 0xFFFF0000)) | ((op1 - op2) & 0xFFFF);
}

__STATIC_FORCEINLINE uint32_t abs_diff(uint32_t op1, uint32_t op2) {
    return (op1 > op2) ? (op1 - op2) : (op2 - op1);
}

__STATIC_FORCEINLINE uint32_t __USAD8(uint32_t op1, uint32_t op2) {
    uint32_t result = abs_diff((op1 & 0xFF), (op2 & 0xFF));
    result += abs_diff(((op1 >> 8) & 0xFF), ((op2 >> 8) & 0xFF));
    result += abs_diff(((op1 >> 16) & 0xFF), ((op2 >> 16) & 0xFF));
    result += abs_diff(((op1 >> 24) & 0xFF), ((op2 >> 24) & 0xFF));
    return result;
}

__STATIC_FORCEINLINE uint32_t __USADA8(uint32_t op1, uint32_t op2, uint32_t op3) {
    op3 += abs_diff((op1 & 0xFF), (op2 & 0xFF));
    op3 += abs_diff(((op1 >> 8) & 0xFF), ((op2 >> 8) & 0xFF));
    op3 += abs_diff(((op1 >> 16) & 0xFF), ((op2 >> 16) & 0xFF));
    op3 += abs_diff(((op1 >> 24) & 0xFF), ((op2 >> 24) & 0xFF));
    return op3;
}

#endif /* __CMSIS_EXTENSIONS_H */