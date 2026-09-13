#pragma once

#include <juce_graphics/juce_graphics.h>
#include <cstdint>
#include <vector>

/**
    A true low-resolution, indexed-colour framebuffer.

    Pixel art only looks right when every element shares one pixel grid, so
    the main screen is drawn here at 480x420 and scaled up by a whole number
    with nearest-neighbour sampling. Nothing can land between pixels, and
    because the buffer stores palette indices rather than colours, nothing can
    step outside the palette either - the same constraint the hardware had.
*/
namespace px
{

/** The PICO-8 palette: sixteen colours chosen to work together at low
    resolution, and immediately legible as a retro game. */
enum Ink : std::uint8_t
{
    Black = 0, Navy, Plum, Forest, Brown, Slate, Silver, White,
    Red, Orange, Yellow, Green, Blue, Lavender, Pink, Peach,
    NumInks
};

juce::Colour toColour (std::uint8_t ink);

class Canvas
{
public:
    Canvas (int width, int height);

    int getWidth() const noexcept  { return width; }
    int getHeight() const noexcept { return height; }

    void clear (std::uint8_t ink);
    void set (int x, int y, std::uint8_t ink);

    void fillRect (int x, int y, int w, int h, std::uint8_t ink);
    void fillRect (juce::Rectangle<int> r, std::uint8_t ink) { fillRect (r.getX(), r.getY(), r.getWidth(), r.getHeight(), ink); }
    void rect (int x, int y, int w, int h, std::uint8_t ink);
    void hline (int x0, int x1, int y, std::uint8_t ink);
    void vline (int x, int y0, int y1, std::uint8_t ink);
    void line (int x0, int y0, int x1, int y1, std::uint8_t ink);

    /** Integer ellipses, symmetric about their centre. */
    void fillEllipse (int cx, int cy, int rx, int ry, std::uint8_t ink);
    void ellipse (int cx, int cy, int rx, int ry, std::uint8_t ink);

    /** Checkerboard of two inks - the classic way to fake a third shade. */
    void ditherRect (int x, int y, int w, int h, std::uint8_t a, std::uint8_t b);

    /** 8x8 bitmap text. Returns the width drawn, in pixels. */
    int text (int x, int y, const juce::String& s, std::uint8_t ink, int scale = 1);

    /** Text whose top half and bottom half use different inks, as on arcade
        title screens. */
    int textTwoTone (int x, int y, const juce::String& s, std::uint8_t top, std::uint8_t bottom, int scale = 1);

    /** Width of a string as drawn. Letters are proportional, digits share one
        width so that changing numbers do not shuffle the text around them. */
    static int textWidth (const juce::String& s, int scale = 1);

    /** A raised window frame: outline, lit top-left, shaded bottom-right. */
    void panel (int x, int y, int w, int h, std::uint8_t face, std::uint8_t light, std::uint8_t shade);

    /** Writes the frame into an ARGB image of the same size. */
    void copyTo (juce::Image& image) const;

private:
    int glyph (int x, int y, juce::juce_wchar c, std::uint8_t topInk, std::uint8_t bottomInk, int scale);

    int width, height;
    std::vector<std::uint8_t> pixels;
};

} // namespace px
