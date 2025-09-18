/*
  ==============================================================================

    Serializer.cpp
    Created: 31 Aug 2025 9:42:01pm
    Author:  ikamo

  ==============================================================================
*/

#include "Serializer.h"
#include "NodeType.h"
#include "SceneData.h"
#include "PluginProcessor.h"
#include "NodeComponent.h"
std::optional<nlohmann::json> Serializer::serializeToJson(SceneData& scene, const WaviateFlow2025AudioProcessor& processor)
{
    nlohmann::json j;
	const auto& type = scene.customNodeType;
    if (scene.isLocalOnly) {
        j["id"] = type.getNodeId();
		j["userId"] = type.getNodeUserID();
    }
    else {
        if (processor.currentLogin.has_value()) {
            j["author"] = processor.currentLogin->username.toStdString();
			j["userId"] = processor.currentLogin->userId;
			j["id"] = type.getNodeId();
        }
    }
    j["name"] = type.name.toStdString();
    j["address"] = type.address.toStdString();
    int sceneIndex = -1;
    std::map<int, NodeData*> sceneIndexToNode;
	std::map<NodeData*, int> nodeToSceneIndex;
    std::set<std::array<uint64_t, 2>> dependencies;
	nlohmann::json nodes = nlohmann::json::array();
    for (auto n : scene.nodeDatas) {
        nlohmann::json nn;
		nn["id"] = n->getType()->getNodeId();
		nn["userId"] = n->getType()->getNodeUserID();
        nn["sceneIndex"] = ++sceneIndex;
		sceneIndexToNode.insert({ sceneIndex, n });
		nodeToSceneIndex.insert({ n, sceneIndex });
        nodes.push_back(nn);
    }

    sceneIndex = -1;
    for (auto n : scene.nodeDatas) {
        auto& nn = nodes.at(++sceneIndex);
		nlohmann::json inputs = nlohmann::json::array();
		for (int i = 0; i < n->getNumInputs(); ++i) {
			auto input = n->getInput(i);
			if (input) {
				inputs.push_back(nodeToSceneIndex.at(input));
			}
            else {
                inputs.push_back(-1);
            }
		}
        nn["inputs"] = inputs;
    }
	j["nodes"] = nodes;
    
    
    return j;
}

std::optional<NodeType> Serializer::deserialize(const std::string& rep) {
    return {}; //TODO
}
