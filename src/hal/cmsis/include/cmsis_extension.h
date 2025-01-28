/*
 * This file is part of the OpenMV project.
 *
 * Copyright (c) 2013-2024 Ibrahim Abdelkader <iabdalkader@openmv.io>
 * Copyright (c) 2013-2024 Kwabena W. Agyeman <kwagyeman@openmv.io>
 *
 * This work is licensed under the MIT license, see the file LICENSE for details.
 *
 * CMSIS Extension
 */
#ifndef __CMSIS_EXTENSION_H
#define __CMSIS_EXTENSION_H
#include "cmsis_compiler.h"

#if ((defined (__ARM_ARCH_7M__      ) && (__ARM_ARCH_7M__      == 1)) || \
     (defined (__ARM_ARCH_7EM__     ) && (__ARM_ARCH_7EM__     == 1)) || \
     (defined (__ARM_ARCH_8M_MAIN__ ) && (__ARM_ARCH_8M_MAIN__ == 1))    )

/**
  \brief   Signed Saturate
  \details Saturates a signed value.
  \param [in]  ARG1  Value to be saturated
  \param [in]  ARG2  Bit position to saturate to (1..32)
  \param [in]  ARG3  Right shift (0..31)
  \return             Saturated value
 */
#define __SSAT_ASR(ARG1, ARG2, ARG3) \
__extension__ \
({                          \
  int32_t __RES, __ARG1 = (ARG1); \
  __ASM volatile ("ssat %0, %1, %2, asr %3" : "=r" (__RES) :  "I" (ARG2), "r" (__ARG1), "I" (ARG3) : "cc" ); \
  __RES; \
 })

/**
  \brief   Unsigned Saturate
  \details Saturates an unsigned value.
  \param [in]  ARG1  Value to be saturated
  \param [in]  ARG2  Bit position to saturate to (0..31)
  \param [in]  ARG3  Right shift (0..31)
  \return             Saturated value
 */
#define __USAT_ASR(ARG1, ARG2, ARG3) \
 __extension__ \
({                          \
  uint32_t __RES, __ARG1 = (ARG1); \
  __ASM volatile ("usat %0, %1, %2, asr %3" : "=r" (__RES) :  "I" (ARG2), "r" (__ARG1), "I" (ARG3) : "cc" ); \
  __RES; \
 })

#else  /* ((defined (__ARM_ARCH_7M__      ) && (__ARM_ARCH_7M__      == 1)) || \
           (defined (__ARM_ARCH_7EM__     ) && (__ARM_ARCH_7EM__     == 1)) || \
           (defined (__ARM_ARCH_8M_MAIN__ ) && (__ARM_ARCH_8M_MAIN__ == 1))    ) */

/**
  \brief   Signed Saturate
  \details Saturates a signed value.
  \param [in]  value  Value to be saturated
  \param [in]    sat  Bit position to saturate to (1..32)
  \param [in]  shift  Right shift (0..31)
  \return             Saturated value
 */
__STATIC_FORCEINLINE int32_t __SSAT_ASR(int32_t val, uint32_t sat, uint32_t shift)
{
  val >>= shift & 0x1F;

  if ((sat >= 1U) && (sat <= 32U))
  {
    const int32_t max = (int32_t)((1U << (sat - 1U)) - 1U);
    const int32_t min = -1 - max ;
    if (val > max)
    {
      return max;
    }
    else if (val < min)
    {
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
__STATIC_FORCEINLINE int32_t __SSAT16(int32_t val, uint32_t sat)
{
  if ((sat >= 1U) && (sat <= 32U))
  {
    const int32_t max = (int32_t)((1U << (sat - 1U)) - 1U);
    const int32_t min = -1 - max ;
    int32_t valHi = val >> 16;
    if (valHi > max)
    {
      valHi = max;
    }
    else if (valHi < min)
    {
      valHi = min;
    }
    int32_t valLo = (val << 16) >> 16;
    if (valLo > max)
    {
      valLo = max;
    }
    else if (valLo < min)
    {
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
__STATIC_FORCEINLINE uint32_t __USAT_ASR(int32_t val, uint32_t sat, uint32_t shift)
{
  val >>= shift & 0x1F;

  if (sat <= 31U)
  {
    const uint32_t max = ((1U << sat) - 1U);
    if (val > (int32_t)max)
    {
      return max;
    }
    else if (val < 0)
    {
      return 0U;
    }
  }
  return (uint32_t)val;
}

/**
  \brief   Unsigned Saturate
  \details Saturates two unsigned values.
  \param [in]  value  Values to be saturated
  \param [in]    sat  Bit position to saturate to (0..15)
  \return             Saturated value
 */
__STATIC_FORCEINLINE uint32_t __USAT16(int32_t val, uint32_t sat)
{
  if (sat <= 15U)
  {
    const uint32_t max = ((1U << sat) - 1U);
    int32_t valHi = val >> 16;
    if (valHi > (int32_t)max)
    {
      valHi = max;
    }
    else if (valHi < 0)
    {
      valHi = 0U;
    }
    int32_t valLo = (val << 16) >> 16;
    if (valLo > (int32_t)max)
    {
      valLo = max;
    }
    else if (valLo < 0)
    {
      valLo = 0U;
    }
    return (valHi << 16) | valLo;
  }
  return (uint32_t)val;
}

#endif /* ((defined (__ARM_ARCH_7M__      ) && (__ARM_ARCH_7M__      == 1)) || \
           (defined (__ARM_ARCH_7EM__     ) && (__ARM_ARCH_7EM__     == 1)) || \
           (defined (__ARM_ARCH_8M_MAIN__ ) && (__ARM_ARCH_8M_MAIN__ == 1))    ) */

#if (defined (__ARM_FEATURE_DSP) && (__ARM_FEATURE_DSP == 1))

__STATIC_FORCEINLINE uint32_t __SMULBB(uint32_t op1, uint32_t op2)
{
  uint32_t result;

  __ASM volatile ("smulbb %0, %1, %2" : "=r" (result) : "r" (op1), "r" (op2) );
  return(result);
}

__STATIC_FORCEINLINE uint32_t __SMULBT(uint32_t op1, uint32_t op2)
{
  uint32_t result;

  __ASM volatile ("smulbt %0, %1, %2" : "=r" (result) : "r" (op1), "r" (op2) );
  return(result);
}

__STATIC_FORCEINLINE uint32_t __SMULTB(uint32_t op1, uint32_t op2)
{
  uint32_t result;

  __ASM volatile ("smultb %0, %1, %2" : "=r" (result) : "r" (op1), "r" (op2) );
  return(result);
}

__STATIC_FORCEINLINE uint32_t __SMULTT(uint32_t op1, uint32_t op2)
{
  uint32_t result;

  __ASM volatile ("smultt %0, %1, %2" : "=r" (result) : "r" (op1), "r" (op2) );
  return(result);
}

__STATIC_FORCEINLINE uint32_t __SMULWB(uint32_t op1, uint32_t op2)
{
  uint32_t result;

  __ASM volatile ("smulwb %0, %1, %2" : "=r" (result) : "r" (op1), "r" (op2) );
  return(result);
}

__STATIC_FORCEINLINE uint32_t __SMULWT(uint32_t op1, uint32_t op2)
{
  uint32_t result;

  __ASM volatile ("smulwt %0, %1, %2" : "=r" (result) : "r" (op1), "r" (op2) );
  return(result);
}

__STATIC_FORCEINLINE uint32_t __SMLABB(uint32_t op1, uint32_t op2, uint32_t op3)
{
  uint32_t result;

  __ASM volatile ("smlabb %0, %1, %2, %3" : "=r" (result) : "r" (op1), "r" (op2), "r" (op3) );
  return(result);
}

__STATIC_FORCEINLINE uint32_t __SMLABT(uint32_t op1, uint32_t op2, uint32_t op3)
{
  uint32_t result;

  __ASM volatile ("smlabt %0, %1, %2, %3" : "=r" (result) : "r" (op1), "r" (op2), "r" (op3) );
  return(result);
}

__STATIC_FORCEINLINE uint32_t __SMLATB(uint32_t op1, uint32_t op2, uint32_t op3)
{
  uint32_t result;

  __ASM volatile ("smlatb %0, %1, %2, %3" : "=r" (result) : "r" (op1), "r" (op2), "r" (op3) );
  return(result);
}

__STATIC_FORCEINLINE uint32_t __SMLATT(uint32_t op1, uint32_t op2, uint32_t op3)
{
  uint32_t result;

  __ASM volatile ("smlatt %0, %1, %2, %3" : "=r" (result) : "r" (op1), "r" (op2), "r" (op3) );
  return(result);
}

__STATIC_FORCEINLINE uint32_t __SMLAWB(uint32_t op1, uint32_t op2, uint32_t op3)
{
  uint32_t result;

  __ASM volatile ("smlawb %0, %1, %2, %3" : "=r" (result) : "r" (op1), "r" (op2), "r" (op3) );
  return(result);
}

__STATIC_FORCEINLINE uint32_t __SMLAWT(uint32_t op1, uint32_t op2, uint32_t op3)
{
  uint32_t result;

  __ASM volatile ("smlawt %0, %1, %2, %3" : "=r" (result) : "r" (op1), "r" (op2), "r" (op3) );
  return(result);
}

__STATIC_FORCEINLINE uint32_t __SMMULR(uint32_t op1, uint32_t op2)
{
  uint32_t result;

  __ASM volatile ("smmulr %0, %1, %2" : "=r" (result) : "r" (op1), "r" (op2) );
  return(result);
}

__STATIC_FORCEINLINE uint32_t __UXTH(uint32_t op1)
{
  uint32_t result;

  __ASM volatile ("uxth %0, %1" : "=r" (result) : "r" (op1));
  return(result);
}

__STATIC_FORCEINLINE uint32_t __UXTB(uint32_t op1)
{
  uint32_t result;

  __ASM volatile ("uxtb %0, %1" : "=r" (result) : "r" (op1));
  return(result);
}

__STATIC_FORCEINLINE uint32_t __UXTB_RORn(uint32_t op1, uint32_t rotate)
{
  uint32_t result;

  __ASM volatile ("uxtb %0, %1, ROR %2" : "=r" (result) : "r" (op1), "i" (rotate) );
  return result;
}

__STATIC_FORCEINLINE uint32_t __UXTB16_RORn(uint32_t op1, uint32_t rotate)
{
  uint32_t result;

  __ASM volatile ("uxtb16 %0, %1, ROR %2" : "=r" (result) : "r" (op1), "i" (rotate) );
  return result;
}

__STATIC_FORCEINLINE uint32_t __UXTAB_RORn(uint32_t op1, uint32_t op2, uint32_t rotate)
{
  uint32_t result;

  __ASM volatile ("uxtab %0, %1, %2, ROR %3" : "=r" (result) : "r" (op1), "r" (op2), "i" (rotate) );
  return result;
}

__STATIC_FORCEINLINE uint32_t __UXTAB16_RORn(uint32_t op1, uint32_t op2, uint32_t rotate)
{
  uint32_t result;

  __ASM volatile ("uxtab16 %0, %1, %2, ROR %3" : "=r" (result) : "r" (op1), "r" (op2), "i" (rotate) );
  return result;
}

__STATIC_FORCEINLINE uint32_t __SXTH(uint32_t op1)
{
  uint32_t result;

  __ASM volatile ("sxth %0, %1" : "=r" (result) : "r" (op1));
  return(result);
}

__STATIC_FORCEINLINE uint32_t __SXTB(uint32_t op1)
{
  uint32_t result;

  __ASM volatile ("sxtb %0, %1" : "=r" (result) : "r" (op1));
  return(result);
}

__STATIC_FORCEINLINE uint32_t __SXTB_RORn(uint32_t op1, uint32_t rotate)
{
  uint32_t result;

  __ASM volatile ("sxtb %0, %1, ROR %2" : "=r" (result) : "r" (op1), "i" (rotate) );
  return result;
}

__STATIC_FORCEINLINE uint32_t __SXTAB_RORn(uint32_t op1, uint32_t op2, uint32_t rotate)
{
  uint32_t result;

  __ASM volatile ("sxtab %0, %1, %2, ROR %3" : "=r" (result) : "r" (op1), "r" (op2), "i" (rotate) );
  return result;
}

#else

__STATIC_FORCEINLINE int32_t __SMULBB(int32_t op1, int32_t op2)
{
  return ((op1 << 16) >> 16) * ((op2 << 16) >> 16);
}

__STATIC_FORCEINLINE int32_t __SMULBT(int32_t op1, int32_t op2)
{
  return ((op1 << 16) >> 16) * (op2 >> 16);
}

__STATIC_FORCEINLINE int32_t __SMULTB(int32_t op1, int32_t op2)
{
  return (op1 >> 16) * ((op2 << 16) >> 16);
}

__STATIC_FORCEINLINE int32_t __SMULTT(int32_t op1, int32_t op2)
{
  return (op1 >> 16) * (op2 >> 16);
}

__STATIC_FORCEINLINE int32_t __SMULWB(int32_t op1, int32_t op2)
{
  return (((int64_t) op1) * ((int64_t) ((op2 << 16) >> 16))) >> 16;
}

__STATIC_FORCEINLINE int32_t __SMULWT(int32_t op1, int32_t op2)
{
  return (((int64_t) op1) * ((int64_t) (op2 >> 16))) >> 16;
}

__STATIC_FORCEINLINE int32_t __SMLABB(int32_t op1, int32_t op2, int32_t op3)
{
  return (((op1 << 16) >> 16) * ((op2 << 16) >> 16)) + op3;
}

__STATIC_FORCEINLINE int32_t __SMLABT(int32_t op1, int32_t op2, int32_t op3)
{
  return (((op1 << 16) >> 16) * (op2 >> 16)) + op3;
}

__STATIC_FORCEINLINE int32_t __SMLATB(int32_t op1, int32_t op2, int32_t op3)
{
  return ((op1 >> 16) * ((op2 << 16) >> 16)) + op3;
}

__STATIC_FORCEINLINE int32_t __SMLATT(int32_t op1, int32_t op2, int32_t op3)
{
  return ((op1 >> 16) * (op2 >> 16)) + op3;
}

__STATIC_FORCEINLINE int32_t __SMLAWB(int32_t op1, int32_t op2, int32_t op3)
{
  return ((((int64_t) op1) * ((int64_t) ((op2 << 16) >> 16))) >> 16) + op3;
}

__STATIC_FORCEINLINE int32_t __SMLAWT(int32_t op1, int32_t op2, int32_t op3)
{
  return ((((int64_t) op1) * ((int64_t) (op2 >> 16))) >> 16) + op3;
}

__STATIC_FORCEINLINE int32_t __SMMULR(int32_t op1, int32_t op2)
{
  return ((((int64_t) op1) * ((int64_t) op2)) + 0x80000000) >> 32;
}

__STATIC_FORCEINLINE uint32_t __UXTH(uint32_t op1)
{
  return op1 & 0xFFFF;
}

__STATIC_FORCEINLINE uint32_t __UXTB(uint32_t op1)
{
  return op1 & 0xFF;
}

__STATIC_FORCEINLINE uint32_t __UXTB_RORn(uint32_t op1, uint32_t rotate)
{
  return (op1 >> rotate) & 0xFF;
}

__STATIC_FORCEINLINE uint32_t __UXTAB_RORn(uint32_t op1, uint32_t op2, uint32_t rotate)
{
  return ((op1 >> rotate) & 0xFF) + op2;
}

__STATIC_FORCEINLINE uint32_t __SXTH(int32_t op1)
{
  return (op1 << 16) >> 16;
}

__STATIC_FORCEINLINE uint32_t __SXTB(int32_t op1)
{
  return (op1 << 24) >> 24;
}

__STATIC_FORCEINLINE uint32_t __SXTB_RORn(int32_t op1, uint32_t rotate)
{
  return ((op1 >> rotate) << 24) >> 24;
}

__STATIC_FORCEINLINE uint32_t __SXTAB_RORn(int32_t op1, uint32_t op2, uint32_t rotate)
{
  return (((op1 >> rotate) << 24) >> 24) + op2;
}

__STATIC_FORCEINLINE uint32_t __SSUB16(uint32_t op1, uint32_t op2)
{
  return ((op1 & 0xFFFF0000) - (op2 & 0xFFFF0000)) | ((op1 - op2) & 0xFFFF);
}

__STATIC_FORCEINLINE uint32_t __USAD8(uint32_t op1, uint32_t op2)
{
  uint32_t result = abs((op1 & 0xFF) - (op2 & 0xFF));
  result += abs(((op1 >> 8) & 0xFF) - ((op2 >> 8) & 0xFF));
  result += abs(((op1 >> 16) & 0xFF) - ((op2 >> 16) & 0xFF));
  result += abs(((op1 >> 24) & 0xFF) - ((op2 >> 24) & 0xFF));
  return result;
}

__STATIC_FORCEINLINE uint32_t __USADA8(uint32_t op1, uint32_t op2, uint32_t op3)
{
  op3 += abs((op1 & 0xFF) - (op2 & 0xFF));
  op3 += abs(((op1 >> 8) & 0xFF) - ((op2 >> 8) & 0xFF));
  op3 += abs(((op1 >> 16) & 0xFF) - ((op2 >> 16) & 0xFF));
  op3 += abs(((op1 >> 24) & 0xFF) - ((op2 >> 24) & 0xFF));
  return op3;
}

#endif /* (__ARM_FEATURE_DSP == 1) */
#endif /* __CMSIS_EXTENSIONS_H */
