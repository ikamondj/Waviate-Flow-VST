// ============================= VisualVectorCreator.h (revised) =============================
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>


class NodeComponent; // fwd


class VisualVectorCreator : public juce::Component
{
public:
	explicit VisualVectorCreator(NodeComponent& nc);
	~VisualVectorCreator() override;


	void paint(juce::Graphics& g) override;
	void resized() override;


	// Editing
	void mouseDrag(const juce::MouseEvent& e) override;
	void mouseUp(const juce::MouseEvent& e) override;
	void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;


	// Resolution / capacity ("bars")
	void setCapacity(double newCapacity); // resamples existing data to new capacity
	int getCapacity() const { return capacity; }


private:
	NodeComponent& node;


	// Number of bars/samples shown + edited. This replaces the old #define storedsize.
	double capacity = 512; // [16..8192] sane bounds


	// Drag state to avoid gaps while moving quickly
	bool hasLastDrag = false;
	int lastSample = -1;
	double lastValue = 0.0; // normalized [0,1]


	// Helpers
	void ensureBufferSized(); // allocate/resize node buffer to capacity (no-op if already sized)
	void resampleNodeBuffer(int newCapacity); // linear resample node buffer to new size
	void paintPerPixelMinMax(juce::Graphics& g, int w, int h) const; // robust renderer
	void writeSampleLerped(int s0, double v0, int s1, double v1); // fill gaps during drag
};