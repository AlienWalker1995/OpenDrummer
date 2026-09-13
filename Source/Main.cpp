#include "MainComponent.h"
#include "SampledKitPlayer.h"
#include "SampledKits.h"
#include "SynthKit.h"
#include "Theme.h"

#include <juce_audio_formats/juce_audio_formats.h>

namespace
{
    void writeWav (const juce::File& file, const juce::AudioBuffer<float>& audio, double sampleRate)
    {
        file.deleteFile();

        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (file);

        auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions{}
                                                     .withSampleRate (sampleRate)
                                                     .withNumChannels (audio.getNumChannels())
                                                     .withBitsPerSample (24));

        if (writer != nullptr)
            writer->writeFromAudioSampleBuffer (audio, 0, audio.getNumSamples());
    }

    /** Plays one hit of each key drum on every downloaded kit through sfizz,
        using the Roland note numbers a real module sends, and writes WAVs. It
        proves end to end that each kit loads, maps and makes sound. */
    void renderSampledKits (const juce::File& folder)
    {
        folder.createDirectory();

        constexpr double kSampleRate = 48000.0;
        constexpr int kBlock = 512;
        constexpr double kSeconds = 4.0;

        // note, pedal CC4 to send first (-1 = none). 127 closed, 0 open.
        struct Hit { const char* name; int note; int pedal; };

        const Hit hits[] =
        {
            { "kick", 36, -1 }, { "snare", 38, -1 }, { "rimshot", 40, -1 },
            { "tom1", 48, -1 }, { "tom4", 41, -1 },
            { "hat_closed", 42, 127 }, { "hat_open", 46, 0 },
            { "crash1", 49, -1 }, { "crash2", 57, -1 }, { "ride", 51, -1 }
        };

        const midimap::Mapping mapping;
        juce::String report;

        for (const auto& kit : sampledkits::findInstalledKits (sampledkits::getDefaultKitsFolder()))
        {
            SampledKitPlayer player (mapping);
            player.prepare (kSampleRate, kBlock);

            const auto started = juce::Time::getMillisecondCounterHiRes();
            juce::String error;

            if (! player.loadNow (kit, error))
            {
                report << kit.definition->name << ": LOAD FAILED - " << error << juce::newLine;
                continue;
            }

            report << kit.definition->name << ": loaded in "
                   << juce::String ((juce::Time::getMillisecondCounterHiRes() - started) / 1000.0, 1)
                   << " s" << juce::newLine;

            for (const auto& hit : hits)
            {
                player.allSoundOff();

                juce::AudioBuffer<float> out (2, (int) (kSampleRate * kSeconds));
                out.clear();

                for (int pos = 0; pos < out.getNumSamples(); pos += kBlock)
                {
                    const int n = juce::jmin (kBlock, out.getNumSamples() - pos);
                    juce::MidiBuffer events;

                    if (pos == 0)
                    {
                        if (hit.pedal >= 0)
                            events.addEvent (juce::MidiMessage::controllerEvent (10, 4, hit.pedal), 0);

                        events.addEvent (juce::MidiMessage::noteOn (10, hit.note, (juce::uint8) 110), 1);
                    }

                    juce::AudioBuffer<float> block (2, n);
                    player.renderNextBlock (block, events);

                    for (int ch = 0; ch < 2; ++ch)
                        out.copyFrom (ch, pos, block, ch, 0, n);
                }

                writeWav (folder.getChildFile (juce::String (kit.definition->id) + "_" + hit.name + ".wav"),
                          out, kSampleRate);
            }
        }

        folder.getChildFile ("render-report.txt").replaceWithText (report);
    }

    /** Renders one hit of each key articulation for every kit to WAV files,
        through the real engine - the same trigger, velocity-layer and voice
        path live playing uses - then exits. It exists so the kits can be
        measured and auditioned without a drum kit or an audio device. */
    void renderKits (const juce::File& folder)
    {
        folder.createDirectory();

        constexpr double kSampleRate = 48000.0;
        constexpr int kBlock = 512;
        constexpr double kSeconds = 4.5;

        struct Hit { const char* name; int note; };

        const Hit hits[] =
        {
            { "kick", 36 }, { "snare", 38 }, { "rimshot", 40 },
            { "tom1", 48 }, { "tom4", 41 },
            { "hat_closed", 42 }, { "hat_open", 46 },
            { "crash1", 49 }, { "crash2", 57 }, { "ride", 51 }
        };

        juce::WavAudioFormat wav;

        for (int s = 0; s < synthkit::getNumStyles(); ++s)
        {
            const auto style = (synthkit::Style) s;

            DrumEngine engine;
            engine.prepare (kSampleRate, kBlock);
            synthkit::build (engine, kSampleRate, style);

            const auto kitName = synthkit::getStyleName (style).replaceCharacters (" -", "__");

            for (const auto& hit : hits)
            {
                // Clear any voice still ringing from the previous hit.
                engine.releaseResources();

                juce::AudioBuffer<float> out (2, (int) (kSampleRate * kSeconds));
                out.clear();

                engine.noteOn (hit.note, 0.9f);

                const juce::MidiBuffer noScheduledNotes;

                for (int pos = 0; pos < out.getNumSamples(); pos += kBlock)
                {
                    const int n = juce::jmin (kBlock, out.getNumSamples() - pos);
                    juce::AudioBuffer<float> view (out.getArrayOfWritePointers(), 2, pos, n);
                    engine.renderNextBlock (view, noScheduledNotes);
                }

                const auto file = folder.getChildFile (kitName + "_" + hit.name + ".wav");
                file.deleteFile();

                std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (file);

                auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions{}
                                                             .withSampleRate (kSampleRate)
                                                             .withNumChannels (2)
                                                             .withBitsPerSample (24));

                if (writer != nullptr)
                    writer->writeFromAudioSampleBuffer (out, 0, out.getNumSamples());
            }
        }
    }
}

#include <juce_gui_basics/juce_gui_basics.h>

class OpenDrummerApplication : public juce::JUCEApplication
{
public:
    OpenDrummerApplication() = default;

    const juce::String getApplicationName() override    { return JUCE_APPLICATION_NAME_STRING; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override          { return false; }

    void initialise (const juce::String& commandLine) override
    {
        const auto args = juce::StringArray::fromTokens (commandLine, true);
        const int renderIndex = args.indexOf ("--render-kits");

        const int sampledIndex = args.indexOf ("--render-sampled");

        if (sampledIndex >= 0 && sampledIndex + 1 < args.size())
        {
            renderSampledKits (juce::File (args[sampledIndex + 1].unquoted()));
            quit();
            return;
        }

        if (renderIndex >= 0 && renderIndex + 1 < args.size())
        {
            renderKits (juce::File (args[renderIndex + 1].unquoted()));
            quit();
            return;
        }

        mainWindow = std::make_unique<MainWindow> (getApplicationName());
    }

    void shutdown() override
    {
        mainWindow = nullptr;
    }

    void systemRequestedQuit() override
    {
        quit();
    }

private:
    class MainWindow : public juce::DocumentWindow
    {
    public:
        explicit MainWindow (const juce::String& name)
            : DocumentWindow (name,
                              theme::background,
                              DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new MainComponent(), true);
            setResizable (true, true);
            setResizeLimits (720, 560, 2400, 1800);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }

        void closeButtonPressed() override
        {
            JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (OpenDrummerApplication)
