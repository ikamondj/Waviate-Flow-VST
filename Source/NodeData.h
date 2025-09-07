/*
  ==============================================================================

    NodeData.h
    Created: 9 Aug 2025 11:06:44pm
    Author:  ikamo

  ==============================================================================
*/
#include <vector>
#include <map>
#include <string>
#include <span>
#include <JuceHeader.h>
#pragma once

class NodeData
{
public:
	NodeData(const struct NodeType& type);

    const struct NodeType* getType() const;

    const bool isSingleton(class RunnerInput* inlineInstance) const;

    const std::map<juce::String, juce::String>& getProperties() const noexcept;
	const juce::String getStringProperty(const juce::String& key) const noexcept;
	const double getNumericProperty(const juce::String& key) const noexcept;

    const std::vector<double> getCompileTimeValue(class RunnerInput* inlineInstance) const noexcept;
    const std::map<juce::String, double>& getNumericProperties() const noexcept;
    void setProperty(const juce::String& key, const juce::String& value);
    bool needsCompileTimeInputs() const;
    void setProperty(const juce::String& key, const double value);

    juce::Point<int> getPosition() const noexcept;
    void setPosition(juce::Point<int> newPos) noexcept;

    const NodeData* getInput(size_t idx) const noexcept;

    NodeData* getInput(size_t idx);

    // Attach only if index valid & no loop
    bool attachInput(size_t idx, NodeData* other, class RunnerInput& inlineInstance);

    void detachInput(size_t idx);

	const bool isCompileTimeKnown() const noexcept;

    const int getNumInputs() const noexcept;

    const int getMaxOutputDimension(const std::vector<std::vector<double>>&, class RunnerInput& context, int inputNum) const noexcept;

    class NodeComponent* owningComponent = nullptr;
    bool compileTimeSizeReady(class RunnerInput* inlineInstance) const;
    int getCompileTimeSize(RunnerInput* inlineInstance) const;
    void setCompileTimeSize(RunnerInput* inlineInstance, int s);
    void markUncompiled(RunnerInput* inlineInstance);
    std::vector<NodeData*> inputNodes;
    NodeData* output = nullptr;
    int inputIndex = -1;
private:
	
    
    std::map<juce::String, juce::String> properties;
	std::map<juce::String, double> numericProperties;
    juce::Point<int> position;
    const NodeType& type;
   
    std::optional<int> compileTimeSize;
    std::unordered_map<class RunnerInput*, int> compileTimeSizes;

    bool wouldIntroduceCycle(NodeData* candidate) const;

    static bool dfsHasPath(const NodeData* start, const NodeData* target);
};
