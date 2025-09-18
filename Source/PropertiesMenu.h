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
    
    void setButtonIcon(bool isActive);
protected:
    virtual void onUpdateUI() = 0;
    void setButtonIcon(const char* binImgData, int size, bool isActive = false);
};