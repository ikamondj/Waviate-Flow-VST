#pragma once
#include <JuceHeader.h>
#include <functional>
#include <string>
#include <vector>
#include <span>
#include <limits>
#include "NodeData.h"
#include "InputType.h"
#include "ddtype.h"
#include "UserInput.h"


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
    double minVal = -std::numeric_limits<double>::infinity();
    double maxVal = std::numeric_limits<double>::infinity();
    InputType inputType = InputType::decimal;
    int requiredSize;
    bool requiresCompileTimeKnowledge;
    ddtype defaultValue = 0.0;
};

struct GlobalClangVar {
    bool isStatic;
    std::string type;
    std::string varName;
};

struct NodeType {
    juce::String name;
    juce::String address;
    juce::String tooltip;
    std::vector<InputFeatures> inputs;
    using ExecuteFn = void(*)(const NodeData& node, UserInput& userInput, const std::vector<std::span<ddtype>>& inputs, std::span<ddtype> output, const class RunnerInput& inlineInstance);
    ExecuteFn execute;
    std::function<void(class NodeComponent&, NodeData&)> buildUI;
    std::function<int(const std::vector<NodeData*>& inputNodes, const std::vector<std::vector<ddtype>>& inputs, const class RunnerInput& inlineInstance, int inputNum, const NodeData& self)> getOutputSize;
    std::function<void(NodeComponent&)> onResized = [](NodeComponent&) {};
    /* returns true if the change in state should update the compiled state */
    std::function<bool(NodeComponent&)> _nodeDataChanged = [](NodeComponent&) { return false; }; // define this to be how the state gets modified. capture UI changes in the lambda cap to make the proper change
    void nodeDataChanged(NodeComponent& n) const; // call this function whenever UI changes the node's internal state. such as web audio got downloaded and cached in the node
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
    std::function<std::string(NodeData&, int uniqueNodeOrder)> emitCode;
    std::function<std::vector<GlobalClangVar>(NodeData&, int uniqueNodeOrder)> globalVarNames;
    bool ownsScene = false;
    NodeType(uint64_t nodeId);
    NodeType(uint64_t userId, uint64_t nodeId, class WaviateFlow2025AudioProcessor&);
    int whichInputToFollowWildcard = -1;
    inline static uint64_t registryCreatePrefix;
	static NodeType& getConversionType(InputType from, InputType to);
    std::function<void(class NodePropertiesComponent& npc)> setupPropertiesUI = [](class NodePropertiesComponent& npc) {};
    ~NodeType();
};

