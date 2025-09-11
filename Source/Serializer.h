/*
  ==============================================================================

    Serializer.h
    Created: 31 Aug 2025 9:42:01pm
    Author:  ikamo

  ==============================================================================
*/

#pragma once

#include <optional>
#include <nlohmann/json.hpp>
#include "NodeType.h"
class Serializer {
public:
    static std::optional<nlohmann::json> serializeToJson(class SceneData& scene, const class WaviateFlow2025AudioProcessor& processor);
    static std::optional<NodeType> deserialize(const std::string& rep);
};