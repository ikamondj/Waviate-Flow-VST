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

    // Set the node this properties panel is attached to
    void setNodeComponent(NodeComponent* node);
    // Get the current node
    NodeComponent* getNodeComponent() const;
    void onUpdateUI() override;
    

private:
    NodeComponent* attachedNode = nullptr;

    // Add property editors (e.g., sliders, combo boxes) as member variables here

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NodePropertiesComponent)
};