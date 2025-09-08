#include "SceneComponent.h"
#include "PluginEditor.h"
#include "NodeComponent.h"
#include "DrawingUtils.h"

SceneComponent::SceneComponent(WaviateFlow2025AudioProcessor& processor, const juce::String& name)
    : processorRef(processor), customNodeType(processorRef.getCurrentLoadedTypeIndex())
{
    customNodeType.setNodeId(processorRef.getCurrentLoadedUserIndex(), customNodeType.getNodeId());
    constructWithName(name);
}

SceneComponent::SceneComponent(WaviateFlow2025AudioProcessor& processor, const uint64_t fullId) : processorRef(processor), customNodeType(processorRef.getCurrentLoadedTypeIndex())
{
    customNodeType.setNodeId(processorRef.getCurrentLoadedUserIndex(), customNodeType.getNodeId());
    juce::String json = getJsonFromFullID(fullId);
    juce::String name = getNameFromJson(json);
    initSceneWithJson(json);
    constructWithName(name);
}

struct MenuNode
{
    juce::String name;
    int menuId = 0; // 0 means it's not a leaf
    const NodeType* typeRef;
    std::map<juce::String, MenuNode> children;
};

SceneComponent::~SceneComponent()
{
	// Clear all nodes and pins
	nodes.clear();
	highlightedNode = nullptr;
	drawReady = false;
}

void SceneComponent::constructWithName(const juce::String& name)
{
    setName(name);
    nodes.reserve(2000);
    addAndMakeVisible(deleteSceneButton);
    deleteSceneButton.onClick = [this]() {
        processorRef.deleteScene(this);
        };
    if (!processorRef.scenes.empty()) {
        sceneNameBox.setJustification(juce::Justification::centred);
        addAndMakeVisible(sceneNameBox);
        sceneNameBox.setText(name);

    }
    highlightedNode = nullptr;
    setSize(8192, 8192);
    setWantsKeyboardFocus(true);
}

juce::String SceneComponent::getNameFromJson(const juce::String& json)
{
    return juce::String();
}

juce::String SceneComponent::getJsonFromFullID(const uint64_t fullId)
{
    return juce::String();
}

void SceneComponent::initSceneWithJson(const juce::String& json)
{
}

void SceneComponent::mouseDown(const juce::MouseEvent& e)
{
    if (processorRef.getCurrentEditor()) {
        processorRef.getCurrentEditor()->deselectBrowserItem();
    }
    grabKeyboardFocus();
    
    if (this != processorRef.activeScene) {
        processorRef.activeScene = this;
    }
    this->highlightedNode = nullptr;
    if (e.mods.isRightButtonDown())
    {
        
        auto localPos = e.getPosition() - getPosition();
        showMenu(localPos, nullptr, -1);
    }
    else if (e.mods.isMiddleButtonDown())
    {
        middleDragging = true;
        dragStartMouse = e.getScreenPosition();
		dragStartScenePos = getPosition();
    }
    else if (e.mods.isLeftButtonDown()) {
        isLeftDragging = true;
        currentMouse = dragStartMouse = e.getPosition();
        
    }
    repaint();
    for (auto& node : nodes) {
        node->repaint();
    }
}

void SceneComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (middleDragging)
    {
        currentMouse = e.getScreenPosition();
        auto delta = currentMouse - dragStartMouse;

        // New unclamped position
        auto newPos = dragStartScenePos + delta;

        if (newPos.x > 0) {
            newPos.x = 0;
        }
        if (newPos.y > 0) {
            newPos.y = 0;
        }
        if (newPos.x < -getWidth() - getParentWidth()) {
            newPos.x = -getWidth() - getParentWidth();
        }
        if (newPos.y < -getHeight() - getParentHeight()) {
            newPos.y = -getHeight() - getParentHeight();
        }

            setTopLeftPosition(newPos);
    }
    else if (isLeftDragging) {
        currentMouse = e.getOffsetFromDragStart() + dragStartMouse;
        repaint();
    }
}

void SceneComponent::mouseUp(const juce::MouseEvent& e)
{
    if (middleDragging)
        middleDragging = false;

    if (isLeftDragging)
    {
        // Try to "cut" any connection whose bezier this drag line intersects
        juce::Line<float> dragLine(dragStartMouse.toFloat(), currentMouse.toFloat());

        for (const auto& compPtr : nodes)
        {
            NodeComponent* target = compPtr.get();
            auto& nodeData = target->getNodeData();

            for (int inputIndex = 0; inputIndex < nodeData.getNumInputs(); ++inputIndex)
            {
                NodeData* other = nodeData.getInput(inputIndex);
                if (!other || !other->owningComponent) continue;

                // Bezier endpoints in scene coords
                juce::Point<float> end = target->getInputPinScenePos(inputIndex);
                juce::Point<float> start = other->owningComponent->getOutputPinScenePos();

                float dx = std::max(40.0f, std::abs(end.x - start.x) / 2.0f);
                juce::Point<float> c1(start.x + dx, start.y);
                juce::Point<float> c2(end.x - dx, end.y);

                if (lineIntersectsBezier(dragLine, start, c1, c2, end))
                {
                    nodeData.detachInput(inputIndex);
                    // Matches your old behavior: detach only once per gesture
                    goto mouseUp_exitCutLoop;
                }
            }
        }

    mouseUp_exitCutLoop:
        isLeftDragging = false;
        repaint();
    }
}


void SceneComponent::mouseWheelMove(const juce::MouseEvent& event,
    const juce::MouseWheelDetails& wheel)
{
    const double prev = logScale;
    logScale = std::clamp(logScale + wheel.deltaY * .5, -1.5, 1.5);

    const double dLog = logScale - prev;
    if (dLog == 0.0)
        return;

    const double scaleFactor = std::pow(2.0, dLog);
    const double scale = std::pow(2.0, logScale);

    // anchor in SceneComponent's local coords (same space as node positions)
    const juce::Point<float> anchor = event.getPosition().toFloat();

    for (std::unique_ptr<NodeComponent>& comp : nodes)
    {
        // current top-left in scene coords
        juce::Point<float> p = comp->getBounds().getTopLeft().toFloat();

        // vector from anchor -> node
        juce::Point<float> v(p.x - anchor.x, p.y - anchor.y);

        // scaled position
        juce::Point<float> newP(anchor.x + v.x * (float)scaleFactor,
            anchor.y + v.y * (float)scaleFactor);

        comp->setTopLeftPosition((int)std::lround(newP.x),
            (int)std::lround(newP.y));
        
        comp->updateSize();
    }

    double size = 8192 * scale;
    
    setSize(size, size);
    repaint();
}

void SceneComponent::showMenu(juce::Point<int> localPos, NodeData* toConnect, int index)
{
    showMenu(localPos, toConnect, index, [](const NodeType&) {
        return true;
    });
}

void SceneComponent::showMenu(juce::Point<int> localPos, NodeData* toConnect, int index, std::function<bool(const NodeType&)> condition)
{
    bool isMainScene = (this == &processorRef.getScene());
    auto menu = buildNodeTypeMenuCache(condition);
    menu.showMenuAsync(
        juce::PopupMenu::Options(),
        [this, localPos, &menu, toConnect, index](int result)
        {
            if (result > 0)
            {
                int typeId = result;
                if (typeId < processorRef.getRegistry().size()) {
                    auto& type = processorRef.getRegistry()[typeId];
                    addNode(type, localPos, toConnect, index);
                }
                else {
                    typeId -= processorRef.getRegistry().size();
                    auto& type = processorRef.getScenes()[typeId]->customNodeType;
                    if (canPlaceType(type)) {
                        addNode(type, localPos, toConnect, index);
                    }
                }

            }
        });
}


void SceneComponent::resized()
{
    juce::Point<int> newPos = getPosition();
    if (newPos.x < -getWidth() - getParentWidth()) {
        newPos.x = -getWidth() - getParentWidth();
    }               
    if (newPos.y < -getHeight() - getParentHeight()) {
        newPos.y = -getHeight() - getParentHeight();
    }
    setTopLeftPosition(newPos);
}

void SceneComponent::paint(juce::Graphics& g)
{
    const int gridSpacing = 40;
    g.fillAll(juce::Colours::darkslategrey.darker(.6f));

    g.setColour(juce::Colours::darkgrey.withAlpha(0.5f));
    auto bounds = getLocalBounds();

    int startX = bounds.getX() % gridSpacing;
    int startY = bounds.getY() % gridSpacing;

    for (int x = startX; x < bounds.getWidth(); x += gridSpacing)
        g.drawLine((float)x, 0.0f, (float)x, (float)bounds.getHeight());

    for (int y = startY; y < bounds.getHeight(); y += gridSpacing)
        g.drawLine(0.0f, (float)y, (float)bounds.getWidth(), (float)y);

    if (isLeftDragging) {
        g.setColour(juce::Colours::red.withAlpha(0.5f));
        g.drawLine(dragStartMouse.x, dragStartMouse.y, currentMouse.x, currentMouse.y, 2);
    }

    const double scale = std::pow(2.0, logScale);

    // -------------------------------------
    // Helpers to mirror Pin color logic
    // -------------------------------------
    auto inputOutlineColour = [](NodeComponent* nc, int idx) -> juce::Colour {
        const auto& t = nc->getType();
        const auto& f = t.inputs[(size_t)idx];
        return f.requiresCompileTimeKnowledge ? juce::Colours::darkred : juce::Colours::black;
        };
    auto outputOutlineColour = [](NodeComponent* nc) -> juce::Colour {
        return nc->getNodeData().isCompileTimeKnown() ? juce::Colours::black : juce::Colours::red;
        };
    auto inputFillColour = [](NodeComponent* nc, int idx) -> juce::Colour {
        const auto& t = nc->getType();
        const auto& f = t.inputs[(size_t)idx];
        return (f.requiredSize == 1) ? juce::Colours::lawngreen.darker(.4f) : juce::Colours::gold;
        };
    auto outputFillColour = [](NodeComponent* nc) -> juce::Colour {
            if (nc->getNodeDataConst().compileTimeSizeReady(nc->getOwningScene())) {
                return nc->getNodeData().isSingleton(nc->getOwningScene()) ? juce::Colours::lawngreen.darker(.4f)
                    : juce::Colours::gold;
            }
            return juce::Colours::lawngreen.darker(.4f);
        };

    // -------------------------------------
    // Draw live rubber-band from any node currently pin-dragging
    // -------------------------------------
    g.setColour(juce::Colours::lightgreen);
    try
    {
        // -------------------------------------
// Draw live rubber-band from any node currently pin-dragging
// -------------------------------------
        for (const auto& compPtr : nodes)
        {
            NodeComponent* nc = compPtr.get();
            if (!nc || !nc->getIsPinDragging()) continue;

            const bool fromOutput = nc->isPinDragFromOutput();
            const int  fromInput = nc->getPinDragInputIndex(); // -1 if from output

            const juce::Point<float> anchorOut = nc->getOutputPinScenePos();
            const juce::Point<float> anchorIn = (fromInput >= 0) ? nc->getInputPinScenePos(fromInput) : anchorOut; // guard
            const juce::Point<float> mousePos = nc->getPinDragCurrentScenePos();

            juce::Point<float> start = fromOutput ? anchorOut : mousePos;
            juce::Point<float> end = fromOutput ? mousePos : anchorIn;

            const float dx = std::max(40.0f, std::abs(end.x - start.x) / 2.0f);
            juce::Point<float> c1(start.x + dx, start.y);
            juce::Point<float> c2(end.x - dx, end.y);

            juce::Colour outline = fromOutput
                ? (nc->getNodeData().isCompileTimeKnown() ? juce::Colours::black : juce::Colours::darkred)
                : (nc->getType().inputs[(size_t)fromInput].requiresCompileTimeKnowledge || nc->getNodeDataConst().needsCompileTimeInputs() ? juce::Colours::darkred : juce::Colours::black);

            juce::Colour inner = fromOutput
                ? [&] { try { return nc->getNodeData().isSingleton(nullptr) ? juce::Colours::lawngreen.darker(.4f) : juce::Colours::gold; }
            catch (...) { return juce::Colours::lawngreen.darker(.4f); } }()
                : ((nc->getType().inputs[(size_t)fromInput].requiredSize == 1) ? juce::Colours::lawngreen.darker(.4f) : juce::Colours::gold);

            const float thickOuter = 12.f * (float)std::pow(2.0, logScale);
            const float thickInner = 8.f * (float)std::pow(2.0, logScale);

            drawGradientBezier(g, start, c1, c2, end, outline, outline, thickOuter);
            drawGradientBezier(g, start, c1, c2, end, inner, inner, thickInner);
        }



        // -------------------------------------
        // Draw existing connections
        // -------------------------------------
        for (const auto& compPtr : nodes)
        {
            NodeComponent* pinNode = compPtr.get();
            if (!pinNode) continue;

            auto& nodeData = pinNode->getNodeData();
            const int numInputs = nodeData.getNumInputs();

            for (int inputIndex = 0; inputIndex < numInputs; ++inputIndex)
            {
                NodeData* other = nodeData.getInput(inputIndex);
                if (!other || !other->owningComponent) continue;

                NodeComponent* otherNode = other->owningComponent;

                juce::Point<float> end = pinNode->getInputPinScenePos(inputIndex);
                juce::Point<float> start = otherNode->getOutputPinScenePos();

                float dx = std::max(40.0f, std::abs(end.x - start.x) / 2.0f);
                juce::Point<float> c1(start.x + dx, start.y);
                juce::Point<float> c2(end.x - dx, end.y);

                // If we're drawing the red "will cut" overlay during a left drag
                if (isLeftDragging)
                {
                    juce::Line<float> dragLine(dragStartMouse.toFloat(), currentMouse.toFloat());
                    bool hit = lineIntersectsBezier(dragLine, start, c1, c2, end);

                    if (hit)
                    {
                        drawGradientBezier(g, start, c1, c2, end,
                            juce::Colours::red, juce::Colours::red, 12.f * (float)scale);
                        drawGradientBezier(g, start, c1, c2, end,
                            juce::Colours::red, juce::Colours::red, 8.f * (float)scale);
                    }
                    else
                    {
                        drawGradientBezier(g, start, c1, c2, end,
                            outputOutlineColour(otherNode),
                            inputOutlineColour(pinNode, inputIndex),
                            12.f * (float)scale);

                        drawGradientBezier(g, start, c1, c2, end,
                            outputFillColour(otherNode),
                            inputFillColour(pinNode, inputIndex),
                            8.f * (float)scale);
                    }
                }
                else
                {
                    drawGradientBezier(g, start, c1, c2, end,
                        outputOutlineColour(otherNode),
                        inputOutlineColour(pinNode, inputIndex),
                        12.f * (float)scale);

                    drawGradientBezier(g, start, c1, c2, end,
                        outputFillColour(otherNode),
                        inputFillColour(pinNode, inputIndex),
                        8.f * (float)scale);
                }
            }
        }
    }
    catch (const std::exception& e)
    {
        DBG("Exception in SceneComponent::paint: " << e.what());
    }
}


bool SceneComponent::canPlaceType(const NodeType& type)
{
    // Built-in / non-user-defined types are always safe
    if (!type.fromScene) return true;

    // Immediate self-cycle
    if (type.fromScene == this) return false;

    std::queue<SceneComponent*> q;
    std::unordered_set<SceneComponent*> visited;

    q.push(type.fromScene);
    visited.insert(type.fromScene);

    while (!q.empty())
    {
        SceneComponent* sc = q.front();
        q.pop();

        // Walk all node types used inside 'sc'
        for (const std::unique_ptr<NodeComponent>& c : sc->nodes)
        {
            const NodeType& nt = c->getType();
            SceneComponent* origin = nt.fromScene; // may be null for built-ins
            if (!origin) continue;

            // Found a path back to 'this' -> placing would create a cycle
            if (origin == this) return false;

            if (visited.insert(origin).second) // newly visited
                q.push(origin);
        }
    }

    // No path from type.fromScene back to 'this' => safe to place
    return true;
}

void SceneComponent::editNodeType()
{

    customNodeType.name = getName();
    customNodeType.address = "custom/";
    customNodeType.buildUI = [](NodeComponent&, NodeData&) {};
    std::vector<NodeData*> routineInputs;
    customNodeType.inputs.clear();
    std::vector<NodeComponent*> nodesFilteredByInputAndSorted;
    for (const std::unique_ptr<class NodeComponent>& node : nodes) {
        NodeData& data = node->getNodeData();
        const NodeType* type = data.getType();
        if (type->address == "inputs/") {
            nodesFilteredByInputAndSorted.push_back(node.get());
        }
    }

    std::sort(nodesFilteredByInputAndSorted.begin(),
        nodesFilteredByInputAndSorted.end(),
        [](NodeComponent* a, NodeComponent* b) {
            return a->getPosition().y < b->getPosition().y;
        });

    
    for (int inputIdx = 0; inputIdx < nodesFilteredByInputAndSorted.size(); inputIdx++) {
        auto node = nodesFilteredByInputAndSorted[inputIdx];
        NodeData& data = node->getNodeData();
        juce::String inputName = data.getProperties().contains("name") ? data.getProperties().at("name") : "input";
        bool requiresCompileTime = false;
        node->getNodeData().inputIndex = inputIdx;
        for (const std::unique_ptr<class NodeComponent>& downstream : nodes) {
            auto downstreamData = downstream->getNodeDataConst();
            for (int i = 0; i < downstreamData.getNumInputs(); i += 1) {
                if (downstreamData.getInput(i) == &data && (downstreamData.getType()->inputs[i].requiresCompileTimeKnowledge || downstreamData.needsCompileTimeInputs())) {
                    requiresCompileTime = true;
                    goto exit_loop;
                }
            }
        }
    exit_loop:
        InputFeatures features(inputName, data.getType()->isBoolean, 0, requiresCompileTime);
        features.optionalInputComponent = node;
        customNodeType.inputs.push_back(features);
    }
    customNodeType.fromScene = this;

    customNodeType.execute = [](const NodeData& node, UserInput& userInput, const std::vector<std::span<double>>& inputs, std::span<double>& output, RunnerInput& inlineInstance) {
        NodeComponent* n = node.owningComponent;
        Runner::run(*n, userInput, inputs);
    };
    customNodeType.getOutputSize = [this](const std::vector<NodeData*>& inputs, const std::vector<std::vector<double>>& d, RunnerInput& n, int, const NodeData&) {
        auto& outData = this->nodes[0]->getNodeDataConst();
        std::vector<std::span<double>> outerInputs;
        for (auto v : d) {
            outerInputs.push_back(std::span<double>(v.data(), v.size()));
        }
        if (!outData.compileTimeSizeReady(&n)) {
            Runner::initialize(n, this, outerInputs);
        }
        return outData.getCompileTimeSize(&n);
    };
}

void SceneComponent::addNode(const NodeType& type, juce::Point<int> localPos)
{

    addNode(type, localPos, nullptr, -1);
}

void SceneComponent::addNode(const NodeType& type, juce::Point<int> localPos, NodeData* toConnect, int index)
{

    // Create node
    nodes.push_back(std::make_unique<NodeComponent>(NodeData(type), type, *this));
    auto nodePtr = nodes.back().get();
    type.buildUI(*nodePtr, nodePtr->getNodeData());
    nodePtr->getNodeData().owningComponent = nodePtr;
    nodePtr->setTopLeftPosition(localPos);
    
    addAndMakeVisible(nodePtr);

    processorRef.updateRegistryFull();

    onSceneChanged();
    if (toConnect) {
        if (index >= 0) {
            toConnect->attachInput(index, &nodePtr->getNodeData(), *this);
        }
        else {
            nodePtr->getNodeData().attachInput(0, toConnect, *this);
        }
    }
}


void SceneComponent::deleteNode(NodeComponent* node)
{
	drawReady = false;
	// Remove node from nodes vector
	auto it = std::find_if(nodes.begin(), nodes.end(),
		[node](const std::unique_ptr<NodeComponent>& n) { return n.get() == node; });
	if (it != nodes.end())
	{
		nodes.erase(it);
	}

    auto& nodeType = node->getType();

    for (const std::unique_ptr<NodeComponent>& other : nodes) {
        auto& type = other->getType();
		for (int i = 0; i < type.inputs.size(); ++i) {
			auto input = other->getNodeDataConst().getInput(i);
            if (input && input == &node->getNodeDataConst()) {
                other->getNodeData().detachInput(i);
            }
		}
    }

	// Remove the node component from the scene
	removeChildComponent(node);
    onSceneChanged();
    
    processorRef.updateRegistryFull();
}

void SceneComponent::ensureNodeConnectionsCorrect(RunnerInput* r) {
    while (true) {
        bool hasBadConnections = false;
        for (auto& nodeComponent : nodes) {
            auto& data = nodeComponent->getNodeData();
            for (int i = 0; i < data.getNumInputs(); i += 1) {
                int requiredSize = data.getType()->inputs[i].requiredSize;
                if (auto other = data.getInput(i)) {
                    if (requiredSize != 0 && other->getCompileTimeSize(r) != requiredSize) {
                        if (i < data.inputNodes.size()) {
                            data.detachInput(i);
                        }
                        hasBadConnections = true;
                    }
                }
            }
        }
        if (!hasBadConnections) {
            break;
        }
    }
}

void SceneComponent::onSceneChanged()
{
    Runner::initialize(*this, this, std::vector<std::span<double>>());
    ensureNodeConnectionsCorrect(this);
    processorRef.initializeRunner();
    
    editNodeType();

    repaint();
}





static void insertNode(MenuNode& root, const NodeType& node, int id)
{
    juce::StringArray parts;
    parts.addTokens(node.address.trimCharactersAtStart("/"), "/", "");
    parts.trim();
    parts.removeEmptyStrings();

    MenuNode* current = &root;
    for (auto& part : parts)
    {
        current = &current->children[part]; // create if missing
        current->name = part;
    }

    // Add the leaf node
    current->children[node.name] = MenuNode{ node.name, id, &node};
}

juce::PopupMenu SceneComponent::buildNodeTypeMenuCache()
{
    return buildNodeTypeMenuCache([](const NodeType& type) {
        return true;
    });
}

juce::PopupMenu SceneComponent::buildNodeTypeMenuCache(std::function<bool(const NodeType&)> condition)
{
    juce::PopupMenu cachedAddNodeMenu;

    MenuNode root;

    // Build tree structure
    size_t i;
    auto& registry = processorRef.getRegistry();
    for (i = 0; i < registry.size(); ++i)
    {
        if (registry[i].name.toLowerCase() == "output" || !condition(registry[i])) // skip output node
        {
            continue;
        }
        insertNode(root, registry[i], static_cast<int>(i));
    }
    for (int j = 0, i = registry.size(); j < processorRef.getScenes().size(); j += 1, i += 1) {
        auto& scene = processorRef.getScenes()[j];
        if (scene.get() != &processorRef.getScene() && condition(scene->customNodeType)) {
            insertNode(root, scene->customNodeType, i);
        }
    }
    // Build the menu in one pass
    buildMenuRecursive(cachedAddNodeMenu, root, condition);
    return cachedAddNodeMenu;
}

void SceneComponent::buildMenuRecursive(juce::PopupMenu& menu, const MenuNode& node, std::function<bool(const NodeType&)> condition)
{
    for (auto& [_, child] : node.children)
    {
        if (child.menuId != 0) // leaf
        {
            auto* type = child.typeRef;
            if (canPlaceType(*type)) {
                menu.addItem(child.menuId, child.name);
            }
        }
        else
        {
            juce::PopupMenu sub;
            buildMenuRecursive(sub, child, condition);
            menu.addSubMenu(child.name, sub);
        }
    }
}