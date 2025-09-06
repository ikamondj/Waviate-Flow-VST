/*
  ==============================================================================

    CircleBuffer.h
    Created: 2 Sep 2025 8:49:14pm
    Author:  ikamo

  ==============================================================================
*/
#include <deque>
#pragma once
class CircleBuffer {
    std::deque<double> backingBuffer;
    int maxSize = 480000;
public:
    const double& operator[](size_t index) const;
    void add(double value);
	size_t size() const;
};