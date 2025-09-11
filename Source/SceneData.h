/*
  ==============================================================================

    Scene.h
    Created: 10 Sep 2025 4:38:50pm
    Author:  ikamo

  ==============================================================================
*/
#include <string>
#include <vector>
#include <memory>
#include "RunnerInput.h"
#include "NodeType.h"
#pragma once
class SceneData : public RunnerInput {
public:
    SceneData(const std::string& name);
    SceneData(const uint64_t userId, const uint64_t nodeId);
    virtual ~SceneData() = default;
    class WaviateFlow2025AudioProcessor* processorRef;
    virtual void constructWithName(const std::string & name);
    void getSortedInputNodes(std::vector<NodeData*>& nodesFilteredByInputAndSorted);
    std::string getNameFromJson(const std::string& json);
    std::string getJsonFromFullID(const uint64_t userId, const uint64_t nodeId);
    void initSceneWithJson(const std::string& json);
    std::vector<NodeData*> nodeDatas;
    
    void computeAllNodeWildCards();
    NodeType customNodeType;
    void editNodeType();
	const std::string& getSceneName() const;
	void setSceneName(const std::string& newName);
    bool isLocalOnly;
private:
    std::string sceneName;
    SceneData();
};