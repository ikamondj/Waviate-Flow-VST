#include "DawManager.h"

DawManager::DawManager(juce::AudioProcessor& p) : processor(p) {}

double DawManager::getBPM() {
    if (auto* playHead = processor.getPlayHead()) {
        juce::AudioPlayHead::CurrentPositionInfo posInfo;
        if (playHead->getCurrentPosition(posInfo)) {
            return posInfo.bpm;
        }
    }
    return 120.0; // Default fallback
}

double DawManager::getPositionInBeat() {
    if (auto* playHead = processor.getPlayHead()) {
        juce::AudioPlayHead::CurrentPositionInfo posInfo;
        if (playHead->getCurrentPosition(posInfo)) {
            return posInfo.ppqPosition - (int)posInfo.ppqPosition;
        }
    }
    return 0.0;
}

std::array<int, 2> DawManager::getTimeSignature() {
    if (auto* playHead = processor.getPlayHead()) {
        juce::AudioPlayHead::CurrentPositionInfo posInfo;
        if (playHead->getCurrentPosition(posInfo)) {
            return {posInfo.timeSigNumerator, posInfo.timeSigDenominator};
        }
    }
    return {4, 4};
}

double DawManager::getTimeSinceStartOfBarSeconds() {
    if (auto* playHead = processor.getPlayHead()) {
        juce::AudioPlayHead::CurrentPositionInfo posInfo;
        if (playHead->getCurrentPosition(posInfo)) {
            double beatsSinceBar = posInfo.ppqPositionOfLastBarStart >= 0 ?
                posInfo.ppqPosition - posInfo.ppqPositionOfLastBarStart : 0.0;
            double secondsPerBeat = 60.0 / (posInfo.bpm > 0 ? posInfo.bpm : 120.0);
            return beatsSinceBar * secondsPerBeat;
        }
    }
    return 0.0;
}

double DawManager::getTimeSinceStartOfTrackSeconds() {
    if (auto* playHead = processor.getPlayHead()) {
        juce::AudioPlayHead::CurrentPositionInfo posInfo;
        if (playHead->getCurrentPosition(posInfo)) {
            return posInfo.timeInSeconds;
        }
    }
    return 0.0;
}

int64_t DawManager::getNumSamplesSinceStartOfTrack() {
    if (auto* playHead = processor.getPlayHead()) {
        juce::AudioPlayHead::CurrentPositionInfo posInfo;
        if (playHead->getCurrentPosition(posInfo)) {
            return posInfo.timeInSamples;
        }
    }
    return 0;
}

bool DawManager::isPlaying() {
    if (auto* playHead = processor.getPlayHead()) {
        juce::AudioPlayHead::CurrentPositionInfo posInfo;
        if (playHead->getCurrentPosition(posInfo)) {
            return posInfo.isPlaying;
        }
    }
    return false;
}

bool DawManager::isRecording() {
    if (auto* playHead = processor.getPlayHead()) {
        juce::AudioPlayHead::CurrentPositionInfo posInfo;
        if (playHead->getCurrentPosition(posInfo)) {
            return posInfo.isRecording;
        }
    }
    return false;
}

bool DawManager::isLooping() {
    if (auto* playHead = processor.getPlayHead()) {
        juce::AudioPlayHead::CurrentPositionInfo posInfo;
        if (playHead->getCurrentPosition(posInfo)) {
            return posInfo.isLooping;
        }
    }
    return false;
}


const juce::Array< juce::AudioProcessorParameter * > & DawManager::getParameters() {
    return processor.getParameters();
}

bool DawManager::isRealtime() {
    return !processor.isNonRealtime();
}