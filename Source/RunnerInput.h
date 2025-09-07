/*
  ==============================================================================

    RunnerInput.h
    Created: 23 Aug 2025 12:15:24am
    Author:  ikamo

  ==============================================================================
*/
#include <mutex>
#include <unordered_map>
#include <span>
#include <unordered_set>
#include "NodeData.h"
#pragma once
class RunnerInput {
public:
    virtual ~RunnerInput() = default; // makes it polymorphic
    std::vector<NodeData> nodeCopies;
    std::vector<double> field;
    std::vector<class NodeData*> nodesOrder;
    std::unordered_map<NodeData*, std::span<double>> nodeOwnership;
    std::unordered_map<NodeData*, std::tuple<int, int>> safeOwnership;
    std::unordered_set<NodeData*> compileTimeKnown;
    std::unordered_map<NodeData*, std::vector<double>> nodeCompileTimeOutputs;
    std::unordered_map<const NodeData*, NodeData*> remap;
    NodeData* outputNode = nullptr;
};