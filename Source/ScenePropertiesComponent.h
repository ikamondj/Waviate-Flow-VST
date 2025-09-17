
#pragma once
#include <JuceHeader.h>

class ScenePropertiesComponent : public juce::Component
{
public:
    ScenePropertiesComponent(class WaviateFlow2025AudioProcessor& pcessor);
    void setActiveScene(class SceneComponent* scene);
private:
    SceneComponent* activeSceneData = nullptr;
    juce::TextEditor nameEditor;
    juce::TextEditor addressEditor;
    juce::TextButton publishToMarketplaceButton{ "Publish" };
    juce::Label nameLabel{ {}, "Name:" };
    juce::Label addressLabel{ {}, "Menu Address:" };
    WaviateFlow2025AudioProcessor& processor;
};