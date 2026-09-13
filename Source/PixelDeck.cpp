#include "PixelDeck.h"

#include <juce_audio_basics/juce_audio_basics.h>   // Decibels

using midimap::Articulation;
using namespace px;

namespace
{
    constexpr int kFloorY = 190;   // top of the drum riser, where every foot stands
    constexpr int kFps = 30;

    /** Trims text to fit `maxWidth` pixels, marking the cut with a full stop. */
    juce::String fit (juce::String text, int maxWidth)
    {
        if (px::Canvas::textWidth (text) <= maxWidth)
            return text;

        while (text.isNotEmpty() && px::Canvas::textWidth (text + ".") > maxWidth)
            text = text.dropLastCharacters (1);

        return text.trimEnd() + ".";
    }

    /** Word-wraps to `maxWidth` pixels, keeping at most `maxLines` lines; the
        last kept line is trimmed if anything had to be dropped. */
    juce::StringArray wrap (const juce::String& text, int maxWidth, int maxLines)
    {
        juce::StringArray lines;
        juce::String current;

        for (const auto& word : juce::StringArray::fromTokens (text, " ", ""))
        {
            const auto candidate = current.isEmpty() ? word : current + " " + word;

            if (current.isEmpty() || px::Canvas::textWidth (candidate) <= maxWidth)
                current = candidate;
            else
            {
                lines.add (current);
                current = word;
            }
        }

        if (current.isNotEmpty())
            lines.add (current);

        for (auto& line : lines)
            line = fit (line, maxWidth);

        if (lines.size() > maxLines)
        {
            lines.removeRange (maxLines, lines.size() - maxLines);
            lines.set (maxLines - 1, fit (lines[maxLines - 1] + " ...", maxWidth));
        }

        return lines;
    }
}

//==============================================================================
PixelDeck::PixelDeck()
{
    glow.fill (0.0f);
    wobble.fill (0);
    setWantsKeyboardFocus (true);
    setOpaque (true);
    startTimerHz (kFps);
}

PixelDeck::Piece PixelDeck::pieceFor (Articulation a)
{
    switch (a)
    {
        case Articulation::Kick:            return Kick;
        case Articulation::SnareHead:
        case Articulation::SnareRimshot:
        case Articulation::SnareCrossStick: return Snare;
        case Articulation::Tom1:
        case Articulation::Tom1Rim:         return Tom1;
        case Articulation::Tom2:
        case Articulation::Tom2Rim:         return Tom2;
        case Articulation::Tom3:
        case Articulation::Tom3Rim:         return Tom3;
        case Articulation::Tom4:
        case Articulation::Tom4Rim:         return Tom4;
        case Articulation::HiHatClosedBow:
        case Articulation::HiHatClosedEdge:
        case Articulation::HiHatOpenBow:
        case Articulation::HiHatOpenEdge:
        case Articulation::HiHatPedal:      return HiHat;
        case Articulation::Crash1Bow:
        case Articulation::Crash1Edge:      return Crash1;
        case Articulation::Crash2Bow:
        case Articulation::Crash2Edge:      return Crash2;
        case Articulation::RideBow:
        case Articulation::RideEdge:
        case Articulation::RideBell:        return Ride;
        case Articulation::None:
        case Articulation::NumArticulations:
        default:                            return NumPieces;
    }
}

juce::Point<int> PixelDeck::hitPointFor (Piece piece) const
{
    switch (piece)
    {
        case Kick:   return { 240, 146 };
        case Snare:  return { 166, 142 };
        case HiHat:  return { 116, 116 };
        case Tom1:   return { 212, 106 };
        case Tom2:   return { 268, 106 };
        case Tom3:   return { 314, 140 };
        case Tom4:   return { 358, 146 };
        case Crash1: return { 176, 76 };
        case Crash2: return { 310, 72 };
        case Ride:   return { 410, 100 };
        case NumPieces:
        default:     return { 240, 120 };
    }
}

void PixelDeck::registerHit (Articulation articulation, float velocity)
{
    const auto piece = pieceFor (articulation);

    if (piece == NumPieces)
        return;

    const auto index = (size_t) piece;
    velocity = juce::jlimit (0.0f, 1.0f, velocity);

    glow[index] = juce::jmax (glow[index], 0.4f + 0.6f * velocity);
    wobble[index] = 1 + juce::roundToInt (velocity * 2.0f);

    const auto at = hitPointFor (piece);

    // Sparks, more of them the harder the hit.
    int toSpawn = 4 + juce::roundToInt (velocity * 8.0f);

    for (auto& spark : sparks)
    {
        if (toSpawn == 0)
            break;

        if (spark.life > 0)
            continue;

        const float angle = juce::MathConstants<float>::pi * (1.05f + 0.9f * random.nextFloat());
        const float speed = (0.8f + 1.4f * random.nextFloat()) * (0.6f + velocity);

        spark.x = (float) at.x;
        spark.y = (float) at.y;
        spark.vx = std::cos (angle) * speed;
        spark.vy = std::sin (angle) * speed;
        spark.life = 10 + random.nextInt (8);
        --toSpawn;
    }

    // The velocity floats up off the drum, like a damage number - except it
    // tells a drummer something useful: how hard that hit actually landed.
    auto& floater = floaters[index];
    floater.value = juce::roundToInt (velocity * 127.0f);
    floater.x = at.x;
    floater.y = at.y - 10;
    floater.life = 24;
}

//==============================================================================
void PixelDeck::timerCallback()
{
    ++frameCount;

    for (auto& g : glow)
        g = juce::jmax (0.0f, g - 0.08f);

    if ((frameCount & 1) == 0)
        for (auto& w : wobble)
            w = juce::jmax (0, w - 1);

    for (auto& spark : sparks)
    {
        if (spark.life <= 0)
            continue;

        spark.x += spark.vx;
        spark.y += spark.vy;
        spark.vy += 0.14f;      // gravity
        --spark.life;
    }

    for (auto& floater : floaters)
    {
        if (floater.life <= 0)
            continue;

        if ((floater.life & 1) == 0)
            --floater.y;

        --floater.life;
    }

    // Game-style level bar: instant attack, steady fall, a hold marker.
    const float peakDb = juce::Decibels::gainToDecibels (snapshot.peak, -60.0f);
    const float target = juce::jlimit (0.0f, 1.0f, (peakDb + 48.0f) / 48.0f);

    meterLevel = juce::jmax (target, meterLevel - 0.04f);
    meterHold = juce::jmax (meterLevel, meterHold - 0.01f);

    if (snapshot.peak > 0.89f)
        clipFrames = kFps;
    else if (clipFrames > 0)
        --clipFrames;

    render();
    repaint();
}

void PixelDeck::render()
{
    canvas.clear (Black);

    drawHeader();
    drawStage();
    drawGrooves();
    drawFooter();

    canvas.copyTo (frame);
}

//==============================================================================
void PixelDeck::drawButton (juce::Rectangle<int> r, const juce::String& label, Target target,
                            bool active, std::uint8_t activeFace)
{
    const bool isHover = hovered == target;
    const bool isDown = pressed == target && isHover;

    std::uint8_t face = active ? activeFace : (isHover ? Peach : Silver);

    if (isDown)
        face = Slate;

    // Pressed keys drop by a pixel and lose their shadow - the whole trick of
    // a satisfying 8-bit button.
    const int drop = isDown ? 1 : 0;

    if (! isDown)
        canvas.fillRect (r.getX() + 1, r.getBottom(), r.getWidth(), 1, Black);

    canvas.panel (r.getX(), r.getY() + drop, r.getWidth(), r.getHeight(), face, White, isDown ? Black : Slate);

    const int tx = r.getCentreX() - Canvas::textWidth (label) / 2;
    const int ty = r.getY() + (r.getHeight() - 8) / 2 + drop;
    canvas.text (tx, ty, label, isDown ? White : Black);
}

//==============================================================================
void PixelDeck::drawHeader()
{
    canvas.panel (0, 0, kWidth, 40, Navy, Lavender, Black);

    // Title: two-tone arcade logo with a hard drop shadow.
    canvas.text (10, 8, "OPENDRUMMER", Plum, 2);
    canvas.textTwoTone (8, 6, "OPENDRUMMER", Yellow, Orange, 2);

    canvas.text (8, 27, fit (snapshot.statusLine, 272), Lavender);

    // Kit selector, cycled like a character select.
    canvas.panel (288, 4, 188, 32, Black, Slate, Navy);
    canvas.text (296, 7, "KIT", Lavender);

    drawButton (kitPrev, "<", Target::KitPrev);
    drawButton (kitNext, ">", Target::KitNext);

    juce::String name;

    if (snapshot.kitLoading)
        name = "LOADING" + juce::String::repeatedString (".", (frameCount / 8) % 4);
    else if (juce::isPositiveAndBelow (snapshot.kitIndex, snapshot.kitNames.size()))
        name = snapshot.kitNames[snapshot.kitIndex];

    name = fit (name.toUpperCase(), 140);
    canvas.text (385 - Canvas::textWidth (name) / 2, 21, name, snapshot.kitLoading ? Orange : White);
}

//==============================================================================
void PixelDeck::drawStand (int x, int topY, int floorY, bool tripod)
{
    canvas.vline (x, topY, floorY - 1, Slate);

    // Height clamp, halfway up.
    const int clampY = (topY + floorY) / 2;
    canvas.fillRect (x - 1, clampY, 3, 2, Silver);

    if (tripod)
    {
        canvas.line (x, floorY - 7, x - 8, floorY - 1, Slate);
        canvas.line (x, floorY - 7, x + 8, floorY - 1, Slate);
    }
}

void PixelDeck::drawCymbal (int cx, int cy, int rx, float g, int shake)
{
    // A struck cymbal flexes: its edge swings up and down for a few frames.
    const int flex = shake == 0 ? 0 : ((frameCount & 1) ? shake : -shake);
    const int ry = juce::jmax (1, 3 + flex);

    const std::uint8_t top = g > 0.6f ? White : (g > 0.25f ? Peach : Yellow);

    canvas.fillEllipse (cx, cy + 1, rx, ry, Orange);
    canvas.fillEllipse (cx, cy, rx, juce::jmax (1, ry - 1), top);

    // Bell.
    canvas.fillEllipse (cx, cy - ry, juce::jmax (2, rx / 6), 2, top);
    canvas.set (cx - rx, cy + 1, Brown);
    canvas.set (cx + rx, cy + 1, Brown);
}

void PixelDeck::drawDrum (int cx, int topY, int rx, int ry, int bodyHeight, float g)
{
    // A struck head gives a pixel under the stick.
    const int push = g > 0.7f ? 1 : 0;
    const std::uint8_t head = g > 0.6f ? White : (g > 0.25f ? Yellow : Peach);

    const int bottomY = topY + bodyHeight;

    canvas.fillEllipse (cx, bottomY, rx, ry, Plum);
    canvas.fillRect (cx - rx, topY, rx * 2 + 1, bodyHeight, Red);

    // Shell lighting: a lit stripe on the left, shade on the right.
    canvas.fillRect (cx - rx + rx / 3, topY + 1, 2, bodyHeight - 1, Pink);
    canvas.fillRect (cx + rx - 2, topY + 1, 2, bodyHeight - 1, Plum);

    // Hoops and tension lugs.
    canvas.hline (cx - rx, cx + rx, bottomY - 1, Silver);

    for (int x = cx - rx + 4; x <= cx + rx - 4; x += 7)
        canvas.fillRect (x, topY + bodyHeight / 2 - 1, 1, 3, Silver);

    canvas.fillEllipse (cx, topY + push, rx, ry, head);
    canvas.ellipse (cx, topY + push, rx, ry, Silver);
}

void PixelDeck::drawKick (int cx, int cy, int r, float g)
{
    const std::uint8_t head = g > 0.6f ? White : (g > 0.25f ? Yellow : Peach);
    const int bump = g > 0.7f ? 1 : 0;

    // Spurs to the riser.
    canvas.line (cx - r + 8, cy + r - 8, cx - r - 4, kFloorY - 1, Silver);
    canvas.line (cx + r - 8, cy + r - 8, cx + r + 4, kFloorY - 1, Silver);

    canvas.fillEllipse (cx, cy, r + bump, r + bump, Red);
    canvas.ellipse (cx, cy, r + bump, r + bump, Black);
    canvas.ellipse (cx, cy, r - 2, r - 2, Silver);
    canvas.fillEllipse (cx, cy, r - 4, r - 4, head);

    // Logo on the reso head, and the port hole.
    canvas.panel (cx - 17, cy - 12, 34, 16, Black, Red, Plum);
    canvas.textTwoTone (cx - 8, cy - 8, "OD", Yellow, Orange);
    canvas.fillEllipse (cx + 12, cy + 14, 3, 3, Black);
}

void PixelDeck::drawStage()
{
    canvas.panel (4, 40, kWidth - 8, 198, Navy, Slate, Black);

    // Backdrop: fades into darkness at the top.
    canvas.ditherRect (6, 42, kWidth - 12, 6, Black, Navy);

    // Drum riser and the stage floor.
    canvas.fillRect (6, 200, kWidth - 12, 36, Plum);
    canvas.ditherRect (6, 200, kWidth - 12, 3, Plum, Black);

    for (int y = 210; y < 236; y += 10)
        canvas.hline (6, kWidth - 7, y, Black);

    canvas.fillRect (86, kFloorY, 308, 10, Brown);
    canvas.hline (86, 393, kFloorY, Peach);
    canvas.hline (86, 393, kFloorY + 9, Black);

    const float hat = snapshot.hiHatOpenness;

    // Back to front, as the kit would occlude itself.
    drawStand (186, 82, kFloorY, true);
    drawStand (300, 78, kFloorY, true);
    drawStand (404, 106, kFloorY, true);

    drawCymbal (176, 78, 26, glow[Crash1], wobble[Crash1]);
    drawCymbal (310, 74, 26, glow[Crash2], wobble[Crash2]);
    drawCymbal (410, 102, 30, glow[Ride], wobble[Ride]);

    // Rack tom mounts, then the toms.
    canvas.line (212, 124, 232, 132, Slate);
    canvas.line (268, 124, 248, 132, Slate);
    drawDrum (212, 108, 14, 3, 12, glow[Tom1]);
    drawDrum (268, 108, 14, 3, 12, glow[Tom2]);

    // Floor toms on legs.
    for (const int x : { 298, 330 })   canvas.vline (x, 168, kFloorY - 1, Slate);
    for (const int x : { 340, 376 })   canvas.vline (x, 176, kFloorY - 1, Slate);
    drawDrum (314, 142, 19, 4, 22, glow[Tom3]);
    drawDrum (358, 148, 21, 4, 24, glow[Tom4]);

    drawKick (240, 156, 30, glow[Kick]);

    drawStand (166, 162, kFloorY, true);
    drawDrum (166, 144, 18, 4, 12, glow[Snare]);

    // Hi-hat: the gap between the cymbals follows the pedal.
    drawStand (116, 112, kFloorY, true);
    canvas.fillRect (116, kFloorY - 3, 9, 3, Black);          // pedal
    const int gap = 2 + juce::roundToInt (hat * 8.0f);
    drawCymbal (116, 126, 18, glow[HiHat] * 0.6f, 0);
    drawCymbal (116, 126 - gap, 18, glow[HiHat], wobble[HiHat]);

    // Sparks.
    for (const auto& spark : sparks)
    {
        if (spark.life <= 0)
            continue;

        const std::uint8_t ink = spark.life > 12 ? White : spark.life > 7 ? Yellow : spark.life > 3 ? Orange : Red;
        const int size = spark.life > 12 ? 2 : 1;
        canvas.fillRect ((int) spark.x, (int) spark.y, size, size, ink);
    }

    // Velocity numbers, outlined so they read over anything.
    for (const auto& floater : floaters)
    {
        if (floater.life <= 0)
            continue;

        const auto label = juce::String (floater.value);
        const int x = floater.x - Canvas::textWidth (label) / 2;

        static constexpr int outline[4][2] = { { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 } };

        for (const auto& o : outline)
            canvas.text (x + o[0], floater.y + o[1], label, Black);

        canvas.text (x, floater.y, label, floater.life > 8 ? White : Lavender);
    }
}

//==============================================================================
void PixelDeck::drawGrooves()
{
    canvas.panel (4, 240, kWidth - 8, 100, Navy, Slate, Black);
    canvas.text (12, 244, "GROOVES", Yellow);

    const int count = snapshot.grooveNames.size();
    const bool scrollable = count > kVisibleRows;

    // Keep the selection on screen.
    if (snapshot.grooveSelected >= 0)
    {
        if (snapshot.grooveSelected < grooveScroll)
            grooveScroll = snapshot.grooveSelected;
        else if (snapshot.grooveSelected >= grooveScroll + kVisibleRows)
            grooveScroll = snapshot.grooveSelected - kVisibleRows + 1;
    }

    grooveScroll = juce::jlimit (0, juce::jmax (0, count - kVisibleRows), grooveScroll);

    canvas.fillRect (grooveList.getX(), grooveList.getY() - 1, grooveList.getWidth(), grooveList.getHeight() + 2, Black);

    const int rowWidth = grooveList.getWidth() - (scrollable ? 16 : 0);

    if (count == 0)
        canvas.text (grooveList.getX() + 12, grooveList.getY() + 30, "NO GROOVES IN FOLDER", Slate);

    for (int i = 0; i < kVisibleRows; ++i)
    {
        const int index = grooveScroll + i;

        if (index >= count)
            break;

        const int y = grooveList.getY() + i * kRowHeight;
        const bool selected = index == snapshot.grooveSelected;
        const bool hot = hovered == Target::GrooveRow && hoveredRow == index;

        if (selected)
            canvas.fillRect (grooveList.getX(), y, rowWidth, kRowHeight, Plum);
        else if (hot)
            canvas.fillRect (grooveList.getX(), y, rowWidth, kRowHeight, Navy);

        // The menu cursor - a shape, not just a colour, marks the selection.
        if (selected)
        {
            for (int k = 0; k < 4; ++k)
                canvas.vline (grooveList.getX() + 3 + k, y + 1 + k, y + 7 - k, Yellow);
        }

        // Equaliser bars bouncing beside the groove that is playing.
        if (selected && snapshot.playing)
        {
            for (int b = 0; b < 3; ++b)
            {
                const int h = 2 + (int) (2.5f + 2.5f * std::sin ((float) frameCount * 0.6f + (float) b * 1.9f));
                canvas.fillRect (grooveList.getX() + rowWidth - 34 + b * 3, y + 8 - h, 2, h, Green);
            }
        }

        const auto name = fit (snapshot.grooveNames[index], rowWidth - 60);
        canvas.text (grooveList.getX() + 12, y + 1, name, selected ? White : Silver);

        if (index < snapshot.grooveBpm.size())
        {
            const auto bpm = juce::String (snapshot.grooveBpm[index]);
            canvas.text (grooveList.getX() + rowWidth - 4 - Canvas::textWidth (bpm), y + 1, bpm,
                         selected ? Peach : Lavender);
        }
    }

    if (scrollable)
    {
        drawButton ({ 294, 256, 14, 12 }, "^", Target::GrooveUp);
        drawButton ({ 294, 324, 14, 12 }, "v", Target::GrooveDown);
    }

    // Transport.
    drawButton (playBtn, snapshot.playing ? "STOP" : "PLAY", Target::Play, snapshot.playing, Orange);
    drawButton (loopBtn, "LOOP", Target::Loop, snapshot.looping, Green);

    canvas.text (318, 272, "TEMPO", Lavender);
    drawButton (tempoDown, "-", Target::TempoDown);
    drawButton (tempoUp, "+", Target::TempoUp);
    canvas.panel (tempoValue.getX(), tempoValue.getY(), tempoValue.getWidth(), tempoValue.getHeight(),
                  hovered == Target::TempoValue ? Navy : Black, Slate, Navy);
    const auto tempoText = juce::String (juce::roundToInt (snapshot.tempo)) + " BPM";
    canvas.text (tempoValue.getCentreX() - Canvas::textWidth (tempoText) / 2, tempoValue.getY() + 4, tempoText, White);

    // Progress through the loop, with a mark at each bar line.
    const juce::Rectangle<int> track { 318, 302, 154, 5 };
    canvas.fillRect (track, Black);

    if (snapshot.playing)
        canvas.fillRect (track.getX(), track.getY(), juce::roundToInt ((float) track.getWidth() * snapshot.progress), track.getHeight(), Green);

    for (int b = 1; b < snapshot.bars; ++b)
        canvas.vline (track.getX() + track.getWidth() * b / snapshot.bars, track.getY(), track.getBottom() - 1, Slate);

    if (snapshot.bars > 0)
        canvas.text (318, 309, "BAR " + juce::String (juce::jmax (1, snapshot.bar)) + "/" + juce::String (snapshot.bars), Lavender);

    drawButton (folderBtn, "FOLDER", Target::Folder);

    const bool canDrag = snapshot.grooveSelected >= 0;
    canvas.panel (dragBtn.getX(), dragBtn.getY(), dragBtn.getWidth(), dragBtn.getHeight(),
                  hovered == Target::DragMidi && canDrag ? Navy : Black, canDrag ? Orange : Slate, Navy);
    canvas.text (dragBtn.getCentreX() - Canvas::textWidth ("DRAG") / 2, dragBtn.getY() + 5, "DRAG", canDrag ? Orange : Slate);
}

//==============================================================================
void PixelDeck::drawFooter()
{
    canvas.panel (4, 342, kWidth - 8, 74, Navy, Slate, Black);

    // Volume, as a segmented bar.
    canvas.text (10, 350, "VOL", Yellow);
    drawButton (volDown, "-", Target::VolDown);
    drawButton (volUp, "+", Target::VolUp);

    canvas.fillRect (volBar, Black);

    constexpr int kSegments = 14;
    const float t = (float) ((snapshot.gainDb + 40.0) / 52.0);

    for (int k = 0; k < kSegments; ++k)
    {
        const bool lit = t >= ((float) k + 0.5f) / (float) kSegments;
        const std::uint8_t on = k >= 12 ? Red : k >= 10 ? Yellow : Green;
        canvas.fillRect (volBar.getX() + 1 + k * 7, volBar.getY() + 2, 5, volBar.getHeight() - 4, lit ? on : Slate);
    }

    const auto gainText = (snapshot.gainDb > 0.05 ? "+" : "") + juce::String (snapshot.gainDb, 1) + "DB";
    canvas.text (10, 386, gainText, White);

    // Dialog box, as in every RPG.
    const juce::Rectangle<int> box { 144, 346, 234, 66 };
    canvas.fillRect (box, White);
    canvas.fillRect (box.reduced (1), Black);
    canvas.rect (box.getX() + 3, box.getY() + 3, box.getWidth() - 6, box.getHeight() - 6, Lavender);

    // Five fixed rows: the hit, two for MIDI, two for the kit credit. Every
    // message is wrapped and capped to its rows, so nothing can overprint.
    const int textWidth = box.getWidth() - 16;
    const int left = box.getX() + 8;
    const int top = box.getY() + 6;

    const std::uint8_t hitInk = snapshot.hitState == 1 ? White : snapshot.hitState == 2 ? Orange : Silver;
    canvas.text (left, top, fit (snapshot.hitText, textWidth), hitInk);

    const std::uint8_t midiInk = snapshot.midiState == 0 ? Green : snapshot.midiState == 1 ? Yellow : Red;
    const auto midiLines = wrap (snapshot.midiText, textWidth, 2);

    for (int i = 0; i < midiLines.size(); ++i)
        canvas.text (left, top + 11 + i * 10, midiLines[i], midiInk);

    // Kit credit - the CC BY kits require their authors to travel with them.
    const auto creditLines = wrap (snapshot.kitCredit, textWidth, 2);

    for (int i = 0; i < creditLines.size(); ++i)
        canvas.text (left, top + 34 + i * 10, creditLines[i], Lavender);

    // Level bar with a clip warning.
    canvas.text (384, 350, "LEVEL", Yellow);

    if (clipFrames > 0 && ((frameCount / 4) & 1) == 0)
        canvas.text (440, 350, "CLIP", Red);

    const juce::Rectangle<int> meter { 384, 362, 88, 10 };
    canvas.fillRect (meter, Black);

    constexpr int kMeterSegments = 11;

    for (int k = 0; k < kMeterSegments; ++k)
    {
        const float threshold = ((float) k + 0.5f) / (float) kMeterSegments;
        const std::uint8_t on = k >= 9 ? Red : k >= 7 ? Yellow : Green;
        const bool lit = meterLevel >= threshold;
        const bool held = ! lit && juce::jlimit (0, kMeterSegments - 1, (int) (meterHold * kMeterSegments)) == k && meterHold > 0.05f;

        canvas.fillRect (meter.getX() + 1 + k * 8, meter.getY() + 2, 6, meter.getHeight() - 4,
                         lit ? on : held ? Lavender : Slate);
    }

    drawButton (settingsBtn, "SETUP", Target::Settings);
}

//==============================================================================
void PixelDeck::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);

    // Whole-number scaling with nearest-neighbour sampling is the entire trick:
    // any other scale or filter smears the pixels and the illusion is gone.
    g.setImageResamplingQuality (juce::Graphics::lowResamplingQuality);
    g.drawImageTransformed (frame, juce::AffineTransform::scale ((float) scale)
                                     .translated ((float) offset.x, (float) offset.y));
}

void PixelDeck::resized()
{
    scale = juce::jmax (1, juce::jmin (getWidth() / kWidth, getHeight() / kHeight));
    offset = { (getWidth() - kWidth * scale) / 2, (getHeight() - kHeight * scale) / 2 };
}

juce::Point<int> PixelDeck::toLogical (juce::Point<float> screen) const
{
    return { (int) std::floor ((screen.x - (float) offset.x) / (float) scale),
             (int) std::floor ((screen.y - (float) offset.y) / (float) scale) };
}

PixelDeck::Target PixelDeck::targetAt (juce::Point<int> p, int* rowOut) const
{
    const bool scrollable = snapshot.grooveNames.size() > kVisibleRows;

    if (kitPrev.contains (p))     return Target::KitPrev;
    if (kitNext.contains (p))     return Target::KitNext;
    if (playBtn.contains (p))     return Target::Play;
    if (loopBtn.contains (p))     return Target::Loop;
    if (tempoDown.contains (p))   return Target::TempoDown;
    if (tempoUp.contains (p))     return Target::TempoUp;
    if (tempoValue.contains (p))  return Target::TempoValue;
    if (folderBtn.contains (p))   return Target::Folder;
    if (dragBtn.contains (p))     return Target::DragMidi;
    if (volDown.contains (p))     return Target::VolDown;
    if (volUp.contains (p))       return Target::VolUp;
    if (volBar.contains (p))      return Target::VolBar;
    if (settingsBtn.contains (p)) return Target::Settings;

    if (scrollable && juce::Rectangle<int> (294, 256, 14, 12).contains (p)) return Target::GrooveUp;
    if (scrollable && juce::Rectangle<int> (294, 324, 14, 12).contains (p)) return Target::GrooveDown;

    const int rowWidth = grooveList.getWidth() - (scrollable ? 16 : 0);

    if (grooveList.withWidth (rowWidth).contains (p))
    {
        const int row = grooveScroll + (p.y - grooveList.getY()) / kRowHeight;

        if (row < snapshot.grooveNames.size())
        {
            if (rowOut != nullptr)
                *rowOut = row;

            return Target::GrooveRow;
        }
    }

    return Target::None;
}

double PixelDeck::gainFromBarX (int x) const
{
    const double t = juce::jlimit (0.0, 1.0, (double) (x - volBar.getX()) / (double) volBar.getWidth());
    return std::round ((-40.0 + t * 52.0) * 2.0) / 2.0;
}

//==============================================================================
void PixelDeck::mouseMove (const juce::MouseEvent& e)
{
    hoveredRow = -1;
    hovered = targetAt (toLogical (e.position), &hoveredRow);

    const bool clickable = hovered != Target::None;
    setMouseCursor (hovered == Target::DragMidi ? juce::MouseCursor::DraggingHandCursor
                  : hovered == Target::TempoValue ? juce::MouseCursor::UpDownResizeCursor
                  : clickable ? juce::MouseCursor::PointingHandCursor
                              : juce::MouseCursor::NormalCursor);
}

void PixelDeck::mouseExit (const juce::MouseEvent&)
{
    hovered = Target::None;
    hoveredRow = -1;
}

void PixelDeck::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();

    int row = -1;
    const auto p = toLogical (e.position);
    pressed = targetAt (p, &row);
    hovered = pressed;
    draggingOut = false;

    switch (pressed)
    {
        case Target::GrooveRow:  if (onGrooveSelect) onGrooveSelect (row); break;
        case Target::VolBar:     if (onGainChange) onGainChange (gainFromBarX (p.x)); break;
        case Target::TempoValue: tempoDragStart = snapshot.tempo; break;
        default: break;
    }
}

void PixelDeck::mouseDrag (const juce::MouseEvent& e)
{
    const auto p = toLogical (e.position);
    hovered = targetAt (p);

    switch (pressed)
    {
        case Target::VolBar:
            if (onGainChange) onGainChange (gainFromBarX (p.x));
            break;

        case Target::TempoValue:
        {
            // Drag up to speed up: two screen pixels per BPM.
            const double bpm = juce::jlimit (40.0, 240.0, std::round (tempoDragStart - e.getDistanceFromDragStartY() / 2.0));
            if (onTempoChange) onTempoChange (bpm);
            break;
        }

        case Target::DragMidi:
            if (! draggingOut && e.getDistanceFromDragStart() > 6 && getGrooveFileForDrag)
            {
                const auto file = getGrooveFileForDrag();

                if (file.existsAsFile())
                {
                    draggingOut = true;

                    // A copy, never a move: a drag must not empty the library.
                    juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this);
                }
            }
            break;

        default:
            break;
    }
}

void PixelDeck::mouseUp (const juce::MouseEvent& e)
{
    const auto releasedOver = targetAt (toLogical (e.position));
    const auto target = pressed;
    pressed = Target::None;

    // A button fires only if the pointer is still on it when released.
    if (releasedOver != target)
        return;

    switch (target)
    {
        case Target::KitPrev:    if (onKitStep) onKitStep (-1); break;
        case Target::KitNext:    if (onKitStep) onKitStep (1); break;
        case Target::Play:       if (onPlayToggle) onPlayToggle(); break;
        case Target::Loop:       if (onLoopToggle) onLoopToggle(); break;
        case Target::TempoDown:  if (onTempoChange) onTempoChange (juce::jmax (40.0, std::round (snapshot.tempo) - 1.0)); break;
        case Target::TempoUp:    if (onTempoChange) onTempoChange (juce::jmin (240.0, std::round (snapshot.tempo) + 1.0)); break;
        case Target::Folder:     if (onFolder) onFolder(); break;
        case Target::VolDown:    if (onGainChange) onGainChange (juce::jmax (-40.0, snapshot.gainDb - 1.0)); break;
        case Target::VolUp:      if (onGainChange) onGainChange (juce::jmin (12.0, snapshot.gainDb + 1.0)); break;
        case Target::Settings:   if (onSettings) onSettings(); break;
        case Target::GrooveUp:   grooveScroll = juce::jmax (0, grooveScroll - 1); break;
        case Target::GrooveDown: ++grooveScroll; break;
        default: break;
    }
}

void PixelDeck::mouseDoubleClick (const juce::MouseEvent& e)
{
    switch (targetAt (toLogical (e.position)))
    {
        case Target::GrooveRow: if (onPlayToggle) onPlayToggle(); break;
        case Target::VolBar:    if (onGainChange) onGainChange (0.0); break;
        default: break;
    }
}

void PixelDeck::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const auto p = toLogical (e.position);
    const int direction = wheel.deltaY > 0 ? 1 : (wheel.deltaY < 0 ? -1 : 0);

    if (direction == 0)
        return;

    if (grooveList.contains (p))
        grooveScroll = juce::jmax (0, grooveScroll - direction);
    else if (volBar.expanded (16, 6).contains (p))
    {
        if (onGainChange) onGainChange (juce::jlimit (-40.0, 12.0, snapshot.gainDb + direction));
    }
    else if (tempoValue.contains (p) || tempoDown.contains (p) || tempoUp.contains (p))
    {
        if (onTempoChange) onTempoChange (juce::jlimit (40.0, 240.0, std::round (snapshot.tempo) + direction));
    }
}

bool PixelDeck::keyPressed (const juce::KeyPress& key)
{
    const int count = snapshot.grooveNames.size();

    if (key == juce::KeyPress::upKey && count > 0)
    {
        if (onGrooveSelect) onGrooveSelect (juce::jmax (0, snapshot.grooveSelected - 1));
        return true;
    }

    if (key == juce::KeyPress::downKey && count > 0)
    {
        if (onGrooveSelect) onGrooveSelect (juce::jmin (count - 1, snapshot.grooveSelected + 1));
        return true;
    }

    if (key == juce::KeyPress::spaceKey || key == juce::KeyPress::returnKey)
    {
        if (onPlayToggle) onPlayToggle();
        return true;
    }

    return false;
}
