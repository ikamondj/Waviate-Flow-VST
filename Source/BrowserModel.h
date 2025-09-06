/*
  ==============================================================================

    BrowserModel.h
    Created: 18 Aug 2025 8:37:09pm
    Author:  ikamo

  ==============================================================================
*/
#include <JuceHeader.h>

#pragma once
class BrowserModel : public juce::ListBoxModel {
    // Inherited via ListBoxModel

    int getNumRows() override;
    void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;
    void listBoxItemClicked(int row, const juce::MouseEvent&) override;
    void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override;
    void backgroundClicked(const juce::MouseEvent&) override;
    class WaviateFlow2025AudioProcessorEditor& editorRef;
public:
    BrowserModel(WaviateFlow2025AudioProcessorEditor& editor);
};