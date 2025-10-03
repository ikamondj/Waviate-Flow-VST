/*
  ==============================================================================

    IntegralTypes.cpp
    Created: 25 Sep 2025 10:30:25pm
    Author:  ikamo

  ==============================================================================
*/

#include "Registry.h"

void WaviateFlow2025AudioProcessor::initializeRegistryIntegral() {
    NodeType::registryCreatePrefix = 0;
    // ========= output =========
    {
        NodeType outputType(1);
        outputType.name = "output";
        outputType.address = "";
        outputType.tooltip = "Pass-through: forwards its single input to the output.";
        outputType.inputs = { InputFeatures("audio", InputType::any, 0, false) };
        outputType.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& inputs, std::span<ddtype> output, const RunnerInput& inlineInstance)
            {
                for (int i = 0; i < static_cast<int>(inputs[0].size()); ++i) output[i] = inputs[0][i];
            };
        outputType.getOutputSize = outputSizeEqualsSingleInputSize;
        outputType.buildUI = [](NodeComponent& n, NodeData& d) {
            n.inputGUIElements.push_back(std::make_unique<juce::AudioVisualiserComponent>(1));
            auto* v = dynamic_cast<juce::AudioVisualiserComponent*>(n.inputGUIElements.back().get());
            v->setSamplesPerBlock(12);
            n.addAndMakeVisible(v);
            float scale = (float)std::pow(2.0, n.getOwningScene()->logScale);
            const float sides = 20.0f * scale;
            float cornerSize = 24.0f * scale;
            if (!n.inputGUIElements.empty()) {
                n.inputGUIElements.back()->setBounds(sides, cornerSize, n.getWidth() - sides - sides, n.getHeight() - cornerSize * 2);
            }
            };
        outputType.onResized = [](NodeComponent& n) {
            float scale = (float)std::pow(2.0, n.getOwningScene()->logScale);
            const float sides = 20.0f * scale;
            float cornerSize = 24.0f * scale;
            if (!n.inputGUIElements.empty()) {
                n.inputGUIElements.back()->setBounds(sides, cornerSize, n.getWidth() - sides - sides, n.getHeight() - cornerSize * 2);
            }
            };
        outputType.outputType = InputType::any;
        outputType.alwaysOutputsRuntimeData = false;
        outputType.fromScene = nullptr;
        registry.push_back(outputType);
    }

    // ========= input =========
    NodeType inputType(2);
    inputType.name = "input";
    inputType.address = "";
    inputType.tooltip = "Audio input placeholder (to be wired to a source).";
    inputType.inputs = { };// InputFeatures("", InputType::any, 0, false)};
    inputType.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& i, std::span<ddtype> o, const RunnerInput& inlineInstance)
        {
            const size_t size = o.size();
            if (!i.empty()) {
                for (int x = 0; x < size; x += 1) {
                    o[x] = i[0][x];
                }
            }
            else {
                for (int x = 0; x < size; x += 1) {
                    o[x] = 0.0;
                }
            }

        };
    inputType.isInputNode = true;
    inputType.getOutputSize = [](const std::vector<NodeData*>& inputNodes, const std::vector<std::vector<ddtype>>& inputs, const RunnerInput& inlineInstance, int inputNum, const NodeData&) {
        auto* inlineComponent = dynamic_cast<const NodeComponent*>(&inlineInstance);
        if (inlineComponent) {
            return (int)inputs[inputNum].size();
        }

        return 1;
        };
    inputType.buildUI = [](NodeComponent& comp, NodeData& node)
        {
            node.setProperty("name", "input");
            comp.inputGUIElements.push_back(std::make_unique<juce::TextEditor>("name"));
            auto back = dynamic_cast<juce::TextEditor*>(comp.inputGUIElements.back().get());
            back->setJustification(juce::Justification::horizontallyCentred);
            back->setText("input");
            back->onTextChange = [&node, &comp]()
                {
                    auto el = dynamic_cast<juce::TextEditor*>(comp.inputGUIElements.back().get());
                    node.setProperty("name", el->getText().toStdString());
                    comp.getOwningScene()->onSceneChanged();
                };

            const float scale = (float)std::pow(2.0, comp.getOwningScene()->logScale);
            const float sides = 20.0f * scale;
            const float cornerSize = 24.0f * scale;
            back->setMultiLine(false, false);
            back->setBounds(sides, cornerSize, comp.getWidth() - sides - sides, comp.getHeight() - cornerSize * 2);
            back->applyFontToAllText(back->getFont().withHeight(14 * scale));
            comp.addAndMakeVisible(back);
        };
    inputType.onResized = [](NodeComponent& comp) {
        if (comp.inputGUIElements.empty()) return;
        auto back = dynamic_cast<juce::TextEditor*>(comp.inputGUIElements.back().get());

        if (!back) return;
        const float scale = (float)std::pow(2.0, comp.getOwningScene()->logScale);
        const float sides = 20.0f * scale;
        const float cornerSize = 24.0f * scale;
        back->setMultiLine(false, false);
        back->setBounds(sides, cornerSize, comp.getWidth() - sides - sides, comp.getHeight() - cornerSize * 2);
        back->applyFontToAllText(back->getFont().withHeight(14 * scale));
        };
    inputType.outputType = InputType::followsInput;
    inputType.alwaysOutputsRuntimeData = false;
    inputType.fromScene = nullptr;
    registry.push_back(inputType);


}