#include "RestApiHandler.h"
#include <juce_core/juce_core.h>
using opts = juce::URL::InputStreamOptions;
std::string RestApiHandler::get(const std::string& url,
    const std::map<std::string, std::string>& headers,
    int retryCount)
{
    juce::String result;

    for (int attempt = 0; attempt < retryCount; ++attempt)
    {
        juce::URL juceURL = juce::URL(juce::String(url));
        auto opt = opts(juce::URL::ParameterHandling::inAddress);
        juce::String headerBlock;
        for (const auto& [key, value] : headers)
        {
            headerBlock << juce::String(key) << ": " << juce::String(value) << "\r\n";
        }

        const auto& trueOpts = !headerBlock.isEmpty() ? opt.withExtraHeaders(headerBlock) : opt;
        std::unique_ptr<juce::InputStream> stream(juceURL.createInputStream(trueOpts));
        if (stream != nullptr)
        {
            juce::MemoryBlock block;
            stream->readIntoMemoryBlock(block);
            return std::string((const char*)block.getData(), block.getSize());
        }
    }

    return "";
}

std::string RestApiHandler::post(const std::string& url,
    const std::string& body,
    const std::map<std::string, std::string>& headers,
    int retryCount)
{
    for (int attempt = 0; attempt < retryCount; ++attempt)
    {
        juce::URL juceURL = juce::URL(juce::String(url)).withPOSTData(juce::String(body));

        auto opt = opts(juce::URL::ParameterHandling::inAddress);
        juce::String headerBlock;
        for (const auto& [key, value] : headers)
        {
            headerBlock << juce::String(key) << ": " << juce::String(value) << "\r\n";
        }

        auto trueOpts = !headerBlock.isEmpty() ? opt.withExtraHeaders(headerBlock) : opt;
        std::unique_ptr<juce::InputStream> stream(juceURL.createInputStream(trueOpts));

        if (stream != nullptr)
        {
            juce::MemoryBlock block;
            stream->readIntoMemoryBlock(block);
            return std::string((const char*)block.getData(), block.getSize());
        }
    }

    return "";
}
