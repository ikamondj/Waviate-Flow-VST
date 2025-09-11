/*
  ==============================================================================

    Scene.cpp
    Created: 10 Sep 2025 4:38:50pm
    Author:  ikamo

  ==============================================================================
*/

#include "SceneData.h"
#include "NodeComponent.h"
#include "Runner.h"
#include "SceneComponent.h"

void SceneData::computeAllNodeWildCards() {
    for (auto& nc : nodeDatas) {
        nc->markWildCardTypesDirty();
    }

    bool anyDirty;
    do {
        anyDirty = false;
        int resolvedThisPass = 0;

        for (auto& nc : nodeDatas) {
            auto& d = *nc;
            if (d.getTrueType() == InputType::dirty) {
                std::unordered_set<NodeData*> visited;
                bool clean = d.computeWildCardTypes(visited);
                anyDirty |= !clean;
                if (clean) ++resolvedThisPass;
            }
        }

        if (anyDirty && resolvedThisPass == 0) {
            // Stalemate: collapse remaining wildcards to ANY
            for (auto* nc : nodeDatas) {
                auto& d = *nc;
                if (d.getTrueType() == InputType::dirty) {
                    d.setTrueType(InputType::any);
                }
            }
            anyDirty = false;
        }
    } while (anyDirty);
}

std::string SceneData::getNameFromJson(const std::string& json)
{
    return std::string();
}

std::string SceneData::getJsonFromFullID(const uint64_t userId, const uint64_t nodeId)
{
    return std::string();
}

void SceneData::initSceneWithJson(const std::string& json)
{
    sceneName = getNameFromJson(json);
}

SceneData::SceneData() : customNodeType(-1), processorRef(nullptr) {
    nodeDatas.reserve(2000);
}

SceneData::SceneData(const std::string& name) : SceneData()
{
	sceneName = name;
}

SceneData::SceneData(const uint64_t userId, const uint64_t nodeId) : SceneData()
{
    std::string json = getJsonFromFullID(userId, nodeId);
    initSceneWithJson(json);
}

void SceneData::constructWithName(const std::string& name)
{
    sceneName = name;
}

void SceneData::getSortedInputNodes(std::vector<NodeData*>& nodesFilteredByInputAndSorted) {
    for (NodeData* node : nodeDatas) {
        const NodeData& data = *node;
        const NodeType* type = data.getType();
        if (type->isInputNode) {
            nodesFilteredByInputAndSorted.push_back(node);
        }
    }

    std::sort(nodesFilteredByInputAndSorted.begin(),
        nodesFilteredByInputAndSorted.end(),
        [](NodeData* a, NodeData* b) {
            return a->getPosition().y < b->getPosition().y;
        });
}

void SceneData::editNodeType()
{

    customNodeType.name = sceneName;
    customNodeType.address = "custom/";
    customNodeType.buildUI = [](NodeComponent&, NodeData&) {};
    std::vector<NodeData*> routineInputs;
    customNodeType.inputs.clear();
    std::vector<NodeData*> nodesFilteredByInputAndSorted;
	getSortedInputNodes(nodesFilteredByInputAndSorted);


    for (int inputIdx = 0; inputIdx < nodesFilteredByInputAndSorted.size(); inputIdx++) {
        auto node = nodesFilteredByInputAndSorted[inputIdx];
        NodeData& data = *node;
        std::string inputName = data.getProperties().contains("name") ? data.getProperties().at("name") : "input";
        bool requiresCompileTime = false;
        data.inputIndex = inputIdx;
        for (NodeData* downstream : nodeDatas) {
            auto& downstreamData = *downstream;
            for (int i = 0; i < downstreamData.getNumInputs(); i += 1) {
                if (downstreamData.getInput(i) == &data && (downstreamData.getType()->inputs[i].requiresCompileTimeKnowledge || downstreamData.needsCompileTimeInputs())) {
                    requiresCompileTime = true;
                    goto exit_loop;
                }
            }
        }
    exit_loop:
        int requiredSize = 0;
        for (const auto& [downstream, index] : data.outputs) {
            if (downstream) {
                int sz = downstream->getType()->inputs[index].requiredSize;
                if (sz > requiredSize) {
                    requiredSize = sz;
                }
            }
        }
        InputType it = data.getTrueType();
        //if (it == InputType::dirty) {
        //    throw std::logic_error("Input nodes cannot have undetermined output types!");
        //}
        InputFeatures features(inputName, it, requiredSize, requiresCompileTime);
        customNodeType.inputs.push_back(features);
    }
    customNodeType.fromScene = this;
    customNodeType.outputType = nodeDatas[0]->getTrueType();
    //if (customNodeType.outputType == InputType::dirty) {
    //    throw std::logic_error("Output node cannot have undetermined output type!");
    //}

    customNodeType.execute = [](const NodeData& node, UserInput& userInput, const std::vector<std::span<ddtype>>& inputs, std::span<ddtype> output, const RunnerInput& inlineInstance) {
        auto span = Runner::run(node.optionalRunnerInput.get(), userInput, inputs);
        for (int i = 0; i < output.size(); i += 1) {
            output[i] = span[i];
        }
    };
    customNodeType.getOutputSize =
        [this](const std::vector<NodeData*>& /*inputs*/,
            const std::vector<std::vector<ddtype>>& d,
            const RunnerInput& outerRunner,
            int /*unused*/,
            const NodeData& self) -> int
        {
            // Prepare spans for outer inputs
            std::vector<std::span<ddtype>> outerInputs;
            outerInputs.reserve(d.size());
            for (auto const& v : d) {
                outerInputs.emplace_back(const_cast<ddtype*>(v.data()), v.size());
            }

            // Ensure the wrapper node has its sub-runner initialized

            RunnerInput& subRunner = *self.optionalRunnerInput;

            // Inline/compile the scene graph into this runner (SceneData::this)
            Runner::initialize(subRunner,dynamic_cast<SceneComponent*>(this),outerInputs);

            // Ask the *sub-runner’s* output node for its size
            if (subRunner.outputNode) {
                return subRunner.outputNode->getCompileTimeSize(&subRunner);
            }
            return 0;
    };

}

const std::string& SceneData::getSceneName() const { return (std::string&)sceneName; }

void SceneData::setSceneName(const std::string& newName)
{
	sceneName = newName;
}
