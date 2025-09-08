/*
  ==============================================================================

    JWTManager.h
    Created: 7 Sep 2025 12:10:39pm
    Author:  ikamo

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>

class JWTManager {
public:
    JWTManager();
    bool validateToken(const juce::String& token) const;
    juce::String getClaim(const juce::String& token, const juce::String& claim) const;
    bool isExpired(const juce::String& token) const;
    void setToken(const juce::String& token);
    juce::String getToken() const;

private:
    juce::String jwtToken;
};