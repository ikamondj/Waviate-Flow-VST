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
#include "ddtype.h"
class Runner {
public:
    static void setupRecursive(class NodeData* node, class RunnerInput& inlineInstance);
    static void setupIterative(NodeData* root, RunnerInput& inlineInstance);
    
    
    static std::span<ddtype> run(const class RunnerInput* runnerInput, class UserInput& userInput, const std::vector<std::span<ddtype>>& outerInputs);
    static std::span<ddtype> getNodeField(NodeData*, std::unordered_map<NodeData*, std::span<ddtype>>& nodeOwnership);
    static bool containsNodeField(NodeData*, std::unordered_map<NodeData*, std::span<ddtype>>& nodeOwnership);
    static std::vector<ddtype> findRemainingSizes(NodeData* node, class RunnerInput& inlineInstance, const std::vector<std::span<ddtype>>& outerInputs);

    
    
    static void initialize(RunnerInput& input, class SceneData* scene, const std::vector<std::span<ddtype>>& outerInputs);

private:
};