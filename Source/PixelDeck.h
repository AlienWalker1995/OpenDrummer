#pragma once

#include "MidiMapping.h"
#include "Pixel.h"

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <functional>

/**
    The main screen, drawn as a retro video game.

    Everything is rendered into one 480x420 indexed framebuffer and scaled up
    by a whole number, so every element - the kit, the text, the buttons -
    shares a single pixel grid. The component owns no app state: the host
    hands it a Snapshot each frame and receives user actions through the
    callbacks, which keeps all the real logic where it already lived.
*/
class PixelDeck : public juce::Component,
                  private juce::Timer
{
public:
    static constexpr int kWidth = 480;
    static constexpr int kHeight = 420;

    /** Everything the screen shows, refreshed by the host every UI frame. */
    struct Snapshot
    {
        juce::StringArray kitNames;
        int kitIndex = 0;
        juce::String kitCredit;
        bool kitLoading = false;
        bool kitLoaded = false;
        float kitLoadSeconds = 0.0f;

        juce::String statusLine;     // "ASIO 48K 2.7MS  VOX 3  HAT 20%"
        float hiHatOpenness = 0.0f;

        juce::String hitText;
        int hitState = 0;            // 0 idle, 1 hit, 2 unmapped

        juce::String midiText;
        int midiState = 0;           // 0 ok, 1 waiting/warning, 2 error

        float meanSquare = 0.0f;
        float peak = 0.0f;

        double gainDb = 0.0;

        juce::StringArray grooveNames;
        juce::Array<int> grooveBpm;
        int grooveSelected = -1;
        bool playing = false;
        bool looping = true;
        double tempo = 120.0;
        float progress = 0.0f;
        int bar = 0;
        int bars = 0;
    };

    PixelDeck();

    void setSnapshot (const Snapshot& s) { snapshot = s; }

    /** Makes the matching piece of the kit react to a hit. */
    void registerHit (midimap::Articulation articulation, float velocity);

    //==============================================================================
    std::function<void (int delta)>   onKitStep;
    std::function<void (double db)>   onGainChange;
    std::function<void (int index)>   onGrooveSelect;
    std::function<void()>             onPlayToggle;
    std::function<void()>             onLoopToggle;
    std::function<void (double bpm)>  onTempoChange;
    std::function<void()>             onFolder;
    std::function<void()>             onSettings;
    std::function<juce::File()>       getGrooveFileForDrag;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    //==============================================================================
    enum class Target
    {
        None,
        KitPrev, KitNext,
        GrooveRow, GrooveUp, GrooveDown,
        Play, Loop, TempoDown, TempoUp, TempoValue,
        Folder, DragMidi,
        VolDown, VolUp, VolBar,
        Settings
    };

    enum Piece
    {
        Kick = 0, Snare, HiHat, Tom1, Tom2, Tom3, Tom4, Crash1, Crash2, Ride,
        NumPieces
    };

    struct Spark { float x = 0, y = 0, vx = 0, vy = 0; int life = 0; };
    struct Floater { int x = 0, y = 0, value = 0, life = 0; };

    //==============================================================================
    void timerCallback() override;
    void render();

    void drawHeader();
    void drawStage();
    void drawGrooves();
    void drawFooter();

    void drawButton (juce::Rectangle<int> r, const juce::String& label, Target target,
                     bool active = false, std::uint8_t activeFace = px::Green);
    void drawCymbal (int cx, int cy, int rx, float glow, int wobble);
    void drawDrum (int cx, int topY, int rx, int ry, int bodyHeight, float glow);
    void drawStand (int x, int topY, int floorY, bool tripod);
    void drawKick (int cx, int cy, int r, float glow);

    juce::Point<int> hitPointFor (Piece piece) const;
    static Piece pieceFor (midimap::Articulation a);

    Target targetAt (juce::Point<int> logical, int* rowOut = nullptr) const;
    juce::Point<int> toLogical (juce::Point<float> screen) const;
    double gainFromBarX (int x) const;

    //==============================================================================
    px::Canvas canvas { kWidth, kHeight };
    juce::Image frame { juce::Image::ARGB, kWidth, kHeight, true };
    Snapshot snapshot;

    int scale = 1;
    juce::Point<int> offset;

    Target hovered = Target::None;
    Target pressed = Target::None;
    int hoveredRow = -1;
    int grooveScroll = 0;

    bool draggingOut = false;
    double tempoDragStart = 0.0;

    std::array<float, NumPieces> glow {};
    std::array<int, NumPieces> wobble {};
    std::array<Spark, 64> sparks;
    std::array<Floater, NumPieces> floaters;

    juce::Random random;
    int frameCount = 0;
    float meterLevel = 0.0f;
    float meterHold = 0.0f;
    int clipFrames = 0;

    // Layout, in logical pixels.
    static constexpr int kRowHeight = 10;
    static constexpr int kVisibleRows = 8;
    const juce::Rectangle<int> kitPrev    { 296, 18, 14, 14 };
    const juce::Rectangle<int> kitNext    { 458, 18, 14, 14 };
    const juce::Rectangle<int> grooveList { 8, 256, 300, kRowHeight * kVisibleRows };
    const juce::Rectangle<int> grooveUp   { 294, 244, 14, 11 };
    const juce::Rectangle<int> grooveDown { 294, 337, 14, 1 };
    const juce::Rectangle<int> playBtn    { 318, 248, 74, 18 };
    const juce::Rectangle<int> loopBtn    { 398, 248, 74, 18 };
    const juce::Rectangle<int> tempoDown  { 318, 282, 18, 16 };
    const juce::Rectangle<int> tempoValue { 338, 282, 114, 16 };
    const juce::Rectangle<int> tempoUp    { 454, 282, 18, 16 };
    const juce::Rectangle<int> folderBtn  { 318, 318, 74, 18 };
    const juce::Rectangle<int> dragBtn    { 398, 318, 74, 18 };
    const juce::Rectangle<int> volDown    { 8, 364, 14, 14 };
    const juce::Rectangle<int> volBar     { 24, 364, 98, 14 };
    const juce::Rectangle<int> volUp      { 124, 364, 14, 14 };
    const juce::Rectangle<int> settingsBtn{ 384, 392, 88, 18 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PixelDeck)
};
