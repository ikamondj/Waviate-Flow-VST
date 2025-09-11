#pragma once
#include <functional>
#include <string>
#include <vector>
#include <JuceHeader.h>
#include <span>
#include "NodeData.h"
#include "InputType.h"
#include "ddtype.h"


struct InputFeatures {
    /// <summary>
    /// Constructor for InputFeatures.
    /// </summary>
    /// <param name="n">Name of the input feature.</param>
    /// <param name="isBool">Whether the input is a boolean type.</param>
    /// <param name="requiredSize">Whether the input requires specific size vector input.</param>
    /// <param name="reqCompileTime">Whether the input requires compile-time knowledge.</param>
	InputFeatures(const juce::String& n, InputType, int requiredSize, bool reqCompileTime);
    juce::String name;
    InputType inputType = InputType::decimal;
    bool isBoolean();
    bool isInt();
    int requiredSize;
    bool requiresCompileTimeKnowledge;
    ddtype defaultValue = 0.0;
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
    using ExecuteFn = void(*)(const NodeData& node, class UserInput& userInput, const std::vector<std::span<ddtype>>& inputs, std::span<ddtype> output, const class RunnerInput& inlineInstance);
    ExecuteFn execute;
    std::function<void(class NodeComponent&, NodeData&)> buildUI;
    std::function<int(const std::vector<NodeData*>& inputNodes, const std::vector<std::vector<ddtype>>& inputs, const class RunnerInput& inlineInstance, int inputNum, const NodeData& self)> getOutputSize;
    std::function<void(NodeComponent&)> onResized = [](NodeComponent&) {};
    bool isSingleton(const NodeData* node, const std::vector<std::span<ddtype>>& inputs) const;
    InputType outputType = InputType::decimal;
    bool alwaysOutputsRuntimeData = false;
    class SceneData* fromScene = nullptr;
    bool isInputNode = false;
    uint64_t NodeID;
    uint64_t UserID;
    uint64_t getNodeUserID() const;
    uint64_t getNodeId() const;
    std::array<uint64_t, 2> getNodeFullID() const;
    static const NodeType* getTypeByNodeID(uint64_t fullId);
    static void putIdLookup(const NodeType& t);
    void setNodeId(uint64_t userId, uint64_t nodeId);
    bool ownsScene = false;
    NodeType(uint64_t nodeId);
    NodeType(uint64_t userId, uint64_t nodeId, class WaviateFlow2025AudioProcessor&);
    int whichInputToFollowWildcard = -1;
	static NodeType& getConversionType(InputType from, InputType to);
    ~NodeType();
};

