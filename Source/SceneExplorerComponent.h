/*
  ==============================================================================

    SceneExplorerComponent.h
    Created: 11 Sep 2025 6:18:10pm
    Author:  ikamo

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>
#include "PropertiesMenu.h"

class WaviateFlow2025AudioProcessor;
class SceneData;

/**
 * SceneExplorerComponent
 *
 * Tree-based explorer of SceneData* grouped by folders under:
 *   userApplicationDataDirectory / "waviate" / "waviate_flow"
 *
 * Interactions:
 *  - Expand/Collapse folders
 *  - Double-click scene: setActiveScene(scene) if editable (local/network).
 *                        Marketplace scenes are read-only (ignored for active).
 *  - Middle-click scene: setAudibleScene(scene)
 *  - Drag scene: provides a drag description (sceneId + nodeType) for the canvas
 *  - Right-click: context menu (delete/duplicate/rename; handlers are stubs)
 */
class SceneExplorerComponent final : public PropertiesMenu,
    public juce::DragAndDropContainer
{
public:
    explicit SceneExplorerComponent(WaviateFlow2025AudioProcessor& processor);
    ~SceneExplorerComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    /** Rebuilds the tree from AppData (calls into SceneData helpers). */
    void refresh();

    /** Returns the root directory used for enumeration: AppData/waviate/waviate_flow */
    juce::File getAppDataRoot() const;
    static void enumerateSceneFolders(const juce::File& appRoot, juce::StringArray& outFolderRelPaths);

    static void enumerateScenesInFolder(const juce::File& appRoot, const juce::String& folderRelPath, juce::Array<SceneData*>& outScenes);
    static juce::StringArray splitPathComponents(const juce::String& relPath);
    std::unique_ptr<juce::TreeViewItem> rootItem;

private:
    WaviateFlow2025AudioProcessor& audioProcessor;

    // UI
    juce::TreeView tree;
    // Optional: adjust row height if you like
    int rowHeight = 22;

    // Helpers
    void buildTree();
    void clearTree();

    // Utilities for path splitting
    

    

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SceneExplorerComponent)

        // Inherited via PropertiesMenu
        void onUpdateUI() override;
};
