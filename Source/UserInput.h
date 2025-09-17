/*
  ==============================================================================

    UserInput.h
    Created: 11 Aug 2025 5:35:21pm
    Author:  ikamo

  ==============================================================================
*/
#include <array>
#include <unordered_map>
#include "CircleBuffer.h"
#include <JuceHeader.h>
#pragma once
class UserInput
{
public:
    std::array<double, 128> notesOn;
    std::array<double, 128> noteStartFrame;
	std::array<double, 128> noteEndFrame;
	std::array<double, 128> noteVelocity;
	std::array<double, 128> controllerValues;
    std::array<double, 128> noteHz;
    std::array<double, 128> noteCycle;
    std::array<int, 128> midiCCValues;
    std::array<double, 128> dawParams;
    double pitchWheelValue;
    double modWheelValue;
    int64_t numFramesStartOfBlock;
    int64_t sampleInBlock;
    double sampleRate;
    double leftInput;
    double rightInput;
    double sideChainL;
    double sideChainR;
    bool isStereoRight;
    bool isRealTime;
    bool isPlaying;
    bool isLooping;
    bool isRecording;
    bool isMetronomeGoing;
    int timeSigTop;
    int timeSigBottom;
    double BPM;
	CircleBuffer leftInputHistory;
    CircleBuffer rightInputHistory;
    double getHistoricalSample(int samplesAgo) const;
    std::unordered_map<juce::String, double> namedValues;
    std::unordered_map<int, double> storeableValues;
    const class RunnerInput* runner = nullptr;
};