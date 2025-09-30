/*
  ==============================================================================

    MathTypes.cpp
    Created: 25 Sep 2025 10:28:11pm
    Author:  ikamo

  ==============================================================================
*/

#include "Registry.h"

#define OP_ADD(a, b) ((a).d + (b).d)
#define OP_SUB(a, b) ((a).d - (b).d)
#define OP_MUL(a, b) ((a).d * (b).d)
#define OP_DIV(a, b) (( (b).d == 0.0 ) ? 0.0 : (a).d / (b).d)


#define EXEC_PAD(OP) \
    { \
        int a = (int)in[0].size(); \
        int b = (int)in[1].size(); \
        int n = std::min(a, b); \
        int m = std::max(a, b); \
        bool xIsBigger = a > b; \
        for (int i = 0; i < n; ++i) out[i].d = OP(in[0][i], in[1][i]); \
        if (xIsBigger) for (int i = n; i < m; ++i) out[i] = in[0][i]; \
        else           for (int i = n; i < m; ++i) out[i] = in[1][i]; \
    }

#define EXEC_TRUNC(OP) \
    { \
        int n = std::min((int)in[0].size(), (int)in[1].size()); \
        for (int i = 0; i < n; ++i) out[i].d = OP(in[0][i], in[1][i]); \
    }

#define EXEC_OUTER(OP) \
    { \
        int a = (int)in[0].size(); \
        int b = (int)in[1].size(); \
        int k = 0; \
        for (int i = 0; i < a; ++i) \
            for (int j = 0; j < b; ++j) \
                out[k++].d = OP(in[0][i], in[1][j]); \
    }

#define MAKE_BIN_EXEC(OP) [](const NodeData& nd, UserInput&, const std::vector<std::span<ddtype>>& in, std::span<ddtype> out, const RunnerInput& inlineInstance)\
{ \
    int mode = (int)nd.getNumericProperty("op_mode");\
    if (mode == 0) {\
        EXEC_PAD(OP); \
    } else if (mode == 1) {\
        EXEC_TRUNC(OP); \
    } else if (mode == 2) {\
        EXEC_OUTER(OP); \
    }\
};

inline bool WaviateFlow2025AudioProcessor::supportsDoublePrecisionProcessing() const { return true; }

void WaviateFlow2025AudioProcessor::initializeRegistryMath() {
    NodeType::registryCreatePrefix = 300000;

    std::function<void(class NodeComponent& nc, NodeData& nd)> binaryOpBuildUI = [](class NodeComponent& nc, NodeData& nd) {
        nc.inputGUIElements.push_back(std::make_unique<juce::ComboBox>());
        auto* combo = static_cast<juce::ComboBox*>(nc.inputGUIElements.back().get());
        const float scale = (float)std::pow(2.0, nc.getOwningScene()->logScale);
        const float sides = 20.0f * scale;
        const float cornerSize = 24.0f * scale;
        // populate with the broadcast modes
        combo->addItem("Pad", 1); // mode = 0
        combo->addItem("Truncate", 2); // mode = 1
        combo->addItem("Outer product", 3); // mode = 2

        // initialise selection from NodeData property (defaults to pad if missing)
        int mode = (int)nd.getNumericProperty("op_mode");
        combo->setSelectedId(mode + 1, juce::dontSendNotification);

        // hook change callback: update nd property when combo changes
        combo->onChange = [combo, &nd]()
        {
            int selectedMode = combo->getSelectedId() - 1; // back to 0,1,2
            nd.setProperty("op_mode", (double)selectedMode);
        };

        combo->setBounds(nc.getWidth() / 2.0, cornerSize, nc.getWidth() / 2.0 - sides, nc.getHeight() - cornerSize * 2);
        nc.addAndMakeVisible(combo);
    };

    // ========= add =========
    {
        NodeType addType(4);
        addType.name = "add";
        addType.address = "math/arithmetic/";
        addType.tooltip = "Component-wise addition; pads by copying the longer input.";
        addType.inputs = { InputFeatures("x", InputType::decimal, 0, false), InputFeatures("y", InputType::decimal, 0, false) };
        addType.getOutputSize = [](const std::vector<NodeData*>& inputs, const std::vector<std::vector<ddtype>>& s, const RunnerInput& r, int x, const NodeData& nd)
        {
                int mode = (int)nd.getNumericProperty("op_mode");
                switch (mode) {
                case 0: //componentwise pad
                    return outputSizeComponentWise(inputs, s, r, x, nd);
                case 1: //componentwise truncate
                    return outputSizeComponentWiseTrunc(inputs, s, r, x, nd);
                case 2: //outer product
                    return outputSizeOuterProduct(inputs, s, r, x, nd);
                }
                return outputSizeComponentWise(inputs, s, r, x, nd);
        };
        addType.buildUI = binaryOpBuildUI;
        addType.onResized = [](NodeComponent&) {};
        addType.execute = MAKE_BIN_EXEC(OP_ADD);
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
        subType.buildUI = binaryOpBuildUI;
        subType.onResized = [](NodeComponent&) {};
        subType.execute = MAKE_BIN_EXEC(OP_SUB);
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
        mulType.buildUI = binaryOpBuildUI;
        mulType.onResized = [](NodeComponent&) {};
        mulType.execute = MAKE_BIN_EXEC(OP_MUL);
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
    divType.buildUI = binaryOpBuildUI;
    divType.onResized = [](NodeComponent&) {};
    divType.execute = MAKE_BIN_EXEC(OP_DIV);
    divType.outputType = InputType::decimal;
    divType.alwaysOutputsRuntimeData = false;
    divType.fromScene = nullptr;
    registry.push_back(divType);
    keyCodeTypeMapping.insert({ juce::KeyPress::createFromDescription("/").getTextDescription(), registry.size() - 1 });

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
}