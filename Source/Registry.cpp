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
#include "AudioReader.h"
#include "Noise.h"
#include "ddtype.h"
#include "VisualVectorCreator.h"
#define MAX_UNKNOWN_SIZE 1024

NodeType* intToBoolType = nullptr;
NodeType* boolToIntType = nullptr;
NodeType* decToBoolType = nullptr;
NodeType* boolToDecType = nullptr;
NodeType* decToIntType =  nullptr;
NodeType* intToDecType =  nullptr;

inline bool truthy(ddtype d) { return d.i != 0; }

//get output size general functions
int outputSize1Known(const std::vector<NodeData*>& inputs, const std::vector<std::vector<ddtype>>&, const RunnerInput&, int, const NodeData&) {
	return 1;
}

int outputSizeAllMidi(const std::vector<NodeData*>& inputs, const std::vector<std::vector<ddtype>>&, const RunnerInput&, int, const NodeData&) {
    return 128;
}

int outputSizeComponentWise(const std::vector<NodeData*>& inputs, const std::vector<std::vector<ddtype>>& s, const RunnerInput& r, int, const NodeData&) {
    int size = 1;
    bool getsPadded = false;
    for (const auto& input : s) {
        if (input.size() > size) {
            size = input.size();
        }
    }

    return size;
}

void fillInnerBounds(NodeComponent& n, juce::Component* c, double leftRatio, double rightRatio) {
    float scale = (float)std::pow(2.0, n.getOwningScene()->logScale);
    const float sides = 20.0f * scale;
    float cornerSize = 24.0f * scale;
    double l = std::lerp(sides, n.getWidth() - sides, leftRatio);
    n.inputGUIElements.back()->setBounds(l, cornerSize, rightRatio * (n.getWidth() - sides) - l, n.getHeight() - cornerSize * 2);
}

std::function<int(const std::vector<NodeData*>&, const std::vector<std::vector<ddtype>>& s, const RunnerInput& r, int, const NodeData&)> outputSizeFromInputScalar(int whichInput) {
    return [whichInput](const std::vector<NodeData*>&, const std::vector<std::vector<ddtype>>& s, const RunnerInput&, int, const NodeData&)
    {
        return std::max(1, static_cast<int>(std::round(s[whichInput][0].i)));
    };
}

std::function<int(const std::vector<NodeData*>&, const std::vector<std::vector<ddtype>>& s, const RunnerInput& r, int, const NodeData&)> outputSizeByNamedProperty(const std::string& name) {
    return [name](const std::vector<NodeData*>&, const std::vector<std::vector<ddtype>>& s, const RunnerInput& r, int, const NodeData& nd) {
        if (!nd.getNumericProperties().contains(name)) { return 1; }
        return (int)nd.getNumericProperty(name);
    };
}

double wrapTo(double v, double lo, double hi) {
    const double range = hi - lo;
    if (!(range > 0.0)) return lo;            // degenerate -> lo
    double w = std::fmod(v - lo, range);
    if (w < 0.0) w += range;                  // match zigzag's negative handling
    return lo + w;                            // in [lo, hi)
}

int64_t truthy(int64_t v) { return v != 0; }

using Comparator = bool (*)(double, double);
template <Comparator cmp>
void cmpExec(const NodeData&,
    UserInput&,
    const std::vector<std::span<ddtype>>& in,
    std::span<ddtype> out,
    const RunnerInput&)
{
    const auto& x = in[0];
    const auto& y = in[1];
    const int n = std::max((int)x.size(), (int)y.size());

    for (int i = 0; i < n; ++i) {
        const double xv = (i < (int)x.size()) ? x[i].d : 0.0;
        const double yv = (i < (int)y.size()) ? y[i].d : 0.0;
        out[i].i = cmp(xv, yv) ? 1 : 0;
    }
}

template<Comparator cmp>
void makeCmp(uint16_t id, const char* nm, const char* tip, std::vector<NodeType>& registry)
{
    NodeType t(id);
    t.name = nm;
    t.address = "math/logic/compare/";
    t.tooltip = tip;
    t.inputs = { InputFeatures("x", InputType::decimal,0,false),
                 InputFeatures("y", InputType::decimal,0,false) };
    t.getOutputSize = outputSizeComponentWise;
    t.buildUI = [](NodeComponent&, NodeData&) {};
    t.onResized = [](NodeComponent&) {};
    t.execute = &cmpExec<cmp>;   // <-- function pointer, no std::function
    t.outputType = InputType::boolean;
    t.alwaysOutputsRuntimeData = false;
    t.fromScene = nullptr;
    registry.push_back(t);
}


int outputSizeEqualsSingleInputSize(const std::vector<NodeData*>& inputs, const std::vector<std::vector<ddtype>>& s, const RunnerInput& r, int, const NodeData&) {
	if (inputs.empty()) {
		return 1;
	}
    return s[0].size();
}



int outputSizeOuterProduct(const std::vector<NodeData*>& inputs, const std::vector<std::vector<ddtype>>& s, const RunnerInput& r, int, const NodeData&) {
	int size = 1;
	bool getsPadded = false;
	for (const auto& input : s) {
		size *= input.size();
	}
	return size;
}


int outputSizeLerpStyle(const std::vector<NodeData*>& inputs, const std::vector<std::vector<ddtype>>& s, const RunnerInput& c, int x, const NodeData& self) {
	std::vector<NodeData*> justTheFirstTwo;
	if (inputs.size() > 2) {
		justTheFirstTwo.push_back(inputs[0]);
		justTheFirstTwo.push_back(inputs[1]);
	}
	else {
		throw std::exception("Lerp style output size requires 2 lerp inputs and a interpolator");
	}
	return outputSizeComponentWise(justTheFirstTwo, s, c, x,self);
}

std::function<int(const std::vector<NodeData*>&, const std::vector<std::vector<ddtype>>& s, const RunnerInput& r, int, const NodeData&)> outputSizeByInputPin(int pinIndex) {
	return [pinIndex](const std::vector<NodeData*>& inputs, const std::vector<std::vector<ddtype>>& s, const RunnerInput& r, int, const NodeData&) {
		if (s.size() > pinIndex) {
			return (int)s[pinIndex].size();
		}
        return 1;
	};
}

void WaviateFlow2025AudioProcessor::initializeRegistry()
{
    registry.clear();

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

    // ========= audio file =========
    {
        NodeType audioFileInputType(3);
        audioFileInputType.name = "audio file";
        audioFileInputType.address = "audio/media/";
        audioFileInputType.tooltip = "Reads a local audio file into a vector";
        audioFileInputType.inputs = {};
        audioFileInputType.isInputNode = false;

        // Output size is based on cached audio (or 1 if invalid)
        audioFileInputType.getOutputSize = [](const std::vector<NodeData*>& inputs, const std::vector<std::vector<ddtype>>&, const RunnerInput&, int, const NodeData& node) -> int
            {
                return node.optionalStoredAudio.empty()
                    ? 1
                    : static_cast<int>(node.optionalStoredAudio.size());
            };

        // Execution: copy from cache, or emit a single 0
        audioFileInputType.execute = [](const NodeData& node,
            UserInput&,
            const std::vector<std::span<ddtype>>&,
            std::span<ddtype> out,
            const RunnerInput&)
            {
                if (node.optionalStoredAudio.empty())
                {
                    if (!out.empty())
                        out[0] = 0.0;
                    return;
                }

                const size_t n = std::min(out.size(), node.optionalStoredAudio.size());
                std::copy_n(node.optionalStoredAudio.begin(), n, out.begin());
            };

        audioFileInputType._nodeDataChanged = [](NodeComponent& comp) -> bool
            {
                auto& node = comp.getNodeData();
                if (!node.getProperties().contains("filepath"))
                    return false;

                const std::string path = node.getStringProperty("filepath");
                std::shared_ptr<std::vector<ddtype>> newAudio = std::make_shared<std::vector<ddtype>>();

                std::thread([&]()
                    {
                        // Blocking I/O off the GUI thread
                        if (!AudioReader::readAudioFile(path, *newAudio))
                            newAudio->clear();

                        // GUI update when done
                        juce::MessageManager::callAsync([newAudio, &node, &comp, path]()
                            {
                                node.optionalStoredAudio = std::move(*newAudio);

                                // Update label colour/text
                                for (auto* c : comp.getChildren())
                                    if (auto* lbl = dynamic_cast<juce::Label*>(c))
                                    {
                                        const bool valid = !node.optionalStoredAudio.empty();
                                        lbl->setColour(juce::Label::textColourId,
                                            valid ? juce::Colours::green : juce::Colours::red);
                                        juce::String text = valid ? juce::File(path).getFileName() : "No file selected";
                                        lbl->setTooltip(path);
                                        lbl->setText(text, juce::dontSendNotification);
                                    }

                                if (auto* s = comp.getOwningScene())
                                    s->onSceneChanged();

                                comp.repaint();
                            });

                    }).detach();

                return false; // actual "change detection" is handled async
            };

        audioFileInputType.buildUI = [](NodeComponent& comp, NodeData& node)
            {
                comp.inputGUIElements.push_back(std::make_unique<juce::TextButton>("Choose audio…"));
                auto* chooseBtn = dynamic_cast<juce::TextButton*>(comp.inputGUIElements.back().get());
                comp.addAndMakeVisible(chooseBtn);

                comp.inputGUIElements.push_back(std::make_unique<juce::Label>("audioPath", "No file selected"));
                auto* statusLabel = dynamic_cast<juce::Label*>(comp.inputGUIElements.back().get());
                statusLabel->setJustificationType(juce::Justification::centred);
                statusLabel->setInterceptsMouseClicks(false, false);
                comp.addAndMakeVisible(statusLabel);

                chooseBtn->onClick = [&comp, &node]()
                    {
                        auto chooser = std::make_shared<juce::FileChooser>(
                            "Select an audio file...",
                            juce::File(),
                            "*.wav;*.aiff;*.aif;*.flac;*.ogg;*.mp3");

                        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                            [chooser, &comp, &node](const juce::FileChooser& fc)
                            {
                                juce::File file = fc.getResult();
                                if (file.existsAsFile())
                                {
                                    node.setProperty("filepath", file.getFullPathName().toStdString());
                                    node.getType()->nodeDataChanged(comp); // triggers async decode
                                }
                            });
                    };
            };

        audioFileInputType.onResized = [](NodeComponent& comp)
            {
                // Find our controls
                juce::TextButton* chooseBtn = nullptr;
                juce::Label* statusLabel = nullptr;

                for (auto* c : comp.getChildren())
                {
                    if (!chooseBtn)   chooseBtn = dynamic_cast<juce::TextButton*>(c);
                    if (!statusLabel) statusLabel = dynamic_cast<juce::Label*>(c);
                }

                const float scale = (float)std::pow(2.0, comp.getOwningScene()->logScale);
                const int padding = (int)(12.0f * scale);
                const int btnH = (int)(28.0f * scale);
                const int lblH = (int)(22.0f * scale);

                if (chooseBtn)
                    chooseBtn->setBounds(padding, padding,
                        comp.getWidth() - 2 * padding, btnH);

                if (statusLabel)
                    statusLabel->setBounds(padding, padding + btnH + (int)(8.0f * scale),
                        comp.getWidth() - 2 * padding, lblH);
            };

        audioFileInputType.outputType = InputType::decimal;
        audioFileInputType.alwaysOutputsRuntimeData = true; // outputs actual audio data at run time
        audioFileInputType.fromScene = nullptr;

        registry.push_back(audioFileInputType);
    }


    // ========= add =========
    {
        NodeType addType(4);
        addType.name = "add";
        addType.address = "math/arithmetic/";
        addType.tooltip = "Component-wise addition; pads by copying the longer input.";
        addType.inputs = { InputFeatures("x", InputType::decimal, 0, false), InputFeatures("y", InputType::decimal, 0, false) };
        addType.getOutputSize = outputSizeComponentWise;
        addType.buildUI = [](NodeComponent&, NodeData&) {};
        addType.onResized = [](NodeComponent&) {};
        addType.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& inputs, std::span<ddtype> output, const RunnerInput& inlineInstance)
            {
                const int a = static_cast<int>(inputs[0].size());
                const int b = static_cast<int>(inputs[1].size());
                const int n = std::min(a, b);
                const int m = std::max(a, b);
                const bool xIsBigger = a > b;

                for (int i = 0; i < n; ++i) output[i].d = inputs[0][i].d + inputs[1][i].d;
                if (xIsBigger) for (int i = n; i < m; ++i) output[i].d = inputs[0][i].d;
                else           for (int i = n; i < m; ++i) output[i].d = inputs[1][i].d;
            };
        addType.outputType = InputType::decimal;
        addType.alwaysOutputsRuntimeData = false;
        addType.fromScene = nullptr;
        registry.push_back(addType);
        keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("+").getTextDescription(), registry.size() - 1 });
    }


    // ========= subtract =========
    {
        NodeType subType(5);
        subType.name = "subtract";
        subType.address = "math/arithmetic/";
        subType.tooltip = "Component-wise x − y; pads with remaining of longer input (negated for y).";
        subType.inputs = { InputFeatures("x", InputType::decimal, 0, false), InputFeatures("y", InputType::decimal, 0, false) };
        subType.getOutputSize = outputSizeComponentWise;
        subType.buildUI = [](NodeComponent&, NodeData&) {};
        subType.onResized = [](NodeComponent&) {};
        subType.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& inputs, std::span<ddtype> output, const RunnerInput& inlineInstance)
            {
                const int a = static_cast<int>(inputs[0].size());
                const int b = static_cast<int>(inputs[1].size());
                const int n = std::min(a, b);
                const int m = std::max(a, b);
                const bool xIsBigger = a > b;

                for (int i = 0; i < n; ++i) output[i] = inputs[0][i].d - inputs[1][i].d;
                if (xIsBigger) for (int i = n; i < m; ++i) output[i] = inputs[0][i].d;
                else           for (int i = n; i < m; ++i) output[i] = -inputs[1][i].d;
            };
        subType.outputType = InputType::decimal;
        subType.alwaysOutputsRuntimeData = false;
        subType.fromScene = nullptr;
        registry.push_back(subType);
        keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("-").getTextDescription(), registry.size() - 1 });
    }

    // ========= multiply =========
    {
        NodeType mulType(6);
        mulType.name = "multiply";
        mulType.address = "math/arithmetic/";
        mulType.tooltip = "Component-wise multiplication; pads by copying the longer input.";
        mulType.inputs = { InputFeatures("x", InputType::decimal, 0, false), InputFeatures("y", InputType::decimal, 0, false) };
        mulType.getOutputSize = outputSizeComponentWise;
        mulType.buildUI = [](NodeComponent&, NodeData&) {};
        mulType.onResized = [](NodeComponent&) {};
        mulType.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& inputs, std::span<ddtype> output, const RunnerInput& inlineInstance)
            {
                const int a = static_cast<int>(inputs[0].size());
                const int b = static_cast<int>(inputs[1].size());
                const int n = std::min(a, b);
                const int m = std::max(a, b);
                const bool xIsBigger = a > b;

                for (int i = 0; i < n; ++i) output[i] = inputs[0][i].d * inputs[1][i].d;
                if (xIsBigger) for (int i = n; i < m; ++i) output[i] = inputs[0][i].d;
                else           for (int i = n; i < m; ++i) output[i] = inputs[1][i].d;
            };
        mulType.outputType = InputType::decimal;
        mulType.alwaysOutputsRuntimeData = false;
        mulType.fromScene = nullptr;
        registry.push_back(mulType);
        keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("*").getTextDescription(), registry.size() - 1 });
    }

    // ========= divide =========
    NodeType divType(7);
    divType.name = "divide";
    divType.address = "math/arithmetic/";
    divType.tooltip = "Component-wise x / y; pads with x or zeros to avoid div-by-zero.";
    divType.inputs = { InputFeatures("x", InputType::decimal, 0, false), InputFeatures("y", InputType::decimal, 0, false) };
    divType.getOutputSize = outputSizeComponentWise;
    divType.buildUI = [](NodeComponent&, NodeData&) {};
    divType.onResized = [](NodeComponent&) {};
    divType.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& inputs, std::span<ddtype> output, const RunnerInput& inlineInstance)
        {
            const int a = static_cast<int>(inputs[0].size());
            const int b = static_cast<int>(inputs[1].size());
            const int n = std::min(a, b);
            const int m = std::max(a, b);
            const bool xIsBigger = a > b;

            for (int i = 0; i < n; ++i)
                output[i] = (inputs[1][i].d != 0.0) ? (inputs[0][i].d / inputs[1][i].d) : 0.0;

            if (xIsBigger) for (int i = n; i < m; ++i) output[i] = inputs[0][i];
            else           for (int i = n; i < m; ++i) output[i] = 0.0;
        };
    divType.outputType = InputType::decimal;
    divType.alwaysOutputsRuntimeData = false;
    divType.fromScene = nullptr;
    registry.push_back(divType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("/").getTextDescription(), registry.size() - 1 });
    // NOTE: Your original code *defines* modulo but never pushes it. To keep behavior identical,
    // we do not push a modulo node here. (See bug list below.)

    // ========= sin =========
    NodeType sinType(8);
    sinType.name = "sin";
    sinType.address = "math/trigonometry/";
    sinType.tooltip = "Applies sin(x) component-wise.";
    sinType.inputs = { InputFeatures("x", InputType::decimal, 0, false) };
    sinType.getOutputSize = outputSizeEqualsSingleInputSize;
    sinType.buildUI = [](NodeComponent&, NodeData&) {};
    sinType.onResized = [](NodeComponent&) {};
    sinType.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& inputs, std::span<ddtype> output, const RunnerInput& inlineInstance)
        {
            for (int i = 0; i < static_cast<int>(inputs[0].size()); ++i) output[i].d = std::sin(inputs[0][i].d);
        };
    sinType.outputType = InputType::decimal;
    sinType.alwaysOutputsRuntimeData = false;
    sinType.fromScene = nullptr;
    registry.push_back(sinType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("alt s").getTextDescription(), registry.size() - 1 });

    // ========= cos =========
    NodeType cosType(9);
    cosType.name = "cos";
    cosType.address = "math/trigonometry/";
    cosType.tooltip = "Applies cos(x) component-wise.";
    cosType.inputs = { InputFeatures("x", InputType::decimal, 0, false) };
    cosType.getOutputSize = outputSizeEqualsSingleInputSize;
    cosType.buildUI = [](NodeComponent&, NodeData&) {};
    cosType.onResized = [](NodeComponent&) {};
    cosType.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& inputs, std::span<ddtype> output, const RunnerInput& inlineInstance)
        {
            for (int i = 0; i < static_cast<int>(inputs[0].size()); ++i) output[i].d = std::cos(inputs[0][i].d);
        };
    cosType.outputType = InputType::decimal;
    cosType.alwaysOutputsRuntimeData = false;
    cosType.fromScene = nullptr;
    registry.push_back(cosType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("alt c").getTextDescription(), registry.size() - 1 });

    // ========= tan =========
    NodeType tanType(10);
    tanType.name = "tan";
    tanType.address = "math/trigonometry/";
    tanType.tooltip = "Applies tan(x) component-wise.";
    tanType.inputs = { InputFeatures("x", InputType::decimal, 0, false) };
    tanType.getOutputSize = outputSizeEqualsSingleInputSize;
    tanType.buildUI = [](NodeComponent&, NodeData&) {};
    tanType.onResized = [](NodeComponent&) {};
    tanType.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& inputs, std::span<ddtype> output, const RunnerInput& inlineInstance)
        {
            for (int i = 0; i < static_cast<int>(inputs[0].size()); ++i) output[i].d = std::tan(inputs[0][i].d);
        };
    tanType.outputType = InputType::decimal;
    tanType.alwaysOutputsRuntimeData = false;
    tanType.fromScene = nullptr;
    registry.push_back(tanType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("alt t").getTextDescription(), registry.size() - 1 });

    // ========= atan2 =========
    NodeType atan2Type(11);
    atan2Type.name = "atan2";
    atan2Type.address = "math/trigonometry/inverse/";
    atan2Type.tooltip = "Component-wise atan2(y, x). Pads with the longer input.";
    atan2Type.inputs = { InputFeatures("y", InputType::decimal, 0, false), InputFeatures("x", InputType::decimal, 0, false) };
    atan2Type.getOutputSize = outputSizeComponentWise;
    atan2Type.buildUI = [](NodeComponent&, NodeData&) {};
    atan2Type.onResized = [](NodeComponent&) {};
    atan2Type.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& inputs, std::span<ddtype> output, const RunnerInput& inlineInstance)
        {
            const int a = static_cast<int>(inputs[0].size());
            const int b = static_cast<int>(inputs[1].size());
            const int n = std::min(a, b);
            const int m = std::max(a, b);
            const bool yIsBigger = a > b;

            for (int i = 0; i < n; ++i) output[i] = std::atan2(inputs[0][i].d, inputs[1][i].d);
            if (yIsBigger) for (int i = n; i < m; ++i) output[i] = inputs[0][i].d;
            else           for (int i = n; i < m; ++i) output[i] = inputs[1][i].d;
        };
    atan2Type.outputType = InputType::decimal;
    atan2Type.alwaysOutputsRuntimeData = false;
    atan2Type.fromScene = nullptr;
    registry.push_back(atan2Type);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("alt a").getTextDescription(), registry.size() - 1 });

    // ========= constant =========
    NodeType constantType(12);
    constantType.name = "constant decimal";
    constantType.address = "constants/";
    constantType.tooltip = "Outputs a fixed numeric value.";
    constantType.inputs = {};
    constantType.getOutputSize = outputSize1Known;
    constantType.execute = [](const NodeData& node, UserInput&, const std::vector<std::span<ddtype>>&, std::span<ddtype> output, const RunnerInput& inlineInstance)
        {
            output[0] = node.getNumericProperty("value");
        };
    constantType.buildUI = [](NodeComponent& comp, NodeData& node)
        {
            node.setProperty("value", 0.0);
            comp.inputGUIElements.push_back(std::make_unique<juce::TextEditor>("value"));
            auto back = dynamic_cast<juce::TextEditor*>(comp.inputGUIElements.back().get());
            back->setJustification(juce::Justification::horizontallyCentred);
            back->setText("");
            back->onTextChange = [&node, &comp]()
                {
                    auto el = dynamic_cast<juce::TextEditor*>(comp.inputGUIElements.back().get());
                    if (el->getText().isEmpty()) {
                        node.setProperty("value", 0.0);
                    }
                    else {
                        juce::String s;
                        for (char c : el->getText()) {
                            if (c >= '0' && c <= '9') {
                                s += c;
                            }
                            if (c == 'e' || c == 'E' || c == '-' || c == '.' || c == '+') {
                                s += c;
                            }
                        }
                        el->setText(s, false);
                        try {
                            const double newVal = std::stod(el->getText().toStdString());
                            if (std::isfinite(newVal))
                            {
                                node.setProperty("value", newVal);
                            }
                            else
                            {
                                el->setText(juce::String(node.getNumericProperty("value")), false);
                            }
                        }
                        catch (...) {
                            el->setText(juce::String(node.getNumericProperty("value")));
                        }
                    }
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
    constantType.onResized = [](NodeComponent& comp) {
        if (comp.inputGUIElements.empty()) { return; }
            auto back = dynamic_cast<juce::TextEditor*>(comp.inputGUIElements.back().get());
            if (back) {
                const float scale = (float)std::pow(2.0, comp.getOwningScene()->logScale);
                const float sides = 20.0f * scale;
                const float cornerSize = 24.0f * scale;
                back->setBounds(sides, cornerSize, comp.getWidth() - sides - sides, comp.getHeight() - cornerSize * 2);
                back->applyFontToAllText(back->getFont().withHeight(14*scale));
            }

            
        };
    constantType.outputType = InputType::decimal;
    constantType.alwaysOutputsRuntimeData = false;
    constantType.fromScene = nullptr;
    registry.push_back(constantType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("c").getTextDescription(), registry.size() - 1 });

    // ========= constant =========
    NodeType constantVecType(13);
    constantVecType.name = "constant decimals";
    constantVecType.address = "constants/";
    constantVecType.tooltip = "Outputs a list of fixed numeric values (enter comma separated numbers).";
    constantVecType.inputs = {};
    constantVecType.getOutputSize = [](const std::vector<NodeData*>& inputs, const std::vector<std::vector<ddtype>>&, const RunnerInput&, int, const NodeData& data) {
        return data.getNumericProperty("size");
    };
    constantVecType.execute = [](const NodeData& node, UserInput&, const std::vector<std::span<ddtype>>&, std::span<ddtype> output, const RunnerInput& inlineInstance)
        {
            for (int i = 0; i < output.size(); i += 1) {
                juce::String j = juce::String(i);
                output[i] = node.getNumericProperty(j.toStdString());
            }
            
        };
    constantVecType.buildUI = [](NodeComponent& comp, NodeData& node)
        {
            node.setProperty("size", 1.0); // start empty

            comp.inputGUIElements.push_back(std::make_unique<juce::TextEditor>("values"));
            auto* back = dynamic_cast<juce::TextEditor*>(comp.inputGUIElements.back().get());
            back->setJustification(juce::Justification::horizontallyCentred);
            back->setMultiLine(false, false);
            back->setText("");

            back->onTextChange = [&node, &comp]()
                {
                    auto* el = dynamic_cast<juce::TextEditor*>(comp.inputGUIElements.back().get());
                    if (!el) return;

                    juce::String s;
                    for (char c : el->getText()) {
                        if (c >= '0' && c <= '9') {
                            s += c;
                        }
                        if (c == 'e' || c == 'E' || c == '-' || c == '.' || c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '+') {
                            s += c;
                        }
                    }
                    el->setText(s, false);

                    juce::StringArray tokens;
                    tokens.addTokens(el->getText(), ",", "");   // split on commas
                    tokens.removeEmptyStrings();                // drop blanks

                    int count = 0;
                    node.setProperty(juce::String(0).toStdString(), 0.0);
                    for (int i = 0; i < tokens.size(); ++i)
                    {
                        try
                        {
                            const double val = std::stod(tokens[i].toStdString());
                            if (std::isfinite(val))
                            {
                                node.setProperty(juce::String(count++).toStdString(), val);
                            }
                        }
                        catch (...) { /* skip bad tokens */ }
                    }

                    node.setProperty("size", std::max((double)count, 1.0));

                    // If no valid numbers, reset to size 0
                    if (count == 0)
                        node.setProperty("size", 1.0);

                    comp.updateSize();                         // trigger resize
                    comp.getOwningScene()->onSceneChanged();   // notify scene
                };

            const float scale = (float)std::pow(2.0, comp.getOwningScene()->logScale);
            const float sides = 20.0f * scale;
            const float cornerSize = 24.0f * scale;
            back->setBounds(sides, cornerSize,
                comp.getWidth() - sides - sides,
                comp.getHeight() - cornerSize * 2);
            back->applyFontToAllText(back->getFont().withHeight(14 * scale));
            comp.addAndMakeVisible(back);
        };

    constantVecType.onResized = [](NodeComponent& comp) {
        if (comp.inputGUIElements.empty()) { return; }
        auto back = dynamic_cast<juce::TextEditor*>(comp.inputGUIElements.back().get());
        if (back) {
            const float scale = (float)std::pow(2.0, comp.getOwningScene()->logScale);
            const float sides = 20.0f * scale;
            const float cornerSize = 24.0f * scale;
            back->setBounds(sides, cornerSize, comp.getWidth() - sides - sides, comp.getHeight() - cornerSize * 2);
            back->applyFontToAllText(back->getFont().withHeight(14 * scale));
        }
    };
    constantVecType.outputType = InputType::decimal;
    constantVecType.alwaysOutputsRuntimeData = false;
    constantVecType.fromScene = nullptr;
    registry.push_back(constantVecType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("v").getTextDescription(), registry.size() - 1 });

    // ========= constant boolean =========
    NodeType constBoolType(14);
    constBoolType.name = "constant boolean";
    constBoolType.address = "constants/";
    constBoolType.tooltip = "Outputs a fixed boolean (1 or 0).";
    constBoolType.inputs = {};
    constBoolType.getOutputSize = outputSize1Known;
    constBoolType.execute = [](const NodeData& node, UserInput&, const std::vector<std::span<ddtype>>&, std::span<ddtype> output, const RunnerInput& inlineInstance)
        {
            output[0] = node.getNumericProperty("value");
        };
    constBoolType.buildUI = [](NodeComponent& comp, NodeData& node)
        {
            comp.inputGUIElements.push_back(std::make_unique<juce::ToggleButton>(juce::String("true/false")));
            auto back = dynamic_cast<juce::ToggleButton*>(comp.inputGUIElements.back().get());
            back->onStateChange = [&]()
                {
                    auto el = dynamic_cast<juce::ToggleButton*>(comp.inputGUIElements.back().get());
                    node.setProperty("value", el->getToggleState() ? 1.0 : 0.0);
                    comp.getOwningScene()->onSceneChanged();
                };
            const float scale = (float)std::pow(2.0, comp.getOwningScene()->logScale);
            const float sides = 20.0f * scale;
            const float cornerSize = 24.0f * scale;
            back->setBounds(sides, cornerSize, comp.getWidth() - sides - sides, comp.getHeight() - cornerSize * 2);
            comp.addAndMakeVisible(back);
        };
    constBoolType.onResized = [](NodeComponent& comp)
        {
            if (comp.inputGUIElements.empty()) { return; }
            auto back = dynamic_cast<juce::ToggleButton*>(comp.inputGUIElements.back().get());
            const float scale = (float)std::pow(2.0, comp.getOwningScene()->logScale);
            const float sides = 20.0f * scale;
            const float cornerSize = 24.0f * scale;
            back->setBounds(sides, cornerSize, comp.getWidth() - sides - sides, comp.getHeight() - cornerSize * 2);
        };
    constBoolType.outputType = InputType::boolean;
    constBoolType.alwaysOutputsRuntimeData = false;
    constBoolType.fromScene = nullptr;
    registry.push_back(constBoolType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("b").getTextDescription(), registry.size() - 1 });

    // ========= branch =========
    NodeType branchType(15);
    branchType.name = "branch";
    branchType.address = "math/logic/choice/";
    branchType.tooltip = "Selects between two inputs based on a boolean t/f.";
    branchType.inputs = {
        InputFeatures("true",  InputType::any, 0, false),
        InputFeatures("false", InputType::any, 0, false),
        InputFeatures("t/f",   InputType::boolean,  1, false)
    };
    branchType.getOutputSize = outputSizeComponentWise;
    branchType.buildUI = [](NodeComponent&, NodeData&) {};
    branchType.onResized = [](NodeComponent&) {};
    branchType.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& inputs, std::span<ddtype> output, const RunnerInput& inlineInstance)
        {
            for (int i = 0; i < static_cast<int>(output.size()); ++i) output[i] = 0.0;
            const int n = std::min(static_cast<int>(inputs[0].size()), static_cast<int>(inputs[1].size()));
            if (!inputs[2].empty() && inputs[2][0].i != 0)
                for (int i = 0; i < n; ++i) output[i] = inputs[0][i];
            else
                for (int i = 0; i < n; ++i) output[i] = inputs[1][i];
        };
    branchType.outputType = InputType::decimal;
    branchType.alwaysOutputsRuntimeData = false;
    branchType.fromScene = nullptr;
    registry.push_back(branchType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("shift b").getTextDescription(), registry.size() - 1 });

    // ========= comparison node =========
    NodeType comparisonType(16);
    comparisonType.name = "compare";
    comparisonType.address = "math/logic/choice/";
    comparisonType.tooltip = "Selects between 3 inputs based on values of a and b.";
    comparisonType.inputs = {
        InputFeatures("a < b",  InputType::decimal, 0, false),
        InputFeatures("a == b", InputType::decimal, 0, false),
        InputFeatures("a > b",   InputType::decimal,  0, false),
        InputFeatures("a", InputType::decimal, 1, false),
        InputFeatures("b", InputType::decimal, 1, false)
    };
    comparisonType.getOutputSize = outputSizeComponentWise;
    comparisonType.buildUI = [](NodeComponent&, NodeData&) {};
    comparisonType.onResized = [](NodeComponent&) {};
    comparisonType.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& inputs,
        std::span<ddtype> output, const RunnerInput&)
        {
            for (int i = 0; i < (int)output.size(); ++i) output[i] = 0.0;

            const auto& lt = inputs[0];  // use when a < b
            const auto& eq = inputs[1];  // use when a == b (also NaN fallback)
            const auto& gt = inputs[2];  // use when a > b
            const auto& a = inputs[3];
            const auto& b = inputs[4];

            auto at = [](const std::span<const ddtype>& v, int i) -> double {
                if (v.empty()) return 0.0;
                return v.size() == 1 ? v[0].d : v[i].d;
                };

            const int n = (int)output.size();
            for (int i = 0; i < n; ++i)
            {
                const double ai = at(a, i), bi = at(b, i);
                const auto& src = (ai < bi) ? lt : ((ai > bi) ? gt : eq);
                output[i] = at(src, i);
            }
        };
    comparisonType.outputType = InputType::decimal;
    comparisonType.alwaysOutputsRuntimeData = false;
    comparisonType.fromScene = nullptr;
    registry.push_back(comparisonType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("shift c").getTextDescription(), registry.size() - 1 });

    // ========= lerp =========
    NodeType lerpType(17);
    lerpType.name = "lerp";
    lerpType.address = "math/blend/";
    lerpType.tooltip = "Linearly blends between two inputs based on \"s\".";
    lerpType.inputs = {
        InputFeatures("a",  InputType::decimal, 0, false),
        InputFeatures("b",  InputType::decimal, 0, false),
        InputFeatures("s",  InputType::decimal,  1, false)
    };
    lerpType.getOutputSize = outputSizeComponentWise;
    lerpType.buildUI = [](NodeComponent&, NodeData&) {};
    lerpType.onResized = [](NodeComponent&) {};
    lerpType.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& inputs, std::span<ddtype> output, const RunnerInput& inlineInstance)
        {
            for (int i = 0; i < static_cast<int>(output.size()); ++i) output[i] = 0.0;
            const int n = std::min(static_cast<int>(inputs[0].size()), static_cast<int>(inputs[1].size()));
            if (!inputs[2].empty()) {
                double lerpFactor = std::max(0.0, std::min(1.0, inputs[2][0].d));
                double inv = (1 - lerpFactor);
                for (int i = 0; i < n; ++i) {
                    output[i] = inputs[0][i].d * inv + inputs[1][i].d * lerpFactor;
                }
            }
        };
    lerpType.outputType = InputType::decimal;
    lerpType.alwaysOutputsRuntimeData = false;
    lerpType.fromScene = nullptr;
    registry.push_back(lerpType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("l").getTextDescription(), registry.size() - 1 });

    // ========= smooth lerp =========
    NodeType smoothLerpType(18);
    smoothLerpType.name = "smooth lerp";
    smoothLerpType.address = "math/blend/smooth";
    smoothLerpType.tooltip = "Lerps from a to b using smoothstep(s): s' = s*s*(3-2*s).";

    smoothLerpType.inputs = {
        InputFeatures("a", InputType::decimal, 0, false),
        InputFeatures("b", InputType::decimal, 0, false),
        InputFeatures("s", InputType::decimal, 1, false) // scalar blend
    };

    smoothLerpType.getOutputSize = outputSizeComponentWise;
    smoothLerpType.buildUI = [](NodeComponent&, NodeData&) {};
    smoothLerpType.onResized = [](NodeComponent&) {};

    smoothLerpType.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& in,
        std::span<ddtype> out, const RunnerInput&) {
            // init output
            for (int i = 0; i < (int)out.size(); ++i) out[i] = 0.0;

            const int n = std::min((int)in[0].size(), (int)in[1].size());
            if (n == 0 || in[2].empty()) return;

            // read & sanitize s (no asserts; stable fallback)
            double s = in[2][0].d;
            if (!std::isfinite(s)) s = 0.0;
            if (s < 0.0) s = 0.0;
            else if (s > 1.0) s = 1.0;

            // cubic smoothstep easing
            double s2 = s * s;
            double sSmooth = s2 * (3.0 - 2.0 * s);  // smoothstep
            double inv = 1.0 - sSmooth;

            for (int i = 0; i < n; ++i) {
                out[i] = in[0][i].d * inv + in[1][i].d * sSmooth;
            }
        };

    smoothLerpType.outputType = InputType::decimal;
    smoothLerpType.alwaysOutputsRuntimeData = false;
    smoothLerpType.fromScene = nullptr;
    registry.push_back(smoothLerpType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("shift l").getTextDescription(), registry.size() - 1 });


    // ========= time since app started =========
    NodeType timeType(19);
    timeType.name = "time since app started";
    timeType.address = "time/";
    timeType.tooltip = "Seconds since the app started (runtime).";
    timeType.inputs = {};
    timeType.getOutputSize = outputSize1Known;
    timeType.execute = [](const NodeData&, UserInput& userInput, const std::vector<std::span<ddtype>>&, std::span<ddtype> output, const RunnerInput& inlineInstance)
        {
            const int64_t trueFrame = userInput.numFramesStartOfBlock + userInput.sampleInBlock;
            output[0] = trueFrame / userInput.sampleRate;
        };
    timeType.buildUI = [](NodeComponent&, NodeData&) {};
    timeType.onResized = [](NodeComponent&) {};
    timeType.outputType = InputType::decimal;
    timeType.alwaysOutputsRuntimeData = true;
    timeType.fromScene = nullptr;
    registry.push_back(timeType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("t").getTextDescription(), registry.size() - 1 });


    // ========= not =========
    NodeType notType(20);
    notType.name = "not";
    notType.address = "math/logic/";
    notType.tooltip = "Logical NOT (1 if input == 0, else 0).";
    notType.inputs = { InputFeatures("a", InputType::boolean, 0, false) };
    notType.getOutputSize = outputSizeEqualsSingleInputSize;
    notType.buildUI = [](NodeComponent&, NodeData&) {};
    notType.onResized = [](NodeComponent&) {};
    notType.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& inputs, std::span<ddtype> output, const RunnerInput& inlineInstance)
        {
            for (int i = 0; i < static_cast<int>(inputs[0].size()); ++i)
                output[i] = inputs[0][i].i ? 1.0 : 0.0;
        };
    notType.outputType = InputType::boolean;
    notType.alwaysOutputsRuntimeData = false;
    notType.fromScene = nullptr;
    registry.push_back(notType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("n").getTextDescription(), registry.size() - 1 });

    // ========= and =========
    NodeType andType(21);
    andType.name = "and";
    andType.address = "math/logic/";
    andType.tooltip = "Logical AND component-wise (nonzero treated as true).";
    andType.inputs = { InputFeatures("x", InputType::boolean, 0, false), InputFeatures("y", InputType::boolean, 0, false) };
    andType.getOutputSize = outputSizeComponentWise;
    andType.buildUI = [](NodeComponent&, NodeData&) {};
    andType.onResized = [](NodeComponent&) {};
    andType.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& inputs, std::span<ddtype> output, const RunnerInput& inlineInstance)
        {
            const int a = static_cast<int>(inputs[0].size());
            const int b = static_cast<int>(inputs[1].size());
            const int n = std::min(a, b);
            const int m = std::max(a, b);
            for (int i = 0; i < n; ++i) output[i] = (inputs[0][i].i && inputs[1][i].i) ? 1.0 : 0.0;
            for (int i = n; i < m; ++i) output[i] = 0.0;
        };
    andType.outputType = InputType::boolean;
    andType.alwaysOutputsRuntimeData = false;
    andType.fromScene = nullptr;
    registry.push_back(andType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("a").getTextDescription(), registry.size() - 1 });

    // ========= or =========
    NodeType orType(22);
    orType.name = "or";
    orType.address = "math/logic/";
    orType.tooltip = "Logical OR component-wise (nonzero treated as true).";
    orType.inputs = { InputFeatures("x", InputType::boolean, 0, false), InputFeatures("y", InputType::boolean, 0, false) };
    orType.getOutputSize = outputSizeComponentWise;
    orType.buildUI = [](NodeComponent&, NodeData&) {};
    orType.onResized = [](NodeComponent&) {};
    orType.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& inputs, std::span<ddtype> output, const RunnerInput& inlineInstance)
        {
            const int a = static_cast<int>(inputs[0].size());
            const int b = static_cast<int>(inputs[1].size());
            const int n = std::min(a, b);
            const int m = std::max(a, b);
            const bool xIsBigger = a > b;

            for (int i = 0; i < n; ++i) output[i] = (inputs[0][i].i || inputs[1][i].i) ? 1.0 : 0.0;
            if (xIsBigger) for (int i = n; i < m; ++i) output[i] = inputs[0][i];
            else           for (int i = n; i < m; ++i) output[i] = inputs[1][i];
        };
    orType.outputType = InputType::boolean;
    orType.alwaysOutputsRuntimeData = false;
    orType.fromScene = nullptr;
    registry.push_back(orType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("o").getTextDescription(), registry.size() - 1 });

    // ======== OUTER PRODUCTS ========
    NodeType outerAddType(23);
    outerAddType.name = "outer add";
    outerAddType.address = "math/arithmetic/outer product/";
    outerAddType.tooltip = "Computes all x_i + y_j into a flattened matrix.";
    outerAddType.inputs = { InputFeatures("x", InputType::decimal, 0, false), InputFeatures("y", InputType::decimal, 0, false) };
    outerAddType.getOutputSize = outputSizeOuterProduct;
    outerAddType.buildUI = [](NodeComponent&, NodeData&) {};
    outerAddType.onResized = [](NodeComponent&) {};
    outerAddType.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& inputs, std::span<ddtype> output, const RunnerInput& inlineInstance)
        {
            for (int i = 0; i < static_cast<int>(inputs[0].size()); ++i) {
                const double xv = inputs[0][i].d;
                for (int j = 0; j < static_cast<int>(inputs[1].size()); ++j) {
                    const int z = j + i * static_cast<int>(inputs[1].size());
                    output[z] = xv + inputs[1][j].d;
                }
            }
        };
    outerAddType.outputType = InputType::decimal;
    outerAddType.alwaysOutputsRuntimeData = false;
    outerAddType.fromScene = nullptr;
    registry.push_back(outerAddType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("shift +").getTextDescription(), registry.size() - 1 });

    NodeType outerSubType(24);
    outerSubType.name = "outer subtract";
    outerSubType.address = "math/arithmetic/outer product/";
    outerSubType.tooltip = "Computes all x_i − y_j into a flattened matrix.";
    outerSubType.inputs = { InputFeatures("x", InputType::decimal, 0, false), InputFeatures("y", InputType::decimal, 0, false) };
    outerSubType.getOutputSize = outputSizeOuterProduct;
    outerSubType.buildUI = [](NodeComponent&, NodeData&) {};
    outerSubType.onResized = [](NodeComponent&) {};
    outerSubType.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& inputs, std::span<ddtype> output, const RunnerInput& inlineInstance)
        {
            for (int i = 0; i < static_cast<int>(inputs[0].size()); ++i) {
                const double xv = inputs[0][i].d;
                for (int j = 0; j < static_cast<int>(inputs[1].size()); ++j) {
                    const int z = j + i * static_cast<int>(inputs[1].size());
                    output[z] = xv - inputs[1][j].d;
                }
            }
        };
    outerSubType.outputType = InputType::decimal;
    outerSubType.alwaysOutputsRuntimeData = false;
    outerSubType.fromScene = nullptr;
    registry.push_back(outerSubType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("shift -").getTextDescription(), registry.size() - 1 });

    NodeType outerMulType(25);
    outerMulType.name = "outer multiply";
    outerMulType.address = "math/arithmetic/outer product/";
    outerMulType.tooltip = "Computes all x_i * y_j into a flattened matrix.";
    outerMulType.inputs = { InputFeatures("x", InputType::decimal, 0, false), InputFeatures("y", InputType::decimal, 0, false) };
    outerMulType.inputs[0].defaultValue = 1.0;
    outerMulType.inputs[1].defaultValue = 1.0;
    outerMulType.getOutputSize = outputSizeOuterProduct;
    outerMulType.buildUI = [](NodeComponent&, NodeData&) {};
    outerMulType.onResized = [](NodeComponent&) {};
    outerMulType.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& inputs, std::span<ddtype> output, const RunnerInput& inlineInstance)
        {
            for (int i = 0; i < static_cast<int>(inputs[0].size()); ++i) {
                const double xv = inputs[0][i].d;
                for (int j = 0; j < static_cast<int>(inputs[1].size()); ++j) {
                    const int z = j + i * static_cast<int>(inputs[1].size());
                    output[z] = xv * inputs[1][j].d;
                }
            }
        };
    outerMulType.outputType = InputType::decimal;
    outerMulType.alwaysOutputsRuntimeData = false;
    outerMulType.fromScene = nullptr;
    registry.push_back(outerMulType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("shift *").getTextDescription(), registry.size() - 1 });

    NodeType outerDivType(26);
    outerDivType.name = "outer divide";
    outerDivType.address = "math/arithmetic/outer product/";
    outerDivType.tooltip = "Computes all x_i / y_j into a flattened matrix (0 if y_j==0).";
    outerDivType.inputs = { InputFeatures("x", InputType::decimal, 0, false), InputFeatures("y", InputType::decimal, 0, false) };
    outerDivType.getOutputSize = outputSizeOuterProduct;
    outerDivType.buildUI = [](NodeComponent&, NodeData&) {};
    outerDivType.onResized = [](NodeComponent&) {};
    outerDivType.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& inputs, std::span<ddtype> output, const RunnerInput& inlineInstance)
        {
            for (int i = 0; i < static_cast<int>(inputs[0].size()); ++i) {
                const double xv = inputs[0][i].d;
                for (int j = 0; j < static_cast<int>(inputs[1].size()); ++j) {
                    const int z = j + i * static_cast<int>(inputs[1].size());
                    const double yv = inputs[1][j].d;
                    output[z] = (yv != 0.0) ? (xv / yv) : 0.0;
                }
            }
        };
    outerDivType.outputType = InputType::decimal;
    outerDivType.alwaysOutputsRuntimeData = false;
    outerDivType.fromScene = nullptr;
    registry.push_back(outerDivType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("shift /").getTextDescription(), registry.size() - 1});

    // ======== VST / audio inputs ========
    NodeType sidechainType(27);
    sidechainType.name = "sidechain";
    sidechainType.address = "audio/vst/";
    sidechainType.tooltip = "Sidechain input (L or R depending on channel).";
    sidechainType.inputs = {};
    sidechainType.getOutputSize = outputSize1Known;
    sidechainType.buildUI = [](NodeComponent&, NodeData&) {};
    sidechainType.onResized = [](NodeComponent&) {};
    sidechainType.execute = [](const NodeData&, UserInput& userInput, const std::vector<std::span<ddtype>>&, std::span<ddtype> output, const RunnerInput& inlineInstance)
        {
            output[0] = userInput.isStereoRight ? userInput.sideChainR : userInput.sideChainL;
        };
    sidechainType.outputType = InputType::decimal;
    sidechainType.alwaysOutputsRuntimeData = false;
    sidechainType.fromScene = nullptr;
    registry.push_back(sidechainType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("alt shift s").getTextDescription(), registry.size() - 1 });

    NodeType leftInType(28);
    leftInType.name = "left input";
    leftInType.address = "audio/vst/";
    leftInType.tooltip = "Current frame's left-channel input sample.";
    leftInType.inputs = {};
    leftInType.getOutputSize = outputSize1Known;
    leftInType.buildUI = [](NodeComponent&, NodeData&) {};
    leftInType.onResized = [](NodeComponent&) {};
    leftInType.execute = [](const NodeData&, UserInput& userInput, const std::vector<std::span<ddtype>>&, std::span<ddtype> output, const RunnerInput& inlineInstance)
        {
            output[0] = userInput.leftInput;
        };
    leftInType.outputType = InputType::decimal;
    leftInType.alwaysOutputsRuntimeData = false;
    leftInType.fromScene = nullptr;
    registry.push_back(leftInType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("shift i").getTextDescription(), registry.size() - 1 });

    NodeType rightInType(29);
    rightInType.name = "right input";
    rightInType.address = "audio/vst/";
    rightInType.tooltip = "Current frame's right-channel input sample.";
    rightInType.inputs = {};
    rightInType.getOutputSize = outputSize1Known;
    rightInType.buildUI = [](NodeComponent&, NodeData&) {};
    rightInType.onResized = [](NodeComponent&) {};
    rightInType.execute = [](const NodeData&, UserInput& userInput, const std::vector<std::span<ddtype>>&, std::span<ddtype> output, const RunnerInput& inlineInstance)
        {
            output[0] = userInput.rightInput;
        };
    rightInType.outputType = InputType::decimal;
    rightInType.alwaysOutputsRuntimeData = false;
    rightInType.fromScene = nullptr;
    registry.push_back(rightInType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("alt i").getTextDescription(), registry.size() - 1 });

    NodeType stereoInType(30);
    stereoInType.name = "stereo input";
    stereoInType.address = "audio/vst/";
    stereoInType.tooltip = "Selects L or R for the current channel pass.";
    stereoInType.inputs = {};
    stereoInType.getOutputSize = outputSize1Known;
    stereoInType.buildUI = [](NodeComponent&, NodeData&) {};
    stereoInType.onResized = [](NodeComponent&) {};
    stereoInType.execute = [](const NodeData&, UserInput& userInput, const std::vector<std::span<ddtype>>&, std::span<ddtype> output, const RunnerInput& inlineInstance)
        {
            output[0] = userInput.isStereoRight ? userInput.rightInput : userInput.leftInput;
        };
    stereoInType.outputType = InputType::decimal;
    stereoInType.alwaysOutputsRuntimeData = false;
    stereoInType.fromScene = nullptr;
    registry.push_back(stereoInType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("i").getTextDescription(), registry.size() - 1 });


    {
        NodeType t(31);
        t.name = "custom data";
        t.address = "audio/";
        t.tooltip = "";
        t.inputs = { };
        t.getOutputSize = outputSizeByNamedProperty("size");
        t.buildUI = [](NodeComponent& comp, NodeData&) {
            comp.inputGUIElements.push_back(std::make_unique<VisualVectorCreator>(comp));
            auto* visBox = dynamic_cast<VisualVectorCreator*>(comp.inputGUIElements.back().get());
            fillInnerBounds(comp, visBox, 0.0, 1.0);
            comp.addAndMakeVisible(visBox);
            };
        t.onResized = [](NodeComponent& comp) {
            if (!comp.inputGUIElements.empty()) {
                auto* visBox = dynamic_cast<VisualVectorCreator*>(comp.inputGUIElements.back().get());
                fillInnerBounds(comp, visBox, 0.0, 1.0);
            }
            };
        t.execute = [](const NodeData& nd, UserInput&, const std::vector<std::span<ddtype>>& inputs,
            std::span<ddtype> output, const RunnerInput& inlineInstance)
            {
                const auto& src = nd.optionalStoredAudio;
                const int srcSize = static_cast<int>(src.size());
                const int dstSize = static_cast<int>(output.size());

                if (srcSize == 0 || dstSize == 0) {
                    for (int i = 0; i < dstSize; i++) output[i].d = 0.0;
                    return;
                }

                for (int i = 0; i < dstSize; i++) {
                    double virtualIndex = static_cast<double>(i) / dstSize;
                    double srcPos = virtualIndex * srcSize;
                    int j = i;
                    if (j < 0) j = 0;
                    if (j >= srcSize) j = srcSize - 1;
                    output[i].d = src[j].d;

                }
            };
        t.outputType = InputType::decimal;
        t.alwaysOutputsRuntimeData = false;
        t.fromScene = nullptr;
        registry.push_back(t);
        boolToIntType = &registry.back();
    }

    NodeType velocity(32);
    velocity.name = "velocity";
    velocity.address = "audio/midi/";
    velocity.tooltip = "Vector of 128 values for all MIDI note velocities";
    velocity.inputs = {};
    velocity.getOutputSize = outputSizeAllMidi;
    velocity.buildUI = [](NodeComponent&, NodeData&) {};
    velocity.onResized = [](NodeComponent&) {};
    velocity.execute = [](const NodeData&, UserInput& userInput, const std::vector<std::span<ddtype>>&, std::span<ddtype> output, const RunnerInput& inlineInstance)
        {
            for (int i = 0; i < 128; i += 1) {
                output[i] = userInput.noteVelocity[i];
            }
            
        };
    velocity.outputType = InputType::decimal;
    velocity.alwaysOutputsRuntimeData = true;
    velocity.fromScene = nullptr;
    registry.push_back(velocity);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("shift v").getTextDescription(), registry.size() - 1 });


    NodeType pitchWheelType(33);
    pitchWheelType.name = "pitch wheel";
    pitchWheelType.address = "audio/midi/";
    pitchWheelType.tooltip = "Current MIDI pitch wheel value (runtime).";
    pitchWheelType.inputs = {};
    pitchWheelType.getOutputSize = outputSize1Known;
    pitchWheelType.buildUI = [](NodeComponent&, NodeData&) {};
    pitchWheelType.onResized = [](NodeComponent&) {};
    pitchWheelType.execute = [](const NodeData&, UserInput& userInput, const std::vector<std::span<ddtype>>&, std::span<ddtype> output, const RunnerInput& inlineInstance)
        {
            output[0] = userInput.pitchWheelValue;
        };
    pitchWheelType.outputType = InputType::decimal;
    pitchWheelType.alwaysOutputsRuntimeData = true;
    pitchWheelType.fromScene = nullptr;
    registry.push_back(pitchWheelType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("p").getTextDescription(), registry.size() - 1 });

    NodeType allNotesType(34);
    allNotesType.name = "note on";
    allNotesType.address = "audio/midi/";
    allNotesType.tooltip = "Vector of 128 booleans for all MIDI notes.";
    allNotesType.inputs = {};
    allNotesType.getOutputSize = outputSizeAllMidi;
    allNotesType.buildUI = [](NodeComponent&, NodeData&) {};
    allNotesType.onResized = [](NodeComponent&) {};
    allNotesType.execute = [](const NodeData&, UserInput& userInput, const std::vector<std::span<ddtype>>&, std::span<ddtype> output, const RunnerInput& inlineInstance)
        {
            for (int i = 0; i < 128; ++i) output[i] = userInput.notesOn[i];
        };
    allNotesType.outputType = InputType::boolean;
    allNotesType.alwaysOutputsRuntimeData = true;
    allNotesType.fromScene = nullptr;
    registry.push_back(allNotesType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("shift n").getTextDescription(), registry.size() - 1 });

    NodeType waveCycleType(35);
    waveCycleType.name = "wave cycle";
    waveCycleType.address = "audio/midi/";
    waveCycleType.tooltip = "Per-note cycle phase in [0,1) for 128 MIDI notes.";
    waveCycleType.inputs = {};
    waveCycleType.getOutputSize = outputSizeAllMidi;
    waveCycleType.buildUI = [](NodeComponent&, NodeData&) {};
    waveCycleType.onResized = [](NodeComponent&) {};
    waveCycleType.execute = [](const NodeData&, UserInput& userInput, const std::vector<std::span<ddtype>>&, std::span<ddtype> output, const RunnerInput& inlineInstance)
        {
            for (int i = 0; i < 128; ++i) output[i] = userInput.noteCycle[i];
        };
    waveCycleType.outputType = InputType::decimal;
    waveCycleType.alwaysOutputsRuntimeData = true;
    waveCycleType.fromScene = nullptr;
    registry.push_back(waveCycleType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("w").getTextDescription(), registry.size() - 1 });

    // ======== Reductions / vector ops ========
    NodeType sumType(36);
    sumType.name = "sum";
    sumType.address = "math/vector/reductions/";
    sumType.tooltip = "Sum of all elements.";
    sumType.inputs = { InputFeatures("x", InputType::decimal, 0, false) };
    sumType.getOutputSize = outputSize1Known;
    sumType.buildUI = [](NodeComponent&, NodeData&) {};
    sumType.onResized = [](NodeComponent&) {};
    sumType.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& inputs, std::span<ddtype> output, const RunnerInput& inlineInstance)
        {
            double s = 0.0;
            for (ddtype v : inputs[0]) s += v.d;
            output[0] = s;
        };
    sumType.outputType = InputType::decimal;
    sumType.alwaysOutputsRuntimeData = false;
    sumType.fromScene = nullptr;
    registry.push_back(sumType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("alt +").getTextDescription(), registry.size() - 1 });

    NodeType avgType(37);
    avgType.name = "average";
    avgType.address = "math/vector/reductions/";
    avgType.tooltip = "Arithmetic mean of all elements.";
    avgType.inputs = { InputFeatures("x", InputType::decimal, 0, false) };
    avgType.getOutputSize = outputSize1Known;
    avgType.buildUI = [](NodeComponent&, NodeData&) {};
    avgType.onResized = [](NodeComponent&) {};
    avgType.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& inputs, std::span<ddtype> output, const RunnerInput& inlineInstance)
        {
            double s = 0.0; int c = 0;
            for (ddtype v : inputs[0]) { s += v.d; ++c; }
            output[0] = (c > 0) ? (s / c) : 0.0;
        };
    avgType.outputType = InputType::decimal;
    avgType.alwaysOutputsRuntimeData = false;
    avgType.fromScene = nullptr;
    registry.push_back(avgType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("alt a").getTextDescription(), registry.size() - 1 });

    NodeType maxType(38);
    maxType.name = "max element";
    maxType.address = "math/vector/reductions/";
    maxType.tooltip = "Maximum element.";
    maxType.inputs = { InputFeatures("x", InputType::decimal, 0, false) };
    maxType.getOutputSize = outputSize1Known;
    maxType.buildUI = [](NodeComponent&, NodeData&) {};
    maxType.onResized = [](NodeComponent&) {};
    maxType.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& inputs, std::span<ddtype> output, const RunnerInput& inlineInstance)
        {
            if (inputs[0].empty()) { output[0] = 0.0; return; }
            double m = inputs[0][0].d;
            for (ddtype v : inputs[0]) if (v.d > m) m = v.d;
            output[0] = m;
        };
    maxType.outputType = InputType::decimal;
    maxType.alwaysOutputsRuntimeData = false;
    maxType.fromScene = nullptr;
    registry.push_back(maxType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("alt m").getTextDescription(), registry.size() - 1 });

    NodeType minType(39);
    minType.name = "min element";
    minType.address = "math/vector/reductions/";
    minType.tooltip = "Minimum element.";
    minType.inputs = { InputFeatures("x", InputType::decimal, 0, false) };
    minType.getOutputSize = outputSize1Known;
    minType.buildUI = [](NodeComponent&, NodeData&) {};
    minType.onResized = [](NodeComponent&) {};
    minType.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& inputs, std::span<ddtype> output, const RunnerInput& inlineInstance)
        {
            if (inputs[0].empty()) { output[0] = 0.0; return; }
            double m = inputs[0][0].d;
            for (ddtype v : inputs[0]) if (v.d < m) m = v.d;
            output[0] = m;
        };
    minType.outputType = InputType::decimal;
    minType.alwaysOutputsRuntimeData = false;
    minType.fromScene = nullptr;
    registry.push_back(minType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("shift alt m").getTextDescription(), registry.size() - 1 });

    NodeType getElemType(40);
    getElemType.name = "get element";
    getElemType.address = "math/vector/ops/";
    getElemType.tooltip = "Reads element at index from a vector.";
    getElemType.inputs = { InputFeatures("vector", InputType::any, 0, false), InputFeatures("index", InputType::integer, 1, false) };
    getElemType.getOutputSize = outputSize1Known;
    getElemType.buildUI = [](NodeComponent&, NodeData&) {};
    getElemType.onResized = [](NodeComponent&) {};
    getElemType.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& inputs, std::span<ddtype> output, const RunnerInput& inlineInstance)
        {
            if (inputs[0].empty() || inputs[1].empty()) { output[0] = 0.0; return; }
            const int64_t idx = inputs[1][0].i;
            if (idx < 0 || idx >= static_cast<int>(inputs[0].size())) { output[0] = 0.0; return; }
            output[0] = inputs[0][idx];
        };
    getElemType.outputType = InputType::any;
    getElemType.alwaysOutputsRuntimeData = false;
    getElemType.fromScene = nullptr;
    registry.push_back(getElemType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("alt e").getTextDescription(), registry.size() - 1 });

    NodeType sliceElemType(41);
   sliceElemType.name = "vector lookup";
   sliceElemType.address = "math/vector/ops/";
   sliceElemType.tooltip = "Reads elements at all indices from a vector.";
   sliceElemType.inputs = { InputFeatures("vector", InputType::decimal, 0, false), InputFeatures("indices", InputType::integer, 0, false) };
   sliceElemType.getOutputSize = outputSizeByInputPin(1);
   sliceElemType.buildUI = [](NodeComponent&, NodeData&) {};
   sliceElemType.onResized = [](NodeComponent&) {};
   sliceElemType.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& inputs, std::span<ddtype> output, const RunnerInput& inlineInstance)
    {
        for (int i = 0; i < inputs[1].size(); i += 1) {
            const int64_t idx = inputs[1][i].i;
            if (idx < 0 || idx >= static_cast<int>(inputs[0].size())) { output[i] = 0.0; }
            else {
                output[i] = inputs[0][i];
            }
            
        }
        
    };
    sliceElemType.outputType = InputType::decimal;
    sliceElemType.alwaysOutputsRuntimeData = false;
    sliceElemType.fromScene = nullptr;
    registry.push_back(sliceElemType);

    NodeType changeElemType(42);
    changeElemType.name = "change element";
    changeElemType.address = "math/vector/ops/";
    changeElemType.tooltip = "Copies vector and overwrites one element by index.";
    changeElemType.inputs = {
        InputFeatures("vector", InputType::decimal, 0, false),
        InputFeatures("index",  InputType::integer, 1, false),
        InputFeatures("value",  InputType::decimal, 1, false)
    };
    changeElemType.getOutputSize = outputSizeComponentWise;
    changeElemType.buildUI = [](NodeComponent&, NodeData&) {};
    changeElemType.onResized = [](NodeComponent&) {};
    changeElemType.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& inputs, std::span<ddtype> output, const RunnerInput& inlineInstance)
        {
            if (inputs[0].empty() || inputs[1].empty() || inputs[2].empty()) return;
            const int n = static_cast<int>(inputs[0].size());
            for (int i = 0; i < n; ++i) output[i] = inputs[0][i];
            const int64_t idx = inputs[1][0].i;
            if (idx < 0 || idx >= n) return;
            output[idx] = inputs[2][0].d;
        };
    changeElemType.outputType = InputType::decimal;
    changeElemType.alwaysOutputsRuntimeData = false;
    changeElemType.fromScene = nullptr;
    registry.push_back(changeElemType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("shift alt e").getTextDescription(), registry.size() - 1 });

    NodeType rangeType(43);
    rangeType.name = "range";
    rangeType.address = "math/vector/create/";
    rangeType.tooltip = "Creates a sequence: start, start+step, ... (step count items).";
    rangeType.inputs = {
        InputFeatures("start",      InputType::decimal, 1, false),
        InputFeatures("step count", InputType::integer, 1, true),
        InputFeatures("step size",  InputType::decimal, 1, false)
    };
    for (auto& f : rangeType.inputs) f.defaultValue = 1.0;
    rangeType.getOutputSize = outputSizeFromInputScalar(1);
    rangeType.buildUI = [](NodeComponent&, NodeData&) {};
    rangeType.onResized = [](NodeComponent&) {};
    rangeType.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& inputs, std::span<ddtype> output, const RunnerInput& inlineInstance)
        {
            int64_t stepCount = inputs[1][0].i;
            if (stepCount < 1) stepCount = 1;
            double value = inputs[0][0].d;
            const double step = inputs[2][0].d;
            for (int i = 0; i < stepCount; ++i) { output[i] = value; value += step; }
        };
    rangeType.outputType = InputType::decimal;
    rangeType.alwaysOutputsRuntimeData = false;
    rangeType.fromScene = nullptr;
    registry.push_back(rangeType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("r").getTextDescription(), registry.size() - 1 });

    NodeType repeatType(44);
    repeatType.name = "repeat";
    repeatType.address = "math/vector/create/";
    repeatType.tooltip = "Repeats the value x n times.";
    repeatType.inputs = {
        InputFeatures("x",      InputType::decimal, 1, false),
        InputFeatures("n", InputType::integer, 1, true)
    };
    repeatType.inputs[1].defaultValue.i = 1;
    repeatType.getOutputSize = outputSizeFromInputScalar(1);
    repeatType.buildUI = [](NodeComponent&, NodeData&) {};
    repeatType.onResized = [](NodeComponent&) {};
    repeatType.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& inputs, std::span<ddtype> output, const RunnerInput& inlineInstance)
        {
            int64_t stepCount = inputs[1][0].i;
            double value = inputs[0][0].d;
            for (int i = 0; i < stepCount; ++i) { output[i] = value; }
        };
    repeatType.outputType = InputType::decimal;
    repeatType.alwaysOutputsRuntimeData = false;
    repeatType.fromScene = nullptr;
    registry.push_back(repeatType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("shift r").getTextDescription(), registry.size() - 1 });



    // ======== Waves ========
    NodeType sinWaveType(45);
    sinWaveType.name = "sin wave";
    sinWaveType.address = "audio/waves/";
    sinWaveType.tooltip = "Sine from phase in [0,1): sin(2π * phase).";
    sinWaveType.inputs = { InputFeatures("wave cycle", InputType::decimal, 0, false) };
    sinWaveType.getOutputSize = outputSizeEqualsSingleInputSize;
    sinWaveType.buildUI = [](NodeComponent&, NodeData&) {};
    sinWaveType.onResized = [](NodeComponent&) {};
    sinWaveType.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& inputs, std::span<ddtype> output, const RunnerInput& inlineInstance)
        {
            for (int i = 0; i < static_cast<int>(output.size()); ++i)
                output[i] = std::sin(2.0 * 3.14159265358979323846 * inputs[0][i].d);
        };
    sinWaveType.outputType = InputType::decimal;
    sinWaveType.alwaysOutputsRuntimeData = false;
    sinWaveType.fromScene = nullptr;
    registry.push_back(sinWaveType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("s").getTextDescription(), registry.size() - 1 });

    NodeType squareType(46);
    squareType.name = "square";
    squareType.address = "audio/waves/";
    squareType.tooltip = "Square from phase in [0,1): outputs +1 or −1 at 0.5 threshold.";
    squareType.inputs = { InputFeatures("wave cycle", InputType::decimal, 0, false) };
    squareType.getOutputSize = outputSizeEqualsSingleInputSize;
    squareType.buildUI = [](NodeComponent&, NodeData&) {};
    squareType.onResized = [](NodeComponent&) {};
    squareType.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& inputs, std::span<ddtype> output, const RunnerInput& inlineInstance)
        {
            for (int i = 0; i < static_cast<int>(output.size()); ++i)
                output[i] = (inputs[0][i].d > 0.5) ? -1.0 : 1.0; // preserves original behavior
        };
    squareType.outputType = InputType::decimal;
    squareType.alwaysOutputsRuntimeData = false;
    squareType.fromScene = nullptr;
    registry.push_back(squareType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("q").getTextDescription(), registry.size() - 1 });

    NodeType triangleType(47);
    triangleType.name = "zigzag";
    triangleType.address = "audio/waves/";
    triangleType.tooltip = "Triangle wave from phase in [0,1), scaled to [-1,1].";
    triangleType.inputs = { InputFeatures("wave cycle", InputType::decimal, 0, false) };
    triangleType.getOutputSize = outputSizeEqualsSingleInputSize;
    triangleType.buildUI = [](NodeComponent&, NodeData&) {};
    triangleType.onResized = [](NodeComponent&) {};
    triangleType.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& inputs, std::span<ddtype> output, const RunnerInput& inlineInstance)
        {
            for (int i = 0; i < static_cast<int>(output.size()); ++i) {
                double t = std::fmod(inputs[0][i].d, 1.0); if (t < 0.0) t += 1.0;
                const double tri = 1.0 - std::abs(2.0 * t - 1.0);
                output[i] = 2.0 * tri - 1.0;
            }
        };
    triangleType.outputType = InputType::decimal;
    triangleType.alwaysOutputsRuntimeData = false;
    triangleType.fromScene = nullptr;
    registry.push_back(triangleType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("z").getTextDescription(), registry.size() - 1 });

    NodeType circleWaveType(48);
    circleWaveType.name = "circle";
    circleWaveType.address = "audio/waves/";
    circleWaveType.tooltip = "Semicircle wave from phase in [0,1), scaled to [-1,1].";
    circleWaveType.inputs = { InputFeatures("wave cycle", InputType::decimal, 0, false) };
    circleWaveType.getOutputSize = outputSizeEqualsSingleInputSize;
    circleWaveType.buildUI = [](NodeComponent&, NodeData&) {};
    circleWaveType.onResized = [](NodeComponent&) {};
    circleWaveType.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& inputs, std::span<ddtype> output, const RunnerInput& inlineInstance)
        {
            for (int i = 0; i < static_cast<int>(output.size()); ++i) {
                double x = inputs[0][i].d;
                double t = x - std::floor(x);   
                double m = 2.0 * t;                
                double k = std::floor(m);       
                double u = 2.0 * (m - k) - 1.0;    
                double a = std::sqrt(std::max(0.0, 1.0 - u * u));
                output[i] = (1.0 - 2.0 * k) * a;
            }
        };
    circleWaveType.outputType = InputType::decimal;
    circleWaveType.alwaysOutputsRuntimeData = false;
    circleWaveType.fromScene = nullptr;
    registry.push_back(circleWaveType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("z").getTextDescription(), registry.size() - 1 });


    // ======== Envelope (ADSR, per MIDI note) ========
    NodeType envelopeType(49);
    envelopeType.name = "envelope (ADSR)";
    envelopeType.address = "audio/midi/envelopes/";
    envelopeType.tooltip = "Per-note ADSR: outputs 128 values in [0,1] from note start/end and ADSR times.";
    envelopeType.inputs = {
        // scalar controls (seconds for A/D/R; [0,1] for sustain)
        InputFeatures("attack (s)",  InputType::decimal, 1, false),
        InputFeatures("decay (s)",   InputType::decimal, 1, false),
        InputFeatures("sustain",     InputType::decimal, 1, false),
        InputFeatures("release (s)", InputType::decimal, 1, false)
    };
    envelopeType.inputs[0].defaultValue = 0.01;
    envelopeType.inputs[0].defaultValue = 0.2;
    envelopeType.inputs[0].defaultValue = 0.8;
    envelopeType.inputs[0].defaultValue = 0.3;
    envelopeType.getOutputSize = outputSizeAllMidi;
    envelopeType.buildUI = [](NodeComponent&, NodeData&) {};
    envelopeType.onResized = [](NodeComponent&) {};
    envelopeType.outputType = InputType::decimal;
    envelopeType.alwaysOutputsRuntimeData = true;
    envelopeType.fromScene = nullptr;

    envelopeType.execute = [](
        const NodeData&,
        UserInput& userInput,
        const std::vector<std::span<ddtype>>& in,
        std::span<ddtype> out, const RunnerInput& inlineInstance)
        {
            // Read scalar parameters (with sane clamps)
            const double A = (in.size() > 0 && !in[0].empty()) ? std::max(0.0, in[0][0].d) : 0.0;
            const double D = (in.size() > 1 && !in[1].empty()) ? std::max(0.0, in[1][0].d) : 0.0;
            const double S = (in.size() > 2 && !in[2].empty()) ? std::clamp(in[2][0].d, 0.0, 1.0) : 0.0;
            const double R = (in.size() > 3 && !in[3].empty()) ? std::max(0.0, in[3][0].d) : 0.0;

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
        NodeType t(50);
        t.name = "round toward zero";
        t.address = "math/ops/";
        t.tooltip = "Element-wise truncate towards zero.";
        t.inputs = { InputFeatures("x", InputType::decimal, 0, false) };
        t.getOutputSize = outputSizeEqualsSingleInputSize;
        t.buildUI = [](NodeComponent&, NodeData&) {};
        t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& in,
            std::span<ddtype> out, const RunnerInput&)
            {
                const auto& x = in[0];
                for (int i = 0; i < (int)x.size(); ++i) out[i] = (int64_t)x[i].d;
            };
        t.outputType = InputType::integer; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr;
        registry.push_back(t);
        // (No hotkey assigned)
    }

    // ========= floor =========
    {
        NodeType t(51);
        t.name = "floor";
        t.address = "math/ops/";
        t.tooltip = "Element-wise floor.";
        t.inputs = { InputFeatures("x", InputType::decimal, 0, false) };
        t.getOutputSize = outputSizeEqualsSingleInputSize;
        t.buildUI = [](NodeComponent&, NodeData&) {};
        t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& in,
            std::span<ddtype> out, const RunnerInput&)
            {
                const auto& x = in[0];
                for (int i = 0; i < (int)x.size(); ++i) out[i] = (int64_t)std::floor(x[i].d);
            };
        t.outputType = InputType::decimal; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr;
        registry.push_back(t);
    }

    // ========= ceil =========
    {
        NodeType t(52);
        t.name = "ceil";
        t.address = "math/ops/";
        t.tooltip = "Element-wise ceil.";
        t.inputs = { InputFeatures("x", InputType::decimal, 0, false) };
        t.getOutputSize = outputSizeEqualsSingleInputSize;
        t.buildUI = [](NodeComponent&, NodeData&) {};
        t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& in,
            std::span<ddtype> out, const RunnerInput&)
            {
                const auto& x = in[0];
                for (int i = 0; i < (int)x.size(); ++i) out[i] = (int64_t)std::ceil(x[i].d);
            };
        t.outputType = InputType::decimal; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr;
        registry.push_back(t);
    }

    // ========= pow (x ^ yScalar) =========
    {
        NodeType t(53);
        t.name = "pow";
        t.address = "math/arithmetic/";
        t.tooltip = "Raises each element of x to the power y. y is scalar.";
        t.inputs = {
            InputFeatures("x", InputType::decimal, 0, false), // vector
            InputFeatures("y", InputType::decimal, 1, false)  // scalar exponent
        };
        t.getOutputSize = outputSizeEqualsSingleInputSize; // match x
        t.buildUI = [](NodeComponent&, NodeData&) {};
        t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& in,
            std::span<ddtype> out, const RunnerInput&)
            {
                const auto& x = in[0];
                const double y = in[1].empty() ? 1.0 : in[1][0].d;
                for (int i = 0; i < (int)x.size(); ++i) out[i] = std::pow(std::abs(x[i].d), y);
            };
        t.outputType = InputType::decimal; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr;
        registry.push_back(t);
        // Hotkey: caret (Shift+6)
        keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("shift 6").getTextDescription(),
                                    registry.size() - 1 });
    }

    // ========= pow (x ^ yScalar) =========
    {
        NodeType t(54);
        t.name = "signed pow";
        t.address = "math/arithmetic/";
        t.tooltip = "Raises each element of x to the power y. y is scalar.";
        t.inputs = {
            InputFeatures("x", InputType::decimal, 0, false), // vector
            InputFeatures("y", InputType::decimal, 1, false)  // scalar exponent
        };
        t.getOutputSize = outputSizeEqualsSingleInputSize; // match x
        t.buildUI = [](NodeComponent&, NodeData&) {};
        t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& in,
            std::span<ddtype> out, const RunnerInput&)
            {
                const auto& x = in[0];
                const double y = in[1].empty() ? 1.0 : in[1][0].d;
                
                for (int i = 0; i < (int)x.size(); ++i) {
                    double xx = x[i].d;
                    double sign = xx > 0 ? 1.0 : xx < 0 ? -1.0 : 0.0;
                    out[i].d = std::pow(std::abs(xx), y);
                }
            };
        t.outputType = InputType::decimal; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr;
        registry.push_back(t);
        // Hotkey: caret (Shift+6)
        keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("alt shift 6").getTextDescription(),
                                    registry.size() - 1 });
    }

    // ========= log_n (base b scalar) =========
    {
        NodeType t(55);
        t.name = "log base b";
        t.address = "math/arithmetic/";
        t.tooltip = "Element-wise log_b(x) with scalar base b.";
        t.inputs = {
            InputFeatures("x", InputType::decimal, 0, false),
            InputFeatures("b", InputType::decimal, 1, false) // scalar base
        };
        t.getOutputSize = outputSizeEqualsSingleInputSize;
        t.buildUI = [](NodeComponent&, NodeData&) {};
        t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& in,
            std::span<ddtype> out, const RunnerInput&)
            {
                const auto& x = in[0];
                const double b = in[1].empty() ? 2.0 : in[1][0].d;
                const double lb = (b > 0.0 && b != 1.0) ? std::log(b) : 0.0;
                for (int i = 0; i < (int)x.size(); ++i) {
                    const double v = x[i].d;
                    out[i] = (v > 0.0 && lb != 0.0) ? (std::log(v) / lb) : 0.0;
                }
            };
        t.outputType = InputType::decimal; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr;
        registry.push_back(t);
    }

    // ========= natural log (ln) =========
    {
        NodeType t(56);
        t.name = "ln";
        t.address = "math/arithmetic/";
        t.tooltip = "Element-wise natural logarithm.";
        t.inputs = { InputFeatures("x", InputType::decimal, 0, false) };
        t.getOutputSize = outputSizeEqualsSingleInputSize;
        t.buildUI = [](NodeComponent&, NodeData&) {};
        t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& in,
            std::span<ddtype> out, const RunnerInput&)
            {
                const auto& x = in[0];
                for (int i = 0; i < (int)x.size(); ++i) {
                    const double v = x[i].d;
                    out[i] = (v > 0.0) ? std::log(v) : 0.0;
                }
            };
        t.outputType = InputType::decimal; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr;
        registry.push_back(t);
    }

    // ========= secant =========
    {
        NodeType t(57);
        t.name = "sec";
        t.address = "math/trigonometry/";
        t.tooltip = "Element-wise sec(x) = 1 / cos(x).";
        t.inputs = { InputFeatures("x", InputType::decimal, 0, false) };
        t.getOutputSize = outputSizeEqualsSingleInputSize;
        t.buildUI = [](NodeComponent&, NodeData&) {};
        t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& in,
            std::span<ddtype> out, const RunnerInput&)
            {
                const auto& x = in[0];
                for (int i = 0; i < (int)x.size(); ++i) {
                    const double c = std::cos(x[i].d);
                    out[i] = (c != 0.0) ? (1.0 / c) : 0.0;
                }
            };
        t.outputType = InputType::decimal; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr;
        registry.push_back(t);
    }

    // ========= cosecant =========
    {
        NodeType t(58);
        t.name = "csc";
        t.address = "math/trigonometry/";
        t.tooltip = "Element-wise csc(x) = 1 / sin(x).";
        t.inputs = { InputFeatures("x", InputType::decimal, 0, false) };
        t.getOutputSize = outputSizeEqualsSingleInputSize;
        t.buildUI = [](NodeComponent&, NodeData&) {};
        t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& in,
            std::span<ddtype> out, const RunnerInput&)
            {
                const auto& x = in[0];
                for (int i = 0; i < (int)x.size(); ++i) {
                    const double s = std::sin(x[i].d);
                    out[i] = (s != 0.0) ? (1.0 / s) : 0.0;
                }
            };
        t.outputType = InputType::decimal; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr;
        registry.push_back(t);
    }

    // ========= cotangent =========
    {
        NodeType t(59);
        t.name = "cot";
        t.address = "math/trigonometry/";
        t.tooltip = "Element-wise cot(x) = 1 / tan(x).";
        t.inputs = { InputFeatures("x", InputType::decimal, 0, false) };
        t.getOutputSize = outputSizeEqualsSingleInputSize;
        t.buildUI = [](NodeComponent&, NodeData&) {};
        t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& in,
            std::span<ddtype> out, const RunnerInput&)
            {
                const auto& x = in[0];
                for (int i = 0; i < (int)x.size(); ++i) {
                    const double t = std::tan(x[i].d);
                    out[i].d = (t != 0.0) ? (1.0 / t) : 0.0;
                }
            };
        t.outputType = InputType::decimal; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr;
        registry.push_back(t);
    }

    // ========= clamp (generic) =========
    {
        NodeType t(60);
        t.name = "clamp";
        t.address = "math/range/";
        t.tooltip = "Clamp each element of x to [min, max].";
        t.inputs = {
            InputFeatures("x",   InputType::decimal, 0, false),
            InputFeatures("min", InputType::decimal, 1, false),
            InputFeatures("max", InputType::decimal, 1, false)
        };
        t.inputs[1].defaultValue.d = 0.0;
        t.inputs[2].defaultValue.d = 1.0;
        t.getOutputSize = outputSizeEqualsSingleInputSize;
        t.buildUI = [](NodeComponent&, NodeData&) {};
        t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& in,
            std::span<ddtype> out, const RunnerInput&)
            {
                const auto& x = in[0];
                const double mn = in[1].empty() ? 0.0 : in[1][0].d;
                const double mx = in[2].empty() ? 1.0 : in[2][0].d;
                const double lo = std::min(mn, mx), hi = std::max(mn, mx);
                for (int i = 0; i < (int)x.size(); ++i)
                    out[i].d = std::clamp(x[i].d, lo, hi);
            };
        t.outputType = InputType::decimal; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr;
        registry.push_back(t);
    }


    // ========= clamp [-1,1] =========
    {
        NodeType t(61);
        t.name = "clamp [-1,1]";
        t.address = "math/range/";
        t.tooltip = "Clamp to [-1, 1].";
        t.inputs = { InputFeatures("x", InputType::decimal, 0, false) };
        t.getOutputSize = outputSizeEqualsSingleInputSize;
        t.buildUI = [](NodeComponent&, NodeData&) {};
        t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& in,
            std::span<ddtype> out, const RunnerInput&)
            {
                const auto& x = in[0];
                for (int i = 0; i < (int)x.size(); ++i)
                    out[i].d = std::clamp(x[i].d, -1.0, 1.0);
            };
        t.outputType = InputType::decimal; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr;
        registry.push_back(t);
    }

    // ===== wrap helpers =====
    

    // ========= wrap (generic) =========
    {
        NodeType t(62);
        t.name = "wrap";
        t.address = "math/range/";
        t.tooltip = "Wrap each element into [min, max) (half-open).";
        t.inputs = {
            InputFeatures("x",   InputType::decimal, 0, false),
            InputFeatures("min", InputType::decimal, 1, false),
            InputFeatures("max", InputType::decimal, 1, false)
        };
        t.inputs[1].defaultValue.d = 0.0;
        t.inputs[2].defaultValue.d = 1.0;
        t.getOutputSize = outputSizeEqualsSingleInputSize;
        t.buildUI = [](NodeComponent&, NodeData&) {};
        t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& in,
            std::span<ddtype> out, const RunnerInput&)
            {
                const auto& x = in[0];
                const double mn = in[1].empty() ? 0.0 : in[1][0].d;
                const double mx = in[2].empty() ? 1.0 : in[2][0].d;
                const double lo = std::min(mn, mx), hi = std::max(mn, mx);
                for (int i = 0; i < (int)x.size(); ++i)
                    out[i].d = wrapTo(x[i].d, lo, hi);
            };
        t.outputType = InputType::decimal; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr;
        registry.push_back(t);
    }

    // ========= wrap [-1,1) =========
    {
        NodeType t(63);
        t.name = "wrap [-1,1)";
        t.address = "math/range/";
        t.tooltip = "Wrap to [-1, 1).";
        t.inputs = { InputFeatures("x", InputType::decimal , 0, false) };
        t.getOutputSize = outputSizeEqualsSingleInputSize;
        t.buildUI = [](NodeComponent&, NodeData&) {};
        t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& in,
            std::span<ddtype> out, const RunnerInput&)
            {
                const auto& x = in[0];
                for (int i = 0; i < (int)x.size(); ++i)
                    out[i].d = wrapTo(x[i].d, -1.0, 1.0);
            };
        t.outputType = InputType::decimal; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr;
        registry.push_back(t);
    }

    // ========= soft clamp (tanh) =========
    {
        NodeType t(64);
        t.name = "soft clamp (tanh)";
        t.address = "math/range/";
        t.tooltip = "Smoothly squashes x into (min, max) via y = m + r * tanh((x - m)/r) with m=(min+max)/2, r=(max-min)/2.";
        t.inputs = {
            InputFeatures("x",   InputType::decimal, 0, false),  // vector
            InputFeatures("min", InputType::decimal, 1, false),  // scalar
            InputFeatures("max", InputType::decimal, 1, false)   // scalar
        };
        t.inputs[1].defaultValue.d = 0.0;
        t.inputs[2].defaultValue.d = 1.0;
        t.getOutputSize = outputSizeEqualsSingleInputSize;
        t.buildUI = [](NodeComponent&, NodeData&) {};
        t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&,
            const std::vector<std::span<ddtype>>& in,
            std::span<ddtype> out,
            const RunnerInput&)
            {
                const auto& x = in[0];
                const double mnIn = in[1].empty() ? -1.0 : in[1][0].d;
                const double mxIn = in[2].empty() ? 1.0 : in[2][0].d;

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
                        out[i].d = std::clamp(x[i].d, lo, hi);
                    return;
                }

                // Soft clamp: y = m + r * tanh((x - m)/r)
                for (int i = 0; i < (int)x.size(); ++i)
                {
                    const double z = (x[i].d - m) / (r + eps);
                    out[i].d = m + r * std::tanh(z);
                }
            };
        t.outputType = InputType::decimal;
        t.alwaysOutputsRuntimeData = false;
        t.fromScene = nullptr;

        registry.push_back(t);
        // (No hotkey by default; say the word if you want one)
    }

    // ========= abs =========
    {
        NodeType t(65);
        t.name = "abs"; t.address = "math/ops/"; t.tooltip = "Element-wise absolute value.";
        t.inputs = { InputFeatures("x", InputType::decimal, 0, false) };
        t.getOutputSize = outputSizeEqualsSingleInputSize;
        t.buildUI = [](NodeComponent&, NodeData&) {}; t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& in, std::span<ddtype> out, const RunnerInput&) {
            const auto& x = in[0]; for (int i = 0; i < (int)x.size(); ++i) out[i].d = std::abs(x[i].d);
            };
        t.outputType = InputType::decimal; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr;
        registry.push_back(t);
    }

    // ========= sign =========
    {
        NodeType t(66);
        t.name = "sign"; t.address = "math/ops/"; t.tooltip = "Element-wise sign: {-1,0,1}.";
        t.inputs = { InputFeatures("x", InputType::decimal, 0, false) };
        t.getOutputSize = outputSizeEqualsSingleInputSize;
        t.buildUI = [](NodeComponent&, NodeData&) {}; t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& in, std::span<ddtype> out, const RunnerInput&) {
            const auto& x = in[0]; for (int i = 0; i < (int)x.size(); ++i) out[i].d = (x[i].d > 0) ? 1.0 : ((x[i].d < 0) ? -1.0 : 0.0);
            };
        t.outputType = InputType::decimal; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr;
        registry.push_back(t);
    }

    // ========= sqrt =========
    {
        NodeType t(67);
        t.name = "sqrt"; t.address = "math/ops/"; t.tooltip = "Element-wise sqrt (negatives -> 0).";
        t.inputs = { InputFeatures("x", InputType::decimal, 0, false) };
        t.getOutputSize = outputSizeEqualsSingleInputSize;
        t.buildUI = [](NodeComponent&, NodeData&) {}; t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& in, std::span<ddtype> out, const RunnerInput&) {
            const auto& x = in[0]; for (int i = 0; i < (int)x.size(); ++i) out[i].d = (x[i].d >= 0.0) ? std::sqrt(x[i].d) : 0.0;
            };
        t.outputType = InputType::decimal; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr;
        registry.push_back(t);
    }

    // ========= nthroot (x^(1/nScalar)) =========
    {
        NodeType t(68);
        t.name = "nthroot"; t.address = "math/ops/"; t.tooltip = "Element-wise n-th root with scalar n.";
        t.inputs = { InputFeatures("x", InputType::decimal, 0, false), InputFeatures("n", InputType::decimal, 1, false) };
        t.getOutputSize = outputSizeEqualsSingleInputSize;
        t.buildUI = [](NodeComponent&, NodeData&) {}; t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& in, std::span<ddtype> out, const RunnerInput&) {
            const auto& x = in[0]; const double n = in[1].empty() ? 2.0 : in[1][0].d;
            const double eps = 1e-12;
            const bool nIsInt = std::abs(n - std::round(n)) < 1e-9;
            const int  nInt = (int)std::llround(n);
            for (int i = 0; i < (int)x.size(); ++i) {
                const double v = x[i].d;
                if (std::abs(n) < eps) { out[i].d = 0.0; continue; }
                if (v < 0.0) {
                    if (nIsInt && (nInt % 2 != 0)) { out[i].d = -std::pow(std::abs(v), 1.0 / n); }
                    else { out[i].d = 0.0; }
                }
                else {
                    out[i].d = std::pow(v, 1.0 / n);
                }
            }
            };
        t.outputType = InputType::decimal; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr;
        registry.push_back(t);
    }

    // ========= exp =========
    {
        NodeType t(69);
        t.name = "exp"; t.address = "math/ops/"; t.tooltip = "Element-wise exp(x).";
        t.inputs = { InputFeatures("x", InputType::decimal, 0, false) };
        t.getOutputSize = outputSizeEqualsSingleInputSize;
        t.buildUI = [](NodeComponent&, NodeData&) {}; t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& in, std::span<ddtype> out, const RunnerInput&) {
            const auto& x = in[0]; for (int i = 0; i < (int)x.size(); ++i) out[i].d = std::exp(x[i].d);
            };
        t.outputType = InputType::decimal; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr;
        registry.push_back(t);
    }

    // ========= arcsin / arccos / arctan =========
    {
        NodeType t(70);
        t.name = "asin"; t.address = "math/trigonometry/inverse/"; t.tooltip = "Element-wise asin(clamp(x,-1,1)).";
        t.inputs = { InputFeatures("x", InputType::decimal, 0, false) }; t.getOutputSize = outputSizeEqualsSingleInputSize;
        t.buildUI = [](NodeComponent&, NodeData&) {}; t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& in, std::span<ddtype> out, const RunnerInput&) {
            const auto& x = in[0]; for (int i = 0; i < (int)x.size(); ++i) { const double v = std::clamp(x[i].d, -1.0, 1.0); out[i].d = std::asin(v); } };
        t.outputType = InputType::decimal; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr; registry.push_back(t);
    }
    {
        NodeType t(71);
        t.name = "acos"; t.address = "math/trigonometry/inverse/"; t.tooltip = "Element-wise acos(clamp(x,-1,1)).";
        t.inputs = { InputFeatures("x", InputType::decimal, 0, false) }; t.getOutputSize = outputSizeEqualsSingleInputSize;
        t.buildUI = [](NodeComponent&, NodeData&) {}; t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& in, std::span<ddtype> out, const RunnerInput&) {
            const auto& x = in[0]; for (int i = 0; i < (int)x.size(); ++i) { const double v = std::clamp(x[i].d, -1.0, 1.0); out[i].d = std::acos(v); } };
        t.outputType = InputType::decimal; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr; registry.push_back(t);
    }
    {
        NodeType t(72);
        t.name = "atan"; t.address = "math/trigonometry/inverse/"; t.tooltip = "Element-wise atan(x).";
        t.inputs = { InputFeatures("x", InputType::decimal, 0, false) }; t.getOutputSize = outputSizeEqualsSingleInputSize;
        t.buildUI = [](NodeComponent&, NodeData&) {}; t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& in, std::span<ddtype> out, const RunnerInput&) {
            const auto& x = in[0]; for (int i = 0; i < (int)x.size(); ++i) out[i].d = std::atan(x[i].d); };
        t.outputType = InputType::decimal; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr; registry.push_back(t);
    }

    // ========= mod (x mod mScalar), sign-safe to [0,|m|) =========
    {
        NodeType t(73);
        t.name = "mod"; t.address = "math/arithmetic/"; t.tooltip = "x mod m (scalar m), result in [0, |m|).";
        t.inputs = { InputFeatures("x", InputType::decimal, 0, false), InputFeatures("m", InputType::decimal, 1, false) };
        t.getOutputSize = outputSizeEqualsSingleInputSize;
        t.buildUI = [](NodeComponent&, NodeData&) {}; t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& in, std::span<ddtype> out, const RunnerInput&) {
            const auto& x = in[0]; const double m = in[1].empty() ? 1.0 : in[1][0].d; const double am = std::abs(m);
            for (int i = 0; i < (int)x.size(); ++i) { double r = am > 0 ? std::fmod(x[i].d, am) : 0.0; if (r < 0) r += am; out[i].d = r; }
            };
        t.outputType = InputType::decimal; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr; registry.push_back(t);
    }

    // ========= reciprocal =========
    {
        NodeType t(74);
        t.name = "reciprocal"; t.address = "math/ops/"; t.tooltip = "Element-wise 1/x (0 -> 0).";
        t.inputs = { InputFeatures("x", InputType::decimal, 0, false) };
        t.getOutputSize = outputSizeEqualsSingleInputSize;
        t.buildUI = [](NodeComponent&, NodeData&) {}; t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& in, std::span<ddtype> out, const RunnerInput&) {
            const auto& x = in[0]; for (int i = 0; i < (int)x.size(); ++i) out[i].d = (x[i].d != 0.0) ? 1.0 / x[i].d : 0.0;
            };
        t.outputType = InputType::decimal; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr; registry.push_back(t);
    }

    // ========= XOR / NAND / NOR / XNOR (component-wise boolean) =========
    
    {
        NodeType t(75); t.name = "xor"; t.address = "math/logic/binary ops/"; t.tooltip = "Boolean XOR component-wise.";
        t.inputs = { InputFeatures("x", InputType::boolean, 0, false), InputFeatures("y", InputType::boolean, 0, false) };
        t.getOutputSize = outputSizeComponentWise; t.buildUI = [](NodeComponent&, NodeData&) {}; t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& in, std::span<ddtype> out, const RunnerInput&) {
            const auto& a = in[0], & b = in[1]; const int n = std::max((int)a.size(), (int)b.size());
            for (int i = 0; i < n; ++i) {
                const bool av = (i < (int)a.size() ? truthy(a[i]) : false);
                const bool bv = (i < (int)b.size() ? truthy(b[i]) : false);
                out[i].i = (av ^ bv) ? 1 : 0;
            }
            };
        t.outputType = InputType::boolean; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr; registry.push_back(t);
    }
    {
        NodeType t(76); t.name = "nand"; t.address = "math/logic/binary ops/"; t.tooltip = "NOT (x AND y) component-wise.";
        t.inputs = { InputFeatures("x", InputType::boolean, 0, false), InputFeatures("y", InputType::boolean, 0, false) };
        t.getOutputSize = outputSizeComponentWise; t.buildUI = [](NodeComponent&, NodeData&) {}; t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& in, std::span<ddtype> out, const RunnerInput&) {
            const auto& a = in[0], & b = in[1]; const int n = std::max((int)a.size(), (int)b.size());
            for (int i = 0; i < n; ++i) {
                const bool v = (i < (int)a.size() ? truthy(a[i]) : false) && (i < (int)b.size() ? truthy(b[i]) : false);
                out[i].i = v ? 0 : 1;
            }
            };
        t.outputType = InputType::boolean; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr; registry.push_back(t);
    }
    {
        NodeType t(77); t.name = "nor"; t.address = "math/logic/binary ops/"; t.tooltip = "NOT (x OR y) component-wise.";
        t.inputs = { InputFeatures("x", InputType::boolean, 0, false), InputFeatures("y", InputType::boolean, 0, false) };
        t.getOutputSize = outputSizeComponentWise; t.buildUI = [](NodeComponent&, NodeData&) {}; t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& in, std::span<ddtype> out, const RunnerInput&) {
            const auto& a = in[0], & b = in[1]; const int n = std::max((int)a.size(), (int)b.size());
            for (int i = 0; i < n; ++i) {
                const bool v = (i < (int)a.size() ? truthy(a[i]) : false) || (i < (int)b.size() ? truthy(b[i]) : false);
                out[i].i = v ? 0 : 1;
            }
            };
        t.outputType = InputType::boolean; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr; registry.push_back(t);
    }
    {
        NodeType t(78); t.name = "xnor"; t.address = "math/logic/binary ops/"; t.tooltip = "NOT XOR component-wise (equality).";
        t.inputs = { InputFeatures("x", InputType::boolean, 0, false), InputFeatures("y", InputType::boolean, 0, false) };
        t.getOutputSize = outputSizeComponentWise; t.buildUI = [](NodeComponent&, NodeData&) {}; t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& in, std::span<ddtype> out, const RunnerInput&) {
            const auto& a = in[0], & b = in[1]; const int n = std::max((int)a.size(), (int)b.size());
            for (int i = 0; i < n; ++i) {
                const bool av = (i < (int)a.size() ? truthy(a[i]) : false);
                const bool bv = (i < (int)b.size() ? truthy(b[i]) : false);
                out[i].i = (av == bv) ? 1 : 0;
            }
            };
        t.outputType = InputType::boolean; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr; registry.push_back(t);
    }

    // ========= where (vectorized branch) =========
    {
        NodeType t(79);
        t.name = "where"; t.address = "math/logic/choice/"; t.tooltip = "Vectorized branch: out[i] = cond[i]? a[i] : b[i]. If cond is padded, defaults to picking a.";
        t.inputs = {
            InputFeatures("cond", InputType::boolean, 0, false),
            InputFeatures("a", InputType::any,0, false),
            InputFeatures("b", InputType::any,0, false)
        };
        t.getOutputSize = outputSizeComponentWise;
        t.buildUI = [](NodeComponent&, NodeData&) {}; t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& in, std::span<ddtype> out, const RunnerInput&) {
            const auto& c = in[0]; const auto& a = in[1]; const auto& b = in[2];
            const int n = std::max((int)a.size(), (int)b.size());
            for (int i = 0; i < n; ++i) {
                const bool cv = (i < (int)c.size()) ? c[i].i != 0 : true; // default to 'a'
                const ddtype av = (i < (int)a.size()) ? a[i] : 0.0;
                const ddtype bv = (i < (int)b.size()) ? b[i] : 0.0;
                out[i] = cv ? av : bv;
            }
            };
        t.whichInputToFollowWildcard = 1;
        t.outputType = InputType::followsInput; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr; registry.push_back(t);
    }

    // ========= Comparators: gt/ge/lt/le/eq =========

    makeCmp< [](double a, double b) {return a > b; }>(80,"greater", "x > y (component-wise; padding uses 0).", registry);
    makeCmp< [](double a, double b) {return a < b; } >(81,"less", "x < y (component-wise; padding uses 0).", registry);
    makeCmp< [](double a, double b) {return a >= b; } >(82,"greater eq", "x >= y (component-wise; padding uses 0).", registry);
    makeCmp< [](double a, double b) {return a <= b; } >(83,"less eq", "x <= y (component-wise; padding uses 0).", registry);
    makeCmp< [](double a, double b) {return a == b; } >(84,"equal", "x == y (component-wise; padding uses 0).", registry);

    //{
    //    NodeType t(84); t.name = "equal"; t.address = "math/logic/compare/"; t.tooltip = "x == y (component-wise; padding uses 0).";
    //    t.inputs = { InputFeatures("x", false, 0, false), InputFeatures("y", false, 0, false) };
    //    t.getOutputSize = outputSizeComponentWise;
    //
    //}

    // ========= is between / is not between (inclusive) =========
    {
        NodeType t(85); t.name = "is between"; t.address = "math/logic/compare/"; t.tooltip = "Inclusive: lo <= x <= hi. lo, hi are scalars.";
        t.inputs = { InputFeatures("x", InputType::decimal,0,false), InputFeatures("lo", InputType::decimal,1,false), InputFeatures("hi", InputType::decimal,1,false) };
        t.getOutputSize = outputSizeEqualsSingleInputSize; t.buildUI = [](NodeComponent&, NodeData&) {}; t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& in, std::span<ddtype> out, const RunnerInput&) {
            const auto& x = in[0]; double lo = in[1].empty() ? 0.0 : in[1][0].d, hi = in[2].empty() ? 1.0 : in[2][0].d;
            if (lo > hi) std::swap(lo, hi);
            for (int i = 0; i < (int)x.size(); ++i) {
                out[i].i = (x[i].d>lo && x[i].d<hi) ? 1 : 0;
            }
        };
        t.outputType = InputType::boolean; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr; registry.push_back(t);
    }
    {
        NodeType t(86); t.name = "is not between"; t.address = "math/logic/compare/"; t.tooltip = "Complement of 'is between'.";
        t.inputs = { InputFeatures("x", InputType::decimal,0,false), InputFeatures("lo", InputType::decimal,1,false), InputFeatures("hi", InputType::decimal,1,false) };
        t.getOutputSize = outputSizeEqualsSingleInputSize; t.buildUI = [](NodeComponent&, NodeData&) {}; t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& in, std::span<ddtype> out, const RunnerInput&) {
            const auto& x = in[0]; double lo = in[1].empty() ? 0.0 : in[1][0].d, hi = in[2].empty() ? 1.0 : in[2][0].d;
            if (lo > hi) std::swap(lo, hi);
            for (int i = 0; i < (int)x.size(); ++i) {
                out[i].i = (x[i].d<lo || x[i].d>hi) ? 1 : 0;
            }
        };
        t.outputType = InputType::boolean; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr; registry.push_back(t);
    }

    // ========= Boolean reductions =========
    {
        NodeType t(87); t.name = "any true"; t.address = "math/vector/reductions/boolean/"; t.tooltip = "1 if any element is nonzero, else 0.";
        t.inputs = { InputFeatures("x", InputType::boolean, 0, false) }; t.getOutputSize = outputSize1Known;
        t.buildUI = [](NodeComponent&, NodeData&) {}; t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& in, std::span<ddtype> out, const RunnerInput&) {
            int64_t any = 0;
            for (ddtype v : in[0])
                if (v.i != 0) {
                    any = 1; break;
                }
            out[0].i = any;
            };
        t.outputType = InputType::boolean; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr; registry.push_back(t);
    }
    {
        NodeType t(88); t.name = "any false"; t.address = "math/vector/reductions/boolean/"; t.tooltip = "1 if any element is zero, else 0.";
        t.inputs = { InputFeatures("x", InputType::boolean, 0, false) }; t.getOutputSize = outputSize1Known;
        t.buildUI = [](NodeComponent&, NodeData&) {}; t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& in, std::span<ddtype> out, const RunnerInput&) {
            int64_t any = 0;
            for (ddtype v : in[0]) 
                if (v.i == 0) { 
                    any = 1; break; 
                } 
            out[0].i = any;
            };
        t.outputType = InputType::boolean; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr; registry.push_back(t);
    }
    {
        NodeType t(89); t.name = "all true"; t.address = "math/vector/reductions/boolean/"; t.tooltip = "1 if all elements are nonzero (empty -> 1).";
        t.inputs = { InputFeatures("x", InputType::boolean, 0, false) }; t.getOutputSize = outputSize1Known;
        t.buildUI = [](NodeComponent&, NodeData&) {}; t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& in, std::span<ddtype> out, const RunnerInput&) {
            int64_t all = 1; for (ddtype v : in[0]) if (v.i == 0) { all = 0; break; } out[0].i = all;
            };
        t.outputType = InputType::boolean; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr; registry.push_back(t);
    }
    {
        NodeType t(90); t.name = "all false"; t.address = "math/vector/reductions/boolean/"; t.tooltip = "1 if all elements are zero (empty -> 1).";
        t.inputs = { InputFeatures("x", InputType::boolean, 0, false) }; t.getOutputSize = outputSize1Known;
        t.buildUI = [](NodeComponent&, NodeData&) {}; t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& in, std::span<ddtype> out, const RunnerInput&) {
            int64_t all = 1; for (ddtype v : in[0]) if (v.i != 0) { all = 0; break; } out[0].i = all;
            };
        t.outputType = InputType::boolean; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr; registry.push_back(t);
    }
    {
        NodeType t(91); t.name = "count true"; t.address = "math/vector/reductions/boolean/"; t.tooltip = "Counts nonzero elements.";
        t.inputs = { InputFeatures("x", InputType::boolean, 0, false) }; t.getOutputSize = outputSize1Known;
        t.buildUI = [](NodeComponent&, NodeData&) {}; t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& in, std::span<ddtype> out, const RunnerInput&) {
            int64_t c = 0; for (ddtype v : in[0])
                if (v.i != 0) ++c;
            out[0].i = c;
        };
        t.outputType = InputType::decimal; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr; registry.push_back(t);
    }
    {
        NodeType t(92); t.name = "count false"; t.address = "math/vector/reductions/boolean/"; t.tooltip = "Counts zeros.";
        t.inputs = { InputFeatures("x", InputType::boolean, 0, false) }; t.getOutputSize = outputSize1Known;
        t.buildUI = [](NodeComponent&, NodeData&) {}; t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& in, std::span<ddtype> out, const RunnerInput&) {
            int64_t c = 0; for (ddtype v : in[0]) 
                if (v.i == 0) ++c; 
                out[0].i = c;
            };
        t.outputType = InputType::integer; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr; registry.push_back(t);
    }

    // ========= concat (a || b) =========
    {
        NodeType t(93);
        t.name = "concat"; t.address = "math/vector/ops/"; t.tooltip = "Concatenate a then b.";
        t.inputs = { InputFeatures("a", InputType::any,0,false), InputFeatures("b", InputType::any,0,false) };
        t.getOutputSize = [](const std::vector<NodeData*>& nodes, const std::vector<std::vector<ddtype>>& s, const RunnerInput& ri, int, const NodeData&) {
            int as = (nodes.size() > 0 && s[0].size());
            int bs = (nodes.size() > 1 && s[1].size());
            int tot = as + bs;
            return tot > 1 ? tot : 2;
        };
        t.buildUI = [](NodeComponent&, NodeData&) {}; t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& in, std::span<ddtype> out, const RunnerInput&) {
            const auto& a = in[0], & b = in[1]; 
            int idx = -1; 
            for (ddtype v : a) out[++idx] = v; 
            for (ddtype v : b) out[++idx] = v;
            };
		t.whichInputToFollowWildcard = 0;
        t.outputType = InputType::followsInput; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr; registry.push_back(t);
    }

    // ========= pad_to (truncate or pad with value) =========
    {
        NodeType t(94);
        t.name = "pad to"; t.address = "math/vector/ops/"; t.tooltip = "Resize to N: copy then pad with value.";
        t.inputs = {
            InputFeatures("x",     InputType::any,0,false),
            InputFeatures("sizeN", InputType::integer,1,true),
            InputFeatures("value", InputType::any,1,false)
        };
        t.inputs[1].defaultValue.i = 1;
        t.getOutputSize = [](const std::vector<NodeData*>&, const std::vector<std::vector<ddtype>>& s, const RunnerInput&, int, const NodeData&) {
            const int N = (s.size() > 1 && !s[1].empty()) ? std::max((int64_t)0, s[1][0].i) : 1;
            return N;
            };
        t.buildUI = [](NodeComponent&, NodeData&) {}; t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& in, std::span<ddtype> out, const RunnerInput&) {
            const auto& x = in[0]; 
            const int N = (int)out.size(); 
            const ddtype v = !in[2].empty() ? in[2][0] : 0.0;
            const int nCopy = std::min((int)x.size(), N);
            for (int i = 0; i < nCopy; ++i) out[i] = x[i];
            for (int i = nCopy; i < N; ++i) out[i] = v;
            };
        t.outputType = InputType::decimal; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr; registry.push_back(t);
    }

    // ========= append zeros =========
    {
        NodeType t(95);
        t.name = "append zeros"; t.address = "math/vector/ops/"; t.tooltip = "Append K zeros to x.";
        t.inputs = { InputFeatures("x", InputType::any,0,false), InputFeatures("K", InputType::integer,1,true) };
        t.getOutputSize = [](const std::vector<NodeData*>& nodes, const std::vector<std::vector<ddtype>>& s, const RunnerInput& ri, int, const NodeData&) {
            int xs = s[0].size();
            int K = (s.size() > 1 && !s[1].empty()) ? std::max((int64_t)0, s[1][0].i) : 0;
            return xs + K;
            };
        t.buildUI = [](NodeComponent&, NodeData&) {}; t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& in, std::span<ddtype> out, const RunnerInput&) {
            const auto& x = in[0]; const int xs = (int)x.size(); const int N = (int)out.size();
            for (int i = 0; i < xs && i < N; ++i) out[i] = x[i];
            for (int i = xs; i < N; ++i) out[i] = { 0.0 };
            };
		t.whichInputToFollowWildcard = 0;
        t.outputType = InputType::followsInput; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr; registry.push_back(t);
    }

    // ========= reverse =========
    {
        NodeType t(96);
        t.name = "reverse"; t.address = "math/vector/ops/"; t.tooltip = "Reverse order of elements.";
        t.inputs = { InputFeatures("x", InputType::any,0,false) }; t.getOutputSize = outputSizeEqualsSingleInputSize;
        t.buildUI = [](NodeComponent&, NodeData&) {}; t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& in, std::span<ddtype> out, const RunnerInput&) {
            const auto& x = in[0]; const int n = (int)x.size(); for (int i = 0; i < n; ++i) out[i] = x[n - 1 - i];
            };
        t.whichInputToFollowWildcard = 0;
        t.outputType = InputType::followsInput; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr; registry.push_back(t);
    }

    // ========= interleave (a0,b0,a1,b1,...) then remainder =========
    {
        NodeType t(97);
        t.name = "interleave"; t.address = "math/vector/ops/"; t.tooltip = "Alternate elements from a and b; append remainder.";
        t.inputs = { InputFeatures("a", InputType::any,0,false), InputFeatures("b", InputType::any,0,false) };
        t.getOutputSize = [](const std::vector<NodeData*>& nodes, const std::vector<std::vector<ddtype>>&s, const RunnerInput& ri, int, const NodeData&) {
            int as = s[0].size();
            int bs = s[1].size();
            return std::max(1,as + bs);
            };
        t.buildUI = [](NodeComponent&, NodeData&) {}; t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& in, std::span<ddtype> out, const RunnerInput&) {
            const auto& a = in[0]; const auto& b = in[1];
            int ia = 0, ib = 0, k = 0, na = (int)a.size(), nb = (int)b.size();
            while (ia < na && ib < nb) { out[k++] = a[ia++]; out[k++] = b[ib++]; }
            while (ia < na) out[k++] = a[ia++];
            while (ib < nb) out[k++] = b[ib++];
            };
        t.whichInputToFollowWildcard = 0;
        t.outputType = InputType::followsInput; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr; registry.push_back(t);
    }

    // ========= argmax / argmin =========
    {
        NodeType t(98); t.name = "argmax"; t.address = "math/vector/reductions/"; t.tooltip = "Index of first maximum (0 if empty).";
        t.inputs = { InputFeatures("x", InputType::decimal,0,false) }; t.getOutputSize = outputSize1Known;
        t.buildUI = [](NodeComponent&, NodeData&) {}; t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& in, std::span<ddtype> out, const RunnerInput&) {
            const auto& x = in[0]; if (x.empty()) { out[0].i = 0.0; return; }
            int64_t idx = 0; double best = x[0].d; for (int i = 1; i < (int)x.size(); ++i) 
                if (x[i].d > best) { 
                    best = x[i].d; 
                    idx = i; 
                } 
            out[0].i = idx;
        };
        t.outputType = InputType::integer; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr; registry.push_back(t);
    }
    

    // ========= size of =========
    {
        NodeType t(99); t.name = "size of"; t.address = "utility/size/"; t.tooltip = "Length of the input vector.";
        t.inputs = { InputFeatures("x", InputType::any,0,false) }; t.getOutputSize = outputSize1Known;
        t.buildUI = [](NodeComponent&, NodeData&) {}; t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& in, std::span<ddtype> out, const RunnerInput&) {
            out[0].i = in[0].size();
        };
        t.outputType = InputType::integer; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr; registry.push_back(t);
    }

    // ======== sliding window
    {
        NodeType t(100);
        t.name = "sliding window"; t.address = "audio/filters/"; t.tooltip = "Computes weighted average of the last n audio samples (n = weights.size - 1)";
		t.inputs = { InputFeatures("current sample", InputType::decimal, 1, false), InputFeatures("weights", InputType::decimal, 0, false)}; t.getOutputSize = outputSize1Known;
		t.buildUI = [](NodeComponent& nc, NodeData& nd) {};
		t.execute = [](const NodeData&, UserInput& u, const std::vector<std::span<ddtype>>& in, std::span<ddtype> out, const RunnerInput&) {
            out[0].d = in[1][0].d * in[0][0].d;
            for (int i = 1; i < in[1].size(); i += 1) {
				out[0].d += in[1][i].d * u.getHistoricalSample(i - 1);
            }
		};
        t.outputType = InputType::decimal; t.alwaysOutputsRuntimeData = true; t.fromScene = nullptr; registry.push_back(t);
    }

    // ======== sliding window
    {
        NodeType t(101);
		t.name = "FIR filter"; t.address = "audio/filters/"; t.tooltip = "Averages current input with more previous samples as filter strength increases.";
		t.inputs = { InputFeatures("current sample", InputType::decimal, 1, false), InputFeatures("filter strength", InputType::decimal, 1, false) }; t.getOutputSize = outputSize1Known;
		t.buildUI = [](NodeComponent& nc, NodeData& nd) {};
        t.execute = [](const NodeData&, UserInput& u, const std::vector<std::span<ddtype>>& in, std::span<ddtype> out, const RunnerInput&) {
            const double maxFilter = 60;
            double n = in[0].empty() ? 0.0 : in[1][0].d * maxFilter;
            n = std::max(1.0, std::min(n, maxFilter));
            double sum = in[0][0].d;
            for (int i = 0; i < n; i += 1) {
                double v = (1.0 * n - i >= 1) + (n - i) * (n - i < 1);
                sum += u.getHistoricalSample(i) * v;
            }
            out[0].d = sum / n;
        };
		t.outputType = InputType::decimal; t.alwaysOutputsRuntimeData = true; t.fromScene = nullptr; registry.push_back(t);
    }

    // ======== get old samples
    {
        NodeType t(102);
        t.name = "get past samples"; t.address = "time/"; t.tooltip = "For each input index, gets the sample that many frames ago";
        t.inputs = { InputFeatures("indices", InputType::integer, 0, false) }; t.getOutputSize = outputSizeEqualsSingleInputSize;
        t.buildUI = [](NodeComponent& nc, NodeData& nd) {};
        t.execute = [](const NodeData&, UserInput& u, const std::vector<std::span<ddtype>>& in, std::span<ddtype> out, const RunnerInput&) {
            for (int i = 0; i < in[0].size(); i += 1) {
                int64_t idx = in[0][i].i;
                out[i] = u.getHistoricalSample(idx);
            }
            };
        t.outputType = InputType::decimal; t.alwaysOutputsRuntimeData = true; t.fromScene = nullptr; registry.push_back(t);
    }

    // ========= set storeable value 
    {
        NodeType t(103);
        t.name = "set stored value"; t.address = "utility/"; t.tooltip = "Stores a value during execution that can be read next frame. outputs value";
        t.inputs = { InputFeatures("key", InputType::integer, 1, false), InputFeatures("value", InputType::decimal, 1, false) }; t.getOutputSize = outputSize1Known;
        t.buildUI = [](NodeComponent& nc, NodeData& nd) {};
        t.execute = [](const NodeData&, UserInput& u, const std::vector<std::span<ddtype>>& in, std::span<ddtype> out, const RunnerInput& r) {
            out[0].d = in[1][0].d;
            if (&r == u.runner) { 
                int key = in[0][0].i;
                if (u.storeableValues.contains(key)) {
                    u.storeableValues[key] = in[1][0].d;
                }
                else {
                    u.storeableValues.insert({ key, in[1][0].d });
                }
            }
        };
        t.outputType = InputType::decimal; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr; registry.push_back(t);
    }


    // ========= get storeable value 
    {
        NodeType t(104);
        t.name = "get stored value"; t.address = "utility/"; t.tooltip = "Outputs a specific stored value from the previous frame";
        t.inputs = { InputFeatures("key", InputType::integer, 1, false)}; t.getOutputSize = outputSize1Known;
        t.buildUI = [](NodeComponent& nc, NodeData& nd) {};
        t.execute = [](const NodeData&, UserInput& u, const std::vector<std::span<ddtype>>& in, std::span<ddtype> out, const RunnerInput&) {
            int key = in[0][0].i;
			out[0].d = u.storeableValues.contains(key) ? u.storeableValues.at(key) : 0.0;
        };
        t.outputType = InputType::decimal; t.alwaysOutputsRuntimeData = true; t.fromScene = nullptr; registry.push_back(t);
    }

    // ========= white noise
    {
        NodeType t(105);
        t.name = "white noise"; t.address = "math/noise/random/"; t.tooltip = "Generates n random values between a and b";
        t.inputs = { InputFeatures("a", InputType::decimal, 1, false), InputFeatures("b", InputType::decimal, 1, false), InputFeatures("n", InputType::integer, 1, true) };
        t.inputs[0].defaultValue.d = 0.0;
        t.inputs[1].defaultValue.d = 1.0;
        t.inputs[2].defaultValue.i = 1;
        t.buildUI = [](NodeComponent& nc, NodeData& nd) {}; t.getOutputSize = outputSizeFromInputScalar(2);
        t.execute = [](const NodeData&, UserInput& u, const std::vector<std::span<ddtype>>& in, std::span<ddtype> out, const RunnerInput&) {
            double a = in[0][0].d;
            double b = in[1][0].d;
            for (int i = 0; i < out.size(); i += 1) {
                out[i] = uniform_closed(a, b);
            }
        };
        t.outputType = InputType::decimal; t.alwaysOutputsRuntimeData = true; t.fromScene = nullptr; registry.push_back(t);
    }

    // ========= white noise
    {
        NodeType t(106);
        t.name = "bernoulli noise"; t.address = "math/noise/random/"; t.tooltip = "Generates (t/f) n times";
        t.inputs = { InputFeatures("n", InputType::integer, 1, true) };
        t.inputs[0].defaultValue.i = 1;
        t.buildUI = [](NodeComponent& nc, NodeData& nd) {}; t.getOutputSize = outputSizeFromInputScalar(0);
        t.execute = [](const NodeData&, UserInput& u, const std::vector<std::span<ddtype>>& in, std::span<ddtype> out, const RunnerInput&) {
            int64_t a = std::max((int64_t)1, (int64_t)std::abs(in[0][0].i));
            
            for (int i = 0; i < out.size(); i += 1) {
                out[i].i = coin_flip();
            }
        };
        t.outputType = InputType::boolean; t.alwaysOutputsRuntimeData = true; t.fromScene = nullptr; registry.push_back(t);
    }
    // ========= deterministic white noise
    {
        NodeType t(107);
        t.name = "deterministic white noise"; t.address = "math/noise/deterministic/"; t.tooltip = "Generates a uniformly distributed value between a and b for each number in \"hash keys\"";
        t.inputs = { InputFeatures("hash keys", InputType::decimal, 0, false), InputFeatures("a", InputType::decimal, 1, false), InputFeatures("b", InputType::decimal, 1, false)};
        t.inputs[2].defaultValue.d = 1.0;
        t.buildUI = [](NodeComponent& nc, NodeData& nd) {}; t.getOutputSize = outputSizeByInputPin(0);
        t.execute = [](const NodeData&, UserInput& u, const std::vector<std::span<ddtype>>& in, std::span<ddtype> out, const RunnerInput&) {
            double a = in[1][0].d;
            double b = in[2][0].d;
            if (a > b) { std::swap(a, b); }
            for (int i = 0; i < out.size(); i += 1) {
                out[i] = deterministic_uniform_closed(a, b, in[0][i].d);
            }
        };
        t.outputType = InputType::decimal; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr; registry.push_back(t);
    }


    {
        NodeType t(108); t.name = "argmin"; t.address = "math/vector/reductions/"; t.tooltip = "Index of first minimum (0 if empty).";
        t.inputs = { InputFeatures("x", InputType::decimal,0,false) }; t.getOutputSize = outputSize1Known;
        t.buildUI = [](NodeComponent&, NodeData&) {}; t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& in, std::span<ddtype> out, const RunnerInput&) {
            if (in[0].empty()) { out[0].i = 0; return; }
            int64_t idx = 0; 
            double best = in[0][0].d;
            for (int i = 1; i < (int)in[0].size(); ++i) {
                if (in[0][i].d < best) { 
                    best = in[0][i].d; idx = i;
                } 
                out[0].i = idx;
            }
        };
        t.outputType = InputType::decimal; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr; registry.push_back(t);
    }

    // ======== divisible by
    {
        NodeType t(109);
        t.name = "divisible by"; t.address = "math/utils/";
        t.tooltip = "Returns true if value is divisible by \"divisor\".";
        t.inputs = { InputFeatures("value", InputType::integer, 0, false), InputFeatures("divisor", InputType::integer, 1, false) };
        t.getOutputSize = outputSizeByInputPin(0);
        t.buildUI = [](NodeComponent&, NodeData&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& in, std::span<ddtype> out, const RunnerInput&) {
            for (int i = 0; i < out.size(); i += 1) {
                const int x = in[0][i].i;
                const int d = in[1][i].i;
                out[i].i = d != 0 && x % d == 0;
            }
            
        };
        t.outputType = InputType::boolean; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr; registry.push_back(t);
    }

    // ======== current sample number
    {
        NodeType t(110);
        t.name = "current sample number"; t.address = "audio/time/";
        t.tooltip = "Monotonic sample index for the running stream.";
        t.inputs = {}; t.getOutputSize = outputSize1Known;
        t.buildUI = [](NodeComponent&, NodeData&) {};
        t.execute = [](const NodeData&, UserInput& u, const std::vector<std::span<ddtype>>&, std::span<ddtype> out, const RunnerInput& r) {
            out[0].i = u.sampleInBlock + u.numFramesStartOfBlock;
        };
        t.outputType = InputType::integer; t.alwaysOutputsRuntimeData = true; t.fromScene = nullptr; registry.push_back(t);
    }

    // ======== sample rate
    {
        NodeType t(111);
        t.name = "sample rate"; t.address = "audio/time/";
        t.tooltip = "Outputs the current sample rate (Hz).";
        t.inputs = {}; t.getOutputSize = outputSize1Known;
        t.buildUI = [](NodeComponent&, NodeData&) {};
        t.execute = [](const NodeData&, UserInput& u, const std::vector<std::span<ddtype>>&, std::span<ddtype> out, const RunnerInput&) {
            // Expect a field like u.sampleRate (ddtype). Adjust if your API differs.
            out[0].d = u.sampleRate; // e.g., 44100.0 / 48000.0
            };
        t.outputType = InputType::decimal; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr; registry.push_back(t);
    }

    // ======== value noise (1D, direct mapping)
    {
        NodeType t(112);
        t.name = "value noise"; t.address = "math/noise/deterministic/";
        t.tooltip = "Maps v -> value_noise(v). Vector in, vector out (same size).";
        t.inputs = { InputFeatures("v", InputType::decimal, 0, false) };
        t.getOutputSize = outputSizeByInputPin(0);
        t.buildUI = [](NodeComponent&, NodeData&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& in, std::span<ddtype> out, const RunnerInput&) {
            for (int i = 0; i < (int)in[0].size(); ++i) out[i] = valueNoise(in[0][i].d);
            };
        t.outputType = InputType::decimal; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr; registry.push_back(t);
    }

    // ======== voronoi noise (1D, direct mapping)
    {
        NodeType t(113);
        t.name = "voronoi noise"; t.address = "math/noise/deterministic/";
        t.tooltip = "Maps v -> voronoi_noise(v). Vector in, vector out (same size).";
        t.inputs = { InputFeatures("v", InputType::decimal, 0, false) };
        t.getOutputSize = outputSizeByInputPin(0);
        t.buildUI = [](NodeComponent&, NodeData&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& in, std::span<ddtype> out, const RunnerInput&) {
            for (int i = 0; i < (int)in[0].size(); ++i) out[i] = voronoiNoise(in[0][i].d);
            };
        t.outputType = InputType::decimal; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr; registry.push_back(t);
    }

    // ======== perlin (simple mapping, no octaves) 
    {
        NodeType t(114);
        t.name = "perlin noise"; t.address = "math/noise/deterministic/";
        t.tooltip = "Maps v -> perlin_noise(v). Vector in, vector out (same size).";
        t.inputs = { InputFeatures("v", InputType::decimal, 0, false) };
        t.getOutputSize = outputSizeByInputPin(0);
        t.buildUI = [](NodeComponent&, NodeData&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& in, std::span<ddtype> out, const RunnerInput&) {
            int size = in[0].size();
            for (int i = 0; i < size; ++i) out[i] = perlinNoise(in[0][i].d, 6, 2.353, .5);
            };
        t.outputType = InputType::decimal; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr; registry.push_back(t);
    }

    // ======== perlin fBm (octaves / lacunarity / roughness)
    {
        NodeType t(115);
        t.name = "perlin fBm"; t.address = "math/noise/deterministic/";
        t.tooltip = "Fractal Brownian Motion over Perlin: octaves (1,12), lacunarity [1,8], roughness in [0,1].";
        t.inputs = {
            InputFeatures("v", InputType::decimal, 0, false),
            InputFeatures("octaves", InputType::integer, 1, true),
            InputFeatures("lacunarity", InputType::decimal, 1, true),
            InputFeatures("roughness", InputType::decimal, 1, true)
        };
        t.inputs[1].defaultValue.i = 5.0;     // octaves
        t.inputs[2].defaultValue.d = 2.0;     // lacunarity
        t.inputs[3].defaultValue.d = 0.5;     // roughness (gain)
        t.getOutputSize = outputSizeByInputPin(0);
        t.buildUI = [](NodeComponent&, NodeData&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& in, std::span<ddtype> out, const RunnerInput&) {
            int size = in[0].size();
            int oct = in[1][0].i;           oct = std::max(1, std::min(oct, 12));
            double lac = in[2][0].d;                         lac = std::max(1.0, std::min(lac, 8.0));
            double gain = in[3][0].d;                        gain = std::max(0.0, std::min(gain, 1.0));

            for (int i = 0; i < size; ++i) {
                out[i] = perlinNoise(in[0][i].d, oct, lac, gain);
            }
        };
        t.outputType = InputType::decimal; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr; registry.push_back(t);
    }

    // ======== ridged multifractal (Perlin-based)
    {
        NodeType t(116);
        t.name = "ridged multi"; t.address = "math/noise/deterministic/";
        t.tooltip = "Ridged multifractal using |1 - |noise|| with octaves, lacunarity, roughness (gain).";
        t.inputs = {
            InputFeatures("v", InputType::decimal, 0, false),
            InputFeatures("octaves", InputType::integer, 1, true),
            InputFeatures("lacunarity", InputType::decimal, 1, true),
            InputFeatures("roughness", InputType::decimal, 1, true)
        };
        t.inputs[1].defaultValue.i = 6;
        t.inputs[2].defaultValue.d = 2.0;
        t.inputs[3].defaultValue.d = 0.5;
        t.getOutputSize = outputSizeByInputPin(0);
        t.buildUI = [](NodeComponent&, NodeData&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& in, std::span<ddtype> out, const RunnerInput&) {
			int size = in[0].size();
            int64_t oct = in[1][0].i;         oct = std::max((int64_t)1L, std::min(oct, (int64_t)12L));
            double lac = in[2][0].d;                         lac = std::max(1.0, std::min(lac, 8.0));
            double gain = in[3][0].d;                        gain = std::max(0.0, std::min(gain, 1.0));
			
            for (int i = 0; i < size; ++i) {

                out[i] = ridgedMultiNoise(in[0][i].d, oct, lac);
            }
            };
        t.outputType = InputType::decimal; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr; registry.push_back(t);
    }


    // ========= set named value 
    {
        NodeType t(117);
        t.name = "set named value"; t.address = "utility/"; t.tooltip = "Stores a value during execution that can be read next frame. outputs value";
        t.inputs = { InputFeatures("value", InputType::decimal, 1, false)}; t.getOutputSize = outputSize1Known;
        t.buildUI = [](NodeComponent& comp, NodeData& node) {
            node.setProperty("name", "var");
            comp.inputGUIElements.push_back(std::make_unique<juce::TextEditor>("name"));
            auto back = dynamic_cast<juce::TextEditor*>(comp.inputGUIElements.back().get());
            back->setJustification(juce::Justification::horizontallyCentred);
            back->setText("var");
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
        t.execute = [](const NodeData& node, UserInput& u, const std::vector<std::span<ddtype>>& in, std::span<ddtype> out, const RunnerInput& r) {
            out[0].d = in[1][0].d;
            if (&r == u.runner) {
                auto key = node.getStringProperty("name");
                if (u.namedValues.contains(key)) {
                    u.namedValues[key] = in[1][0].d;
                }
                else {
                    u.namedValues.insert({ key, in[1][0].d });
                }
            }
            };
        t.outputType = InputType::decimal; t.alwaysOutputsRuntimeData = false; t.fromScene = nullptr; registry.push_back(t);
    }

    // ========= get named value 
    {
        NodeType t(118);
        t.name = "get named value"; t.address = "utility/"; t.tooltip = "Outputs a specific stored value from the previous frame";
        t.inputs = {}; t.getOutputSize = outputSize1Known;
        t.buildUI = [](NodeComponent& comp, NodeData& node) {
            node.setProperty("name", "var");
            comp.inputGUIElements.push_back(std::make_unique<juce::TextEditor>("name"));
            auto back = dynamic_cast<juce::TextEditor*>(comp.inputGUIElements.back().get());
            back->setJustification(juce::Justification::horizontallyCentred);
            back->setText("var");
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
        t.execute = [](const NodeData& node, UserInput& u, const std::vector<std::span<ddtype>>& in, std::span<ddtype> out, const RunnerInput&) {
            auto key = node.getStringProperty("name");
            out[0].d = u.namedValues.contains(key) ? u.namedValues.at(key) : 0.0;
        };
        t.outputType = InputType::decimal; t.alwaysOutputsRuntimeData = true; t.fromScene = nullptr; registry.push_back(t);
    }

    {
        // ========= decimal to bool =========
        NodeType t(119);
        t.name = "";
        t.address = "math/convert/";
        t.tooltip = "Converts decimal vectors to boolean vectors: ";
        t.inputs = { InputFeatures("x", InputType::decimal, 0, false) };
        t.getOutputSize = outputSizeEqualsSingleInputSize;
        t.buildUI = [](NodeComponent&, NodeData&) {};
        t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& inputs, std::span<ddtype> output, const RunnerInput& inlineInstance)
            {
                for (int i = 0; i < static_cast<int>(inputs[0].size()); ++i) output[i].i = inputs[0][i].d > 0.5;
            };
        t.outputType = InputType::boolean;
        t.alwaysOutputsRuntimeData = false;
        t.fromScene = nullptr;
        registry.push_back(t);
        keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("alt b").getTextDescription(), registry.size() - 1 });
        decToBoolType = &registry.back();
    }

    // ========= web audio =========
    {
        NodeType webAudioInputType(120);
        webAudioInputType.name = "web audio";
        webAudioInputType.address = "audio/media/";
        webAudioInputType.tooltip = "Reads a web-hosted audio file into a vector";
        webAudioInputType.inputs = {};
        webAudioInputType.isInputNode = false;

        // Output size is based on cached audio (or 1 if invalid)
        webAudioInputType.getOutputSize = [](const std::vector<NodeData*>&,
            const std::vector<std::vector<ddtype>>&,
            const RunnerInput&,
            int,
            const NodeData& node) -> int
            {
                return node.optionalStoredAudio.empty()
                    ? 1
                    : static_cast<int>(node.optionalStoredAudio.size());
            };

        // Execution: copy from cache, or emit a single 0
        webAudioInputType.execute = [](const NodeData& node,
            UserInput&,
            const std::vector<std::span<ddtype>>&,
            std::span<ddtype> out,
            const RunnerInput&)
            {
                if (node.optionalStoredAudio.empty())
                {
                    if (!out.empty())
                        out[0] = 0.0;
                    return;
                }

                const size_t n = std::min(out.size(), node.optionalStoredAudio.size());
                std::copy_n(node.optionalStoredAudio.begin(), n, out.begin());
            };

        webAudioInputType._nodeDataChanged = [](NodeComponent& comp) -> bool
            {
                auto& node = comp.getNodeData();

                if (!node.getProperties().contains("url"))
                {
                    node.optionalStoredAudio.clear();
                    return true; // scene should update (empty now)
                }

                const std::string url = node.getStringProperty("url");
                if (url.empty())
                {
                    node.optionalStoredAudio.clear();
                    return true;
                }

                // This runs synchronously (but on a background thread thanks to nodeDataChanged)
                if (!AudioReader::readWebAudio(url, node.optionalStoredAudio))
                    node.optionalStoredAudio.clear();

                // Indicate scene needs an update
                return true;
            };

        webAudioInputType.buildUI = [](NodeComponent& comp, NodeData& node)
            {
                comp.inputGUIElements.push_back(std::make_unique<juce::TextEditor>("urlBox"));
                auto* urlBox = dynamic_cast<juce::TextEditor*>(comp.inputGUIElements.back().get());
                urlBox->setText(node.getStringProperty("url")); // restore if re-open
                urlBox->setJustification(juce::Justification::centred);
                fillInnerBounds(comp, urlBox, 0.0, 0.5);
                urlBox->applyFontToAllText(urlBox->getFont().withHeight(14));
                comp.addAndMakeVisible(urlBox);

                comp.inputGUIElements.push_back(std::make_unique<juce::Label>("status", ""));
                auto* statusLabel = dynamic_cast<juce::Label*>(comp.inputGUIElements.back().get());
                statusLabel->setJustificationType(juce::Justification::centred);
                fillInnerBounds(comp, statusLabel, 0.5, 1.0);
                statusLabel->setInterceptsMouseClicks(false, false);
                comp.addAndMakeVisible(statusLabel);

                // Initial status update
                juce::String url = node.getStringProperty("url");
                statusLabel->setText(url.isEmpty() ? "No URL set" : url, juce::dontSendNotification);
                statusLabel->setColour(juce::Label::textColourId,
                    node.optionalStoredAudio.empty() ? juce::Colours::red : juce::Colours::green);

                // On text change → update property + trigger async load
                urlBox->onTextChange = [urlBox, &comp, &node]()
                    {
                        const juce::String url = urlBox->getText();
                        node.setProperty("url", url.toStdString());
                        comp.getType().nodeDataChanged(comp); // triggers async fetch/decode
                    };
            };

        webAudioInputType.onResized = [](NodeComponent& comp)
            {
                juce::TextEditor* urlBox = nullptr;
                juce::Label* statusLabel = nullptr;

                for (auto* c : comp.getChildren())
                {
                    if (!urlBox)      urlBox = dynamic_cast<juce::TextEditor*>(c);
                    if (!statusLabel) statusLabel = dynamic_cast<juce::Label*>(c);
                }

                const float scale = (float)std::pow(2.0, comp.getOwningScene()->logScale);
                const int padding = (int)(12.0f * scale);
                const int boxH = (int)(24.0f * scale);
                const int lblH = (int)(20.0f * scale);

                if (urlBox)
                    urlBox->setBounds(padding, padding,
                        comp.getWidth() - 2 * padding, boxH);

                if (statusLabel)
                    statusLabel->setBounds(padding, padding + boxH + (int)(6.0f * scale),
                        comp.getWidth() - 2 * padding, lblH);
            };

        webAudioInputType.outputType = InputType::decimal;
        webAudioInputType.alwaysOutputsRuntimeData = true; // outputs actual audio samples
        webAudioInputType.fromScene = nullptr;

        registry.push_back(webAudioInputType);
    }


    {
        // ========= decimal to int =========
        NodeType t(121);
        t.name = "";
        t.address = "math/convert/";
        t.tooltip = "Converts decimal vectors to integer vectors: ";
        t.inputs = { InputFeatures("x", InputType::decimal, 0, false) };
        t.getOutputSize = outputSizeEqualsSingleInputSize;
        t.buildUI = [](NodeComponent&, NodeData&) {};
        t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& inputs, std::span<ddtype> output, const RunnerInput& inlineInstance)
            {
                for (int i = 0; i < static_cast<int>(inputs[0].size()); ++i) output[i].i = static_cast<int64_t>(std::round(inputs[0][i].d));
            };
        t.outputType = InputType::integer;
        t.alwaysOutputsRuntimeData = false;
        t.fromScene = nullptr;
        registry.push_back(t);
        decToIntType = &registry.back();
    }

    {
        // ========= int to bool =========
        NodeType t(122);
        t.name = "";
        t.address = "math/convert/";
        t.tooltip = "Converts integer vectors to boolean vectors: ";
        t.inputs = { InputFeatures("x", InputType::decimal, 0, false) };
        t.getOutputSize = outputSizeEqualsSingleInputSize;
        t.buildUI = [](NodeComponent&, NodeData&) {};
        t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& inputs, std::span<ddtype> output, const RunnerInput& inlineInstance)
            {
                for (int i = 0; i < static_cast<int>(inputs[0].size()); ++i) output[i].i = inputs[0][i].i != 0;
            };
        t.outputType = InputType::boolean;
        t.alwaysOutputsRuntimeData = false;
        t.fromScene = nullptr;
        registry.push_back(t);
		intToBoolType = &registry.back();
    }

    {
        // ========= int to dec =========
        NodeType t(123);
        t.name = "";
        t.address = "math/convert/";
        t.tooltip = "Converts integer vectors to decimal vectors: ";
        t.inputs = { InputFeatures("x", InputType::decimal, 0, false) };
        t.getOutputSize = outputSizeEqualsSingleInputSize;
        t.buildUI = [](NodeComponent&, NodeData&) {};
        t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& inputs, std::span<ddtype> output, const RunnerInput& inlineInstance)
            {
                for (int i = 0; i < static_cast<int>(inputs[0].size()); ++i) output[i] = (double)inputs[0][i].i;
            };
        t.outputType = InputType::decimal;
        t.alwaysOutputsRuntimeData = false;
        t.fromScene = nullptr;
        registry.push_back(t);
        intToDecType = &registry.back();
    }

    {
        // ========= int to bool =========
        NodeType t(124);
        t.name = "";
        t.address = "math/convert/";
        t.tooltip = "Converts bool vectors to decimal vectors: ";
        t.inputs = { InputFeatures("x", InputType::decimal, 0, false) };
        t.getOutputSize = outputSizeEqualsSingleInputSize;
        t.buildUI = [](NodeComponent&, NodeData&) {};
        t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& inputs, std::span<ddtype> output, const RunnerInput& inlineInstance)
            {
                for (int i = 0; i < static_cast<int>(inputs[0].size()); ++i) output[i] = static_cast<double>(inputs[0][i].i);
            };
        t.outputType = InputType::decimal;
        t.alwaysOutputsRuntimeData = false;
        t.fromScene = nullptr;
        registry.push_back(t);
        boolToDecType = &registry.back();
    }

    {
        // ========= int to bool =========
        NodeType t(125);
        t.name = "";
        t.address = "math/convert/";
        t.tooltip = "Converts decimal vectors to boolean vectors: ";
        t.inputs = { InputFeatures("x", InputType::decimal, 0, false) };
        t.getOutputSize = outputSizeEqualsSingleInputSize;
        t.buildUI = [](NodeComponent&, NodeData&) {};
        t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& inputs, std::span<ddtype> output, const RunnerInput& inlineInstance)
            {
                for (int i = 0; i < static_cast<int>(inputs[0].size()); ++i) output[i].i = inputs[0][i].i;
            };
        t.outputType = InputType::integer;
        t.alwaysOutputsRuntimeData = false;
        t.fromScene = nullptr;
        registry.push_back(t);
        boolToIntType = &registry.back();
    }

    {
        NodeType t(126);
        t.name = "custom curve";
        t.address = "audio/";
        t.tooltip = "";
        t.inputs = { InputFeatures("x", InputType::decimal, 0, false )};
        t.getOutputSize = outputSizeByInputPin(0);
        t.buildUI = [](NodeComponent& comp, NodeData&) {
            comp.inputGUIElements.push_back(std::make_unique<VisualVectorCreator>(comp));
            auto* visBox = dynamic_cast<VisualVectorCreator*>(comp.inputGUIElements.back().get());
            fillInnerBounds(comp, visBox, 0.0, 1.0);
            comp.addAndMakeVisible(visBox);
        };
        t.onResized = [](NodeComponent& comp) {
            if (!comp.inputGUIElements.empty()) {
                auto* visBox = dynamic_cast<VisualVectorCreator*>(comp.inputGUIElements.back().get());
                fillInnerBounds(comp, visBox, 0.0, 1.0);
            }
        };
        t.execute = [](const NodeData& nd, UserInput&, const std::vector<std::span<ddtype>>& inputs,
            std::span<ddtype> output, const RunnerInput& inlineInstance)
            {
                const auto& uvs = inputs[0];
                const auto& vec = nd.optionalStoredAudio;
                int n = static_cast<int>(vec.size());
                if (n == 0) {
                    for (int i = 0; i < output.size(); i++) output[i] = 0.0;
                    return;
                }

                for (int i = 0; i < uvs.size(); i++) {
                    double uv = uvs[i].d;

                    // wrap uv into [0,1)
                    uv = uv - std::floor(uv);

                    // map into [0, n-1]
                    double scaledIdx = uv * (n - 1);

                    int lower = static_cast<int>(std::floor(scaledIdx));
                    int upper = static_cast<int>(std::ceil(scaledIdx));
                    double frac = scaledIdx - lower;

                    // wrap indices
                    lower = (lower % n + n) % n;
                    upper = (upper % n + n) % n;

                    double v0 = vec[lower].d;
                    double v1 = vec[upper].d;
                    output[i] = v0 + frac * (v1 - v0);
                }
            };
        t.outputType = InputType::decimal;
        t.alwaysOutputsRuntimeData = false;
        t.fromScene = nullptr;
        registry.push_back(t);
    }
    {
        NodeType vectorResampleType(127);
        vectorResampleType.name = "vector resample";
        vectorResampleType.address = "math/vector/ops/";
        vectorResampleType.tooltip = "Samples a vector at normalized coordinates [0..1], with interpolation and wrapping.";
        vectorResampleType.inputs = {
            InputFeatures("vector", InputType::decimal, 0, false),
            InputFeatures("uv", InputType::decimal, 0, false)
        };
        vectorResampleType.getOutputSize = outputSizeByInputPin(1);
        vectorResampleType.buildUI = [](NodeComponent&, NodeData&) {};
        vectorResampleType.onResized = [](NodeComponent&) {};
        vectorResampleType.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& inputs, std::span<ddtype> output, const RunnerInput& inlineInstance)
            {
                auto vec = inputs[0];
                auto uvs = inputs[1];

                int n = static_cast<int>(vec.size());
                if (n == 0) {
                    for (int i = 0; i < output.size(); i++) output[i] = 0.0;
                    return;
                }

                for (int i = 0; i < uvs.size(); i++) {
                    double uv = uvs[i].d;

                    // wrap uv into [0,1)
                    uv = uv - std::floor(uv);

                    // map into [0, n-1]
                    double scaledIdx = uv * (n - 1);

                    int lower = static_cast<int>(std::floor(scaledIdx));
                    int upper = static_cast<int>(std::ceil(scaledIdx));
                    double frac = scaledIdx - lower;

                    // wrap indices
                    lower = (lower % n + n) % n;
                    upper = (upper % n + n) % n;

                    double v0 = vec[lower].d;
                    double v1 = vec[upper].d;
                    output[i] = v0 + frac * (v1 - v0);
                }
            };
        vectorResampleType.outputType = InputType::decimal;
        vectorResampleType.alwaysOutputsRuntimeData = false;
        vectorResampleType.fromScene = nullptr;
        registry.push_back(vectorResampleType);
    }

    {
        NodeType t(128);
        t.name = "indices from bool mask";
        t.address = "math/vector/ops/";
        t.tooltip = "Outputs indices of true values; fills rest with -1.";
        t.inputs = {
            InputFeatures("mask", InputType::boolean, 0, false)
        };
        t.getOutputSize = outputSizeByInputPin(0); // output has same length as input
        t.buildUI = [](NodeComponent&, NodeData&) {};
        t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& in,
            std::span<ddtype> out, const RunnerInput&)
            {
                const auto& mask = in[0];
                int n = static_cast<int>(mask.size());

                int writePos = 0;
                for (int i = 0; i < n; i++) {
                    if (mask[i].i) {
                        if (writePos < n) {
                            out[writePos++].i = i;
                        }
                    }
                }
                // Fill remaining with -1
                for (; writePos < n; writePos++) {
                    out[writePos].i = -1;
                }
            };
        t.whichInputToFollowWildcard = 0;
        t.outputType = InputType::integer;
        t.alwaysOutputsRuntimeData = false;
        t.fromScene = nullptr;
        registry.push_back(t);
    }

    {
        NodeType t(129);
        t.name = "midi hz";
        t.address = "audio/midi/";
        t.tooltip = "Converts MIDI note numbers to frequencies in Hz.";
        t.inputs = {
            InputFeatures("midi", InputType::integer, 0, false)
        };
        t.getOutputSize = outputSizeByInputPin(0); // same length as input
        t.buildUI = [](NodeComponent&, NodeData&) {};
        t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput&, const std::vector<std::span<ddtype>>& in,
            std::span<ddtype> out, const RunnerInput&)
            {
                const auto& midi = in[0];
                int n = static_cast<int>(midi.size());

                for (int i = 0; i < n; i++) {
                    int m = midi[i].i;
                    double hz = 440.0 * std::pow(2.0, (m - 69) / 12.0);
                    out[i].d = hz;
                }
            };
        t.whichInputToFollowWildcard = 0;
        t.outputType = InputType::decimal;
        t.alwaysOutputsRuntimeData = false;
        t.fromScene = nullptr;
        registry.push_back(t);
    }

    // MIDI Controller value node
    {
        NodeType t(130);
        t.name = "midi cc value";
        t.address = "midi/";
        t.tooltip = "Reads MIDI controller (CC) values from UserInput, normalized 0..1.";
        t.inputs = {
            InputFeatures("ccIndex", InputType::integer, 0, false)
        };
        t.getOutputSize = outputSizeByInputPin(0); // same length as indices
        t.buildUI = [](NodeComponent&, NodeData&) {};
        t.onResized = [](NodeComponent&) {};
        t.execute = [](const NodeData&, UserInput& userInput,
            const std::vector<std::span<ddtype>>& in,
            std::span<ddtype> out, const RunnerInput&)
            {
                const auto& ccIdx = in[0];
                int n = static_cast<int>(ccIdx.size());

                for (int i = 0; i < n; ++i)
                {
                    int cc = ccIdx[i].i;
                    float val = 0.0f;
                    if (cc >= 0 && cc < (int)userInput.midiCCValues.size())
                        val = userInput.midiCCValues[cc];
                    out[i].d = val; // already normalized 0..1
                }
            };
        t.whichInputToFollowWildcard = 0;
        t.outputType = InputType::decimal;
        t.alwaysOutputsRuntimeData = true;
        t.fromScene = nullptr;
        registry.push_back(t);
    }

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





    for (const auto& type : registry) {
        NodeType::putIdLookup(type);
    }
}

NodeType& NodeType::getConversionType(InputType from, InputType to)
{
    if (from == InputType::decimal) {
		if (to == InputType::integer) {
			return *decToIntType;
		}
		else {
			return *decToBoolType;
		}
    }
    else if (from == InputType::boolean) {
		if (to == InputType::integer) {
			return *boolToIntType;
		}
		else {
			return *boolToDecType;
		}
    }
    else {
        if (to == InputType::decimal) {
			return *intToDecType;
        }
        else {
            return *intToBoolType;
        }
    }
}