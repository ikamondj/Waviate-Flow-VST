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
class Serializer {
public:
    static std::optional<nlohmann::json> serializeToJson(struct NodeType& type);
    static std::optional<NodeType> deserialize(const std::string& rep);
};