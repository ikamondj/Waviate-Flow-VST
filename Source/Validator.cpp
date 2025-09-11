

#include "Validator.h"
#include <optional>
#include <pybind11/pybind11.h>
#include "Serializer.h"
#include "NodeType.h"
#include "SceneData.h"

bool isValidType(const NodeType& type, const SceneData& scene) {
    // Implement your validation logic here
    return true; // Placeholder
}

bool validateSerializedScene(const std::string& json) {
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