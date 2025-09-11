
#pragma once
#include <JuceHeader.h>

class ScenePropertiesComponent : public juce::Component
{
    juce::TextEditor nameEditor;
    juce::TextEditor addressEditor;
    juce::TextButton publishToMarketplaceButton{ "Publish"};
    juce::Label nameLabel{ {}, "Name:" };
    juce::Label addressLabel{ {}, "Menu Address:" };
    void setActiveScene(class SceneComponent* scene);
private:
    SceneComponent* activeSceneData = nullptr;
};