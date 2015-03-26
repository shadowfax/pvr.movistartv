/*
 *      Copyright (C) 2014 Jean-Luc Barriere
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301 USA
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#ifndef ATOMIC_H
#define	ATOMIC_H

#ifdef	__cplusplus
extern "C" {
#endif

#if defined _MSC_VER
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#if !defined CC_INLINE
#define CC_INLINE __inline
#endif
#else
#if !defined CC_INLINE
#define CC_INLINE inline
#endif
#endif

#if defined __APPLE__
#include <libkern/OSAtomic.h>
typedef volatile int32_t atomic_t;
#else
typedef volatile unsigned atomic_t;
#endif

#if defined __arm__ && (!defined __thumb__ || defined __thumb2__)
/* The __ARM_ARCH define is provided by gcc 4.8.  Construct it otherwise.  */
#ifndef __ARM_ARCH
#ifdef __ARM_ARCH_2__
#define __ARM_ARCH 2
#elif defined (__ARM_ARCH_3__) || defined (__ARM_ARCH_3M__)
#define __ARM_ARCH 3
#elif defined (__ARM_ARCH_4__) || defined (__ARM_ARCH_4T__)
#define __ARM_ARCH 4
#elif defined (__ARM_ARCH_5__) || defined (__ARM_ARCH_5E__) \
        || defined(__ARM_ARCH_5T__) || defined(__ARM_ARCH_5TE__) \
        || defined(__ARM_ARCH_5TEJ__)
#define __ARM_ARCH 5
#elif defined (__ARM_ARCH_6__) || defined(__ARM_ARCH_6J__) \
        || defined (__ARM_ARCH_6Z__) || defined(__ARM_ARCH_6ZK__) \
        || defined (__ARM_ARCH_6K__) || defined(__ARM_ARCH_6T2__)
#define __ARM_ARCH 6
#elif defined (__ARM_ARCH_7__) || defined(__ARM_ARCH_7A__) \
        || defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7M__) \
        || defined(__ARM_ARCH_7EM__)
#define __ARM_ARCH 7
#else
#warning could not detect ARM architecture
#define __ARM_ARCH 8
#endif
#endif
#endif

/**
 * Atomically incremente a reference count variable.
 * \param valp address of atomic variable
 * \return incremented reference count
 */
static CC_INLINE unsigned atomic_increment(atomic_t *valp)
{
  atomic_t __val;

#if defined _MSC_VER
  __val = InterlockedIncrement(valp);

#elif defined __APPLE__
  __val = OSAtomicIncrement32(valp);

#elif defined ANDROID
  __val = __sync_add_and_fetch(valp, 1);

#elif defined __mips__
  int temp, inc = 1;
  __asm __volatile (
    "1:  ll     %0, %1\n"       /* load old value */
    "    addu   %2, %0, %3\n"   /* calculate new value */
    "    sc     %2, %1\n"       /* attempt to store */
    "    beqz   %2, 1b\n"       /* spin if failed */
    : "=&r" (__val), "=m" (*valp), "=&r" (temp)
    : "r" (inc), "m" (*valp));
  /* __val is the old value, so normalize it. */
  ++__val;

#elif defined __i486__ || defined __i586__ || defined __i686__
  __asm__ volatile (
    "lock xaddl %0, (%1);"
    "     inc   %0;"
    : "=r" (__val)
    : "r" (valp), "0" (0x1)
    : "cc", "memory"
    );

#elif defined __i386__ || defined __x86_64__
  __asm__ volatile (
    ".byte 0xf0, 0x0f, 0xc1, 0x02" /*lock; xaddl %eax, (%edx) */
    : "=a" (__val)
    : "0" (1), "m" (*valp), "d" (valp)
    : "memory");
  /* __val is the pre-increment value, so normalize it. */
  ++__val;

#elif defined __powerpc__ || defined __ppc__
  __asm__ volatile (
    "1:	lwarx   %0,0,%1\n"
    "	addic.   %0,%0,1\n"
    "	dcbt    %0,%1\n"
    "	stwcx.  %0,0,%1\n"
    "	bne-    1b\n"
    "	isync\n"
    : "=&r" (__val)
    : "r" (valp)
    : "cc", "memory");

#elif defined __sparcv9__
  atomic_t __newval, __oldval = (*valp);
  do {
    __newval = __oldval + 1;
    __asm__ (
      "cas	[%4], %2, %0"
      : "=r" (__oldval), "=m" (*valp)
      : "r" (__oldval), "m" (*valp), "r"((valp)), "0" (__newval));
  } while (__newval != __oldval);
  /* The value for __val is in '__oldval' */
  __val = __oldval;

#elif (defined __ARM_ARCH && __ARM_ARCH >= 7)
  int inc = 1;
  __asm__ volatile (
    "dmb     ish\n"           /* Memory barrier */
    "1:"
    "ldrex   %0, [%1]\n"
    "add     %0, %0,  %2\n"
    "strex   r1, %0, [%1]\n"
    "cmp     r1, #0\n"
    "bne     1b\n"
    "dmb     ish\n"           /* Memory barrier */
    : "=&r" (__val)
    : "r"(valp), "r"(inc)
    : "r1");

#elif (defined __ARM_ARCH && __ARM_ARCH == 6)
  int inc = 1;
  __asm__ volatile (
    "mcr p15, 0, %0, c7, c10, 5"  /* Memory barrier */
    : : "r"(0) : "memory");
  __asm__ volatile (
    "1:"
    "ldrex   %0, [%1]\n"
    "add     %0, %0,  %2\n"
    "strex   r1, %0, [%1]\n"
    "cmp     r1, #0\n"
    "bne     1b\n"
    : "=&r" (__val)
    : "r"(valp), "r"(inc)
    : "r1");
  __asm__ volatile (
    "mcr p15, 0, %0, c7, c10, 5"  /* Memory barrier */
    : : "r"(0) : "memory");

#elif (defined __ARM_ARCH && __ARM_ARCH < 6)
  int tmp1, tmp2;
  int inc = 1;
  __asm__ volatile (
    "0:"
    "ldr     %0, [%3]\n"
    "add     %1, %0, %4\n"
    "swp     %2, %1, [%3]\n"
    "cmp     %0, %2\n"
    "swpne   %0, %2, [%3]\n"
    "bne     0b\n"
    : "=&r"(tmp1), "=&r"(__val), "=&r"(tmp2)
    : "r" (valp), "r"(inc)
    : "cc", "memory");

#elif defined __GNUC__
  /*
   * Don't know how to atomic increment for a generic architecture
   * so try to use GCC builtin
   */
  __val = __sync_add_and_fetch(valp, 1);

#else
#warning unknown architecture, atomic increment is not...
  __val = ++(*valp);

#endif
  return __val;
}

/**
 * Atomically decrement a reference count variable.
 * \param valp address of atomic variable
 * \return decremented reference count
 */
static CC_INLINE unsigned atomic_decrement(atomic_t *valp)
{
  atomic_t __val;

#if defined _MSC_VER
  __val = InterlockedDecrement(valp);

#elif defined __APPLE__
  __val = OSAtomicDecrement32(valp);

#elif defined ANDROID
  __val = __sync_sub_and_fetch(valp, 1);

#elif defined __mips__
  int temp, sub = 1;
  __asm __volatile (
    "1:  ll     %0, %1\n"       /* load old value */
    "    subu   %2, %0, %3\n"   /* calculate new value */
    "    sc     %2, %1\n"       /* attempt to store */
    "    beqz   %2, 1b\n"       /* spin if failed */
    : "=&r" (__val), "=m" (*valp), "=&r" (temp)
    : "r" (sub), "m" (*valp));
  /* __val is the old value, so normalize it */
  --__val;

#elif defined __i486__ || defined __i586__ || defined __i686__
  __asm__ volatile (
    "lock xaddl %0, (%1);"
    "     dec   %0;"
    : "=r" (__val)
    : "r" (valp), "0" (-0x1)
    : "cc", "memory"
    );

#elif defined __i386__ || defined __x86_64__
  __asm__ volatile (
    ".byte 0xf0, 0x0f, 0xc1, 0x02" /*lock; xaddl %eax, (%edx) */
    : "=a" (__val)
    : "0" (-1), "m" (*valp), "d" (valp)
    : "memory");
  /* __val is the pre-decrement value, so normalize it */
  --__val;

#elif defined __powerpc__ || defined __ppc__
  __asm__ volatile (
    "1:	lwarx   %0,0,%1\n"
    "	addic.   %0,%0,-1\n"
    "	dcbt    %0,%1\n"
    "	stwcx.  %0,0,%1\n"
    "	bne-    1b\n"
    "	isync\n"
    : "=&r" (__val)
    : "r" (valp)
    : "cc", "memory");

#elif defined __sparcv9__
  atomic_t __newval, __oldval = (*valp);
  do {
    __newval = __oldval - 1;
    __asm__ (
      "cas	[%4], %2, %0"
      : "=r" (__oldval), "=m" (*valp)
      : "r" (__oldval), "m" (*valp), "r"((valp)), "0" (__newval));
  } while (__newval != __oldval);
  /* The value for __val is in '__oldval' */
  __val = __oldval;

#elif (defined __ARM_ARCH && __ARM_ARCH >= 7)
  int dec = 1;
  __asm__ volatile (
    "1:"
    "dmb     ish\n"           /* Memory barrier */
    "ldrex   %0, [%1]\n"
    "sub     %0, %0,  %2\n"
    "strex   r1, %0, [%1]\n"
    "cmp     r1, #0\n"
    "bne     1b\n"
    "dmb     ish\n"           /* Memory barrier */
    : "=&r" (__val)
    : "r"(valp), "r"(dec)
    : "r1");

#elif (defined __ARM_ARCH && __ARM_ARCH == 6)
  int dec = 1;
  __asm__ volatile (
    "mcr p15, 0, %0, c7, c10, 5"  /* Memory barrier */
    : : "r"(0) : "memory");
  __asm__ volatile (
    "1:"
    "ldrex   %0, [%1]\n"
    "sub     %0, %0,  %2\n"
    "strex   r1, %0, [%1]\n"
    "cmp     r1, #0\n"
    "bne     1b\n"
    : "=&r" (__val)
    : "r"(valp), "r"(dec)
    : "r1");
  __asm__ volatile (
    "mcr p15, 0, %0, c7, c10, 5"  /* Memory barrier */
    : : "r"(0) : "memory");

#elif (defined __ARM_ARCH && __ARM_ARCH < 6)
  int tmp1, tmp2;
  int inc = -1;
  __asm__ volatile (
    "0:"
    "ldr     %0, [%3]\n"
    "add     %1, %0, %4\n"
    "swp     %2, %1, [%3]\n"
    "cmp     %0, %2\n"
    "swpne   %0, %2, [%3]\n"
    "bne     0b\n"
    : "=&r"(tmp1), "=&r"(__val), "=&r"(tmp2)
    : "r" (valp), "r"(inc)
    : "cc", "memory");

#elif defined __GNUC__
  /*
   * Don't know how to atomic decrement for a generic architecture
   * so use GCC builtin
   */
  __val = __sync_sub_and_fetch(valp, 1);

#else
#warning unknown architecture, atomic deccrement is not...
  __val = --(*valp);

#endif
  return __val;
}

#ifdef	__cplusplus
}
#endif

#endif	/* ATOMIC_H */
