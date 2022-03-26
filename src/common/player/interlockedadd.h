#pragma once

#include <atomic>

inline void InterlockedAdd(std::atomic<double>& clock, double correction)
{
    for (double v = clock; !clock.compare_exchange_weak(v, v + correction);)
    {
    }
}
