#include "ScenePropertiesComponent.h"
#include "SceneComponent.h"

ScenePropertiesComponent::ScenePropertiesComponent(WaviateFlow2025AudioProcessor& pcessor) : processor(pcessor)
{
}

void ScenePropertiesComponent::setActiveScene(SceneComponent* scene) {
    activeSceneData = scene;
    if (activeSceneData) {
        nameEditor.setText(activeSceneData->getSceneName(), juce::dontSendNotification);
        addressEditor.setText(activeSceneData->customNodeType.address, juce::dontSendNotification);
        bool authed = true; // Placeholder for actual authentication check
        bool hasReachedPublishLimit = false; // Placeholder for actual limit check
        bool canPublish = authed && !hasReachedPublishLimit;
        publishToMarketplaceButton.setEnabled(canPublish);
    }
    else {
        nameEditor.setText("", juce::dontSendNotification);
        addressEditor.setText("", juce::dontSendNotification);
        nameEditor.setEnabled(false);
        addressEditor.setEnabled(false);
        publishToMarketplaceButton.setEnabled(false);
    }
}