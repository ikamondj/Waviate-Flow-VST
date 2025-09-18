/*
  ==============================================================================

    BrowserModel.cpp
    Created: 18 Aug 2025 8:37:09pm
    Author:  ikamo

  ==============================================================================
*/

#include "BrowserModel.h"
#include "PluginEditor.h"

BrowserModel::BrowserModel(WaviateFlow2025AudioProcessorEditor& editor)
    : editorRef(editor)
{
}

int BrowserModel::getNumRows()
{
    return static_cast<int>(editorRef.audioProcessor.getScenes().size());
}

void BrowserModel::paintListBoxItem(int rowNumber, juce::Graphics& g,
    int width, int height, bool rowIsSelected)
{
    if (rowIsSelected)
        g.fillAll(juce::Colours::lightblue);
    else
        g.fillAll(juce::Colours::transparentBlack);

    if (rowNumber >= 0 && rowNumber < getNumRows())
    {
        auto* scene = editorRef.audioProcessor.getScenes()[rowNumber].get();
        if (scene == editorRef.audioProcessor.getActiveScene()) {
            g.setColour(juce::Colours::forestgreen);
        }
        else if (rowIsSelected)
            g.setColour(juce::Colours::darkslateblue);
        else
            g.setColour(juce::Colours::white);
        
        g.drawText(scene->getName(), 4, 0, width - 8, height,
            juce::Justification::centredLeft, true);
    }
}

void BrowserModel::listBoxItemClicked(int row, const juce::MouseEvent&)
{
    // Just select � ListBox handles selection highlighting automatically.
    if (auto* lb = dynamic_cast<juce::ListBox*>(&editorRef.browser))
        lb->selectRow(row);
    if (row >= 0 && row < getNumRows())
    {
        auto* scene = editorRef.audioProcessor.getScenes()[row].get();
        if (scene != nullptr)
        {
            editorRef.nameSceneBox.setText(scene->getName(), false);
            bool isMain = scene == &editorRef.audioProcessor.getScene();
            editorRef.nameSceneBox.setEnabled(!isMain);
            editorRef.removeButton.setEnabled(!isMain);
        }
        else {
            editorRef.nameSceneBox.setText("");
            editorRef.nameSceneBox.setEnabled(false);
            editorRef.removeButton.setEnabled(false);
        }
    }
}

void BrowserModel::listBoxItemDoubleClicked(int row, const juce::MouseEvent&e)
{
    listBoxItemClicked(row, e);
    if (row >= 0 && row < getNumRows())
    {
        auto* scene = editorRef.audioProcessor.getScenes()[row].get();
        if (scene)
        {
            editorRef.audioProcessor.setActiveScene(scene);
        }
    }
}

void BrowserModel::backgroundClicked(const juce::MouseEvent&)
{
    editorRef.deselectBrowserItem();
}
