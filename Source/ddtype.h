/*
  ==============================================================================

    ddtype.h
    Created: 8 Sep 2025 3:42:31pm
    Author:  ikamo

  ==============================================================================
*/

#pragma once
#include <stdint.h>
union ddtype {
    double d;
    int64_t i;
    constexpr ddtype(double x) : d(x) {}
    constexpr ddtype(int64_t x) : i(x) {}
    constexpr ddtype(bool b) : i(b ? 1 : 0) {}
    ddtype() : d(0.0) {}
};