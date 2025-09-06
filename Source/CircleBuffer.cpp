/*
  ==============================================================================

    CircleBuffer.cpp
    Created: 2 Sep 2025 8:49:14pm
    Author:  ikamo

  ==============================================================================
*/

#include "CircleBuffer.h"
const double& CircleBuffer::operator[](size_t index) const {
	if (index >= backingBuffer.size()) {
        return 0.0;
	}
    return backingBuffer[index];
}

void CircleBuffer::add(double value) {
    if (backingBuffer.size() >= maxSize) {
        backingBuffer.pop_back();
    }
    backingBuffer.push_front(value);
}

size_t CircleBuffer::size() const {
    return backingBuffer.size();
}
