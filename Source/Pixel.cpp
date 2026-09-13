#include "Pixel.h"
#include "PixelFontData.h"

#include <array>

#include <cmath>

namespace px
{

juce::Colour toColour (std::uint8_t ink)
{
    static const juce::uint32 palette[NumInks] =
    {
        0xff000000, 0xff1d2b53, 0xff7e2553, 0xff008751,
        0xffab5236, 0xff5f574f, 0xffc2c3c7, 0xfffff1e8,
        0xffff004d, 0xffffa300, 0xffffec27, 0xff00e436,
        0xff29adff, 0xff83769c, 0xffff77a8, 0xffffccaa
    };

    return juce::Colour (palette[ink % NumInks]);
}

Canvas::Canvas (int w, int h)
    : width (w), height (h), pixels ((size_t) (w * h), Black)
{
}

void Canvas::clear (std::uint8_t ink)
{
    std::fill (pixels.begin(), pixels.end(), ink);
}

void Canvas::set (int x, int y, std::uint8_t ink)
{
    if (x >= 0 && y >= 0 && x < width && y < height)
        pixels[(size_t) (y * width + x)] = ink;
}

void Canvas::fillRect (int x, int y, int w, int h, std::uint8_t ink)
{
    const int x0 = juce::jmax (0, x), y0 = juce::jmax (0, y);
    const int x1 = juce::jmin (width, x + w), y1 = juce::jmin (height, y + h);

    for (int yy = y0; yy < y1; ++yy)
        std::fill (pixels.begin() + yy * width + x0, pixels.begin() + yy * width + x1, ink);
}

void Canvas::rect (int x, int y, int w, int h, std::uint8_t ink)
{
    if (w <= 0 || h <= 0)
        return;

    hline (x, x + w - 1, y, ink);
    hline (x, x + w - 1, y + h - 1, ink);
    vline (x, y, y + h - 1, ink);
    vline (x + w - 1, y, y + h - 1, ink);
}

void Canvas::hline (int x0, int x1, int y, std::uint8_t ink)
{
    if (x1 < x0)
        std::swap (x0, x1);

    fillRect (x0, y, x1 - x0 + 1, 1, ink);
}

void Canvas::vline (int x, int y0, int y1, std::uint8_t ink)
{
    if (y1 < y0)
        std::swap (y0, y1);

    fillRect (x, y0, 1, y1 - y0 + 1, ink);
}

void Canvas::line (int x0, int y0, int x1, int y1, std::uint8_t ink)
{
    // Bresenham: whole pixels only, no anti-aliasing.
    const int dx = std::abs (x1 - x0), sx = x0 < x1 ? 1 : -1;
    const int dy = -std::abs (y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    for (;;)
    {
        set (x0, y0, ink);

        if (x0 == x1 && y0 == y1)
            break;

        const int e2 = 2 * err;

        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

namespace
{
    /** Half-width of an integer ellipse on row `dy`. */
    int spanAt (int rx, int ry, int dy)
    {
        if (ry <= 0)
            return rx;

        const double t = 1.0 - (double) (dy * dy) / ((double) ry * (double) ry + 0.5);
        return t <= 0.0 ? 0 : (int) std::floor ((double) rx * std::sqrt (t) + 0.5);
    }
}

void Canvas::fillEllipse (int cx, int cy, int rx, int ry, std::uint8_t ink)
{
    for (int dy = -ry; dy <= ry; ++dy)
    {
        const int span = spanAt (rx, ry, dy);
        hline (cx - span, cx + span, cy + dy, ink);
    }
}

void Canvas::ellipse (int cx, int cy, int rx, int ry, std::uint8_t ink)
{
    // Join each row to its neighbours so the outline never breaks into dots
    // where the curve is steep.
    for (int dy = -ry; dy <= ry; ++dy)
    {
        const int span = spanAt (rx, ry, dy);
        const int neighbour = juce::jmin (dy > -ry ? spanAt (rx, ry, dy - 1) : 0,
                                          dy <  ry ? spanAt (rx, ry, dy + 1) : 0);
        const int inner = juce::jmin (span, juce::jmax (0, neighbour + 1));

        hline (cx + inner, cx + span, cy + dy, ink);
        hline (cx - span, cx - inner, cy + dy, ink);
    }
}

void Canvas::ditherRect (int x, int y, int w, int h, std::uint8_t a, std::uint8_t b)
{
    for (int yy = y; yy < y + h; ++yy)
        for (int xx = x; xx < x + w; ++xx)
            set (xx, yy, ((xx + yy) & 1) ? a : b);
}

namespace
{
    struct GlyphMetrics { int left = 0; int advance = 8; };

    /** Per-glyph ink extents, measured once from the font data. The source font
        is monospaced, which at 8 px leaves narrow letters like l and i adrift
        in wide gaps; trimming each glyph to its ink with one pixel between
        reads far better. Digits are the exception - they share one width. */
    const std::array<GlyphMetrics, 95>& metrics()
    {
        static const auto table = []
        {
            std::array<GlyphMetrics, 95> t;
            int digitLeft = 8, digitRight = -1;

            const auto extents = [] (int index, int& left, int& right)
            {
                left = 8; right = -1;

                for (int row = 0; row < 8; ++row)
                    for (int col = 0; col < 8; ++col)
                        if (pixelfont::kGlyphs[index][row] & (0x80 >> col))
                        {
                            left = juce::jmin (left, col);
                            right = juce::jmax (right, col);
                        }
            };

            for (int d = '0'; d <= '9'; ++d)
            {
                int l, r;
                extents (d - 32, l, r);
                digitLeft = juce::jmin (digitLeft, l);
                digitRight = juce::jmax (digitRight, r);
            }

            for (int i = 0; i < 95; ++i)
            {
                const int code = i + 32;

                if (code == ' ')
                {
                    t[(size_t) i] = { 0, 4 };
                    continue;
                }

                int l, r;

                if (code >= '0' && code <= '9')
                {
                    l = digitLeft;
                    r = digitRight;
                }
                else
                {
                    extents (i, l, r);
                }

                t[(size_t) i] = r < 0 ? GlyphMetrics { 0, 4 } : GlyphMetrics { l, r - l + 2 };
            }

            return t;
        }();

        return table;
    }

    int indexFor (juce::juce_wchar c)
    {
        return (c < 32 || c > 126) ? '?' - 32 : (int) c - 32;
    }
}

int Canvas::textWidth (const juce::String& s, int scale)
{
    int width = 0;

    for (auto c : s)
        width += metrics()[(size_t) indexFor (c)].advance * scale;

    return width;
}

int Canvas::glyph (int x, int y, juce::juce_wchar c, std::uint8_t topInk, std::uint8_t bottomInk, int scale)
{
    const int index = indexFor (c);
    const auto& m = metrics()[(size_t) index];
    const auto& rows = pixelfont::kGlyphs[index];

    for (int row = 0; row < 8; ++row)
    {
        const auto ink = row < 4 ? topInk : bottomInk;

        for (int col = 0; col < 8; ++col)
            if (rows[row] & (0x80 >> col))
                fillRect (x + (col - m.left) * scale, y + row * scale, scale, scale, ink);
    }

    return m.advance * scale;
}

int Canvas::text (int x, int y, const juce::String& s, std::uint8_t ink, int scale)
{
    return textTwoTone (x, y, s, ink, ink, scale);
}

int Canvas::textTwoTone (int x, int y, const juce::String& s, std::uint8_t top, std::uint8_t bottom, int scale)
{
    int cursor = x;

    for (auto c : s)
        cursor += glyph (cursor, y, c, top, bottom, scale);

    return cursor - x;
}

void Canvas::panel (int x, int y, int w, int h, std::uint8_t face, std::uint8_t light, std::uint8_t shade)
{
    fillRect (x, y, w, h, face);
    rect (x, y, w, h, Black);
    hline (x + 1, x + w - 2, y + 1, light);
    vline (x + 1, y + 1, y + h - 2, light);
    hline (x + 1, x + w - 2, y + h - 2, shade);
    vline (x + w - 2, y + 1, y + h - 2, shade);
}

void Canvas::copyTo (juce::Image& image) const
{
    jassert (image.getWidth() == width && image.getHeight() == height);

    // Resolve the palette once rather than per pixel.
    juce::PixelARGB lut[NumInks];

    for (int i = 0; i < NumInks; ++i)
        lut[i] = toColour ((std::uint8_t) i).getPixelARGB();

    const juce::Image::BitmapData data (image, juce::Image::BitmapData::writeOnly);

    for (int y = 0; y < height; ++y)
    {
        const auto* src = pixels.data() + y * width;

        for (int x = 0; x < width; ++x)
            data.setPixelColour (x, y, juce::Colour (lut[src[x]]));
    }
}

} // namespace px
