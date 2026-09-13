#pragma once

#include "DrumEngine.h"
#include "SampledKitPlayer.h"
#include "SampledKits.h"
#include "GroovePanel.h"
#include "GroovePlayer.h"
#include "KitView.h"
#include "PixelDeck.h"
#include "SynthKit.h"
#include "Theme.h"

#include <juce_audio_utils/juce_audio_utils.h>
#include <deque>
#include <vector>

/**
    An analog VU meter with a peak lamp.

    The needle follows the VU standard rather than just easing towards the
    level: a second-order movement tuned to reach 99% in about 300 ms with
    roughly 1.2% overshoot, which is what gives a real meter its weight. The
    lamp exists because that same slowness means the needle can never show a
    drum hit clipping.
*/
class LevelMeter : public juce::Component,
                   private juce::Timer
{
public:
    LevelMeter();

    /** Whatever is producing sound right now reports its smoothed mean-square
        level and its instantaneous peak through these. */
    void setSources (std::function<float()> meanSquareSource, std::function<float()> peakSource)
    {
        meanSquare = std::move (meanSquareSource);
        peak = std::move (peakSource);
    }

    void paint (juce::Graphics& g) override;

private:
    void timerCallback() override;

    std::function<float()> meanSquare;
    std::function<float()> peak;

    // Needle state, in VU.
    float position = -22.0f;
    float velocity = 0.0f;
    double lastTick = 0.0;

    juce::uint32 peakLitUntil = 0;
};

//==============================================================================
class MainComponent : public juce::AudioAppComponent,
                      private juce::MidiInputCallback,
                      private juce::Timer,
                      private juce::AsyncUpdater,
                      private juce::ChangeListener
{
public:
    MainComponent();
    ~MainComponent() override;

    //==============================================================================
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    //==============================================================================
    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    //==============================================================================
    void handleIncomingMidiMessage (juce::MidiInput* source,
                                    const juce::MidiMessage& message) override;

    void timerCallback() override;
    void handleAsyncUpdate() override;                       // builds the kit off-thread
    void changeListenerCallback (juce::ChangeBroadcaster*) override;   // device changed

    void loadSettings();
    void saveSettings();
    void chooseSensibleFirstRunDefaults();
    void rebuildKit();

    void populateKitSelector();
    void selectKitFromCombo (int comboId);
    void applyKitSelection (const juce::String& savedKitId);
    void logAvailableDevices();
    juce::String getSystemOutputDeviceName();
    bool selectAudioDevice (const juce::String& referenceName);
    void applyPreferredRateAndBuffer();
    void showSettingsPage (bool shouldShow);
    void updatePageVisibility();
    void pollEngine();
    void pollMidiMonitor();
    void refreshMidiStatus();

    /** Keeps the remembered MIDI inputs open: enables any that are present but
        closed, and reopens one that disappeared and came back. */
    void ensureMidiInputs();
    void appendMidiLog (const juce::String& line);

    juce::String describeAudioDevice() const;

    //==============================================================================
    // Declared first so it outlives every component that draws with it -
    // members are destroyed in reverse order.
    theme::LookAndFeel lookAndFeel;

    DrumEngine engine;
    SampledKitPlayer sampledPlayer { engine.getMapping() };
    GroovePlayer groovePlayer;

    // Which source renders. Flipped only once a recorded kit has finished
    // loading, so switching kits never leaves a gap of silence.
    std::atomic<bool> useSampled { false };
    std::atomic<bool> synthSelected { true };
    juce::ApplicationProperties appProperties;

    // Reused each block rather than allocated in the audio callback.
    juce::MidiBuffer scheduledMidi;

    // --- main page -----------------------------------------------------------
    KitView          kitView;

    // The main screen. The widgets around it are kept, hidden, as the single
    // source of truth for settings and state; the deck only draws and routes.
    PixelDeck        deck;
    PixelDeck::Snapshot deckSnapshot;
    void refreshDeckSnapshot();
    int currentKitIndex() const;
    juce::Rectangle<int> wordmarkBounds;

    // The status readout is drawn rather than set in a Label, so it can carry
    // the vacuum-fluorescent bloom. It is the only lit element in the window.
    juce::String         statusText;
    juce::Rectangle<int> statusBounds;

    juce::ComboBox   kitSelector;
    juce::Label      kitLabel;
    juce::Label      kitCreditLabel;
    synthkit::Style  currentStyle = synthkit::Style::Studio;

    // Recorded kits get combo IDs from here up; synthesised kits use 1..N.
    static constexpr int kSampledIdBase = 1000;

    std::vector<sampledkits::InstalledKit> installedKits;
    juce::String selectedKitId;     // "sampled:<id>" or "synth:<index>", persisted

    juce::String kitNotice;         // "Big Rusty ready in 3.2 s", shown briefly
    juce::uint32 kitNoticeUntil = 0;
    juce::String loadingKitName;
    juce::Label      hitLabel;
    LevelMeter       meter;
    juce::Slider     gainSlider;
    juce::Label      gainLabel;
    juce::Label      gainValueLabel;
    juce::Rectangle<int> statusStripBounds;
    juce::Label      midiStatusLabel;
    juce::TextButton groovesButton  { "Grooves" };
    juce::TextButton settingsButton { "Settings" };

    std::unique_ptr<GroovePanel> groovePanel;
    // Shown by default: the groove library is the reason most people open
    // the app, and a feature hidden behind a button is a feature nobody
    // finds. Persisted, so closing it sticks.
    bool groovesVisible = true;

    // --- settings page -------------------------------------------------------
    std::unique_ptr<juce::AudioDeviceSelectorComponent> deviceSelector;
    juce::Slider       curveSlider;
    juce::Label        curveLabel;
    juce::ToggleButton invertPedalButton { "Invert hi-hat pedal (CC4)" };
    juce::Label        midiHelpLabel;
    juce::Label        rawLogTitle;
    juce::Label        hitLogTitle;
    juce::TextEditor   rawLog;
    juce::TextEditor   hitLog;
    juce::TextButton   backButton { "Back to kit" };

    bool settingsVisible = false;

    //==============================================================================
    juce::uint64 hitReadIndex = 0;
    juce::uint64 sampledHitReadIndex = 0;
    std::vector<DrumEngine::HitInfo> hitScratch;
    std::deque<juce::String> recentHits;

    /** Everything the module sends, captured before any mapping is applied.
        A pad whose note falls outside the Roland map would otherwise be
        indistinguishable from an unplugged cable: silence, and nothing
        logged anywhere. */
    struct RawMidiEvent
    {
        enum class Kind { NoteOn, NoteOff, CC, Aftertouch, Other };

        Kind kind = Kind::Other;
        int  channel = 1;
        int  data1 = 0;
        int  data2 = 0;
        bool mapped = false;
    };

    static constexpr int kRawRingSize = 512;

    std::array<RawMidiEvent, kRawRingSize> rawRing;
    std::atomic<juce::uint64> rawWriteIndex { 0 };
    juce::uint64 rawReadIndex = 0;

    std::atomic<juce::uint64> midiMessageCount { 0 };
    std::atomic<juce::uint64> noteOnCount { 0 };
    std::atomic<juce::uint64> unmappedNoteCount { 0 };

    std::deque<juce::String> recentRaw;

    // Which drivers were tried and why they failed, for devices.log.
    juce::String deviceSelectionLog;

    // MIDI inputs are remembered by name, not by Windows' device identifier,
    // and retried until they open. A port is exclusive on Windows: if another
    // program - including a previous copy of this app still shutting down -
    // holds it at launch, the open fails, and without a retry the input would
    // silently stay off from then on.
    juce::StringArray rememberedMidiInputs;
    bool midiInputsChosenByUser = false;
    juce::StringArray lastSeenMidiIds;
    juce::StringArray midiOpenFailuresLogged;

    // Enumerating MIDI devices is too costly to repeat every frame.
    juce::String cachedMidiInputNames;
    int midiRecheckCountdown = 0;

    // Set when the device opens at a new rate; the kit is then regenerated on
    // the message thread rather than inside the audio callback.
    std::atomic<double> pendingKitSampleRate { 0.0 };
    double builtKitSampleRate = 0.0;
    std::atomic<bool> kitReady { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
