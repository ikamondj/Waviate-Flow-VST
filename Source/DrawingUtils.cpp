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

void fillHexagon(juce::Graphics& g, const juce::Rectangle<float>& bounds)
{
    juce::Path hex;
    auto centre = bounds.getCentre();
    float radius = 0.5f * std::min(bounds.getWidth(), bounds.getHeight());

    hex.startNewSubPath(centre.getX() + radius, centre.getY());

    for (int k = 1; k < 6; ++k)
    {
        float angle = juce::MathConstants<float>::twoPi * (float)k / 6.0f;
        hex.lineTo(centre.getX() + radius * std::cos(angle),
            centre.getY() + radius * std::sin(angle));
    }

    hex.closeSubPath();
    g.fillPath(hex);
}

void fillStar(juce::Graphics& g, const juce::Rectangle<float>& bounds)
{
    juce::Path star;
    const auto centre = bounds.getCentre();
    const float outerR = 0.5f * std::min(bounds.getWidth(), bounds.getHeight());
    const float innerR = outerR * 0.381966011f;

    constexpr int  points = 5;
    const float    step = juce::MathConstants<float>::twoPi / points;
    const float    rotation = -juce::MathConstants<float>::halfPi; // tip up

    // First outer point
    float angleOuter = rotation;
    star.startNewSubPath(centre.x + outerR * std::cos(angleOuter),
        centre.y + outerR * std::sin(angleOuter));


    for (int i = 0; i < points; ++i)
    {
        float angleInner = rotation + i * step + step * 0.5f;
        star.lineTo(centre.x + innerR * std::cos(angleInner),
            centre.y + innerR * std::sin(angleInner));

        float angleNextOuter = rotation + (i + 1) * step;
        star.lineTo(centre.x + outerR * std::cos(angleNextOuter),
            centre.y + outerR * std::sin(angleNextOuter));
    }

    star.closeSubPath();
    g.fillPath(star);
}

