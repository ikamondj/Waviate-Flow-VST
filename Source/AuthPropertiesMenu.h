/*
  ==============================================================================

    AuthPropertiesMenu.h
    Created: 17 Sep 2025 9:36:57pm
    Author:  ikamo

  ==============================================================================
*/

#pragma once
#include "PropertiesMenu.h"


class AuthPropertiesMenu : public PropertiesMenu {
public:
    AuthPropertiesMenu(class WaviateFlow2025AudioProcessor& p);
protected:
    // Inherited via PropertiesMenu
    void onUpdateUI() override;
    void paint(juce::Graphics& g) override;
    void resized() override;
    void launchOAuth(const juce::String& provider);
    void exchangeCodeForToken(const juce::String& provider, const juce::String& code, const juce::String& verifier);
    WaviateFlow2025AudioProcessor& processor;
private:
    std::vector<std::unique_ptr<juce::TextButton>> providerButtons;

};