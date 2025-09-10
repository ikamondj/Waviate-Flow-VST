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
#pragma once


struct SceneData; 
class SceneComponent : public juce::Component, public RunnerInput
{
public:
    SceneComponent(class WaviateFlow2025AudioProcessor& processor, const juce::String& name);
    SceneComponent(class WaviateFlow2025AudioProcessor& processor, const uint64_t fullId);
    ~SceneComponent();

    void constructWithName(const juce::String& name);
    juce::String getNameFromJson(const juce::String& json);
    juce::String getJsonFromFullID(const uint64_t fullId);
    void initSceneWithJson(const juce::String& json);


    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
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

	WaviateFlow2025AudioProcessor& processorRef;
	std::vector<std::unique_ptr<class NodeComponent>> nodes;
    NodeComponent* highlightedNode;
    bool canPlaceType(const NodeType& type);
    void editNodeType();
    

	NodeComponent& addNode(const NodeType& type, juce::Point<int> localPos);
    NodeComponent& addNode(const NodeType& type, juce::Point<int> localPos, NodeData* toConnect, int index);
	void deleteNode(NodeComponent* node);
    void ensureNodeConnectionsCorrect(RunnerInput*);
    void computeAllNodeWildCards();
    juce::TextButton deleteSceneButton{ "x" };
    juce::TextEditor sceneNameBox;
    void onSceneChanged();
    
    NodeType customNodeType;
    double logScale = 0;
    juce::PopupMenu buildNodeTypeMenuCache();
    juce::PopupMenu buildNodeTypeMenuCache(std::function<bool(const NodeType&)> condition);
private:
    void buildMenuRecursive(juce::PopupMenu& menu, const struct MenuNode& node, std::function<bool(const NodeType&)> condition);
    bool drawReady = false;
};
