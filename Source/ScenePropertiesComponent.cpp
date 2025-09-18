#include "ScenePropertiesComponent.h"
#include "SceneComponent.h"
#include "PluginProcessor.h"

ScenePropertiesComponent::ScenePropertiesComponent(WaviateFlow2025AudioProcessor& pcessor)
    : processor(pcessor)
{
    setButtonIcon(BinaryData::ScenePropertiesEditorLogo_png,
        BinaryData::ScenePropertiesEditorLogo_pngSize);

    configureLookAndFeel();

    // Add and configure children
    addAndMakeVisible(nameLabel);
    addAndMakeVisible(nameEditor);
    addAndMakeVisible(addressLabel);
    addAndMakeVisible(addressEditor);
    addAndMakeVisible(publishToMarketplaceButton);

    attachCallbacks();

    setActiveScene(pcessor.getActiveScene());
}

void ScenePropertiesComponent::configureLookAndFeel()
{
    auto accent = juce::Colours::mediumaquamarine.darker();
    auto background = juce::Colours::black;

    // Labels
    nameLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addressLabel.setColour(juce::Label::textColourId, juce::Colours::white);

    // Editors
    nameEditor.setColour(juce::TextEditor::backgroundColourId, background.brighter(0.1f));
    nameEditor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    nameEditor.setColour(juce::TextEditor::highlightColourId, accent.withAlpha(0.5f));
    nameEditor.setColour(juce::TextEditor::outlineColourId, accent);

    addressEditor.setColour(juce::TextEditor::backgroundColourId, background.brighter(0.1f));
    addressEditor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    addressEditor.setColour(juce::TextEditor::highlightColourId, accent.withAlpha(0.5f));
    addressEditor.setColour(juce::TextEditor::outlineColourId, accent);

    // Button
    publishToMarketplaceButton.setColour(juce::TextButton::buttonColourId, accent);
    publishToMarketplaceButton.setColour(juce::TextButton::textColourOnId, juce::Colours::black);
    publishToMarketplaceButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
}

void ScenePropertiesComponent::attachCallbacks()
{
    nameEditor.onTextChange = [this]
        {
            if (activeSceneData != nullptr)
                activeSceneData->setSceneName(nameEditor.getText().toStdString());
        };

    addressEditor.onTextChange = [this]
        {
            if (activeSceneData != nullptr)
                activeSceneData->customNodeType.address = addressEditor.getText().toStdString();
        };

    publishToMarketplaceButton.onClick = [this]
        {
            if (activeSceneData != nullptr)
            {
                // TODO: Hook into processor’s publish flow
                juce::AlertWindow::showMessageBoxAsync(
                    juce::AlertWindow::InfoIcon,
                    "Publish",
                    "Scene \"" + activeSceneData->getSceneName() + "\" would be published here."
                );
            }
        };
}

void ScenePropertiesComponent::setActiveScene(SceneComponent* scene)
{
    activeSceneData = scene;
    updateProperties();
}

void ScenePropertiesComponent::onUpdateUI()
{
    if (activeSceneData)
    {
        nameEditor.setText(activeSceneData->getSceneName(), juce::dontSendNotification);
        addressEditor.setText(activeSceneData->customNodeType.address, juce::dontSendNotification);

        nameEditor.setEnabled(true);
        addressEditor.setEnabled(true);

        bool authed = true; // Placeholder for actual authentication check
        bool hasReachedPublishLimit = false; // Placeholder for actual limit check
        bool canPublish = authed && !hasReachedPublishLimit;
        publishToMarketplaceButton.setEnabled(canPublish);
    }
    else
    {
        nameEditor.clear();
        addressEditor.clear();
        nameEditor.setEnabled(false);
        addressEditor.setEnabled(false);
        publishToMarketplaceButton.setEnabled(false);
    }
}

void ScenePropertiesComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black.withAlpha(0.95f));

    // Accent line at top
    g.setColour(juce::Colours::mediumaquamarine);
    g.fillRect(0, 0, getWidth(), 2);

    // Optional: subtle outline
    g.setColour(juce::Colours::mediumaquamarine.withAlpha(0.3f));
    g.drawRect(getLocalBounds(), 1);
}

void ScenePropertiesComponent::resized()
{
    auto area = getLocalBounds().reduced(10);
    int rowHeight = 28;
    int labelWidth = 100;
    int spacing = 8;

    auto row = area.removeFromTop(rowHeight);
    nameLabel.setBounds(row.removeFromLeft(labelWidth));
    nameEditor.setBounds(row);

    area.removeFromTop(spacing);

    row = area.removeFromTop(rowHeight);
    addressLabel.setBounds(row.removeFromLeft(labelWidth));
    addressEditor.setBounds(row);

    area.removeFromTop(spacing * 2);
    publishToMarketplaceButton.setBounds(area.removeFromTop(rowHeight).reduced(0, 4));
}
