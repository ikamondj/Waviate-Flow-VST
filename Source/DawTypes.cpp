/*
  ==============================================================================

    DawTypes.cpp
    Created: 25 Sep 2025 10:30:08pm
    Author:  ikamo

  ==============================================================================
*/

#include "Registry.h"

void WaviateFlow2025AudioProcessor::initializeRegistryDaw() {
    NodeType::registryCreatePrefix = 100000;


    // DAW parameter value node
    {
        NodeType t(131);
        t.name = "daw parameter";
        t.address = "daw/";
        t.tooltip = "Reads a DAW parameter’s current value (normalized 0..1).";
        t.inputs = {
            InputFeatures("paramIndex", InputType::integer, 0, false)
        };
        t.getOutputSize = outputSizeByInputPin(0); // same length as indices
        t.buildUI = [](NodeComponent&, NodeData&) {};
        t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput& userInput,
            const std::vector<std::span<ddtype>>& in,
            std::span<ddtype> out, const RunnerInput&)
            {
                const auto& idx = in[0];
                int n = static_cast<int>(idx.size());

                auto& params = userInput.dawParams; // from dawManager

                for (int i = 0; i < n; ++i)
                {
                    int pi = idx[i].i;
                    float val = 0.0f;
                    if (pi >= 0 && pi < params.size())
                        val = params[pi]; // already normalized 0..1
                    out[i].d = val;
                }
            };
        t.whichInputToFollowWildcard = 0;
        t.outputType = InputType::decimal;
        t.alwaysOutputsRuntimeData = true;
        t.fromScene = nullptr;
        registry.push_back(t);
    }
}