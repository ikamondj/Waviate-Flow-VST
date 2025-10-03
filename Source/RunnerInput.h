/*
  ==============================================================================

    RunnerInput.h
    Created: 23 Aug 2025 12:15:24am
    Author:  ikamo

  ==============================================================================
*/

#pragma once
#include <mutex>
#include <unordered_map>
#include <span>
#include <unordered_set>
#include <vector>
#include "ddtype.h"
#include "OptLevel.h"

using NodeFn = void(*)(ddtype* dataField, int dataFieldSize,
    ddtype* output, int outputSize,
    ddtype** inputs, int* inputSizes, int numInputs,
    struct UserInput* u);


class NodeData;
class RunnerInput {
public:
    virtual ~RunnerInput() = default; // makes it polymorphic
    std::vector<std::unique_ptr<NodeData>> nodeCopies;
    std::vector<union ddtype> field;
    std::vector<class NodeData*> nodesOrder;
    std::unordered_map<NodeData*, std::span<ddtype>> nodeOwnership;
    std::unordered_map<NodeData*, std::tuple<int, int>> safeOwnership;
    std::unordered_set<NodeData*> compileTimeKnown;
    std::unordered_map<NodeData*, std::vector<ddtype>> nodeCompileTimeOutputs;
    std::unordered_map<NodeData*, NodeData*> remap;
    std::string clangcode;
    NodeFn compiledFunc;
    OptLevel optLevel = OptLevel::medium;
    NodeData* outputNode = nullptr;
};