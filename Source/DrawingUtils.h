/*
  ==============================================================================

    DrawingUtils.h
    Created: 11 Aug 2025 9:49:03pm
    Author:  ikamo

  ==============================================================================
*/

#pragma once
#pragma once
#include <JuceHeader.h>

// Draws a cubic Bezier curve with a gradient from startColour to endColour
void drawGradientBezier(juce::Graphics& g,
    juce::Point<float> start,
    juce::Point<float> control1,
    juce::Point<float> control2,
    juce::Point<float> end,
    juce::Colour startColour,
    juce::Colour endColour,
    float thickness = 2.0f,
    int segments = 30); // Higher = smoother gradient

bool lineIntersectsBezier(const juce::Line<float>& dragLine,
    const juce::Point<float>& start,
    const juce::Point<float>& c1,
    const juce::Point<float>& c2,
    const juce::Point<float>& end);

juce::Point<float> cubicBezierPoint(const juce::Point<float>& p0,
    const juce::Point<float>& p1,
    const juce::Point<float>& p2,
    const juce::Point<float>& p3,
    float t);


void fillHexagon(juce::Graphics& g, const juce::Rectangle<float>& bounds);

void fillStar(juce::Graphics& g, const juce::Rectangle<float>& bounds);
