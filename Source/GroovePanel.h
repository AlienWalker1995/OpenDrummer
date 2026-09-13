#pragma once

#include "GroovePlayer.h"

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <memory>

/**
    The groove browser: pick a pattern, audition it through the kit, drag it
    into a DAW.
*/
class GroovePanel : public juce::Component,
                    private juce::ListBoxModel,
                    private juce::Timer
{
public:
    explicit GroovePanel (GroovePlayer& playerToDrive);
    ~GroovePanel() override;

    /** Scans a folder and refreshes the list. */
    void setGrooveFolder (const juce::File& folder);
    juce::File getGrooveFolder() const { return library.getFolder(); }

    /** Fired when the user picks a different folder, so it can be persisted. */
    std::function<void()> onFolderChanged;

    //==============================================================================
    // Transport and selection, for front ends that draw their own controls
    // (the pixel screen) while this panel keeps owning the groove logic.

    const groove::Library& getLibrary() const noexcept { return library; }
    int  getSelectedIndex() const                       { return list.getSelectedRow(); }
    void selectIndex (int index)                        { list.selectRow (index); }

    void togglePlayback();
    void setLoop (bool shouldLoop);
    bool getLoop() const                                { return loopButton.getToggleState(); }
    void setTempoBpm (double bpm)                       { tempoSlider.setValue (bpm, juce::sendNotificationSync); }
    double getTempoBpm() const                          { return tempoSlider.getValue(); }
    void openFolderChooser()                            { chooseFolder(); }
    juce::File getSelectedGrooveFile() const            { return getSelectedFile(); }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    //==============================================================================
    int getNumRows() override;
    void paintListBoxItem (int rowNumber, juce::Graphics&, int width, int height,
                           bool rowIsSelected) override;
    void selectedRowsChanged (int lastRowSelected) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;

    void timerCallback() override;

    void chooseFolder();
    void loadSelectedPattern();
    void updateTransportState();
    juce::File getSelectedFile() const;

    //==============================================================================
    /** Drag source for exporting the selected groove to a DAW. Kept as its own
        component because an external file drag has to originate from the thing
        the user actually grabbed. */
    class DragStrip;

    GroovePlayer& player;
    groove::Library library;

    juce::ListBox    list { "grooves", this };
    juce::Label      folderLabel;
    juce::TextButton folderButton { "Folder..." };
    juce::TextButton playButton   { "Play" };
    juce::TextButton loopButton   { "Loop" };
    juce::Slider     tempoSlider;
    juce::Label      tempoLabel;
    juce::Label      emptyLabel;

    // Cached in resized() so paint() can draw the position bar without
    // recomputing the layout.
    juce::Rectangle<int> progressBounds;

    std::unique_ptr<DragStrip> dragStrip;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GroovePanel)
};
