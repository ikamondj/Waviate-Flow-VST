/*
  ==============================================================================

    NodeData.h
    Created: 9 Aug 2025 11:06:44pm
    Author:  ikamo

  ==============================================================================
*/
#pragma once
#include <vector>
#include <map>
#include <string>
#include <span>
#include <JuceHeader.h>
#include "InputType.h"
#include "ddtype.h"
#include "RunnerInput.h"

class NodeData
{
public:
    NodeData(const NodeData& other);
	NodeData(const struct NodeType& type);

    const struct NodeType* getType() const;

    const bool isSingleton(class RunnerInput* inlineInstance) const;

    const std::map<std::string, std::string>& getProperties() const noexcept;
	const std::string getStringProperty(const std::string& key) const noexcept;
	const double getNumericProperty(const std::string& key) const noexcept;

    const std::vector<ddtype> getCompileTimeValue(class RunnerInput* inlineInstance, UserInput& fakeInput) const noexcept;
    const std::map<std::string, double>& getNumericProperties() const noexcept;
    void setProperty(const std::string& key, const std::string& value);
    bool needsCompileTimeInputs() const;
    void setProperty(const std::string& key, const double value);

    juce::Point<int> getPosition() const noexcept;
    void setPosition(juce::Point<int> newPos) noexcept;

    const NodeData* getInput(size_t idx) const noexcept;

    NodeData* getInput(size_t idx);

    std::unique_ptr<RunnerInput> optionalRunnerInput = nullptr;

    // Attach only if index valid & no loop
    bool attachInput(size_t idx, NodeData* other, class RunnerInput& inlineInstance, class SceneData* referenceScene, bool updateScene = true);

    void detachInput(size_t idx, SceneData* referenceScene, bool updateScene = true);

	const bool isCompileTimeKnown() const noexcept;

    const int getNumInputs() const noexcept;

    const int getMaxOutputDimension(const std::vector<std::vector<ddtype>>&, class RunnerInput& context, int inputNum) const noexcept;

    bool compileTimeSizeReady(const class RunnerInput* inlineInstance) const;
    int getCompileTimeSize(const RunnerInput* inlineInstance) const;
    void setCompileTimeSize(const RunnerInput* inlineInstance, int s);
    void markUncompiled(const RunnerInput* inlineInstance);
    std::vector<NodeData*> inputNodes;
    std::vector<ddtype> optionalStoredAudio;
    std::vector<ddtype> defaultValues;
    std::set<std::tuple<NodeData*, int>> outputs;
    enum class InputType getTrueType() const;
    void markWildCardTypesDirty();
    bool computeWildCardTypes(std::unordered_set<NodeData*>& visited);
	void setTrueType(InputType t);
    int inputIndex = -1;
    bool isCopy = false;
private:
    InputType trueType;
    
    std::map<std::string, std::string> properties;
	std::map<std::string, double> numericProperties;
    juce::Point<int> position;
    const NodeType& type;
   
    std::optional<int> compileTimeSize;
    std::unordered_map<const class RunnerInput*, int> compileTimeSizes;

    bool wouldIntroduceCycle(NodeData* candidate) const;

    static bool dfsHasPath(const NodeData* start, const NodeData* target);
};
