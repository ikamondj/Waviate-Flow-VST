/*
  ==============================================================================

    Registry.cpp
    Created: 14 Aug 2025 2:14:03pm
    Author:  ikamo

  ==============================================================================
*/

#include "PluginEditor.h"
#include "NodeType.h"
#include "RunnerInput.h"
#define MAX_UNKNOWN_SIZE 1024

//get output size general functions
int outputSize1Known(const std::vector<NodeData*>& inputs, const std::vector<std::vector<double>>&, RunnerInput&, int) {
	return 1;
}

int outputSizeComponentWise(const std::vector<NodeData*>& inputs, const std::vector<std::vector<double>>& s, RunnerInput& r, int) {
    int size = 1;
    bool getsPadded = false;
    for (const NodeData* input : inputs) {
        if (input) {
            orableBool inputUnknown;
            int inputSize = input->getCompileTimeSize(&r);
            if (inputSize > size) {
                size = inputSize;
            }
        }
    }

    return size;
}

double wrapTo(double v, double lo, double hi) {
    const double range = hi - lo;
    if (!(range > 0.0)) return lo;            // degenerate -> lo
    double w = std::fmod(v - lo, range);
    if (w < 0.0) w += range;                  // match zigzag's negative handling
    return lo + w;                            // in [lo, hi)
}

double truthy(double v) { return v != 0.0; }

using Comparator = bool (*)(double, double);
template <Comparator cmp>
void cmpExec(const NodeData&,
    UserInput&,
    const std::vector<std::span<double>>& in,
    std::span<double>& out,
    RunnerInput&)
{
    const auto& x = in[0];
    const auto& y = in[1];
    const int n = std::max((int)x.size(), (int)y.size());

    for (int i = 0; i < n; ++i) {
        const double xv = (i < (int)x.size()) ? x[i] : 0.0;
        const double yv = (i < (int)y.size()) ? y[i] : 0.0;
        out[i] = cmp(xv, yv) ? 1.0 : 0.0;
    }
}
template <Comparator cmp>
void makeCmp(const char* nm, const char* tip, std::vector<NodeType>& registry)
{
    NodeType t;
    t.name = nm;
    t.address = "logic/compare/";
    t.tooltip = tip;
    t.inputs = { InputFeatures("x", false,0,false),
                 InputFeatures("y", false,0,false) };
    t.getOutputSize = outputSizeComponentWise;
    t.buildUI = [](NodeComponent&, NodeData&) {};
    t.onResized = [](NodeComponent&) {};
    t.execute = &cmpExec<cmp>;   // <-- function pointer, no std::function
    t.isBoolean = true;
    t.alwaysOutputsRuntimeData = false;
    t.fromScene = nullptr;
    registry.push_back(t);
}


int outputSizeEqualsSingleInputSize(const std::vector<NodeData*>& inputs, const std::vector<std::vector<double>>& s, RunnerInput& r, int) {
	if (inputs.empty()) {
		return 1;
	}
	return inputs[0] ? inputs[0]->getCompileTimeSize(&r) : 1;
}



int outputSizeOuterProduct(const std::vector<NodeData*>& inputs, const std::vector<std::vector<double>>& s, RunnerInput& r, int) {
	int size = 1;
	bool getsPadded = false;
	for (const NodeData* input : inputs) {
		if (input) {
			size *= input->getCompileTimeSize(&r);
		}
	}
	return size;
}


int outputSizeLerpStyle(const std::vector<NodeData*>& inputs, const std::vector<std::vector<double>>& s, RunnerInput& c, int x) {
	std::vector<NodeData*> justTheFirstTwo;
	if (inputs.size() > 2) {
		justTheFirstTwo.push_back(inputs[0]);
		justTheFirstTwo.push_back(inputs[1]);
	}
	else {
		throw std::exception("Lerp style output size requires 2 lerp inputs and a interpolator");
	}
	return outputSizeComponentWise(justTheFirstTwo, s, c, x);
}

std::function<int(const std::vector<NodeData*>&, const std::vector<std::vector<double>>& s, RunnerInput& r, int)> outputSizeByInputPin(int pinIndex) {
	return [pinIndex](const std::vector<NodeData*>& inputs, const std::vector<std::vector<double>>& s, RunnerInput& r, int) {
		if (inputs.size() > pinIndex) {
			if (inputs[pinIndex]) {
				return inputs[pinIndex]->getCompileTimeSize(&r);
			}
		}
        return 1;
	};
}


void WaviateFlow2025AudioProcessor::initializeRegistry()
{
    registry.clear();

    // ========= output =========
    NodeType outputType;
    outputType.name = "output";
    outputType.address = "";
    outputType.tooltip = "Pass-through: forwards its single input to the output.";
    outputType.inputs = { InputFeatures("audio", false, 0, false) };
    outputType.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& inputs, std::span<double>& output, RunnerInput& inlineInstance)
        {
            for (int i = 0; i < static_cast<int>(inputs[0].size()); ++i) output[i] = inputs[0][i];
        };
    outputType.getOutputSize = outputSizeEqualsSingleInputSize;
    outputType.buildUI = [](NodeComponent& n, NodeData& d) {
        n.inputGUIElements.push_back(std::make_unique<juce::AudioVisualiserComponent>(1));
        auto* v = dynamic_cast<juce::AudioVisualiserComponent*>(n.inputGUIElements.back().get());
        n.addAndMakeVisible(v);
    };
    outputType.onResized = [](NodeComponent& n) {
        double scale = std::pow(2.0, n.getOwningScene()->logScale);
        float cornerSize = 24.0f * (float)scale;
        if (!n.inputGUIElements.empty()) {
            n.inputGUIElements.back()->setBounds(n.getWidth() / 2.0, cornerSize, n.getWidth() / 2.0, n.getHeight() - cornerSize * 2);
        }
    };
    outputType.isBoolean = false;
    outputType.alwaysOutputsRuntimeData = false;
    outputType.fromScene = nullptr;
    registry.push_back(outputType);

    // ========= input =========
    NodeType inputType;
    inputType.name = "input";
    inputType.address = "inputs/";
    inputType.tooltip = "Audio input placeholder (to be wired to a source).";
    inputType.inputs = {};
    inputType.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& i, std::span<double>& o, RunnerInput& inlineInstance)
    {
        if (!i.empty()) {
            for (int x = 0; x < o.size(); x += 1) {
                o[x] = i[0][x];
            }
        }
        else {
            for (int x = 0; x < o.size(); x += 1) {
                o[x] = 0.0;
            }
        }
        
    };
    inputType.isInputNode = true;
    inputType.getOutputSize = [](const std::vector<NodeData*>& inputNodes, const std::vector<std::vector<double>>& inputs, RunnerInput& inlineInstance, int inputNum) {
        auto* inlineComponent = dynamic_cast<NodeComponent*>(&inlineInstance);
        if (inlineComponent) {
            auto outerInput = inlineComponent->getNodeDataConst().getInput(inputNum);
            return outerInput ? outerInput->getCompileTimeSize(&inlineInstance) : 1;
        }

        return 1;
    };
    inputType.buildUI = [](NodeComponent& comp, NodeData& node)
        {
            node.setProperty("name", "input");
            comp.inputGUIElements.push_back(std::make_unique<juce::TextEditor>("name"));
            auto back = dynamic_cast<juce::TextEditor*>(comp.inputGUIElements.back().get());
            back->setJustification(juce::Justification::centred);
            back->setText("input");
            back->onTextChange = [&node, &comp]()
                {
                    auto el = dynamic_cast<juce::TextEditor*>(comp.inputGUIElements.back().get());
                    node.setProperty("name", el->getText());
                    comp.getOwningScene()->onSceneChanged();
                };
            back->setBounds(20, 20, comp.getWidth() - 40, 30);
            comp.addAndMakeVisible(back);
        };
    inputType.onResized = [](NodeComponent&) {};
    inputType.isBoolean = false;
    inputType.alwaysOutputsRuntimeData = false;
    inputType.fromScene = nullptr;
    registry.push_back(inputType);

    // ========= boolean input =========
    NodeType boolInputType;
    boolInputType.name = "boolean input";
    boolInputType.address = "inputs/";
    boolInputType.tooltip = "Boolean input placeholder (1.0 = true, 0.0 = false).";
    boolInputType.inputs = {};
    inputType.isInputNode = true;
    boolInputType.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& in, std::span<double>& out, RunnerInput& inlineInstance)
        {
            if (in.empty()) {
                for (int i = 0; i < out.size(); i += 1) {
                    out[i] = 0.0;
                }
            }
            else {
                for (int i = 0; i < out.size(); i += 1) {
                    out[i] = in[0][i] >= 0.5;
                }
            }
        };
    boolInputType.getOutputSize = inputType.getOutputSize; 
    boolInputType.buildUI = [](NodeComponent&, NodeData&) {};
    boolInputType.onResized = [](NodeComponent&) {};
    boolInputType.isBoolean = true;
    boolInputType.alwaysOutputsRuntimeData = false;
    boolInputType.fromScene = nullptr;
    registry.push_back(boolInputType);

    // ========= add =========
    NodeType addType;
    addType.name = "add";
    addType.address = "math/arithmetic/";
    addType.tooltip = "Component-wise addition; pads by copying the longer input.";
    addType.inputs = { InputFeatures("x", false, 0, false), InputFeatures("y", false, 0, false) };
    addType.getOutputSize = outputSizeComponentWise;
    addType.buildUI = [](NodeComponent&, NodeData&) {};
    addType.onResized = [](NodeComponent&) {};
    addType.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& inputs, std::span<double>& output, RunnerInput& inlineInstance)
        {
            const int a = static_cast<int>(inputs[0].size());
            const int b = static_cast<int>(inputs[1].size());
            const int n = std::min(a, b);
            const int m = std::max(a, b);
            const bool xIsBigger = a > b;

            for (int i = 0; i < n; ++i) output[i] = inputs[0][i] + inputs[1][i];
            if (xIsBigger) for (int i = n; i < m; ++i) output[i] = inputs[0][i];
            else           for (int i = n; i < m; ++i) output[i] = inputs[1][i];
        };
    addType.isBoolean = false;
    addType.alwaysOutputsRuntimeData = false;
    addType.fromScene = nullptr;
    registry.push_back(addType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("+").getTextDescription(), registry.size() - 1 });


    // ========= subtract =========
    NodeType subType;
    subType.name = "subtract";
    subType.address = "math/arithmetic/";
    subType.tooltip = "Component-wise x − y; pads with remaining of longer input (negated for y).";
    subType.inputs = { InputFeatures("x", false, 0, false), InputFeatures("y", false, 0, false) };
    subType.getOutputSize = outputSizeComponentWise;
    subType.buildUI = [](NodeComponent&, NodeData&) {};
    subType.onResized = [](NodeComponent&) {};
    subType.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& inputs, std::span<double>& output, RunnerInput& inlineInstance)
        {
            const int a = static_cast<int>(inputs[0].size());
            const int b = static_cast<int>(inputs[1].size());
            const int n = std::min(a, b);
            const int m = std::max(a, b);
            const bool xIsBigger = a > b;

            for (int i = 0; i < n; ++i) output[i] = inputs[0][i] - inputs[1][i];
            if (xIsBigger) for (int i = n; i < m; ++i) output[i] = inputs[0][i];
            else           for (int i = n; i < m; ++i) output[i] = -inputs[1][i];
        };
    subType.isBoolean = false;
    subType.alwaysOutputsRuntimeData = false;
    subType.fromScene = nullptr;
    registry.push_back(subType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("-").getTextDescription(), registry.size() - 1 });

    // ========= multiply =========
    NodeType mulType;
    mulType.name = "multiply";
    mulType.address = "math/arithmetic/";
    mulType.tooltip = "Component-wise multiplication; pads by copying the longer input.";
    mulType.inputs = { InputFeatures("x", false, 0, false), InputFeatures("y", false, 0, false) };
    mulType.getOutputSize = outputSizeComponentWise;
    mulType.buildUI = [](NodeComponent&, NodeData&) {};
    mulType.onResized = [](NodeComponent&) {};
    mulType.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& inputs, std::span<double>& output, RunnerInput& inlineInstance)
        {
            const int a = static_cast<int>(inputs[0].size());
            const int b = static_cast<int>(inputs[1].size());
            const int n = std::min(a, b);
            const int m = std::max(a, b);
            const bool xIsBigger = a > b;

            for (int i = 0; i < n; ++i) output[i] = inputs[0][i] * inputs[1][i];
            if (xIsBigger) for (int i = n; i < m; ++i) output[i] = inputs[0][i];
            else           for (int i = n; i < m; ++i) output[i] = inputs[1][i];
        };
    mulType.isBoolean = false;
    mulType.alwaysOutputsRuntimeData = false;
    mulType.fromScene = nullptr;
    registry.push_back(mulType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("*").getTextDescription(), registry.size() - 1 });

    // ========= divide =========
    NodeType divType;
    divType.name = "divide";
    divType.address = "math/arithmetic/";
    divType.tooltip = "Component-wise x / y; pads with x or zeros to avoid div-by-zero.";
    divType.inputs = { InputFeatures("x", false, 0, false), InputFeatures("y", false, 0, false) };
    divType.getOutputSize = outputSizeComponentWise;
    divType.buildUI = [](NodeComponent&, NodeData&) {};
    divType.onResized = [](NodeComponent&) {};
    divType.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& inputs, std::span<double>& output, RunnerInput& inlineInstance)
        {
            const int a = static_cast<int>(inputs[0].size());
            const int b = static_cast<int>(inputs[1].size());
            const int n = std::min(a, b);
            const int m = std::max(a, b);
            const bool xIsBigger = a > b;

            for (int i = 0; i < n; ++i)
                output[i] = (inputs[1][i] != 0.0) ? (inputs[0][i] / inputs[1][i]) : 0.0;

            if (xIsBigger) for (int i = n; i < m; ++i) output[i] = inputs[0][i];
            else           for (int i = n; i < m; ++i) output[i] = 0.0;
        };
    divType.isBoolean = false;
    divType.alwaysOutputsRuntimeData = false;
    divType.fromScene = nullptr;
    registry.push_back(divType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("/").getTextDescription(), registry.size() - 1 });
    // NOTE: Your original code *defines* modulo but never pushes it. To keep behavior identical,
    // we do not push a modulo node here. (See bug list below.)

    // ========= sin =========
    NodeType sinType;
    sinType.name = "sin";
    sinType.address = "math/trigonometry/";
    sinType.tooltip = "Applies sin(x) component-wise.";
    sinType.inputs = { InputFeatures("x", false, 0, false) };
    sinType.getOutputSize = outputSizeEqualsSingleInputSize;
    sinType.buildUI = [](NodeComponent&, NodeData&) {};
    sinType.onResized = [](NodeComponent&) {};
    sinType.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& inputs, std::span<double>& output, RunnerInput& inlineInstance)
        {
            for (int i = 0; i < static_cast<int>(inputs[0].size()); ++i) output[i] = std::sin(inputs[0][i]);
        };
    sinType.isBoolean = false;
    sinType.alwaysOutputsRuntimeData = false;
    sinType.fromScene = nullptr;
    registry.push_back(sinType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("alt s").getTextDescription(), registry.size() - 1 });

    // ========= cos =========
    NodeType cosType;
    cosType.name = "cos";
    cosType.address = "math/trigonometry/";
    cosType.tooltip = "Applies cos(x) component-wise.";
    cosType.inputs = { InputFeatures("x", false, 0, false) };
    cosType.getOutputSize = outputSizeEqualsSingleInputSize;
    cosType.buildUI = [](NodeComponent&, NodeData&) {};
    cosType.onResized = [](NodeComponent&) {};
    cosType.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& inputs, std::span<double>& output, RunnerInput& inlineInstance)
        {
            for (int i = 0; i < static_cast<int>(inputs[0].size()); ++i) output[i] = std::cos(inputs[0][i]);
        };
    cosType.isBoolean = false;
    cosType.alwaysOutputsRuntimeData = false;
    cosType.fromScene = nullptr;
    registry.push_back(cosType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("alt c").getTextDescription(), registry.size() - 1 });

    // ========= tan =========
    NodeType tanType;
    tanType.name = "tan";
    tanType.address = "math/trigonometry/";
    tanType.tooltip = "Applies tan(x) component-wise.";
    tanType.inputs = { InputFeatures("x", false, 0, false) };
    tanType.getOutputSize = outputSizeEqualsSingleInputSize;
    tanType.buildUI = [](NodeComponent&, NodeData&) {};
    tanType.onResized = [](NodeComponent&) {};
    tanType.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& inputs, std::span<double>& output, RunnerInput& inlineInstance)
        {
            for (int i = 0; i < static_cast<int>(inputs[0].size()); ++i) output[i] = std::tan(inputs[0][i]);
        };
    tanType.isBoolean = false;
    tanType.alwaysOutputsRuntimeData = false;
    tanType.fromScene = nullptr;
    registry.push_back(tanType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("alt t").getTextDescription(), registry.size() - 1 });

    // ========= atan2 =========
    NodeType atan2Type;
    atan2Type.name = "atan2";
    atan2Type.address = "math/trigonometry/inverse/";
    atan2Type.tooltip = "Component-wise atan2(y, x). Pads with the longer input.";
    atan2Type.inputs = { InputFeatures("y", false, 0, false), InputFeatures("x", false, 0, false) };
    atan2Type.getOutputSize = outputSizeComponentWise;
    atan2Type.buildUI = [](NodeComponent&, NodeData&) {};
    atan2Type.onResized = [](NodeComponent&) {};
    atan2Type.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& inputs, std::span<double>& output, RunnerInput& inlineInstance)
        {
            const int a = static_cast<int>(inputs[0].size());
            const int b = static_cast<int>(inputs[1].size());
            const int n = std::min(a, b);
            const int m = std::max(a, b);
            const bool yIsBigger = a > b;

            for (int i = 0; i < n; ++i) output[i] = std::atan2(inputs[0][i], inputs[1][i]);
            if (yIsBigger) for (int i = n; i < m; ++i) output[i] = inputs[0][i];
            else           for (int i = n; i < m; ++i) output[i] = inputs[1][i];
        };
    atan2Type.isBoolean = false;
    atan2Type.alwaysOutputsRuntimeData = false;
    atan2Type.fromScene = nullptr;
    registry.push_back(atan2Type);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("alt a").getTextDescription(), registry.size() - 1 });

    // ========= constant =========
    NodeType constantType;
    constantType.name = "constant";
    constantType.address = "math/";
    constantType.tooltip = "Outputs a fixed numeric value.";
    constantType.inputs = {};
    constantType.getOutputSize = outputSize1Known;
    constantType.execute = [](const NodeData& node, UserInput&, const std::vector<std::span<double>>&, std::span<double>& output, RunnerInput& inlineInstance)
        {
            output[0] = node.getNumericProperty("value");
        };
    constantType.buildUI = [](NodeComponent& comp, NodeData& node)
        {
            node.setProperty("value", 0.0);
            comp.inputGUIElements.push_back(std::make_unique<juce::TextEditor>("value"));
            auto back = dynamic_cast<juce::TextEditor*>(comp.inputGUIElements.back().get());
            back->setJustification(juce::Justification::centred);
            back->setText("0.0");
            back->onTextChange = [&node, &comp]()
                {
                    auto el = dynamic_cast<juce::TextEditor*>(comp.inputGUIElements.back().get());
                    try {
                        const double newVal = std::stod(el->getText().toStdString());
                        node.setProperty("value", newVal);
                    }
                    catch (...) {
                        el->setText(juce::String(node.getNumericProperty("value")));
                    }
                    comp.getOwningScene()->onSceneChanged();
                };
            back->setBounds(20, 20, comp.getWidth() - 40, 30);
            comp.addAndMakeVisible(back);
        };
    constantType.onResized = [](NodeComponent&) {};
    constantType.isBoolean = false;
    constantType.alwaysOutputsRuntimeData = false;
    constantType.fromScene = nullptr;
    registry.push_back(constantType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("c").getTextDescription(), registry.size() - 1 });

    // ========= constant boolean =========
    NodeType constBoolType;
    constBoolType.name = "constant boolean";
    constBoolType.address = "logic/";
    constBoolType.tooltip = "Outputs a fixed boolean (1 or 0).";
    constBoolType.inputs = {};
    constBoolType.getOutputSize = outputSize1Known;
    constBoolType.execute = [](const NodeData& node, UserInput&, const std::vector<std::span<double>>&, std::span<double>& output, RunnerInput& inlineInstance)
        {
            output[0] = node.getNumericProperty("value");
        };
    constBoolType.buildUI = [](NodeComponent& comp, NodeData& node)
        {
            const auto scale = std::pow(2.0, comp.getOwningScene()->logScale);
            comp.inputGUIElements.push_back(std::make_unique<juce::ToggleButton>(juce::String("true/false")));
            auto back = dynamic_cast<juce::ToggleButton*>(comp.inputGUIElements.back().get());
            back->onStateChange = [&]()
                {
                    auto el = dynamic_cast<juce::ToggleButton*>(comp.inputGUIElements.back().get());
                    node.setProperty("value", el->getToggleState() ? 1.0 : 0.0);
                    comp.getOwningScene()->onSceneChanged();
                };
            int zheight = static_cast<int>(30 * scale);
            back->setSize(comp.getWidth() - 4, zheight);
            int height = (comp.getHeight() - zheight) / 2.0;
            back->setTopLeftPosition(2, height);
            comp.addAndMakeVisible(back);
        };
    constBoolType.onResized = [](NodeComponent& comp)
        {
            if (comp.inputGUIElements.empty()) { return; }
            auto back = dynamic_cast<juce::ToggleButton*>(comp.inputGUIElements.back().get());
            const auto scale = std::pow(2.0, comp.getOwningScene()->logScale);
            int zheight = static_cast<int>(30 * scale);
            back->setSize(comp.getWidth() - 4, zheight);
            int height = (comp.getHeight() - zheight) / 2.0;
            back->setTopLeftPosition(2, height);
        };
    constBoolType.isBoolean = true;
    constBoolType.alwaysOutputsRuntimeData = false;
    constBoolType.fromScene = nullptr;
    registry.push_back(constBoolType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("b").getTextDescription(), registry.size() - 1 });

    // ========= branch =========
    NodeType branchType;
    branchType.name = "branch";
    branchType.address = "flow/";
    branchType.tooltip = "Selects between two inputs based on a boolean t/f.";
    branchType.inputs = {
        InputFeatures("true",  false, 0, false),
        InputFeatures("false", false, 0, false),
        InputFeatures("t/f",   true,  1, false)
    };
    branchType.getOutputSize = outputSizeComponentWise;
    branchType.buildUI = [](NodeComponent&, NodeData&) {};
    branchType.onResized = [](NodeComponent&) {};
    branchType.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& inputs, std::span<double>& output, RunnerInput& inlineInstance)
        {
            for (int i = 0; i < static_cast<int>(output.size()); ++i) output[i] = 0.0;
            const int n = std::min(static_cast<int>(inputs[0].size()), static_cast<int>(inputs[1].size()));
            if (!inputs[2].empty() && inputs[2][0] > 0.5)
                for (int i = 0; i < n; ++i) output[i] = inputs[0][i];
            else
                for (int i = 0; i < n; ++i) output[i] = inputs[1][i];
        };
    branchType.isBoolean = false;
    branchType.alwaysOutputsRuntimeData = false;
    branchType.fromScene = nullptr;
    registry.push_back(branchType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("shift b").getTextDescription(), registry.size() - 1 });

    // ========= comparison node =========
    NodeType comparisonType;
    comparisonType.name = "compare";
    comparisonType.address = "flow/";
    comparisonType.tooltip = "Selects between 3 inputs based on values of a and b.";
    comparisonType.inputs = {
        InputFeatures("a < b",  false, 0, false),
        InputFeatures("a == b", false, 0, false),
        InputFeatures("a > b",   false,  0, false),
        InputFeatures("a", false, 1, false),
        InputFeatures("b", false, 1, false)
    };
    comparisonType.getOutputSize = outputSizeComponentWise;
    comparisonType.buildUI = [](NodeComponent&, NodeData&) {};
    comparisonType.onResized = [](NodeComponent&) {};
    comparisonType.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& inputs,
        std::span<double>& output, RunnerInput&)
        {
            for (int i = 0; i < (int)output.size(); ++i) output[i] = 0.0;

            const auto& lt = inputs[0];  // use when a < b
            const auto& eq = inputs[1];  // use when a == b (also NaN fallback)
            const auto& gt = inputs[2];  // use when a > b
            const auto& a = inputs[3];
            const auto& b = inputs[4];

            auto at = [](const std::span<const double>& v, int i) -> double {
                if (v.empty()) return 0.0;
                return v.size() == 1 ? v[0] : v[i];
                };

            const int n = (int)output.size();
            for (int i = 0; i < n; ++i)
            {
                const double ai = at(a, i), bi = at(b, i);
                const auto& src = (ai < bi) ? lt : ((ai > bi) ? gt : eq);
                output[i] = at(src, i);
            }
        };
    comparisonType.isBoolean = false;
    comparisonType.alwaysOutputsRuntimeData = false;
    comparisonType.fromScene = nullptr;
    registry.push_back(branchType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("shift c").getTextDescription(), registry.size() - 1 });

    // ========= lerp =========
    NodeType lerpType;
    lerpType.name = "lerp";
    lerpType.address = "flow/";
    lerpType.tooltip = "Linearly blends between two inputs based on \"s\".";
    lerpType.inputs = {
        InputFeatures("a",  false, 0, false),
        InputFeatures("b", false, 0, false),
        InputFeatures("s",   false,  1, false)
    };
    lerpType.getOutputSize = outputSizeComponentWise;
    lerpType.buildUI = [](NodeComponent&, NodeData&) {};
    lerpType.onResized = [](NodeComponent&) {};
    lerpType.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& inputs, std::span<double>& output, RunnerInput& inlineInstance)
        {
            for (int i = 0; i < static_cast<int>(output.size()); ++i) output[i] = 0.0;
            const int n = std::min(static_cast<int>(inputs[0].size()), static_cast<int>(inputs[1].size()));
            if (!inputs[2].empty()) {
                double lerpFactor = std::max(0.0, std::min(1.0, inputs[2][0]));
                double inv = (1 - lerpFactor);
                for (int i = 0; i < n; ++i) {
                    output[i] = inputs[0][i] * inv + inputs[1][i] * lerpFactor;
                }
            }
        };
    lerpType.isBoolean = false;
    lerpType.alwaysOutputsRuntimeData = false;
    lerpType.fromScene = nullptr;
    registry.push_back(lerpType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("l").getTextDescription(), registry.size() - 1 });

    // ========= time since app started =========
    NodeType timeType;
    timeType.name = "time since app started";
    timeType.address = "time/";
    timeType.tooltip = "Seconds since the app started (runtime).";
    timeType.inputs = {};
    timeType.getOutputSize = outputSize1Known;
    timeType.execute = [](const NodeData&, UserInput& userInput, const std::vector<std::span<double>>&, std::span<double>& output, RunnerInput& inlineInstance)
        {
            const double trueFrame = userInput.numFramesStartOfBlock + userInput.sampleInBlock;
            output[0] = trueFrame / userInput.sampleRate;
        };
    timeType.buildUI = [](NodeComponent&, NodeData&) {};
    timeType.onResized = [](NodeComponent&) {};
    timeType.isBoolean = false;
    timeType.alwaysOutputsRuntimeData = true;
    timeType.fromScene = nullptr;
    registry.push_back(timeType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("t").getTextDescription(), registry.size() - 1 });


    // ========= not =========
    NodeType notType;
    notType.name = "not";
    notType.address = "logic/";
    notType.tooltip = "Logical NOT (1 if input == 0, else 0).";
    notType.inputs = { InputFeatures("a", true, 0, false) };
    notType.getOutputSize = outputSizeEqualsSingleInputSize;
    notType.buildUI = [](NodeComponent&, NodeData&) {};
    notType.onResized = [](NodeComponent&) {};
    notType.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& inputs, std::span<double>& output, RunnerInput& inlineInstance)
        {
            for (int i = 0; i < static_cast<int>(inputs[0].size()); ++i)
                output[i] = (inputs[0][i] == 0.0) ? 1.0 : 0.0;
        };
    notType.isBoolean = true;
    notType.alwaysOutputsRuntimeData = false;
    notType.fromScene = nullptr;
    registry.push_back(notType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("n").getTextDescription(), registry.size() - 1 });

    // ========= and =========
    NodeType andType;
    andType.name = "and";
    andType.address = "logic/";
    andType.tooltip = "Logical AND component-wise (nonzero treated as true).";
    andType.inputs = { InputFeatures("x", true, 0, false), InputFeatures("y", true, 0, false) };
    andType.getOutputSize = outputSizeComponentWise;
    andType.buildUI = [](NodeComponent&, NodeData&) {};
    andType.onResized = [](NodeComponent&) {};
    andType.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& inputs, std::span<double>& output, RunnerInput& inlineInstance)
        {
            const int a = static_cast<int>(inputs[0].size());
            const int b = static_cast<int>(inputs[1].size());
            const int n = std::min(a, b);
            const int m = std::max(a, b);
            for (int i = 0; i < n; ++i) output[i] = (inputs[0][i] != 0.0 && inputs[1][i] != 0.0) ? 1.0 : 0.0;
            for (int i = n; i < m; ++i) output[i] = 0.0;
        };
    andType.isBoolean = true;
    andType.alwaysOutputsRuntimeData = false;
    andType.fromScene = nullptr;
    registry.push_back(andType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("a").getTextDescription(), registry.size() - 1 });

    // ========= or =========
    NodeType orType;
    orType.name = "or";
    orType.address = "logic/";
    orType.tooltip = "Logical OR component-wise (nonzero treated as true).";
    orType.inputs = { InputFeatures("x", true, 0, false), InputFeatures("y", true, 0, false) };
    orType.getOutputSize = outputSizeComponentWise;
    orType.buildUI = [](NodeComponent&, NodeData&) {};
    orType.onResized = [](NodeComponent&) {};
    orType.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& inputs, std::span<double>& output, RunnerInput& inlineInstance)
        {
            const int a = static_cast<int>(inputs[0].size());
            const int b = static_cast<int>(inputs[1].size());
            const int n = std::min(a, b);
            const int m = std::max(a, b);
            const bool xIsBigger = a > b;

            for (int i = 0; i < n; ++i) output[i] = (inputs[0][i] != 0.0 || inputs[1][i] != 0.0) ? 1.0 : 0.0;
            if (xIsBigger) for (int i = n; i < m; ++i) output[i] = inputs[0][i];
            else           for (int i = n; i < m; ++i) output[i] = inputs[1][i];
        };
    orType.isBoolean = true;
    orType.alwaysOutputsRuntimeData = false;
    orType.fromScene = nullptr;
    registry.push_back(orType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("o").getTextDescription(), registry.size() - 1 });

    // ======== OUTER PRODUCTS ========
    NodeType outerAddType;
    outerAddType.name = "outer add";
    outerAddType.address = "math/arithmetic/outer product/";
    outerAddType.tooltip = "Computes all x_i + y_j into a flattened matrix.";
    outerAddType.inputs = { InputFeatures("x", false, 0, false), InputFeatures("y", false, 0, false) };
    outerAddType.getOutputSize = outputSizeOuterProduct;
    outerAddType.buildUI = [](NodeComponent&, NodeData&) {};
    outerAddType.onResized = [](NodeComponent&) {};
    outerAddType.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& inputs, std::span<double>& output, RunnerInput& inlineInstance)
        {
            for (int i = 0; i < static_cast<int>(inputs[0].size()); ++i) {
                const double xv = inputs[0][i];
                for (int j = 0; j < static_cast<int>(inputs[1].size()); ++j) {
                    const int z = j + i * static_cast<int>(inputs[1].size());
                    output[z] = xv + inputs[1][j];
                }
            }
        };
    outerAddType.isBoolean = false;
    outerAddType.alwaysOutputsRuntimeData = false;
    outerAddType.fromScene = nullptr;
    registry.push_back(outerAddType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("shift +").getTextDescription(), registry.size() - 1 });

    NodeType outerSubType;
    outerSubType.name = "outer subtract";
    outerSubType.address = "math/arithmetic/outer product/";
    outerSubType.tooltip = "Computes all x_i − y_j into a flattened matrix.";
    outerSubType.inputs = { InputFeatures("x", false, 0, false), InputFeatures("y", false, 0, false) };
    outerSubType.getOutputSize = outputSizeOuterProduct;
    outerSubType.buildUI = [](NodeComponent&, NodeData&) {};
    outerSubType.onResized = [](NodeComponent&) {};
    outerSubType.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& inputs, std::span<double>& output, RunnerInput& inlineInstance)
        {
            for (int i = 0; i < static_cast<int>(inputs[0].size()); ++i) {
                const double xv = inputs[0][i];
                for (int j = 0; j < static_cast<int>(inputs[1].size()); ++j) {
                    const int z = j + i * static_cast<int>(inputs[1].size());
                    output[z] = xv - inputs[1][j];
                }
            }
        };
    outerSubType.isBoolean = false;
    outerSubType.alwaysOutputsRuntimeData = false;
    outerSubType.fromScene = nullptr;
    registry.push_back(outerSubType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("shift -").getTextDescription(), registry.size() - 1 });

    NodeType outerMulType;
    outerMulType.name = "outer multiply";
    outerMulType.address = "math/arithmetic/outer product/";
    outerMulType.tooltip = "Computes all x_i * y_j into a flattened matrix.";
    outerMulType.inputs = { InputFeatures("x", false, 0, false), InputFeatures("y", false, 0, false) };
    outerMulType.inputs[0].defaultValue = 1.0;
    outerMulType.inputs[1].defaultValue = 1.0;
    outerMulType.getOutputSize = outputSizeOuterProduct;
    outerMulType.buildUI = [](NodeComponent&, NodeData&) {};
    outerMulType.onResized = [](NodeComponent&) {};
    outerMulType.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& inputs, std::span<double>& output, RunnerInput& inlineInstance)
        {
            for (int i = 0; i < static_cast<int>(inputs[0].size()); ++i) {
                const double xv = inputs[0][i];
                for (int j = 0; j < static_cast<int>(inputs[1].size()); ++j) {
                    const int z = j + i * static_cast<int>(inputs[1].size());
                    output[z] = xv * inputs[1][j];
                }
            }
        };
    outerMulType.isBoolean = false;
    outerMulType.alwaysOutputsRuntimeData = false;
    outerMulType.fromScene = nullptr;
    registry.push_back(outerMulType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("shift *").getTextDescription(), registry.size() - 1 });

    NodeType outerDivType;
    outerDivType.name = "outer divide";
    outerDivType.address = "math/arithmetic/outer product/";
    outerDivType.tooltip = "Computes all x_i / y_j into a flattened matrix (0 if y_j==0).";
    outerDivType.inputs = { InputFeatures("x", false, 0, false), InputFeatures("y", false, 0, false) };
    outerDivType.getOutputSize = outputSizeOuterProduct;
    outerDivType.buildUI = [](NodeComponent&, NodeData&) {};
    outerDivType.onResized = [](NodeComponent&) {};
    outerDivType.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& inputs, std::span<double>& output, RunnerInput& inlineInstance)
        {
            for (int i = 0; i < static_cast<int>(inputs[0].size()); ++i) {
                const double xv = inputs[0][i];
                for (int j = 0; j < static_cast<int>(inputs[1].size()); ++j) {
                    const int z = j + i * static_cast<int>(inputs[1].size());
                    const double yv = inputs[1][j];
                    output[z] = (yv != 0.0) ? (xv / yv) : 0.0;
                }
            }
        };
    outerDivType.isBoolean = false;
    outerDivType.alwaysOutputsRuntimeData = false;
    outerDivType.fromScene = nullptr;
    registry.push_back(outerDivType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("shift /").getTextDescription(), registry.size() - 1});

    // ======== VST / audio inputs ========
    NodeType sidechainType;
    sidechainType.name = "sidechain";
    sidechainType.address = "vst/";
    sidechainType.tooltip = "Sidechain input (L or R depending on channel).";
    sidechainType.inputs = {};
    sidechainType.getOutputSize = outputSize1Known;
    sidechainType.buildUI = [](NodeComponent&, NodeData&) {};
    sidechainType.onResized = [](NodeComponent&) {};
    sidechainType.execute = [](const NodeData&, UserInput& userInput, const std::vector<std::span<double>>&, std::span<double>& output, RunnerInput& inlineInstance)
        {
            output[0] = userInput.isStereoRight ? userInput.sideChainR : userInput.sideChainL;
        };
    sidechainType.isBoolean = false;
    sidechainType.alwaysOutputsRuntimeData = false;
    sidechainType.fromScene = nullptr;
    registry.push_back(sidechainType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("alt shift s").getTextDescription(), registry.size() - 1 });

    NodeType leftInType;
    leftInType.name = "left input";
    leftInType.address = "vst/";
    leftInType.tooltip = "Current frame's left-channel input sample.";
    leftInType.inputs = {};
    leftInType.getOutputSize = outputSize1Known;
    leftInType.buildUI = [](NodeComponent&, NodeData&) {};
    leftInType.onResized = [](NodeComponent&) {};
    leftInType.execute = [](const NodeData&, UserInput& userInput, const std::vector<std::span<double>>&, std::span<double>& output, RunnerInput& inlineInstance)
        {
            output[0] = userInput.leftInput;
        };
    leftInType.isBoolean = false;
    leftInType.alwaysOutputsRuntimeData = false;
    leftInType.fromScene = nullptr;
    registry.push_back(leftInType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("shift i").getTextDescription(), registry.size() - 1 });

    NodeType rightInType;
    rightInType.name = "right input";
    rightInType.address = "vst/";
    rightInType.tooltip = "Current frame's right-channel input sample.";
    rightInType.inputs = {};
    rightInType.getOutputSize = outputSize1Known;
    rightInType.buildUI = [](NodeComponent&, NodeData&) {};
    rightInType.onResized = [](NodeComponent&) {};
    rightInType.execute = [](const NodeData&, UserInput& userInput, const std::vector<std::span<double>>&, std::span<double>& output, RunnerInput& inlineInstance)
        {
            output[0] = userInput.rightInput;
        };
    rightInType.isBoolean = false;
    rightInType.alwaysOutputsRuntimeData = false;
    rightInType.fromScene = nullptr;
    registry.push_back(rightInType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("alt i").getTextDescription(), registry.size() - 1 });

    NodeType stereoInType;
    stereoInType.name = "stereo input";
    stereoInType.address = "vst/";
    stereoInType.tooltip = "Selects L or R for the current channel pass.";
    stereoInType.inputs = {};
    stereoInType.getOutputSize = outputSize1Known;
    stereoInType.buildUI = [](NodeComponent&, NodeData&) {};
    stereoInType.onResized = [](NodeComponent&) {};
    stereoInType.execute = [](const NodeData&, UserInput& userInput, const std::vector<std::span<double>>&, std::span<double>& output, RunnerInput& inlineInstance)
        {
            output[0] = userInput.isStereoRight ? userInput.rightInput : userInput.leftInput;
        };
    stereoInType.isBoolean = false;
    stereoInType.alwaysOutputsRuntimeData = false;
    stereoInType.fromScene = nullptr;
    registry.push_back(stereoInType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("i").getTextDescription(), registry.size() - 1 });


    // ======== MIDI ========
    NodeType noteOnType;
    noteOnType.name = "note on";
    noteOnType.address = "midi/";
    noteOnType.tooltip = "True if a given MIDI note (0,...,127) is currently on.";
    noteOnType.inputs = { InputFeatures("note ID", false, 0, false) };
    noteOnType.getOutputSize = outputSizeEqualsSingleInputSize;
    noteOnType.buildUI = [](NodeComponent&, NodeData&) {};
    noteOnType.onResized = [](NodeComponent&) {};
    noteOnType.execute = [](const NodeData&, UserInput& userInput, const std::vector<std::span<double>>& inputs, std::span<double>& output, RunnerInput& inlineInstance)
        {
            for (int i = 0; i < static_cast<int>(inputs[0].size()); ++i) {
                const int note = static_cast<int>(inputs[0][i]);
                output[i] = (note >= 0 && note < 128) ? userInput.notesOn[note] : 0.0;
            }
        };
    noteOnType.isBoolean = true;
    noteOnType.alwaysOutputsRuntimeData = true;
    noteOnType.fromScene = nullptr;
    registry.push_back(noteOnType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("alt n").getTextDescription(), registry.size() - 1 });

    NodeType velocity;
    velocity.name = "velocity";
    velocity.address = "midi/";
    velocity.tooltip = "Vector of 128 values for all MIDI note velocities";
    velocity.inputs = {};
    velocity.getOutputSize = [](const std::vector<NodeData*>&, const std::vector<std::vector<double>>&, RunnerInput&, int) { return 128; };
    velocity.buildUI = [](NodeComponent&, NodeData&) {};
    velocity.onResized = [](NodeComponent&) {};
    velocity.execute = [](const NodeData&, UserInput& userInput, const std::vector<std::span<double>>&, std::span<double>& output, RunnerInput& inlineInstance)
        {
            for (int i = 0; i < 128; i += 1) {
                output[i] = userInput.noteVelocity[i];
            }
            
        };
    velocity.isBoolean = false;
    velocity.alwaysOutputsRuntimeData = true;
    velocity.fromScene = nullptr;
    registry.push_back(velocity);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("v").getTextDescription(), registry.size() - 1 });


    NodeType pitchWheelType;
    pitchWheelType.name = "pitch wheel";
    pitchWheelType.address = "midi/";
    pitchWheelType.tooltip = "Current MIDI pitch wheel value (runtime).";
    pitchWheelType.inputs = {};
    pitchWheelType.getOutputSize = outputSize1Known;
    pitchWheelType.buildUI = [](NodeComponent&, NodeData&) {};
    pitchWheelType.onResized = [](NodeComponent&) {};
    pitchWheelType.execute = [](const NodeData&, UserInput& userInput, const std::vector<std::span<double>>&, std::span<double>& output, RunnerInput& inlineInstance)
        {
            output[0] = userInput.pitchWheelValue;
        };
    pitchWheelType.isBoolean = false;
    pitchWheelType.alwaysOutputsRuntimeData = true;
    pitchWheelType.fromScene = nullptr;
    registry.push_back(pitchWheelType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("p").getTextDescription(), registry.size() - 1 });

    NodeType allNotesType;
    allNotesType.name = "all note states";
    allNotesType.address = "midi/";
    allNotesType.tooltip = "Vector of 128 booleans for all MIDI notes.";
    allNotesType.inputs = {};
    allNotesType.getOutputSize = [](const std::vector<NodeData*>&, const std::vector<std::vector<double>>&, RunnerInput&, int) { return 128; };
    allNotesType.buildUI = [](NodeComponent&, NodeData&) {};
    allNotesType.onResized = [](NodeComponent&) {};
    allNotesType.execute = [](const NodeData&, UserInput& userInput, const std::vector<std::span<double>>&, std::span<double>& output, RunnerInput& inlineInstance)
        {
            for (int i = 0; i < 128; ++i) output[i] = userInput.notesOn[i];
        };
    allNotesType.isBoolean = true;
    allNotesType.alwaysOutputsRuntimeData = true;
    allNotesType.fromScene = nullptr;
    registry.push_back(allNotesType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("shift n").getTextDescription(), registry.size() - 1 });

    NodeType waveCycleType;
    waveCycleType.name = "wave cycle";
    waveCycleType.address = "midi/";
    waveCycleType.tooltip = "Per-note cycle phase in [0,1) for 128 MIDI notes.";
    waveCycleType.inputs = {};
    waveCycleType.getOutputSize = [](const std::vector<NodeData*>&, const std::vector<std::vector<double>>&, RunnerInput&, int) { return 128; };
    waveCycleType.buildUI = [](NodeComponent&, NodeData&) {};
    waveCycleType.onResized = [](NodeComponent&) {};
    waveCycleType.execute = [](const NodeData&, UserInput& userInput, const std::vector<std::span<double>>&, std::span<double>& output, RunnerInput& inlineInstance)
        {
            for (int i = 0; i < 128; ++i) output[i] = userInput.noteCycle[i];
        };
    waveCycleType.isBoolean = false;
    waveCycleType.alwaysOutputsRuntimeData = true;
    waveCycleType.fromScene = nullptr;
    registry.push_back(waveCycleType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("w").getTextDescription(), registry.size() - 1 });

    // ======== Reductions / vector ops ========
    NodeType sumType;
    sumType.name = "sum";
    sumType.address = "vector/reductions/";
    sumType.tooltip = "Sum of all elements.";
    sumType.inputs = { InputFeatures("x", false, 0, false) };
    sumType.getOutputSize = outputSize1Known;
    sumType.buildUI = [](NodeComponent&, NodeData&) {};
    sumType.onResized = [](NodeComponent&) {};
    sumType.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& inputs, std::span<double>& output, RunnerInput& inlineInstance)
        {
            double s = 0.0;
            for (double v : inputs[0]) s += v;
            output[0] = s;
        };
    sumType.isBoolean = false;
    sumType.alwaysOutputsRuntimeData = false;
    sumType.fromScene = nullptr;
    registry.push_back(sumType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("alt +").getTextDescription(), registry.size() - 1 });

    NodeType avgType;
    avgType.name = "average";
    avgType.address = "vector/reductions/";
    avgType.tooltip = "Arithmetic mean of all elements.";
    avgType.inputs = { InputFeatures("x", false, 0, false) };
    avgType.getOutputSize = outputSize1Known;
    avgType.buildUI = [](NodeComponent&, NodeData&) {};
    avgType.onResized = [](NodeComponent&) {};
    avgType.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& inputs, std::span<double>& output, RunnerInput& inlineInstance)
        {
            double s = 0.0; int c = 0;
            for (double v : inputs[0]) { s += v; ++c; }
            output[0] = (c > 0) ? (s / c) : 0.0;
        };
    avgType.isBoolean = false;
    avgType.alwaysOutputsRuntimeData = false;
    avgType.fromScene = nullptr;
    registry.push_back(avgType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("alt a").getTextDescription(), registry.size() - 1 });

    NodeType maxType;
    maxType.name = "max";
    maxType.address = "vector/reductions/";
    maxType.tooltip = "Maximum element.";
    maxType.inputs = { InputFeatures("x", false, 0, false) };
    maxType.getOutputSize = outputSize1Known;
    maxType.buildUI = [](NodeComponent&, NodeData&) {};
    maxType.onResized = [](NodeComponent&) {};
    maxType.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& inputs, std::span<double>& output, RunnerInput& inlineInstance)
        {
            if (inputs[0].empty()) { output[0] = 0.0; return; }
            double m = inputs[0][0];
            for (double v : inputs[0]) if (v > m) m = v;
            output[0] = m;
        };
    maxType.isBoolean = false;
    maxType.alwaysOutputsRuntimeData = false;
    maxType.fromScene = nullptr;
    registry.push_back(maxType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("alt m").getTextDescription(), registry.size() - 1 });

    NodeType minType;
    minType.name = "min";
    minType.address = "vector/reductions/";
    minType.tooltip = "Minimum element.";
    minType.inputs = { InputFeatures("x", false, 0, false) };
    minType.getOutputSize = outputSize1Known;
    minType.buildUI = [](NodeComponent&, NodeData&) {};
    minType.onResized = [](NodeComponent&) {};
    minType.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& inputs, std::span<double>& output, RunnerInput& inlineInstance)
        {
            if (inputs[0].empty()) { output[0] = 0.0; return; }
            double m = inputs[0][0];
            for (double v : inputs[0]) if (v < m) m = v;
            output[0] = m;
        };
    minType.isBoolean = false;
    minType.alwaysOutputsRuntimeData = false;
    minType.fromScene = nullptr;
    registry.push_back(minType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("shift alt m").getTextDescription(), registry.size() - 1 });

    NodeType getElemType;
    getElemType.name = "get element";
    getElemType.address = "vector/ops/";
    getElemType.tooltip = "Reads element at index from a vector.";
    getElemType.inputs = { InputFeatures("vector", false, 0, false), InputFeatures("index", false, 1, false) };
    getElemType.getOutputSize = outputSize1Known;
    getElemType.buildUI = [](NodeComponent&, NodeData&) {};
    getElemType.onResized = [](NodeComponent&) {};
    getElemType.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& inputs, std::span<double>& output, RunnerInput& inlineInstance)
        {
            if (inputs[0].empty() || inputs[1].empty()) { output[0] = 0.0; return; }
            const int idx = static_cast<int>(inputs[1][0]);
            if (idx < 0 || idx >= static_cast<int>(inputs[0].size())) { output[0] = 0.0; return; }
            output[0] = inputs[0][idx];
        };
    getElemType.isBoolean = false;
    getElemType.alwaysOutputsRuntimeData = false;
    getElemType.fromScene = nullptr;
    registry.push_back(getElemType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("alt e").getTextDescription(), registry.size() - 1 });

    NodeType sliceElemType;
   sliceElemType.name = "vector slice";
   sliceElemType.address = "vector/ops/";
   sliceElemType.tooltip = "Reads element at index from a vector.";
   sliceElemType.inputs = { InputFeatures("vector", false, 0, false), InputFeatures("indices", false, 0, false) };
   sliceElemType.getOutputSize = outputSizeByInputPin(1);
   sliceElemType.buildUI = [](NodeComponent&, NodeData&) {};
   sliceElemType.onResized = [](NodeComponent&) {};
   sliceElemType.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& inputs, std::span<double>& output, RunnerInput& inlineInstance)
    {
        for (int i = 0; i < inputs[1].size(); i += 1) {
            const int idx = static_cast<int>(inputs[1][i]);
            if (idx < 0 || idx >= static_cast<int>(inputs[0].size())) { output[i] = 0.0; }
            else {
                output[i] = inputs[0][i];
            }
            
        }
        
    };
    sliceElemType.isBoolean = false;
    sliceElemType.alwaysOutputsRuntimeData = false;
    sliceElemType.fromScene = nullptr;
    registry.push_back(sliceElemType);

    NodeType changeElemType;
    changeElemType.name = "change element";
    changeElemType.address = "vector/ops/";
    changeElemType.tooltip = "Copies vector and overwrites one element by index.";
    changeElemType.inputs = {
        InputFeatures("vector", false, 0, false),
        InputFeatures("index",  false, 1, false),
        InputFeatures("value",  false, 1, false)
    };
    changeElemType.getOutputSize = outputSizeComponentWise;
    changeElemType.buildUI = [](NodeComponent&, NodeData&) {};
    changeElemType.onResized = [](NodeComponent&) {};
    changeElemType.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& inputs, std::span<double>& output, RunnerInput& inlineInstance)
        {
            if (inputs[0].empty() || inputs[1].empty() || inputs[2].empty()) return;
            const int n = static_cast<int>(inputs[0].size());
            for (int i = 0; i < n; ++i) output[i] = inputs[0][i];
            const int idx = static_cast<int>(inputs[1][0]);
            if (idx < 0 || idx >= n) return;
            output[idx] = inputs[2][0];
        };
    changeElemType.isBoolean = false;
    changeElemType.alwaysOutputsRuntimeData = false;
    changeElemType.fromScene = nullptr;
    registry.push_back(changeElemType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("shift alt e").getTextDescription(), registry.size() - 1 });

    NodeType rangeType;
    rangeType.name = "range";
    rangeType.address = "vector/create/";
    rangeType.tooltip = "Creates a sequence: start, start+step, ... (step count items).";
    rangeType.inputs = {
        InputFeatures("start",      false, 1, false),
        InputFeatures("step count", false, 1, true),
        InputFeatures("step size",  false, 1, false)
    };
    for (auto& f : rangeType.inputs) f.defaultValue = 1.0;
    rangeType.getOutputSize = [](const std::vector<NodeData*>&, const std::vector<std::vector<double>>& s, RunnerInput&, int)
        {
            return std::max(1,static_cast<int>(std::round(s[1][0])));
        };
    rangeType.buildUI = [](NodeComponent&, NodeData&) {};
    rangeType.onResized = [](NodeComponent&) {};
    rangeType.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& inputs, std::span<double>& output, RunnerInput& inlineInstance)
        {
            int stepCount = static_cast<int>(std::round(inputs[1][0]));
            if (stepCount < 1) stepCount = 1;
            double value = inputs[0][0];
            const double step = inputs[2][0];
            for (int i = 0; i < stepCount; ++i) { output[i] = value; value += step; }
        };
    rangeType.isBoolean = false;
    rangeType.alwaysOutputsRuntimeData = false;
    rangeType.fromScene = nullptr;
    registry.push_back(rangeType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("r").getTextDescription(), registry.size() - 1 });

    // ======== Waves ========
    NodeType sinWaveType;
    sinWaveType.name = "sin wave";
    sinWaveType.address = "waves/";
    sinWaveType.tooltip = "Sine from phase in [0,1): sin(2π * phase).";
    sinWaveType.inputs = { InputFeatures("wave cycle", false, 0, false) };
    sinWaveType.getOutputSize = outputSizeEqualsSingleInputSize;
    sinWaveType.buildUI = [](NodeComponent&, NodeData&) {};
    sinWaveType.onResized = [](NodeComponent&) {};
    sinWaveType.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& inputs, std::span<double>& output, RunnerInput& inlineInstance)
        {
            for (int i = 0; i < static_cast<int>(output.size()); ++i)
                output[i] = std::sin(2.0 * 3.14159265358979323846 * inputs[0][i]);
        };
    sinWaveType.isBoolean = false;
    sinWaveType.alwaysOutputsRuntimeData = false;
    sinWaveType.fromScene = nullptr;
    registry.push_back(sinWaveType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("s").getTextDescription(), registry.size() - 1 });

    NodeType squareType;
    squareType.name = "square";
    squareType.address = "waves/";
    squareType.tooltip = "Square from phase in [0,1): outputs +1 or −1 at 0.5 threshold.";
    squareType.inputs = { InputFeatures("wave cycle", false, 0, false) };
    squareType.getOutputSize = outputSizeEqualsSingleInputSize;
    squareType.buildUI = [](NodeComponent&, NodeData&) {};
    squareType.onResized = [](NodeComponent&) {};
    squareType.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& inputs, std::span<double>& output, RunnerInput& inlineInstance)
        {
            for (int i = 0; i < static_cast<int>(output.size()); ++i)
                output[i] = (inputs[0][i] > 0.5) ? -1.0 : 1.0; // preserves original behavior
        };
    squareType.isBoolean = false;
    squareType.alwaysOutputsRuntimeData = false;
    squareType.fromScene = nullptr;
    registry.push_back(squareType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("q").getTextDescription(), registry.size() - 1 });

    NodeType triangleType;
    triangleType.name = "zigzag";
    triangleType.address = "waves/";
    triangleType.tooltip = "Triangle wave from phase in [0,1), scaled to [-1,1].";
    triangleType.inputs = { InputFeatures("wave cycle", false, 0, false) };
    triangleType.getOutputSize = outputSizeEqualsSingleInputSize;
    triangleType.buildUI = [](NodeComponent&, NodeData&) {};
    triangleType.onResized = [](NodeComponent&) {};
    triangleType.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& inputs, std::span<double>& output, RunnerInput& inlineInstance)
        {
            for (int i = 0; i < static_cast<int>(output.size()); ++i) {
                double t = std::fmod(inputs[0][i], 1.0); if (t < 0.0) t += 1.0;
                const double tri = 1.0 - std::abs(2.0 * t - 1.0);
                output[i] = 2.0 * tri - 1.0;
            }
        };
    triangleType.isBoolean = false;
    triangleType.alwaysOutputsRuntimeData = false;
    triangleType.fromScene = nullptr;
    registry.push_back(triangleType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("z").getTextDescription(), registry.size() - 1 });

    // ======== Envelope (ADSR, per MIDI note) ========
    NodeType envelopeType;
    envelopeType.name = "envelope (ADSR)";
    envelopeType.address = "midi/envelopes/";
    envelopeType.tooltip = "Per-note ADSR: outputs 128 values in [0,1] from note start/end and ADSR times.";
    envelopeType.inputs = {
        // scalar controls (seconds for A/D/R; [0,1] for sustain)
        InputFeatures("attack (s)",  false, 1, false),
        InputFeatures("decay (s)",   false, 1, false),
        InputFeatures("sustain",     false, 1, false),
        InputFeatures("release (s)", false, 1, false)
    };
    envelopeType.inputs[0].defaultValue = 0.01;
    envelopeType.inputs[0].defaultValue = 0.2;
    envelopeType.inputs[0].defaultValue = 0.8;
    envelopeType.inputs[0].defaultValue = 0.3;
    envelopeType.getOutputSize = [](const std::vector<NodeData*>&, const std::vector<std::vector<double>>&, RunnerInput&, int) {
        return 128; // one slot per MIDI note
        };
    envelopeType.buildUI = [](NodeComponent&, NodeData&) {};
    envelopeType.onResized = [](NodeComponent&) {};
    envelopeType.isBoolean = false;
    envelopeType.alwaysOutputsRuntimeData = true;
    envelopeType.fromScene = nullptr;

    envelopeType.execute = [](
        const NodeData&,
        UserInput& userInput,
        const std::vector<std::span<double>>& in,
        std::span<double>& out, RunnerInput& inlineInstance)
        {
            // Read scalar parameters (with sane clamps)
            const double A = (in.size() > 0 && !in[0].empty()) ? std::max(0.0, in[0][0]) : 0.0;
            const double D = (in.size() > 1 && !in[1].empty()) ? std::max(0.0, in[1][0]) : 0.0;
            const double S = (in.size() > 2 && !in[2].empty()) ? std::clamp(in[2][0], 0.0, 1.0) : 0.0;
            const double R = (in.size() > 3 && !in[3].empty()) ? std::max(0.0, in[3][0]) : 0.0;

            // Current time in frames and seconds
            const double curFrame = userInput.numFramesStartOfBlock + userInput.sampleInBlock;
            const double sr = (userInput.sampleRate > 0.0 ? userInput.sampleRate : 44100.0);

            // Helpers to avoid division by zero in linear ramps
            auto linUp = [](double t, double T) -> double {
                if (T <= 0.0) return 1.0;           // instantaneous to 1
                return std::clamp(t / T, 0.0, 1.0); // 0 -> 1
                };
            auto linDownTo = [](double t, double T, double target) -> double {
                // starts at 1.0 at t=0, linearly reaches target at t=T
                if (T <= 0.0) return target;
                double v = 1.0 - (t / T) * (1.0 - target);
                return std::clamp(v, target, 1.0);
                };
            auto releaseFrom = [](double t, double T, double startVal) -> double {
                if (T <= 0.0) return 0.0;
                double v = startVal * (1.0 - t / T);
                return std::clamp(v, 0.0, startVal);
                };

            // Compute per-note envelope
            for (int n = 0; n < 128; ++n) {
                const bool  on = (userInput.notesOn[n] != 0.0);
                const double tOnF = userInput.noteStartFrame[n];
                const double tOffF = userInput.noteEndFrame[n];

                // Default
                double env = 0.0;

                if (on) {
                    // NOTE IS HELD: time since key-down
                    const double tHeldSec = (curFrame - tOnF) / sr;
                    if (tHeldSec <= 0.0) {
                        env = 0.0;
                    }
                    else if (tHeldSec < A) {
                        // Attack: 0 -> 1
                        env = linUp(tHeldSec, A);
                    }
                    else if (tHeldSec < A + D) {
                        // Decay: 1 -> S
                        env = linDownTo(tHeldSec - A, D, S);
                    }
                    else {
                        // Sustain
                        env = S;
                    }
                }
                else {
                    // NOTE IS RELEASED: compute level at release moment, then decay
                    // If tOffF <= tOnF, or not meaningful, env=0 (no valid note)
                    const double holdDurSec = (tOffF - tOnF) / sr;
                    if (holdDurSec <= 0.0 || curFrame < tOffF) {
                        // No valid release, or we're before the off time
                        env = 0.0;
                    }
                    else {
                        // Level at the exact release moment:
                        double levelAtRelease = 0.0;
                        if (holdDurSec < A) {
                            levelAtRelease = linUp(holdDurSec, A); // still in attack when released
                        }
                        else if (holdDurSec < A + D) {
                            levelAtRelease = linDownTo(holdDurSec - A, D, S); // in decay when released
                        }
                        else {
                            levelAtRelease = S; // at sustain when released
                        }

                        // Time since release:
                        const double tRelSec = (curFrame - tOffF) / sr;
                        if (tRelSec <= 0.0) {
                            env = levelAtRelease;
                        }
                        else {
                            env = releaseFrom(tRelSec, R, levelAtRelease);
                        }
                    }
                }

                out[n] = std::clamp(env, 0.0, 1.0);
            }
        };

    registry.push_back(envelopeType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("e").getTextDescription(), registry.size() - 1 });

    // ========= round =========
    {
        NodeType t;
        t.name = "round";
        t.address = "math/ops/";
        t.tooltip = "Element-wise round to nearest integer.";
        t.inputs = { InputFeatures("x", false, 0, false) };
        t.getOutputSize = outputSizeEqualsSingleInputSize;
        t.buildUI = [](NodeComponent&, NodeData&) {};
        t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& in,
            std::span<double>& out, RunnerInput&)
            {
                const auto& x = in[0];
                for (int i = 0; i < (int)x.size(); ++i) out[i] = std::round(x[i]);
            };
        t.isBoolean = false; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr;
        registry.push_back(t);
        // (No hotkey assigned)
    }

    // ========= floor =========
    {
        NodeType t;
        t.name = "floor";
        t.address = "math/ops/";
        t.tooltip = "Element-wise floor.";
        t.inputs = { InputFeatures("x", false, 0, false) };
        t.getOutputSize = outputSizeEqualsSingleInputSize;
        t.buildUI = [](NodeComponent&, NodeData&) {};
        t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& in,
            std::span<double>& out, RunnerInput&)
            {
                const auto& x = in[0];
                for (int i = 0; i < (int)x.size(); ++i) out[i] = std::floor(x[i]);
            };
        t.isBoolean = false; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr;
        registry.push_back(t);
    }

    // ========= ceil =========
    {
        NodeType t;
        t.name = "ceil";
        t.address = "math/ops/";
        t.tooltip = "Element-wise ceil.";
        t.inputs = { InputFeatures("x", false, 0, false) };
        t.getOutputSize = outputSizeEqualsSingleInputSize;
        t.buildUI = [](NodeComponent&, NodeData&) {};
        t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& in,
            std::span<double>& out, RunnerInput&)
            {
                const auto& x = in[0];
                for (int i = 0; i < (int)x.size(); ++i) out[i] = std::ceil(x[i]);
            };
        t.isBoolean = false; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr;
        registry.push_back(t);
    }

    // ========= pow (x ^ yScalar) =========
    {
        NodeType t;
        t.name = "pow";
        t.address = "math/arithmetic/";
        t.tooltip = "Raises each element of x to the power y. y is scalar.";
        t.inputs = {
            InputFeatures("x", false, 0, false), // vector
            InputFeatures("y", false, 1, false)  // scalar exponent
        };
        t.getOutputSize = outputSizeEqualsSingleInputSize; // match x
        t.buildUI = [](NodeComponent&, NodeData&) {};
        t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& in,
            std::span<double>& out, RunnerInput&)
            {
                const auto& x = in[0];
                const double y = in[1].empty() ? 1.0 : in[1][0];
                for (int i = 0; i < (int)x.size(); ++i) out[i] = std::pow(std::abs(x[i]), y);
            };
        t.isBoolean = false; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr;
        registry.push_back(t);
        // Hotkey: caret (Shift+6)
        keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("shift 6").getTextDescription(),
                                    registry.size() - 1 });
    }

    // ========= pow (x ^ yScalar) =========
    {
        NodeType t;
        t.name = "signed pow";
        t.address = "math/arithmetic/";
        t.tooltip = "Raises each element of x to the power y. y is scalar.";
        t.inputs = {
            InputFeatures("x", false, 0, false), // vector
            InputFeatures("y", false, 1, false)  // scalar exponent
        };
        t.getOutputSize = outputSizeEqualsSingleInputSize; // match x
        t.buildUI = [](NodeComponent&, NodeData&) {};
        t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& in,
            std::span<double>& out, RunnerInput&)
            {
                const auto& x = in[0];
                const double y = in[1].empty() ? 1.0 : in[1][0];
                
                for (int i = 0; i < (int)x.size(); ++i) {
                    double sign = x[i] > 0 ? 1.0 : x[i] < 0 ? -1.0 : 0.0;
                    out[i] = std::pow(std::abs(x[i]), y);
                }
            };
        t.isBoolean = false; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr;
        registry.push_back(t);
        // Hotkey: caret (Shift+6)
        keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("alt shift 6").getTextDescription(),
                                    registry.size() - 1 });
    }

    // ========= log_n (base b scalar) =========
    {
        NodeType t;
        t.name = "log base b";
        t.address = "math/arithmetic/";
        t.tooltip = "Element-wise log_b(x) with scalar base b.";
        t.inputs = {
            InputFeatures("x", false, 0, false),
            InputFeatures("b", false, 1, false) // scalar base
        };
        t.getOutputSize = outputSizeEqualsSingleInputSize;
        t.buildUI = [](NodeComponent&, NodeData&) {};
        t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& in,
            std::span<double>& out, RunnerInput&)
            {
                const auto& x = in[0];
                const double b = in[1].empty() ? 2.0 : in[1][0];
                const double lb = (b > 0.0 && b != 1.0) ? std::log(b) : 0.0;
                for (int i = 0; i < (int)x.size(); ++i) {
                    const double v = x[i];
                    out[i] = (v > 0.0 && lb != 0.0) ? (std::log(v) / lb) : 0.0;
                }
            };
        t.isBoolean = false; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr;
        registry.push_back(t);
    }

    // ========= natural log (ln) =========
    {
        NodeType t;
        t.name = "ln";
        t.address = "math/arithmetic/";
        t.tooltip = "Element-wise natural logarithm.";
        t.inputs = { InputFeatures("x", false, 0, false) };
        t.getOutputSize = outputSizeEqualsSingleInputSize;
        t.buildUI = [](NodeComponent&, NodeData&) {};
        t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& in,
            std::span<double>& out, RunnerInput&)
            {
                const auto& x = in[0];
                for (int i = 0; i < (int)x.size(); ++i) {
                    const double v = x[i];
                    out[i] = (v > 0.0) ? std::log(v) : 0.0;
                }
            };
        t.isBoolean = false; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr;
        registry.push_back(t);
    }

    // ========= secant =========
    {
        NodeType t;
        t.name = "sec";
        t.address = "math/trigonometry/";
        t.tooltip = "Element-wise sec(x) = 1 / cos(x).";
        t.inputs = { InputFeatures("x", false, 0, false) };
        t.getOutputSize = outputSizeEqualsSingleInputSize;
        t.buildUI = [](NodeComponent&, NodeData&) {};
        t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& in,
            std::span<double>& out, RunnerInput&)
            {
                const auto& x = in[0];
                for (int i = 0; i < (int)x.size(); ++i) {
                    const double c = std::cos(x[i]);
                    out[i] = (c != 0.0) ? (1.0 / c) : 0.0;
                }
            };
        t.isBoolean = false; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr;
        registry.push_back(t);
    }

    // ========= cosecant =========
    {
        NodeType t;
        t.name = "csc";
        t.address = "math/trigonometry/";
        t.tooltip = "Element-wise csc(x) = 1 / sin(x).";
        t.inputs = { InputFeatures("x", false, 0, false) };
        t.getOutputSize = outputSizeEqualsSingleInputSize;
        t.buildUI = [](NodeComponent&, NodeData&) {};
        t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& in,
            std::span<double>& out, RunnerInput&)
            {
                const auto& x = in[0];
                for (int i = 0; i < (int)x.size(); ++i) {
                    const double s = std::sin(x[i]);
                    out[i] = (s != 0.0) ? (1.0 / s) : 0.0;
                }
            };
        t.isBoolean = false; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr;
        registry.push_back(t);
    }

    // ========= cotangent =========
    {
        NodeType t;
        t.name = "cot";
        t.address = "math/trigonometry/";
        t.tooltip = "Element-wise cot(x) = 1 / tan(x).";
        t.inputs = { InputFeatures("x", false, 0, false) };
        t.getOutputSize = outputSizeEqualsSingleInputSize;
        t.buildUI = [](NodeComponent&, NodeData&) {};
        t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& in,
            std::span<double>& out, RunnerInput&)
            {
                const auto& x = in[0];
                for (int i = 0; i < (int)x.size(); ++i) {
                    const double t = std::tan(x[i]);
                    out[i] = (t != 0.0) ? (1.0 / t) : 0.0;
                }
            };
        t.isBoolean = false; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr;
        registry.push_back(t);
    }

    // ========= clamp (generic) =========
    {
        NodeType t;
        t.name = "clamp";
        t.address = "math/range/";
        t.tooltip = "Clamp each element of x to [min, max].";
        t.inputs = {
            InputFeatures("x",   false, 0, false),
            InputFeatures("min", false, 1, false),
            InputFeatures("max", false, 1, false)
        };
        t.inputs[1].defaultValue = 0.0;
        t.inputs[2].defaultValue = 1.0;
        t.getOutputSize = outputSizeEqualsSingleInputSize;
        t.buildUI = [](NodeComponent&, NodeData&) {};
        t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& in,
            std::span<double>& out, RunnerInput&)
            {
                const auto& x = in[0];
                const double mn = in[1].empty() ? 0.0 : in[1][0];
                const double mx = in[2].empty() ? 1.0 : in[2][0];
                const double lo = std::min(mn, mx), hi = std::max(mn, mx);
                for (int i = 0; i < (int)x.size(); ++i)
                    out[i] = std::clamp(x[i], lo, hi);
            };
        t.isBoolean = false; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr;
        registry.push_back(t);
    }


    // ========= clamp [-1,1] =========
    {
        NodeType t;
        t.name = "clamp [-1,1]";
        t.address = "math/range/";
        t.tooltip = "Clamp to [-1, 1].";
        t.inputs = { InputFeatures("x", false, 0, false) };
        t.getOutputSize = outputSizeEqualsSingleInputSize;
        t.buildUI = [](NodeComponent&, NodeData&) {};
        t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& in,
            std::span<double>& out, RunnerInput&)
            {
                const auto& x = in[0];
                for (int i = 0; i < (int)x.size(); ++i)
                    out[i] = std::clamp(x[i], -1.0, 1.0);
            };
        t.isBoolean = false; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr;
        registry.push_back(t);
    }

    // ===== wrap helpers =====
    

    // ========= wrap (generic) =========
    {
        NodeType t;
        t.name = "wrap";
        t.address = "math/range/";
        t.tooltip = "Wrap each element into [min, max) (half-open).";
        t.inputs = {
            InputFeatures("x",   false, 0, false),
            InputFeatures("min", false, 1, false),
            InputFeatures("max", false, 1, false)
        };
        t.inputs[1].defaultValue = 0.0;
        t.inputs[2].defaultValue = 1.0;
        t.getOutputSize = outputSizeEqualsSingleInputSize;
        t.buildUI = [](NodeComponent&, NodeData&) {};
        t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& in,
            std::span<double>& out, RunnerInput&)
            {
                const auto& x = in[0];
                const double mn = in[1].empty() ? 0.0 : in[1][0];
                const double mx = in[2].empty() ? 1.0 : in[2][0];
                const double lo = std::min(mn, mx), hi = std::max(mn, mx);
                for (int i = 0; i < (int)x.size(); ++i)
                    out[i] = wrapTo(x[i], lo, hi);
            };
        t.isBoolean = false; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr;
        registry.push_back(t);
    }

    // ========= wrap [-1,1) =========
    {
        NodeType t;
        t.name = "wrap [-1,1)";
        t.address = "math/range/";
        t.tooltip = "Wrap to [-1, 1).";
        t.inputs = { InputFeatures("x", false, 0, false) };
        t.getOutputSize = outputSizeEqualsSingleInputSize;
        t.buildUI = [](NodeComponent&, NodeData&) {};
        t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& in,
            std::span<double>& out, RunnerInput&)
            {
                const auto& x = in[0];
                for (int i = 0; i < (int)x.size(); ++i)
                    out[i] = wrapTo(x[i], -1.0, 1.0);
            };
        t.isBoolean = false; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr;
        registry.push_back(t);
    }

    // ========= soft clamp (tanh) =========
    {
        NodeType t;
        t.name = "soft clamp (tanh)";
        t.address = "math/range/";
        t.tooltip = "Smoothly squashes x into (min, max) via y = m + r * tanh((x - m)/r) with m=(min+max)/2, r=(max-min)/2.";
        t.inputs = {
            InputFeatures("x",   false, 0, false),  // vector
            InputFeatures("min", false, 1, false),  // scalar
            InputFeatures("max", false, 1, false)   // scalar
        };
        t.inputs[1].defaultValue = 0.0;
        t.inputs[2].defaultValue = 1.0;
        t.getOutputSize = outputSizeEqualsSingleInputSize;
        t.buildUI = [](NodeComponent&, NodeData&) {};
        t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&,
            const std::vector<std::span<double>>& in,
            std::span<double>& out,
            RunnerInput&)
            {
                const auto& x = in[0];
                const double mnIn = in[1].empty() ? -1.0 : in[1][0];
                const double mxIn = in[2].empty() ? 1.0 : in[2][0];

                // Order the bounds safely
                const double lo = std::min(mnIn, mxIn);
                const double hi = std::max(mnIn, mxIn);

                // Center/half-range
                const double m = 0.5 * (lo + hi);
                double r = 0.5 * (hi - lo);

                // Degenerate range -> just hard clamp (or return m)
                constexpr double eps = 1e-12;
                const bool degenerate = !(r > 0.0);
                if (degenerate)
                {
                    for (int i = 0; i < (int)x.size(); ++i)
                        out[i] = std::clamp(x[i], lo, hi);
                    return;
                }

                // Soft clamp: y = m + r * tanh((x - m)/r)
                for (int i = 0; i < (int)x.size(); ++i)
                {
                    const double z = (x[i] - m) / (r + eps);
                    out[i] = m + r * std::tanh(z);
                }
            };
        t.isBoolean = false;
        t.alwaysOutputsRuntimeData = false;
        t.fromScene = nullptr;

        registry.push_back(t);
        // (No hotkey by default; say the word if you want one)
    }

    // ========= abs =========
    {
        NodeType t;
        t.name = "abs"; t.address = "math/ops/"; t.tooltip = "Element-wise absolute value.";
        t.inputs = { InputFeatures("x", false, 0, false) };
        t.getOutputSize = outputSizeEqualsSingleInputSize;
        t.buildUI = [](NodeComponent&, NodeData&) {}; t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& in, std::span<double>& out, RunnerInput&) {
            const auto& x = in[0]; for (int i = 0; i < (int)x.size(); ++i) out[i] = std::abs(x[i]);
            };
        t.isBoolean = false; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr;
        registry.push_back(t);
    }

    // ========= sign =========
    {
        NodeType t;
        t.name = "sign"; t.address = "math/ops/"; t.tooltip = "Element-wise sign: {-1,0,1}.";
        t.inputs = { InputFeatures("x", false, 0, false) };
        t.getOutputSize = outputSizeEqualsSingleInputSize;
        t.buildUI = [](NodeComponent&, NodeData&) {}; t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& in, std::span<double>& out, RunnerInput&) {
            const auto& x = in[0]; for (int i = 0; i < (int)x.size(); ++i) out[i] = (x[i] > 0) ? 1.0 : ((x[i] < 0) ? -1.0 : 0.0);
            };
        t.isBoolean = false; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr;
        registry.push_back(t);
    }

    // ========= sqrt =========
    {
        NodeType t;
        t.name = "sqrt"; t.address = "math/ops/"; t.tooltip = "Element-wise sqrt (negatives -> 0).";
        t.inputs = { InputFeatures("x", false, 0, false) };
        t.getOutputSize = outputSizeEqualsSingleInputSize;
        t.buildUI = [](NodeComponent&, NodeData&) {}; t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& in, std::span<double>& out, RunnerInput&) {
            const auto& x = in[0]; for (int i = 0; i < (int)x.size(); ++i) out[i] = (x[i] >= 0.0) ? std::sqrt(x[i]) : 0.0;
            };
        t.isBoolean = false; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr;
        registry.push_back(t);
    }

    // ========= nthroot (x^(1/nScalar)) =========
    {
        NodeType t;
        t.name = "nthroot"; t.address = "math/ops/"; t.tooltip = "Element-wise n-th root with scalar n.";
        t.inputs = { InputFeatures("x", false, 0, false), InputFeatures("n", false, 1, false) };
        t.getOutputSize = outputSizeEqualsSingleInputSize;
        t.buildUI = [](NodeComponent&, NodeData&) {}; t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& in, std::span<double>& out, RunnerInput&) {
            const auto& x = in[0]; const double n = in[1].empty() ? 2.0 : in[1][0];
            const double eps = 1e-12;
            const bool nIsInt = std::abs(n - std::round(n)) < 1e-9;
            const int  nInt = (int)std::llround(n);
            for (int i = 0; i < (int)x.size(); ++i) {
                const double v = x[i];
                if (std::abs(n) < eps) { out[i] = 0.0; continue; }
                if (v < 0.0) {
                    if (nIsInt && (nInt % 2 != 0)) { out[i] = -std::pow(std::abs(v), 1.0 / n); }
                    else { out[i] = 0.0; }
                }
                else {
                    out[i] = std::pow(v, 1.0 / n);
                }
            }
            };
        t.isBoolean = false; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr;
        registry.push_back(t);
    }

    // ========= exp =========
    {
        NodeType t;
        t.name = "exp"; t.address = "math/ops/"; t.tooltip = "Element-wise exp(x).";
        t.inputs = { InputFeatures("x", false, 0, false) };
        t.getOutputSize = outputSizeEqualsSingleInputSize;
        t.buildUI = [](NodeComponent&, NodeData&) {}; t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& in, std::span<double>& out, RunnerInput&) {
            const auto& x = in[0]; for (int i = 0; i < (int)x.size(); ++i) out[i] = std::exp(x[i]);
            };
        t.isBoolean = false; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr;
        registry.push_back(t);
    }

    // ========= arcsin / arccos / arctan =========
    {
        NodeType t;
        t.name = "asin"; t.address = "math/trigonometry/inverse/"; t.tooltip = "Element-wise asin(clamp(x,-1,1)).";
        t.inputs = { InputFeatures("x", false, 0, false) }; t.getOutputSize = outputSizeEqualsSingleInputSize;
        t.buildUI = [](NodeComponent&, NodeData&) {}; t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& in, std::span<double>& out, RunnerInput&) {
            const auto& x = in[0]; for (int i = 0; i < (int)x.size(); ++i) { const double v = std::clamp(x[i], -1.0, 1.0); out[i] = std::asin(v); } };
        t.isBoolean = false; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr; registry.push_back(t);
    }
    {
        NodeType t;
        t.name = "acos"; t.address = "math/trigonometry/inverse/"; t.tooltip = "Element-wise acos(clamp(x,-1,1)).";
        t.inputs = { InputFeatures("x", false, 0, false) }; t.getOutputSize = outputSizeEqualsSingleInputSize;
        t.buildUI = [](NodeComponent&, NodeData&) {}; t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& in, std::span<double>& out, RunnerInput&) {
            const auto& x = in[0]; for (int i = 0; i < (int)x.size(); ++i) { const double v = std::clamp(x[i], -1.0, 1.0); out[i] = std::acos(v); } };
        t.isBoolean = false; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr; registry.push_back(t);
    }
    {
        NodeType t;
        t.name = "atan"; t.address = "math/trigonometry/inverse/"; t.tooltip = "Element-wise atan(x).";
        t.inputs = { InputFeatures("x", false, 0, false) }; t.getOutputSize = outputSizeEqualsSingleInputSize;
        t.buildUI = [](NodeComponent&, NodeData&) {}; t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& in, std::span<double>& out, RunnerInput&) {
            const auto& x = in[0]; for (int i = 0; i < (int)x.size(); ++i) out[i] = std::atan(x[i]); };
        t.isBoolean = false; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr; registry.push_back(t);
    }

    // ========= mod (x mod mScalar), sign-safe to [0,|m|) =========
    {
        NodeType t;
        t.name = "mod"; t.address = "math/arithmetic/"; t.tooltip = "x mod m (scalar m), result in [0, |m|).";
        t.inputs = { InputFeatures("x", false, 0, false), InputFeatures("m", false, 1, false) };
        t.getOutputSize = outputSizeEqualsSingleInputSize;
        t.buildUI = [](NodeComponent&, NodeData&) {}; t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& in, std::span<double>& out, RunnerInput&) {
            const auto& x = in[0]; const double m = in[1].empty() ? 1.0 : in[1][0]; const double am = std::abs(m);
            for (int i = 0; i < (int)x.size(); ++i) { double r = am > 0 ? std::fmod(x[i], am) : 0.0; if (r < 0) r += am; out[i] = r; }
            };
        t.isBoolean = false; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr; registry.push_back(t);
    }

    // ========= reciprocal =========
    {
        NodeType t;
        t.name = "reciprocal"; t.address = "math/ops/"; t.tooltip = "Element-wise 1/x (0 -> 0).";
        t.inputs = { InputFeatures("x", false, 0, false) };
        t.getOutputSize = outputSizeEqualsSingleInputSize;
        t.buildUI = [](NodeComponent&, NodeData&) {}; t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& in, std::span<double>& out, RunnerInput&) {
            const auto& x = in[0]; for (int i = 0; i < (int)x.size(); ++i) out[i] = (x[i] != 0.0) ? 1.0 / x[i] : 0.0;
            };
        t.isBoolean = false; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr; registry.push_back(t);
    }

    // ========= XOR / NAND / NOR / XNOR (component-wise boolean) =========
    
    {
        NodeType t; t.name = "xor"; t.address = "logic/binary ops/"; t.tooltip = "Boolean XOR component-wise.";
        t.inputs = { InputFeatures("x", true, 0, false), InputFeatures("y", true, 0, false) };
        t.getOutputSize = outputSizeComponentWise; t.buildUI = [](NodeComponent&, NodeData&) {}; t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& in, std::span<double>& out, RunnerInput&) {
            const auto& a = in[0], & b = in[1]; const int n = std::max((int)a.size(), (int)b.size());
            for (int i = 0; i < n; ++i) {
                const bool av = (i < (int)a.size() ? truthy(a[i]) : false);
                const bool bv = (i < (int)b.size() ? truthy(b[i]) : false);
                out[i] = (av ^ bv) ? 1.0 : 0.0;
            }
            };
        t.isBoolean = true; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr; registry.push_back(t);
    }
    {
        NodeType t; t.name = "nand"; t.address = "logic/binary ops/"; t.tooltip = "NOT (x AND y) component-wise.";
        t.inputs = { InputFeatures("x", true, 0, false), InputFeatures("y", true, 0, false) };
        t.getOutputSize = outputSizeComponentWise; t.buildUI = [](NodeComponent&, NodeData&) {}; t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& in, std::span<double>& out, RunnerInput&) {
            const auto& a = in[0], & b = in[1]; const int n = std::max((int)a.size(), (int)b.size());
            for (int i = 0; i < n; ++i) {
                const bool v = (i < (int)a.size() ? truthy(a[i]) : false) && (i < (int)b.size() ? truthy(b[i]) : false);
                out[i] = v ? 0.0 : 1.0;
            }
            };
        t.isBoolean = true; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr; registry.push_back(t);
    }
    {
        NodeType t; t.name = "nor"; t.address = "logic/binary ops/"; t.tooltip = "NOT (x OR y) component-wise.";
        t.inputs = { InputFeatures("x", true, 0, false), InputFeatures("y", true, 0, false) };
        t.getOutputSize = outputSizeComponentWise; t.buildUI = [](NodeComponent&, NodeData&) {}; t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& in, std::span<double>& out, RunnerInput&) {
            const auto& a = in[0], & b = in[1]; const int n = std::max((int)a.size(), (int)b.size());
            for (int i = 0; i < n; ++i) {
                const bool v = (i < (int)a.size() ? truthy(a[i]) : false) || (i < (int)b.size() ? truthy(b[i]) : false);
                out[i] = v ? 0.0 : 1.0;
            }
            };
        t.isBoolean = true; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr; registry.push_back(t);
    }
    {
        NodeType t; t.name = "xnor"; t.address = "logic/binary ops/"; t.tooltip = "NOT XOR component-wise (equality).";
        t.inputs = { InputFeatures("x", true, 0, false), InputFeatures("y", true, 0, false) };
        t.getOutputSize = outputSizeComponentWise; t.buildUI = [](NodeComponent&, NodeData&) {}; t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& in, std::span<double>& out, RunnerInput&) {
            const auto& a = in[0], & b = in[1]; const int n = std::max((int)a.size(), (int)b.size());
            for (int i = 0; i < n; ++i) {
                const bool av = (i < (int)a.size() ? truthy(a[i]) : false);
                const bool bv = (i < (int)b.size() ? truthy(b[i]) : false);
                out[i] = (av == bv) ? 1.0 : 0.0;
            }
            };
        t.isBoolean = true; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr; registry.push_back(t);
    }

    // ========= where (vectorized branch) =========
    {
        NodeType t;
        t.name = "where"; t.address = "flow/"; t.tooltip = "Vectorized branch: out[i] = cond[i]? a[i] : b[i]. If cond is padded, defaults to picking a.";
        t.inputs = {
            InputFeatures("cond", true, 0, false),
            InputFeatures("a",    false,0, false),
            InputFeatures("b",    false,0, false)
        };
        t.getOutputSize = outputSizeComponentWise;
        t.buildUI = [](NodeComponent&, NodeData&) {}; t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& in, std::span<double>& out, RunnerInput&) {
            const auto& c = in[0]; const auto& a = in[1]; const auto& b = in[2];
            const int n = std::max((int)a.size(), (int)b.size());
            for (int i = 0; i < n; ++i) {
                const bool cv = (i < (int)c.size()) ? truthy(c[i]) : true; // default to 'a'
                const double av = (i < (int)a.size()) ? a[i] : 0.0;
                const double bv = (i < (int)b.size()) ? b[i] : 0.0;
                out[i] = cv ? av : bv;
            }
            };
        t.isBoolean = false; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr; registry.push_back(t);
    }

    // ========= Comparators: gt/ge/lt/le/eq =========

    makeCmp< [](double a, double b) {return a > b; }>("greater", "x > y (component-wise; padding uses 0).", registry);
    makeCmp< [](double a, double b) {return a < b; } >("less", "x < y (component-wise; padding uses 0).", registry);
    makeCmp< [](double a, double b) {return a >= b; } >("greater eq", "x >= y (component-wise; padding uses 0).", registry);
    makeCmp< [](double a, double b) {return a <= b; } >("less eq", "x <= y (component-wise; padding uses 0).", registry);
    makeCmp< [](double a, double b) {return a == b; } >("equal", "x == y (component-wise; padding uses 0).", registry);

    // ========= is between / is not between (inclusive) =========
    {
        NodeType t; t.name = "is between"; t.address = "logic/compare/"; t.tooltip = "Inclusive: lo <= x <= hi. lo, hi are scalars.";
        t.inputs = { InputFeatures("x", false,0,false), InputFeatures("lo", false,1,false), InputFeatures("hi", false,1,false) };
        t.getOutputSize = outputSizeEqualsSingleInputSize; t.buildUI = [](NodeComponent&, NodeData&) {}; t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& in, std::span<double>& out, RunnerInput&) {
            const auto& x = in[0]; double lo = in[1].empty() ? 0.0 : in[1][0], hi = in[2].empty() ? 1.0 : in[2][0];
            if (lo > hi) std::swap(lo, hi); for (int i = 0; i < (int)x.size(); ++i) out[i] = (x[i] >= lo && x[i] <= hi) ? 1.0 : 0.0;
            };
        t.isBoolean = true; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr; registry.push_back(t);
    }
    {
        NodeType t; t.name = "is not between"; t.address = "logic/compare/"; t.tooltip = "Complement of 'is between'.";
        t.inputs = { InputFeatures("x", false,0,false), InputFeatures("lo", false,1,false), InputFeatures("hi", false,1,false) };
        t.getOutputSize = outputSizeEqualsSingleInputSize; t.buildUI = [](NodeComponent&, NodeData&) {}; t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& in, std::span<double>& out, RunnerInput&) {
            const auto& x = in[0]; double lo = in[1].empty() ? 0.0 : in[1][0], hi = in[2].empty() ? 1.0 : in[2][0];
            if (lo > hi) std::swap(lo, hi); for (int i = 0; i < (int)x.size(); ++i) out[i] = (x[i]<lo || x[i]>hi) ? 1.0 : 0.0;
            };
        t.isBoolean = true; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr; registry.push_back(t);
    }

    // ========= Boolean reductions =========
    {
        NodeType t; t.name = "any true"; t.address = "vector/reductions/boolean/"; t.tooltip = "1 if any element is nonzero, else 0.";
        t.inputs = { InputFeatures("x", true, 0, false) }; t.getOutputSize = outputSize1Known;
        t.buildUI = [](NodeComponent&, NodeData&) {}; t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& in, std::span<double>& out, RunnerInput&) {
            bool any = false; for (double v : in[0]) if (truthy(v)) { any = true; break; } out[0] = any ? 1.0 : 0.0;
            };
        t.isBoolean = true; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr; registry.push_back(t);
    }
    {
        NodeType t; t.name = "any false"; t.address = "vector/reductions/boolean/"; t.tooltip = "1 if any element is zero, else 0.";
        t.inputs = { InputFeatures("x", true, 0, false) }; t.getOutputSize = outputSize1Known;
        t.buildUI = [](NodeComponent&, NodeData&) {}; t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& in, std::span<double>& out, RunnerInput&) {
            bool any = false; for (double v : in[0]) if (!truthy(v)) { any = true; break; } out[0] = any ? 1.0 : 0.0;
            };
        t.isBoolean = true; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr; registry.push_back(t);
    }
    {
        NodeType t; t.name = "all true"; t.address = "vector/reductions/boolean/"; t.tooltip = "1 if all elements are nonzero (empty -> 1).";
        t.inputs = { InputFeatures("x", true, 0, false) }; t.getOutputSize = outputSize1Known;
        t.buildUI = [](NodeComponent&, NodeData&) {}; t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& in, std::span<double>& out, RunnerInput&) {
            bool all = true; for (double v : in[0]) if (!truthy(v)) { all = false; break; } out[0] = all ? 1.0 : 0.0;
            };
        t.isBoolean = true; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr; registry.push_back(t);
    }
    {
        NodeType t; t.name = "all false"; t.address = "vector/reductions/boolean/"; t.tooltip = "1 if all elements are zero (empty -> 1).";
        t.inputs = { InputFeatures("x", true, 0, false) }; t.getOutputSize = outputSize1Known;
        t.buildUI = [](NodeComponent&, NodeData&) {}; t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& in, std::span<double>& out, RunnerInput&) {
            bool all = true; for (double v : in[0]) if (truthy(v)) { all = false; break; } out[0] = all ? 1.0 : 0.0;
            };
        t.isBoolean = true; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr; registry.push_back(t);
    }
    {
        NodeType t; t.name = "count true"; t.address = "vector/reductions/boolean/"; t.tooltip = "Counts nonzero elements.";
        t.inputs = { InputFeatures("x", true, 0, false) }; t.getOutputSize = outputSize1Known;
        t.buildUI = [](NodeComponent&, NodeData&) {}; t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& in, std::span<double>& out, RunnerInput&) {
            int c = 0; for (double v : in[0]) if (truthy(v)) ++c; out[0] = (double)c;
            };
        t.isBoolean = false; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr; registry.push_back(t);
    }
    {
        NodeType t; t.name = "count false"; t.address = "vector/reductions/boolean/"; t.tooltip = "Counts zeros.";
        t.inputs = { InputFeatures("x", true, 0, false) }; t.getOutputSize = outputSize1Known;
        t.buildUI = [](NodeComponent&, NodeData&) {}; t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& in, std::span<double>& out, RunnerInput&) {
            int c = 0; for (double v : in[0]) if (!truthy(v)) ++c; out[0] = (double)c;
            };
        t.isBoolean = false; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr; registry.push_back(t);
    }

    // ========= concat (a || b) =========
    {
        NodeType t;
        t.name = "concat"; t.address = "vector/ops/"; t.tooltip = "Concatenate a then b.";
        t.inputs = { InputFeatures("a", false,0,false), InputFeatures("b", false,0,false) };
        t.getOutputSize = [](const std::vector<NodeData*>& nodes, const std::vector<std::vector<double>>&, RunnerInput& ri, int) {
            int as = (nodes.size() > 0 && nodes[0]) ? nodes[0]->getCompileTimeSize(&ri) : 0;
            int bs = (nodes.size() > 1 && nodes[1]) ? nodes[1]->getCompileTimeSize(&ri) : 0;
            return as + bs;
            };
        t.buildUI = [](NodeComponent&, NodeData&) {}; t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& in, std::span<double>& out, RunnerInput&) {
            const auto& a = in[0], & b = in[1]; int idx = 0; for (double v : a) out[idx++] = v; for (double v : b) out[idx++] = v;
            };
        t.isBoolean = false; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr; registry.push_back(t);
    }

    // ========= pad_to (truncate or pad with value) =========
    {
        NodeType t;
        t.name = "pad to"; t.address = "vector/ops/"; t.tooltip = "Resize to N: copy then pad with value.";
        t.inputs = {
            InputFeatures("x",     false,0,false),
            InputFeatures("sizeN", false,1,true),
            InputFeatures("value", false,1,false)
        };
        t.inputs[2].defaultValue = 0.0;
        t.getOutputSize = [](const std::vector<NodeData*>&, const std::vector<std::vector<double>>& s, RunnerInput&, int) {
            const int N = (s.size() > 1 && !s[1].empty()) ? (int)std::max(0.0, std::round(s[1][0])) : 0;
            return N;
            };
        t.buildUI = [](NodeComponent&, NodeData&) {}; t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& in, std::span<double>& out, RunnerInput&) {
            const auto& x = in[0]; const int N = (int)out.size(); const double v = in[2].empty() ? 0.0 : in[2][0];
            const int nCopy = std::min((int)x.size(), N);
            for (int i = 0; i < nCopy; ++i) out[i] = x[i];
            for (int i = nCopy; i < N; ++i) out[i] = v;
            };
        t.isBoolean = false; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr; registry.push_back(t);
    }

    // ========= append zeros =========
    {
        NodeType t;
        t.name = "append zeros"; t.address = "vector/ops/"; t.tooltip = "Append K zeros to x.";
        t.inputs = { InputFeatures("x", false,0,false), InputFeatures("K", false,1,true) };
        t.getOutputSize = [](const std::vector<NodeData*>& nodes, const std::vector<std::vector<double>>& s, RunnerInput& ri, int) {
            int xs = (nodes.size() > 0 && nodes[0]) ? nodes[0]->getCompileTimeSize(&ri) : 0;
            int K = (s.size() > 1 && !s[1].empty()) ? (int)std::max(0.0, std::round(s[1][0])) : 0;
            return xs + K;
            };
        t.buildUI = [](NodeComponent&, NodeData&) {}; t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& in, std::span<double>& out, RunnerInput&) {
            const auto& x = in[0]; const int xs = (int)x.size(); const int N = (int)out.size();
            for (int i = 0; i < xs && i < N; ++i) out[i] = x[i];
            for (int i = xs; i < N; ++i) out[i] = 0.0;
            };
        t.isBoolean = false; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr; registry.push_back(t);
    }

    // ========= reverse =========
    {
        NodeType t;
        t.name = "reverse"; t.address = "vector/ops/"; t.tooltip = "Reverse order of elements.";
        t.inputs = { InputFeatures("x", false,0,false) }; t.getOutputSize = outputSizeEqualsSingleInputSize;
        t.buildUI = [](NodeComponent&, NodeData&) {}; t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& in, std::span<double>& out, RunnerInput&) {
            const auto& x = in[0]; const int n = (int)x.size(); for (int i = 0; i < n; ++i) out[i] = x[n - 1 - i];
            };
        t.isBoolean = false; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr; registry.push_back(t);
    }

    // ========= interleave (a0,b0,a1,b1,...) then remainder =========
    {
        NodeType t;
        t.name = "interleave"; t.address = "vector/ops/"; t.tooltip = "Alternate elements from a and b; append remainder.";
        t.inputs = { InputFeatures("a", false,0,false), InputFeatures("b", false,0,false) };
        t.getOutputSize = [](const std::vector<NodeData*>& nodes, const std::vector<std::vector<double>>&, RunnerInput& ri, int) {
            int as = (nodes.size() > 0 && nodes[0]) ? nodes[0]->getCompileTimeSize(&ri) : 0;
            int bs = (nodes.size() > 1 && nodes[1]) ? nodes[1]->getCompileTimeSize(&ri) : 0;
            return as + bs;
            };
        t.buildUI = [](NodeComponent&, NodeData&) {}; t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& in, std::span<double>& out, RunnerInput&) {
            const auto& a = in[0]; const auto& b = in[1];
            int ia = 0, ib = 0, k = 0, na = (int)a.size(), nb = (int)b.size();
            while (ia < na && ib < nb) { out[k++] = a[ia++]; out[k++] = b[ib++]; }
            while (ia < na) out[k++] = a[ia++];
            while (ib < nb) out[k++] = b[ib++];
            };
        t.isBoolean = false; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr; registry.push_back(t);
    }

    // ========= argmax / argmin =========
    {
        NodeType t; t.name = "argmax"; t.address = "vector/reductions/"; t.tooltip = "Index of first maximum (0 if empty).";
        t.inputs = { InputFeatures("x", false,0,false) }; t.getOutputSize = outputSize1Known;
        t.buildUI = [](NodeComponent&, NodeData&) {}; t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& in, std::span<double>& out, RunnerInput&) {
            const auto& x = in[0]; if (x.empty()) { out[0] = 0.0; return; }
            int idx = 0; double best = x[0]; for (int i = 1; i < (int)x.size(); ++i) if (x[i] > best) { best = x[i]; idx = i; } out[0] = (double)idx;
            };
        t.isBoolean = false; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr; registry.push_back(t);
    }
    {
        NodeType t; t.name = "argmin"; t.address = "vector/reductions/"; t.tooltip = "Index of first minimum (0 if empty).";
        t.inputs = { InputFeatures("x", false,0,false) }; t.getOutputSize = outputSize1Known;
        t.buildUI = [](NodeComponent&, NodeData&) {}; t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& in, std::span<double>& out, RunnerInput&) {
            const auto& x = in[0]; if (x.empty()) { out[0] = 0.0; return; }
            int idx = 0; double best = x[0]; for (int i = 1; i < (int)x.size(); ++i) if (x[i] < best) { best = x[i]; idx = i; } out[0] = (double)idx;
            };
        t.isBoolean = false; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr; registry.push_back(t);
    }

    // ========= size of =========
    {
        NodeType t; t.name = "size of"; t.address = "utility/"; t.tooltip = "Length of the input vector.";
        t.inputs = { InputFeatures("x", false,0,false) }; t.getOutputSize = outputSize1Known;
        t.buildUI = [](NodeComponent&, NodeData&) {}; t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<double>>& in, std::span<double>& out, RunnerInput&) {
            out[0] = (double)in[0].size();
            };
        t.isBoolean = false; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr; registry.push_back(t);
    }

    // ======== sliding window
    {
        NodeType t;
        t.name = "sliding window"; t.address = "filters/"; t.tooltip = "Computes weighted average of the last n audio samples (n = weights.size - 1)";
		t.inputs = { InputFeatures("current sample", false, 1, false), InputFeatures("weights", false, 0, false)}; t.getOutputSize = outputSize1Known;
		t.buildUI = [](NodeComponent& nc, NodeData& nd) {};
		t.execute = [](const NodeData&, UserInput& u, const std::vector<std::span<double>>& in, std::span<double>& out, RunnerInput&) {
            out[0] = in[1][0] * in[0][0];
            for (int i = 1; i < in[1].size(); i += 1) {
				out[0] += in[1][i] * u.getHistoricalSample(i - 1);
            }
		};
        t.isBoolean = false; t.alwaysOutputsRuntimeData = true; t.fromScene = nullptr; registry.push_back(t);
    }

    // ======== sliding window
    {
        NodeType t;
		t.name = "FIR filter"; t.address = "filters/"; t.tooltip = "Averages current input with n previous samples. As n increases, this node takes longer to compute but filters out more high frequencies.";
		t.inputs = { InputFeatures("current sample", false, 1, false), InputFeatures("n", false, 1, false) }; t.getOutputSize = outputSize1Known;
		t.buildUI = [](NodeComponent& nc, NodeData& nd) {};
        t.execute = [](const NodeData&, UserInput& u, const std::vector<std::span<double>>& in, std::span<double>& out, RunnerInput&) {
            const double maxFilter = 40;
            double n = in[0].empty() ? 0.0 : in[1][0] * maxFilter;
            n = std::max(1.0, std::min(n, maxFilter));
            double sum = in[0][0];
            for (int i = 0; i < n; i += 1) {
                double v = (1.0 * n - i >= 1) + (n - i) * (n - i < 1);
                sum += u.getHistoricalSample(i) * v;
            }
            out[0] = sum / n;
        };
		t.isBoolean = false; t.alwaysOutputsRuntimeData = true; t.fromScene = nullptr; registry.push_back(t);
    }

    // ======== get old samples
    {
        NodeType t;
        t.name = "get old samples"; t.address = "timeseries/"; t.tooltip = "For each input index, gets the sample that many frames ago";
        t.inputs = { InputFeatures("indices", false, 0, false) }; t.getOutputSize = outputSizeEqualsSingleInputSize;
        t.buildUI = [](NodeComponent& nc, NodeData& nd) {};
        t.execute = [](const NodeData&, UserInput& u, const std::vector<std::span<double>>& in, std::span<double>& out, RunnerInput&) {
            const auto& indices = in[0];
            for (int i = 0; i < indices.size(); i += 1) {
                int idx = (int)std::round(indices[i]);
                out[i] = u.getHistoricalSample(idx);
            }
            };
        t.isBoolean = false; t.alwaysOutputsRuntimeData = true; t.fromScene = nullptr; registry.push_back(t);
    }

    // ========= set storeable value 
    {
        NodeType t;
        t.name = "set stored value"; t.address = "storage/"; t.tooltip = "Stores a value during execution that can be read next frame. outputs value";
        t.inputs = { InputFeatures("key", false, 1, false), InputFeatures("value", false, 1, false) }; t.getOutputSize = outputSize1Known;
        t.buildUI = [](NodeComponent& nc, NodeData& nd) {};
        t.execute = [](const NodeData&, UserInput& u, const std::vector<std::span<double>>& in, std::span<double>& out, RunnerInput& r) {
            out[0] = in[1][0];
            if (&r == u.runner) { 
                int key = static_cast<int>(std::round(in[0][0]));
                if (key < 0) key = 0;
                if (key > u.storeableValues.size() - 1) key = u.storeableValues.size() - 1;
                u.storeableValues[key] = in[1][0];
            }
        };
        t.isBoolean = false; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr; registry.push_back(t);
    }

    // ========= set storeable value 
    {
        NodeType t;
        t.name = "get stored value"; t.address = "storage/"; t.tooltip = "Outputs a specific stored value from the previous frame";
        t.inputs = { InputFeatures("key", false, 1, false)}; t.getOutputSize = outputSize1Known;
        t.buildUI = [](NodeComponent& nc, NodeData& nd) {};
        t.execute = [](const NodeData&, UserInput& u, const std::vector<std::span<double>>& in, std::span<double>& out, RunnerInput&) {
            int key = static_cast<int>(std::round(in[0][0]));
            if (key < 0) key = 0;
            if (key > u.storeableValues.size() - 1) key = u.storeableValues.size() - 1;
            out[0] = u.storeableValues[key];
            };
        t.isBoolean = false; t.alwaysOutputsRuntimeData = true; t.fromScene = nullptr; registry.push_back(t);
    }

}