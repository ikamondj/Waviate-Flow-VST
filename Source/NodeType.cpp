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
#include "PluginProcessor.h"
#include "SceneComponent.h"


std::unordered_map<uint64_t, const NodeType*> typeLookup;

InputFeatures::InputFeatures(const juce::String& n, InputType itype, int requiredSize, bool reqCompileTime)
    : name(n), requiredSize(requiredSize), requiresCompileTimeKnowledge(reqCompileTime), inputType(itype) {
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

const NodeType* NodeType::getTypeByNodeID(uint64_t fullId)
{
    if (typeLookup.contains(fullId)) {
        return typeLookup.at(fullId);
    }
    return nullptr;
}

void NodeType::putIdLookup(const NodeType& t) {
    if (typeLookup.contains(t.NodeID)) {
		jassertfalse; // duplicate ID
    }
    typeLookup.insert({ t.NodeID, &t });
}

void NodeType::setNodeId(uint64_t userId, uint16_t nodeId) {
    NodeID = userId << 16 | nodeId;
}

NodeType::NodeType(uint64_t fullId, WaviateFlow2025AudioProcessor& processor)
{
    juce::String name;
    fromScene = new SceneComponent(processor, name);
    ownsScene = true;
    this->NodeID = fullId;
}

NodeType::NodeType(uint16_t nodeId) {
    this->setNodeId(0, nodeId);
}



NodeType::~NodeType()
{
    if (ownsScene) {
        delete fromScene;
    }
}
