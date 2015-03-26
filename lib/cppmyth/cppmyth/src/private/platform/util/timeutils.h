#pragma once
/*
 * This file is part of the libCEC(R) library.
 *
 * libCEC(R) is Copyright (C) 2011-2012 Pulse-Eight Limited.  All rights reserved.
 * libCEC(R) is an original work, containing original code.
 *
 * libCEC(R) is a trademark of Pulse-Eight Limited.
 *
 * This program is dual-licensed; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 *
 * Alternatively, you can license this library under a commercial license,
 * please contact Pulse-Eight Licensing for more information.
 *
 * For more information contact:
 * Pulse-Eight Licensing       <license@pulse-eight.com>
 *     http://www.pulse-eight.com/
 *     http://www.pulse-eight.net/
 */

#include "../os.h"

#if defined(__APPLE__)
#include <sys/time.h>
#include <mach/mach_time.h>
#elif defined(__WINDOWS__)
#include <time.h>
#else
#include <sys/time.h>
#endif

namespace Myth
{
namespace PLATFORM
{
  inline int64_t GetTimeMs()
  {
  #if defined(__APPLE__)
    // Recommended by Apple's QA1398.
    int64_t ticks = 0;
    static mach_timebase_info_data_t timebase;
    // Get the timebase if this is the first time we run.
    if (timebase.denom == 0)
      (void)mach_timebase_info(&timebase);
    // Use timebase to convert absolute time tick units into nanoseconds.
    ticks = mach_absolute_time() * timebase.numer / timebase.denom;
    return ticks / 1000000;
  #elif defined(__WINDOWS__)
    LARGE_INTEGER tickPerSecond;
    LARGE_INTEGER tick;
    if (QueryPerformanceFrequency(&tickPerSecond))
    {
      QueryPerformanceCounter(&tick);
      return (int64_t) (tick.QuadPart / (tickPerSecond.QuadPart / 1000.));
    }
    return -1;
  #else
    timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return (int64_t)time.tv_sec * 1000 + time.tv_nsec / 1000000;
  #endif
  }

  template <class T>
  inline T GetTimeSec()
  {
    return (T)GetTimeMs() / (T)1000.0;
  }

  class CTimeout
  {
  public:
    CTimeout(void) : m_iTarget(0) {}
    CTimeout(uint32_t iTimeout) { Init(iTimeout); }

    bool IsSet(void) const       { return m_iTarget > 0; }
    void Init(uint32_t iTimeout) { m_iTarget = GetTimeMs() + iTimeout; }

    uint32_t TimeLeft(void) const
    {
      uint64_t iNow = GetTimeMs();
      return (iNow > m_iTarget) ? 0 : (uint32_t)(m_iTarget - iNow);
    }

  private:
    uint64_t m_iTarget;
  };
}
}
