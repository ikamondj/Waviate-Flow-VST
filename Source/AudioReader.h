/*
  ==============================================================================

    AudioReader.h
    Created: 13 Sep 2025 1:20:32pm
    Author:  ikamo

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>
#include <vector>
#include "ddtype.h"

class AudioReader {
public:
    static bool readAudioFile(const std::string& url, std::vector<ddtype>& result);
    static bool readWebAudio(const std::string& url, std::vector<ddtype>& result);
};
