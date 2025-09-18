/*
  ==============================================================================

    PropertiesMenu.cpp
    Created: 17 Sep 2025 3:00:38pm
    Author:  ikamo

  ==============================================================================
*/

#include "PropertiesMenu.h"

void PropertiesMenu::updateProperties()
{

    this->onUpdateUI();
    repaint();
}

void PropertiesMenu::setButtonIcon(const char* binImgData, int size, bool isActive)
{
    auto im = juce::ImageCache::getFromMemory(binImgData, size);
    openIconBTN.setImages(false, true, true, im, 0.5,
        (isActive ? juce::Colours::fuchsia : juce::Colours::white).withAlpha(isActive ? 0.15f : 0.0f), im, 1.0,
        juce::Colours::aquamarine.withAlpha(0.2f), im, 1.0,
        juce::Colours::blueviolet.withAlpha(0.2f));
}

void PropertiesMenu::setButtonIcon(bool isActive)
{
    auto im = openIconBTN.getNormalImage();
    openIconBTN.setImages(false, true, true, im, 0.5,
        (isActive ? juce::Colours::fuchsia : juce::Colours::white).withAlpha(isActive ? 0.15f : 0.0f), im, 1.0,
        juce::Colours::aquamarine.withAlpha(0.2f), im, 1.0,
        juce::Colours::blueviolet.withAlpha(0.2f));
}

