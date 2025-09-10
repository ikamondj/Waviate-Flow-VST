/*
  ==============================================================================

    Serializer.h
    Created: 31 Aug 2025 9:42:01pm
    Author:  ikamo

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>
#include <optional>
#include <nlohmann/json.hpp>
class Serializer {
public:
    std::optional<nlohmann::json> serializeToJson(struct NodeType& type);
};