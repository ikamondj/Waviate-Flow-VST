/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Animation.h"

const float hz = 60.f;
const float dt = 1.f / hz;

//==============================================================================
WaviateFlow2025AudioProcessorEditor::WaviateFlow2025AudioProcessorEditor(WaviateFlow2025AudioProcessor& p)
    : AudioProcessorEditor(&p),
    audioProcessor(p),
    resizerBar(&horizontalLayout, 1, true),
    browser("browser", nullptr),
    scenePropertiesComponent(p),
    sceneExplorerComponent(p),
    authPropertiesComponent(p),
    current(0.0),
    sideVel(0.0)
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
    addAndMakeVisible(activeSceneName);

    activeSceneName.setJustificationType(juce::Justification::topRight);

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
    horizontalLayout.setItemLayout(0, -0.0, -0.0, -0.0); // browser: 20–80% (pref ~30%)
    horizontalLayout.setItemLayout(1, 0, 0, 0);  // resizer bar: fixed 8 px
    horizontalLayout.setItemLayout(2, -1.0, -1.0, -1.0); // canvas: 20–80% (pref ~70%)

    for (auto& scene : audioProcessor.scenes) {
        canvas.addAndMakeVisible(scene.get());
    }

    propertiesMenus.reserve(12);
    
    propertiesMenus.push_back(&scenePropertiesComponent);
    propertiesMenus.push_back(&nodePropertiesComponent);
    propertiesMenus.push_back(&sceneExplorerComponent);
    propertiesMenus.push_back(&authPropertiesComponent);

    for (auto* p : propertiesMenus) {
        if (p) {
            addAndMakeVisible(p);
            addAndMakeVisible(p->openIconBTN);
            p->openIconBTN.onClick = [this, p]() {
                if (activePropertyMenu != -1 && propertiesMenus[activePropertyMenu] == p) {
                    this->setActivePropertyMenu(-1);
                }
                else {
                    this->setActivePropertyMenu(p);
                }
            };
        }
    }
    
    

    setSize(800, 600);
    startTimerHz(60);

    p.displaySceneName();
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
    auto propMenu = lastPropMenu;
    juce::Component* comps[] = { propMenu, &resizerBar, &canvas};
    horizontalLayout.layOutComponents(comps, 3,
        0, 0, getWidth(), getHeight(),
        false, true);
    int iconButtonOffsetX = propMenu->getWidth() + 30;
    int y = 30;
    for (auto* p : propertiesMenus) {
        if (p) {
            if (p != propMenu) {
                p->setSize(0, getHeight());
                p->setTopLeftPosition(-1, -1);
            }
            p->openIconBTN.setSize(40, 40);
            p->openIconBTN.setTopLeftPosition(iconButtonOffsetX, y);
            y += 50;
        }
    }
    activeSceneName.setSize(getWidth()  / 8.0, 20);
    activeSceneName.setTopLeftPosition(getWidth() - getWidth() / 8.0, 0);
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
		if (auto* scene = audioProcessor.getActiveScene())
		{
			if (scene->getHighlightedNode() && scene->getHighlightedNode()->getType().name.toLowerCase() != "output")
			{
				scene->deleteNode(scene->getHighlightedNode());
                scene->setHighlightedNode(nullptr);
				repaint();
			}
		}
		return true; // handled
	}
    else if (k.isKeyCode(juce::KeyPress::escapeKey)) {
        if (auto* scene = audioProcessor.getActiveScene())
        {
            scene->setHighlightedNode(nullptr);
            repaint();
        }
        return true; // handled
    }
    else if (audioProcessor.keyCodeTypeMapping.contains(k.getTextDescription())) {
        if (audioProcessor.getActiveScene()) {
            audioProcessor.addNodeOfType(audioProcessor.keyCodeTypeMapping[k.getTextDescription()], juce::Point<int>(300, 300) - audioProcessor.getActiveScene()->getPosition(), audioProcessor.getActiveScene());
            return true;
        }
    }
    return false;
}



void WaviateFlow2025AudioProcessorEditor::buttonClicked(juce::Button*)
{

}










void WaviateFlow2025AudioProcessorEditor::setActivePropertyMenu(int index)
{
    activePropertyMenu = index;
    for (int i = 0; i < propertiesMenus.size(); i += 1) {
        auto* menu = propertiesMenus[i];
        if (menu) { 
            if (index == i) {
                menu->setButtonIcon(true);
                lastPropMenu = menu;
                lastPropMenu->toFront(false);
            }
            else {
                menu->setButtonIcon(false);
                menu->toBack();
            }
            menu->openIconBTN.repaint();
        }
    }
    resized();
}

void WaviateFlow2025AudioProcessorEditor::setActivePropertyMenu(const PropertiesMenu* propMenu)
{
    for (int i = 0; i < propertiesMenus.size(); i += 1) {
        PropertiesMenu* p = propertiesMenus[i];
        if (p == propMenu) {
            return setActivePropertyMenu(i);
        }
    }
    setActivePropertyMenu(-1);
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
    float target = activePropertyMenu != -1 ? 1.001f : -0.001f;
    
    current = smoothDamp(current, target, sideVel, .15f, dt);
    if (current >= 1.f) { 
        resizerBar.setEnabled(true);
        current = 1.0; 
        activePreferredValue = (float)resizerBar.getPosition().x;
    }
    else if (current <= 0.f) { 
        current = 0.0; 
        resizerBar.setEnabled(false);
    }
    else {
        if (current > 0.99f) {
            current = 1.0f;
            sideVel = 0.0;
        }
        else if (current < 0.01f) {
            current = 0.0f;
            sideVel = 0.0;
        }
        horizontalLayout.setItemLayout(0, 200 * current, 320 * current, activePreferredValue); // browser: 20–80% (pref ~30%)
        float fp = 8 * current;
        horizontalLayout.setItemLayout(1, fp, fp, fp);  // resizer bar: fixed 8 px
        horizontalLayout.setItemLayout(2, -0.001, -1.0, -1.0); // canvas: 20–80% (pref ~70%)
        resizerBar.setEnabled(false);
    }


    
    resized();
}

juce::ThreadPool& WaviateFlow2025AudioProcessorEditor::getThreadPool()
{
    return this->pool;
}

void WaviateFlow2025AudioProcessorEditor::setActiveScene(SceneComponent* scene, bool shouldOpen)
{
    scenePropertiesComponent.setActiveScene(scene);
    if (shouldOpen) {
        setActivePropertyMenu(&scenePropertiesComponent);
    }
}

void WaviateFlow2025AudioProcessorEditor::setActiveNode(NodeComponent* n, bool shouldOpen)
{
    nodePropertiesComponent.setNodeComponent(n);
    if (shouldOpen) {
        setActivePropertyMenu(&nodePropertiesComponent);
    }
}

