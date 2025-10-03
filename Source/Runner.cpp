/*
  ==============================================================================

    Runner.cpp
    Created: 15 Aug 2025 10:46:29pm
    Author:  ikamo

  ==============================================================================
*/
#include <stack>
#include "Runner.h"
#include "NodeData.h"
#include "SceneComponent.h"
#include "UserInput.h"
#include "NodeComponent.h"
#include "PluginProcessor.h"
#include "NodeType.h"


void Runner::setupIterative(NodeData* root, RunnerInput& inlineInstance) {
	if (!root) return;

	struct Frame {
		NodeData* node;
		int inputIndex;
	};

	std::vector<Frame> stack;
	stack.push_back({ root, 0 });

	while (!stack.empty()) {
		Frame& frame = stack.back();
		NodeData* node = frame.node;

		// Already processed?
		if (!node || inlineInstance.safeOwnership.contains(node)) {
			stack.pop_back();
			continue;
		}

		// Process inputs one by one (simulate recursion)
		if (frame.inputIndex < node->getNumInputs()) {
			auto input = node->getInput(frame.inputIndex++);
			if (input != nullptr && !inlineInstance.safeOwnership.contains(input)) {
				stack.push_back({ input, 0 });
			}
		}
		else {
			// All inputs done, now "return" and process node
			int size = node->getCompileTimeSize(&inlineInstance);
			int offset = inlineInstance.field.size();
			inlineInstance.field.resize(inlineInstance.field.size() + size);
			inlineInstance.safeOwnership.insert({ node, {offset, size} });
			inlineInstance.nodesOrder.push_back(node);

			stack.pop_back();
		}
	}
}



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

inline void convert(const std::span<ddtype>& data, InputType outboundType, InputType inboundType) {
	if (outboundType == InputType::decimal) {
		if (inboundType == InputType::boolean) {
			for (ddtype& d : data) { d.i = d.d > 0.5 ? 1 : 0; }
		}
		else if (inboundType == InputType::integer) {
			for (ddtype& d : data) { d.i = static_cast<int64_t>(std::round(d.d)); }
		}
	}
	else if (outboundType == InputType::integer) {
		if (inboundType == InputType::boolean) {
			for (ddtype& d : data) { d.i = (d.i == 0 ? 0 : 1); }
		}
		else if (inboundType == InputType::decimal) {
			for (ddtype& d : data) { d.d = static_cast<double>(d.i); }
		}
	}
	else if (inboundType == InputType::decimal) {
		for (ddtype& d : data) { d.d = d.i ? 1.0 : 0.0; }
	}
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
				auto outboundType = inputNode->getType()->outputType;
				auto inboundType = node->getType()->inputs[i].inputType;
				if (outboundType == InputType::followsInput) {
					outboundType = inputNode->getTrueType();
				}
				if (inboundType == InputType::any) {
					inboundType = node->getTrueType();
				}
				convert(otherspan, outboundType, inboundType);
				inputs.push_back(otherspan);
			}
			else {
				emptyInputDefaults[i] = node->defaultValues[i];
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

		
std::vector<ddtype> Runner::findRemainingSizes(NodeData* root, RunnerInput& inlineInstance, const std::vector<std::span<ddtype>>& outerInputs)
{
	// If already cached
	if (inlineInstance.nodeCompileTimeOutputs.contains(root)) {
		return inlineInstance.nodeCompileTimeOutputs[root];
	}

	struct Frame {
		NodeData* node;
		int state; // 0 = expand inputs, 1 = compute
		std::vector<std::vector<ddtype>> inputspans;
	};

	std::vector<ddtype> result;
	std::stack<Frame> stack;
	stack.push({ root, 0, {} });

	while (!stack.empty()) {
		Frame frame = std::move(stack.top());
		stack.pop();

		NodeData* node = frame.node;

		// If cached, reuse and continue
		if (inlineInstance.nodeCompileTimeOutputs.contains(node)) {
			continue;
		}

		if (frame.state == 0) {
			// Expand inputs first
			frame.state = 1;
			stack.push(std::move(frame)); // push back current frame for later compute

			for (int i = node->getNumInputs() - 1; i >= 0; i--) {
				NodeData* input = node->getInput(i);
				if (input) {
					if (!inlineInstance.nodeCompileTimeOutputs.contains(input)) {
						stack.push({ input, 0, {} }); // process this child first
					}
				}
			}
		}
		else {
			// All inputs processed — compute this node
			std::vector<std::vector<ddtype>> inputspans;
			for (int i = 0; i < node->getNumInputs(); i++) {
				NodeData* input = node->getInput(i);
				if (input) {
					inputspans.push_back(inlineInstance.nodeCompileTimeOutputs[input]);
				}
				else {
					inputspans.push_back({ node->defaultValues[i] });
				}
			}

			if (node->getType()->isInputNode) {
				int inputIndex = node->inputIndex;
				if (outerInputs.empty()) {
					inputspans.push_back({ 0.0 }); // TODO: custom defaults
				}
				else {
					inputspans.push_back(std::vector<ddtype>(outerInputs[inputIndex].begin(), outerInputs[inputIndex].end()));
				}
			}

			std::vector<std::span<ddtype>> actualspans;
			for (auto& vec : inputspans) {
				actualspans.emplace_back(vec);
			}

			int dim = node->getMaxOutputDimension(inputspans, inlineInstance, node->inputIndex);
			node->setCompileTimeSize(&inlineInstance, dim);

			std::vector<ddtype> tempOutput(dim, 0.0);
			std::span<ddtype> tempOutSpan(tempOutput);

			UserInput user;
			node->getType()->execute(*node, user, actualspans, tempOutSpan, inlineInstance);

			inlineInstance.nodeCompileTimeOutputs[node] = tempOutput;

			if (node == root) {
				result = tempOutput;
			}
		}
	}

	return result;
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


void Runner::initialize(RunnerInput& input, class SceneData* scene, const std::vector<std::span<ddtype>>& outerInputs)
{
	input.nodesOrder.clear();
	input.nodeOwnership.clear();
	input.safeOwnership.clear();
	input.nodeCompileTimeOutputs.clear();
	input.nodeCopies.clear();
	input.remap.clear();
	input.field.clear();
	if (!scene) { return; }
	if (scene->nodeDatas.empty()) { return; }
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
				extraspace.push_back(node->defaultValues[i]);
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