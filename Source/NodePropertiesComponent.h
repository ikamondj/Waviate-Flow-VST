#pragma once

#include <JuceHeader.h>
#include "PropertiesMenu.h"
class NodeComponent;



class NodePropertiesComponent : public PropertiesMenu
{
public:
    NodePropertiesComponent();
    ~NodePropertiesComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    void setNodeComponent(NodeComponent* node);
    NodeComponent* getNodeComponent() const;

    void onUpdateUI() override;
    std::unordered_map<std::string, std::unique_ptr<juce::Component>> ownedComponents;
private:
    NodeComponent* attachedNode = nullptr;

    struct InputEditor
    {
        int index;
        std::unique_ptr<juce::Component> control;
        std::unique_ptr<juce::Label> label;
    };

    std::vector<InputEditor> inputEditors;

    // Special UI for input-type nodes
    std::unique_ptr<juce::Label> placeholderNameLabel;
    std::unique_ptr<juce::TextEditor> placeholderNameEditor;
    std::unique_ptr<juce::ToggleButton> compileTimeToggle;
    

    void rebuildEditors();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NodePropertiesComponent)
};

std::function<void(NodePropertiesComponent& npc)> setupBinaryArithmeticUI(class NodeType& componentWise, NodeType& outerProduct, NodeType& scalar);

std::function<void(NodePropertiesComponent& ncp)> setupInputTypeUI();
