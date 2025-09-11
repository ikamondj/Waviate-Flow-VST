
#include <JuceHeader.h>


class DawManager {
    class juce::AudioProcessor& processor;
public:
    DawManager(class juce::AudioProcessor& p);
    double getBPM();
    double getPositionInBeat();
    std::array<int, 2> getTimeSignature();
    double getTimeSinceStartOfBarSeconds();
    double getTimeSinceStartOfTrackSeconds();
    int64_t getNumSamplesSinceStartOfTrack();
    bool isPlaying();
    bool isRecording();
    bool isLooping();
    bool isMetronomeEnabled();
    const juce::Array< juce::AudioProcessorParameter * > & getParameters();
    bool isRealtime();
}
