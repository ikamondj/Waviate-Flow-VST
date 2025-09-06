/*
  ==============================================================================

    Runner.h
    Created: 15 Aug 2025 10:46:29pm
    Author:  ikamo

  ==============================================================================
*/

#pragma once
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <span>
#include <memory>
#include <mutex>
class Runner {
public:
	static void setupRecursive(class NodeData* node, class RunnerInput& inlineInstance);
    
    static std::span<double> run(class RunnerInput& runnerInput, class UserInput& userInput, const std::vector<std::span<double>>& outerInputs);
    static std::span<double> getNodeField(NodeData*, std::unordered_map<NodeData*, std::span<double>>& nodeOwnership);
    static bool containsNodeField(NodeData*, std::unordered_map<NodeData*, std::span<double>>& nodeOwnership);
    static std::vector<double> findRemainingSizes(NodeData* node, std::unordered_map<NodeData*, std::vector<double>>& nodeCompileTimeOutputs, class RunnerInput& inlineInstance, const std::vector<std::span<double>>& outerInputs);
    
    static void initialize(RunnerInput& input, class SceneComponent* scene, const std::vector<std::span<double>>& outerInputs);
    static void initializeTop(class WaviateFlow2025AudioProcessor& processor, SceneComponent* scene, const std::vector<std::span<double>>& outerInputs);
};