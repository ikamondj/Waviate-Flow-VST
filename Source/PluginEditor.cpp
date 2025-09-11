/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
WaviateFlow2025AudioProcessorEditor::WaviateFlow2025AudioProcessorEditor(WaviateFlow2025AudioProcessor& p)
    : AudioProcessorEditor(&p), 
    audioProcessor(p), 
    resizerBar(&horizontalLayout, 1, true), 
    browser("browser", nullptr)
{
    
    setResizable(true, true);

    browserModel = std::make_unique<BrowserModel>(*this);
    browser.setModel(browserModel.get());
    // Add the tab control
    addAndMakeVisible(canvas);
    addAndMakeVisible(resizerBar);
    addAndMakeVisible(browserArea);
    addAndMakeVisible(browser);
    addAndMakeVisible(addButton);
    addAndMakeVisible(nameSceneBox);
    addAndMakeVisible(removeButton);

    nameSceneBox.setJustification(juce::Justification::centred);
    nameSceneBox.setEnabled(false);
    nameSceneBox.setText("", false);
    nameSceneBox.onTextChange = [this]() {
        int row = browser.getSelectedRow(0);
        if (row < 0 || row >= audioProcessor.scenes.size()) { return; }
        juce::String start = nameSceneBox.getText().trim();
        juce::String name = start;
        bool nameFound = false;
        for (auto& scene : audioProcessor.scenes) {
            if (scene->getSceneName() == name) {
                nameFound = true;
            }
        }
        if (nameFound) {
            int i = 1;
            while (true) {
                nameFound = false;
                name = start + " " + juce::String(i);
                for (auto& scene : audioProcessor.scenes) {
                    if (scene->getSceneName() == name) {
                        nameFound = true;
                    }
                    
                }
                i += 1;
                if (!nameFound) { break; }
            }
            
        }
        audioProcessor.scenes[row]->setSceneName(name.toStdString());
        audioProcessor.scenes[row]->customNodeType.name = name;
    };

    addButton.onClick = [&]() {
        juce::String name = "New Function";
        int i = 1;

        bool nameFound = false;
        for (auto& scene : audioProcessor.scenes) {
            if (scene->getSceneName() == name) {
                nameFound = true;
            }
        }
        if (nameFound) {
            
            int i = 1;
            while (true) {
                nameFound = false;
                for (auto& scene : audioProcessor.scenes) {
                    name = "New Function " + juce::String(i);
                    if (scene->getSceneName() == name) {
                        nameFound = true;
                    }
                    
                }
                i += 1;
                if (!nameFound) {
                    break;
                }
            }
        }
        
        audioProcessor.addScene(name);
    };

    // Configure the layout manager for horizontal split
    horizontalLayout.setItemLayout(0, -0.2, -0.8, -0.3); // browser: 20–80% (pref ~30%)
    horizontalLayout.setItemLayout(1, 8, 8, 8);  // resizer bar: fixed 8 px
    horizontalLayout.setItemLayout(2, -0.2, -0.8, -0.7); // canvas: 20–80% (pref ~70%)

    for (auto& scene : audioProcessor.scenes) {
        canvas.addAndMakeVisible(scene.get());
    }
    
    
    
    setSize(800, 600);
    startTimerHz(60);
}

WaviateFlow2025AudioProcessorEditor::~WaviateFlow2025AudioProcessorEditor()
{
}

//==============================================================================
void WaviateFlow2025AudioProcessorEditor::paint(juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

    g.setColour(juce::Colours::white);
    g.drawFittedText("Hello World!", getLocalBounds(), juce::Justification::centred, 1);
    browser.repaint();
}




void WaviateFlow2025AudioProcessorEditor::resized()
{

    juce::Component* comps[] = { &browserArea, &resizerBar, &canvas };
    horizontalLayout.layOutComponents(comps, 3,
        0, 0, getWidth(), getHeight(),
        false, true);
    browser.setSize(browserArea.getWidth(), browserArea.getHeight() - 30);
    browser.setTopLeftPosition(0, 0);
    addButton.setSize(30, 30);
    addButton.setTopLeftPosition(0,browser.getHeight());
    removeButton.setSize(30, addButton.getHeight());
    nameSceneBox.setSize(browserArea.getWidth() - addButton.getWidth() - removeButton.getWidth(), addButton.getHeight());
    nameSceneBox.setTopLeftPosition(addButton.getWidth(), addButton.getPosition().y);
    removeButton.setTopLeftPosition(addButton.getWidth()+nameSceneBox.getWidth(), addButton.getPosition().y);
    browser.repaint();
}

int WaviateFlow2025AudioProcessorEditor::getNodeTypeId(const juce::String& name) const
{
    for (size_t i = 0; i < audioProcessor.registry.size(); ++i)
    {
        if (audioProcessor.registry[i].name == name)
            return static_cast<int>(i);
    }
	return -1; // not found
}



bool WaviateFlow2025AudioProcessorEditor::keyPressed(const juce::KeyPress& k)
{
    if (k.isKeyCode(juce::KeyPress::deleteKey)) {
		if (auto* scene = audioProcessor.activeScene)
		{
			if (scene->highlightedNode && scene->highlightedNode->getType().name.toLowerCase() != "output")
			{
				scene->deleteNode(scene->highlightedNode);
				scene->highlightedNode = nullptr;
				repaint();
			}
		}
		return true; // handled
	}
    else if (k.isKeyCode(juce::KeyPress::escapeKey)) {
        if (auto* scene = audioProcessor.activeScene)
        {
            scene->highlightedNode = nullptr;
            repaint();
        }
        return true; // handled
    }
    else if (audioProcessor.keyCodeTypeMapping.contains(k.getTextDescription())) {
        if (audioProcessor.activeScene) {
            audioProcessor.addNodeOfType(audioProcessor.keyCodeTypeMapping[k.getTextDescription()], juce::Point<int>(300, 300) - audioProcessor.activeScene->getPosition(), audioProcessor.activeScene);
            return true;
        }
    }
    return false;
}



void WaviateFlow2025AudioProcessorEditor::buttonClicked(juce::Button*)
{

}










void WaviateFlow2025AudioProcessorEditor::deselectBrowserItem()
{
    browser.deselectAllRows();
    nameSceneBox.setEnabled(false);
    nameSceneBox.setText("", false);
    removeButton.setEnabled(false);
}

void WaviateFlow2025AudioProcessorEditor::timerCallback()
{
    return;
    constexpr int tempSize = 256;
    float temp[tempSize];
    auto* scene = audioProcessor.getAudibleScene();
    if (scene) {
        juce::AudioVisualiserComponent* visualizer = dynamic_cast<juce::AudioVisualiserComponent*>(scene->nodes[0]->inputGUIElements.back().get());

        while (audioProcessor.fifo.getNumReady() > 0)
        {
            int start1, size1, start2, size2;
            int toRead = std::min(audioProcessor.fifo.getNumReady(), tempSize);

            audioProcessor.fifo.prepareToRead(toRead, start1, size1, start2, size2);

            if (size1 > 0) std::memcpy(temp, audioProcessor.ring.data() + start1, size1 * sizeof(float));
            if (size2 > 0) std::memcpy(temp + size1, audioProcessor.ring.data() + start2, size2 * sizeof(float));

            audioProcessor.fifo.finishedRead(size1 + size2);

            // Feed the chunk into JUCE's AudioVisualiserComponent
            juce::AudioBuffer<float> buffer(1, tempSize);
            buffer.addFrom(0, 0, temp, size1 + size2);
            visualizer->pushBuffer(buffer);
        }
    }

}

