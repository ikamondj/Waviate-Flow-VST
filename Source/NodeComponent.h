#include <JuceHeader.h>
#include "NodeData.h"
#include "NodeType.h"
#include "RunnerInput.h"

#pragma once
class NodeComponent : public juce::Component
{
public:
    NodeComponent(NodeData nodeData, const NodeType& type, class SceneComponent& scene);

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;

    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent& event) override;
    void updateSize();
    bool keyPressed(const juce::KeyPress& k) override;
	NodeData& getNodeData();
    const NodeData& getNodeDataConst() const;
    juce::Point<float> getOutputPinScenePos() const;
    juce::Point<float> getInputPinScenePos(int inputIndex) const;
	class SceneComponent* getOwningScene() const;
    const NodeType& getType() const;
    void resized() override;
	std::vector<std::unique_ptr<juce::Component>> inputGUIElements;
    class WaviateFlow2025AudioProcessor& getProcessorRef();
    // public accessors the Scene can query (mimic old Pin API)
    bool getIsPinDragging() const;
    juce::Point<float> getPinDragCurrentScreenPos() const { return pinDragCurrentScenePos; }
    int getInputPinIndexAtPoint(juce::Point<int> p) const;
    bool isOverOutputPin(juce::Point<int> p) const;
    
	
    
    bool isPinDragFromOutput() const { return pinDragFromOutput; }
    int  getPinDragInputIndex() const { return pinDragInputIndex; } // -1 if from output
    juce::Point<float> getPinDragStartScenePos() const { return pinDragStartScenePos; }
    juce::Point<float> getPinDragCurrentScenePos() const { return pinDragCurrentScenePos; }
    double colorHash(juce::String s);
private:
    // --- Pin-drag state (mirrors old Pin fields) ---
    bool pinDragging = false;
    bool pinDragFromOutput = false;     // true = dragging from this node's output, false = from one of its inputs
    int  pinDragInputIndex = -1;        // valid only when pinDragFromOutput == false
    juce::Point<float> pinDragStartScenePos;
    juce::Point<float> pinDragCurrentScenePos;
    
    NodeData node;
    
    const NodeType& type;
    SceneComponent& owningScene;

    juce::Point<int> dragStartPos;
    bool draggingConnection = false;
    int hoveredInputIndex = -1;
    

    
};
