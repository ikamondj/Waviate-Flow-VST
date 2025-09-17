/*
  ==============================================================================

    AudioReader.cpp
    Created: 13 Sep 2025 1:20:39pm
    Author:  ikamo

  ==============================================================================
*/

#include "AudioReader.h"
#include "RestAPIHandler.h"

bool AudioReader::readAudioFile(const std::string& url, std::vector<ddtype>& samples)
{
    juce::File afile(url);
    if (!afile.existsAsFile())
        return false;

    // Register formats (do this once in your app, not every call ideally)
    static juce::AudioFormatManager formatManager;
    static bool initialized = false;
    if (!initialized) {
        formatManager.registerBasicFormats(); // WAV, AIFF, FLAC, Ogg Vorbis, MP3 (if available)
        initialized = true;
    }

    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(afile));
    if (reader == nullptr)
        return false;

    // Read into a temporary AudioBuffer<float>
    juce::AudioBuffer<float> buffer((int)reader->numChannels,
        (int)reader->lengthInSamples);
    reader->read(&buffer,                 // dest
        0,                       // dest start sample
        (int)reader->lengthInSamples,
        0,                       // reader start sample
        true, true);             // use left+right channels

    // Flatten to mono double vector (you can change this to interleaved stereo if you want)
    samples.clear();
    samples.reserve((size_t)buffer.getNumSamples());

    const float* left = buffer.getReadPointer(0);
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        double value = left[i];

        // If stereo, average channels (basic downmix)
        if (buffer.getNumChannels() > 1)
        {
            const float* right = buffer.getReadPointer(1);
            value = 0.5 * (value + right[i]);
        }

        samples.push_back(value);
    }
    return true;
}

bool AudioReader::readWebAudio(const std::string& url, std::vector<ddtype>& out)
{
    out.clear();

    // 1) Download the bytes (must be binary-safe)
    const std::string bytes = RestApiHandler::get(url);
    if (bytes.empty())
        return false;

    // 2) Init formats once
    static juce::AudioFormatManager fm;
    static std::once_flag fmInit;
    std::call_once(fmInit, [] { fm.registerBasicFormats(); });

    // 3) Build a unique_ptr<InputStream> (NOT a copy of MemoryInputStream)
    std::unique_ptr<juce::InputStream> stream =
        std::make_unique<juce::MemoryInputStream>(bytes.data(), bytes.size(), false);

    // 4) Give ownership to JUCE and get a reader
    std::unique_ptr<juce::AudioFormatReader> reader(fm.createReaderFor(std::move(stream)));
    if (!reader)
        return false;

    // 5) Read all samples, downmix to mono doubles (match your file reader behavior)
    juce::AudioBuffer<float> buffer((int)reader->numChannels, (int)reader->lengthInSamples);
    if (!reader->read(&buffer, 0, buffer.getNumSamples(), 0, true, true))
        return false;

    out.resize((size_t)buffer.getNumSamples());
    const float* l = buffer.getReadPointer(0);
    if (buffer.getNumChannels() == 1)
    {
        for (int i = 0; i < buffer.getNumSamples(); ++i) out[(size_t)i] = (ddtype)l[i];
    }
    else
    {
        const float* r = buffer.getReadPointer(1);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            out[(size_t)i] = (ddtype)(0.5 * (l[i] + r[i]));
    }

    return true;
}

