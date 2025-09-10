/*
  ==============================================================================

    Serializer.cpp
    Created: 31 Aug 2025 9:42:01pm
    Author:  ikamo

  ==============================================================================
*/

#include "Serializer.h"
#include "NodeType.h"

std::optional<nlohmann::json> Serializer::serializeToJson(NodeType& type)
{
    nlohmann::json j;
    if (!type.fromScene) {
        return {};
    }
    j["id"] = type.getNodeFullID();
	j["name"] = type.name.toStdString();
	j["address"] = type.address.toStdString();
    return j;
}
