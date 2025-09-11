/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "NodeType.h"
#include "NodeComponent.h"
#include "SceneComponent.h"
#include "ScenePropertiesComponent.h"
#include "NodeData.h"
#include "BrowserModel.h"
#include "RunnerInput.h"


class WaviateFlow2025AudioProcessorEditor : public juce::AudioProcessorEditor,
    public juce::TabBarButton::Listener, private juce::Timer
{
public:
    WaviateFlow2025AudioProcessorEditor(WaviateFlow2025AudioProcessor&);
    ~WaviateFlow2025AudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    int getNodeTypeId(const juce::String& name) const;
    juce::TextButton addButton{ "+" };
    juce::TextButton removeButton{ "x" };
    juce::TextEditor nameSceneBox;
	juce::Label addSceneBoxPlaceHolder{ {}, "Enter scene name..." };
    juce::ListBox browser;
    juce::Component browserArea;
    juce::Component browserBottomArea;
    std::unique_ptr<BrowserModel> browserModel;
    juce::Component canvas;
    juce::StretchableLayoutManager horizontalLayout;
    juce::StretchableLayoutResizerBar resizerBar;
    ScenePropertiesComponent scenePropertiesComponent;
    
    void deselectBrowserItem();
    void timerCallback() override;
    WaviateFlow2025AudioProcessor& audioProcessor;
private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaviateFlow2025AudioProcessorEditor)

public:
    
    // Inherited via Listener
    bool keyPressed(const juce::KeyPress& k) override;
    void buttonClicked(juce::Button*) override;
};