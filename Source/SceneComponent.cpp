#include "SceneComponent.h"
#include "PluginEditor.h"
#include "NodeComponent.h"
#include "DrawingUtils.h"

SceneComponent::SceneComponent(WaviateFlow2025AudioProcessor& processor, const std::string& name) : SceneData(name)
{
    processorRef = &processor;
    customNodeType = NodeType(processorRef->getCurrentLoadedTypeIndex());
    customNodeType.setNodeId(processorRef->getCurrentLoadedUserIndex(), customNodeType.getNodeId());
    highlightedNode = nullptr;
    nodes.reserve(2000);
    constructWithName(name);
}

SceneComponent::SceneComponent(WaviateFlow2025AudioProcessor& processor, const uint64_t userId, const uint64_t nodeId) : SceneData(userId, nodeId)
{
    nodes.reserve(2000);
    highlightedNode = nullptr;
    processorRef = &processor;
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

void SceneComponent::onSceneChanged()
{
    
    processorRef->initializeAllScenes();
    computeAllNodeWildCards();
    ensureNodeConnectionsCorrect(this);
    processorRef->initializeRunner();

    editNodeType();

    repaint();
}

void SceneComponent::constructWithName(const std::string& name)
{
    SceneData::constructWithName(name);
    setName(name);
    addAndMakeVisible(deleteSceneButton);
    deleteSceneButton.onClick = [this]() {
        processorRef->deleteScene(this);
        };
    if (!processorRef->scenes.empty()) {
        sceneNameBox.setJustification(juce::Justification::centred);
        addAndMakeVisible(sceneNameBox);
        sceneNameBox.setText(name);

    }
    highlightedNode = nullptr;
    setSize(8192, 8192);
    setWantsKeyboardFocus(true);
}





void SceneComponent::mouseDown(const juce::MouseEvent& e)
{
    if (processorRef->getCurrentEditor()) {
        processorRef->getCurrentEditor()->deselectBrowserItem();
    }
    grabKeyboardFocus();
    
    if (this != processorRef->activeScene) {
        processorRef->activeScene = this;
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
                if (!other || !nodeToComponentMap.contains(other)) continue;

                // Bezier endpoints in scene coords
                juce::Point<float> end = target->getInputPinScenePos(inputIndex);
                juce::Point<float> start = nodeToComponentMap.at(other)->getOutputPinScenePos();

                float dx = std::max(40.0f, std::abs(end.x - start.x) / 2.0f);
                juce::Point<float> c1(start.x + dx, start.y);
                juce::Point<float> c2(end.x - dx, end.y);

                if (lineIntersectsBezier(dragLine, start, c1, c2, end))
                {
                    nodeData.detachInput(inputIndex, this);
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
    showMenu(localPos, toConnect, index, [this](const NodeType& t) {
        return canPlaceType(t);
    });
}

void SceneComponent::showMenu(juce::Point<int> localPos, NodeData* toConnect, int index, std::function<bool(const NodeType&)> condition)
{
    auto menu = buildNodeTypeMenuCache(condition);
    menu.showMenuAsync(
        juce::PopupMenu::Options(),
        [this, localPos, &menu, toConnect, index](int result)
        {
            if (result > 0)
            {
                int typeId = result;
                if (typeId < processorRef->getRegistry().size()) {
                    auto& type = processorRef->getRegistry()[typeId];
                    addNode(type, localPos, toConnect, index);
                }
                else {
                    typeId -= processorRef->getRegistry().size();
                    auto& type = processorRef->getScenes()[typeId]->customNodeType;
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
        return f.requiresCompileTimeKnowledge ? juce::Colours::mediumvioletred.darker() : juce::Colours::black;
        };
    auto outputOutlineColour = [](NodeComponent* nc) -> juce::Colour {
        return nc->getNodeData().isCompileTimeKnown() ? juce::Colours::black : juce::Colours::mediumvioletred.darker();
        };
    auto inputFillColour = [this](NodeComponent* nc, int idx) -> juce::Colour {
        const auto& t = nc->getType();
        const auto& f = t.inputs[(size_t)idx];
        auto inp = nc->getNodeDataConst().getInput(idx);
        return (f.requiredSize == 1 || (inp && inp->compileTimeSizeReady(this) && inp->getCompileTimeSize(this) == 1)) ? juce::Colours::lawngreen.darker(.4f) : juce::Colours::gold;
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
                ? [&] { return nc->getNodeData().compileTimeSizeReady(this) && nc->getNodeData().isSingleton(this) ? juce::Colours::lawngreen.darker(.4f) : juce::Colours::gold;  }()
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
                if (!other || !nodeToComponentMap.contains(other)) continue;

                NodeComponent* otherNode = nodeToComponentMap.at(other);

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

    std::queue<SceneData*> q;
    std::unordered_set<SceneData*> visited;

    q.push(type.fromScene);
    visited.insert(type.fromScene);

    while (!q.empty())
    {
        SceneData* sc = q.front();
        q.pop();

        // Walk all node types used inside 'sc'
        for (const NodeData* c : sc->nodeDatas)
        {
            const NodeType& nt = *c->getType();
            SceneData* origin = nt.fromScene; // may be null for built-ins
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



NodeComponent& SceneComponent::addNode(const NodeType& type, juce::Point<int> localPos)
{

    return addNode(type, localPos, nullptr, -1);
}

NodeComponent& SceneComponent::addNode(const NodeType& type, juce::Point<int> localPos, NodeData* toConnect, int index)
{

    // Create node
    nodes.push_back(std::make_unique<NodeComponent>(NodeData(type), type, *this));
    auto nodePtr = nodes.back().get();
    type.buildUI(*nodePtr, nodePtr->getNodeData());
    type.onResized(*nodePtr);
	nodeDatas.push_back(&nodePtr->getNodeData());
    nodePtr->getNodeData().setPosition(localPos);
	nodeToComponentMap.insert_or_assign(&nodePtr->getNodeData(), nodePtr);
    nodePtr->setTopLeftPosition(localPos);
    
    addAndMakeVisible(nodePtr);

    onSceneChanged();
    
    if (toConnect) {
        if (index >= 0) {
            toConnect->attachInput(index, &nodePtr->getNodeData(), *this, this);
        }
        else {
            nodePtr->getNodeData().attachInput(0, toConnect, *this, this);
        }
    }
    onSceneChanged();
	return *nodePtr;
}


void SceneComponent::deleteNode(NodeComponent* node)
{
	drawReady = false;
	// Remove node from nodes vector
	auto it = std::find_if(nodes.begin(), nodes.end(),
		[node](const std::unique_ptr<NodeComponent>& n) { return n.get() == node; });

    auto nt = std::find_if(nodeDatas.begin(), nodeDatas.end(),
        [node](NodeData* n) { return n == &node->getNodeDataConst(); });



    for (const std::unique_ptr<NodeComponent>& other : nodes) {
        auto& type = other->getType();
        if (other.get() != node) {
            for (const auto& [c, i] : other->getNodeDataConst().outputs) {
				if (c && c == &node->getNodeDataConst()) {
					node->getNodeData().detachInput(i, this);
				}
            }
            for (int i = 0; i < type.inputs.size(); ++i) {
                auto input = other->getNodeDataConst().getInput(i);
                if (input && input == &node->getNodeDataConst()) {
                    other->getNodeData().detachInput(i, this);
                }
            }
        }

    }

    if (nt != nodeDatas.end()) {
        nodeDatas.erase(nt);
    }

	if (it != nodes.end())
	{
		nodes.erase(it);
	}

    auto& nodeType = node->getType();

	// Remove the node component from the scene
	removeChildComponent(node);
    onSceneChanged();
    
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
                            data.detachInput(i, this);
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
    auto& registry = processorRef->getRegistry();
    for (i = 0; i < registry.size(); ++i)
    {
        if (registry[i].name.toLowerCase() == "output" || !condition(registry[i])) // skip output node
        {
            continue;
        }
        insertNode(root, registry[i], static_cast<int>(i));
    }
    for (int j = 0, i = registry.size(); j < processorRef->getScenes().size(); j += 1, i += 1) {
        auto& scene = processorRef->getScenes()[j];
        if (condition(scene->customNodeType)) {
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