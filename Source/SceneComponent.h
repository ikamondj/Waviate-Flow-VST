/*
  ==============================================================================

    SceneComponent.h
    Created: 10 Aug 2025 12:38:50am
    Author:  ikamo

  ==============================================================================
*/
#include <JuceHeader.h>
#include "NodeType.h"
#include "RunnerInput.h"
#include "Runner.h"
#include "SceneData.h"
#pragma once



class SceneComponent : public juce::Component, public SceneData
{
public:
    SceneComponent(class WaviateFlow2025AudioProcessor& processor, const std::string& name);
    SceneComponent(class WaviateFlow2025AudioProcessor& processor, const uint64_t userId, const uint64_t nodeId);
    ~SceneComponent();

    void onSceneChanged();

	void constructWithName(const std::string& name) override;


    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;
    void showMenu(juce::Point<int>, class NodeData* toConnect, int index);
    void showMenu(juce::Point<int>, class NodeData* toConnect, int index, std::function<bool(const NodeType&)> condition);
    
    //resize override is next
	void resized() override;

    juce::Point<int> lastMiddleDragPos;
    bool middleDragging = false;
    juce::Point<int> dragStartMouse;
    juce::Point<int> dragStartScenePos;
    juce::Point<int> dragCurrentScenePos;
    juce::Point<int> currentMouse;
    bool isLeftDragging = false;
    void paint(juce::Graphics& g) override;
    std::vector<std::unique_ptr<class NodeComponent>> nodes; 
	
    std::vector<NodeComponent*> highlightedNodes;
	std::unordered_map<NodeData*, NodeComponent*> nodeToComponentMap;
    
    bool canPlaceType(const NodeType& type);
    
    

	NodeComponent& addNode(const NodeType& type, juce::Point<int> localPos, bool updateScene = true);
    NodeComponent& addNode(const NodeType& type, juce::Point<int> localPos, NodeData* toConnect, int index, bool updateScene = true);
	void deleteNode(NodeComponent* node, bool updateScene = true);
    void replaceNode(NodeComponent* node, const NodeType& newType);
    void ensureNodeConnectionsCorrect(RunnerInput*);
    
    juce::TextButton deleteSceneButton{ "x" };
    juce::TextEditor sceneNameBox;
    
    void setHighlightedNode(NodeComponent* n);
    NodeComponent* getHighlightedNode() const;


    
    double logScale = 0;
    juce::PopupMenu buildNodeTypeMenuCache();
    juce::PopupMenu buildNodeTypeMenuCache(std::function<bool(const NodeType&)> condition);
private:
    void buildMenuRecursive(juce::PopupMenu& menu, const struct MenuNode& node, std::function<bool(const NodeType&)> condition);
    bool drawReady = false;
    NodeComponent* highlightedNode;
};
