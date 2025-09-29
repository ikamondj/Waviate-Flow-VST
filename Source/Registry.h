/*
  ==============================================================================

    Registry.h
    Created: 25 Sep 2025 10:27:54pm
    Author:  ikamo

  ==============================================================================
*/

#pragma once
#include <math.h>
#include <cmath>
#include "PluginEditor.h"
#include "NodeType.h"
#include "RunnerInput.h"
#include "AudioReader.h"
#include "Noise.h"
#include "ddtype.h"
#include "VisualVectorCreator.h"
#include "PluginProcessor.h"
#include "NodeComponent.h"
#include "SceneComponent.h"
#define MAX_UNKNOWN_SIZE 1024

inline bool truthy(ddtype d) { return d.i != 0; }

//get output size general functions
inline int outputSize1Known(const std::vector<NodeData*>& inputs, const std::vector<std::vector<ddtype>>&, const RunnerInput&, int, const NodeData&) {
    return 1;
}

inline int outputSizeAllMidi(const std::vector<NodeData*>& inputs, const std::vector<std::vector<ddtype>>&, const RunnerInput&, int, const NodeData&) {
    return 128;
}

inline int outputSizeComponentWise(const std::vector<NodeData*>& inputs, const std::vector<std::vector<ddtype>>& s, const RunnerInput& r, int, const NodeData&) {
    int size = 1;
    for (const auto& input : s) {
        if (input.size() > size) {
            size = input.size();
        }
    }

    return size;
}

inline int outputSizeComponentWiseTrunc(const std::vector<NodeData*>& inputs, const std::vector<std::vector<ddtype>>& s, const RunnerInput& r, int, const NodeData&) {
    if (s.empty()) { return 1; }
    int size = s[0].size();
    for (const auto& input : s) {
        if (input.size() < size && input.size() >= 1) {
            size = input.size();
        }
    }
    return size;
}

inline void fillInnerBounds(NodeComponent& n, juce::Component* c, double leftRatio, double rightRatio) {
    float scale = (float)std::pow(2.0, n.getOwningScene()->logScale);
    const float sides = 20.0f * scale;
    float cornerSize = 24.0f * scale;
    double l = std::lerp(sides, n.getWidth() - sides, leftRatio);
    n.inputGUIElements.back()->setBounds(l, cornerSize, rightRatio * (n.getWidth() - sides) - l, n.getHeight() - cornerSize * 2);
}

inline std::function<int(const std::vector<NodeData*>&, const std::vector<std::vector<ddtype>>& s, const RunnerInput& r, int, const NodeData&)> outputSizeFromInputScalar(int whichInput) {
    return [whichInput](const std::vector<NodeData*>&, const std::vector<std::vector<ddtype>>& s, const RunnerInput&, int, const NodeData&)
        {
            return std::max(1, static_cast<int>(std::round(s[whichInput][0].i)));
        };
}

inline std::function<int(const std::vector<NodeData*>&, const std::vector<std::vector<ddtype>>& s, const RunnerInput& r, int, const NodeData&)> outputSizeByNamedProperty(const std::string& name) {
    return [name](const std::vector<NodeData*>&, const std::vector<std::vector<ddtype>>& s, const RunnerInput& r, int, const NodeData& nd) {
        if (!nd.getNumericProperties().contains(name)) { return 1; }
        return (int)nd.getNumericProperty(name);
        };
}

inline double wrapTo(double v, double lo, double hi) {
    const double range = hi - lo;
    if (!(range > 0.0)) return lo;            // degenerate -> lo
    double w = std::fmod(v - lo, range);
    if (w < 0.0) w += range;                  // match zigzag's negative handling
    return lo + w;                            // in [lo, hi)
}

inline int64_t truthy(int64_t v) { return v != 0; }

using Comparator = bool (*)(double, double);
template <Comparator cmp>
inline void cmpExec(const NodeData&,
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
inline void makeCmp(uint16_t id, const char* nm, const char* tip, std::vector<NodeType>& registry)
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


inline int outputSizeEqualsSingleInputSize(const std::vector<NodeData*>& inputs, const std::vector<std::vector<ddtype>>& s, const RunnerInput& r, int, const NodeData&) {
    if (inputs.empty()) {
        return 1;
    }
    return s[0].size();
}



inline int outputSizeOuterProduct(const std::vector<NodeData*>& inputs, const std::vector<std::vector<ddtype>>& s, const RunnerInput& r, int, const NodeData&) {
    int size = 1;
    bool getsPadded = false;
    for (const auto& input : s) {
        size *= input.size();
    }
    return size;
}


inline int outputSizeLerpStyle(const std::vector<NodeData*>& inputs, const std::vector<std::vector<ddtype>>& s, const RunnerInput& c, int x, const NodeData& self) {
    std::vector<NodeData*> justTheFirstTwo;
    if (inputs.size() > 2) {
        justTheFirstTwo.push_back(inputs[0]);
        justTheFirstTwo.push_back(inputs[1]);
    }
    else {
        throw std::exception("Lerp style output size requires 2 lerp inputs and a interpolator");
    }
    return outputSizeComponentWise(justTheFirstTwo, s, c, x, self);
}

inline std::function<int(const std::vector<NodeData*>&, const std::vector<std::vector<ddtype>>& s, const RunnerInput& r, int, const NodeData&)> outputSizeByInputPin(int pinIndex) {
    return [pinIndex](const std::vector<NodeData*>& inputs, const std::vector<std::vector<ddtype>>& s, const RunnerInput& r, int, const NodeData&) {
        if (s.size() > pinIndex) {
            return (int)s[pinIndex].size();
        }
        return 1;
        };
}