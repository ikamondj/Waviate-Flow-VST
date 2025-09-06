/*
  ==============================================================================

    NodeType.cpp
    Created: 9 Aug 2025 11:06:32pm
    Author:  ikamo

  ==============================================================================
*/

#include "NodeType.h"
#include "NodeComponent.h"
#include "UserInput.h"
#include "SceneComponent.h"


InputFeatures::InputFeatures(const juce::String& n, bool isBool, int requiredSize, bool reqCompileTime)
    : name(n), isBoolean(isBool), requiredSize(requiredSize), requiresCompileTimeKnowledge(reqCompileTime) {
}






