/*
  ==============================================================================

    PropertiesMenu.h
    Created: 17 Sep 2025 3:00:38pm
    Author:  ikamo

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>
class PropertiesMenu : public juce::Component {
public:
    void updateProperties();
    juce::ImageButton openIconBTN;
protected:
    virtual void onUpdateUI() = 0;
};