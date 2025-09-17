

// ============================= VisualVectorCreator.cpp (revised) =============================
#include "VisualVectorCreator.h"
#include "NodeComponent.h"
#include "SceneComponent.h"
#include <type_traits>


static constexpr double kMinCapacity = 2;
static constexpr double kMaxCapacity = 8192;

VisualVectorCreator::VisualVectorCreator(NodeComponent& nc) : node(nc) {}
VisualVectorCreator::~VisualVectorCreator() {}

void VisualVectorCreator::ensureBufferSized()
{
    auto& buf = node.getNodeData().optionalStoredAudio;
    if ((int)buf.size() != capacity)
        resampleNodeBuffer(capacity);
}

void VisualVectorCreator::resampleNodeBuffer(int newCapacity)
{
    auto& buf = node.getNodeData().optionalStoredAudio;

    if (newCapacity <= 0)
        return;

    if (buf.empty()) {
        buf.resize(newCapacity);
        for (int i = 0; i < newCapacity; ++i) buf[i] = 0.0; // normalized default
        return;
    }

    const int oldN = (int)buf.size();
    if (oldN == newCapacity) return;

    using Elem = std::remove_reference_t<decltype(buf[0])>;
    std::vector<Elem> out(newCapacity);

    if (oldN == 1) {
        for (int i = 0; i < newCapacity; ++i) out[i] = buf[0];
    }
    else {
        // Linear interpolation in sample domain
        for (int i = 0; i < newCapacity; ++i) {
            const double pos = (double)i * (oldN - 1) / (double)(newCapacity - 1);
            const int    i0 = (int)std::floor(pos);
            const int    i1 = std::min(i0 + 1, oldN - 1);
            const double alpha = pos - i0;
            const double v0 = (double)buf[i0].d;
            const double v1 = (double)buf[i1].d;
            const double v = v0 + (v1 - v0) * alpha;
            out[i] = v;
        }
    }

    buf.clear();
    buf.resize(out.size());
    for (size_t i = 0; i < out.size(); ++i)
        buf[i] = out[i];
}

void VisualVectorCreator::setCapacity(double newCapacity)
{
    newCapacity = std::clamp(newCapacity, kMinCapacity, kMaxCapacity);
    if (newCapacity == capacity) return;

    capacity = newCapacity;
    resampleNodeBuffer(capacity);

    node.getNodeData().setProperty("size", capacity);

    if (auto* scene = node.getOwningScene())
        scene->onSceneChanged();

    repaint();
}

void VisualVectorCreator::paintPerPixelMinMax(juce::Graphics& g, int w, int h) const
{
    const auto& audio = node.getNodeDataConst().optionalStoredAudio;
    const int N = (int)audio.size();
    if (N <= 0 || w <= 0 || h <= 0) return;

    // One column per pixel; aggregate the corresponding sample window.
    for (int px = 0; px < w; ++px)
    {
        // Map pixel column -> fractional sample range [start, end)
        // Using half-open mapping ensures coverage without gaps.
        const double startF = (double)px * N / (double)w;
        const double endF = (double)(px + 1) * N / (double)w;
        int start = (int)std::floor(startF);
        int end = (int)std::ceil(endF);
        if (start < 0) start = 0;
        if (end > N) end = N;
        if (end <= start) end = std::min(start + 1, N);

        double minV = 1.0, maxV = 0.0;
        for (int s = start; s < end; ++s) {
            const double v = (double)audio[s].d; // normalized [0,1]
            if (v < minV) minV = v;
            if (v > maxV) maxV = v;
        }

        const float y1 = (1.0f - (float)maxV) * h;
        const float y2 = (1.0f - (float)minV) * h;
        const float colH = std::max(1.0f, y2 - y1); // at least 1px tall so peaks are visible
        g.fillRect((float)px, y1, 1.0f, colH);
    }
}

void VisualVectorCreator::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
    g.setColour(juce::Colours::aquamarine);

    // Ensure node buffer matches current capacity so UI reflects intended resolution.
    const auto& audio = node.getNodeDataConst().optionalStoredAudio;
    if ((int)audio.size() != capacity) {
        // We avoid mutating here; just show whatever exists to keep paint fast/stable.
        // Actual resize happens on interaction or setCapacity().
    }

    const int w = getWidth();
    const int h = getHeight();
    if (w <= 0 || h <= 0) return;


    paintPerPixelMinMax(g, w, h);

    g.drawText(juce::String((int)capacity), juce::Rectangle<float>(getWidth() / 2.0, 0, getWidth() / 2.0, getHeight() / 2.0), juce::Justification::topRight, false);
}

void VisualVectorCreator::resized() {}

void VisualVectorCreator::writeSampleLerped(int s0, double v0, int s1, double v1)
{
    auto& buf = node.getNodeData().optionalStoredAudio;
    if (buf.empty()) return;

    s0 = std::clamp(s0, 0, (int)buf.size() - 1);
    s1 = std::clamp(s1, 0, (int)buf.size() - 1);

    if (s0 == s1) {
        buf[s0] = std::clamp(v0, 0.0, 1.0);
        return;
    }

    if (s0 > s1) { std::swap(s0, s1); std::swap(v0, v1); }
    const int len = s1 - s0;
    for (int i = 0; i <= len; ++i) {
        const double t = (double)i / (double)len;
        const double v = v0 + (v1 - v0) * t;
        buf[s0 + i] = std::clamp(v, 0.0, 1.0);
    }
}

void VisualVectorCreator::mouseDrag(const juce::MouseEvent& e)
{
    const auto bounds = getLocalBounds();
    if (!bounds.contains(e.getPosition()))
        return;

    ensureBufferSized();

    auto& nd = node.getNodeData();
    const int w = std::max(1, getWidth());
    const int h = std::max(1, getHeight());

    // Map mouse to sample + value
    int sample = (int)std::floor((e.getPosition().x / (double)w) * nd.optionalStoredAudio.size());
    sample = std::clamp(sample, 0, (int)nd.optionalStoredAudio.size() - 1);

    double val = 1.0 - (e.getPosition().y / (double)h);
    val = std::clamp(val, 0.0, 1.0);

    bool changed = false;

    if (e.mods.isLeftButtonDown()) {
        if (hasLastDrag) {
            writeSampleLerped(lastSample, lastValue, sample, val); // fill gaps across fast motion
        }
        else {
            nd.optionalStoredAudio[sample] = val;
        }
        changed = true;
        hasLastDrag = true;
        lastSample = sample;
        lastValue = val;
    }
    else if (e.mods.isRightButtonDown()) {
        // Erase with interpolation if moving quickly
        if (hasLastDrag) {
            writeSampleLerped(lastSample, 0.0, sample, 0.0);
        }
        else {
            nd.optionalStoredAudio[sample] = 0.0;
        }
        changed = true;
        hasLastDrag = true;
        lastSample = sample;
        lastValue = 0.0;
    }

    if (changed) {
        if (auto* scene = node.getOwningScene())
            scene->onSceneChanged();
        repaint();
    }
}

void VisualVectorCreator::mouseUp(const juce::MouseEvent& e)
{
    juce::ignoreUnused(e);
    hasLastDrag = false;
    lastSample = -1;
}

void VisualVectorCreator::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (capacity <= 0) capacity = 1;

    double step = wheel.deltaY * 0.25;

    double logVal = std::log2((double)capacity);
    logVal += step;

    double newCapacity = std::pow(2.0, logVal);

    newCapacity = std::clamp(newCapacity, 2.0, 65536.0);

    setCapacity(newCapacity);
}