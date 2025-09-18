#include "NodePropertiesComponent.h"
#include "NodeComponent.h"
#include "NodeType.h"
#include "SceneComponent.h"
NodePropertiesComponent::NodePropertiesComponent()
{
    setButtonIcon(BinaryData::NodePropertiesEditorLogo_png,
        BinaryData::NodePropertiesEditorLogo_pngSize);
}

NodePropertiesComponent::~NodePropertiesComponent() {}

void NodePropertiesComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
    g.setColour(juce::Colours::white);
    g.setFont(16.0f);
    g.drawText("Node Properties", getLocalBounds().removeFromTop(24),
        juce::Justification::centred);
}

void NodePropertiesComponent::resized()
{
    auto area = getLocalBounds().reduced(8).removeFromTop(getHeight() - 16);

    int rowHeight = 28;

    // Layout special input-node controls
    if (attachedNode && attachedNode->getType().isInputNode)
    {
        if (placeholderNameLabel && placeholderNameEditor)
        {
            placeholderNameLabel->setBounds(area.removeFromTop(rowHeight));
            placeholderNameEditor->setBounds(area.removeFromTop(rowHeight));
            area.removeFromTop(8);
        }
        if (compileTimeToggle)
        {
            compileTimeToggle->setBounds(area.removeFromTop(rowHeight));
            area.removeFromTop(8);
        }
    }

    // Layout input editors
    for (auto& editor : inputEditors)
    {
        if (editor.label)
            editor.label->setBounds(area.removeFromTop(rowHeight));
        if (editor.control)
            editor.control->setBounds(area.removeFromTop(rowHeight));
        area.removeFromTop(8);
    }
}

void NodePropertiesComponent::setNodeComponent(NodeComponent* node)
{
    if (attachedNode != node)
    {
        attachedNode = node;
    }
    updateProperties();
}

NodeComponent* NodePropertiesComponent::getNodeComponent() const
{
    return attachedNode;
}

void NodePropertiesComponent::onUpdateUI()
{
    rebuildEditors();
    resized();
}

void NodePropertiesComponent::rebuildEditors()
{
    // Clear old controls
    inputEditors.clear();
    removeAllChildren();

    if (!attachedNode)
        return;

    // Input-node special fields
    if (attachedNode->getType().isInputNode)
    {
        placeholderNameLabel = std::make_unique<juce::Label>("", "Placeholder Name:");
        placeholderNameEditor = std::make_unique<juce::TextEditor>();
        compileTimeToggle = std::make_unique<juce::ToggleButton>("Force Compile-Time");

        addAndMakeVisible(*placeholderNameLabel);
        addAndMakeVisible(*placeholderNameEditor);
        addAndMakeVisible(*compileTimeToggle);

        placeholderNameEditor->onTextChange = [this] {
            if (attachedNode)
                attachedNode->getNodeData().setProperty("name",placeholderNameEditor->getText().toStdString());
            };
        compileTimeToggle->setEnabled(false);
        compileTimeToggle->onClick = [this] (){
            if (attachedNode)
            {
                //TODO force compile time
            }
        };
    }
    else
    {
        placeholderNameLabel.reset();
        placeholderNameEditor.reset();
        compileTimeToggle.reset();
    }

    // Per-input controls
    inputEditors.reserve(attachedNode->getType().inputs.size());

    for (size_t i = 0; i < attachedNode->getType().inputs.size(); ++i)
    {
        auto& inputDef = attachedNode->getType().inputs[i];
        auto type = inputDef.inputType;

        // Push slot
        inputEditors.push_back({ static_cast<int>(i), nullptr, nullptr });
        auto& slot = inputEditors.back();

        // Label
        slot.label = std::make_unique<juce::Label>("", juce::String(inputDef.name));
        slot.label->setColour(juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible(*slot.label);

        // Control
        if (type == InputType::decimal)
        {
            slot.control = std::make_unique<juce::TextEditor>();
            auto* editor = static_cast<juce::TextEditor*>(slot.control.get());

            editor->setText(juce::String(attachedNode->getNodeData().defaultValues[i].d));

            editor->onTextChange = [this, editor, i]() {
                if (!attachedNode) return;

                auto& node = attachedNode->getNodeData();
                auto txt = editor->getText().trim();

                try {
                    double newVal = std::stod(txt.toStdString());
                    if (std::isfinite(newVal))
                        node.defaultValues[i] = newVal;
                    else
                        editor->setText(juce::String(node.defaultValues[i].d), false);
                }
                catch (...) {
                    editor->setText(juce::String(node.defaultValues[i].d), false);
                }

                attachedNode->getOwningScene()->onSceneChanged();
                };
        }
        else if (type == InputType::integer)
        {
            slot.control = std::make_unique<juce::TextEditor>();
            auto* editor = static_cast<juce::TextEditor*>(slot.control.get());

            editor->setText(juce::String(attachedNode->getNodeData().defaultValues[i].i));

            editor->onTextChange = [this, editor, i]() {
                if (attachedNode)
                    attachedNode->getNodeData().defaultValues[i].i = editor->getText().getIntValue();
                    attachedNode->getOwningScene()->onSceneChanged();
                };
        }
        else if (type == InputType::boolean)
        {
            slot.control = std::make_unique<juce::ToggleButton>("Value");
            auto* toggle = static_cast<juce::ToggleButton*>(slot.control.get());

            toggle->setToggleState(attachedNode->getNodeData().defaultValues[i].i != 0,
                juce::dontSendNotification);

            toggle->onClick = [this, toggle, i]() {
                if (attachedNode)
                    attachedNode->getNodeData().defaultValues[i].i = toggle->getToggleState() ? 1 : 0;
                    attachedNode->getOwningScene()->onSceneChanged();
                };
        }

        if (slot.control)
            addAndMakeVisible(*slot.control);
    }

}