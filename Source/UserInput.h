/*
  ==============================================================================

    UserInput.h
    Created: 11 Aug 2025 5:35:21pm
    Author:  ikamo

  ==============================================================================
*/
#include <array>
#include "CircleBuffer.h"
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
    double pitchWheelValue;
    double modWheelValue;
    long numFramesStartOfBlock;
    double sampleInBlock;
    double sampleRate;
    double leftInput;
    double rightInput;
    double sideChainL;
    double sideChainR;
    double isStereoRight;
	CircleBuffer leftInputHistory;
    CircleBuffer rightInputHistory;
    double getHistoricalSample(int samplesAgo) const;
    std::array<double, 1024> storeableValues;
    class RunnerInput* runner = nullptr;
};