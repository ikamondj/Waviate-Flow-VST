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
#include <unordered_set>
// Attach only if index valid & no loop






NodeData::NodeData(const NodeType& type) : type(type), compileTimeSizes()
{
    if (type.fromScene) {
		optionalRunnerInput = std::make_unique<RunnerInput>();
    }
    defaultValues.resize(type.inputs.size());
    for (int i = 0; i < type.inputs.size(); i += 1) {
        defaultValues[i] = type.inputs[i].defaultValue;
    }
}

NodeData::NodeData(const NodeData& other) : NodeData(other.type) {
	properties = other.properties;
	numericProperties = other.numericProperties;
	position = other.position;
	inputNodes = other.inputNodes;
	outputs = other.outputs;
	compileTimeSizes = other.compileTimeSizes;
	compileTimeSize = other.compileTimeSize;
	inputIndex = other.inputIndex;
	trueType = other.trueType;
    optionalStoredAudio = other.optionalStoredAudio;
    defaultValues = other.defaultValues;
	isCopy = true;
}


const NodeType* NodeData::getType() const
{
    return &type;
}

const bool NodeData::isSingleton(RunnerInput* inlineInstance) const
{
    return getCompileTimeSize(inlineInstance) == 1;
}

const std::map<std::string, std::string>& NodeData::getProperties() const noexcept { return properties; }
const std::string NodeData::getStringProperty(const std::string& key) const noexcept
{
	return properties.contains(key) ? properties.at(key) : "";
}
const double NodeData::getNumericProperty(const std::string& key) const noexcept
{
	return numericProperties.contains(key) ? numericProperties.at(key) : 0.0;
}
const std::vector<ddtype> NodeData::getCompileTimeValue(RunnerInput* inlineInstance, UserInput& fakeInput) const noexcept
{
    std::vector<std::vector<ddtype>> ins;
    ins.resize(getNumInputs());

    // explicit stack frame struct
    struct Frame {
        const NodeData* node;
        int inputIndex;
    };

    std::vector<Frame> stack;

    // populate stack with this node's inputs
    for (int i = 0; i < getNumInputs(); i++) {
        auto input = getInput(i);
        if (input) {
            stack.push_back({ input, i });
        }
        else {
            ins[i].push_back(defaultValues[i]);
        }
    }

    // iterative DFS
    while (!stack.empty()) {
        Frame frame = stack.back();
        stack.pop_back();

        const NodeData* node = frame.node;
        int idx = frame.inputIndex;

        // check if already computed
        if (ins[idx].empty()) {
            // compute node’s value iteratively
            std::vector<std::vector<ddtype>> localIns;
            localIns.resize(node->getNumInputs());

            std::vector<int> childIndices;
            for (int j = 0; j < node->getNumInputs(); j++) {
                auto child = node->getInput(j);
                if (child) {
                    if (localIns[j].empty()) {
                        stack.push_back({ node, idx }); // reprocess parent later
                        stack.push_back({ child, j });  // compute child first
                        goto nextIteration;           // delay processing
                    }
                }
                else {
                    localIns[j].push_back(node->defaultValues[j]);
                }
            }

            // compute node output
            {
                std::vector<std::span<ddtype>> spanInputs;
                for (auto& in : localIns) {
                    spanInputs.push_back(std::span<ddtype>(&in[0], in.size()));
                }
                std::vector<ddtype> outputField;
                outputField.resize(node->getMaxOutputDimension(localIns, *inlineInstance, inputIndex));
                std::span<ddtype> outputSpan(&outputField[0], outputField.size());
                node->getType()->execute(*node, fakeInput, spanInputs, outputSpan, *inlineInstance);
                ins[idx] = std::move(outputField);
            }
        }

    nextIteration:;
    }

    std::vector<std::span<ddtype>> spanInputs;
    for (auto& in : ins) {
        spanInputs.push_back(std::span<ddtype>(&in[0], in.size()));
    }
    std::vector<ddtype> outputField;
    outputField.resize(getMaxOutputDimension(ins, *inlineInstance, inputIndex));
    std::span<ddtype> outputSpan(&outputField[0], outputField.size());
    getType()->execute(*this, fakeInput, spanInputs, outputSpan, *inlineInstance);
    return outputField;
}

const std::map<std::string, double>& NodeData::getNumericProperties() const noexcept { return numericProperties; };

void NodeData::setProperty(const std::string & key, const std::string& value) { properties[key] = value; }

bool dfsNCTI(
    const NodeData* node,
    std::unordered_set<const NodeData*>& visiting,
    std::unordered_map<const NodeData*, bool>& memo)
{
    if (!node) return false;


    auto mit = memo.find(node);
    if (mit != memo.end())
        return mit->second;


    if (!visiting.insert(node).second)
        return false;

    bool result = false;


    for (const auto& [child, index] : node->outputs) {
        if (!child) continue;


        if (child->getInput(index) == node) {

            if (child->getType()->inputs[index].requiresCompileTimeKnowledge) {
                result = true;
            }
            else if (dfsNCTI(child, visiting, memo)) {
                result = true;
            }
        }
        
        if (result) break;
    }

    visiting.erase(node);
    memo[node] = result;
    return result;
}

bool NodeData::needsCompileTimeInputs() const
{
std::unordered_set<const NodeData*> visiting;
std::unordered_map<const NodeData*, bool> memo;

return dfsNCTI(this, visiting, memo);
}

void NodeData::setProperty(const std::string& key, const double value) {
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

bool NodeData::attachInput(size_t idx, NodeData* other, RunnerInput& r, SceneData* referenceScene, bool updateScene)
{
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
        std::vector<std::span<ddtype>> tempStores;
        ddtype d[] = { 0.0 };
        std::span<ddtype> empty(d);
        for (int i = 0; i < other->getNumInputs(); i += 1) {
            auto upup = other->getInput(i);
            if (upup) {
                tempStores.push_back(Runner::getNodeField(upup, referenceScene->nodeOwnership));
            } else {
                tempStores.push_back(empty);
            }
        }

       
        
        if (other) {
            if (other )
            if (other->getCompileTimeSize(&r) != input.requiredSize) {
                return false;
            }
        }
        
    }
    bool typeMismatch = false;
    InputType from, to;
    InputType it = input.inputType;
    auto ref = dynamic_cast<SceneComponent*>(referenceScene);
    if (other) {
        InputType ot = other->type.outputType;
        if (it == InputType::any) {
            auto trueType = getTrueType();
			auto oTrueType = other->getTrueType();
            if (trueType == InputType::dirty) {
                return false;
            }
            else if (ot == InputType::followsInput) {
                if (oTrueType != trueType && trueType != InputType::any && oTrueType != InputType::any) {
                    to = trueType;
                    from = oTrueType;
                }
            }
            else if (ot != trueType && trueType != InputType::any) {
				to = trueType;
				from = ot;
            }
        }
        else if (it != ot) {
            if (ot == InputType::followsInput) {
                auto trueType = other->getTrueType();
                if (trueType == InputType::dirty) {
                    return false;
                }
				else if (trueType != it && trueType != InputType::any) {
					from = trueType;
					to = it;
					typeMismatch = true;
				}
            }
            else {
                from = ot;
                to = it;
                typeMismatch = true;
            }
        }
    }
    if (wouldIntroduceCycle(other))
        return false; // reject loops



    // Resize vector if needed
    if (inputNodes.size() < type.inputs.size())
        inputNodes.resize(type.inputs.size(), nullptr);

    //if (typeMismatch) {
    //    NodeType& typeToAdd = NodeType::getConversionType(from, to);
	//    auto thisPos = this->getPosition();
	//    auto otherPos = other->getPosition();
	//    auto avgPos = (thisPos + otherPos) / 2;
    //
    //    if (ref) {
    //        NodeComponent& nc = ref->addNode(typeToAdd, avgPos);
    //        NodeData& nd = nc.getNodeData();
    //        inputNodes[idx] = &nd;
    //        auto t = std::tuple<NodeData*, int>(this, idx);
    //        if (!other->outputs.contains(t)) {
    //            other->outputs.insert(t);
    //        }
    //        nd.inputNodes.push_back(other);
    //        auto t2 = std::tuple<NodeData*, int>(&nd, 0);
    //        if (!other->outputs.contains(t2)) {
    //            other->outputs.insert(t2);
    //        }
    //    }
    //}
    //else {
        inputNodes[idx] = other;
        if (other) {
            auto t = std::tuple<NodeData*, int>(this, idx);
            if (!other->outputs.contains(t)) {
                other->outputs.insert(t);
            }
        }
    //}

    if (ref && updateScene) {
        ref->onSceneChanged();
    }
    
    return true;
}

void NodeData::detachInput(size_t idx, SceneData* referenceScene, bool updateScene)
{
    if (idx < inputNodes.size()) {
        if (inputNodes[idx]) {
            auto t = std::tuple<NodeData*, int>(this, idx);
            if (inputNodes[idx]->outputs.contains(t)) {
                inputNodes[idx]->outputs.erase(t);
            }
        }
        inputNodes[idx] = nullptr;
		auto ref = dynamic_cast<SceneComponent*>(referenceScene);
        if (ref && updateScene) { ref->onSceneChanged(); }
    }
}





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

const int NodeData::getMaxOutputDimension(const std::vector<std::vector<ddtype>>& inputs, RunnerInput& context, int inputNum) const noexcept
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



bool NodeData::compileTimeSizeReady(const RunnerInput* inlineInstance) const
{

    return this->compileTimeSizes.contains(inlineInstance);
}


int NodeData::getCompileTimeSize(const RunnerInput* inlineInstance) const
{
    if (this->compileTimeSizes.contains(inlineInstance)) {
        return compileTimeSizes.at(inlineInstance);
    }
    else {
		///DBG("Node of type " + getType()->name + " compile time size not ready!");
        ///return 1;
        throw std::invalid_argument("Node has not been compiled at this time!");
    }
}

void NodeData::setCompileTimeSize(const RunnerInput* inlineInstance, int s)
{
    _ASSERTE(_CrtCheckMemory());
    compileTimeSizes.insert_or_assign(inlineInstance, s);
    _ASSERTE(_CrtCheckMemory());
}


void NodeData::markUncompiled(const RunnerInput* inlineInstance)
{
    if (compileTimeSizes.contains(inlineInstance)){
        compileTimeSizes.erase(inlineInstance);
    }
}

InputType NodeData::getTrueType() const { return trueType; }

void NodeData::markWildCardTypesDirty()
{
	trueType = InputType::dirty; // if confirmed but unknown this must be any. followsInput marks it as dirty
}
// Returns true if type is found, false if still dirty
bool NodeData::computeWildCardTypes(
    std::unordered_set<NodeData*>& visited)
{
    if (trueType != InputType::dirty) {
        return true; // already computed
    }

    // cycle guard
    if (visited.count(this)) {
        return false; // already in recursion chain, don't recurse again
    }
    visited.insert(this);

    auto type = getType();
    const auto& inputInfos = type->inputs;

    bool connectedToAnyButUnresolved = false;

    // ---- Check outputs ----
    if (type->outputType == InputType::followsInput) {
        for (const auto& [other, i] : outputs) {
            if (!other) continue;

            auto it = other->getType()->inputs[i].inputType;
            if (it != InputType::any) {
                trueType = it;
                visited.erase(this);
                return true;
            }
            else if (other->computeWildCardTypes(visited)) {
                connectedToAnyButUnresolved = true;
                if (other->getTrueType() != InputType::any) {
                    trueType = other->getTrueType();
                    visited.erase(this);
                    return true;
                }
            }
        }
    }

    // ---- Check inputs ----
    bool allAnysDisconnected = true;
    for (int i = 0; i < getNumInputs(); i++) {
        if (inputInfos[i].inputType != InputType::any) continue;

        auto inputNode = getInput(i);
        if (!inputNode) continue;

        allAnysDisconnected = false;
        if (inputNode->type.outputType != InputType::followsInput) {
            trueType = inputNode->type.outputType;
            visited.erase(this);
            return true;
        }
        else {
            if (inputNode->computeWildCardTypes(visited)) {
                if (inputNode->getTrueType() != InputType::any) {
                    trueType = inputNode->trueType;
                    visited.erase(this);
                    return true;
                }
            }
        }
    }

    if (allAnysDisconnected && !connectedToAnyButUnresolved) {
        trueType = InputType::any;
        visited.erase(this);
        return true;
    }

    visited.erase(this);
    return false;
}

void NodeData::setTrueType(InputType t) { trueType = t; }

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
