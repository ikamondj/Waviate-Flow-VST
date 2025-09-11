/*
  ==============================================================================

    Runner.cpp
    Created: 15 Aug 2025 10:46:29pm
    Author:  ikamo

  ==============================================================================
*/

#include "Runner.h"
#include "NodeData.h"
#include "SceneComponent.h"
#include "UserInput.h"
#include "NodeComponent.h"
#include "PluginProcessor.h"
#include "NodeType.h"





void Runner::setupRecursive(NodeData* node, RunnerInput& inlineInstance) {
	
	if (!node || inlineInstance.safeOwnership.contains(node)) return;
	std::vector < std::tuple<int, int> > tempSpans;
	for (int i = 0; i < node->getNumInputs(); i += 1) {
		auto input = node->getInput(i);
		if (input != nullptr) {
			setupRecursive(input, inlineInstance);
			auto [start,length] = inlineInstance.safeOwnership[input];
			tempSpans.push_back({start, length});
		}
	}
	int size = node->getCompileTimeSize(&inlineInstance);
	int offset = inlineInstance.field.size();
	inlineInstance.field.resize(inlineInstance.field.size() + size);
	inlineInstance.safeOwnership.insert({ node, {offset, size} });
	inlineInstance.nodesOrder.push_back(node);
}



std::span<ddtype> Runner::run(const RunnerInput* runnerInputP, class UserInput& userInput, const std::vector<std::span<ddtype>>& outerInputs)
{
	if (!runnerInputP) return std::span<ddtype, 0>();
	auto& runnerInput = *runnerInputP;
	if (runnerInput.nodeCopies.empty()) return std::span<ddtype, 0>();
	for (NodeData* node : runnerInput.nodesOrder)
	{
		auto& output = runnerInput.nodeOwnership.at(node);
		std::vector<std::span<ddtype>> inputs;
		std::vector<ddtype> emptyInputDefaults(node->getNumInputs());
		for (int i = 0; i < node->getNumInputs(); i += 1) {
			if (auto inputNode = node->getInput(i)) {
				auto& otherspan = runnerInput.nodeOwnership.at(inputNode);
				inputs.push_back(otherspan);
			}
			else {
				emptyInputDefaults[i] = node->getType()->inputs[i].defaultValue;
				std::span<ddtype> otherspan(&emptyInputDefaults[i], 1);
				inputs.push_back(otherspan);
			}
		}
		ddtype defaultIfNeeded = 0.0;
		
		
		if (node->getType()->isInputNode) {
			int inputIndex = node->inputIndex;
			if (inputIndex >= 0 && inputIndex < outerInputs.size()) {
				inputs.push_back(outerInputs[inputIndex]);
			}
			else {
				if (node->getType()->outputType == InputType::decimal) {
					defaultIfNeeded.d = node->getNumericProperties().contains("defaultValue") ? node->getNumericProperty("defaultValue") : 0.0;
				}
				else if (node->getType()->outputType == InputType::boolean) {
					defaultIfNeeded.i = node->getNumericProperties().contains("defaultValue") ? (node->getNumericProperty("defaultValue") > 0.5 ? 1 : 0) : 0;
				}
				else {
					defaultIfNeeded.i = node->getNumericProperties().contains("defaultValue") ? static_cast<int>(std::round(node->getNumericProperty("defaultValue"))) : 0;
				}
				std::span<ddtype> spanOverDefaultIfNeeded(&defaultIfNeeded, 1);
				inputs.push_back(spanOverDefaultIfNeeded);
			}
		}
		node->getType()->execute(*node, userInput, inputs, output, runnerInput);
	}
	return runnerInput.nodeOwnership.at(runnerInput.outputNode);
}


std::span<ddtype> Runner::getNodeField(NodeData* nodeData, std::unordered_map<NodeData*, std::span<ddtype>>& nodeOwnership)
{
	return nodeOwnership[nodeData];
}

bool Runner::containsNodeField(NodeData* d, std::unordered_map<NodeData*, std::span<ddtype>>& nodeOwnership)
{
	return nodeOwnership.contains(d);
}

		
std::vector<ddtype> Runner::findRemainingSizes(NodeData* node, RunnerInput& inlineInstance, const std::vector<std::span<ddtype>>& outerInputs)
{
	std::vector<ddtype> tempOutput;
	std::vector<std::span<ddtype>> actualspans;
	UserInput user;

	if (inlineInstance.nodeCompileTimeOutputs.contains(node)) {
		return inlineInstance.nodeCompileTimeOutputs[node];
	}
	else {
		std::vector<std::vector<ddtype>> inputspans;
		for (int i = 0; i < node->getNumInputs(); i += 1) {
			NodeData* input = node->getInput(i);
			if (input) {
				std::vector<ddtype> inputsOutput = findRemainingSizes(input, inlineInstance, outerInputs);
				inputspans.push_back(inputsOutput);
			}
			else {

				std::vector<ddtype> inputsOutput = { node->getType()->inputs[i].defaultValue };
				inputspans.push_back(inputsOutput);
			}
		}
		if (node->getType()->isInputNode) {
			
			int inputIndex = node->inputIndex;
			if (outerInputs.empty()) {
				std::vector<ddtype> defaultValue;
				defaultValue.push_back(0.0); //TODO actual custom default values for functions
				inputspans.push_back(defaultValue);
			}
			else {
				inputspans.push_back(std::vector<ddtype>(outerInputs[inputIndex].begin(), outerInputs[inputIndex].end()));
			}
			
		}
		
		for (int i = 0; i < inputspans.size(); i += 1) {
			actualspans.emplace_back(inputspans[i]);
		}
		auto nodeType = node->getType();
		int dim = node->getMaxOutputDimension(inputspans, inlineInstance, node->inputIndex);
		node->setCompileTimeSize(&inlineInstance, dim);
		for (int i = 0; i < dim; i += 1) {
			tempOutput.push_back(0.0);
		}
		std::span tempOutSpan = std::span(tempOutput);



		node->getType()->execute(*node, user, actualspans, tempOutSpan, inlineInstance);
		inlineInstance.nodeCompileTimeOutputs.insert({ node, tempOutput });
		return tempOutput;
	}
}

void storeCopies(RunnerInput& input,
	SceneData* startScene,
	NodeData*& editorOutput,
	std::unordered_map<NodeData*, NodeData*>& remap)
{
		std::vector<NodeData*> sceneInputNodes;

		// clone nodes
		for (int i = 0; i < (int)startScene->nodeDatas.size(); i++) {
			auto& n = *startScene->nodeDatas[i];
			input.nodeCopies.push_back(std::make_unique<NodeData>(n)); // deep copy
			NodeData* copyPtr = input.nodeCopies.back().get();
			if (copyPtr) {
				remap[&n] = copyPtr;
				remap[copyPtr] = &n;

				if (n.getType()->name == "output") {
					if (!editorOutput) {
						editorOutput = copyPtr; // safe, only if copy exists
					}
				}
			}
			
		}


	// cleanup / fix references
	for (auto& nodeCopy : input.nodeCopies) {
		if (!nodeCopy) continue;

		// fix inputs
		for (int i = 0; i < nodeCopy->getNumInputs(); i++) {
			if (auto in = nodeCopy->getInput(i)) {
				if (!in->isCopy) {
					auto it = remap.find(in);
					if (it != remap.end()) {
						nodeCopy->detachInput(i, nullptr);
						nodeCopy->attachInput(i, it->second, input, nullptr);
					}
				}
			}
		}

		// fix outputs
		for (const auto& [output, idx] : nodeCopy->outputs) {
			if (output && !output->isCopy) {
				auto it = remap.find(output);
				if (it != remap.end()) {
					it->second->attachInput(idx, nodeCopy.get(), input, nullptr);
				}
			}
		}
	}
}


void Runner::initialize(RunnerInput& input, class SceneComponent* scene, const std::vector<std::span<ddtype>>& outerInputs)
{
	input.nodesOrder.clear();
	input.nodeOwnership.clear();
	input.safeOwnership.clear();
	input.nodeCompileTimeOutputs.clear();
	input.nodeCopies.clear();
	input.remap.clear();
	input.field.clear();
	if (!scene) { return; }
	if (scene->nodes.empty()) { return; }
	std::unordered_map<NodeData*, NodeData*>& remap = input.remap;
	NodeData* editorOutput = nullptr;

	storeCopies(input, scene, editorOutput, remap);
	
	// fallback if you truly use index 0 as output in your scene model:
	if (!editorOutput) editorOutput = input.nodeCopies[0].get();

	input.outputNode = editorOutput;

	for (auto& newNode : input.nodeCopies) {
		newNode->markUncompiled(&input);
	}
	for (auto& node : input.nodeCopies) {
		findRemainingSizes(node.get(), input, outerInputs);
	}
	setupRecursive(input.outputNode, input);
	for (auto& [node, ownership] : input.safeOwnership) {
		if (node) {
			auto [offset, size] = ownership;
			input.nodeOwnership[node] = std::span<ddtype>(input.field.data() + offset, size);
		}
	}

	UserInput fakeInput;
	std::vector<NodeData*> tempNodesOrder;
	for (NodeData* node : input.nodesOrder) {
		if (node->isCompileTimeKnown()) {
			input.compileTimeKnown.insert(node);
			std::vector<ddtype> extraspace;
			for (int i = 0; i < node->getNumInputs(); i += 1) {
				auto input = node->getInput(i);
				extraspace.push_back(node->getType()->inputs[i].defaultValue);
			}
			auto& output = input.nodeOwnership[node];
			std::vector<std::span<ddtype>> inputs;
			for (int i = 0; i < node->getNumInputs(); i += 1) {

				auto nodeInput = node->getInput(i);
				if (nodeInput) {
					auto& otherspan = input.nodeOwnership[node->getInput(i)];
					inputs.push_back(otherspan);
				}
				else {
					inputs.push_back(std::span<ddtype>(&extraspace[i], 1));
				}
			}
			ddtype defaultIfNeeded = node->getNumericProperties().contains("defaultValue") ? node->getNumericProperty("defaultValue") : 0.0;
			std::span<ddtype> spanOverDefaultIfNeeded(&defaultIfNeeded, 1);
			if (node->getType()->isInputNode) {
				int inputIndex = node->inputIndex;
				if (inputIndex >= 0 && inputIndex < outerInputs.size()) {
					inputs.push_back(outerInputs[inputIndex]);
				}
				else {
					inputs.push_back(spanOverDefaultIfNeeded);
				}
			}
			node->getType()->execute(*node, fakeInput, inputs, output, input);
		}
		else {
			tempNodesOrder.push_back(node);
		}
	}

	for (auto& node : input.nodeCopies) {
		if (remap.contains(node.get())) {
			auto sceneNode = remap[node.get()];
			if (sceneNode) {
				sceneNode->setCompileTimeSize(scene, node->getCompileTimeSize(&input));
			}
		}
	}
	input.nodesOrder = tempNodesOrder;
	std::vector<ddtype> tempField;
}