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
