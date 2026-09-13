#include "GroovePanel.h"
#include "Theme.h"

//==============================================================================
class GroovePanel::DragStrip : public juce::Component
{
public:
    std::function<juce::File()> getFileToDrag;

    void mouseDrag (const juce::MouseEvent&) override
    {
        if (dragging || ! getFileToDrag)
            return;

        const auto file = getFileToDrag();

        if (! file.existsAsFile())
            return;

        dragging = true;

        // canMoveFiles stays false: the DAW gets a copy, and the library keeps
        // the original. Anything else would let a drag silently empty a folder.
        juce::DragAndDropContainer::performExternalDragDropOfFiles (
            { file.getFullPathName() }, false, this,
            [safe = juce::Component::SafePointer<DragStrip> (this)]
            {
                if (safe != nullptr)
                    safe->dragging = false;
            });
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (0.5f);
        const bool enabled = getFileToDrag && getFileToDrag().existsAsFile();

        g.setColour (theme::sunken);
        g.fillRoundedRectangle (bounds, 3.0f);

        // Dashed border reads as "grab me" without needing a label to say so.
        juce::Path border;
        border.addRoundedRectangle (bounds, 3.0f);

        const float dashes[] = { 4.0f, 3.0f };
        juce::Path dashed;
        juce::PathStrokeType (1.0f).createDashedStroke (dashed, border, dashes, 2);

        g.setColour (enabled ? theme::brass.withAlpha (0.75f) : theme::bevel);
        g.fillPath (dashed);

        g.setColour (enabled ? theme::brass : theme::textFaint);
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText (enabled ? "DRAG TO DAW" : "select a groove",
                    getLocalBounds(), juce::Justification::centred, false);
    }

    void mouseEnter (const juce::MouseEvent&) override { setMouseCursor (juce::MouseCursor::DraggingHandCursor); }

private:
    bool dragging = false;
};

//==============================================================================
GroovePanel::GroovePanel (GroovePlayer& playerToDrive)
    : player (playerToDrive)
{
    folderLabel.setFont (juce::FontOptions (11.0f));
    folderLabel.setColour (juce::Label::textColourId, theme::textFaint);
    addAndMakeVisible (folderLabel);

    folderButton.onClick = [this] { chooseFolder(); };
    addAndMakeVisible (folderButton);

    list.setRowHeight (28);
    list.setColour (juce::ListBox::backgroundColourId, theme::sunken);
    list.setOutlineThickness (1);
    addAndMakeVisible (list);

    emptyLabel.setText ("No .mid files in this folder", juce::dontSendNotification);
    emptyLabel.setFont (juce::FontOptions (12.0f));
    emptyLabel.setColour (juce::Label::textColourId, theme::textFaint);
    emptyLabel.setJustificationType (juce::Justification::centred);
    addChildComponent (emptyLabel);

    playButton.setClickingTogglesState (true);
    playButton.onClick = [this]
    {
        if (playButton.getToggleState())
        {
            loadSelectedPattern();
            player.start();
        }
        else
        {
            player.stop();
        }

        updateTransportState();
    };
    addAndMakeVisible (playButton);

    loopButton.setClickingTogglesState (true);
    loopButton.setToggleState (true, juce::dontSendNotification);
    loopButton.onClick = [this] { player.setLooping (loopButton.getToggleState()); };
    addAndMakeVisible (loopButton);

    tempoLabel.setText ("Tempo", juce::dontSendNotification);
    tempoLabel.setFont (juce::FontOptions (11.0f));
    tempoLabel.setColour (juce::Label::textColourId, theme::textDim);
    addAndMakeVisible (tempoLabel);

    tempoSlider.setRange (40.0, 240.0, 1.0);
    tempoSlider.setValue (120.0, juce::dontSendNotification);
    tempoSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    tempoSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 54, 20);
    tempoSlider.setTextValueSuffix (" BPM");
    tempoSlider.onValueChange = [this] { player.setTempo (tempoSlider.getValue()); };
    addAndMakeVisible (tempoSlider);

    dragStrip = std::make_unique<DragStrip>();
    dragStrip->getFileToDrag = [this] { return getSelectedFile(); };
    addAndMakeVisible (*dragStrip);

    startTimerHz (30);
}

GroovePanel::~GroovePanel() = default;

//==============================================================================
void GroovePanel::setGrooveFolder (const juce::File& folder)
{
    library.scan (folder);

    folderLabel.setText (folder.getFullPathName(), juce::dontSendNotification);
    emptyLabel.setVisible (library.getNumPatterns() == 0);

    list.deselectAllRows();
    list.updateContent();
    list.repaint();

    player.stop();
    player.clearPattern();
    playButton.setToggleState (false, juce::dontSendNotification);

    // Start with the first groove selected, so Play works straight away
    // instead of sitting greyed out until something is clicked.
    if (library.getNumPatterns() > 0)
        list.selectRow (0);

    updateTransportState();
}

void GroovePanel::togglePlayback()
{
    // Same path as clicking the Play key, so both front ends stay in step.
    playButton.setToggleState (! player.isPlaying(), juce::dontSendNotification);

    if (playButton.getToggleState())
    {
        loadSelectedPattern();
        player.start();
    }
    else
    {
        player.stop();
    }

    updateTransportState();
}

void GroovePanel::setLoop (bool shouldLoop)
{
    loopButton.setToggleState (shouldLoop, juce::dontSendNotification);
    player.setLooping (shouldLoop);
}

void GroovePanel::chooseFolder()
{
    chooser = std::make_unique<juce::FileChooser> ("Choose a folder of MIDI grooves",
                                                   library.getFolder(),
                                                   juce::String());

    chooser->launchAsync (juce::FileBrowserComponent::openMode
                            | juce::FileBrowserComponent::canSelectDirectories,
                          [this] (const juce::FileChooser& fc)
                          {
                              const auto result = fc.getResult();

                              if (result.isDirectory())
                              {
                                  setGrooveFolder (result);

                                  if (onFolderChanged)
                                      onFolderChanged();
                              }
                          });
}

//==============================================================================
int GroovePanel::getNumRows()
{
    return library.getNumPatterns();
}

void GroovePanel::paintListBoxItem (int rowNumber, juce::Graphics& g,
                                    int width, int height, bool rowIsSelected)
{
    const auto* pattern = library.getPattern (rowNumber);

    if (pattern == nullptr)
        return;

    // Drawn as a backlit tuning dial: every groove printed on the glass in dim
    // blue, the selected one lit like a tuned station with the pointer beside
    // it. Selection is shown by the pointer and a heavier weight as well as by
    // brightness, so it never depends on colour alone.
    const auto bounds = juce::Rectangle<int> (0, 0, width, height).toFloat();
    const bool playingThis = rowIsSelected && player.isPlaying();

    const float pointerX = 14.0f;
    const float textLeft = 30.0f;

    // Scale ticks at both edges instead of full-width stripes - a dial scale
    // marks positions, it does not rule lines across the glass.
    g.setColour (theme::dial.withAlpha (0.22f));
    g.fillRect (4.0f, bounds.getBottom() - 1.0f, 6.0f, 1.0f);
    g.fillRect (bounds.getRight() - 10.0f, bounds.getBottom() - 1.0f, 6.0f, 1.0f);

    if (rowIsSelected)
    {
        // A soft band of light where the pointer sits.
        g.setGradientFill (juce::ColourGradient (theme::dial.withAlpha (0.16f), bounds.getX(), bounds.getCentreY(),
                                                 theme::dial.withAlpha (0.0f), bounds.getX() + bounds.getWidth() * 0.6f,
                                                 bounds.getCentreY(), false));
        g.fillRect (bounds);

        // The lit pointer: a glowing vertical needle, as on a tuning dial.
        const auto needle = juce::Rectangle<float> (pointerX - 1.5f, 3.0f, 3.0f, bounds.getHeight() - 6.0f);

        g.setColour (theme::orange.withAlpha (0.30f));
        g.fillRoundedRectangle (needle.expanded (2.5f, 1.0f), 2.5f);

        g.setColour (theme::orange.brighter (0.35f));
        g.fillRoundedRectangle (needle, 1.5f);
    }

    // While playing, a small lit triangle beside the pointer.
    if (playingThis)
    {
        const float cy = bounds.getCentreY();
        juce::Path play;
        play.addTriangle (pointerX + 5.0f, cy - 4.0f, pointerX + 5.0f, cy + 4.0f, pointerX + 11.0f, cy);
        g.setColour (theme::dial);
        g.fillPath (play);
    }

    // Name. Unselected rows sit at 72% of the dial colour, which still clears
    // 4.5:1 against the glass; anything dimmer looked period but failed contrast.
    g.setColour (rowIsSelected ? theme::dial.brighter (0.55f) : theme::dial.withAlpha (0.72f));
    g.setFont (theme::legendFont (13.0f, rowIsSelected));
    g.drawText (pattern->name, bounds.withTrimmedLeft (textLeft).withTrimmedRight (130.0f),
                juce::Justification::centredLeft, true);

    // Metadata in the dial's figures, quieter than the name.
    juce::String meta;
    meta << pattern->bars << (pattern->bars == 1 ? " bar" : " bars")
         << "   " << juce::roundToInt (pattern->bpm) << " bpm";

    if (pattern->timeSigNumerator != 4 || pattern->timeSigDenominator != 4)
        meta << "   " << pattern->timeSigNumerator << "/" << pattern->timeSigDenominator;

    g.setColour (theme::dial.withAlpha (rowIsSelected ? 0.85f : 0.55f));
    g.setFont (theme::readoutFont (11.0f));
    g.drawText (meta, bounds.withTrimmedRight (16.0f), juce::Justification::centredRight, false);
}

void GroovePanel::selectedRowsChanged (int)
{
    // Auditioning as you browse is the whole point of a groove library, so a
    // new selection keeps playing rather than making you press Play again.
    const bool wasPlaying = player.isPlaying();

    loadSelectedPattern();

    if (wasPlaying)
        player.start();

    if (dragStrip != nullptr)
        dragStrip->repaint();

    updateTransportState();
}

void GroovePanel::listBoxItemDoubleClicked (int, const juce::MouseEvent&)
{
    loadSelectedPattern();
    player.start();
    playButton.setToggleState (true, juce::dontSendNotification);
    updateTransportState();
}

juce::File GroovePanel::getSelectedFile() const
{
    if (const auto* pattern = library.getPattern (list.getSelectedRow()))
        return pattern->file;

    return {};
}

void GroovePanel::loadSelectedPattern()
{
    const auto* pattern = library.getPattern (list.getSelectedRow());

    if (pattern == nullptr)
        return;

    player.setPattern (*pattern);

    // The player adopts the pattern's own tempo; mirror that in the slider so
    // the two never disagree about what is actually playing.
    tempoSlider.setValue (player.getTempo(), juce::dontSendNotification);
}

void GroovePanel::updateTransportState()
{
    const bool hasSelection = list.getSelectedRow() >= 0;

    playButton.setEnabled (hasSelection);
    playButton.setButtonText (player.isPlaying() ? "Stop" : "Play");
    list.repaint();   // the playing marker lives in the rows
}

void GroovePanel::timerCallback()
{
    // Keep the button honest if playback stopped on its own at the end of a
    // one-shot pattern.
    if (playButton.getToggleState() && ! player.isPlaying())
    {
        playButton.setToggleState (false, juce::dontSendNotification);
        updateTransportState();
    }

    if (player.isPlaying())
        repaint();
}

//==============================================================================
void GroovePanel::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    theme::drawPanel (g, bounds, 5.0f, true);

    auto header = getLocalBounds().removeFromTop (26).reduced (10, 0);
    g.setColour (theme::silk);
    g.setFont (theme::legendFont (12.0f, true));
    g.drawText ("Grooves", header, juce::Justification::centredLeft, false);

    // Playback position, drawn under the transport row.
    if (player.hasPattern())
    {
        auto track = progressBounds.toFloat();

        theme::drawGlassWindow (g, track, 2.0f);

        const float fraction = player.isPlaying() ? player.getProgress() : 0.0f;

        if (fraction > 0.0f)
        {
            g.setColour (theme::dial);
            g.fillRoundedRectangle (track.reduced (1.0f).withWidth (
                juce::jmax (1.0f, (track.getWidth() - 2.0f) * fraction)), 1.5f);
        }

        // Bar ticks, so you can see where the loop sits.
        if (const auto* pattern = library.getPattern (list.getSelectedRow()))
        {
            g.setColour (theme::background.withAlpha (0.8f));

            for (int bar = 1; bar < pattern->bars; ++bar)
            {
                const float x = track.getX() + track.getWidth() * (float) bar / (float) pattern->bars;
                g.fillRect (x, track.getY(), 1.0f, track.getHeight());
            }
        }
    }
}

void GroovePanel::resized()
{
    auto area = getLocalBounds().reduced (10);

    auto header = area.removeFromTop (26);
    header.removeFromLeft (70);                       // room for the GROOVES label
    folderButton.setBounds (header.removeFromRight (74).reduced (0, 3));
    header.removeFromRight (8);
    folderLabel.setBounds (header);

    area.removeFromTop (4);

    // Transport pinned to the bottom, list takes the rest.
    auto transport = area.removeFromBottom
                     (26);
    playButton.setBounds (transport.removeFromLeft (60));
    transport.removeFromLeft (6);
    loopButton.setBounds (transport.removeFromLeft (56));
    transport.removeFromLeft (12);
    tempoLabel.setBounds (transport.removeFromLeft (44));
    tempoSlider.setBounds (transport.removeFromLeft (juce::jmax (140, transport.getWidth() - 130)));
    transport.removeFromLeft (10);
    dragStrip->setBounds (transport);

    area.removeFromBottom (6);
    progressBounds = area.removeFromBottom (5);
    area.removeFromBottom (6);

    list.setBounds (area);
    emptyLabel.setBounds (area);
}
