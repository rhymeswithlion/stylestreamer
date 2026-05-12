#pragma once
#include <PluginProcessor.h>

/* Run test code with a real `PluginProcessor` editor instance (see Pamplejuce
 * discussion: https://github.com/sudara/pamplejuce/issues/18#issuecomment-1425836807).
 *
 * For a PNG export of the editor without opening Standalone, use the Catch test
 * `PluginEditor renders to an image` in `PluginEditorSnapshotTests.cpp`:
 *   MRT_JUCE_UI_SNAPSHOT=1 ./Tests "[ui]"
 * Optional `MRT_JUCE_UI_SNAPSHOT_DIR` (directory) overrides the default
 * `output/<date>-juce-editor-snapshot/` path under the monorepo root.
 * Optional `MRT_JUCE_UI_SNAPSHOT_SCALE` (default 2) sets `createComponentSnapshot` scale.
 */
[[maybe_unused]] static void runWithinPluginEditor (const std::function<void (PluginProcessor& plugin)>& testCode)
{
    PluginProcessor plugin;
    const auto editor = plugin.createEditorIfNeeded();

    testCode (plugin);

    plugin.editorBeingDeleted (editor);
    delete editor;
}
