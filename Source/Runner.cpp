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



std::span<double> Runner::run(RunnerInput& runnerInput, class UserInput& userInput, const std::vector<std::span<double>>& outerInputs)
{
	if (runnerInput.nodeCopies.empty()) return std::span<double, 0>();
	for (NodeData* node : runnerInput.nodesOrder)
	{
		auto& output = runnerInput.nodeOwnership[node];
		std::vector<std::span<double>> inputs;
		std::vector<double> emptyInputDefaults(node->getNumInputs());
		for (int i = 0; i < node->getNumInputs(); i += 1) {
			if (auto inputNode = node->getInput(i)) {
				auto& otherspan = runnerInput.nodeOwnership[inputNode];
				inputs.push_back(otherspan);
			}
			else {
				emptyInputDefaults[i] = node->getType()->inputs[i].defaultValue;
				std::span<double> otherspan(&emptyInputDefaults[i], 1);
				inputs.push_back(otherspan);
			}
		}
		double defaultIfNeeded = node->getNumericProperties().contains("defaultValue") ? node->getNumericProperty("defaultValue") : 0.0;
		std::span<double> spanOverDefaultIfNeeded(&defaultIfNeeded, 1);
		if (node->getType()->isInputNode) {
			int inputIndex = node->inputIndex;
			if (inputIndex >= 0 && inputIndex < outerInputs.size()) {
				inputs.push_back(outerInputs[inputIndex]);
			}
			else {
				inputs.push_back(spanOverDefaultIfNeeded);
			}
		}
		node->getType()->execute(*node, userInput, inputs, output, runnerInput);
	}
	return runnerInput.nodeOwnership[runnerInput.outputNode];
}


std::span<double> Runner::getNodeField(NodeData* nodeData, std::unordered_map<NodeData*, std::span<double>>& nodeOwnership)
{
	return nodeOwnership[nodeData];
}

bool Runner::containsNodeField(NodeData* d, std::unordered_map<NodeData*, std::span<double>>& nodeOwnership)
{
	return nodeOwnership.contains(d);
}

std::vector<double> Runner::findRemainingSizes(NodeData* node, std::unordered_map<NodeData*, std::vector<double>>& nodeCompileTimeOutputs, RunnerInput& inlineInstance, const std::vector<std::span<double>>& outerInputs)
{
	std::vector<double> tempOutput;
	std::vector<std::span<double>> actualspans;
	UserInput user;

	if (nodeCompileTimeOutputs.contains(node)) {
		return nodeCompileTimeOutputs[node];
	}
	else {
		std::vector<std::vector<double>> inputspans;
		for (int i = 0; i < node->getNumInputs(); i += 1) {
			NodeData* input = node->getInput(i);
			if (input) {
				std::vector<double> inputsOutput = findRemainingSizes(input, nodeCompileTimeOutputs, inlineInstance, outerInputs);
				inputspans.push_back(inputsOutput);
			}
			else {

				std::vector<double> inputsOutput = { node->getType()->inputs[i].defaultValue };
				inputspans.push_back(inputsOutput);
			}
		}
		if (node->getType()->isInputNode) {
			
			int inputIndex = node->inputIndex;
			if (outerInputs.empty()) {
				std::vector<double> defaultValue;
				defaultValue.push_back(0.0); //TODO actual custom default values for functions
				inputspans.push_back(defaultValue);
			}
			else {
				inputspans.push_back(std::vector<double>(outerInputs[inputIndex].begin(), outerInputs[inputIndex].end()));
			}
			
		}
		for (int i = 0; i < inputspans.size(); i += 1) {
			actualspans.emplace_back(inputspans[i]);
		}
		node->setCompileTimeSize(&inlineInstance, node->getMaxOutputDimension(inputspans, inlineInstance, node->inputIndex));
		
		for (int i = 0; i < node->getCompileTimeSize(&inlineInstance); i += 1) {
			tempOutput.push_back(0.0);
		}
		std::span tempOutSpan = std::span(tempOutput);
		
		
		
		node->getType()->execute(*node, user, actualspans, tempOutSpan, inlineInstance);
		nodeCompileTimeOutputs.insert({ node, tempOutput });

		return tempOutput;
	}
}

void Runner::initializeTop(WaviateFlow2025AudioProcessor& processor, class SceneComponent* scene, const std::vector<std::span<double>>& outerInputs) {
	initialize(processor.getBuildableRunner(), scene, outerInputs);
	processor.swapToNextRunner();
}

void Runner::initialize(RunnerInput& input, class SceneComponent* scene, const std::vector<std::span<double>>& outerInputs)
{
	input.nodesOrder.clear();
	input.nodeOwnership.clear();
	input.safeOwnership.clear();
	input.nodeCompileTimeOutputs.clear();
	input.nodeCopies.clear();
	input.field.clear();
	if (!scene) { return; }
	if (scene->nodes.empty()) { return; }
	std::unordered_map<const NodeData*, NodeData*> remap;
	const NodeData* editorOutput = nullptr;

	for (size_t i = 0; i < scene->nodes.size(); ++i) {
		auto& n = scene->nodes[i];
		input.nodeCopies.push_back(n->getNodeData()); // value copy (deep copy of maps etc.)
		if (n->getType().name == "output") {
			editorOutput = &n->getNodeDataConst();
		}
	}
	// fallback if you truly use index 0 as output in your scene model:
	if (!editorOutput) editorOutput = &input.nodeCopies[0];
	for (size_t i = 0; i < scene->nodes.size(); ++i) {
		remap[&scene->nodes[i]->getNodeData()] = &input.nodeCopies[i];
		remap[&input.nodeCopies[i]] = &scene->nodes[i]->getNodeData();
	}
	input.outputNode = remap[editorOutput];

	for (auto& newNode : input.nodeCopies) {
		for (auto*& in : newNode.inputNodes) {
			if (in) {
				auto it = remap.find(in);
				jassert(it != remap.end()); // should always exist
				in = it->second;
			}
		}
	}
	for (auto& node : input.nodeCopies) {
		findRemainingSizes(&node, input.nodeCompileTimeOutputs, input, outerInputs);
	}
	setupRecursive(input.outputNode, input);
	for (auto& [node, ownership] : input.safeOwnership) {
		if (node) {
			auto [offset, size] = ownership;
			input.nodeOwnership[node] = std::span<double>(input.field.data() + offset, size);
		}

	}

	UserInput fakeInput;
	std::vector<NodeData*> tempNodesOrder;
	for (NodeData* node : input.nodesOrder) {
		if (node->isCompileTimeKnown()) {
			input.compileTimeKnown.insert(node);
			std::vector<double> extraspace;
			for (int i = 0; i < node->getNumInputs(); i += 1) {
				auto input = node->getInput(i);
				extraspace.push_back(node->getType()->inputs[i].defaultValue);
			}
			auto& output = input.nodeOwnership[node];
			std::vector<std::span<double>> inputs;
			for (int i = 0; i < node->getNumInputs(); i += 1) {

				auto nodeInput = node->getInput(i);
				if (nodeInput) {
					auto& otherspan = input.nodeOwnership[node->getInput(i)];
					inputs.push_back(otherspan);
				}
				else {
					inputs.push_back(std::span<double>(&extraspace[i], 1));
				}
			}
			double defaultIfNeeded = node->getNumericProperties().contains("defaultValue") ? node->getNumericProperty("defaultValue") : 0.0;
			std::span<double> spanOverDefaultIfNeeded(&defaultIfNeeded, 1);
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
		if (remap.contains(&node)) {
			auto sceneNode = remap[&node];
			if (sceneNode) {
				sceneNode->setCompileTimeSize(&input, node.getCompileTimeSize(&input));
			}
		}
		
		
	}
	input.nodesOrder = tempNodesOrder;
	std::vector<double> tempField;
}