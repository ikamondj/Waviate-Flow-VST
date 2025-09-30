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
#include "PluginEditor.h"


std::unordered_map<uint64_t, const NodeType*> typeLookup;

InputFeatures::InputFeatures(const juce::String& n, InputType itype, int requiredSize, bool reqCompileTime)
    : name(n), requiredSize(requiredSize), requiresCompileTimeKnowledge(reqCompileTime), inputType(itype) {
}

void operateNodeDataChange(const NodeType& t, NodeComponent& n) {
    if (t._nodeDataChanged(n))
    {
        if (auto scene = n.getOwningScene()) {
            scene->onSceneChanged();
        }
    }
}

void NodeType::nodeDataChanged(NodeComponent& n) const { //call from UI changes of the node's internal state
    auto jobVersion = n.bumpVersion();
    auto scene = n.getOwningScene();
    if (!scene) {
        operateNodeDataChange(*this, n);
    }
    else {
        auto processor = scene->processorRef;
        if (!processor) {
            operateNodeDataChange(*this, n);
        }
        else {
            auto editor = processor->getCurrentEditor();
            if (!editor) {
                operateNodeDataChange(*this, n);
            }
            else {
                auto weakComp = juce::Component::SafePointer(&n);
                editor->getThreadPool().addJob([&]() 
                {
                    // background thread
                    if (_nodeDataChanged(*weakComp))
                    {
                        // schedule back on GUI thread
                        juce::MessageManager::callAsync([weakComp, jobVersion]()
                            {
                                if (weakComp != nullptr && weakComp->currentVersion() == jobVersion)
                                {
                                    if (auto* scene = weakComp->getOwningScene())
                                        scene->onSceneChanged();
                                }
                            });
                    }
                }); 
            }
        }
    }

    
}

uint64_t NodeType::getNodeUserID() const
{
    return NodeID >> 16;
}

uint64_t NodeType::getNodeId() const
{
    return static_cast<uint16_t>(NodeID & static_cast<uint64_t>(0xFFFFL));
}

std::array<uint64_t,2> NodeType::getNodeFullID() const
{
    return { UserID, NodeID };
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

void NodeType::setNodeId(uint64_t userId, uint64_t nodeId) {
    NodeID = nodeId;
    UserID = userId;
}

NodeType::NodeType(uint64_t userId, uint64_t nodeId, WaviateFlow2025AudioProcessor& processor)
{
    std::string name; //TODO get name
    fromScene = new SceneComponent(processor, name);
    ownsScene = true;
    this->NodeID = nodeId;
    this->UserID = userId;
}

NodeType::NodeType(uint64_t nodeId) {
    this->setNodeId(0, nodeId + registryCreatePrefix);
}



NodeType::~NodeType()
{
    if (ownsScene) {
        delete fromScene;
    }
}
