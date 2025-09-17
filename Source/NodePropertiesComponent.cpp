#include "NodePropertiesComponent.h"
#include "NodeComponent.h"

NodePropertiesComponent::NodePropertiesComponent()
{
    // Initialize property editors here (e.g., addAndMakeVisible(slider), etc.)
}

NodePropertiesComponent::~NodePropertiesComponent() {}

void NodePropertiesComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::darkgrey);
    // Optionally draw property panel title, etc.
}

void NodePropertiesComponent::resized()
{
    // Layout property editors here
}

void NodePropertiesComponent::setNodeComponent(NodeComponent* node)
{
    if (attachedNode != node)
    {
        attachedNode = node;
        updatePropertiesUI();
    }
}

NodeComponent* NodePropertiesComponent::getNodeComponent() const
{
    return attachedNode;
}

void NodePropertiesComponent::updatePropertiesUI()
{
    // Clear and rebuild property editors based on attachedNode's type and properties
    // For example, show/hide controls for compile-time, min/max, etc.

    repaint();
}