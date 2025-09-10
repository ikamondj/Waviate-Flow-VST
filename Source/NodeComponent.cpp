#include "NodeComponent.h"
#include "PluginEditor.h"
#include "NodeType.h"
#include "DrawingUtils.h"


NodeComponent::NodeComponent(NodeData nodeData, const NodeType& nodeType, SceneComponent& scene)
	: node(nodeData), type(nodeType), owningScene(scene)
{
    setMouseClickGrabsKeyboardFocus(false);
    setWantsKeyboardFocus(true);
    updateSize();
}
// Is the mouse over any input pin? (local coordinates)
static int hitInputPinIndex(const NodeComponent& self, juce::Point<int> localP) {
    return self.getInputPinIndexAtPoint(localP);
}

// Is the mouse over this node's output pin? (local coordinates)
static bool hitOutputPin(const NodeComponent& self, juce::Point<int> localP) {
    return self.isOverOutputPin(localP) && self.getType().name != "output";
}

// Find a NodeComponent* under a scene-space point
NodeComponent* nodeAtScenePoint(SceneComponent* scene, juce::Point<int> sceneP) {
    if (!scene) return nullptr;
    auto* c = scene->getComponentAt(sceneP);
    return dynamic_cast<NodeComponent*>(c);
}

void NodeComponent::paint(juce::Graphics& g)
{
    const double scale = std::pow(2.0, owningScene.logScale);
    const float sides = 20.0f * (float)scale;
    auto rect = getLocalBounds().toFloat();
    float cornerSize = 24.0f * (float)scale;
    rect = rect.withLeft(rect.getX() + sides).withRight(rect.getRight() - sides);

    
    g.setColour(juce::Colour::fromHSV(colorHash(getType().address), this == owningScene.highlightedNode ? .9f : .5f, this == owningScene.highlightedNode ? .9f : .45f, 1.f));
    g.fillRoundedRectangle(rect, cornerSize);

    if (this == owningScene.highlightedNode) {
        g.setColour(juce::Colours::darkgrey.brighter(0.3f));
    }
    else {
        g.setColour(juce::Colours::darkgrey);
    }
    g.fillRect(rect.withBottom(rect.getBottom() - cornerSize).withTop(rect.getTopLeft().y + cornerSize));

    g.setColour(juce::Colours::white);
    g.setFont(14.0f * (float)scale);
    g.drawText(getType().name, getLocalBounds().reduced(4), juce::Justification::centredTop);

    const auto& t = getType();

    // pin metrics
    const float pinRadius = 9.0f * (float)scale;
    const float pinDiameter = pinRadius * 2.0f;
    const float pinSpacing = 20.0f * (float)scale;
    const float topOffset = 30.0f * (float)scale;

    // helpers for colors
    auto inputOutlineColour = [&](int idx) -> juce::Colour {
        const auto& f = t.inputs[(size_t)idx];
        return f.requiresCompileTimeKnowledge || getNodeDataConst().needsCompileTimeInputs() ? juce::Colours::mediumvioletred.darker() : juce::Colours::black;
        };
    auto outputOutlineColour = [&]() -> juce::Colour {
        return getNodeData().isCompileTimeKnown() ? juce::Colours::black : juce::Colours::mediumvioletred.darker();
        };
    auto inputFillColour = [&](int idx) -> juce::Colour {
        const auto& f = t.inputs[(size_t)idx];
        auto inp = getNodeDataConst().getInput(idx);
        return (f.requiredSize == 1 || (inp && inp->compileTimeSizeReady(getOwningScene()) && inp->getCompileTimeSize(getOwningScene()) == 1)) ? juce::Colours::lawngreen.darker(.4f) : juce::Colours::gold;
        };
    auto outputFillColour = [&]() -> juce::Colour {
        if (getNodeData().compileTimeSizeReady(getOwningScene())) {
            return getNodeData().isSingleton(getOwningScene()) ? juce::Colours::lawngreen.darker(.4f)
                : juce::Colours::gold;
        }
            
            return juce::Colours::lawngreen.darker(.4f);
        };

    // draw input pins + labels
    g.setFont(10.0f * (float)scale);
    auto trueType = getNodeDataConst().getTrueType();
    for (int i = 0; i < (int)t.inputs.size(); ++i) {
        const auto& in = t.inputs[(size_t)i];
        
        InputType inputType = in.inputType;
        const float yCenter = topOffset + i * pinSpacing + pinRadius;

        const float cx = rect.getX() - sides * 0.5f;               // left gutter center
        const float cy = yCenter;

        juce::Rectangle<float> pinBounds(cx - pinRadius, cy - pinRadius, pinDiameter, pinDiameter);
        juce::Rectangle<float> inner = pinBounds.reduced(4.0f * (float)scale);

        g.setColour(inputOutlineColour(i));
        if (inputType == InputType::any) {
			auto inputNode = getNodeDataConst().getInput((size_t)i);
            if (inputNode && inputNode->getType()->outputType != InputType::followsInput) {
				inputType = inputNode->getType()->outputType;
            }
            else if (getType().outputType == InputType::followsInput) {
                if (!getNodeDataConst().outputs.empty()) {
                    for (auto& [c,indx] : getNodeDataConst().outputs) {
                        auto dit = c->getType()->inputs[indx].inputType;
                        if (dit != InputType::any) {
                            inputType = dit;
                        }
                    }
                }
            }
        }
        g.setColour(inputOutlineColour(i));
        g.fillEllipse(pinBounds);


        g.setColour(inputFillColour(i));
        auto renderedType = inputType == InputType::any && trueType != InputType::dirty ? trueType : inputType;
        switch (renderedType) {
        case InputType::boolean: g.fillRect(inner); break;
        case InputType::decimal: g.fillEllipse(inner); break;
        case InputType::integer: fillHexagon(g, inner); break;
		case InputType::any: 
		case InputType::followsInput: fillStar(g, pinBounds); break;
        }

        const int textX = (int)(rect.getX() - (sides - pinDiameter) * 0.5f + pinDiameter + 2.0f);
        const int textY = (int)(yCenter - 7.0f);
        g.setColour(juce::Colours::white);
        g.drawText(in.name, textX, textY, getWidth() - textX - 4, 14, juce::Justification::left, true);
    }

    // draw single output pin (right side)
    if (getType().name != "output") {
        InputType inputType = getType().outputType;
        const float yCenter = topOffset + pinRadius;               // match your old output placement
        const float cx = rect.getRight() + sides * 0.5f;           // right gutter center
        const float cy = yCenter;

        juce::Rectangle<float> pinBounds(cx - pinRadius, cy - pinRadius, pinDiameter, pinDiameter);
        juce::Rectangle<float> inner = pinBounds.reduced(4.0f * (float)scale);

        g.setColour(outputOutlineColour());
        if (inputType == InputType::followsInput) {
            if (!getNodeDataConst().outputs.empty()) {
                for (auto& [c, indx] : getNodeDataConst().outputs) {
                    auto dit = c->getType()->inputs[indx].inputType;
                    if (dit != InputType::any) {
                        inputType = dit;
                    }
                }
            }
            else {
                const auto& input = getType().inputs[getType().whichInputToFollowWildcard];
                if (input.inputType == InputType::any) {
                    auto inputNode = getNodeDataConst().getInput((size_t)getType().whichInputToFollowWildcard);
                    if (inputNode && inputNode->getType()->outputType != InputType::followsInput) {
                        inputType = inputNode->getType()->outputType;
                    }
                    else {
                        inputType = InputType::any;
                    }
                }
            }
            
        }
        g.fillEllipse(pinBounds);

        g.setColour(outputFillColour());
        auto renderedType = inputType == InputType::followsInput && trueType != InputType::dirty ? trueType : inputType;
        switch (inputType) {
        case InputType::boolean: g.fillRect(inner); break;
        case InputType::decimal: g.fillEllipse(inner); break;
        case InputType::integer: fillHexagon(g, pinBounds); break;
        case InputType::followsInput:
		case InputType::any: 
            fillStar(g, pinBounds); 
            break;
        }
    }
}


int NodeComponent::getInputPinIndexAtPoint(juce::Point<int> p) const
{
    const double scale = std::pow(2.0, owningScene.logScale);
    const float  sides = 20.0f * (float)scale;
    const float  pinRadius = 9.0f * (float)scale;   // match paint()
    const float  pinSpacing = 20.0f * (float)scale;
    const float  topOffset = 30.0f * (float)scale;
    const float  xCenter = sides * 0.5f;           // left gutter center
    const float  hitPad = 3.0f * (float)scale;   // click forgiveness

    for (int i = 0; i < (int)type.inputs.size(); ++i)
    {
        const float yCenter = topOffset + i * pinSpacing + pinRadius;
        const float dx = (float)p.x - xCenter;
        const float dy = (float)p.y - yCenter;
        const float r = pinRadius + hitPad;
        if (dx * dx + dy * dy <= r * r) return i;
    }
    return -1;
}

bool NodeComponent::isOverOutputPin(juce::Point<int> p) const
{
    const double scale = std::pow(2.0, owningScene.logScale);
    const float  sides = 20.0f * (float)scale;
    const float  pinRadius = 9.0f * (float)scale;   // match paint()
    const float  topOffset = 30.0f * (float)scale;
    const float  xCenter = (float)getWidth() - sides * 0.5f;   // right gutter center
    const float  yCenter = topOffset + pinRadius;              // same row as paint()
    const float  hitPad = 3.0f * (float)scale;

    const float dx = (float)p.x - xCenter;
    const float dy = (float)p.y - yCenter;
    const float r = pinRadius + hitPad;
    return dx * dx + dy * dy <= r * r;
}


double NodeComponent::colorHash(juce::String s)
{
    double d = 0;
    for (int i = 0; i < s.length(); i += 1) {
        d += s[i] * 17.0 + sin(s[i]) * 12.0;
    }
    return d;
}

juce::String ddtypeToString(ddtype d, InputType type) {
	if (type == InputType::decimal) {
		return juce::String(d.d);
	}
	else if (type == InputType::boolean) {
		return d.i == 0 ? "false" : "true";
	}
	else { // integer
		return juce::String(d.i);
	}
}

void NodeComponent::mouseDown(const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    if (getProcessorRef().getCurrentEditor()) {
        getProcessorRef().getCurrentEditor()->deselectBrowserItem();
    }

    auto* scene = getOwningScene();

    const int overInput = hitInputPinIndex(*this, e.getPosition());
    const bool overOutput = hitOutputPin(*this, e.getPosition());

    // --- Right click: pin tooltips or node menu ---
    if (e.mods.isRightButtonDown()) {
        // If we're on a pin, show the pin tooltip logic (from old Pin::mouseDown right-click)
        if (overOutput || overInput >= 0) {
            juce::PopupMenu tooltip;

            if (overOutput) {
                // OUTPUT tooltip
                int size = getNodeDataConst().getCompileTimeSize(scene);
                tooltip.addItem(juce::String("output size: ") + juce::String(size), false, false, []() {});
                if (getNodeDataConst().isCompileTimeKnown()) {
                    auto field = getNodeDataConst().getCompileTimeValue(getOwningScene());
                    juce::String result = "value: { ";
                    for (int i = 0; i < (int)field.size(); ++i) {
                        result += ddtypeToString(field[i], getType().outputType);
                        result += juce::String(", ");
                    }
                    result += juce::String("}");
                    tooltip.addItem(result, false, false, []() {});
                }
            }
            else {
                // INPUT tooltip
                orableBool unknown; // matches your existing API patterns
                auto* inputNode = getNodeData().getInput(overInput);
                const int inputSize = inputNode ? inputNode->getCompileTimeSize(scene) : 1;
                const int require = getNodeDataConst().getType()->inputs[overInput].requiredSize;

                if (require != 0) {
                    tooltip.addItem(juce::String("required size: ") + juce::String(require), false, false, []() {});
                }
                if (!inputNode) {
                    const auto& inp = getNodeDataConst().getType()->inputs[overInput];
                    ddtype value = inp.defaultValue;
					juce::String v = inp.inputType == InputType::decimal ? juce::String(value.d)
						: inp.inputType == InputType::boolean ? (value.i == 0 ? juce::String("false") : juce::String("true"))
						: juce::String(value.i);
                    tooltip.addItem(juce::String("value: ") + v, false, false, []() {});
                }
                else if (inputNode->isCompileTimeKnown()) {
                    auto field = inputNode->getCompileTimeValue(getOwningScene());
                    juce::String result = "value: { ";
                    for (int i = 0; i < (int)field.size(); ++i) {
                        result += ddtypeToString(field[i], inputNode->getType()->outputType);
                        result += juce::String(", ");
                    }
                    result += juce::String("}");
                    tooltip.addItem(result, false, false, []() {});
                }
                tooltip.addItem(juce::String("input size: ") + juce::String(inputSize), false, false, []() {});
            }

            tooltip.showMenuAsync(juce::PopupMenu::Options());
            return;
        }

        // Fallback to existing node menu if not on a pin
        juce::PopupMenu options;
        options.addItem(getType().tooltip, false, false, []() {});
        if (getType().name.toLowerCase() != "output") {
            options.addItem("Delete Node", [this]() { owningScene.deleteNode(this); });
        }
        options.showMenuAsync({});
        return;
    }

    // --- Left button: detach or start drag (pin or node) ---
    if (e.mods.isLeftButtonDown()) {
        // Alt+click detach, mirroring old Pin behavior
        if (e.mods.isAltDown()) {
            if (overOutput) {
                // Detach any inputs in the scene that are fed by THIS node's output
                for (std::unique_ptr<NodeComponent>& comp : scene->nodes) {
                    auto& data = comp->getNodeData();
                    for (int j = 0; j < data.getNumInputs(); ++j) {
                        if (data.getInput(j) == &getNodeData()) {
                            data.detachInput(j);
                            scene->repaint();
                            return; // detach only once
                        }
                    }
                }
            }
            else if (overInput >= 0) {
                getNodeData().detachInput(overInput);
                scene->repaint();
                return; // detach only once
            }
            // fallthrough if Alt but not over a pin -> treat as normal node drag
        }

        // If we clicked a pin, start a connection drag (like Pin::mouseDown left branch)
        if (overOutput || overInput >= 0) {
            pinDragging = true;
            pinDragFromOutput = overOutput;
            pinDragInputIndex = overOutput ? -1 : overInput;

            if (overOutput) {
                pinDragStartScenePos = getOutputPinScenePos();
                pinDragCurrentScenePos = pinDragStartScenePos;
            }
            else {
                pinDragStartScenePos = getInputPinScenePos(overInput);
                pinDragCurrentScenePos = pinDragStartScenePos;
            }


            scene->repaint();
            return; // don't start moving the node in this gesture
        }

        // Otherwise: this is a node move gesture (your original behavior)
        hoveredInputIndex = overInput; // normally -1 here
        draggingConnection = false;
        dragStartPos = getPosition();
        toFront(true);
        owningScene.highlightedNode = this;
        repaint();
        owningScene.repaint();
    }
}


void NodeComponent::mouseDrag(const juce::MouseEvent& e)
{
    auto* scene = getOwningScene();

    // If we're dragging a connection line from a pin
    if (pinDragging) {
        pinDragCurrentScenePos = e.getEventRelativeTo(scene).getPosition().toFloat();
        scene->repaint(); // let the scene show the rubber-band or just refresh
        return;
    }

    // Otherwise: move node (original behavior)
    if (e.mods.isLeftButtonDown()) {
        auto delta = e.getOffsetFromDragStart();
        auto newPos = dragStartPos + delta;
        setTopLeftPosition(newPos);

        owningScene.repaint();
    }
}


void NodeComponent::mouseUp(const juce::MouseEvent& e)
{
    auto* scene = getOwningScene();

    if (pinDragging) {
        // End of a connection drag
        pinDragging = false;

        // Locate potential target node under the cursor (scene coords)
        const auto sceneClick = e.getEventRelativeTo(scene).getPosition();
        NodeComponent* targetNode = nodeAtScenePoint(scene, sceneClick);

        if (targetNode && targetNode != this) {
            // Hit-test target pins in its LOCAL coordinates
            juce::Point<int> localToTarget = sceneClick - targetNode->getPosition();

            const int  targetInput = hitInputPinIndex(*targetNode, localToTarget);
            const bool targetOverOutput = hitOutputPin(*targetNode, localToTarget);

            if (pinDragFromOutput) {
                // Dragging from THIS output -> must land on OTHER node's input
                if (targetInput >= 0) {
                    targetNode->getNodeData().attachInput(
                        targetInput,
                        &this->getNodeData(),
                        *scene
                    );
                }
            }
            else {
                // Dragging from THIS input -> can land on OTHER node's output
                if (targetOverOutput && pinDragInputIndex >= 0) {
                    this->getNodeData().attachInput(
                        pinDragInputIndex,
                        &targetNode->getNodeData(),
                        *scene
                    );
                }
                else if (pinDragInputIndex >= 0) {
                    // Dropped in empty space: detach and open menu (old Pin behavior)
                    this->getNodeData().detachInput(pinDragInputIndex);
                    scene->showMenu(e.position.toInt() + getPosition(), &this->getNodeData(), pinDragInputIndex);
                }
            }
        }
        else {
            // No target node: if drag started from an input, detach + menu
            std::function<bool(const NodeType& t)> condition = [](const NodeType& t) {return true; };
            if (pinDragInputIndex >= 0) {
                this->getNodeData().detachInput(pinDragInputIndex);
                condition = [&](const NodeType& t) {
                    if (this->getType().inputs[pinDragInputIndex].requiresCompileTimeKnowledge || this->getNodeDataConst().needsCompileTimeInputs()) {
                        if (t.alwaysOutputsRuntimeData) {
                            return false;
                        }
                    }
                    return true;
                    };
            }
            else {
                condition = [](const NodeType& t) { return t.inputs.size() >= 1; };
            }
            scene->showMenu(e.position.toInt() + getPosition(), &this->getNodeData(), pinDragInputIndex, condition);
        }

        owningScene.repaint();
        hoveredInputIndex = -1;
        draggingConnection = false;
        return;
    }

    // Node drag completed (original behavior)
    draggingConnection = false;
    hoveredInputIndex = -1;

    if (getType().isInputNode) {
        getOwningScene()->editNodeType();
        owningScene.onSceneChanged();
    }

    owningScene.repaint();
}


void NodeComponent::mouseDoubleClick(const juce::MouseEvent& event)
{
    if (auto scene = getType().fromScene) {
        scene->setVisible(true);
        scene->toFront(true); // bring to front
        getProcessorRef().activeScene = scene;
        if (getProcessorRef().getCurrentEditor()) {
            getProcessorRef().getCurrentEditor()->browser.repaint();
        }
        
    }
}

void NodeComponent::updateSize()
{
    double currentLogScale = owningScene.logScale;
    double scale = std::pow(2.0, currentLogScale);
    double y = getType().inputs.size() < 1 ? 80 : 80 + (getType().inputs.size() - 1) * 20;
    double x = 50 + getType().name.length() * 10 + 40;
    if (!inputGUIElements.empty()) {
        auto t = dynamic_cast<juce::TextEditor*>(inputGUIElements.back().get());
        if (t) {
            double xx = 50 + t->getText().length() * 10 + 40;
            if (xx > x) x = xx;
        }
    }
    
    x *= scale;
    y *= scale;
    setSize(x, y);
}

bool NodeComponent::keyPressed(const juce::KeyPress& k)
{
    auto editor = getOwningScene()->processorRef.getCurrentEditor();
    if (editor) {
        return editor->keyPressed(k);
    }
    return false;
}

NodeData& NodeComponent::getNodeData() { return node; }
const NodeData& NodeComponent::getNodeDataConst() const { return node; }


juce::Point<float> NodeComponent::getOutputPinScenePos() const
{
    const double scale = std::pow(2.0, owningScene.logScale);
    const float  sides = 20.0f * (float)scale;
    const float  pinRadius = 9.0f * (float)scale;
    const float  topOffset = 30.0f * (float)scale;

    const float xLocal = (float)getWidth() - sides * 0.5f;               // right gutter center
    const float yLocal = topOffset + pinRadius;

    auto nodePos = getPosition().toFloat();                              // -> scene
    return { nodePos.x + xLocal, nodePos.y + yLocal };
}

juce::Point<float> NodeComponent::getInputPinScenePos(int inputIndex) const
{
    const double scale     = std::pow(2.0, owningScene.logScale);
    const float  sides     = 20.0f * (float)scale;
    const float  pinRadius = 9.0f  * (float)scale;
    const float  pinSpacing= 20.0f * (float)scale;
    const float  topOffset = 30.0f * (float)scale;

    const float xLocal = sides * 0.5f;                                  // left gutter center
    const float yLocal = topOffset + inputIndex * pinSpacing + pinRadius;

    auto nodePos = getPosition().toFloat();                              // -> scene
    return { nodePos.x + xLocal, nodePos.y + yLocal };
}

SceneComponent* NodeComponent::getOwningScene() const { 
    return &owningScene; 
}


const NodeType& NodeComponent::getType() const
{
    return type;
}

void NodeComponent::resized()
{
    getType().onResized(*this);
}

WaviateFlow2025AudioProcessor& NodeComponent::getProcessorRef()
{
    return getOwningScene()->processorRef;
}


bool NodeComponent::getIsPinDragging() const { return pinDragging; }

