/*
  ==============================================================================

    SceneExplorerComponent.cpp
    Created: 11 Sep 2025 6:18:10pm
    Author:  ikamo

  ==============================================================================
*/

#include "SceneExplorerComponent.h"
#include "PluginProcessor.h"  // WaviateFlow2025AudioProcessor
#include "SceneComponent.h"        // SceneData (assumed available)
#include "Serializer.h"

/* ---------- Assumed SceneData surface (adjust names to your real API) ----------
 *
 * class SceneData {
 * public:
 *     // Classification
 *     bool        isMarketplaceContent() const;    // true if read-only marketplace
 *     bool        isEditable() const;              // true if local or network owned
 *
 *     // Identity / addressing
 *     juce::String getDisplayName() const;         // name for UI
 *     juce::String getSceneId() const;             // unique ID (stable across sessions)
 *     juce::String getCanvasNodeTypeIdentifier() const; // node type to place on canvas drop
 *
 *     // Enumeration helpers (static). You can back these by your repository/index.
 *     // Returns a list of relative folder paths under the app root (using / or \ separators).
 *     static void enumerateSceneFolders(const juce::File& appRoot, juce::StringArray& outFolderRelPaths);
 *
 *     // Fills 'outScenes' with SceneData* for a given relative folder (under appRoot).
 *     static void enumerateScenesInFolder(const juce::File& appRoot,
 *                                         const juce::String& folderRelPath,
 *                                         juce::Array<SceneData*>& outScenes);
 * };
 *
 * ---------- Assumed processor surface ----------
 *
 * class WaviateFlow2025AudioProcessor {
 * public:
 *     void setActiveScene(SceneData* scene);
 *     void setAudibleScene(SceneData* scene);
 *     SceneData* getActiveScene() const;   // optional, used for highlighting
 *     SceneData* getAudibleScene() const;  // optional, used for highlighting
 * };
 */

 // ============================================================================
 // Internal Tree Items
 // ============================================================================

namespace
{
    // Forward declarations
    struct FolderItem;
    struct SceneItem;

    // Base item with access to owner + processor for interactions
    struct ExplorerItemBase : public juce::TreeViewItem
    {
        ExplorerItemBase(SceneExplorerComponent& owner, WaviateFlow2025AudioProcessor& proc)
            : explorer(owner), processor(proc) {
        }

        SceneExplorerComponent& explorer;
        WaviateFlow2025AudioProcessor& processor;

        int getItemHeight() const override { return 22; } // can use explorer.rowHeight if preferred

        // Slightly dim marketplace scenes in SceneItem::paintItem
        static void drawText(juce::Graphics& g, const juce::String& text,
            juce::Colour colour, int width, int height, int indent = 6)
        {
            g.setColour(colour);
            g.setFont(juce::Font(13.0f));
            g.drawText(text, indent, 0, width - indent, height, juce::Justification::centredLeft, true);
        }

        // Utility for a small indicator prefix (e.g., active/audible)
        static juce::String prefixFlags(bool isActive, bool isAudible)
        {
            // Use simple ASCII prefixes to avoid asset management:
            const juce::String audible = isAudible ? "[*] " : "";
            const juce::String active = isActive ? "[A] " : "";
            return active + audible;
        }
    };

    // ---------------------- Scene item ----------------------
    struct SceneItem : public ExplorerItemBase
    {
        explicit SceneItem(SceneExplorerComponent& owner,
            WaviateFlow2025AudioProcessor& proc,
            SceneComponent* s)
            : ExplorerItemBase(owner, proc), scene(s)
        {
            jassert(scene != nullptr);
        }

        SceneComponent* scene = nullptr;

        bool mightContainSubItems() override { return false; }

        juce::String getUniqueName() const override
        {
            // Prefer a stable unique id
            return "scene:" + (scene ? scene->customNodeType.name : juce::String("null"));
        }

        void paintItem(juce::Graphics& g, int width, int height) override
        {
            const bool marketplace = scene && scene->isMarketplaceContent();
            const bool editable = scene && scene->isLocalEditable();

            // Optional: highlight active/audible
            bool isActive = false;
            bool isAudible = false;
            if (auto* active = processor.getActiveScene())  isActive = (active == scene);
            if (auto* audible = processor.getAudibleScene()) isAudible = (audible == scene);

            auto colour = marketplace ? juce::Colours::lightgrey.withAlpha(0.85f)
                : juce::Colours::white;

            juce::String text = prefixFlags(isActive, isAudible)
                + (scene ? scene->customNodeType.name : juce::String("(null)"));

            drawText(g, text, colour, width, height);
        }

        // Enable dragging of scenes into the canvas
        juce::var getDragSourceDescription() override
        {
            if (scene == nullptr)
                return {};

            auto* obj = new juce::DynamicObject();
            obj->setProperty("kind", "scene");
            obj->setProperty("nodeId", juce::String(scene->customNodeType.getNodeId()));
            obj->setProperty("userId", juce::String(scene->customNodeType.getNodeUserID()));
            // You can add more (e.g., readOnly) if the canvas cares
            obj->setProperty("readOnly", !scene->isLocalEditable());
            return juce::var(obj); // TreeView will find a DragAndDropContainer up the parent chain
        }

        void itemClicked(const juce::MouseEvent& e) override
        {
            if (scene == nullptr)
                return;

            if (e.mods.isMiddleButtonDown())
            {
                // Middle-click => set audible
                processor.setAudibleScene(scene);
                // Redraw just this row for immediate visual feedback
                if (auto* tv = getOwnerView()) tv->repaint();
            }
            else if (e.mods.isRightButtonDown())
            {
                // Context menu (actions are stubs for now)
                juce::PopupMenu m;
                m.addItem(1, "Delete");
                m.addItem(2, "Duplicate");
                m.addItem(3, "Rename");
                m.showMenuAsync(juce::PopupMenu::Options().withParentComponent(&explorer),
                    [this](int result)
                    {
                        switch (result)
                        {
                        case 1: /* TODO: delete scene */ break;
                        case 2: /* TODO: duplicate scene */ break;
                        case 3: /* TODO: rename scene */ break;
                        default: break;
                        }
                    });
            }
        }

        void itemDoubleClicked(const juce::MouseEvent&) override
        {
            if (scene == nullptr)
                return;

            
            if (scene->isLocalEditable())
            {
                processor.setActiveScene(scene);
                if (auto* tv = getOwnerView()) tv->repaint();
            }
            else
            {
                // Marketplace: do nothing (active scene unchanged)
                // You can toast/tooltip here if desired
            }
        }
    };

    // ---------------------- Folder item ----------------------
    struct FolderItem : public ExplorerItemBase
    {
        FolderItem(SceneExplorerComponent& owner,
            WaviateFlow2025AudioProcessor& proc,
            juce::String folderName,
            juce::String relativePath)
            : ExplorerItemBase(owner, proc),
            name(std::move(folderName)),
            relPath(std::move(relativePath))
        {
        }

        juce::String name;
        juce::String relPath;

        bool mightContainSubItems() override { return true; }

        juce::String getUniqueName() const override
        {
            return "folder:" + relPath; 
        }

        void paintItem(juce::Graphics& g, int width, int height) override
        {
            auto colour = juce::Colours::lightgrey;
            drawText(g, name.isNotEmpty() ? name : juce::String("(root)"), colour, width, height);
        }

        void refreshChildren()
        {
            // Clear any existing
            clearSubItems();

            // Build subfolders list and add items in a later pass
            // Strategy: enumerate all folder paths from root, then insert those that are direct children of relPath
            juce::StringArray allFolders;
            SceneExplorerComponent::enumerateSceneFolders(explorer.getAppDataRoot(), allFolders);

            const auto isDirectChildOf = [this](const juce::String& fullRel) -> bool
                {
                    // fullRel starts with relPath (or relPath empty), and has exactly one next segment
                    auto remaining = fullRel;

                    if (relPath.isNotEmpty())
                    {
                        if (!remaining.startsWithIgnoreCase(relPath))
                            return false;

                        // Consume relPath + separator if present
                        remaining = remaining.substring(relPath.length());
                        if (remaining.startsWithChar('/') || remaining.startsWithChar('\\'))
                            remaining = remaining.substring(1);
                    }

                    // A direct child folder has no further separators after its first segment
                    if (remaining.isEmpty())
                        return false;

                    // If remaining contains a separator, it is deeper than direct child
                    auto posSlash = remaining.indexOfChar('/');
                    auto posBSlash = remaining.indexOfChar('\\');
                    const bool hasSep = (posSlash >= 0) || (posBSlash >= 0);
                    if (!hasSep)
                        return true;

                    // If first separator is at end => this path points to itself; ignore
                    return false;
                };

            // Add direct child folders
            juce::StringArray addedFolderNames; // to avoid duplicates
            for (const auto& path : allFolders)
            {
                if (!isDirectChildOf(path))
                    continue;

                // Determine child's displayed name
                auto comps = SceneExplorerComponent::splitPathComponents(path);
                juce::String childName = comps.isEmpty() ? juce::String("(root)") : comps[comps.size() - 1];

                if (addedFolderNames.contains(childName, true))
                    continue;

                addedFolderNames.add(childName);

                // Build child's relPath
                juce::String childRel = relPath.isNotEmpty() ? (relPath + "/" + childName) : childName;

                addSubItem(new FolderItem(explorer, processor, childName, childRel));
            }

            // Add scenes that live directly in this folder
            juce::Array<SceneData*> scenes;
            SceneExplorerComponent::enumerateScenesInFolder(explorer.getAppDataRoot(), relPath, scenes);

            for (auto* s : scenes)
            {
                auto* si = new SceneItem(explorer, processor, dynamic_cast<SceneComponent*>(s));
                addSubItem(si);
            }

            // Optionally auto-open folders with content
            setOpen(true);
        }

        // When a folder opens, populate children
        void itemOpennessChanged(bool isNowOpen) override
        {
            if (isNowOpen && getNumSubItems() == 0)
                refreshChildren();
        }
    };

} // namespace

// ============================================================================
// SceneExplorerComponent
// ============================================================================

SceneExplorerComponent::SceneExplorerComponent(WaviateFlow2025AudioProcessor& processor)
    : audioProcessor(processor)
{
    addAndMakeVisible(tree);
    tree.setRootItemVisible(true);
    tree.setDefaultOpenness(true);
    tree.setMultiSelectEnabled(false);
    tree.setIndentSize(14);
    tree.setRootItem(nullptr); // ensure clean state
    setButtonIcon( BinaryData::SceneExplorerLogo_png, BinaryData::SceneExplorerLogo_pngSize);
    refresh();
}

SceneExplorerComponent::~SceneExplorerComponent()
{
    clearTree(); // deletes root item (TreeView owns it)
}

void SceneExplorerComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
}

void SceneExplorerComponent::resized()
{
    tree.setBounds(getLocalBounds());
}

juce::File SceneExplorerComponent::getAppDataRoot() const
{
    // ~/AppData/Roaming/waviate/waviate_flow   (Windows)
    // ~/Library/Application Support/waviate/waviate_flow (macOS)
    // ~/.local/share/waviate/waviate_flow      (Linux)
    auto base = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    auto waviateDir = base.getChildFile("waviate");
    auto flowDir = waviateDir.getChildFile("waviate_flow");
    return flowDir;
}

void SceneExplorerComponent::refresh()
{
    clearTree();
    buildTree();
}

void SceneExplorerComponent::clearTree()
{
    tree.setRootItem(nullptr); // TreeView deletes the previous root
}

void SceneExplorerComponent::onUpdateUI()
{
}

void SceneExplorerComponent::buildTree()
{
    rootItem = std::make_unique<FolderItem>(*this, audioProcessor, "Waviate Flow", "");
    tree.setRootItem(rootItem.get());

    rootItem->setOpen(true);

    dynamic_cast<FolderItem*>(rootItem.get())->refreshChildren();
}

juce::StringArray SceneExplorerComponent::splitPathComponents(const juce::String& relPath)
{
    juce::StringArray result;

    juce::String tmp = relPath;
    tmp = tmp.replaceCharacter('\\', '/');
    result.addTokens(tmp, "/", "");
    result.removeEmptyStrings();

    return result;
}

void SceneExplorerComponent::enumerateSceneFolders(const juce::File& appRoot,
    juce::StringArray& outFolderRelPaths)
{
    outFolderRelPaths.clear();

    if (!appRoot.exists() || !appRoot.isDirectory())
        return;

    // Depth-first walk of all subdirectories
    juce::Array<juce::File> stack;
    stack.add(appRoot);

    while (stack.size() > 0)
    {
        auto dir = stack.removeAndReturn(stack.size() - 1);
        auto rel = dir.getRelativePathFrom(appRoot);

        if (dir != appRoot) // skip the root itself
            outFolderRelPaths.add(rel);

        juce::RangedDirectoryIterator iter(dir, false, "*", juce::File::findDirectories);
        for (const auto& iter : juce::RangedDirectoryIterator(dir, false, "*", juce::File::findDirectories))
            stack.add(iter.getFile());
    }
}

void SceneExplorerComponent::enumerateScenesInFolder(const juce::File& appRoot,
    const juce::String& folderRelPath,
    juce::Array<SceneData*>& outScenes)
{
    outScenes.clear();

    auto dir = folderRelPath.isNotEmpty()
        ? appRoot.getChildFile(folderRelPath)
        : appRoot;

    if (!dir.exists() || !dir.isDirectory())
        return;


    for (const auto& entry : juce::RangedDirectoryIterator(dir, false, "*.wvtf", juce::File::findFiles))
    {
        auto file = entry.getFile();
        juce::StringArray lines;
        file.readLines(lines);
        // Assume you have a SceneData::loadFromFile(File) or similar
        std::optional<NodeType> nt = Serializer::deserialize(lines.joinIntoString(" ").toStdString());
        if (nt.has_value()) {
            outScenes.add(nt->fromScene);
        }
    }
}

