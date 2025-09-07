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

uint64_t NodeType::getNodeUserID() const
{
    return NodeID >> 16;
}

uint16_t NodeType::getNodeId() const
{
    return static_cast<uint16_t>(NodeID & static_cast<uint64_t>(0xFFFFL));
}

uint64_t NodeType::getNodeFullID() const
{
    return NodeID;
}
