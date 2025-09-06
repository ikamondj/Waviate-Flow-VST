/*
  ==============================================================================

    Pin.cpp
    Created: 11 Aug 2025 9:22:42pm
    Author:  ikamo

  ==============================================================================
*/

#include "Pin.h"
#include "NodeType.h"
#include "NodeComponent.h"
#include "SceneComponent.h"
#include "PluginEditor.h"
Pin::Pin(int inputNum, NodeComponent* attachedComponent)
{
	inputNumber = inputNum;
	attachedComponent = attachedComponent;
	setSize(20, 20);
}

int Pin::getInputNumber() const
{
	return inputNumber;
}

NodeComponent* Pin::getAttachedComponent() const
{
	return owningComponent;
}

void Pin::setInputNumber(int newInputNumber)
{
	inputNumber = newInputNumber;
}

void Pin::setAttachedComponent(NodeComponent* newAttachedComponent)
{
	owningComponent = newAttachedComponent;
}

juce::Colour Pin::outlineColorFromType() const {
	if (inputNumber < 0) {
		return owningComponent->getNodeData().isCompileTimeKnown() ? juce::Colours::black : juce::Colours::red;
	}
	else {
		const auto& features = owningComponent->getType().inputs[inputNumber];
		return features.requiresCompileTimeKnowledge ? juce::Colours::darkred : juce::Colours::black;
	}
}

juce::Colour Pin::pinColorFromType() const {
	juce::Colour result;
	if (inputNumber < 0) {
		try {
			result = owningComponent->getNodeData().isSingleton(nullptr) ? juce::Colours::lawngreen.darker(.4) : juce::Colours::gold;

		}
		catch (const std::logic_error& e) {
			result = juce::Colours::lawngreen.darker(.4);
		}
	}
	else {
		const auto& features = owningComponent->getType().inputs[inputNumber];
		NodeData* connectedUpstream = owningComponent->getNodeData().getInput(inputNumber);
		result = features.requiredSize == 1 ? juce::Colours::lawngreen.darker(.4) : juce::Colours::gold;
	}
	return result;
}

void Pin::reposition()
{
	
	juce::Point<int> localToNodePos;

	auto comp = getAttachedComponent();
	if (comp) {
		if (getInputNumber() >= 0) // only input pins
		{
			localToNodePos = comp->getInputPinScenePos(getInputNumber()).toInt();
		}
		else {
			localToNodePos = comp->getOutputPinScenePos().toInt();
		}
		setTopLeftPosition(localToNodePos + comp->getPosition() - juce::Point<int>(getWidth(), getHeight()) / 2);
	}
	toFront(false);
}

void Pin::updateSize() {
	double logScale = owningComponent->getOwningScene()->logScale;
	double scale = std::pow(2.0, logScale);
	double diameter = 20 * scale;
	setSize(diameter, diameter);
}


void Pin::paint(juce::Graphics& g)
{
	if (inputNumber < 0) {
		if (owningComponent->getNodeData().getType()->isBoolean) {
			g.setColour(outlineColorFromType());
			g.fillRect(getLocalBounds().toFloat());
			g.setColour(pinColorFromType());
			juce::Rectangle<float> rectArea = getLocalBounds().toFloat().reduced(4.0f);
			g.fillRect(rectArea);
		}
		else {
			g.setColour(outlineColorFromType());
			g.fillEllipse(getLocalBounds().toFloat());
			g.setColour(pinColorFromType());
			juce::Rectangle<float> ellipseArea = getLocalBounds().toFloat().reduced(4.0f);
			g.fillEllipse(ellipseArea);
		}
	}
	else if (!owningComponent->getType().inputs[inputNumber].isBoolean) {
		g.setColour(outlineColorFromType());
		g.fillEllipse(getLocalBounds().toFloat());
		g.setColour(pinColorFromType());
		juce::Rectangle<float> ellipseArea = getLocalBounds().toFloat().reduced(4.0f);
		g.fillEllipse(ellipseArea);
	}
	else {
		g.setColour(outlineColorFromType());
		g.fillRect(getLocalBounds().toFloat());
		g.setColour(pinColorFromType());
		juce::Rectangle<float> rectArea = getLocalBounds().toFloat().reduced(4.0f);
		g.fillRect(rectArea);
	}
	
}

void Pin::mouseDown(const juce::MouseEvent& e)
{
	owningComponent->getProcessorRef().getCurrentEditor()->deselectBrowserItem();
	if (e.mods.isLeftButtonDown()) {
		if (e.mods.isAltDown()) {
			if (inputNumber < 0) {
				auto scene = owningComponent->getOwningScene();
				for (std::unique_ptr<NodeComponent>& comp : scene->nodes) {
					auto& data = comp->getNodeData();
					for (int j = 0; j < data.getNumInputs(); j += 1) {
						if (data.getInput(j) == &owningComponent->getNodeData()) {
							data.detachInput(j);
							scene->repaint();
							return; // detach only once
						}
					}
				}
			}
			else {
				owningComponent->getNodeData().detachInput(inputNumber);
				owningComponent->getOwningScene()->repaint();
				return; // detach only once
			}
		}
		isDragging = true;
		auto scene = owningComponent->getOwningScene();
		dragStartPos = e.getEventRelativeTo(scene).getMouseDownPosition().toFloat();
		dragCurrentPos = dragStartPos; // start at same point
		scene->repaint();
	}
	else if (e.mods.isRightButtonDown()) {
		juce::PopupMenu tooltip;
		if (inputNumber < 0) {
			orableBool unknown;
			int size = owningComponent->getNodeData().getCompileTimeSize(owningComponent->getOwningScene());
			tooltip.addItem(juce::String("output size: " ) + juce::String(size), false, false, []() {});
		}
		else {
			orableBool unknown;
			auto input = owningComponent->getNodeData().getInput(inputNumber);

			int size = input ? 
				input->getCompileTimeSize(owningComponent->getOwningScene()) : 1;
			int require = owningComponent->getNodeDataConst().getType()->inputs[inputNumber].requiredSize;
			if (require != 0) {
				tooltip.addItem(juce::String("required size: ") + juce::String(require), false, false, []() {});
			}
			if (!input) {
				double value = owningComponent->getNodeDataConst().getType()->inputs[inputNumber].defaultValue;
				tooltip.addItem(juce::String("value: ") + juce::String(value), false, false, []() {});
			}
			else if (input->isCompileTimeKnown()) {
				auto processor = &owningComponent->getProcessorRef();

				juce::String result = "value: { ";
				RunnerInput& imp = owningComponent->getProcessorRef().getBuildableRunner();
				if (!Runner::containsNodeField(input, imp.nodeOwnership)) {
					auto field = Runner::getNodeField(input, imp.nodeOwnership);
					for (int i = 0; i < field.size(); i += 1) {
						result += juce::String(field[i]);
						result += juce::String(", ");
					}
					result += juce::String("}");
					tooltip.addItem(result, false, false, []() {});
				}
			}
			tooltip.addItem(juce::String("input size: ") + juce::String(size), false, false, []() {});

		}
		tooltip.showMenuAsync(juce::PopupMenu::Options());
	}
}

void Pin::mouseDrag(const juce::MouseEvent& e)
{
	auto scene = owningComponent->getOwningScene();
	dragCurrentPos = e.getEventRelativeTo(scene).getPosition().toFloat();
	scene->repaint(); // repaint directly in scene coords
}

juce::Point<float> Pin::getCentreInSceneCoords() const
{
	auto scene = owningComponent->getOwningScene();
	return scene->getLocalPoint(this, getLocalBounds().getCentre()).toFloat();
}



void Pin::mouseUp(const juce::MouseEvent& e)
{
	auto scene = owningComponent->getOwningScene();
	if (isDragging) {

		isDragging = false;
		// Here you would handle connecting the pin to another pin if applicable
		// Find target node under mouse
		auto* target = dynamic_cast<Pin*>(scene->getComponentAt(e.getScreenPosition() - scene->getScreenPosition()));

		if (target && target != this)
		{
			if (inputNumber < 0 && target->getInputNumber() >= 0)
				target->owningComponent->getNodeData().attachInput(target->getInputNumber(), &owningComponent->getNodeData(), *owningComponent->getOwningScene());
			else if (inputNumber >= 0 && target->getInputNumber() < 0) {
				this->owningComponent->getNodeData().attachInput(inputNumber, &target->owningComponent->getNodeData(),*owningComponent->getOwningScene());
			}
		}
		else {
			if (inputNumber >= 0) {
				owningComponent->getNodeData().detachInput(inputNumber);
				
			}
			scene->showMenu(e.position.toInt() + getPosition(), &owningComponent->getNodeData(), inputNumber);
		}
	}
	scene->repaint();
}

bool Pin::getIsDragging() const { 
	return isDragging; 
}

juce::Point<float> Pin::getDragCurrentScreenPos() const
{
	return dragCurrentPos;
	
}

