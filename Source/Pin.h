/*
  ==============================================================================

    Pin.h
    Created: 11 Aug 2025 9:22:42pm
    Author:  ikamo

  ==============================================================================
*/
#include <JuceHeader.h>
#pragma once
class Pin : public juce::Component
{
public:
    Pin(int inputNum, class NodeComponent* attachedComponent);
    int getInputNumber() const;
    NodeComponent* getAttachedComponent() const;
	void setInputNumber(int newInputNumber);
	void setAttachedComponent(class NodeComponent* newAttachedComponent);
	void paint(juce::Graphics& g) override;
	void mouseDown(const juce::MouseEvent& e) override;
	void mouseDrag(const juce::MouseEvent& e) override;
    juce::Point<float> getCentreInSceneCoords() const;
    void mouseUp(const juce::MouseEvent& e) override;
	bool getIsDragging() const;
    juce::Point<float> getDragCurrentScreenPos() const;
    juce::Colour outlineColorFromType() const;

    juce::Colour pinColorFromType() const;
    void reposition();
    void updateSize();
private:
    bool isDragging = false;
    juce::Point<float> dragStartPos;
	juce::Point<float> dragCurrentPos;
    int inputNumber; //-1 for output
	NodeComponent* owningComponent;
};