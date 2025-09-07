/*
  ==============================================================================

    NodeData.cpp
    Created: 9 Aug 2025 11:06:44pm
    Author:  ikamo

  ==============================================================================
*/

#include "NodeData.h"
#include "NodeType.h"
#include "NodeComponent.h"
#include "SceneComponent.h"
#include "PluginEditor.h"

// Attach only if index valid & no loop




NodeData::NodeData(const NodeType& type) : type(type), compileTimeSizes()
{
}


const NodeType* NodeData::getType() const
{
    return &type;
}

const bool NodeData::isSingleton(RunnerInput* inlineInstance) const
{
    return getCompileTimeSize(inlineInstance) == 1;
}

const std::map<juce::String, juce::String>& NodeData::getProperties() const noexcept { return properties; }
const juce::String NodeData::getStringProperty(const juce::String& key) const noexcept
{
	return properties.contains(key) ? properties.at(key) : "";
}
const double NodeData::getNumericProperty(const juce::String& key) const noexcept
{
	return numericProperties.contains(key) ? numericProperties.at(key) : 0.0;
}
const std::vector<double> NodeData::getCompileTimeValue(RunnerInput* inlineInstance) const noexcept
{
    std::vector<std::vector<double>> ins;
    for (int i = 0; i < getNumInputs(); i += 1) {
        auto input = getInput(i);
        std::vector<double> in;
        if (input) {
            in = input->getCompileTimeValue(inlineInstance);
        }
        else {
            in.push_back(getType()->inputs[i].defaultValue);
        }
        ins.push_back(in);
    }
    std::vector<std::span<double>> spanInputs;
    for (auto& in : ins) {
        spanInputs.push_back(std::span<double>(&in[0], in.size()));
    }
    UserInput fakeInput;
    std::vector<double> outputField;
    
    outputField.resize(getMaxOutputDimension(ins, *inlineInstance, inputIndex));
    std::span<double> outputSpan(&outputField[0], outputField.size());
    getType()->execute(*this, fakeInput, spanInputs, outputSpan, *inlineInstance);
    return outputField;
}
const std::map<juce::String, double>& NodeData::getNumericProperties() const noexcept { return numericProperties; };

void NodeData::setProperty(const juce::String& key, const juce::String& value) { properties[key] = value; }
bool NodeData::needsCompileTimeInputs() const
{
    const NodeData* prev = this;
    NodeData* outp = output;
    while (outp) {
        for (int i = 0; i < outp->getNumInputs(); i += 1) {
            if (outp->getInput(i) == this && (outp->getType()->inputs[i].requiresCompileTimeKnowledge || outp->needsCompileTimeInputs())) {
                return true;
            }
        }
        prev = outp;
        outp = outp->output;
    }
    return false;
}
void NodeData::setProperty(const juce::String& key, const double value) {
	numericProperties[key] = value;
}

juce::Point<int> NodeData::getPosition() const noexcept { return position; }

void NodeData::setPosition(juce::Point<int> newPos) noexcept { position = newPos; }

const NodeData* NodeData::getInput(size_t idx) const noexcept
{
    if (idx >= inputNodes.size()) return nullptr;
    return inputNodes[idx];
}

NodeData* NodeData::getInput(size_t idx) {
	if (idx >= inputNodes.size()) return nullptr;
	return inputNodes[idx];
}

bool NodeData::attachInput(size_t idx, NodeData* other, RunnerInput& r)
{
    auto& registry = owningComponent->getOwningScene()->processorRef.getRegistry();
    if (idx >= type.inputs.size())
        return false; // out of bounds for this node type

    auto input = type.inputs[idx];
    if (input.requiresCompileTimeKnowledge || needsCompileTimeInputs()) {
        auto otherType = other->getType();
        if (!other->isCompileTimeKnown()) {
            if (!other->getType()->isInputNode) {
                return false;
            }
        }
    }

    if (input.requiredSize > 0) {
        std::vector<std::span<double>> tempStores;
        double d[] = { 0.0 };
        std::span<double> empty(d);
        for (int i = 0; i < other->getNumInputs(); i += 1) {
            auto upup = other->getInput(i);
            if (upup) {
                tempStores.push_back(Runner::getNodeField(upup, owningComponent->getOwningScene()->processorRef.getBuildableRunner().nodeOwnership));
            } else {
                tempStores.push_back(empty);
            }
        }
        
        if (other && other->getCompileTimeSize(&r) != input.requiredSize) {
            // If the input requires a singleton, the other node must output a singleton
            return false;
        }
    }
    if (wouldIntroduceCycle(other))
        return false; // reject loops



    // Resize vector if needed
    if (inputNodes.size() < type.inputs.size())
        inputNodes.resize(type.inputs.size(), nullptr);

    inputNodes[idx] = other;
    if (other) {
        other->output = this;
    }
    owningComponent->getOwningScene()->processorRef.updateRegistryFull();
    owningComponent->getOwningScene()->onSceneChanged();
    return true;
}

void NodeData::detachInput(size_t idx)
{
    if (idx < inputNodes.size()) {
        if (inputNodes[idx]) {
            inputNodes[idx]->output = nullptr;
        }
        inputNodes[idx] = nullptr;
        owningComponent->getOwningScene()->processorRef.updateRegistryFull();
        owningComponent->getOwningScene()->onSceneChanged();
    }
        
}

#include <unordered_set>



const bool NodeData::isCompileTimeKnown() const noexcept
{
    // Node explicitly marked as runtime
    if (getType()->alwaysOutputsRuntimeData)
        return false;

    // For non-input nodes: all inputs that *require* compile-time knowledge
    // must themselves be compile-time known. if they're present.
    if (!getType()->isInputNode)
    {
        const int nin = static_cast<int>(getType()->inputs.size());
        for (int i = 0; i < nin; ++i)
        {
            const auto& inFeat = getType()->inputs[i];
 
            const NodeData* inp = getInput(i);
            if (inp && !inp->isCompileTimeKnown()) return false; // required but not CT-known
        }
        return true; // all required inputs are compile-time known
    }

    return needsCompileTimeInputs();
}




 const int NodeData::getNumInputs() const noexcept
 {
     return getType()->inputs.size();
 }

 const int NodeData::getMaxOutputDimension(const std::vector<std::vector<double>>& inputs, RunnerInput& context, int inputNum) const noexcept
 {
	 std::vector<NodeData*> upstreams;
	 for (auto* in : inputNodes)
	 {
	    upstreams.push_back(in);
	 }
     auto type = getType();
     int result = type->getOutputSize(upstreams,inputs,context, inputNum, *this);
     return result;
 }



 bool NodeData::compileTimeSizeReady(RunnerInput* inlineInstance) const
 {
     return this->compileTimeSizes.contains(inlineInstance);
 }


 int NodeData::getCompileTimeSize(RunnerInput* inlineInstance) const
 {
     if (this->compileTimeSizes.contains(inlineInstance)) {
         return compileTimeSizes.at(inlineInstance);
     }
     else {
         throw std::logic_error("Node has not been compiled at this time!");
     }
 }

 void NodeData::setCompileTimeSize(RunnerInput* inlineInstance, int s)
 {
     compileTimeSizes.insert_or_assign(inlineInstance, s);
 }


 void NodeData::markUncompiled(RunnerInput* inlineInstance)
 {
     if (compileTimeSizes.contains(inlineInstance)){
         compileTimeSizes.erase(inlineInstance);
     }
 }

bool NodeData::wouldIntroduceCycle(NodeData* candidate) const
{
    if (!candidate) return false;
    return dfsHasPath(candidate, this);
}

bool NodeData::dfsHasPath(const NodeData* start, const NodeData* target)
{
    if (start == target) return true;
    for (auto* in : start->inputNodes)
        if (in && dfsHasPath(in, target))
            return true;
    return false;
}
