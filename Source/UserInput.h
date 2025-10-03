/*
  ==============================================================================

    UserInput.h
    Created: 11 Aug 2025 5:35:21pm
    Author:  ikamo

  ==============================================================================
*/
#pragma once
#define MIDI_NOTE_COUNT 128
#define CIRCLEBUFFER_CAPACITY 480000
#define DAW_PARAM_SIZE 1024
#include "StringifyDefines.h"

DEFINE_AND_CREATE_VAR(
    static inline void CircleBuffer_init(int* head, int* count) {
    *head = 0;
    *count = 0;
}

static inline void CircleBuffer_add(double* data,
    int* head,
    int* count,
    double value) {
    data[*head] = value;
    *head = (*head + 1) % CIRCLEBUFFER_CAPACITY;
    if (*count < CIRCLEBUFFER_CAPACITY) {
        (*count)++;
    }
}

static inline int CircleBuffer_size(const int* count) {
    return *count;
}

static inline int CircleBuffer_capacity(void) {
    return CIRCLEBUFFER_CAPACITY;
}

static inline double CircleBuffer_get(const double* data,
    const int head,
    const int count,
    int indexAgo) {
    if (indexAgo >= count) {
        return 0.0;
    }
    int idx = (head + CIRCLEBUFFER_CAPACITY - 1 - indexAgo) % CIRCLEBUFFER_CAPACITY;
    return data[idx];
}

typedef struct UserInput
{
    double notesOn[MIDI_NOTE_COUNT];
    double noteStartFrame[MIDI_NOTE_COUNT];
    double noteEndFrame[MIDI_NOTE_COUNT];
    double noteVelocity[MIDI_NOTE_COUNT];
    double controllerValues[MIDI_NOTE_COUNT];
    double noteHz[MIDI_NOTE_COUNT];
    double noteCycle[MIDI_NOTE_COUNT];
    double dawParams[DAW_PARAM_SIZE];
    int midiCCValues[MIDI_NOTE_COUNT];
    double pitchWheelValue;
    double modWheelValue;
    long long numFramesStartOfBlock;
    long long sampleInBlock;
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
    double leftInputHistoryArray[CIRCLEBUFFER_CAPACITY];
    int leftInputHistoryHead;
    int leftInputHistorySize;
    double rightInputHistoryArray[CIRCLEBUFFER_CAPACITY];
    int rightInputHistoryHead;
    int rightInputHistorySize;
    //double getHistoricalSample(int samplesAgo) const;
    //std::unordered_map<juce::String, double> namedValues;
    //std::unordered_map<int, double> storeableValues;
} UserInput;


, UserInputClang
);

static inline const juce::String UserInputClangJ(UserInputClang);