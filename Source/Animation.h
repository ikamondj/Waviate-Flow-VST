/*
  ==============================================================================

    Animation.h
    Created: 17 Sep 2025 9:37:07pm
    Author:  ikamo

  ==============================================================================
*/

#pragma once
#include <cmath>
#include <algorithm>
#include <type_traits>

template <typename T>
T smoothDamp(T current, T target, T& currentVelocity,
    T smoothTime, T deltaTime, T maxSpeed = std::numeric_limits<T>::infinity())
{
    static_assert(std::is_floating_point<T>::value, "smoothDamp requires float or double");

    // Prevent divide by zero
    smoothTime = std::max(static_cast<T>(0.0001), smoothTime);

    T omega = static_cast<T>(2.0) / smoothTime;

    T x = omega * deltaTime;
    T exp = static_cast<T>(1.0) / (static_cast<T>(1.0) + x + static_cast<T>(0.48) * x * x + static_cast<T>(0.235) * x * x * x);

    T change = current - target;
    T originalTarget = target;

    // Clamp maximum speed
    T maxChange = maxSpeed * smoothTime;
    if (std::fabs(change) > maxChange)
        change = (change > 0 ? maxChange : -maxChange);

    target = current - change;

    T temp = (currentVelocity + omega * change) * deltaTime;
    currentVelocity = (currentVelocity - omega * temp) * exp;

    T output = target + (change + temp) * exp;

    // Prevent overshoot
    if ((originalTarget - current > 0.0) == (output > originalTarget))
    {
        output = originalTarget;
        currentVelocity = (output - originalTarget) / deltaTime;
    }

    return output;
}
