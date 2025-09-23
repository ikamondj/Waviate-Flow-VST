/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

WaviateFlow2025AudioProcessor* activeInstance;

//==============================================================================
WaviateFlow2025AudioProcessor::WaviateFlow2025AudioProcessor() : dawManager(*this)
#ifndef JucePlugin_PreferredChannelConfigurations
      , AudioProcessor (BusesProperties()
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
		               .withInput("Side Chain", juce::AudioChannelSet::stereo(), false)
                       )
#endif
{
    userInput.numFramesStartOfBlock = 0;
    for (int i = 0; i < noteHzOfficialValues.size(); i += 1) {
        userInput.noteHz[i] = noteHzOfficialValues[i] = 440.0 * std::pow(2.0, (i - 69) / 12.0);
        userInput.noteVelocity[i] = 0.0;
        userInput.notesOn[i] = 0.0;
        userInput.noteStartFrame[i] = -100000000;
        userInput.noteEndFrame[i] = 0.0;
    }
    // Always create the "main" scene
    registry.reserve(65536);
    initializeRegistry();
    audibleScene = nullptr;
    scenes.reserve(1048576);
    if (scenes.empty()) {
        addScene("main");
        audibleScene = scenes[0].get();
        setActiveScene(dynamic_cast<SceneComponent*>(audibleScene));
    }
    activeInstance = this;
}

WaviateFlow2025AudioProcessor::~WaviateFlow2025AudioProcessor()
{
    scenes.clear();
}

//==============================================================================
const juce::String WaviateFlow2025AudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool WaviateFlow2025AudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool WaviateFlow2025AudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool WaviateFlow2025AudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double WaviateFlow2025AudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int WaviateFlow2025AudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int WaviateFlow2025AudioProcessor::getCurrentProgram()
{
    return 0;
}

void WaviateFlow2025AudioProcessor::setCurrentProgram (int index)
{
}

const juce::String WaviateFlow2025AudioProcessor::getProgramName (int index)
{
    return {};
}

void WaviateFlow2025AudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void WaviateFlow2025AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
}

void WaviateFlow2025AudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

bool WaviateFlow2025AudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // --- Main Output: require stereo ---
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // --- Main Input: allow none, mono, stereo ---
    auto mainIn = layouts.getMainInputChannelSet();
    if (mainIn != juce::AudioChannelSet::disabled()
        && mainIn != juce::AudioChannelSet::mono()
        && mainIn != juce::AudioChannelSet::stereo())
        return false;

    // --- Sidechain Input: allow none, mono, stereo ---
    if (getBusCount(true) > 1) // has sidechain
    {
        auto sideIn = layouts.getChannelSet(true, 1); // (isInput = true, index = 1)
        if (sideIn != juce::AudioChannelSet::disabled()
            && sideIn != juce::AudioChannelSet::mono()
            && sideIn != juce::AudioChannelSet::stereo())
            return false;
    }

    return true;
}


void WaviateFlow2025AudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    const double sampleRate = userInput.sampleRate = getSampleRate();

    auto mainIn = getBusBuffer(buffer, true, 0); // main input bus
    auto mainOut = getBusBuffer(buffer, false, 0); // main output bus

    juce::MidiBuffer::Iterator it(midiMessages);
    juce::MidiMessage msg;
    int eventSample;
    bool hasEvent = it.getNextEvent(msg, eventSample);

    const int numInCh = mainIn.getNumChannels();
    const int numOutCh = mainOut.getNumChannels();

    // --- Safe sidechain acquisition ---
    juce::AudioBuffer<float> sidechainIn;
    int numSInCh = 0;
    if (getBusCount(true) > 1) // sidechain bus exists
    {
        sidechainIn = getBusBuffer(buffer, true, 1);
        numSInCh = sidechainIn.getNumChannels();
    }

    auto editor = dynamic_cast<WaviateFlow2025AudioProcessorEditor*>(getActiveEditor());

    // Pointers to the actual INPUT audio
    const float* inL = (numInCh > 0) ? mainIn.getReadPointer(0) : nullptr;
    const float* inR = (numInCh > 1) ? mainIn.getReadPointer(1)
        : (numInCh > 0 ? mainIn.getReadPointer(0) : nullptr);

    const float* inSL = (numSInCh > 0) ? sidechainIn.getReadPointer(0) : nullptr;
    const float* inSR = (numSInCh > 1) ? sidechainIn.getReadPointer(1)
        : (numSInCh > 0 ? sidechainIn.getReadPointer(0) : nullptr);

    const double timePerSample = 1.0 / sampleRate;

    if (numOutCh > 0) {
        float* outR, * outL;
        outR = outL = mainOut.getWritePointer(0);
        if (numOutCh > 1) {
            outR = mainOut.getWritePointer(1);
        }

        const int64_t lastSwap = 0; //TODO fix!
        const int64_t base = 50000; //TODO fix!
        const RunnerInput* runner = nullptr;
        const RunnerInput* prevRunner = nullptr;

        runner = getCurrentRunner();
        prevRunner = getPreviousRunner();

        const int fadeWindowSamples = int(fadeWindowSeconds * sampleRate);

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            int64_t now = base + sample;
            int64_t sinceSwap = now - lastSwap;
            double alpha = (sinceSwap < fadeWindowSamples)
                ? double(sinceSwap) / double(fadeWindowSamples)
                : 1.0;

            userInput.leftInput = inL ? inL[sample] : 0.0f;
            userInput.rightInput = inR ? inR[sample] : 0.0f;
            userInput.sideChainL = inSL ? inSL[sample] : 0.0f;
            userInput.sideChainR = inSR ? inSR[sample] : 0.0f;

            userInput.sampleInBlock = sample;
            // Process all MIDI events scheduled for this sample
            while (hasEvent && eventSample == sample)
            {
                handleMidi(msg, userInput); // <-- your MIDI handling
                hasEvent = it.getNextEvent(msg, eventSample);
            }

            for (int i = 0; i < 128; i += 1) {
                userInput.noteCycle[i] = std::fmod(userInput.noteCycle[i] +
                    noteHzOfficialValues[i] * timePerSample, 1.0);
            }

            outL[sample] = outR[sample] = userInput.isStereoRight = 0.0;
            if (audibleScene) {

                auto ndata = audibleScene->nodeDatas[0];
                std::span<ddtype> l = Runner::run(runner, userInput, std::vector<std::span<ddtype>>());
                for (ddtype d : l) {
                    outL[sample] += d.d * alpha;
                }

                userInput.isStereoRight = 1.0;
                std::span<ddtype> r = Runner::run(runner, userInput, std::vector<std::span<ddtype>>());
                for (ddtype d : r) {
                    outR[sample] += d.d * alpha;
                }

                const double beta = 1 - alpha;
                if (beta > 0.0) {
                    userInput.isStereoRight = 0.0;
                    std::span<ddtype> l = Runner::run(prevRunner, userInput, std::vector<std::span<ddtype>>());
                    for (ddtype d : l) {
                        outL[sample] += d.d * beta;
                    }

                    userInput.isStereoRight = 1.0;
                    std::span<ddtype> r = Runner::run(prevRunner, userInput, std::vector<std::span<ddtype>>());
                    for (ddtype d : r) {
                        outR[sample] += d.d * beta;
                    }
                }
                CircleBuffer_add(userInput.rightInputHistoryArray, &userInput.rightInputHistoryHead, &userInput.rightInputHistorySize, outR[sample] = 10.0 * std::tanh(outR[sample] * 0.1));
                CircleBuffer_add(userInput.leftInputHistoryArray, &userInput.leftInputHistoryHead, &userInput.leftInputHistorySize, outL[sample] = 10.0 * std::tanh(outL[sample] * 0.1));
                auto sc = dynamic_cast<SceneComponent*>(audibleScene);
                if (sc) {
                    dynamic_cast<juce::AudioVisualiserComponent*>(sc->nodes[0]->inputGUIElements[0].get())->pushSample(&outL[sample], 1);
                }
            }
        }
    }
    const int n = buffer.getNumSamples();
    int start1, size1, start2, size2;
    fifo.prepareToWrite(n, start1, size1, start2, size2);
    if (size1 > 0) memcpy(ring.data() + start1, buffer.getReadPointer(0), size1 * sizeof(float));
    if (size2 > 0) memcpy(ring.data() + start2, buffer.getReadPointer(0) + size1, size2 * sizeof(float));
    fifo.finishedWrite(size1 + size2);

    //totalSamplesProcessed.fetch_add(buffer.getNumSamples(), std::memory_order_relaxed);
    userInput.numFramesStartOfBlock += buffer.getNumSamples();
}


//==============================================================================
bool WaviateFlow2025AudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

WaviateFlow2025AudioProcessorEditor* WaviateFlow2025AudioProcessor::getCurrentEditor()
{
    return dynamic_cast<WaviateFlow2025AudioProcessorEditor*>(getActiveEditor());
}

juce::AudioProcessorEditor* WaviateFlow2025AudioProcessor::createEditor()
{
    return new WaviateFlow2025AudioProcessorEditor (*this);
}

//==============================================================================
void WaviateFlow2025AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void WaviateFlow2025AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

void WaviateFlow2025AudioProcessor::handleMidi(const juce::MidiMessage& message, UserInput& input)
{
    if (message.isNoteOn()) {
		input.notesOn[message.getNoteNumber()] = 1.0;
		input.noteVelocity[message.getNoteNumber()] = message.getFloatVelocity();
    }
    else if (message.isNoteOff()) {
		input.notesOn[message.getNoteNumber()] = 0.0;
    }
	else if (message.isController()) {
		input.controllerValues[message.getControllerNumber()] = message.getControllerValue() / 127.0;
	}
    else if (message.isPitchWheel()) {
        input.pitchWheelValue = (message.getPitchWheelValue() / 8191.5) - 1.0;
    }
}

void WaviateFlow2025AudioProcessor::initializeRunner()
{
    if (scenes.empty()) return;

    swapToNextRunner();

}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new WaviateFlow2025AudioProcessor();
}

void WaviateFlow2025AudioProcessor::addNodeOfType(int typeId, juce::Point<int> localPos, SceneComponent* scene)
{
    if (typeId < 0)
        return; // safety check
    if (typeId >= registry.size()) {
        typeId -= registry.size();
        if (typeId >= scenes.size() || scene == scenes[typeId].get()) { return; }
        scene->addNode(scenes[typeId]->customNodeType, localPos);
    }
    else {
        scene->addNode(registry[typeId], localPos);
    }


    if (getCurrentEditor()) { getCurrentEditor()->repaint(); }
}

void WaviateFlow2025AudioProcessor::addScene(const juce::String& name)
{
    scenes.push_back(std::make_unique<SceneComponent>(*this, name.toStdString()));
    auto scene = scenes.back().get();
    auto editor = getCurrentEditor();
    if (editor) {
        editor->canvas.addAndMakeVisible(scene);
    }
    scene->setName(name);

    

    addNodeOfType(0, juce::Point<int>(400, 500), scene);

    if (editor) {
        editor->browser.updateContent();
    }
}

void WaviateFlow2025AudioProcessor::deleteScene(SceneComponent* scene)
{
    if (scene != scenes[0].get()) {
        for (size_t i = 0; i < scenes.size(); ++i) {
            if (scenes[i].get() == scene) {
                scenes.erase(scenes.begin() + i);
                break;
            }
        }
        if (getCurrentEditor()) {
            getCurrentEditor()->browser.updateContent();
        }
        
    }
}
SceneComponent& WaviateFlow2025AudioProcessor::getScene()
{
    return *scenes[0].get();
}

std::vector< std::unique_ptr<SceneComponent>>& WaviateFlow2025AudioProcessor::getScenes()
{
    return scenes;
}

const std::vector<NodeType>& WaviateFlow2025AudioProcessor::getRegistry() const
{
    return registry;
}



void WaviateFlow2025AudioProcessor::setAudibleScene(SceneData* scene)
{
    auto prev = audibleScene;
    bool containsInputNodes = false;
    if (scene) {
        for (auto& comp : scene->nodeDatas) {
            containsInputNodes |= comp->getType()->isInputNode;
        }
    }
    if (containsInputNodes) {
        return;
    }
    audibleScene = scene;
    if (prev != audibleScene) {
        this->initializeRunner();
    }
}

void WaviateFlow2025AudioProcessor::setActiveScene(SceneComponent* scene, bool shouldOpen)
{
    scene->setVisible(true);
    scene->toFront(true);
    activeScene = scene;
    if (auto editor = getCurrentEditor()) {
        editor->browser.repaint();
        editor->setActiveScene(scene, shouldOpen);
    }
}

SceneComponent* WaviateFlow2025AudioProcessor::getActiveScene()
{
    return activeScene;
}

WaviateFlow2025AudioProcessor* WaviateFlow2025AudioProcessor::GetActiveInstance()
{
    return activeInstance;
}

const RunnerInput* WaviateFlow2025AudioProcessor::getCurrentRunner() const noexcept {
    return &runners[(runnerIndex.load(std::memory_order::acquire) + 0) % nBufferSize];
}

const RunnerInput* WaviateFlow2025AudioProcessor::getPreviousRunner() const noexcept
{
    return &runners[(runnerIndex.load(std::memory_order::acquire) + 1) % nBufferSize];
}


void WaviateFlow2025AudioProcessor::swapToNextRunner()
{
    int i = runnerIndex.load(std::memory_order::acquire);
    auto& nextRunner = runners[(i + 1) % nBufferSize];
    Runner::initialize(nextRunner, audibleScene, std::vector<std::span<ddtype>>());
    runnerIndex.store((i + 1) % nBufferSize, std::memory_order_release);
}

SceneData* WaviateFlow2025AudioProcessor::getAudibleScene() { return audibleScene; }

uint16_t WaviateFlow2025AudioProcessor::getCurrentLoadedTypeIndex()
{
    uint16_t result =  currentLoadedTypeIndex++;
    if (currentLoadedTypeIndex == 0) {
        ++currentLoadedUserIndex;
    }
    return result;
}

uint64_t WaviateFlow2025AudioProcessor::getCurrentLoadedUserIndex()
{
    if (currentLoadedUserIndex >= 16) {
        throw std::out_of_range("Too many loaded Node Types!");
    }
    return currentLoadedUserIndex;
}

void WaviateFlow2025AudioProcessor::initializeAllScenes()
{
    for (auto& scene : scenes) {
        if (scene.get()) {
            Runner::initialize(*scene, scene.get(), std::vector<std::span<ddtype>>());
        }
    }
}

