#pragma once
#include <functional>
#include <string>
#include <vector>
#include <JuceHeader.h>
#include <span>
#include "NodeData.h"

struct InputFeatures {
    /// <summary>
    /// Constructor for InputFeatures.
    /// </summary>
    /// <param name="n">Name of the input feature.</param>
    /// <param name="isBool">Whether the input is a boolean type.</param>
    /// <param name="requiredSize">Whether the input requires specific size vector input.</param>
    /// <param name="reqCompileTime">Whether the input requires compile-time knowledge.</param>
	InputFeatures(const juce::String& n, bool isBool, int requiredSize, bool reqCompileTime);
    juce::String name;
    bool isBoolean;
    int requiredSize;
    bool requiresCompileTimeKnowledge;
    double defaultValue = 0.0;
    NodeComponent* optionalInputComponent;
};

class orableBool {
    bool value = false;
public:
    void setTrue() { value = true; };
	void returnToFalseException() { value = false; };
    //convert to bool implicitly:
	operator bool() const { return value; }
};

struct NodeType {
    juce::String name;
    juce::String address;
    juce::String tooltip;
    std::vector<InputFeatures> inputs;
    using ExecuteFn = void(*)(const NodeData& node, class UserInput& userInput, const std::vector<std::span<double>>& inputs, std::span<double>& output, class RunnerInput& inlineInstance);
    ExecuteFn execute;
    std::function<void(class NodeComponent&, NodeData&)> buildUI;
    std::function<int(const std::vector<NodeData*>& inputNodes, const std::vector<std::vector<double>>& inputs, class RunnerInput& inlineInstance, int inputNum)> getOutputSize;
    std::function<void(NodeComponent&)> onResized = [](NodeComponent&) {};
    bool isSingleton(const NodeData* node, const std::vector<std::span<double>>& inputs) const;
    bool isBoolean = false;
    bool alwaysOutputsRuntimeData = false;
    class SceneComponent* fromScene = nullptr;
    bool isInputNode = false;
    uint64_t NodeID;
    uint64_t getNodeUserID() const;
    uint16_t getNodeId() const;
    uint64_t getNodeFullID() const;
    void setNodeId(uint64_t userId, uint16_t nodeId);
};

