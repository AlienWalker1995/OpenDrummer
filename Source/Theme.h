#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/**
    OpenDrummer's visual identity: the silver-face receiver, c.1972.

    The reference is the era's defining audio look - a brushed aluminium
    faceplate in a walnut cabinet, legends silk-screened straight onto the
    metal, and the working parts sunk behind a dark glass window lit in
    Marantz blue. Pioneer, Marantz and Sansui all shipped this, and the CR-78
    put a drum machine in the same living room.

    The important consequence for the code: there are two ink colours. Text on
    the faceplate is dark, because silkscreen on silver is dark. Text inside a
    display window is light. Using one ink for both leaves half the interface
    unreadable, which is exactly what happens if you treat this as a dark theme
    with the colours inverted.
*/
namespace theme
{
    // --- cabinet --------------------------------------------------------------
    const juce::Colour walnut       { 0xff8a603c };
    const juce::Colour walnutDark   { 0xff44301c };

    // --- brushed aluminium faceplate -----------------------------------------
    const juce::Colour alu          { 0xffd6d3cb };
    const juce::Colour aluMid       { 0xffbcb9b1 };
    const juce::Colour aluDark      { 0xff8e8b84 };
    const juce::Colour aluEdge      { 0xff6f6c66 };

    // --- display glass --------------------------------------------------------
    const juce::Colour glass        { 0xff0e1418 };
    const juce::Colour glassDeep    { 0xff06090b };
    const juce::Colour glassEdge    { 0xff2c3338 };

    // --- ink ------------------------------------------------------------------
    const juce::Colour silk         { 0xff2a2724 };   // silkscreen on metal
    const juce::Colour silkDim      { 0xff5c574f };
    const juce::Colour silkFaint    { 0xff837d74 };
    const juce::Colour inkOnGlass   { 0xffd8d4cb };   // anything inside a window
    const juce::Colour inkOnGlassDim{ 0xff8c8981 };

    // --- the lit dial ---------------------------------------------------------
    const juce::Colour dial         { 0xff5fc8e6 };   // Marantz blue
    const juce::Colour dialDim      { 0xff1d4b5a };

    // --- warm accents, CR-78 preset buttons ----------------------------------
    const juce::Colour orange       { 0xffe2713a };
    const juce::Colour mustard      { 0xffd9a441 };
    const juce::Colour brick        { 0xffb0492a };
    const juce::Colour avocado      { 0xff8a9a3f };

    // --- kit hues, high pitch to low -----------------------------------------
    const juce::Colour kickRed      { 0xffb0492a };
    const juce::Colour vermilion    { 0xffcf5a32 };
    const juce::Colour tomLow       { 0xffd96c33 };
    const juce::Colour tomMidLow    { 0xffdf823a };
    const juce::Colour tomMidHigh   { 0xffd99a3e };
    const juce::Colour amber        { 0xffd9a441 };

    // --- aliases kept so the rest of the app reads naturally ------------------
    const juce::Colour background   = alu;
    const juce::Colour panel        = aluMid;
    const juce::Colour panelRaised  = alu;
    const juce::Colour bevel        = aluEdge;
    const juce::Colour sunken       = glass;
    const juce::Colour text         = silk;
    const juce::Colour textDim      = silkDim;
    const juce::Colour textFaint    = silkFaint;
    const juce::Colour vfd          = dial;
    const juce::Colour brass        = orange;
    const juce::Colour teal         = dial;
    const juce::Colour green        { 0xff2f7d55 };   // legible on metal
    const juce::Colour red          { 0xffa8342a };
    const juce::Colour warning      { 0xff8a5e10 };   // dark enough to read on silver

    // The same three states, lit, for anything behind glass.
    const juce::Colour glassOk      = dial;
    const juce::Colour glassWarn    = mustard;
    const juce::Colour glassError   { 0xffff7a5c };

    /** Brushed metal fill, with the grain running horizontally as it does on a
        real faceplate. */
    void drawBrushedMetal (juce::Graphics& g, juce::Rectangle<float> bounds, float corner = 0.0f);

    /** A window cut into the faceplate: dark glass with a shadowed inner edge. */
    void drawGlassWindow (juce::Graphics& g, juce::Rectangle<float> bounds, float corner = 3.0f);

    /** Panel with a lit top bevel. */
    void drawPanel (juce::Graphics& g, juce::Rectangle<float> bounds,
                    float corner = 4.0f, bool raised = true);

    /** The wordmark, silk-screened: one weight, one colour, wide tracking. */
    void drawWordmark (juce::Graphics& g, juce::Rectangle<int> bounds, float height);

    /** Backlit dial text. Only ever drawn inside a glass window. */
    void drawDialText (juce::Graphics& g, const juce::String& textToDraw,
                       juce::Rectangle<int> bounds, juce::Justification justification,
                       float fontSize, juce::Colour colour = dial);

    /** A walnut cabinet cheek. */
    void drawWalnutCheek (juce::Graphics& g, juce::Rectangle<float> bounds, bool lightFromLeft);

    /** A backlit analog VU meter face with needle and peak lamp.
        `needleVu` is in VU (-20..+3 on the scale, clamped when drawn). */
    void drawVuMeter (juce::Graphics& g, juce::Rectangle<float> bounds,
                      float needleVu, bool peakLit);

    /** Bar-graph meter: discrete segments behind glass. */
    void drawSegmentMeter (juce::Graphics& g, juce::Rectangle<float> bounds,
                           float level, float hold);

    juce::FontOptions legendFont (float height, bool bold = false);
    juce::FontOptions readoutFont (float height);

    //==============================================================================
    class LookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        LookAndFeel();

        void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                                   bool shouldDrawButtonAsHighlighted,
                                   bool shouldDrawButtonAsDown) override;

        void drawButtonText (juce::Graphics&, juce::TextButton&,
                             bool shouldDrawButtonAsHighlighted,
                             bool shouldDrawButtonAsDown) override;

        void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override;

        void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                               float sliderPos, float minSliderPos, float maxSliderPos,
                               juce::Slider::SliderStyle, juce::Slider&) override;

        void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                               float sliderPosProportional, float rotaryStartAngle,
                               float rotaryEndAngle, juce::Slider&) override;

        void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                           int buttonX, int buttonY, int buttonW, int buttonH,
                           juce::ComboBox&) override;

        void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
        juce::Font getComboBoxFont (juce::ComboBox&) override;

        void fillTextEditorBackground (juce::Graphics&, int width, int height,
                                       juce::TextEditor&) override;

        void drawTextEditorOutline (juce::Graphics&, int width, int height,
                                    juce::TextEditor&) override;

        void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int x, int y, int width, int height,
                            bool isScrollbarVertical, int thumbStartPosition, int thumbSize,
                            bool isMouseOver, bool isMouseDown) override;
    };
}
