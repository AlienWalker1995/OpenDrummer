#include "MainComponent.h"
#include "SynthKit.h"
#include "Theme.h"

#include <algorithm>
#include <utility>
#include <vector>

//==============================================================================
LevelMeter::LevelMeter()
{
    startTimerHz (60);
}

void LevelMeter::timerCallback()
{
    if (! meanSquare || ! peak)
        return;

    const double now = juce::Time::getMillisecondCounterHiRes() / 1000.0;
    const float elapsed = lastTick > 0.0 ? (float) juce::jmin (0.1, now - lastTick) : 0.0f;
    lastTick = now;

    // 0 VU aligned to -18 dBFS RMS, the usual digital alignment level.
    const float rmsDb = juce::Decibels::gainToDecibels (std::sqrt (juce::jmax (0.0f, meanSquare())), -80.0f);
    const float target = juce::jmax (-24.0f, rmsDb + 18.0f);

    // Integrate the needle in small fixed steps so the ballistics hold up
    // whatever rate the timer actually manages.
    constexpr float kNaturalFrequency = 13.5f;   // rad/s
    constexpr float kDamping = 0.81f;
    constexpr float kStep = 1.0f / 600.0f;

    for (float t = 0.0f; t < elapsed; t += kStep)
    {
        const float dt = juce::jmin (kStep, elapsed - t);
        const float acceleration = kNaturalFrequency * kNaturalFrequency * (target - position)
                                 - 2.0f * kDamping * kNaturalFrequency * velocity;
        velocity += acceleration * dt;
        position += velocity * dt;
    }

    // Within a dB of full scale lights the lamp, and it holds long enough to
    // be seen.
    if (peak() > 0.89f)
        peakLitUntil = juce::Time::getMillisecondCounter() + 500;

    repaint();
}

void LevelMeter::paint (juce::Graphics& g)
{
    theme::drawVuMeter (g, getLocalBounds().toFloat(), position,
                        juce::Time::getMillisecondCounter() < peakLitUntil);
}

//==============================================================================
MainComponent::MainComponent()
{
    // Settings live in the user's roaming app data so the app comes back up
    // on the same audio and MIDI devices every launch.
    juce::PropertiesFile::Options options;
    options.applicationName = "OpenDrummer";
    options.filenameSuffix = ".settings";
    options.folderName = "OpenDrummer";
    options.osxLibrarySubFolder = "Application Support";
    appProperties.setStorageParameters (options);

    // One look for the whole app, popups and file choosers included.
    juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);

    //--- main page -------------------------------------------------------------
    addAndMakeVisible (kitView);

    hitLabel.setText ("Hit a pad to begin", juce::dontSendNotification);
    hitLabel.setFont (theme::legendFont (23.0f, true));
    // Unlit until something actually arrives - the glow means "on", and it
    // should not be spent on a prompt.
    hitLabel.setColour (juce::Label::textColourId, theme::inkOnGlassDim);
    addAndMakeVisible (hitLabel);

    meter.setSources ([this] { return useSampled.load() ? sampledPlayer.getMeanSquare() : engine.getMeanSquare(); },
                      [this] { return useSampled.load() ? sampledPlayer.getPeakLevel() : engine.getPeakLevel(); });
    addAndMakeVisible (meter);

    // The single most useful line on screen when something is not working:
    // is the module connected, and is it actually sending anything?
    midiStatusLabel.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    midiStatusLabel.setColour (juce::Label::textColourId, theme::textDim);
    addAndMakeVisible (midiStatusLabel);

    gainLabel.setText ("Master", juce::dontSendNotification);
    gainLabel.setFont (theme::legendFont (11.5f, true));
    gainLabel.setJustificationType (juce::Justification::centred);
    gainLabel.setColour (juce::Label::textColourId, theme::silk);
    addAndMakeVisible (gainLabel);

    gainSlider.setRange (-40.0, 12.0, 0.1);
    gainSlider.setSkewFactorFromMidPoint (-6.0);   // more travel where people actually listen
    gainSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);

    // A little under three-quarters of a turn, like a receiver's volume knob.
    gainSlider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                    juce::MathConstants<float>::pi * 2.75f, true);
    gainSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);

    // A knob is awkward to aim with a mouse, so give it every escape hatch:
    // drag straight up or down, scroll, or double-click back to unity.
    gainSlider.setMouseDragSensitivity (220);
    gainSlider.setDoubleClickReturnValue (true, 0.0);
    gainSlider.setTooltip ("Master volume. Drag up or down, scroll, or double-click to reset.");

    gainValueLabel.setFont (theme::readoutFont (11.0f));
    gainValueLabel.setColour (juce::Label::textColourId, theme::silk);
    gainValueLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (gainValueLabel);
    gainSlider.onValueChange = [this]
    {
        const auto gain = juce::Decibels::decibelsToGain ((float) gainSlider.getValue());
        gainValueLabel.setText (juce::String (gainSlider.getValue(), 1) + " dB", juce::dontSendNotification);
        engine.setMasterGain (gain);
        sampledPlayer.setMasterGain (gain);
        saveSettings();
    };
    addAndMakeVisible (gainSlider);

    groovesButton.setClickingTogglesState (true);
    groovesButton.setToggleState (true, juce::dontSendNotification);
    groovesButton.onClick = [this]
    {
        groovesVisible = groovesButton.getToggleState();
        updatePageVisibility();
        saveSettings();
    };
    addAndMakeVisible (groovesButton);

    settingsButton.onClick = [this] { showSettingsPage (true); };
    addAndMakeVisible (settingsButton);

    kitLabel.setText ("Kit", juce::dontSendNotification);
    kitLabel.setFont (theme::legendFont (12.0f));
    kitLabel.setColour (juce::Label::textColourId, theme::textDim);
    kitLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (kitLabel);

    kitCreditLabel.setFont (theme::legendFont (10.5f));
    kitCreditLabel.setColour (juce::Label::textColourId, theme::silkFaint);
    kitCreditLabel.setJustificationType (juce::Justification::centredRight);
    kitCreditLabel.setMinimumHorizontalScale (0.75f);
    addAndMakeVisible (kitCreditLabel);

    populateKitSelector();

    kitSelector.onChange = [this]
    {
        const int id = kitSelector.getSelectedId();

        if (id > 0)
            selectKitFromCombo (id);
    };
    addAndMakeVisible (kitSelector);

    //--- settings page ---------------------------------------------------------
    curveLabel.setText ("Velocity curve", juce::dontSendNotification);
    curveLabel.setColour (juce::Label::textColourId, theme::textDim);
    addChildComponent (curveLabel);

    // Below 1.0 lifts soft hits, which is what most mesh-head players want.
    curveSlider.setRange (0.4, 2.0, 0.01);
    curveSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    curveSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 66, 20);
    curveSlider.onValueChange = [this]
    {
        engine.setVelocityCurve ((float) curveSlider.getValue());
        sampledPlayer.setVelocityCurve ((float) curveSlider.getValue());
        saveSettings();
    };
    addChildComponent (curveSlider);

    invertPedalButton.setColour (juce::ToggleButton::textColourId, theme::textDim);
    invertPedalButton.onClick = [this]
    {
        engine.getMapping().setPedalInverted (invertPedalButton.getToggleState());
        saveSettings();
    };
    addChildComponent (invertPedalButton);

    midiHelpLabel.setText ("Tick your drum module under MIDI Inputs below.",
                           juce::dontSendNotification);
    midiHelpLabel.setFont (juce::FontOptions (12.0f));
    midiHelpLabel.setColour (juce::Label::textColourId, theme::textDim);
    addChildComponent (midiHelpLabel);

    hitLog.setMultiLine (true);
    hitLog.setReadOnly (true);
    hitLog.setCaretVisible (false);
    hitLog.setScrollbarsShown (true);
    hitLog.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 12.0f, 0));
    hitLog.setColour (juce::TextEditor::backgroundColourId, theme::sunken);
    hitLog.setColour (juce::TextEditor::outlineColourId, theme::bevel);
    hitLog.setColour (juce::TextEditor::textColourId, theme::text);
    addChildComponent (hitLog);

    // Two logs, side by side. If the left fills while the right stays empty,
    // the module is connected but the note map is wrong. If the left stays
    // empty, it is a connection problem. That one distinction is what makes a
    // silent kit diagnosable instead of a guessing game.
    for (auto* label : { &rawLogTitle, &hitLogTitle })
    {
        label->setFont (juce::FontOptions (12.0f, juce::Font::bold));
        label->setColour (juce::Label::textColourId, theme::textDim);
        addChildComponent (*label);
    }

    rawLogTitle.setText ("What the kit sent", juce::dontSendNotification);
    hitLogTitle.setText ("What the sampler played", juce::dontSendNotification);

    rawLog.setMultiLine (true);
    rawLog.setReadOnly (true);
    rawLog.setCaretVisible (false);
    rawLog.setScrollbarsShown (true);
    rawLog.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 12.0f, 0));
    rawLog.setColour (juce::TextEditor::backgroundColourId, theme::sunken);
    rawLog.setColour (juce::TextEditor::outlineColourId, theme::bevel);
    rawLog.setColour (juce::TextEditor::textColourId, theme::text);
    addChildComponent (rawLog);

    backButton.onClick = [this] { showSettingsPage (false); };
    addChildComponent (backButton);

    //--- pixel screen ----------------------------------------------------------
    addAndMakeVisible (deck);

    deck.onKitStep = [this] (int delta)
    {
        const int count = installedKits.empty() ? synthkit::getNumStyles() : (int) installedKits.size();

        if (count <= 0)
            return;

        const int next = ((currentKitIndex() + delta) % count + count) % count;
        const int comboId = installedKits.empty() ? next + 1 : kSampledIdBase + next;

        // Through the combo, so the existing selection path loads and saves.
        kitSelector.setSelectedId (comboId, juce::sendNotificationSync);
    };

    deck.onGainChange = [this] (double db) { gainSlider.setValue (db, juce::sendNotificationSync); };
    deck.onSettings   = [this] { showSettingsPage (true); };

    deck.onGrooveSelect = [this] (int index) { if (groovePanel != nullptr) groovePanel->selectIndex (index); };
    deck.onPlayToggle   = [this] { if (groovePanel != nullptr) groovePanel->togglePlayback(); };
    deck.onLoopToggle   = [this] { if (groovePanel != nullptr) groovePanel->setLoop (! groovePanel->getLoop()); };
    deck.onTempoChange  = [this] (double bpm) { if (groovePanel != nullptr) groovePanel->setTempoBpm (bpm); };
    deck.onFolder       = [this] { if (groovePanel != nullptr) groovePanel->openFolderChooser(); };

    deck.getGrooveFileForDrag = [this]
    {
        return groovePanel != nullptr ? groovePanel->getSelectedGrooveFile() : juce::File();
    };

    //--- grooves ---------------------------------------------------------------
    auto grooveFolder = groove::getDefaultGrooveFolder();

    if (auto* props = appProperties.getUserSettings())
    {
        const auto saved = props->getValue ("grooveFolder");

        if (saved.isNotEmpty() && juce::File (saved).isDirectory())
        {
            grooveFolder = juce::File (saved);
        }
        else
        {
            // First run: write a starter set, so the library is not an empty
            // list on a machine that has never had a groove pack on it.
            groove::writeStarterGrooves (grooveFolder);
        }
    }

    groovePanel = std::make_unique<GroovePanel> (groovePlayer);
    groovePanel->onFolderChanged = [this] { saveSettings(); };
    addChildComponent (*groovePanel);
    groovePanel->setGrooveFolder (grooveFolder);

    //--- audio + midi ----------------------------------------------------------
    // Restore the previous device selection before opening anything.
    std::unique_ptr<juce::XmlElement> savedAudioState;

    if (auto* props = appProperties.getUserSettings())
        savedAudioState = props->getXmlValue ("audioDeviceState");

    const bool firstRun = savedAudioState == nullptr;

    setAudioChannels (0, 2, savedAudioState.get());

    if (firstRun)
    {
        // Nothing saved yet. Switch on every MIDI input we can see so the kit
        // makes a sound the very first time the app is opened - otherwise a
        // new user hits a pad, gets silence, and has no reason to suspect
        // there is a checkbox hidden in Settings.
        for (const auto& input : juce::MidiInput::getAvailableDevices())
            deviceManager.setMidiInputDeviceEnabled (input.identifier, true);

        chooseSensibleFirstRunDefaults();
    }

    // A saved device that no longer opens must not leave the app mute with no
    // explanation - fall back to whatever this machine can actually play
    // through. This is the difference between "my app is broken" and "it
    // quietly moved to another output".
    if (deviceManager.getCurrentAudioDevice() == nullptr)
    {
        selectAudioDevice (getSystemOutputDeviceName());
        applyPreferredRateAndBuffer();
    }

    deviceSelector = std::make_unique<juce::AudioDeviceSelectorComponent> (
        deviceManager,
        0, 0,        // no audio inputs
        2, 2,        // stereo output
        true,        // show MIDI inputs  <- the one that matters here
        false,       // no MIDI output selector
        true,        // channels as stereo pairs
        false);      // show advanced options directly
    addChildComponent (*deviceSelector);

    // Empty identifier means "every enabled MIDI input", so devices ticked in
    // the selector start feeding us with no extra wiring.
    deviceManager.addMidiInputDeviceCallback ({}, this);
    deviceManager.addChangeListener (this);

    loadSettings();

    // Open remembered MIDI inputs now, rather than half a second from now, so
    // the device report written below reflects them.
    ensureMidiInputs();

    showSettingsPage (false);

    // Pin whatever we ended up with, so the next launch restores it.
    saveSettings();

    logAvailableDevices();

    // 2x the deck's 480x420 framebuffer plus a small border: whole-number
    // scaling only, anything else would smear the pixels.
    setSize (984, 860);
    startTimerHz (60);
}

MainComponent::~MainComponent()
{
    saveSettings();

    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);

    deviceManager.removeChangeListener (this);
    deviceManager.removeMidiInputDeviceCallback ({}, this);
    shutdownAudio();
}

//==============================================================================
void MainComponent::loadSettings()
{
    auto* props = appProperties.getUserSettings();

    if (props == nullptr)
        return;

    const double gainDb = props->getDoubleValue ("masterGainDb", 0.0);
    const double curve = props->getDoubleValue ("velocityCurve", 1.0);
    const bool inverted = props->getBoolValue ("pedalInverted", false);

    groovesVisible = props->getBoolValue ("groovesVisible", true);

    rememberedMidiInputs = juce::StringArray::fromLines (props->getValue ("midiInputs"));
    rememberedMidiInputs.removeEmptyStrings();
    midiInputsChosenByUser = props->getBoolValue ("midiInputsChosen", false);

    // Older settings only stored a synthesised kit index; carry it forward.
    auto savedKitId = props->getValue ("kitId");

    if (savedKitId.isEmpty() && props->containsKey ("kitStyle"))
        savedKitId = "synth:" + juce::String (props->getIntValue ("kitStyle", 0));

    applyKitSelection (savedKitId);
    groovesButton.setToggleState (groovesVisible, juce::dontSendNotification);

    // sendNotification so the engine picks the values up through the same
    // callbacks the UI uses - no duplicate wiring to drift out of sync.
    gainSlider.setValue (gainDb, juce::sendNotificationSync);

    // setValue only notifies on a change, and a saved 0 dB matches the default,
    // so the readout would otherwise stay blank on launch.
    gainValueLabel.setText (juce::String (gainSlider.getValue(), 1) + " dB", juce::dontSendNotification);
    curveSlider.setValue (curve, juce::sendNotificationSync);
    invertPedalButton.setToggleState (inverted, juce::sendNotificationSync);
}

void MainComponent::saveSettings()
{
    auto* props = appProperties.getUserSettings();

    if (props == nullptr)
        return;

    props->setValue ("masterGainDb", gainSlider.getValue());
    props->setValue ("velocityCurve", curveSlider.getValue());
    props->setValue ("pedalInverted", invertPedalButton.getToggleState());
    props->setValue ("groovesVisible", groovesVisible);
    props->setValue ("kitId", selectedKitId);
    props->setValue ("midiInputs", rememberedMidiInputs.joinIntoString ("\n"));
    props->setValue ("midiInputsChosen", midiInputsChosenByUser);

    if (groovePanel != nullptr)
        props->setValue ("grooveFolder", groovePanel->getGrooveFolder().getFullPathName());

    if (auto state = deviceManager.createStateXml())
        props->setValue ("audioDeviceState", state.get());

    props->saveIfNeeded();
}

namespace
{
    /** How well a driver name matches a reference device name; higher is
        better. A single shared word is not enough to identify hardware: this
        machine registers both "Focusrite Thunderbolt ASIO" and "Focusrite USB
        ASIO", and only one of them is plugged in. Counting matches separates
        them. */
    int nameMatchScore (const juce::String& reference, const juce::String& candidate)
    {
        juce::StringArray words;
        words.addTokens (reference.toLowerCase(), " ()-_", "");

        const auto lower = candidate.toLowerCase();
        int score = 0;

        for (const auto& word : words)
        {
            if (word.length() < 3)
                continue;

            // Words shared by nearly every driver carry no information.
            if (word == "speakers" || word == "audio" || word == "device"
                || word == "asio" || word == "sound" || word == "primary"
                || word == "driver" || word == "output")
                continue;

            if (lower.contains (word))
                ++score;
        }

        return score;
    }
}

juce::String MainComponent::getSystemOutputDeviceName()
{
    // The Windows device name is the most trustworthy description of what is
    // physically connected, so it is what driver names get matched against.
    for (auto* type : deviceManager.getAvailableDeviceTypes())
    {
        if (type->getTypeName() != "Windows Audio")
            continue;

        type->scanForDevices();

        const auto names = type->getDeviceNames (false);
        const int defaultIndex = type->getDefaultDeviceIndex (false);

        if (juce::isPositiveAndBelow (defaultIndex, names.size()))
            return names[defaultIndex];

        if (! names.isEmpty())
            return names[0];
    }

    return {};
}

bool MainComponent::selectAudioDevice (const juce::String& referenceName)
{
    deviceSelectionLog.clear();

    auto attempt = [this] (const juce::String& typeName, const juce::String& deviceName) -> bool
    {
        deviceManager.setCurrentAudioDeviceType (typeName, true);

        auto setup = deviceManager.getAudioDeviceSetup();
        setup.outputDeviceName = deviceName;
        setup.inputDeviceName = {};
        setup.inputChannels.clear();
        setup.useDefaultInputChannels = false;
        setup.useDefaultOutputChannels = true;

        const auto error = deviceManager.setAudioDeviceSetup (setup, true);

        if (error.isNotEmpty())
        {
            deviceSelectionLog << "    failed  " << typeName << " / " << deviceName
                               << "  (" << error << ")" << juce::newLine;
            return false;
        }

        if (deviceManager.getCurrentAudioDevice() == nullptr)
        {
            deviceSelectionLog << "    failed  " << typeName << " / " << deviceName
                               << "  (did not open)" << juce::newLine;
            return false;
        }

        deviceSelectionLog << "    OPENED  " << typeName << " / " << deviceName << juce::newLine;
        return true;
    };

    // ASIO first: on Windows it is the only path that bypasses the system
    // mixer, giving both far lower latency and the freedom to choose a sample
    // rate instead of inheriting the Windows device format. Candidates are
    // tried best-name-match first, but the name is only a hint - a driver can
    // be registered for hardware that is not present, and nothing short of an
    // open attempt tells you which one is real.
    for (auto* type : deviceManager.getAvailableDeviceTypes())
    {
        if (type->getTypeName() != "ASIO")
            continue;

        type->scanForDevices();

        std::vector<std::pair<int, juce::String>> candidates;

        for (const auto& name : type->getDeviceNames (false))
            candidates.emplace_back (nameMatchScore (referenceName, name), name);

        std::stable_sort (candidates.begin(), candidates.end(),
                          [] (const auto& a, const auto& b) { return a.first > b.first; });

        for (const auto& candidate : candidates)
            if (attempt ("ASIO", candidate.second))
                return true;
    }

    // Then the Windows driver paths, best behaved first.
    for (const char* typeName : { "Windows Audio (Low Latency Mode)",
                                  "Windows Audio",
                                  "DirectSound" })
    {
        for (auto* type : deviceManager.getAvailableDeviceTypes())
        {
            if (type->getTypeName() != typeName)
                continue;

            type->scanForDevices();

            for (const auto& name : type->getDeviceNames (false))
                if (attempt (typeName, name))
                    return true;
        }
    }

    deviceSelectionLog << "    no audio device could be opened" << juce::newLine;
    return false;
}

void MainComponent::applyPreferredRateAndBuffer()
{
    auto* device = deviceManager.getCurrentAudioDevice();

    if (device == nullptr)
        return;

    auto setup = deviceManager.getAudioDeviceSetup();

    // Windows often hands back 192 kHz with a 1920-sample buffer. For drums
    // that is 10ms of latency you can feel, and it quadruples both CPU load
    // and the memory the kit occupies, for no audible benefit.
    const auto rates = device->getAvailableSampleRates();

    for (const double preferred : { 48000.0, 44100.0 })
    {
        if (rates.contains (preferred))
        {
            setup.sampleRate = preferred;
            break;
        }
    }

    // Smallest buffer at or above 128 samples - low enough to feel immediate,
    // high enough that most drivers sustain it without glitching.
    const auto sizes = device->getAvailableBufferSizes();
    int chosen = 0;

    for (const int size : sizes)
        if (size >= 128 && (chosen == 0 || size < chosen))
            chosen = size;

    if (chosen > 0)
        setup.bufferSize = chosen;

    deviceManager.setAudioDeviceSetup (setup, true);
}

void MainComponent::chooseSensibleFirstRunDefaults()
{
    selectAudioDevice (getSystemOutputDeviceName());
    applyPreferredRateAndBuffer();
}

void MainComponent::logAvailableDevices()
{
    // Written on every launch. Audio driver problems are nearly impossible to
    // diagnose after the fact without knowing what the machine actually
    // offered, so this records it.
    juce::String report;
    report << "OpenDrummer device report" << juce::newLine << juce::newLine;

    for (auto* type : deviceManager.getAvailableDeviceTypes())
    {
        type->scanForDevices();

        report << "[" << type->getTypeName() << "]" << juce::newLine;

        const auto outputs = type->getDeviceNames (false);

        if (outputs.isEmpty())
            report << "    (no output devices)" << juce::newLine;

        for (const auto& name : outputs)
            report << "    " << name << juce::newLine;

        report << juce::newLine;
    }

    report << "[MIDI inputs]" << juce::newLine;

    for (const auto& input : juce::MidiInput::getAvailableDevices())
    {
        const bool enabled = deviceManager.isMidiInputDeviceEnabled (input.identifier);
        report << "    " << (enabled ? "[on]  " : "[off] ") << input.name
               << "  [" << input.identifier << "]" << juce::newLine;
    }

    if (deviceSelectionLog.isNotEmpty())
        report << "[device selection attempts]" << juce::newLine
               << deviceSelectionLog << juce::newLine;

    report << "[selected]" << juce::newLine;

    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        report << "    type   " << device->getTypeName() << juce::newLine
               << "    device " << device->getName() << juce::newLine
               << "    rate   " << device->getCurrentSampleRate() << juce::newLine
               << "    buffer " << device->getCurrentBufferSizeSamples() << juce::newLine;
    }
    else
    {
        report << "    (no device open)" << juce::newLine;
    }

    auto file = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                    .getChildFile ("OpenDrummer")
                    .getChildFile ("devices.log");

    file.getParentDirectory().createDirectory();
    file.replaceWithText (report);
}

void MainComponent::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // Only a change made on the SETUP page counts as the user choosing inputs.
    // Changes made anywhere else - including the app's own reconnects, or a
    // port failing to open - must not overwrite what the user asked for.
    if (settingsVisible)
    {
        juce::StringArray enabled;

        for (const auto& input : juce::MidiInput::getAvailableDevices())
            if (deviceManager.isMidiInputDeviceEnabled (input.identifier))
                enabled.add (input.name);

        rememberedMidiInputs = enabled;
        midiInputsChosenByUser = true;
    }

    saveSettings();
}

void MainComponent::showSettingsPage (bool shouldShow)
{
    settingsVisible = shouldShow;
    updatePageVisibility();
}

void MainComponent::updatePageVisibility()
{
    const bool shouldShow = settingsVisible;

    // The pixel deck draws the whole main page itself. These widgets stay
    // alive because they still hold the state, but are never shown - drawn at
    // native resolution they would break the pixel grid.
    for (juce::Component* hidden : { (juce::Component*) &kitView, (juce::Component*) &hitLabel,
                                     (juce::Component*) &midiStatusLabel, (juce::Component*) &gainValueLabel,
                                     (juce::Component*) &groovesButton, (juce::Component*) &settingsButton,
                                     (juce::Component*) &kitSelector, (juce::Component*) &kitCreditLabel,
                                     (juce::Component*) &kitLabel, (juce::Component*) &meter,
                                     (juce::Component*) &gainSlider, (juce::Component*) &gainLabel })
        hidden->setVisible (false);

    if (groovePanel != nullptr)
        groovePanel->setVisible (false);

    deck.setVisible (! shouldShow);

    if (! shouldShow)
        deck.grabKeyboardFocus();

    if (deviceSelector != nullptr)
        deviceSelector->setVisible (shouldShow);

    curveLabel.setVisible (shouldShow);
    curveSlider.setVisible (shouldShow);
    invertPedalButton.setVisible (shouldShow);
    midiHelpLabel.setVisible (shouldShow);
    rawLogTitle.setVisible (shouldShow);
    hitLogTitle.setVisible (shouldShow);
    rawLog.setVisible (shouldShow);
    hitLog.setVisible (shouldShow);
    backButton.setVisible (shouldShow);

    resized();
}

//==============================================================================
void MainComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    engine.prepare (sampleRate, samplesPerBlockExpected);
    sampledPlayer.prepare (sampleRate, samplesPerBlockExpected);
    groovePlayer.prepare (sampleRate);

    // Building a synthesised kit allocates and writes several million samples,
    // so it must not happen here - and it is skipped entirely while a recorded
    // kit is selected.
    if (synthSelected.load() && sampleRate != builtKitSampleRate)
    {
        kitReady.store (false);
        pendingKitSampleRate.store (sampleRate);
        triggerAsyncUpdate();
    }
}

void MainComponent::handleAsyncUpdate()
{
    const double sampleRate = pendingKitSampleRate.exchange (0.0);

    // The device can open before settings are read, queueing a synthesised
    // build for a kit nobody selected. rebuildKit() queues a fresh one if the
    // user switches to a synthesised kit later.
    if (sampleRate <= 0.0 || ! synthSelected.load())
        return;

    synthkit::build (engine, sampleRate, currentStyle);
    builtKitSampleRate = sampleRate;
    kitReady.store (true);
}

void MainComponent::populateKitSelector()
{
    kitSelector.clear (juce::dontSendNotification);
    installedKits = sampledkits::findInstalledKits (sampledkits::getDefaultKitsFolder());

    if (! installedKits.empty())
    {
        kitSelector.addSectionHeading ("Recorded kits");

        for (size_t i = 0; i < installedKits.size(); ++i)
            kitSelector.addItem (installedKits[i].definition->name, kSampledIdBase + (int) i);

        kitSelector.setTooltip ("Recorded drum kits, played through sfizz");
        return;
    }

    // No recorded kits on disk. The synthesised kits are a poor substitute,
    // but a drum app that makes no sound at all is worse.
    kitSelector.addSectionHeading ("Synthesised - no recorded kits found");

    for (int i = 0; i < synthkit::getNumStyles(); ++i)
        kitSelector.addItem (synthkit::getStyleName ((synthkit::Style) i), i + 1);

    kitSelector.setTooltip ("Put recorded kits in " + sampledkits::getDefaultKitsFolder().getFullPathName());
}

void MainComponent::applyKitSelection (const juce::String& savedKitId)
{
    int comboId = 0;

    if (savedKitId.startsWith ("sampled:"))
    {
        const auto id = savedKitId.fromFirstOccurrenceOf (":", false, false);

        for (size_t i = 0; i < installedKits.size(); ++i)
            if (id == installedKits[i].definition->id)
                comboId = kSampledIdBase + (int) i;
    }
    else if (savedKitId.startsWith ("synth:") && installedKits.empty())
    {
        comboId = juce::jlimit (0, synthkit::getNumStyles() - 1,
                                savedKitId.fromFirstOccurrenceOf (":", false, false).getIntValue()) + 1;
    }

    // Nothing usable saved - including a synthesised kit saved before recorded
    // kits were installed. Recorded kits win.
    if (comboId == 0)
        comboId = installedKits.empty() ? 1 : kSampledIdBase;

    kitSelector.setSelectedId (comboId, juce::dontSendNotification);
    selectKitFromCombo (comboId);
}

void MainComponent::selectKitFromCombo (int comboId)
{
    if (comboId >= kSampledIdBase)
    {
        const auto index = (size_t) (comboId - kSampledIdBase);

        if (index >= installedKits.size())
            return;

        const auto kit = installedKits[index];
        const juce::String name (kit.definition->name);

        selectedKitId = juce::String ("sampled:") + kit.definition->id;
        synthSelected.store (false);
        loadingKitName = name;
        kitCreditLabel.setText (kit.definition->credit, juce::dontSendNotification);

        const auto started = juce::Time::getMillisecondCounterHiRes();
        juce::Component::SafePointer<MainComponent> safe (this);

        sampledPlayer.loadAsync (kit, [safe, started, name] (bool ok, const juce::String& message)
        {
            if (safe == nullptr)
                return;

            const double seconds = (juce::Time::getMillisecondCounterHiRes() - started) / 1000.0;

            if (ok)
            {
                // Only now does audio move over, so the previous kit keeps
                // playing right up until the new one is ready.
                safe->useSampled.store (true);
                safe->kitNotice = name + " ready in " + juce::String (seconds, 1) + " s";
            }
            else
            {
                safe->kitNotice = name + " failed to load: " + message;
            }

            safe->kitNoticeUntil = juce::Time::getMillisecondCounter() + (ok ? 4000u : 15000u);

            // Kept on disk: a kit that fails to load once, on someone else's
            // machine, is otherwise impossible to diagnose afterwards.
            juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                .getChildFile ("OpenDrummer").getChildFile ("kits.log")
                .appendText (juce::Time::getCurrentTime().toString (true, true) + "  " + name
                             + (ok ? "  loaded in " : "  FAILED after ") + juce::String (seconds, 1)
                             + " s  (" + message + ")" + juce::newLine);
        });
    }
    else
    {
        currentStyle = (synthkit::Style) juce::jlimit (0, synthkit::getNumStyles() - 1, comboId - 1);
        selectedKitId = "synth:" + juce::String ((int) currentStyle);
        synthSelected.store (true);
        useSampled.store (false);
        kitCreditLabel.setText ("Synthesised", juce::dontSendNotification);
        rebuildKit();
    }

    saveSettings();
}

void MainComponent::rebuildKit()
{
    double rate = builtKitSampleRate;

    if (auto* device = deviceManager.getCurrentAudioDevice())
        rate = device->getCurrentSampleRate();

    if (rate <= 0.0)
        return;

    // Generating a kit writes several million samples, so it happens on the
    // message thread. The engine keeps playing the old one until the swap.
    kitReady.store (false);
    pendingKitSampleRate.store (rate);
    triggerAsyncUpdate();
}

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    bufferToFill.clearActiveBufferRegion();

    // The engine adds into the buffer, so hand it a view of just this region.
    juce::AudioBuffer<float> view (bufferToFill.buffer->getArrayOfWritePointers(),
                                   bufferToFill.buffer->getNumChannels(),
                                   bufferToFill.startSample,
                                   bufferToFill.numSamples);

    // The groove player runs on this thread and emits notes with sample
    // offsets; the engine consumes them alongside anything played live.
    scheduledMidi.clear();
    groovePlayer.process (bufferToFill.numSamples, scheduledMidi);

    if (useSampled.load())
        sampledPlayer.renderNextBlock (view, scheduledMidi);
    else
        engine.renderNextBlock (view, scheduledMidi);
}

void MainComponent::releaseResources()
{
    engine.releaseResources();
}

//==============================================================================
void MainComponent::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& message)
{
    // Roland modules stream active sensing continuously whether or not anyone
    // is playing. Counting it would let the status line report healthy green
    // traffic before a single pad had been struck, which is worse than saying
    // nothing. Transport and clock bytes are dropped for the same reason.
    if (message.isActiveSense() || message.isMidiClock() || message.isMidiStart()
        || message.isMidiStop() || message.isMidiContinue()
        || message.isSongPositionPointer())
        return;

    // Record everything else, mapped or not, then forward what we understand.
    // The recording is what makes an unmapped pad visible.
    RawMidiEvent event;
    event.channel = message.getChannel();

    // Read once, so a message is never half-routed across a kit switch.
    const bool sampled = useSampled.load();

    if (message.isNoteOn())
    {
        event.kind = RawMidiEvent::Kind::NoteOn;
        event.data1 = message.getNoteNumber();
        event.data2 = message.getVelocity();
        event.mapped = engine.getMapping().articulationForNote (message.getNoteNumber())
                         != midimap::Articulation::None;

        if (! event.mapped)
            unmappedNoteCount.fetch_add (1);

        noteOnCount.fetch_add (1);

        if (! sampled)
            engine.noteOn (message.getNoteNumber(), message.getFloatVelocity());
    }
    else if (message.isNoteOff())
    {
        // Not forwarded - a drum sample rings out on its own. Logged anyway,
        // because seeing note-offs confirms the cable is alive.
        event.kind = RawMidiEvent::Kind::NoteOff;
        event.data1 = message.getNoteNumber();
    }
    else if (message.isController())
    {
        event.kind = RawMidiEvent::Kind::CC;
        event.data1 = message.getControllerNumber();
        event.data2 = message.getControllerValue();
        event.mapped = message.getControllerNumber() == midimap::kHiHatPedalCC;

        if (! sampled)
            engine.controlChange (message.getControllerNumber(), message.getControllerValue());
    }
    else if (message.isAftertouch())
    {
        // Roland modules signal a hand-choke as polyphonic aftertouch on the
        // cymbal's own note number.
        event.kind = RawMidiEvent::Kind::Aftertouch;
        event.data1 = message.getNoteNumber();
        event.data2 = message.getAfterTouchValue();
        event.mapped = true;

        if (! sampled)
            engine.choke (message.getNoteNumber());
    }

    // Recorded kits take the message whole: they need note-offs and pedal
    // positions too, and the player filters out anything a kit should not see.
    if (sampled)
        sampledPlayer.handleMidi (message);

    const juce::uint64 writeIndex = rawWriteIndex.load();
    rawRing[(size_t) (writeIndex % kRawRingSize)] = event;
    rawWriteIndex.store (writeIndex + 1);

    midiMessageCount.fetch_add (1);
}

//==============================================================================
int MainComponent::currentKitIndex() const
{
    if (selectedKitId.startsWith ("sampled:"))
    {
        const auto id = selectedKitId.fromFirstOccurrenceOf (":", false, false);

        for (size_t i = 0; i < installedKits.size(); ++i)
            if (id == installedKits[i].definition->id)
                return (int) i;

        return 0;
    }

    return (int) currentStyle;
}

void MainComponent::refreshDeckSnapshot()
{
    auto& s = deckSnapshot;
    const bool sampled = useSampled.load();

    s.kitNames.clearQuick();

    if (! installedKits.empty())
        for (const auto& kit : installedKits)
            s.kitNames.add (kit.definition->name);
    else
        for (int i = 0; i < synthkit::getNumStyles(); ++i)
            s.kitNames.add (synthkit::getStyleName ((synthkit::Style) i));

    s.kitIndex = currentKitIndex();
    s.kitCredit = kitCreditLabel.getText();
    s.kitLoading = sampledPlayer.isLoading();

    // A short status line: the full device name will not fit 34 characters.
    s.hiHatOpenness = sampled ? sampledPlayer.getOpenness() : engine.getMapping().getOpenness();
    const int voices = sampled ? sampledPlayer.getActiveVoiceCount() : engine.getActiveVoiceCount();

    juce::String status;

    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        const double rate = device->getCurrentSampleRate();
        status << device->getTypeName().upToFirstOccurrenceOf (" ", false, false).toUpperCase() << " "
               << juce::String (rate / 1000.0, 1) << "K "
               << juce::String (device->getCurrentBufferSizeSamples() / juce::jmax (1.0, rate) * 1000.0, 1) << "MS";
    }
    else
    {
        status << "NO AUDIO DEVICE";
    }

    status << "  VOX " << voices << "  HAT " << juce::roundToInt (s.hiHatOpenness * 100.0f) << "%";
    s.statusLine = status;

    s.hitText = hitLabel.getText();
    const auto hitColour = hitLabel.findColour (juce::Label::textColourId);
    s.hitState = hitColour == theme::glassOk ? 1 : hitColour == theme::glassWarn ? 2 : 0;

    // The status label's sentence is written for a wide line; the dialog box
    // holds two short ones, so the deck gets its own phrasing.
    {
        const auto device = cachedMidiInputNames;
        const auto notes = noteOnCount.load();
        const auto unmapped = unmappedNoteCount.load();

        if (device.isEmpty())
            s.midiText = "No MIDI input. Open SETUP.";
        else if (notes == 0 && midiMessageCount.load() == 0)
            s.midiText = device + ": waiting";
        else if (notes == 0)
            s.midiText = device + ": no hits yet";
        else
            s.midiText = device + ": " + juce::String (notes) + " hits"
                       + (unmapped > 0 ? ", " + juce::String (unmapped) + " unmapped" : juce::String());
    }
    const auto midiColour = midiStatusLabel.findColour (juce::Label::textColourId);
    s.midiState = midiColour == theme::glassOk ? 0 : midiColour == theme::glassError ? 2 : 1;

    s.meanSquare = sampled ? sampledPlayer.getMeanSquare() : engine.getMeanSquare();
    s.peak = sampled ? sampledPlayer.getPeakLevel() : engine.getPeakLevel();
    s.gainDb = gainSlider.getValue();

    s.grooveNames.clearQuick();
    s.grooveBpm.clearQuick();

    if (groovePanel != nullptr)
    {
        for (const auto& pattern : groovePanel->getLibrary().getPatterns())
        {
            s.grooveNames.add (pattern.name);
            s.grooveBpm.add (juce::roundToInt (pattern.bpm));
        }

        s.grooveSelected = groovePanel->getSelectedIndex();
        s.looping = groovePanel->getLoop();
        s.tempo = groovePanel->getTempoBpm();

        const auto* pattern = groovePanel->getLibrary().getPattern (s.grooveSelected);
        s.bars = pattern != nullptr ? pattern->bars : 0;
    }

    s.playing = groovePlayer.isPlaying();
    s.progress = groovePlayer.getProgress();
    s.bar = groovePlayer.getCurrentBar();

    deck.setSnapshot (s);
}

void MainComponent::timerCallback()
{
    pollEngine();
    pollMidiMonitor();
    refreshMidiStatus();
    refreshDeckSnapshot();

    const bool sampled = useSampled.load();
    const auto openness = sampled ? sampledPlayer.getOpenness() : engine.getMapping().getOpenness();
    const int voices = sampled ? sampledPlayer.getActiveVoiceCount() : engine.getActiveVoiceCount();
    kitView.setHiHatOpenness (openness);

    juce::String status;
    status << describeAudioDevice()
           << "     voices " << voices
           << "     hi-hat " << juce::String (juce::roundToInt (openness * 100.0f)) << "%";

    if (sampledPlayer.isLoading())
        status = "Loading " + loadingKitName + " ...";
    else if (synthSelected.load() && ! kitReady.load())
        status = "Building " + synthkit::getStyleName (currentStyle) + " kit";
    else if (juce::Time::getMillisecondCounter() < kitNoticeUntil)
        status = kitNotice;

    if (status != statusText)
    {
        statusText = status;
        repaint (statusBounds.expanded (4));
    }
}

void MainComponent::appendMidiLog (const juce::String& line)
{
    juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("OpenDrummer").getChildFile ("midi.log")
        .appendText (juce::Time::getCurrentTime().toString (true, true) + "  " + line + juce::newLine);
}

void MainComponent::ensureMidiInputs()
{
    const auto available = juce::MidiInput::getAvailableDevices();

    juce::StringArray ids;

    for (const auto& device : available)
        ids.add (device.identifier);

    // Nothing remembered and nothing chosen: take every input. A drum app with
    // no MIDI is useless, and a lost setting must not leave it that way.
    const bool wantAll = rememberedMidiInputs.isEmpty() && ! midiInputsChosenByUser;

    for (const auto& device : available)
    {
        if (! wantAll && ! rememberedMidiInputs.contains (device.name))
            continue;

        // A port that vanished and came back holds a dead handle even though it
        // still reads as enabled. Close it so it can be opened afresh.
        const bool reappeared = ! lastSeenMidiIds.isEmpty() && ! lastSeenMidiIds.contains (device.identifier);

        if (reappeared && deviceManager.isMidiInputDeviceEnabled (device.identifier))
            deviceManager.setMidiInputDeviceEnabled (device.identifier, false);

        if (deviceManager.isMidiInputDeviceEnabled (device.identifier))
        {
            midiOpenFailuresLogged.removeString (device.identifier);
            continue;
        }

        deviceManager.setMidiInputDeviceEnabled (device.identifier, true);

        if (deviceManager.isMidiInputDeviceEnabled (device.identifier))
        {
            appendMidiLog ("opened " + device.name + "  [" + device.identifier + "]");
            midiOpenFailuresLogged.removeString (device.identifier);
        }
        else if (! midiOpenFailuresLogged.contains (device.identifier))
        {
            // Logged once, retried every half second until it succeeds.
            appendMidiLog ("could not open " + device.name + " - another program may be using it. Retrying.");
            midiOpenFailuresLogged.add (device.identifier);
        }
    }

    lastSeenMidiIds = ids;
}

void MainComponent::refreshMidiStatus()
{
    // Re-enumerate devices about twice a second rather than every frame.
    if (--midiRecheckCountdown <= 0)
    {
        midiRecheckCountdown = 30;

        ensureMidiInputs();

        juce::StringArray enabled;

        for (const auto& input : juce::MidiInput::getAvailableDevices())
            if (deviceManager.isMidiInputDeviceEnabled (input.identifier))
                enabled.add (input.name);

        cachedMidiInputNames = enabled.joinIntoString (", ");
    }

    const auto messages = midiMessageCount.load();
    const auto notes = noteOnCount.load();
    const auto unmapped = unmappedNoteCount.load();

    juce::String text;
    juce::Colour colour;

    if (cachedMidiInputNames.isEmpty())
    {
        text = "MIDI: no input enabled - open Settings and tick your drum module";
        colour = theme::glassError;
    }
    else if (notes == 0 && messages == 0)
    {
        text = "MIDI: " + cachedMidiInputNames + " - connected, waiting for a hit";
        colour = theme::glassWarn;
    }
    else if (notes == 0)
    {
        // The port is alive but nothing has been struck. Saying so is the
        // difference between "your cable works" and "your kit works".
        text = "MIDI: " + cachedMidiInputNames + " - receiving data, no pad hits yet";
        colour = theme::glassWarn;
    }
    else
    {
        text = "MIDI: " + cachedMidiInputNames + " - " + juce::String (notes)
             + (notes == 1 ? " hit" : " hits");
        colour = theme::glassOk;

        if (unmapped > 0)
        {
            text << "   |   " << juce::String (unmapped) << " unmapped - see Settings";
            colour = theme::glassWarn;
        }
    }

    midiStatusLabel.setText (text, juce::dontSendNotification);
    midiStatusLabel.setColour (juce::Label::textColourId, colour);
}

void MainComponent::pollMidiMonitor()
{
    const juce::uint64 writeIndex = rawWriteIndex.load();

    // Skip anything lost if the ring wrapped while the UI was busy.
    if (writeIndex > (juce::uint64) kRawRingSize
        && rawReadIndex < writeIndex - (juce::uint64) kRawRingSize)
    {
        rawReadIndex = writeIndex - (juce::uint64) kRawRingSize;
    }

    if (rawReadIndex == writeIndex)
        return;

    int unmappedNote = -1;
    int unmappedVelocity = 0;

    for (; rawReadIndex < writeIndex; ++rawReadIndex)
    {
        const auto& e = rawRing[(size_t) (rawReadIndex % kRawRingSize)];

        juce::String line;

        switch (e.kind)
        {
            case RawMidiEvent::Kind::NoteOn:
                line << "note " << juce::String (e.data1).paddedLeft (' ', 3)
                     << "  vel " << juce::String (e.data2).paddedLeft (' ', 3);

                if (! e.mapped)
                {
                    line << "   UNMAPPED";
                    unmappedNote = e.data1;
                    unmappedVelocity = e.data2;
                }
                break;

            case RawMidiEvent::Kind::NoteOff:
                line << "off  " << juce::String (e.data1).paddedLeft (' ', 3);
                break;

            case RawMidiEvent::Kind::CC:
                line << "CC   " << juce::String (e.data1).paddedLeft (' ', 3)
                     << "  val " << juce::String (e.data2).paddedLeft (' ', 3);

                if (e.data1 == midimap::kHiHatPedalCC)
                    line << "   hi-hat pedal";
                break;

            case RawMidiEvent::Kind::Aftertouch:
                line << "chok " << juce::String (e.data1).paddedLeft (' ', 3)
                     << "  val " << juce::String (e.data2).paddedLeft (' ', 3);
                break;

            case RawMidiEvent::Kind::Other:
            default:
                line << "other";
                break;
        }

        recentRaw.push_front (line);
    }

    while (recentRaw.size() > 300)
        recentRaw.pop_back();

    // An unmapped pad is the most actionable thing we can report, so it takes
    // the headline over a mapped hit in the same batch.
    if (unmappedNote >= 0)
    {
        hitLabel.setText ("Note " + juce::String (unmappedNote)
                            + " unmapped   vel " + juce::String (unmappedVelocity),
                          juce::dontSendNotification);
        hitLabel.setColour (juce::Label::textColourId, theme::glassWarn);
    }

    if (settingsVisible)
    {
        juce::StringArray lines;

        for (const auto& entry : recentRaw)
            lines.add (entry);

        rawLog.setText (lines.joinIntoString ("\n"), false);
    }
}

void MainComponent::pollEngine()
{
    hitScratch.clear();
    hitReadIndex = engine.readHitsSince (hitReadIndex, hitScratch);
    sampledHitReadIndex = sampledPlayer.readHitsSince (sampledHitReadIndex, hitScratch);

    if (hitScratch.empty())
        return;

    for (const auto& hit : hitScratch)
    {
        kitView.registerHit (hit.articulation, hit.velocity);
        deck.registerHit (hit.articulation, hit.velocity);

        const auto name = midimap::getArticulationName (hit.articulation);
        const int velocity127 = juce::roundToInt (hit.velocity * 127.0f);

        juce::String line;
        line << name.paddedRight (' ', 16)
             << " note " << juce::String (hit.noteNumber).paddedLeft (' ', 3)
             << "   vel " << juce::String (velocity127).paddedLeft (' ', 3);

        // Recorded kits choose their own layers inside sfizz; only the
        // synthesised engine can report which one it played.
        if (hit.layerIndex >= 0)
            line << "   layer " << (hit.layerIndex + 1)
                 << "   take " << (hit.roundRobinIndex + 1);

        recentHits.push_front (line);
    }

    while (recentHits.size() > 300)
        recentHits.pop_back();

    const auto& latest = hitScratch.back();
    hitLabel.setText (midimap::getArticulationName (latest.articulation)
                        + "   vel " + juce::String (juce::roundToInt (latest.velocity * 127.0f)),
                      juce::dontSendNotification);
    hitLabel.setColour (juce::Label::textColourId, theme::glassOk);

    // Only rebuild the log text while it is actually on screen.
    if (settingsVisible)
    {
        juce::StringArray lines;

        for (const auto& entry : recentHits)
            lines.add (entry);

        hitLog.setText (lines.joinIntoString ("\n"), false);
    }
}

juce::String MainComponent::describeAudioDevice() const
{
    auto* device = deviceManager.getCurrentAudioDevice();

    if (device == nullptr)
        return "No audio device - open Settings";

    const double sampleRate = device->getCurrentSampleRate();
    const int bufferSize = device->getCurrentBufferSizeSamples();

    if (sampleRate <= 0.0)
        return device->getTypeName() + " (not running)";

    const double bufferMs = bufferSize / sampleRate * 1000.0;

    juce::String text;
    text << device->getTypeName() << " / " << device->getName()
         << "   " << juce::String (sampleRate / 1000.0, 1) << " kHz"
         << "   " << juce::String (bufferMs, 1) << " ms";

    return text;
}

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
    if (! settingsVisible)
        return;   // the deck is opaque and covers everything

    const auto full = getLocalBounds().toFloat();

    // One brushed aluminium faceplate across the whole front.
    theme::drawBrushedMetal (g, full);

    // Walnut cabinet, visible down both sides as it would be on the real thing.
    constexpr float kCheek = 18.0f;
    theme::drawWalnutCheek (g, { 0.0f, 0.0f, kCheek, full.getHeight() }, true);
    theme::drawWalnutCheek (g, { full.getWidth() - kCheek, 0.0f, kCheek, full.getHeight() }, false);

    // Scribed line under the header: a dark groove with a lit lower edge, the
    // way a line pressed into metal actually catches light.
    const float ruleX = kCheek;
    const float ruleW = full.getWidth() - kCheek * 2.0f;
    g.setColour (juce::Colours::black.withAlpha (0.30f));
    g.fillRect (ruleX, 66.0f, ruleW, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.5f));
    g.fillRect (ruleX, 67.0f, ruleW, 1.0f);

    theme::drawWordmark (g, wordmarkBounds, 18.0f);

    // The footer's dial window, behind the hit readout and meter.
    if (! settingsVisible && ! statusStripBounds.isEmpty())
        theme::drawGlassWindow (g, statusStripBounds.toFloat(), 4.0f);

    // The readout is sunk behind glass - the only lit thing on the panel.
    theme::drawGlassWindow (g, statusBounds.toFloat().expanded (7.0f, 3.0f), 2.5f);
    theme::drawDialText (g, statusText, statusBounds, juce::Justification::centredLeft, 11.5f);
}

void MainComponent::resized()
{
    deck.setBounds (getLocalBounds());

    if (! settingsVisible)
        return;

    auto area = getLocalBounds();

    // Keep clear of the walnut cheeks.
    area.removeFromLeft (18);
    area.removeFromRight (18);

    auto header = area.removeFromTop (66).reduced (14, 8);

    // Kit selector on top, the kit's author and licence beneath it - the
    // CC BY kits require the credit to travel with the sound.
    auto kitColumn = header.removeFromRight (300);
    auto kitRow = kitColumn.removeFromTop (26);
    kitSelector.setBounds (kitRow.removeFromRight (190));
    kitRow.removeFromRight (8);
    kitLabel.setBounds (kitRow);
    kitColumn.removeFromTop (2);
    kitCreditLabel.setBounds (kitColumn);

    wordmarkBounds = header.removeFromTop (26);
    statusBounds = header;

    area.reduce (16, 12);

    if (settingsVisible)
    {
        backButton.setBounds (area.removeFromTop (28).removeFromLeft (120));
        area.removeFromTop (12);

        auto row = area.removeFromTop (26);
        curveLabel.setBounds (row.removeFromLeft (110));
        curveSlider.setBounds (row);
        area.removeFromTop (8);

        invertPedalButton.setBounds (area.removeFromTop (26));
        area.removeFromTop (12);

        midiHelpLabel.setBounds (area.removeFromTop (20));
        area.removeFromTop (4);

        // Device setup gets the lion's share; the two logs fill what is left.
        auto logs = area.removeFromBottom (juce::jmax (150, area.getHeight() / 3));
        deviceSelector->setBounds (area);

        logs.removeFromTop (10);
        auto titles = logs.removeFromTop (18);

        const int half = logs.getWidth() / 2 - 6;

        rawLogTitle.setBounds (titles.removeFromLeft (half));
        titles.removeFromLeft (12);
        hitLogTitle.setBounds (titles);

        rawLog.setBounds (logs.removeFromLeft (half));
        logs.removeFromLeft (12);
        hitLog.setBounds (logs);
        return;
    }

    // Main page: the kit fills the window, controls sit underneath.
    // Laid out like a receiver's front: the big knob on the left, the lit dial
    // window in the middle, the push keys on the right.
    auto footer = area.removeFromBottom (92);

    auto knobColumn = footer.removeFromLeft (92);
    gainLabel.setBounds (knobColumn.removeFromTop (14));
    gainValueLabel.setBounds (knobColumn.removeFromBottom (14));
    gainSlider.setBounds (knobColumn.withSizeKeepingCentre (64, 64));

    footer.removeFromLeft (14);

    auto keys = footer.removeFromRight (96);
    keys = keys.withSizeKeepingCentre (keys.getWidth(), 28 * 2 + 8);
    groovesButton.setBounds (keys.removeFromTop (28));
    keys.removeFromTop (8);
    settingsButton.setBounds (keys.removeFromTop (28));

    footer.removeFromRight (14);

    statusStripBounds = footer.reduced (0, 2);
    auto dial = statusStripBounds.reduced (10, 6);

    // The meter keeps a real VU's proportions, roughly 3:2.
    const int meterHeight = dial.getHeight();
    meter.setBounds (dial.removeFromRight (juce::roundToInt (meterHeight * 1.55f)));
    dial.removeFromRight (12);

    dial.removeFromLeft (4);
    auto text = dial.withSizeKeepingCentre (dial.getWidth(), 30 + 6 + 18);
    hitLabel.setBounds (text.removeFromTop (30));
    text.removeFromTop (6);
    midiStatusLabel.setBounds (text.removeFromTop (18));

    area.removeFromBottom (10);

    // The groove panel takes the lower part of the window, leaving the kit
    // visible above it so you can watch it play.
    if (groovesVisible && groovePanel != nullptr)
    {
        groovePanel->setBounds (area.removeFromBottom (juce::jmax (210, area.getHeight() * 46 / 100)));
        area.removeFromBottom (10);
    }

    kitView.setBounds (area);
}
