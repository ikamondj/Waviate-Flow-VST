/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "UserInput.h"
#include "NodeType.h"
#include "Runner.h"
#include "UserData.h"
#include "RunnerInput.h"
#include "DawManager.h"
//==============================================================================
/**
*/

#define nBufferSize 4

class WaviateFlow2025AudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    WaviateFlow2025AudioProcessor();
    ~WaviateFlow2025AudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;
    class WaviateFlow2025AudioProcessorEditor* getCurrentEditor();

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;
    void addNodeOfType(int typeId, juce::Point<int> localPos, class SceneComponent* scene);
    void addScene(const juce::String& name);
    void deleteScene(SceneComponent* scene);
    

    std::vector<std::unique_ptr<SceneComponent>>& getScenes();
    const std::vector<NodeType>& getRegistry() const;

    SceneComponent& getScene();

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    void handleMidi(const juce::MidiMessage& message, UserInput& input);
    void initializeRunner();
    class SceneComponent* activeScene;
    
    std::unordered_map<juce::String, int> keyCodeTypeMapping;
    std::vector<NodeType> registry;
    std::vector<std::unique_ptr<SceneComponent>> scenes;
    UserInput userInput;
    std::array<double, 128> noteHzOfficialValues;
    void initializeRegistry();
    void setAudibleScene(class SceneData* scene);
    void setActiveScene(SceneComponent* scene);
    SceneComponent* getActiveScene();
    double maxOutBeforeDistortion = 10.0;
    static WaviateFlow2025AudioProcessor* GetActiveInstance();
    //std::deque<std::unique_ptr<RunnerInput>> runners;
    std::array<RunnerInput, nBufferSize> runners;
    std::atomic_int runnerIndex;
    const RunnerInput* getCurrentRunner() const noexcept;
    const RunnerInput* getPreviousRunner() const noexcept;
    
    void swapToNextRunner();
    static constexpr double fadeWindowSeconds = 0.020;
    static constexpr int bufferSize = 96000;
    std::array<float, bufferSize> ring;
    juce::AbstractFifo fifo{ bufferSize };
    class SceneData* getAudibleScene();
    std::optional<UserData> currentLogin;
    uint16_t getCurrentLoadedTypeIndex();
    uint64_t getCurrentLoadedUserIndex();
    void initializeAllScenes();
    DawManager dawManager;

private:
    uint16_t currentLoadedTypeIndex = 0;
    uint64_t currentLoadedUserIndex = 1;
    class SceneData* audibleScene;
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaviateFlow2025AudioProcessor)
};


