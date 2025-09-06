/*
  ==============================================================================

    DrawingUtils.cpp
    Created: 11 Aug 2025 9:49:03pm
    Author:  ikamo

  ==============================================================================
*/

#include "DrawingUtils.h"


void drawGradientBezier(juce::Graphics& g, juce::Point<float> start, juce::Point<float> control1, juce::Point<float> control2, juce::Point<float> end, juce::Colour startColour, juce::Colour endColour, float thickness, int segments)
{
    // Helper lambda to calculate cubic Bezier point at t
    auto bezierPoint = [&](float t) -> juce::Point<float>
        {
            float u = 1.0f - t;
            float tt = t * t;
            float uu = u * u;
            float uuu = uu * u;
            float ttt = tt * t;

            return start * uuu
                + control1 * (3 * uu * t)
                + control2 * (3 * u * tt)
                + end * ttt;
        };

    juce::Point<float> prev = bezierPoint(0.0f);

    for (int i = 1; i <= segments; ++i)
    {
        float t = (float)i / (float)segments;
        juce::Point<float> p = bezierPoint(t);

        juce::Colour c = startColour.interpolatedWith(endColour, t);
        g.setColour(c);
        g.drawLine(prev.x, prev.y, p.x, p.y, thickness);

        prev = p;
    }
}


bool lineIntersectsBezier(const juce::Line<float>& dragLine,
    const juce::Point<float>& start,
    const juce::Point<float>& c1,
    const juce::Point<float>& c2,
    const juce::Point<float>& end)
{
    constexpr int resolution = 32; // higher = more accurate
    juce::Point<float> prev = start;

    for (int i = 1; i <= resolution; ++i)
    {
        float t = (float)i / (float)resolution;
        auto pt = cubicBezierPoint(start, c1, c2, end, t);

        juce::Line<float> seg(prev, pt);
        if (seg.intersects(dragLine))
            return true;

        prev = pt;
    }
    return false;
}

juce::Point<float> cubicBezierPoint(const juce::Point<float>& p0, const juce::Point<float>& p1, const juce::Point<float>& p2, const juce::Point<float>& p3, float t)
{
    float u = 1.0f - t;
    float tt = t * t;
    float uu = u * u;
    float uuu = uu * u;
    float ttt = tt * t;

    juce::Point<float> p =
        p0.toFloat() * uuu +                  // (1-t)^3 * P0
        p1.toFloat() * (3 * uu * t) +         // 3(1-t)^2 * t * P1
        p2.toFloat() * (3 * u * tt) +         // 3(1-t) * t^2 * P2
        p3.toFloat() * ttt;                   // t^3 * P3

    return p;
}
