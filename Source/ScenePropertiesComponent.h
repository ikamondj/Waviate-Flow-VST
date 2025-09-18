#pragma once
#include <JuceHeader.h>
#include "PropertiesMenu.h"

class ScenePropertiesComponent : public PropertiesMenu
{
public:
    ScenePropertiesComponent(class WaviateFlow2025AudioProcessor& pcessor);

    void setActiveScene(class SceneComponent* scene);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    SceneComponent* activeSceneData = nullptr;
    juce::TextEditor nameEditor;
    juce::TextEditor addressEditor;
    juce::TextButton publishToMarketplaceButton{ "Publish" };
    juce::Label nameLabel{ {}, "Name:" };
    juce::Label addressLabel{ {}, "Menu Address:" };
    WaviateFlow2025AudioProcessor& processor;

    // Inherited via PropertiesMenu
    void onUpdateUI() override;

    void configureLookAndFeel();
    void attachCallbacks();
};
