

#include "Validator.h"
#include <optional>
#include <pybind11/pybind11.h>
#include "Serializer.h"
#include <nlohmann/json.hpp>
#include "NodeType.h"
#include "SceneData.h"

bool isValidType(const NodeType& type, const SceneData& scene) {
    // Implement your validation logic here
    if (type.outputType == InputType::any || type.outputType == InputType::dirty) {return false;}
    return true; // Placeholder
}

bool validateSerializedScene(const nlohmann::json& json) {
    // Validate all nodes in graph don't have circular dependencies (cycles)
    // Build adjacency list from JSON
    const auto& nodes = json["nodes"];
    std::vector<std::vector<int>> adj(nodes.size());
    for (size_t i = 0; i < nodes.size(); ++i) {
        const auto& inputs = nodes[i]["inputs"];
        for (const auto& inputIdx : inputs) {
            if (inputIdx.is_number_integer() && inputIdx.get<int>() >= 0) {
                int idx = inputIdx.get<int>();
                if (idx >= 0 && static_cast<size_t>(idx) < nodes.size()) {
                    adj[i].push_back(idx);
                }
            }
        }
    }

    // DFS to detect cycles
    enum class State { Unvisited, Visiting, Visited };
    std::vector<State> state(nodes.size(), State::Unvisited);
    std::function<bool(int)> hasCycle = [&](int v) {
        if (state[v] == State::Visiting) return true; // found a cycle
        if (state[v] == State::Visited) return false;
        state[v] = State::Visiting;
        for (int u : adj[v]) {
            if (hasCycle(u)) return true;
        }
        state[v] = State::Visited;
        return false;
    };
    for (size_t i = 0; i < nodes.size(); ++i) {
        if (state[i] == State::Unvisited && hasCycle(i)) {
            return false; // cycle detected
        }
    }

    //TODO validate all node dependencies on other marketplace entries don't have circular dependencies
    std::optional<NodeType> optType = Serializer::deserialize(json);
    if (!optType.has_value()) { return false; }
    const NodeType& type = optType.value();
    const SceneData* fromScenePtr = type.fromScene;
    if (!fromScenePtr) { return false; }

    const SceneData& fromScene = *fromScenePtr;
    return isValidType(type, fromScene);
}

PYBIND11_MODULE(json_validator, m) {
    m.def("validate_json", &validateSerializedScene, "Validate JSON string");
}