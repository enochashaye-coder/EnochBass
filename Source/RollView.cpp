#include "PluginEditor.h"

using namespace enochsolo;
using LF = EnochLookAndFeel;

namespace
{
constexpr float kGutter = 40.f, kChordH = 28.f, kRulerH = 15.f, kPhraseH = 14.f, kLaneH = 96.f, kKsH = 14.f, kBarThk = 14.f;

// little tool cursors drawn in code: pen, eraser, razor blade, muted speaker
juce::MouseCursor makeToolCursor (RollView::Tool t)
{
    using juce::Colour; using juce::Path; using juce::Point;
    juce::Image img (juce::Image::ARGB, 32, 32, true);
    int hx = 16, hy = 16;
    {
        juce::Graphics g (img);
        const auto ink = juce::Colours::black.withAlpha (0.92f);
        if (t == RollView::Tool::Draw)                                   // pencil, tip at the lower left
        {
            const Point<float> T (3.f, 29.f), u (0.7071f, -0.7071f), n (0.7071f, 0.7071f);
            const auto S = T + u * 7.f, E = T + u * 27.f; const float w = 3.7f;
            Path body; body.startNewSubPath (T); body.lineTo (S + n * w); body.lineTo (E + n * w); body.lineTo (E - n * w); body.lineTo (S - n * w); body.closeSubPath();
            g.setColour (Colour (0xfff2c14e)); g.fillPath (body);
            Path tip; tip.addTriangle (T.x, T.y, (S + n * w).x, (S + n * w).y, (S - n * w).x, (S - n * w).y);
            g.setColour (Colour (0xffe9d2a6)); g.fillPath (tip);
            Path lead; lead.addTriangle (T.x, T.y, (T + u * 2.8f + n * 1.1f).x, (T + u * 2.8f + n * 1.1f).y, (T + u * 2.8f - n * 1.1f).x, (T + u * 2.8f - n * 1.1f).y);
            g.setColour (ink); g.fillPath (lead);
            Path band; band.startNewSubPath (E - u * 5.f + n * w); band.lineTo (E + n * w); band.lineTo (E - n * w); band.lineTo (E - u * 5.f - n * w); band.closeSubPath();
            g.setColour (Colour (0xffe56fb0)); g.fillPath (band);
            g.setColour (ink); g.strokePath (body, juce::PathStrokeType (1.5f));
            hx = 3; hy = 29;
        }
        else if (t == RollView::Tool::Erase)                             // rubber eraser
        {
            const auto tf = juce::AffineTransform::rotation (-0.62f, 16.f, 18.f);
            Path e; e.addRoundedRectangle (6.f, 12.f, 21.f, 11.f, 2.5f); e.applyTransform (tf);
            Path sleeve; sleeve.addRoundedRectangle (6.f, 12.f, 9.f, 11.f, 2.5f); sleeve.applyTransform (tf);
            g.setColour (Colour (0xffff8fa8)); g.fillPath (e);
            g.setColour (Colour (0xfff3f3f3)); g.fillPath (sleeve);
            g.setColour (ink); g.strokePath (e, juce::PathStrokeType (1.5f));
            float px = 16.5f, py = 17.5f; tf.transformPoint (px, py); hx = (int) px; hy = (int) py;
            hx = juce::jlimit (0, 31, hx); hy = juce::jlimit (0, 31, hy);
        }
        else if (t == RollView::Tool::Split)                             // razor blade with a handle
        {
            Path handle; handle.addRoundedRectangle (11.f, 1.5f, 10.f, 12.f, 2.f);
            g.setColour (Colour (0xff8a5a33)); g.fillPath (handle);
            g.setColour (ink); g.strokePath (handle, juce::PathStrokeType (1.4f));
            Path blade; blade.startNewSubPath (10.f, 14.f); blade.lineTo (22.f, 14.f); blade.lineTo (22.f, 23.f); blade.lineTo (10.f, 30.f); blade.closeSubPath();
            g.setColour (Colour (0xffe4eaf2)); g.fillPath (blade);
            g.setColour (juce::Colours::white); g.drawLine (11.5f, 15.f, 11.5f, 28.f, 1.6f);
            g.setColour (ink); g.strokePath (blade, juce::PathStrokeType (1.4f));
            g.setColour (Colour (0xff9aa4b2)); g.drawLine (13.f, 17.5f, 20.f, 17.5f, 1.2f);
            hx = 10; hy = 29;
        }
        else if (t == RollView::Tool::Mute)                              // speaker with a red cross
        {
            Path sp; sp.startNewSubPath (4.f, 12.f); sp.lineTo (10.f, 12.f); sp.lineTo (17.f, 6.f); sp.lineTo (17.f, 26.f); sp.lineTo (10.f, 20.f); sp.lineTo (4.f, 20.f); sp.closeSubPath();
            g.setColour (Colour (0xfff3f3f3)); g.fillPath (sp);
            g.setColour (ink); g.strokePath (sp, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved));
            juce::Path x; x.startNewSubPath (20.f, 11.f); x.lineTo (29.f, 21.f); x.startNewSubPath (29.f, 11.f); x.lineTo (20.f, 21.f);
            g.setColour (ink); g.strokePath (x, juce::PathStrokeType (5.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (Colour (0xffff4a3d)); g.strokePath (x, juce::PathStrokeType (3.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            hx = 16; hy = 16;
        }
    }
    return juce::MouseCursor (img, hx, hy);
}

const juce::MouseCursor& toolCursor (RollView::Tool t)
{
    static const juce::MouseCursor pen = makeToolCursor (RollView::Tool::Draw), eraser = makeToolCursor (RollView::Tool::Erase),
                                   razor = makeToolCursor (RollView::Tool::Split), mute = makeToolCursor (RollView::Tool::Mute);
    switch (t) { case RollView::Tool::Draw: return pen; case RollView::Tool::Erase: return eraser; case RollView::Tool::Split: return razor; default: return mute; }
}
}

const char* RollView::laneName (int lane)
{
    static const char* n[] = { "VELOCITY", "PITCH BEND", "MODULATION (CC1)", "AFTERTOUCH", "EXPRESSION (CC11)", "SUSTAIN PEDAL (CC64)", "" };
    return n[juce::jlimit (0, 6, lane)];
}

juce::Colour RollView::colourFor (int a, const juce::Colour& accent)
{
    switch (a)
    {
        case A_Legato: case A_HammerOn: case A_PullOff:                    return juce::Colour (0xff4fc38a);
        case A_Slide: case A_Gliss: case A_Bend: case A_Scoop:
        case A_Fall: case A_Doit:                                          return juce::Colour (0xffe56fb0);
        case A_Staccato: case A_Marcato: case A_PalmMute: case A_Pizz:
        case A_ColLegno:                                                   return juce::Colour (0xff5bc0eb);
        case A_Growl: case A_Flutter: case A_Shake:                        return juce::Colour (0xffe0533d);
        case A_Trill: case A_Tremolo: case A_Arpeggio: case A_Strum:       return juce::Colour (0xffa67df0);
        case A_Ghost: case A_Mute: case A_Subtone:                         return juce::Colour (0xff8d8678);
        case A_Harmonic: case A_Vibrato:                                   return juce::Colour (0xfff7e08a);
        default:                                                           return accent;
    }
}

RollView::RollView (EnochSoloProcessor& p) : proc (p)
{
    setWantsKeyboardFocus (true);
    addAndMakeVisible (hBar); addAndMakeVisible (vBar);
    vBar.addListener (this);
    vBar.setAutoHide (false);
    hBar.onChange = [this] (double s, double l)
    {
        const double len = totalBeats();
        viewLen = (l >= len - 1e-6) ? 0.0 : l; viewStart = s;
        clampView(); updateScrollBars(); repaint(); notifyView();
    };
    hBar.onFit = [this] { viewLen = 0; viewStart = 0; clampView(); updateScrollBars(); repaint(); notifyView(); };
    refresh (true);
    startTimerHz (30);
}

double RollView::totalBeats() const { return snap ? std::max (1.0, snap->lengthBeats) : 4.0; }

//------------------------------------------------------------------------------ geometry
RollView::Geo RollView::geo() const
{
    Geo g;
    auto b = getLocalBounds().toFloat().reduced (4.f);
    const float x = b.getX() + kGutter, w = std::max (10.f, b.getWidth() - kGutter - kBarThk - 2.f);
    g.chord = { x, b.getY(), w, kChordH - 3.f };
    g.ruler = { x, b.getY() + kChordH, w, kRulerH };
    g.phrase = { x, g.ruler.getBottom() + 1.f, w, kPhraseH };
    g.hbar  = { x, b.getBottom() - kBarThk, w, kBarThk };
    g.ks    = { x, g.hbar.getY() - 2.f - kKsH, w, kKsH };
    const float fixedH = kChordH + kRulerH + kPhraseH + 1.f + 3.f + 4.f + 3.f + kKsH + 2.f + kBarThk;
    const float maxLane = std::max (28.f, b.getHeight() - fixedH - 60.f);           // always leave the notes at least 60 px
    const float laneH = laneSel >= 6 ? 0.f : juce::jlimit (28.f, maxLane, laneHpx);
    g.lane  = { x, laneH > 0 ? g.ks.getY() - 3.f - laneH : g.ks.getY(), w, laneH };
    const float nt = g.phrase.getBottom() + 3.f;
    const float nb = (laneH > 0 ? g.lane.getY() : g.ks.getY()) - 4.f;
    g.notes = { x, nt, w, std::max (20.f, nb - nt) };
    g.gutter = { b.getX(), nt, kGutter - 3.f, g.notes.getHeight() };
    g.vbar = { g.notes.getRight() + 2.f, nt, kBarThk, g.notes.getHeight() };
    g.len = totalBeats();
    g.viewLen = (viewLen <= 0) ? g.len : std::min (viewLen, g.len);
    g.viewStart = juce::jlimit (0.0, std::max (0.0, g.len - g.viewLen), viewStart);
    const int rows = juce::jlimit (12, 128, visRows);
    g.hi = juce::jlimit (rows - 1, 127, topPitch); g.lo = g.hi - rows + 1;
    g.ppb = g.notes.getWidth() / (float) g.viewLen;
    g.rowH = g.notes.getHeight() / (float) rows;
    return g;
}

float RollView::xOf (const Geo& g, double beat) const { return g.notes.getX() + (float) (beat - g.viewStart) * g.ppb; }
double RollView::beatAt (const Geo& g, float x) const { return g.viewStart + (x - g.notes.getX()) / g.ppb; }
float RollView::yOf (const Geo& g, int pitch) const { return g.notes.getY() + (float) (g.hi - pitch) * g.rowH; }
int RollView::pitchAt (const Geo& g, float y) const { return g.hi - (int) std::floor ((y - g.notes.getY()) / g.rowH); }

juce::Rectangle<float> RollView::noteRect (const Geo& g, const Note& n) const
{
    return { xOf (g, n.start), yOf (g, n.pitch) + 0.5f, std::max (4.f, (float) n.length * g.ppb - 1.f), std::max (3.f, g.rowH - 1.f) };
}

int RollView::hitTest (const Geo& g, juce::Point<float> p, bool& resizeZone) const
{
    resizeZone = false; hitLeft = false;
    for (int i = (int) raw.size() - 1; i >= 0; --i)
    {
        if (raw[(size_t) i].track == 2) continue;
        auto r = noteRect (g, raw[(size_t) i]).expanded (0.f, 0.5f);
        if (r.contains (p))
        {
            // the right edge grabs a note for resizing; short notes get a smaller edge so they can still be moved
            const float zone = juce::jlimit (2.5f, 6.f, r.getWidth() * 0.4f);
            resizeZone = (p.x > r.getRight() - zone && r.getWidth() > 5.f);
            hitLeft = ! resizeZone && r.getWidth() > 10.f && p.x < r.getX() + juce::jmin (zone, r.getWidth() * 0.3f);   // v3.9: the start edge
            return i;
        }
    }
    // just past the end of a note counts as its resize handle too (tiny 16th notes would otherwise be impossible to lengthen)
    for (int i = (int) raw.size() - 1; i >= 0; --i)
    {
        if (raw[(size_t) i].track == 2) continue;
        const auto r = noteRect (g, raw[(size_t) i]);
        if (juce::Rectangle<float> (r.getRight(), r.getY(), 5.f, r.getHeight()).contains (p)) { resizeZone = true; return i; }
    }
    return -1;
}

int RollView::barAtX (const Geo& g, float x) const
{
    const double bb = snap ? std::max (0.5, snap->pattern.barBeats) : 4.0;
    const int bars = snap ? snap->pattern.bars : 1;
    return juce::jlimit (0, std::max (0, bars - 1), (int) std::floor (beatAt (g, x) / bb));
}

juce::Rectangle<int> RollView::cellRect (const Geo& g, int bar) const
{
    const double bb = snap ? std::max (0.5, snap->pattern.barBeats) : 4.0;
    return juce::Rectangle<float> (xOf (g, bar * bb), g.chord.getY(), (float) bb * g.ppb, g.chord.getHeight()).getIntersection (g.chord).toNearestInt();
}

//------------------------------------------------------------------------------ grid / snapping
double RollView::gridBeats() const
{
    const int i = juce::roundToInt (proc.apvts.getRawParameterValue ("grid")->load());
    static const double v[] = { 1.0, 2.0 / 3.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125, 1.0 / 12.0, 0.0 };
    return v[juce::jlimit (0, 8, i)];
}

bool RollView::snapIsOn() const { return proc.apvts.getRawParameterValue ("snapGrid")->load() >= 0.5f; }

double RollView::snapBeat (double b, bool floorIt) const
{
    const double g = gridBeats();
    if (g <= 0 || ! snapIsOn()) return b;     // grid snapping switched off = free editing
    return (floorIt ? std::floor (b / g + 1e-6) : std::round (b / g)) * g;
}

int RollView::snapPitch (int pitch, double beat) const
{
    pitch = juce::jlimit (0, 127, pitch);
    if (proc.apvts.getRawParameterValue ("snapScale")->load() < 0.5f || ! snap || snap->pattern.chords.empty()) return pitch;
    const auto mask = snap->pattern.chordAt (beat).scaleMask;
    for (int d = 0; d <= 6; ++d)
    {
        if (pitch + d <= 127 && inMask (mask, pitch + d)) return pitch + d;
        if (pitch - d >= 0 && inMask (mask, pitch - d)) return pitch - d;
    }
    return pitch;
}

// v3.4: a scale-locked bar keeps its notes in the chord scale. dir: +1 steps up, -1 steps down, 0 = nearest.
int RollView::pitchFor (int pitch, double beat, int dir) const
{
    const double bbl = snap ? std::max (0.5, snap->pattern.barBeats) : 4.0;
    if (! proc.isScaleLocked ((int) std::floor (beat / bbl + 1e-6))) return snapPitch (pitch, beat);
    const uint16_t m = (snap && ! snap->pattern.chords.empty()) ? snap->pattern.chordAt (beat).scaleMask : (uint16_t) 0;
    return enochsolo::barlock::snapInScale (juce::jlimit (0, 127, pitch), m, dir);
}
double RollView::metricWeight (double beat) const
{
    const double bb = snap ? snap->pattern.barBeats : 4.0;
    const double pos = std::fmod (beat, bb);
    if (pos < 1e-6) return 1.0;
    if (std::fmod (pos, 1.0) < 1e-6) return 0.6;
    if (std::fmod (pos, 0.5) < 1e-6) return 0.35;
    return 0.2;
}

//------------------------------------------------------------------------------ view state
void RollView::recomputeRange()
{
    if (! vAuto) return;
    int lo = 127, hi = 0;
    for (auto& n : raw) { lo = std::min (lo, n.pitch); hi = std::max (hi, n.pitch); }
    if (const auto kv = ksView(); kv.on)                                       // v3.9: the key-switches Enoch wrote are in view too
        for (auto& ev : ksEvents()) { const int r = ev.key - kv.shift; if (r >= 0 && r < 128) { lo = std::min (lo, r); hi = std::max (hi, r); } }
    if (lo > hi) { lo = 52; hi = 76; }
    lo = std::max (0, lo - 3); hi = std::min (127, hi + 3);
    while (hi - lo < 19) { if (lo > 0) --lo; if (hi - lo < 19 && hi < 127) ++hi; }
    visRows = hi - lo + 1; topPitch = hi;
}

void RollView::clampView()
{
    const double len = totalBeats();
    if (viewLen <= 0 || viewLen >= len) { viewLen = 0; viewStart = 0; }
    else viewStart = juce::jlimit (0.0, len - viewLen, viewStart);
    visRows = juce::jlimit (12, 128, visRows);
    topPitch = juce::jlimit (visRows - 1, 127, topPitch);
}

double RollView::getZoomH() const
{
    const double len = totalBeats(), minLen = std::min (len, 2.0);
    if (viewLen <= 0 || viewLen >= len || len <= minLen + 1e-9) return 0.0;
    return juce::jlimit (0.0, 1.0, std::log (viewLen / len) / std::log (minLen / len));
}

void RollView::setZoomH (double z)
{
    const double len = totalBeats(), minLen = std::min (len, 2.0);
    const double cur = (viewLen <= 0) ? len : std::min (viewLen, len);
    const double centre = viewStart + cur / 2;
    z = juce::jlimit (0.0, 1.0, z);
    if (z <= 0.001 || len <= minLen + 1e-9) { viewLen = 0; viewStart = 0; }
    else { viewLen = len * std::exp (std::log (minLen / len) * z); viewStart = centre - viewLen / 2; }
    clampView(); updateScrollBars(); repaint();
}

double RollView::getZoomV() const
{
    return juce::jlimit (0.0, 1.0, (std::log (128.0) - std::log ((double) visRows)) / (std::log (128.0) - std::log (12.0)));
}

void RollView::setZoomV (double z)
{
    z = juce::jlimit (0.0, 1.0, z);
    const int rows = juce::jlimit (12, 128, (int) std::lround (std::exp (std::log (128.0) - z * (std::log (128.0) - std::log (12.0)))));
    const double centre = topPitch - visRows / 2.0;
    visRows = rows; topPitch = (int) std::lround (centre + rows / 2.0); vAuto = false;
    clampView(); updateScrollBars(); repaint();
}

void RollView::zoomFit()
{
    viewLen = 0; viewStart = 0; vAuto = true; recomputeRange(); clampView(); updateScrollBars(); repaint(); notifyView();
}

void RollView::zoomAround (double factor, double anchorBeat)
{
    const double len = totalBeats();
    const double cur = (viewLen <= 0) ? len : std::min (viewLen, len);
    const double frac = juce::jlimit (0.0, 1.0, (anchorBeat - viewStart) / cur);
    const double nl = juce::jlimit (std::min (len, 2.0), len, cur * factor);
    if (nl >= len - 1e-6) { viewLen = 0; viewStart = 0; }
    else { viewLen = nl; viewStart = anchorBeat - frac * nl; }
    clampView(); updateScrollBars(); repaint(); notifyView();
}

void RollView::updateScrollBars()
{
    const auto g = geo();
    hBar.setView (g.len, g.viewStart, g.viewLen);
    vBar.setRangeLimits (0.0, 128.0);
    vBar.setCurrentRange ((double) (127 - g.hi), (double) (g.hi - g.lo + 1), juce::dontSendNotification);
    vBar.setEnabled (visRows < 128);
}

void RollView::scrollBarMoved (juce::ScrollBar* b, double newStart)
{
    (void) b;
    topPitch = 127 - (int) std::lround (newStart); vAuto = false; clampView();
    repaint(); notifyView();
}

void RollView::resized()
{
    const auto g = geo();
    hBar.setBounds (g.hbar.toNearestInt());
    vBar.setBounds (g.vbar.toNearestInt());
    updateScrollBars();
}

void RollView::setTool (Tool t) { tool = t; updateMouseCursor(); if (onToolChanged) onToolChanged(); }
void RollView::setLane (int l) { laneSel = juce::jlimit (0, 6, l); resized(); repaint(); }

//------------------------------------------------------------------------------ zoom bar (overview scrollbar with draggable ends)
float ZoomBar::xOf (double beat) const
{
    const float w = (float) getWidth() - 2.f;
    return 1.f + (float) (beat / tot) * w;
}

int ZoomBar::zoneAt (float x) const
{
    const float l = xOf (st), r = xOf (st + ln);
    const float edge = std::min (9.f, std::max (4.f, (r - l) / 3.f));
    if (x >= l - 5.f && x <= l + edge) return 1;
    if (x >= r - edge && x <= r + 5.f) return 2;
    if (x > l && x < r) return 3;
    return 0;
}

void ZoomBar::paint (juce::Graphics& g)
{
    const auto& T = EnochLookAndFeel::of (*this);
    auto b = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (T.panel2); g.fillRoundedRectangle (b, 5.f);
    g.setColour (T.border); g.drawRoundedRectangle (b, 5.f, 1.f);
    const float l = xOf (st), r = std::max (l + 14.f, xOf (st + ln));
    auto th = juce::Rectangle<float> (l, b.getY() + 1.5f, r - l, b.getHeight() - 3.f);
    const bool full = ln >= tot - 1e-6;
    g.setColour (T.accent.withAlpha (full ? 0.35f : (mode == 3 ? 0.9f : 0.7f))); g.fillRoundedRectangle (th, 4.f);
    // end handles: brighter when the pointer is over them, with little left / right arrows
    for (int side = 1; side <= 2; ++side)
    {
        const bool active = (hoverZone == side && mode == 0) || mode == side;
        const auto h = side == 1 ? juce::Rectangle<float> (th.getX(), th.getY(), 9.f, th.getHeight())
                                 : juce::Rectangle<float> (th.getRight() - 9.f, th.getY(), 9.f, th.getHeight());
        g.setColour (active ? juce::Colours::white.withAlpha (0.95f) : juce::Colours::white.withAlpha (0.38f));
        juce::Path tri;
        const float cx = h.getCentreX(), cy = h.getCentreY();
        if (side == 1) tri.addTriangle (cx + 2.2f, cy - 3.5f, cx + 2.2f, cy + 3.5f, cx - 2.6f, cy);
        else           tri.addTriangle (cx - 2.2f, cy - 3.5f, cx - 2.2f, cy + 3.5f, cx + 2.6f, cy);
        g.fillPath (tri);
    }
}

void ZoomBar::mouseMove (const juce::MouseEvent& e)
{
    const int z = zoneAt (e.position.x);
    if (z != hoverZone) { hoverZone = z; repaint(); }
    setMouseCursor (z == 1 || z == 2 ? juce::MouseCursor::LeftRightResizeCursor
                                     : (z == 3 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor));
}

void ZoomBar::mouseDown (const juce::MouseEvent& e)
{
    mode = zoneAt (e.position.x);
    o0 = st; o1 = st + ln; downX = e.position.x;
    if (mode == 0)                                    // click on the empty track: centre the view there
    {
        const double b = (double) (e.position.x - 1.f) / std::max (1.f, (float) getWidth() - 2.f) * tot;
        o0 = juce::jlimit (0.0, std::max (0.0, tot - ln), b - ln / 2.0); o1 = o0 + ln; mode = 3;
        if (onChange) onChange (o0, ln);
    }
    repaint();
}

void ZoomBar::mouseDrag (const juce::MouseEvent& e)
{
    if (mode == 0 || ! onChange) return;
    const double d = (double) (e.position.x - downX) / std::max (1.f, (float) getWidth() - 2.f) * tot;
    if (mode == 1)        { const double ns = juce::jlimit (0.0, o1 - minLen(), o0 + d); onChange (ns, o1 - ns); }
    else if (mode == 2)   { const double ne = juce::jlimit (o0 + minLen(), tot, o1 + d); onChange (o0, ne - o0); }
    else                  { const double len = o1 - o0; onChange (juce::jlimit (0.0, std::max (0.0, tot - len), o0 + d), len); }
}

//------------------------------------------------------------------------------ data
void RollView::refresh (bool force)
{
    auto s = proc.getSnapshot();
    const int num = s ? s->number : -1;
    if (! force && num == lastNumber) return;
    snap = s; lastNumber = num;
    if (drag == Drag::None)
    {
        raw = snap ? snap->pattern.raw : std::vector<Note>();
        phrases = snap ? findPhrases (snap->pattern) : std::vector<Phrase>();
        std::set<int> ids; for (auto& n : raw) ids.insert (n.id);
        for (auto it = selected.begin(); it != selected.end();) it = ids.count (*it) ? std::next (it) : selected.erase (it);
        recomputeRange();
        clampView();
        if (snap && barA >= snap->pattern.bars) { barA = barB = -1; notifyBars(); }
        updateScrollBars();
    }
    repaint();
    if (onBarSelection) onBarSelection();
}

void RollView::timerCallback()
{
    const double pb = proc.getPlayBeat();
    const bool moved2 = std::abs (pb - lastBeat) > 1e-9;
    const double prevBeat = lastBeat;
    lastBeat = pb;
    auto s = proc.getSnapshot();
    // v3.8: ghost notes of the other Enoch plugin (checked once a second; the file only changes when its notes change)
    if (proc.ghostView.load() && juce::Time::getMillisecondCounter() - ghostCheckAt > 1000)
    {
        ghostCheckAt = juce::Time::getMillisecondCounter();
        const auto before = ghostStamp;
        if (! enochx::readOtherShare (JucePlugin_Name, ghosts, ghostStamp)) { if (! ghosts.notes.empty()) { ghosts = {}; repaint(); } }
        else if (ghostStamp != before) repaint();
    }
    // v3.8 Follow: when the playhead leaves the visible page, the page turns with it (not while you drag)
    if (moved2 && pb >= 0 && proc.followView.load() && drag == Drag::None && snap && viewLen > 0)
    {
        const auto G = geo();
        if (G.len > 0 && G.viewLen < G.len - 1e-6)
        {
            const double at = std::fmod (pb, G.len);
            if (at < G.viewStart - 1e-6 || at > G.viewStart + G.viewLen * 0.97)
            {
                viewStart = at - G.viewLen * 0.03; clampView(); updateScrollBars(); repaint(); notifyView();
                return;
            }
        }
    }
    if ((s ? s->number : -1) != lastNumber) refresh();
    else if (moved2)
    {
        // v3.7: only the thin strips where the playhead was and is now are redrawn, so the roll stays light while playing
        if (! snap) { repaint(); return; }
        const auto G = geo();
        auto strip = [&] (double beat) { if (beat < 0 || G.len <= 0) return; const int x = (int) xOf (G, std::fmod (beat, G.len)); repaint (x - 4, 0, 9, getHeight()); };
        strip (prevBeat); strip (pb);
    }
}

void RollView::commit (std::vector<Note> v)
{
    proc.pushUndo();
    proc.commitRaw (std::move (v));
    refresh (true);
}

//------------------------------------------------------------------------------ controller lane helpers
int RollView::valueAtY (const Geo& g, float y, int lane) const
{
    const float u = juce::jlimit (0.f, 1.f, 1.f - (y - g.lane.getY()) / g.lane.getHeight());
    if (lane == L_Bend)
    {
        int v = (int) std::lround (u * 16383.f);
        if (std::abs (v - 8192) < 420) v = 8192;      // magnetic centre
        return v;
    }
    return (int) std::lround (u * 127.f);
}

float RollView::yOfValue (const Geo& g, int value, int lane) const
{
    const float u = lane == L_Bend ? (float) value / 16383.f : (float) value / 127.f;
    return g.lane.getBottom() - juce::jlimit (0.f, 1.f, u) * g.lane.getHeight();
}

double RollView::ctrlStep (const Geo& g) const { return std::max (1.0 / 64.0, 3.0 / (double) g.ppb); }

void RollView::drawShape (int shape, int lane, double t0, double t1)
{
    if (! snap) return;
    const auto& P = snap->pattern;
    if (t1 - t0 < 0.01) { t0 = 0; t1 = P.lengthBeats; }
    const int maxV = lane == L_Bend ? 16383 : 127, mid = lane == L_Bend ? 8192 : 64;
    const double unit = 8192.0 / std::max (1, P.s.bendRange);          // one semitone in bend units
    const double period = std::max (0.1, (P.s.bpm / 60.0) / 5.8);      // beats per vibrato cycle (about 5.8 Hz)
    const double step = (shape == 4) ? period / 8.0 : 0.125;
    std::vector<CtrlPoint> pts;
    auto add = [&] (double t, double v) { CtrlPoint c; c.t = t; c.lane = lane; c.value = juce::jlimit (0, maxV, (int) std::lround (v)); pts.push_back (c); };
    for (double t = t0; t < t1 + 1e-9; t += step)
    {
        const double tt = std::min (t, t1), u = (tt - t0) / std::max (1e-6, t1 - t0);
        switch (shape)
        {
            case 1: add (tt, lane == L_Bend ? mid : 10 + 110 * u); break;                                   // ramp up (bend: nothing to ramp)
            case 2: add (tt, lane == L_Bend ? mid : 120 - 110 * u); break;                                  // ramp down
            case 3: add (tt, lane == L_Bend ? mid : 10 + 110 * std::sin (juce::MathConstants<double>::pi * u)); break;   // swell
            case 4: { const double s = std::sin (juce::MathConstants<double>::twoPi * (tt - t0) / period);          // vibrato wave
                      add (tt, lane == L_Bend ? mid + 0.3 * unit * s : (lane == L_Mod ? 20.0 + 80.0 * (0.5 + 0.5 * s) : 50.0 + 40.0 * s)); break; }
            case 5: add (tt, lane == L_Bend ? mid : (lane == L_Expression ? 100 : 0)); break;                       // reset
            case 6: add (tt, lane == L_Bend ? mid + 2.0 * unit * u : mid); break;                               // rise
            default: add (tt, lane == L_Bend ? mid - 2.0 * unit * u : mid); break;                              // dive
        }
        if (tt >= t1) break;
    }
    if (shape == 5 && pts.size() > 2) pts.resize (2);
    proc.commitCtrl (lane, t0, t1, pts);
    refresh (true);
}

//------------------------------------------------------------------------------ painting
void RollView::paint (juce::Graphics& g)
{
    const auto& T = LF::of (*this);
    auto b = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (T.bg.brighter (0.03f), 0.f, b.getY(), T.bg.darker (0.25f), 0.f, b.getBottom(), false));
    g.fillRoundedRectangle (b, 10.f);
    if (! snap) return;
    const auto G = geo();
    const auto& P = snap->pattern;
    const double bb = std::max (0.5, P.barBeats);
    const int selLo = barSelFirst(), selHi = barSelLast();

    // ---------- chord cells (one per bar). Click = select the bar and choose its chord.
    if (! P.chords.empty())
    {
        const auto& bc = proc.getBarChords();
        const auto& busy = proc.getBarBusy();
        for (int bar = 0; bar < P.bars; ++bar)
        {
            const float x0 = xOf (G, bar * bb), x1 = xOf (G, (bar + 1) * bb);
            if (x1 < G.chord.getX() - 1 || x0 > G.chord.getRight() + 1) continue;
            auto cell = juce::Rectangle<float> (std::max (x0, G.chord.getX()) + 1.5f, G.chord.getY(), std::min (x1, G.chord.getRight()) - std::max (x0, G.chord.getX()) - 3.f, G.chord.getHeight());
            if (cell.getWidth() < 4) continue;
            const bool isSet = bar < (int) bc.size() && bc[(size_t) bar].set;
            const bool sel = bar >= selLo && bar <= selHi && selLo >= 0;
            const bool hov = (bar == hoverBar);
            if (sel) { g.setGradientFill (LF::accentGradient (T, cell)); g.fillRoundedRectangle (cell, 6.f); }
            else
            {
                g.setGradientFill (juce::ColourGradient ((isSet ? T.accent.withAlpha (hov ? 0.36f : 0.24f) : T.panel2.brighter (hov ? 0.22f : 0.06f)), 0.f, cell.getY(),
                                                         (isSet ? T.accent.withAlpha (hov ? 0.22f : 0.12f) : T.panel2.darker (0.1f)), 0.f, cell.getBottom(), false));
                g.fillRoundedRectangle (cell, 6.f);
            }
            g.setColour (sel ? juce::Colours::white.withAlpha (0.5f) : (isSet ? T.accent.withAlpha (0.8f) : T.border.brighter (0.1f))); g.drawRoundedRectangle (cell, 6.f, 1.f);
            g.setColour (sel ? juce::Colours::black : LF::text());
            g.setFont (LF::fnt (cell.getWidth() < 46.f ? 11.f : 14.f, "Bold"));
            g.drawText (juce::String (P.chordAt (bar * bb + 0.01).name), cell.withTrimmedRight (cell.getWidth() > 40.f ? 12.f : 0.f), juce::Justification::centred);
            if (cell.getWidth() > 40.f)
            {
                juce::Path t; const float cx = cell.getRight() - 9.f, cy = cell.getCentreY();
                t.addTriangle (cx - 3.5f, cy - 2.f, cx + 3.5f, cy - 2.f, cx, cy + 3.f);
                g.setColour (sel ? juce::Colours::black : T.accent); g.fillPath (t);
            }
            if (bar < (int) busy.size() && busy[(size_t) bar] >= 0.f)      // a small "busy" meter under the chord name
            {
                g.setColour (juce::Colours::black.withAlpha (0.35f)); g.fillRect (cell.getX() + 4.f, cell.getBottom() - 4.f, cell.getWidth() - 8.f, 2.f);
                g.setColour (LF::red().interpolatedWith (T.accent, 1.f - busy[(size_t) bar]));
                g.fillRect (cell.getX() + 4.f, cell.getBottom() - 4.f, (cell.getWidth() - 8.f) * busy[(size_t) bar], 2.f);
            }
        }
    }

    // ---------- ruler (bar numbers; scroll the wheel here to zoom)
    g.setColour (T.panel2); g.fillRect (G.ruler);
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (G.ruler.toNearestInt());
        g.setFont (juce::Font (juce::FontOptions (10.5f)));
        for (int bar = 0; bar <= P.bars; ++bar)
        {
            const float x = xOf (G, bar * bb);
            g.setColour (LF::dim()); g.drawVerticalLine ((int) x, G.ruler.getY(), G.ruler.getBottom());
            if (bar < P.bars) g.drawText (juce::String (bar + 1), (int) x + 3, (int) G.ruler.getY(), 30, (int) G.ruler.getHeight(), juce::Justification::centredLeft);
        }
        if (G.ppb > 40.f)
            for (double t = 0; t < G.len; t += 1.0)
            { const float x = xOf (G, t); g.setColour (LF::dim().withAlpha (0.5f)); g.drawVerticalLine ((int) x, G.ruler.getY() + 8.f, G.ruler.getBottom()); }
    }

    // ---------- phrase strip: one block per phrase. Click = lock / unlock it (locked phrases survive "Try another"), right-click = more
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (G.phrase.toNearestInt());
        const auto& locks = proc.getPhraseLocks();
        g.setColour (T.panel2.darker (0.25f)); g.fillRect (G.phrase);
        g.setFont (juce::Font (juce::FontOptions (10.f, juce::Font::bold)));
        for (size_t i = 0; i < phrases.size(); ++i)
        {
            const auto& ph = phrases[i];
            const float x0 = xOf (G, ph.t0), x1 = xOf (G, ph.t1);
            if (x1 < G.phrase.getX() || x0 > G.phrase.getRight()) continue;
            auto r = juce::Rectangle<float> (x0, G.phrase.getY() + 1.f, std::max (6.f, x1 - x0), G.phrase.getHeight() - 2.f);
            const bool locked = phraseLocked (locks, ph), hov = (int) i == hoverPhrase;
            g.setColour (locked ? T.accent.withAlpha (hov ? 1.f : 0.85f) : T.accent.withAlpha (hov ? 0.45f : 0.2f));
            g.fillRoundedRectangle (r, 4.f);
            g.setColour (locked ? T.accent.brighter (0.5f) : T.accent.withAlpha (0.6f)); g.drawRoundedRectangle (r, 4.f, 1.f);
            if (r.getWidth() > 16.f)
            {
                // little padlock
                const float cx = r.getX() + 8.f, cy = r.getCentreY();
                g.setColour (locked ? juce::Colours::black : LF::text().withAlpha (0.8f));
                g.fillRoundedRectangle (cx - 3.f, cy - 0.5f, 6.f, 4.5f, 1.f);
                juce::Path sh; sh.addArc (cx - 2.f, cy - 4.f, 4.f, 5.f, -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi, true);
                if (locked) g.strokePath (sh, juce::PathStrokeType (1.2f));
                else { juce::Path op; op.addArc (cx - 2.f + 3.f, cy - 5.f, 4.f, 5.f, -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi, true); g.strokePath (op, juce::PathStrokeType (1.2f)); }
                if (r.getWidth() > 70.f)
                    g.drawText (locked ? "LOCKED" : "phrase " + juce::String ((int) i + 1), r.withTrimmedLeft (16.f), juce::Justification::centredLeft);
            }
        }
        // v3.14: your splits: a bright cut line with a little notch
        for (double c : proc.getPhraseCuts())
        {
            const float x = xOf (G, c);
            if (x < G.phrase.getX() || x > G.phrase.getRight()) continue;
            g.setColour (juce::Colours::white.withAlpha (0.9f)); g.fillRect (x - 1.f, G.phrase.getY(), 2.f, G.phrase.getHeight());
            juce::Path tri; tri.addTriangle (x - 4.f, G.phrase.getY(), x + 4.f, G.phrase.getY(), x, G.phrase.getY() + 5.f); g.fillPath (tri);
        }
        if (phrases.empty()) { g.setColour (LF::dim()); g.drawText ("phrases appear here once there are notes", G.phrase, juce::Justification::centred); }
    }

    const bool showScale = proc.apvts.getRawParameterValue ("showScale")->load() > 0.5f;
    // ---------- piano gutter (click a key to hear it); with the scale shown, the notes of the selected bar's scale carry a dot
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (G.gutter.getSmallestIntegerContainer());
        const ChordSpan* gc = nullptr;
        if (showScale && ! P.chords.empty()) gc = &P.chordAt ((barSelFirst() >= 0 ? barSelFirst() : 0) * bb + 0.01);
        for (int p = G.lo; p <= G.hi; ++p)
        {
            const int pc = p % 12; const bool blk = (pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10);
            const float y = yOf (G, p);
            if (blk) g.setGradientFill (juce::ColourGradient (juce::Colour (0xff262a33), G.gutter.getX(), 0.f, juce::Colour (0xff0e1015), G.gutter.getRight(), 0.f, false));
            else     g.setGradientFill (juce::ColourGradient (juce::Colour (0xffc9cfd9), G.gutter.getX(), 0.f, juce::Colour (0xffeef1f5), G.gutter.getRight(), 0.f, false));
            g.fillRect (G.gutter.getX(), y, G.gutter.getWidth() - (blk ? 6.f : 0.f), G.rowH - 0.5f);
            if (! blk) { g.setColour (juce::Colour (0xff8d95a3)); g.fillRect (G.gutter.getX(), y + G.rowH - 0.75f, G.gutter.getWidth(), 0.5f); }
            if (pc == 0 || G.rowH > 12.f)
            {
                g.setColour (pc == 0 ? juce::Colour (0xff1a1d24) : (blk ? juce::Colour (0xff8b93a3) : juce::Colour (0xff5d6573)));
                g.setFont (LF::fnt (juce::jmin (10.f, G.rowH + 1.f), pc == 0 ? "Bold" : "Medium"));
                g.drawText (juce::String (noteName (p)), (int) G.gutter.getX() + 2, (int) y, (int) G.gutter.getWidth() - 4, (int) std::max (1.f, G.rowH), juce::Justification::centredRight);
            }
            if (gc && inMask (gc->scaleMask, p))
            {
                // v3.14: the scale's keys are royal blue (the root deepest), easy to see while editing
                const bool root = p % 12 == gc->rootPc % 12;
                g.setColour (juce::Colour (0xff4169e1).withAlpha (root ? 0.95f : 0.65f));
                g.fillRect (G.gutter.getX(), y, G.gutter.getWidth() * 0.45f, G.rowH - 0.5f);
                const float r = std::min (2.6f, G.rowH * 0.3f);
                g.setColour (juce::Colours::white.withAlpha (root ? 1.f : 0.8f));
                g.fillEllipse (G.gutter.getX() + 3.f, y + (G.rowH - 0.5f) * 0.5f - r, r * 2.f, r * 2.f);
            }
        }
    }

    // ---------- note area
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (G.notes.getSmallestIntegerContainer());
        for (int p = G.lo; p <= G.hi; ++p)
        {
            const int pc = p % 12; const bool blk = (pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10);
            const float y = yOf (G, p);
            g.setColour (juce::Colours::white.withAlpha (blk ? 0.022f : 0.05f));
            g.fillRect (G.notes.getX(), y, G.notes.getWidth(), G.rowH - 0.5f);
        }
        // chord tones of every bar's chord glow a little, so you can see where the safe notes are
        for (auto& c : P.chords)
        {
            const float x0 = std::max (G.notes.getX(), xOf (G, c.start)), x1 = std::min (G.notes.getRight(), xOf (G, c.end));
            if (x1 <= x0) continue;
            if (showScale)      // rows outside the bar's scale are dimmed, scale notes stay bright
                for (int p = G.lo; p <= G.hi; ++p)
                {
                    if (! inMask (c.scaleMask, p)) { g.setColour (juce::Colours::black.withAlpha (0.30f)); g.fillRect (x0, yOf (G, p), x1 - x0, G.rowH - 0.5f); }
                    else if (! inMask (c.toneMask, p)) { g.setColour (juce::Colours::white.withAlpha (0.035f)); g.fillRect (x0, yOf (G, p), x1 - x0, G.rowH - 0.5f); }
                }
            for (int p = G.lo; p <= G.hi; ++p)
                if (inMask (c.toneMask, p))
                {
                    g.setColour (T.accent.withAlpha ((p % 12) == (c.rootPc % 12) ? 0.14f : 0.07f));
                    g.fillRect (x0, yOf (G, p), x1 - x0, G.rowH - 0.5f);
                }
        }
        // selected bars
        if (selLo >= 0)
        {
            const float x0 = xOf (G, selLo * bb), x1 = xOf (G, (selHi + 1) * bb);
            g.setColour (T.accent.withAlpha (0.10f)); g.fillRect (x0, G.notes.getY(), x1 - x0, G.notes.getHeight());
            g.setColour (T.accent.withAlpha (0.6f)); g.drawVerticalLine ((int) x0, G.notes.getY(), G.notes.getBottom()); g.drawVerticalLine ((int) x1, G.notes.getY(), G.notes.getBottom());
        }
        // v3.14 / v2.14 Show scale: the scale's rows light up in royal blue on the selected bars (on every bar while none is selected,
        // a little softer), the root brightest, so a note can be moved onto the scale at a glance
        if (showScale)
        {
            const juce::Colour royal (0xff4169e1);
            const bool any = selLo >= 0;
            for (auto& c : P.chords)
            {
                double a = c.start, b = c.end;
                if (any) { a = std::max (a, selLo * bb); b = std::min (b, (selHi + 1) * bb); }
                const float x0 = std::max (G.notes.getX(), xOf (G, a)), x1 = std::min (G.notes.getRight(), xOf (G, b));
                if (x1 <= x0) continue;
                const float k = any ? 1.f : 0.6f;
                for (int p = G.lo; p <= G.hi; ++p)
                {
                    if (! inMask (c.scaleMask, p)) continue;
                    const bool root = p % 12 == c.rootPc % 12;
                    const float y = yOf (G, p);
                    g.setColour (royal.withAlpha ((root ? 0.72f : 0.48f) * k));
                    g.fillRect (x0, y, x1 - x0, G.rowH - 0.5f);
                    g.setColour (royal.brighter (0.4f).withAlpha ((root ? 0.9f : 0.55f) * k));
                    g.fillRect (x0, y, x1 - x0, std::min (1.f, G.rowH * 0.12f));
                }
            }
        }
        // grid lines: bars, beats, then the chosen grid (triplet grids tinted). Grid Visibility scales all three.
        const float vis = juce::jlimit (0.f, 2.f, proc.apvts.getRawParameterValue ("gridVis")->load() / 50.f);
        const double gb = gridBeats();
        if (gb > 0 && gb * G.ppb > 7.f)
        {
            const bool trip = std::abs (gb * 3.0 - std::round (gb * 3.0)) < 1e-6 && std::abs (gb * 2.0 - std::round (gb * 2.0)) > 1e-6;
            // v3.9: 16th and 32nd lines in their own bright colours, so they stand out from the background while editing
            auto onGrid = [] (double t, double step) { const double r = std::fmod (t + 1e-7, step); return r < 2e-6 || step - r < 2e-6; };
            for (int i = 0; i * gb <= G.len + 1e-6; ++i)
            {
                const double t = i * gb;
                if (onGrid (t, 1.0)) continue;                                                   // beat lines are drawn below
                juce::Colour c;
                if (trip) c = T.accent.withAlpha (juce::jmin (1.f, 0.16f * vis));
                else if (onGrid (t, 0.5)) c = juce::Colours::white.withAlpha (juce::jmin (1.f, 0.09f * vis));                 // 8ths
                else if (onGrid (t, 0.25)) c = juce::Colour (0xff3fd8ff).withAlpha (juce::jmin (1.f, 0.30f * vis));            // 16ths: bright cyan
                else c = juce::Colour (0xffc39bff).withAlpha (juce::jmin (1.f, 0.24f * vis));                                  // 32nds and finer: violet
                g.setColour (c); g.drawVerticalLine ((int) xOf (G, t), G.notes.getY(), G.notes.getBottom());
            }
        }
        for (double t = 0; t <= G.len + 1e-6; t += 1.0)
        { g.setColour (juce::Colours::white.withAlpha (juce::jmin (1.f, 0.11f * vis))); g.drawVerticalLine ((int) xOf (G, t), G.notes.getY(), G.notes.getBottom()); }
        for (int bar = 0; bar <= P.bars; ++bar)
        { g.setColour (juce::Colours::white.withAlpha (juce::jmax (0.06f, 0.16f * vis))); g.drawVerticalLine ((int) xOf (G, bar * bb), G.notes.getY(), G.notes.getBottom()); }
        // v3.4 bar locks: a strip along the top of the bar (accent = scale lock, gold = bar lock)
        for (int bar = 0; bar < P.bars; ++bar)
        {
            const bool sc = proc.isScaleLocked (bar), bl = proc.isBarLocked (bar);
            if (! sc && ! bl) continue;
            const float x0 = xOf (G, bar * bb), x1 = xOf (G, (bar + 1) * bb);
            if (sc) { g.setColour (T.accent.withAlpha (0.85f)); g.fillRect (x0, (float) G.notes.getY(), x1 - x0, 3.f); }
            if (bl) { g.setColour (juce::Colour (0xffd9b35a)); g.fillRect (x0, (float) G.notes.getY() + 4.f, x1 - x0, 3.f); }
        }

        // v3.8 ghost notes: the other Enoch plugin's take, faint and outlined, under this one's notes
        if (proc.ghostView.load() && ! ghosts.notes.empty())
        {
            juce::Graphics::ScopedSaveState gs (g);
            g.reduceClipRegion (G.notes.getSmallestIntegerContainer());
            const auto gc = juce::Colour (0xffc9a7ff);
            int below = 0, above = 0;
            for (auto& n : ghosts.notes)
            {
                if (n.start >= G.len) continue;
                const float x = xOf (G, n.start), wN = std::max (3.f, (float) n.length * G.ppb - 1.f);
                if (x + wN < G.notes.getX() || x > G.notes.getRight()) continue;
                if (n.pitch < G.lo || n.pitch > G.hi)
                {
                    // out of view: a tick on the edge it is beyond, so the rhythm still lines up
                    const bool low = n.pitch < G.lo; (low ? below : above)++;
                    g.setColour (gc.withAlpha (n.track == 0 ? 0.55f : 0.30f));
                    g.fillRect (x, low ? G.notes.getBottom() - 4.f : G.notes.getY(), wN, 3.f);
                    continue;
                }
                juce::Rectangle<float> r (x, yOf (G, n.pitch) + 1.f, wN, std::max (2.f, G.rowH - 2.f));
                g.setColour (gc.withAlpha (n.track == 0 ? 0.16f : 0.09f)); g.fillRoundedRectangle (r, 2.f);
                g.setColour (gc.withAlpha (n.track == 0 ? 0.50f : 0.28f)); g.drawRoundedRectangle (r, 2.f, 1.f);
            }
            const juce::String lbl = "ghost notes: " + ghosts.plugin + (below > 0 ? "  (more below: scroll down)" : (above > 0 ? "  (more above: scroll up)" : ""));
            g.setFont (juce::Font (juce::FontOptions (10.5f)));
            const float lw = 14.f + 6.f * (float) lbl.length();
            auto lb = juce::Rectangle<float> (G.notes.getRight() - lw - 6.f, G.notes.getBottom() - 22.f, lw, 16.f);
            g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillRoundedRectangle (lb, 4.f);
            g.setColour (gc.withAlpha (0.85f)); g.drawText (lbl, lb, juce::Justification::centred);
        }

        // v3.9 key-switches for the external instrument: their rows are tinted, and each key-switch Enoch wrote sits on its key,
        // held under the note it plays, named after the articulation. This is exactly what goes into the MIDI.
        if (const auto kv = ksView(); kv.on && kv.lib)
        {
            juce::Graphics::ScopedSaveState ks (g);
            g.reduceClipRegion (G.notes.getSmallestIntegerContainer());
            const auto kc = juce::Colour (0xffffa040);
            for (int r = G.lo; r <= G.hi; ++r)
                if (kv.row[(size_t) r])
                {
                    const float y = yOf (G, r);
                    g.setColour (kc.withAlpha (0.045f)); g.fillRect (G.notes.getX(), y, G.notes.getWidth(), G.rowH);
                    if (G.rowH >= 9.f)
                    {
                        g.setColour (kc.withAlpha (0.5f)); g.setFont (juce::Font (juce::FontOptions (juce::jmin (11.f, G.rowH - 1.f))));
                        g.drawText (juce::String (noteName (r + kv.shift)) + "  " + ksName (*kv.lib, r + kv.shift, -1), juce::Rectangle<float> (G.notes.getX() + 4.f, y, 260.f, G.rowH), juce::Justification::centredLeft, true);
                    }
                }
            const auto& evs = ksEvents();
            for (auto& ev : evs)
            {
                auto r = ksRect (G, kv, ev);
                if (r.isEmpty() || r.getRight() < G.notes.getX() || r.getX() > G.notes.getRight()) continue;
                const bool dragging = drag == Drag::KsMove && ev.noteId == ksDragNote;
                g.setColour (kc.withAlpha (ev.noteId >= 0 ? 0.62f : 0.35f)); g.fillRoundedRectangle (r, 2.f);
                g.setColour (kc.brighter (0.4f)); g.drawRoundedRectangle (r, 2.f, 1.f);
                if (r.getWidth() > 44.f && G.rowH >= 8.f)
                { g.setColour (juce::Colours::black.withAlpha (0.85f)); g.setFont (juce::Font (juce::FontOptions (juce::jmin (10.5f, G.rowH - 1.f)))); g.drawText (ksName (*kv.lib, ev.key, ev.vel), r.reduced (3.f, 0.f), juce::Justification::centredLeft, true); }
                if (dragging) { g.setColour (juce::Colours::white); g.drawRoundedRectangle (r.withY (yOf (G, ksDragRow) + 0.5f), 2.f, 1.5f); }
            }
        }

        // notes: backing tracks first, solo on top
        for (int pass = 0; pass < 2; ++pass)
            for (auto& n : raw)
            {
                const bool solo = (n.track == 0);
                if ((pass == 0) == solo) continue;
                auto r = noteRect (G, n);
                if (r.getRight() < G.notes.getX() || r.getX() > G.notes.getRight()) continue;
                const bool sel = selected.count (n.id) > 0;
                juce::Colour col = n.track == 2 ? juce::Colour (0xff5a4c3d) : n.track == 1 ? juce::Colour (0xff5d86a0)
                                 : n.voice == kKsVoice ? juce::Colour (0xffffa040)
                                 : n.voice == 1 ? juce::Colour (0xff9a8666) : colourFor (n.art, T.accent);
                if (n.mute || n.keyPlayed)          // v3.12: a repeat played by its repetition-key note is drawn like a muted note
                {
                    g.setColour (col.withAlpha (0.18f)); g.fillRoundedRectangle (r, 2.5f);
                    g.setColour (juce::Colour (0xff8d8678)); g.drawRoundedRectangle (r, 2.5f, sel ? 1.8f : 1.f);
                    g.drawLine (r.getX(), r.getBottom(), r.getRight(), r.getY(), 0.8f);
                    continue;
                }
                const float a = 0.55f + 0.45f * (float) n.vel / 127.f;
                const float rad = juce::jmin (3.5f, r.getHeight() * 0.35f);
                const auto base = sel ? col.brighter (0.45f) : col.withAlpha (n.track == 2 ? 0.55f : a);
                if (sel) { juce::Path gp; gp.addRoundedRectangle (r, rad); juce::DropShadow (col.withAlpha (0.8f), 8, {}).drawForPath (g, gp); }
                g.setGradientFill (juce::ColourGradient (base.brighter (0.22f), 0.f, r.getY(), base.darker (0.18f), 0.f, r.getBottom(), false));
                g.fillRoundedRectangle (r, rad);
                g.setColour (juce::Colours::white.withAlpha (sel ? 0.5f : 0.28f)); g.fillRect (r.getX() + rad, r.getY() + 0.5f, juce::jmax (0.f, r.getWidth() - 2.f * rad), 1.f);
                g.setColour (sel ? juce::Colours::white : juce::Colours::black.withAlpha (0.45f)); g.drawRoundedRectangle (r, rad, sel ? 1.4f : 0.8f);
                if (n.art == A_Bend || n.art == A_Fall || n.art == A_Doit || n.art == A_Scoop)
                {
                    g.setColour (juce::Colours::white.withAlpha (0.85f));
                    const float cy = r.getCentreY(); juce::Path pp; pp.startNewSubPath (r.getX() + r.getWidth() * 0.5f, cy);
                    pp.lineTo (r.getRight(), n.art == A_Fall ? cy + G.rowH * 1.2f : cy - G.rowH * 1.2f);
                    g.strokePath (pp, juce::PathStrokeType (1.2f));
                }
                if (r.getWidth() > 34.f && G.rowH >= 10.f && n.track != 2)
                {
                    g.setColour (juce::Colours::black.withAlpha (0.78f)); g.setFont (LF::fnt (juce::jmin (11.f, G.rowH - 1.f), "SemiBold"));
                    g.drawText (juce::String (noteName (n.pitch)), r.reduced (3.f, 0.f), juce::Justification::centredLeft);
                }
            }
        if (raw.empty())
        {
            g.setColour (LF::dim()); g.setFont (juce::Font (juce::FontOptions (15.f)));
            g.drawText ("No notes yet - press TRY ANOTHER, or double-click here to draw one.", G.notes, juce::Justification::centred);
        }
        if (drag == Drag::Rubber)
        {
            g.setColour (T.accent.withAlpha (0.14f)); g.fillRect (rubber);
            g.setColour (T.accent); g.drawRect (rubber, 1.f);
        }
    }

    drawLane (g, G, T);

    // ---------- key-switch lane
    g.setColour (T.panel2); g.fillRoundedRectangle (G.ks, 3.f);
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (G.ks.getSmallestIntegerContainer());
        const auto& ks = artList (P.s.instrument);
        const Library* lib = artLib();
        int last = -1; g.setFont (juce::Font (juce::FontOptions (10.5f)));
        float lastX = -100;
        for (auto& e : P.playEvents)
            if (e.type == EV_ON && e.track == 0 && e.ord == 1 && (lib ? true : (e.a >= P.s.ksBase && e.a < P.s.ksBase + (int) ks.size())))
            {
                int art = -1;
                if (lib) { for (auto& la : lib->arts) if (libNoteOf (la) == e.a && ! la.oneShot) { art = la.art; break; } if (art < 0) continue; }
                else art = ks[(size_t) (e.a - P.s.ksBase)];
                if (art == last) continue;
                last = art;
                const float x = xOf (G, e.t);
                g.setColour (colourFor (art, T.accent)); g.fillRect (x, G.ks.getY() + 1, 2.f, G.ks.getHeight() - 2.f);
                if (x - lastX > 46.f) { g.setColour (LF::text().withAlpha (0.85f)); g.drawText (juce::String (artLabel (lib, art)), (int) x + 4, (int) G.ks.getY(), 120, 14, juce::Justification::centredLeft); lastX = x; }
            }
        if (P.s.exportMode != 0)
        {
            g.setColour (LF::dim()); g.setFont (juce::Font (juce::FontOptions (10.f)));
            g.drawText ("(key-switches are shown here for the test player; the exported MIDI follows your MIDI EXPORT choice)", G.ks.toNearestInt().withTrimmedLeft (4), juce::Justification::centredRight);
        }
    }

    // ---------- v3.8 loop selection: the looped beats are marked on the ruler and over the notes
    if (proc.loopRegionOn())
    {
        const float x0 = juce::jmax (G.notes.getX(), xOf (G, proc.loopRegionA.load())), x1 = juce::jmin (G.notes.getRight(), xOf (G, proc.loopRegionB.load()));
        if (x1 > x0)
        {
            g.setColour (T.accent.withAlpha (0.55f)); g.fillRect (x0, G.ruler.getY(), x1 - x0, G.ruler.getHeight());
            g.setColour (T.accent.withAlpha (0.06f)); g.fillRect (x0, G.notes.getY(), x1 - x0, G.notes.getHeight());
            g.setColour (T.accent.withAlpha (0.8f)); g.drawVerticalLine ((int) x0, G.ruler.getY(), G.notes.getBottom()); g.drawVerticalLine ((int) x1, G.ruler.getY(), G.notes.getBottom());
            g.setColour (juce::Colours::black); g.setFont (juce::Font (juce::FontOptions (10.f, juce::Font::bold)));
            g.drawText ("LOOP", juce::Rectangle<float> (x0 + 3.f, G.ruler.getY(), juce::jmax (0.f, x1 - x0 - 6.f), G.ruler.getHeight()), juce::Justification::centredLeft);
        }
    }

    // ---------- playhead
    if (lastBeat >= 0)
    {
        const float x = xOf (G, std::fmod (lastBeat, G.len));
        if (x >= G.notes.getX() && x <= G.notes.getRight())
        { g.setColour (LF::red()); g.fillRect (x - 1.f, G.ruler.getY(), 2.f, G.notes.getBottom() - G.ruler.getY()); }
    }
    if (hover.isNotEmpty())
    {
        const float w = 20.f + 7.f * (float) hover.length();
        auto hb = juce::Rectangle<float> (G.notes.getRight() - w - 6.f, G.notes.getY() + 4.f, w, 20.f);
        g.setColour (juce::Colours::black.withAlpha (0.7f)); g.fillRoundedRectangle (hb, 4.f);
        g.setColour (LF::text()); g.setFont (juce::Font (juce::FontOptions (13.f))); g.drawText (hover, hb, juce::Justification::centred);
    }
    if (! selected.empty())
    {
        g.setColour (juce::Colours::black.withAlpha (0.6f)); g.fillRoundedRectangle (G.notes.getX() + 4.f, G.notes.getBottom() - 22.f, 130.f, 18.f, 4.f);
        g.setColour (LF::text()); g.setFont (juce::Font (juce::FontOptions (12.f)));
        g.drawText (juce::String ((int) selected.size()) + " note" + (selected.size() == 1 ? "" : "s") + " selected", (int) G.notes.getX() + 10, (int) G.notes.getBottom() - 22, 124, 18, juce::Justification::centredLeft);
    }
    if (showKeys) drawShortcuts (g, G);
}

// v3.8: every keyboard shortcut of the piano roll (press ? to show / hide)
void RollView::drawShortcuts (juce::Graphics& g, const Geo& G)
{
    static const char* rows[][2] = {
        { "Up / Down", "Move the selected notes one step (scale-locked bars: next scale note)" },
        { "Shift + Up / Down", "Move the selected notes an octave" },
        { "Left / Right", "Select the previous / next note (Shift adds to the selection)" },
        { "Alt + Left / Right", "Move the selected notes in time by the grid" },
        { "Ctrl+A / Esc", "Select all / select none" },
        { "Ctrl+C / X / V / D", "Copy / cut / paste / duplicate" },
        { "Ctrl+Z / Ctrl+Y", "Undo / redo" },
        { "Delete", "Delete the selected notes" },
        { "Q", "Quantize the selected notes" },
        { "L", "Legato: each note runs to the next" },
        { "M", "Mute / unmute the selected notes" },
        { "H", "Humanize the selected notes (timing and velocity)" },
        { "Ctrl+L", "Loop the selection with PLAY (again = stop looping)" },
        { "1 2 3 4 5", "Tools: select, draw, erase, split, mute" },
        { "Mouse wheel", "Ruler: zoom time. Piano keys or Ctrl: zoom pitch" },
        { "Drag past an edge", "The view scrolls that way (select, move, resize)" },
        { "?", "Show / hide this list" } };
    const int n = (int) (sizeof (rows) / sizeof (rows[0]));
    (void) G;
    // the whole list fits the roll: one column when there is room, two when the roll is short
    const auto area = getLocalBounds().toFloat().reduced (10.f);
    const float lineH = 18.f;
    const int cols = (34.f + lineH * (float) n <= area.getHeight()) ? 1 : 2;
    const int perCol = (n + cols - 1) / cols;
    const float colW = cols == 1 ? 620.f : 560.f;
    const float w = juce::jmin (area.getWidth(), colW * (float) cols + 28.f), h = juce::jmin (area.getHeight(), 34.f + lineH * (float) perCol + 8.f);
    auto box = juce::Rectangle<float> (w, h).withCentre (area.getCentre());
    g.setColour (juce::Colours::black.withAlpha (0.88f)); g.fillRoundedRectangle (box, 8.f);
    g.setColour (LF::of (*this).accent.withAlpha (0.7f)); g.drawRoundedRectangle (box, 8.f, 1.2f);
    auto r = box.reduced (14.f, 8.f);
    g.setColour (LF::text()); g.setFont (juce::Font (juce::FontOptions (13.f, juce::Font::bold)));
    g.drawText ("KEYBOARD SHORTCUTS  (click anywhere or press ? to close)", r.removeFromTop (22.f), juce::Justification::centredLeft);
    const float cw = r.getWidth() / (float) cols;
    for (int i = 0; i < n; ++i)
    {
        const int c = i / perCol, k = i % perCol;
        auto line = juce::Rectangle<float> (r.getX() + cw * (float) c, r.getY() + lineH * (float) k, cw - 10.f, lineH);
        g.setFont (juce::Font (juce::FontOptions (12.f, juce::Font::bold))); g.setColour (LF::of (*this).accent);
        g.drawText (rows[i][0], line.removeFromLeft (140.f), juce::Justification::centredLeft);
        g.setFont (juce::Font (juce::FontOptions (12.f))); g.setColour (LF::text());
        g.drawText (rows[i][1], line, juce::Justification::centredLeft, true);
    }
}

void RollView::drawLane (juce::Graphics& g, const Geo& G, const EnochLookAndFeel::Theme& T)
{
    if (G.lane.getHeight() <= 0 || ! snap) return;
    {
        // the grip line above the lanes (drag it up / down)
        const float y = G.lane.getY() - 2.5f, on = (overDivider || drag == Drag::Divider) ? 1.f : 0.f;
        g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillRect (G.gutter.getX(), y - 1.f, G.lane.getRight() - G.gutter.getX(), 2.f);
        g.setColour (on > 0 ? T.accent : juce::Colours::white.withAlpha (0.10f)); g.fillRect (G.gutter.getX(), y, G.lane.getRight() - G.gutter.getX(), 1.f);
        const float cx = G.lane.getCentreX();
        g.setColour (on > 0 ? T.accent : juce::Colours::white.withAlpha (0.35f));
        for (int i = -2; i <= 2; ++i) g.fillEllipse (cx + (float) i * 8.f - 1.5f, y - 1.f, 3.f, 3.f);
    }
    const auto& P = snap->pattern;
    const double bb = std::max (0.5, P.barBeats);
    g.setGradientFill (juce::ColourGradient (T.bg.brighter (0.05f), 0.f, G.lane.getY(), T.bg.darker (0.2f), 0.f, G.lane.getBottom(), false));
    g.fillRoundedRectangle (G.lane, 6.f);
    juce::Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (G.lane.getSmallestIntegerContainer());
    for (int bar = 0; bar <= P.bars; ++bar)
    { g.setColour (juce::Colours::white.withAlpha (0.10f)); g.drawVerticalLine ((int) xOf (G, bar * bb), G.lane.getY(), G.lane.getBottom()); }
    for (int k = 1; k < 4; ++k)
    { g.setColour (juce::Colours::white.withAlpha (k == 2 ? 0.10f : 0.05f)); g.drawHorizontalLine ((int) (G.lane.getY() + G.lane.getHeight() * (float) k / 4.f), G.lane.getX(), G.lane.getRight()); }

    if (laneSel == 0)
    {
        for (auto& n : raw)
        {
            if (n.track == 2) continue;
            const float x = xOf (G, n.start);
            if (x < G.lane.getX() - 3 || x > G.lane.getRight()) continue;
            const float h = (G.lane.getHeight() - 4.f) * (float) n.vel / 127.f;
            const bool sel = selected.count (n.id) > 0;
            g.setColour (n.mute ? juce::Colour (0xff6b6458) : (sel ? juce::Colours::white : (n.track == 0 ? colourFor (n.art, T.accent) : juce::Colour (0xff5d86a0))));
            juce::ColourGradient stem (sel ? juce::Colours::white : (n.mute ? juce::Colour (0xff6b6458) : (n.track == 0 ? colourFor (n.art, T.accent) : juce::Colour (0xff5d86a0))),
                                       0.f, G.lane.getBottom() - 2.f - h, T.bg.withAlpha (0.2f), 0.f, G.lane.getBottom(), false);
            g.setGradientFill (stem);
            g.fillRoundedRectangle (x, G.lane.getBottom() - 2.f - h, 3.f, h, 1.5f);
            g.fillEllipse (x - 2.f, G.lane.getBottom() - 4.f - h, 7.f, 7.f);
        }
    }
    else
    {
        const int L = ctrlLane();
        // merged curve: automatic data + drawn data (drawn data replaces automatic data inside its regions)
        std::vector<std::pair<double, int>> curve;
        for (auto& e : P.playEvents)
        {
            if (e.track != 0) continue;
            if (L == L_Bend && e.type == EV_BEND) curve.push_back ({ e.t, e.a });
            else if (L == L_Mod && e.type == EV_CC && e.a == 1) curve.push_back ({ e.t, e.b });
            else if (L == L_Aftertouch && e.type == EV_AT) curve.push_back ({ e.t, e.a });
            else if (L == L_Expression && e.type == EV_CC && e.a == 11) curve.push_back ({ e.t, e.b });
            else if (L == L_Sustain && e.type == EV_CC && e.a == 64) curve.push_back ({ e.t, e.b });
        }
        for (auto& r : P.userRegions)
            if (r.lane == L)
            {
                const float x0 = xOf (G, r.t0), x1 = std::max (x0 + 2.f, xOf (G, r.t1));
                g.setColour (T.accent.withAlpha (0.10f)); g.fillRect (x0, G.lane.getY(), x1 - x0, G.lane.getHeight());
            }
        const int defV = (L == L_Bend) ? 8192 : (L == L_Expression ? 100 : 0);
        if (L == L_Bend) { g.setColour (juce::Colours::white.withAlpha (0.18f)); g.drawHorizontalLine ((int) yOfValue (G, 8192, L), G.lane.getX(), G.lane.getRight()); }
        juce::Path path; int cur = defV; float cx = G.lane.getX();
        path.startNewSubPath (cx, yOfValue (G, cur, L));
        for (auto& c : curve)
        {
            const float x = xOf (G, c.first);
            path.lineTo (x, yOfValue (G, cur, L)); path.lineTo (x, yOfValue (G, c.second, L)); cur = c.second;
        }
        path.lineTo (G.lane.getRight(), yOfValue (G, cur, L));
        g.setColour (T.accent.withAlpha (0.45f)); g.strokePath (path, juce::PathStrokeType (1.4f));
        for (auto& c : P.userPts)
            if (c.lane == L)
            {
                const float x = xOf (G, c.t), y = yOfValue (G, c.value, L);
                g.setColour (T.accent); g.fillEllipse (x - 2.5f, y - 2.5f, 5.f, 5.f);
            }
        // stroke in progress
        if (drag == Drag::LaneDraw && ! stroke.empty())
        {
            juce::Path sp; bool first = true;
            for (auto& c : stroke) { const float x = xOf (G, c.t), y = yOfValue (G, c.value, L); if (first) { sp.startNewSubPath (x, y); first = false; } else sp.lineTo (x, y); }
            g.setColour (juce::Colours::white); g.strokePath (sp, juce::PathStrokeType (2.f));
        }
        if (drag == Drag::LaneErase)
        {
            const float x0 = xOf (G, strokeT0), x1 = std::max (x0 + 2.f, xOf (G, strokeT1));
            g.setColour (LF::red().withAlpha (0.25f)); g.fillRect (x0, G.lane.getY(), x1 - x0, G.lane.getHeight());
        }
    }
    // lane caption
    g.setColour (LF::dim()); g.setFont (juce::Font (juce::FontOptions (10.f, juce::Font::bold)));
    juce::String cap = laneName (laneSel);
    if (laneSel == 1) cap << "  (+/-" << P.s.bendRange << " semitones, centre line = no bend)";
    if (laneSel >= 1) cap << "   -  drag to draw, Shift = straight line, right-drag or Alt-drag = erase, right-click = shapes";
    else cap << "   -  drag across notes to paint their velocity";
    g.drawText (cap, (int) G.lane.getX() + 4, (int) G.lane.getY() + 1, (int) G.lane.getWidth() - 8, 12, juce::Justification::centredLeft);
}

//------------------------------------------------------------------------------ mouse
bool RollView::nearDivider (const Geo& g, juce::Point<float> p) const
{
    if (g.lane.getHeight() <= 0) return false;
    const float y = g.lane.getY() - 2.5f;
    return std::abs (p.y - y) <= 4.5f && p.x >= g.gutter.getX() && p.x <= g.lane.getRight();
}

juce::MouseCursor RollView::getMouseCursor()
{
    if (overDivider || drag == Drag::Divider) return juce::MouseCursor::UpDownResizeCursor;
    if (hoverBar >= 0) return juce::MouseCursor::PointingHandCursor;
    if (overResize) return juce::MouseCursor::LeftRightResizeCursor;
    if (hoverPhrase >= 0) return juce::MouseCursor::PointingHandCursor;
    if (tool == Tool::Draw || tool == Tool::Split || tool == Tool::Erase || tool == Tool::Mute) return toolCursor (tool);
    return juce::MouseCursor::NormalCursor;
}

void RollView::mouseMove (const juce::MouseEvent& e)
{
    juce::String h; int hb = -1, hp = -1; bool rz = false;
    if (snap)
    {
        const auto g = geo(); const auto p = e.position;
        const bool dv = nearDivider (g, p);
        if (dv != overDivider) { overDivider = dv; updateMouseCursor(); repaint(); }
        if (dv) h = "Drag up / down: make the velocity and articulation lanes taller or shorter (more room for the notes). Double-click = reset";
        else if (g.phrase.contains (p))
        {
            hp = phraseAtX (g, p.x);
            h = tool == Tool::Split ? "Split tool: click = split the phrase here, on the grid (click a split again to join). Then right-click a part = Re-roll only this phrase"
              : hp >= 0 ? "Click = lock / unlock this phrase (a locked phrase is kept when you press Try another)   |   right-click = more (split, re-roll)" : "";
        }
        else if (g.chord.contains (p)) { hb = barAtX (g, p.x); h = "Click to select this bar and choose its chord (Shift-click / drag = several bars)"; }
        else if (g.ruler.contains (p)) h = "Click to jump the player here  |  double-click = play / stop  |  mouse wheel = zoom";
        else if (g.gutter.contains (p)) h = "Click a key to hear it  |  mouse wheel = zoom pitch";
        else if (g.notes.contains (p))
        {
            const int i = hitTest (g, p, rz);
            rz = rz || hitLeft;
            const auto kv = ksView();
            if (i >= 0)
            {
                auto& n = raw[(size_t) i];
                if (n.voice == kKsVoice && kv.on && kv.lib) h = "key-switch " + juce::String (noteName (n.pitch + kv.shift)) + "  " + ksName (*kv.lib, n.pitch + kv.shift, n.vel) + "   vel " + juce::String (n.vel) + "   (you drew it: it is sent as it is)";
                else
                {
                    h = juce::String (noteName (n.pitch)) + "   vel " + juce::String (n.vel) + "   " + juce::String (artLabel (artLib(), n.art)) + (n.mute ? "   (muted)" : "");
                    if (n.art == A_Bend) h << " (+" << n.bend << ")";
                    if (n.why > 0 && n.voice == 0 && n.track == 0) h << "   |   why: " << artbrain::whyText (n.why);     // v3.11: the planner's reason
                }
            }
            else if (const int k = ksHit (g, p); k >= 0 && kv.lib)
            {
                const auto& ev = ksEvents()[(size_t) k];
                h = "key-switch " + juce::String (noteName (ev.key)) + "  " + ksName (*kv.lib, ev.key, ev.vel) + (ev.noteId >= 0 ? "   right-click = change / remove   drag up / down = another articulation" : "   (strings / mode key)");
            }
            else if (kv.on && kv.lib && kv.row[(size_t) juce::jlimit (0, 127, pitchAt (g, p.y))])
                h = "key-switch row " + juce::String (noteName (pitchAt (g, p.y) + kv.shift)) + "  " + ksName (*kv.lib, pitchAt (g, p.y) + kv.shift, -1) + "   (draw here to add this key-switch by hand)";
            else h = tool == Tool::Draw ? "Click-drag to draw a note" : "Double-click to add a note";
        }
        else if (g.lane.contains (p) && ctrlLane() >= 0)
        {
            const int L = ctrlLane(); const int v = valueAtY (g, p.y, L);
            h = juce::String (laneName (laneSel)).upToFirstOccurrenceOf (" (", false, false) + ": ";
            if (L == L_Bend) h << juce::String ((v - 8192) / 8192.0 * snap->pattern.s.bendRange, 2) << " st"; else h << v;
        }
    }
    if (h != hover || hb != hoverBar || rz != overResize || hp != hoverPhrase) { hover = h; hoverBar = hb; hoverPhrase = hp; overResize = rz; updateMouseCursor(); repaint(); }
}

void RollView::mouseExit (const juce::MouseEvent&) { hover = {}; hoverBar = -1; hoverPhrase = -1; overResize = false; overDivider = false; repaint(); }

int RollView::phraseAtX (const Geo& g, float x) const
{
    const double b = beatAt (g, x);
    int best = -1; double bd = 1e18;
    for (size_t i = 0; i < phrases.size(); ++i)
    {
        const auto& ph = phrases[i];
        if (b >= ph.t0 - 0.02 && b <= ph.t1 + 0.02) return (int) i;               // inside a phrase block
        const double d = b < ph.t0 ? ph.t0 - b : b - ph.t1;
        if (b >= ph.r0 && b < ph.r1 && d < bd) { bd = d; best = (int) i; }         // in a rest next to it: the nearest block is still easy to hit
    }
    return best;
}

double RollView::phraseCutBeat (double beat) const
{
    const double gb = gridBeats() > 0 ? gridBeats() : 0.25;
    return std::max (0.0, std::round (beat / gb) * gb);
}

void RollView::showPhraseMenu (int index)
{
    if (index < 0 || index >= (int) phrases.size()) return;
    const auto ph = phrases[(size_t) index];
    const bool locked = phraseLocked (proc.getPhraseLocks(), ph);
    juce::PopupMenu m;
    m.addItem (1, locked ? "Unlock this phrase" : "Lock this phrase");
    m.addItem (2, "Re-roll only this phrase");
    // v3.14: split it at the grid where you clicked, or join it with the next part if you split it there before
    const double at = phraseMenuBeat >= 0 ? phraseCutBeat (phraseMenuBeat) : -1.0;
    const bool canSplit = at > ph.t0 + 1e-6 && at < ph.t1 - 1e-6;
    m.addItem (5, "Split this phrase here (at the grid)", canSplit);
    double joinAt = -1.0;
    for (double c : proc.getPhraseCuts()) if (std::abs (c - ph.r1) < 1e-3) joinAt = c;
    m.addItem (6, "Join with the next phrase (undo my split)", joinAt >= 0);
    m.addItem (7, "Remove all my phrase splits", ! proc.getPhraseCuts().empty());
    m.addSeparator();
    m.addItem (3, "Lock all phrases");
    m.addItem (4, "Unlock all phrases", proc.numLockedPhrases() > 0);
    m.showMenuAsync (juce::PopupMenu::Options(), [this, ph, locked, at, joinAt] (int r)
    {
        if (r == 5) { proc.togglePhraseCut (at); }
        else if (r == 6) { proc.togglePhraseCut (joinAt); }
        else if (r == 7) { proc.clearPhraseCuts(); }
        else if (r == 1) proc.setPhraseLocked (ph, ! locked);
        else if (r == 2) proc.rerollPhrase (ph);
        else if (r == 3) proc.setAllPhrasesLocked (true);
        else if (r == 4) proc.setAllPhrasesLocked (false);
        else return;
        refresh (true); repaint();
        if (onPhrasesChanged) onPhrasesChanged();
    });
}

void RollView::eraseAt (const Geo& g, juce::Point<float> p)
{
    bool rz; const int i = hitTest (g, p, rz);
    if (i < 0) return;
    selected.erase (raw[(size_t) i].id);
    raw.erase (raw.begin() + i); moved = true; repaint();
}

void RollView::muteAt (const Geo& g, juce::Point<float> p)
{
    bool rz; const int i = hitTest (g, p, rz);
    if (i < 0) return;
    raw[(size_t) i].mute = muteTarget; moved = true; repaint();
}

void RollView::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    if (! snap) return;
    const auto g = geo(); const auto p = e.position;
    if (showKeys) { setShowShortcuts (false); return; }     // v3.8: a click closes the shortcut list
    downPos = p; moved = false; duplicated = false; drag = Drag::None; rightDownInLane = false;

    // ---------------- the line above the velocity lane: drag to resize the lanes
    if (nearDivider (g, p) && ! e.mods.isPopupMenu())
    {
        if (e.getNumberOfClicks() >= 2) { laneHpx = kLaneH; if (onLaneHeightChanged) onLaneHeightChanged (laneHpx); repaint(); return; }
        laneHAtDown = g.lane.getHeight(); drag = Drag::Divider; updateMouseCursor(); return;
    }

    // ---------------- phrase strip: click locks / unlocks, right-click opens the phrase menu
    if (g.phrase.contains (p))
    {
        // v3.14: the split tool cuts the phrase at the grid line nearest the click (on a cut you made: joins the two again)
        if (tool == Tool::Split && ! e.mods.isPopupMenu()) { splitPhraseAt (beatAt (g, p.x)); return; }
        const int i = phraseAtX (g, p.x);
        if (i < 0) return;
        if (e.mods.isPopupMenu()) { phraseMenuBeat = beatAt (g, p.x); showPhraseMenu (i); return; }
        const auto ph = phrases[(size_t) i];
        proc.setPhraseLocked (ph, ! phraseLocked (proc.getPhraseLocks(), ph));
        repaint(); if (onPhrasesChanged) onPhrasesChanged();
        return;
    }

    // ---------------- chord cells: click selects the bar(s) and opens the chord menu
    if (g.chord.contains (p))
    {
        focusChords = true;
        const int bar = barAtX (g, p.x);
        if (e.mods.isPopupMenu())
        {
            if (barSelFirst() < 0 || bar < barSelFirst() || bar > barSelLast()) { barA = barB = bar; notifyBars(); repaint(); }
            showChordMenu (bar, cellRect (g, bar));
            return;
        }
        if (e.mods.isShiftDown() && barA >= 0) { barB = bar; openMenuOnUp = false; }
        else { barA = barB = bar; openMenuOnUp = false; }          // left click only selects; right click opens the chord menu
        barDragStart = bar; drag = Drag::BarSel; notifyBars(); repaint();
        return;
    }
    focusChords = false;

    if (g.ruler.contains (p))
    {
        drag = Drag::Seek; proc.seekInternal (juce::jlimit (0.0, g.len, beatAt (g, p.x)));
        return;
    }
    if (g.gutter.contains (p)) { lastAuditioned = pitchAt (g, p.y); proc.audition (lastAuditioned, 100, true); drag = Drag::Key; return; }

    // ---------------- lane below the notes
    if (g.lane.getHeight() > 0 && g.lane.contains (p))
    {
        if (laneSel == 0)
        {
            if (e.mods.isPopupMenu()) { showLaneMenu (e.getScreenPosition(), beatAt (g, p.x)); return; }
            dragOrig = raw; drag = Drag::Velocity; lastLaneX = p.x;
            setVelFromY (g, p.x, p.x, p.y); repaint();
            return;
        }
        const int L = ctrlLane();
        if (e.mods.isPopupMenu() || e.mods.isAltDown())
        {
            rightDownInLane = e.mods.isPopupMenu();
            drag = Drag::LaneErase; strokeT0 = strokeT1 = juce::jlimit (0.0, g.len, beatAt (g, p.x));
            return;
        }
        drag = Drag::LaneDraw; stroke.clear(); strokeLine = e.mods.isShiftDown(); strokeStart = p;
        const double t = juce::jlimit (0.0, g.len, beatAt (g, p.x));
        strokeT0 = strokeT1 = t;
        CtrlPoint c; c.t = t; c.lane = L; c.value = valueAtY (g, p.y, L); stroke.push_back (c);
        repaint();
        return;
    }

    // ---------------- note area
    if (g.notes.contains (p))
    {
        bool rz = false; const int idx = hitTest (g, p, rz);
        pasteAt = std::max (0.0, snapBeat (beatAt (g, p.x), true));
        // v3.9: a key-switch Enoch wrote for a note: right-click = change / remove it, drag up or down = another articulation
        if (idx < 0 && tool != Tool::Draw)
        {
            const int k = ksHit (g, p);
            if (k >= 0)
            {
                const auto ev = ksEvents()[(size_t) k];
                if (e.mods.isPopupMenu()) { showKsMenu (e.getScreenPosition(), ev.noteId); return; }
                // v3.12: the key-switches become notes you can move, stretch, delete and re-velocity; this one is grabbed straight away
                editKeySwitchesAsNotes();
                if (drag == Drag::None) { mouseDown (e); return; }
                return;
            }
        }
        if (e.mods.isPopupMenu())
        {
            if (idx >= 0 && ! selected.count (raw[(size_t) idx].id)) selected = { raw[(size_t) idx].id };
            showNoteMenu (e.getScreenPosition(), idx >= 0, beatAt (g, p.x), pitchAt (g, p.y));
            return;
        }
        if (tool == Tool::Erase) { dragOrig = raw; drag = Drag::EraseNotes; eraseAt (g, p); return; }
        if (tool == Tool::Mute && idx >= 0) { dragOrig = raw; muteTarget = ! raw[(size_t) idx].mute; drag = Drag::MuteNotes; muteAt (g, p); return; }
        if (tool == Tool::Split && idx >= 0)
        {
            const Note n = raw[(size_t) idx];
            double at = snapBeat (beatAt (g, p.x));
            if (at <= n.start + 0.02 || at >= n.start + n.length - 0.02) at = n.start + n.length * 0.5;
            auto v = raw; Note a = n, b = n;
            a.length = at - n.start; b.start = at; b.length = n.start + n.length - at; b.id = proc.allocNoteId(); b.weight = metricWeight (b.start);
            if (b.art == A_Bend || b.art == A_Fall || b.art == A_Doit || b.art == A_Scoop || b.art == A_Trill || b.art == A_Shake) b.art = A_Sustain;
            v[(size_t) idx] = a; v.push_back (b);
            selected = { a.id, b.id };
            commit (std::move (v));
            return;
        }
        if (idx >= 0)
        {
            const int id = raw[(size_t) idx].id;
            if (e.mods.isShiftDown() || e.mods.isCommandDown())
            {
                if (selected.count (id)) selected.erase (id); else selected.insert (id);
                if (! selected.count (id)) { repaint(); return; }
            }
            else if (! selected.count (id)) selected = { id };
            anchorId = id;
            if (e.mods.isAltDown() && ! rz)      // alt + drag = drag a copy
            {
                std::set<int> newSel; int newAnchor = id; std::vector<Note> add;
                for (auto& n : raw) if (selected.count (n.id)) { Note c = n; c.id = proc.allocNoteId(); if (n.id == id) newAnchor = c.id; newSel.insert (c.id); add.push_back (c); }
                raw.insert (raw.end(), add.begin(), add.end());
                selected = newSel; anchorId = newAnchor; duplicated = true;
            }
            dragOrig = raw; downBeat = beatAt (g, p.x); downPitch = pitchAt (g, p.y);
            drag = rz ? Drag::Resize : (hitLeft && ! e.mods.isAltDown() ? Drag::ResizeStart : Drag::Move);
            for (auto& n : raw) if (n.id == anchorId) { proc.audition (n.pitch, n.vel, true); lastAuditioned = n.pitch; }
        }
        else if (tool == Tool::Draw)
        {
            dragOrig = raw;
            Note n;
            const double len = std::max (0.25, gridBeats());
            n.start = juce::jlimit (0.0, std::max (0.0, g.len - len), snapBeat (beatAt (g, p.x), true));
            n.length = len; n.pitch = snapPitch (pitchAt (g, p.y), n.start);
            n.vel = 96; n.art = A_Sustain; n.track = 0; n.voice = 0; n.id = proc.allocNoteId(); n.weight = metricWeight (n.start);
            { const auto kv = ksView(); const int rp = juce::jlimit (0, 127, pitchAt (g, p.y)); if (kv.on && kv.row[(size_t) rp]) { n.pitch = rp; std::vector<Note> one { n }; markKsNotes (one, { n.id }); n = one[0]; } }
            raw.push_back (n); selected = { n.id }; anchorId = n.id; downBeat = n.start; moved = true; drag = Drag::DrawLen;
            proc.audition (n.pitch, n.vel, true);
        }
        else
        {
            const bool add = e.mods.isShiftDown() || e.mods.isCommandDown();
            selBeforeRubber = add ? selected : std::set<int>();
            if (! add) selected.clear();
            rubber = { p, p }; drag = Drag::Rubber;
            rubberBeat = beatAt (g, p.x); rubberPitchF = (float) g.hi + 1.f - (p.y - g.notes.getY()) / g.rowH;
        }
        repaint();
    }
}

void RollView::setVelFromY (const Geo& g, float x0, float x1, float y)
{
    const int v = juce::jlimit (1, 127, juce::roundToInt ((g.lane.getBottom() - 2.f - y) / (g.lane.getHeight() - 4.f) * 127.f));
    const float lo = std::min (x0, x1) - 3.f, hi = std::max (x0, x1) + 3.f;
    for (auto& n : raw)
    {
        if (n.track == 2) continue;
        const float x = xOf (g, n.start) + 1.5f;
        if (x >= lo && x <= hi && n.vel != v) { n.vel = v; moved = true; }
    }
}

void RollView::mouseDrag (const juce::MouseEvent& e)
{
    if (! snap || drag == Drag::None) return;
    if (drag == Drag::Rubber || drag == Drag::Move || drag == Drag::Resize || drag == Drag::ResizeStart || drag == Drag::DrawLen) autoScrollFor (e.position);   // v3.8
    const auto g = geo(); const auto p = e.position;

    if (drag == Drag::Divider)
    {
        laneHpx = juce::jlimit (28.f, 900.f, laneHAtDown + (downPos.y - p.y));
        const float shown = geo().lane.getHeight(); laneHpx = shown;               // stays within what fits
        if (onLaneHeightChanged) onLaneHeightChanged (laneHpx);
        repaint(); return;
    }
    if (drag == Drag::Seek) { proc.seekInternal (juce::jlimit (0.0, g.len, beatAt (g, p.x))); return; }
    if (drag == Drag::Key)                                   // slide over the piano keys: each key sounds as the pointer reaches it
    {
        const int kp = pitchAt (g, p.y);
        if (kp != lastAuditioned) { lastAuditioned = kp; proc.audition (kp, 100, true); }
        return;
    }
    if (drag == Drag::BarSel)
    {
        const int bar = barAtX (g, p.x);
        if (bar != barB) { barB = bar; notifyBars(); repaint(); }
        if (bar != barDragStart) openMenuOnUp = false;
        return;
    }
    if (drag == Drag::Velocity) { setVelFromY (g, lastLaneX, p.x, p.y); lastLaneX = p.x; repaint(); return; }
    if (drag == Drag::KsMove) { const int r = pitchAt (g, p.y); if (r != ksDragRow) { ksDragRow = r; repaint(); } return; }   // v3.9
    if (drag == Drag::EraseNotes) { eraseAt (g, p); return; }
    if (drag == Drag::MuteNotes) { muteAt (g, p); return; }
    if (drag == Drag::LaneErase)
    {
        const double t = juce::jlimit (0.0, g.len, beatAt (g, p.x));
        if (p.getDistanceFrom (downPos) > 4.f) moved = true;
        strokeT0 = std::min (strokeT0, t); strokeT1 = std::max (strokeT1, t); repaint(); return;
    }
    if (drag == Drag::LaneDraw)
    {
        const int L = ctrlLane(); const double t = juce::jlimit (0.0, g.len, beatAt (g, p.x));
        CtrlPoint c; c.t = t; c.lane = L; c.value = valueAtY (g, p.y, L);
        if (strokeLine) { if (stroke.size() > 1) stroke.resize (1); stroke.push_back (c); }
        else if (stroke.empty() || std::abs (t - stroke.back().t) >= ctrlStep (g) * 0.5 || c.value != stroke.back().value) stroke.push_back (c);
        strokeT0 = std::min (strokeT0, t); strokeT1 = std::max (strokeT1, t);
        moved = true; repaint(); return;
    }
    if (drag == Drag::Rubber)
    {
        // v3.8: the corner where the drag started stays on the same beat and pitch while the view scrolls
        const float ax = xOf (g, rubberBeat), ay = g.notes.getY() + ((float) g.hi + 1.f - rubberPitchF) * g.rowH;
        const float px = juce::jlimit (g.notes.getX(), g.notes.getRight(), p.x), py = juce::jlimit (g.notes.getY(), g.notes.getBottom(), p.y);
        rubber = juce::Rectangle<float>::leftTopRightBottom (std::min (ax, px), std::min (ay, py), std::max (ax, px), std::max (ay, py));
        selected = selBeforeRubber;
        for (auto& n : raw) if (n.track != 2 && noteRect (g, n).intersects (rubber)) selected.insert (n.id);
        repaint(); return;
    }

    if (drag == Drag::DrawLen)
    {
        const double minLen = snapIsOn() && gridBeats() > 0 ? gridBeats() : 1.0 / 32.0;
        for (auto& n : raw) if (n.id == anchorId) n.length = std::max (minLen, std::min (snapBeat (beatAt (g, p.x)) - n.start, g.len - n.start));
        repaint(); return;
    }

    if (! moved && p.getDistanceFrom (downPos) < 3.f) return;
    moved = true;
    const double total = g.len;
    auto origOf = [&] (int id) -> const Note* { for (auto& n : dragOrig) if (n.id == id) return &n; return nullptr; };
    const Note* a = origOf (anchorId);
    if (! a) return;

    if (drag == Drag::Move)
    {
        const double newStart = snapBeat (a->start + (beatAt (g, p.x) - downBeat));
        double dBeat = newStart - a->start;
        const int dPitch = pitchAt (g, p.y) - downPitch;
        double minS = 1e9, maxE = 0;
        for (auto& n : dragOrig) if (selected.count (n.id)) { minS = std::min (minS, n.start); maxE = std::max (maxE, n.start + n.length); }
        dBeat = std::max (dBeat, -minS);
        if (maxE + dBeat > total) dBeat = std::max (-minS, total - maxE);
        for (size_t i = 0; i < raw.size() && i < dragOrig.size(); ++i)
            if (selected.count (dragOrig[i].id))
            {
                raw[i] = dragOrig[i];
                raw[i].start = dragOrig[i].start + dBeat;
                { const int want = juce::jlimit (0, 127, dragOrig[i].pitch + dPitch); const auto kv = ksView();
                  raw[i].pitch = (kv.on && kv.row[(size_t) want]) ? want : pitchFor (want, raw[i].start, dPitch > 0 ? 1 : (dPitch < 0 ? -1 : 0)); }
                if (dPitch != 0 || dBeat != 0) raw[i].weight = metricWeight (raw[i].start);
            }
        const int ap = juce::jlimit (0, 127, a->pitch + dPitch);
        if (ap != lastAuditioned && dPitch != 0) { lastAuditioned = ap; proc.audition (snapPitch (ap, a->start + dBeat), a->vel, true); }
    }
    else if (drag == Drag::ResizeStart)
    {
        // v3.9: the start edge: the note begins earlier or later, its end stays where it is
        const double minLen = snapIsOn() && gridBeats() > 0 ? gridBeats() : 1.0 / 32.0;
        const double dStart = snapBeat (a->start + (beatAt (g, p.x) - downBeat)) - a->start;
        for (size_t i = 0; i < raw.size() && i < dragOrig.size(); ++i)
            if (selected.count (dragOrig[i].id))
            {
                raw[i] = dragOrig[i];
                const double end = dragOrig[i].start + dragOrig[i].length;
                const double ns = juce::jlimit (0.0, end - minLen, dragOrig[i].start + dStart);
                raw[i].start = ns; raw[i].length = end - ns; raw[i].weight = metricWeight (ns);
            }
    }
    else if (drag == Drag::Resize)
    {
        const double endA = a->start + a->length;
        const double dLen = snapBeat (endA + (beatAt (g, p.x) - downBeat)) - endA;
        const double minLen = snapIsOn() && gridBeats() > 0 ? gridBeats() : 1.0 / 32.0;
        for (size_t i = 0; i < raw.size() && i < dragOrig.size(); ++i)
            if (selected.count (dragOrig[i].id))
            {
                raw[i] = dragOrig[i];
                raw[i].length = std::max (minLen, std::min (dragOrig[i].length + dLen, total - dragOrig[i].start));
            }
    }
    repaint();
}

void RollView::mouseUp (const juce::MouseEvent& e)
{
    const Drag d = drag; drag = Drag::None;
    proc.auditionRelease();
    const auto g = geo();
    switch (d)
    {
        case Drag::Rubber: rubber = {}; repaint(); return;
        case Drag::Seek: case Drag::None: case Drag::Key: return;
        case Drag::Divider: updateMouseCursor(); return;
        case Drag::BarSel:
            if (openMenuOnUp && barDragStart >= 0) showChordMenu (barDragStart, cellRect (g, barDragStart));
            return;
        case Drag::LaneErase:
        {
            if (rightDownInLane && ! moved) { showLaneMenu (e.getScreenPosition(), strokeT0); return; }
            const int L = ctrlLane();
            if (L >= 0) { proc.clearCtrl (L, strokeT0 - 1e-6, strokeT1 + 1e-6); refresh (true); }
            return;
        }
        case Drag::LaneDraw:
        {
            const int L = ctrlLane(); if (L < 0 || stroke.empty()) return;
            std::vector<CtrlPoint> pts;
            if (strokeLine && stroke.size() >= 2)
            {
                const auto a = stroke.front(), b = stroke.back();
                const double t0 = std::min (a.t, b.t), t1 = std::max (a.t, b.t);
                const double step = std::max (ctrlStep (g), (t1 - t0) / 400.0);
                const CtrlPoint& lo = a.t <= b.t ? a : b; const CtrlPoint& hi = a.t <= b.t ? b : a;
                for (double t = t0; t < t1 + 1e-9; t += step)
                {
                    const double u = t1 > t0 ? (std::min (t, t1) - t0) / (t1 - t0) : 0.0;
                    CtrlPoint c; c.t = std::min (t, t1); c.lane = L; c.value = (int) std::lround (lo.value + (hi.value - lo.value) * u); pts.push_back (c);
                    if (t >= t1) break;
                }
                if (pts.back().t < t1 - 1e-9) { CtrlPoint c = hi; pts.push_back (c); }
            }
            else
            {
                pts = stroke;
                std::stable_sort (pts.begin(), pts.end(), [] (const CtrlPoint& x, const CtrlPoint& y) { return x.t < y.t; });
                std::vector<CtrlPoint> dd;
                for (auto& c : pts) { if (! dd.empty() && std::abs (dd.back().t - c.t) < 1e-6) dd.back() = c; else dd.push_back (c); }
                pts = dd;
            }
            const double t0 = pts.front().t, t1 = pts.back().t;
            stroke.clear();
            proc.commitCtrl (L, t0, t1, pts);
            refresh (true);
            return;
        }
        case Drag::KsMove:
        {
            // dropped on another key of the instrument: the note takes that key's articulation
            const auto kv = ksView(); const int note = ksDragNote; ksDragNote = -1;
            if (kv.on && kv.lib != nullptr && note >= 0)
            {
                const int key = ksDragRow + kv.shift;
                for (auto& la : kv.lib->arts)
                    if (! la.press.empty() && la.press[0].note == key) { setArtOnNote (note, la.art); return; }
            }
            repaint(); return;
        }
        case Drag::Move: case Drag::Resize: case Drag::ResizeStart: case Drag::Velocity: case Drag::DrawLen: case Drag::EraseNotes: case Drag::MuteNotes:
            if (moved && d == Drag::Move) { std::vector<Note> v = raw; markKsNotes (v, selected); raw = v; }   // v3.9: notes moved onto / off key rows
            if (moved) commit (raw);
            else { raw = dragOrig; repaint(); }
            break;
    }
    duplicated = false; lastAuditioned = -1;
}

void RollView::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (snap && nearDivider (geo(), e.position)) return;
    if (! snap) return;
    const auto g = geo();
    if (g.ruler.contains (e.position)) { proc.setInternalPlaying (! proc.isInternalPlaying()); return; }   // v3.9: double-click the ruler = play / stop
    if (g.lane.getHeight() > 0 && g.lane.contains (e.position) && ctrlLane() >= 0)
    {
        // double-click a lane: give the bar under the mouse (or the selected bars) back to the automatic data
        const double bb = std::max (0.5, snap->pattern.barBeats);
        const int b0 = barSelFirst() >= 0 ? barSelFirst() : barAtX (g, e.position.x), b1 = barSelFirst() >= 0 ? barSelLast() : b0;
        proc.clearCtrl (ctrlLane(), b0 * bb - 1e-6, (b1 + 1) * bb);
        refresh (true);
        return;
    }
    if (! g.notes.contains (e.position) || tool == Tool::Erase || tool == Tool::Mute || tool == Tool::Split) return;
    bool rz; const int hit = hitTest (g, e.position, rz);
    if (hit >= 0)
    {
        if (tool == Tool::Select)            // double-click a note with the Select tool = delete it
        {
            const int id = raw[(size_t) hit].id;
            drag = Drag::None; moved = false; duplicated = false;
            auto v = raw; v.erase (std::remove_if (v.begin(), v.end(), [id] (const Note& n) { return n.id == id; }), v.end());
            selected.erase (id);
            commit (std::move (v));
        }
        return;
    }
    addNoteAt (beatAt (g, e.position.x), pitchAt (g, e.position.y));
}

void RollView::addNoteAt (double beat, int pitch)
{
    if (! snap) return;
    Note n;
    const double len = std::max (0.25, gridBeats());
    n.start = juce::jlimit (0.0, std::max (0.0, totalBeats() - len), snapBeat (beat, true));
    n.length = len;
    n.pitch = pitchFor (pitch, n.start, 0);
    n.vel = 96; n.art = A_Sustain; n.track = 0; n.voice = 0; n.id = proc.allocNoteId(); n.weight = metricWeight (n.start);
    { const auto kv = ksView(); const int rp = juce::jlimit (0, 127, pitch); if (kv.on && kv.row[(size_t) rp]) n.pitch = rp; }
    auto v = raw; v.push_back (n);
    markKsNotes (v, { n.id });
    selected = { n.id };
    proc.audition (n.pitch, n.vel);
    commit (std::move (v));
}

void RollView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (! snap) return;
    const auto g = geo();
    const double dy = std::abs (w.deltaY) > 1e-6 ? w.deltaY : w.deltaX;
    const bool overRuler = g.ruler.contains (e.position) || g.chord.contains (e.position);
    const bool overGutter = g.gutter.contains (e.position);
    if (overRuler || e.mods.isAltDown())                                      // wheel on the bar-time ruler (or Alt+wheel): zoom in time
        zoomAround (std::exp (-dy * 1.6), beatAt (g, e.position.x));
    else if (overGutter || e.mods.isCtrlDown() || e.mods.isCommandDown())     // wheel on the piano keys (or Ctrl+wheel): make the roll taller / shorter (pitch zoom)
    {
        const double anchor = (double) pitchAt (g, e.position.y);
        const int nr = juce::jlimit (12, 128, (int) std::lround (visRows * std::exp (-dy * 1.6)));
        const double frac = (topPitch - anchor) / (double) visRows;
        topPitch = (int) std::lround (anchor + frac * nr); visRows = nr; vAuto = false;
        clampView(); updateScrollBars(); repaint(); notifyView();
    }
    else if (e.mods.isShiftDown())                                            // Shift+wheel: scroll in time (sideways)
    {
        if (viewLen > 0 && viewLen < g.len) { viewStart -= dy * viewLen * 0.6; clampView(); updateScrollBars(); repaint(); notifyView(); }
    }
    else                                                                      // wheel: scroll the roll up and down (pitch)
    {
        wheelAccum += dy * 8.0;
        const int step = (int) wheelAccum;
        if (step != 0)
        {
            wheelAccum -= step;
            topPitch += step; vAuto = false; clampView(); updateScrollBars(); repaint(); notifyView();
        }
    }
}

//------------------------------------------------------------------------------ selection / chord helpers
static juce::String scaleLabelOf (const ChordSpan& c)
{
    for (int m = 0; m < M_Count; ++m)
        if (c.scaleMask == modeMask (c.rootPc, m)) return juce::String (kNoteNames[c.rootPc % 12]) + " " + modeName (m);
    return juce::String();
}

juce::String RollView::barSelText() const
{
    if (barA < 0 || ! snap) return "No bar selected - click a chord above the roll";
    const int a = barSelFirst(), b = barSelLast();
    const double bb = std::max (0.5, snap->pattern.barBeats);
    juce::String t = a == b ? "BAR " + juce::String (a + 1) : "BARS " + juce::String (a + 1) + "-" + juce::String (b + 1);
    if (! snap->pattern.chords.empty())
    {
        t << "  (" << juce::String (snap->pattern.chordAt (a * bb + 0.01).name);
        if (b > a) t << " ... " << juce::String (snap->pattern.chordAt (b * bb + 0.01).name);
        t << ")";
        const auto lab = scaleLabelOf (snap->pattern.chordAt (a * bb + 0.01));
        if (lab.isNotEmpty() && a == b) t << "   scale: " << lab;
    }
    return t;
}

void RollView::clearBarSelection() { barA = barB = -1; notifyBars(); repaint(); }

float RollView::busyOfSelection() const
{
    const int a = barSelFirst();
    const auto& v = proc.getBarBusy();
    if (a >= 0 && a < (int) v.size() && v[(size_t) a] >= 0.f) return v[(size_t) a];
    return proc.apvts.getRawParameterValue ("busy")->load();
}

bool RollView::selectionHasExplicitBusy() const
{
    const int a = barSelFirst(), b = barSelLast();
    const auto& v = proc.getBarBusy();
    for (int i = std::max (0, a); a >= 0 && i <= b && i < (int) v.size(); ++i) if (v[(size_t) i] >= 0.f) return true;
    return false;
}

void RollView::applyBusyToSelection (float busy, uint64_t seed)
{
    const int a = barSelFirst(), b = barSelLast();
    if (a < 0) return;
    proc.applyBusy (a, b, busy, seed);
    selected.clear();
    refresh (true);
}

void RollView::clearAllNotes()
{
    if (raw.empty()) return;
    selected.clear(); commit ({});
}

void RollView::clearSelectedBarNotes()
{
    const int a = barSelFirst(), b = barSelLast();
    if (a < 0) return;
    const double bb = snap ? std::max (0.5, snap->pattern.barBeats) : 4.0;
    const double t0 = a * bb, t1 = (b + 1) * bb;
    std::vector<Note> v;
    for (auto& n : raw)
    {
        if (n.start >= t0 - 1e-6 && n.start < t1 - 1e-6) continue;              // starts in the bars: gone
        Note k = n;
        if (k.start < t0 && k.start + k.length > t0) k.length = std::max (0.05, t0 - k.start);   // a note held into the bars stops at the bar line
        v.push_back (k);
    }
    if (v.size() == raw.size()) return;
    selected.clear(); commit (std::move (v));
}

void RollView::copyBarPlay (int srcBar, int bar0, int bar1)
{
    auto snap = proc.getSnapshot();
    if (snap == nullptr || srcBar < 0) return;
    const double bb = snap->pattern.barBeats;
    const int bars = snap->pattern.bars;
    bar0 = juce::jlimit (0, bars - 1, bar0); bar1 = juce::jlimit (bar0, bars - 1, bar1);
    if (srcBar >= bars) return;
    const double a = bar0 * bb, b = (bar1 + 1) * bb, sa = srcBar * bb, sb = sa + bb;
    auto inSrc = [&] (const Note& n) { return n.track != 2 && n.start >= sa - 1e-6 && n.start < sb - 1e-6; };
    std::vector<Note> src; for (auto& n : raw) if (inSrc (n)) src.push_back (n);
    std::vector<Note> v;
    for (auto& n : raw)
    {
        if (n.track != 2 && n.start >= a - 1e-6 && n.start < b - 1e-6) continue;          // the target bars' own notes go
        Note k = n;
        if (k.track != 2 && k.start < a - 1e-6 && k.start + k.length > a + 1e-6) k.length = std::max (0.02, a - k.start);   // nothing rings into the copy
        v.push_back (k);
    }
    selected.clear();
    for (int t = bar0; t <= bar1; ++t)
    {
        if (t == srcBar) { for (auto& n : src) v.push_back (n); continue; }
        for (auto& n : src)
        {
            Note c = n;
            c.start += (t - srcBar) * bb;
            c.length = std::min (c.length, (t + 1) * bb - c.start);   // a long note stops at the end of its bar
            c.id = proc.allocNoteId(); c.weight = metricWeight (c.start);
            v.push_back (c); selected.insert (c.id);
        }
    }
    commit (std::move (v));
    if (onMessage) onMessage ("Bar" + juce::String (bar0 == bar1 ? " " + juce::String (bar0 + 1) : "s " + juce::String (bar0 + 1) + "-" + juce::String (bar1 + 1))
                              + " now play" + (bar0 == bar1 ? "s" : "") + " like bar " + juce::String (srcBar + 1) + " (selected: edit them freely, Ctrl+Z undoes)");
}

void RollView::rerollSelectedBars()
{
    const int a = barSelFirst(), b = barSelLast();
    if (a < 0) return;
    proc.rerollBars (a, b); selected.clear(); refresh (true);
}

void RollView::copyChord()
{
    if (! snap || snap->pattern.chords.empty()) return;
    const int a = barSelFirst(); if (a < 0) return;
    const auto& c = snap->pattern.chordAt (a * std::max (0.5, snap->pattern.barBeats) + 0.01);
    chordClip.has = true; chordClip.rootPc = c.rootPc; chordClip.q = c.q;
    notifyBars();
}

void RollView::applyChordToSelection (int rootPc, Quality q)
{
    const int a = barSelFirst(), b = barSelLast();
    if (a < 0) return;
    proc.setBarChordRange (a, b, true, rootPc, q);
    selected.clear(); refresh (true);
}

void RollView::pasteChord()
{
    if (chordClip.has) applyChordToSelection (chordClip.rootPc, chordClip.q);
}

//------------------------------------------------------------------------------ menus
void RollView::showChordMenu (int anchorBar, juce::Rectangle<int> cell)
{
    if (! snap || snap->pattern.chords.empty()) return;
    const auto& P = snap->pattern;
    const int keyRoot = P.s.keyRoot;
    const double bb = std::max (0.5, P.barBeats);
    if (barSelFirst() < 0 || anchorBar < barSelFirst() || anchorBar > barSelLast()) { barA = barB = anchorBar; notifyBars(); }
    const int b0 = barSelFirst(), b1 = barSelLast();
    const auto& cur = P.chordAt (b0 * bb + 0.01);
    const int curDeg = cur.degree; const Quality curQ = cur.q;
    const auto& bc = proc.getBarChords();
    const bool isSet = b0 < (int) bc.size() && bc[(size_t) b0].set;

    struct Sug { int deg; Quality q; };
    std::vector<Sug> sug;
    switch (scaleFamily (P.s.scale))
    {
        case 0:  sug = { {0, Quality::Maj7}, {2, Quality::Min7}, {4, Quality::Min7}, {5, Quality::Maj7}, {7, Quality::Dom7}, {9, Quality::Min7}, {11, Quality::Min7b5}, {5, Quality::Dom7}, {0, Quality::Maj6} }; break;
        case 2:  sug = { {0, Quality::Dom7}, {5, Quality::Dom7}, {7, Quality::Min7}, {10, Quality::Maj7}, {2, Quality::Min7}, {0, Quality::Dom9}, {5, Quality::Dom9}, {10, Quality::Dom7} }; break;
        default: sug = { {0, Quality::Min7}, {2, Quality::Min7b5}, {3, Quality::Maj7}, {5, Quality::Min7}, {7, Quality::Dom7}, {7, Quality::Min7}, {8, Quality::Maj7}, {10, Quality::Dom7}, {0, Quality::Min9}, {5, Quality::Dom7} }; break;
    }
    // chords already used somewhere in this solo (so one click can repeat an earlier bar's chord)
    struct Used { int bar; int rootPc; Quality q; juce::String name; };
    std::vector<Used> used;
    for (int bar = 0; bar < P.bars; ++bar)
    {
        const auto& c = P.chordAt (bar * bb + 0.01);
        bool dup = false; for (auto& u : used) if (u.rootPc == c.rootPc && u.q == c.q) dup = true;
        if (! dup) used.push_back ({ bar, c.rootPc, c.q, juce::String (c.name) });
    }

    juce::PopupMenu m;
    m.addSectionHeader (barSelText().upToFirstOccurrenceOf ("  (", false, false) + " - choose the chord");
    m.addItem (1, "Follow the song (automatic)", true, ! isSet);
    m.addSeparator();
    if (used.size() > 1)
    {
        m.addSectionHeader ("Same as another bar");
        for (size_t i = 0; i < used.size(); ++i) m.addItem (300 + (int) i, "Bar " + juce::String (used[i].bar + 1) + ":  " + used[i].name);
        m.addSeparator();
    }
    m.addSectionHeader ("Fits this key");
    for (size_t i = 0; i < sug.size(); ++i)
        m.addItem (100 + (int) i, juce::String (chordName ((keyRoot + sug[i].deg) % 12, sug[i].q)), true, isSet && curDeg == sug[i].deg && curQ == sug[i].q);
    m.addSeparator();
    const auto& qs = qualityMenuOrder();
    juce::PopupMenu all;
    for (int r = 0; r < 12; ++r)
    {
        const int root = (keyRoot + r) % 12;
        juce::PopupMenu sub;
        for (size_t qi = 0; qi < qs.size(); ++qi)
            sub.addItem (1000 + r * 32 + (int) qi, juce::String (chordName (root, qs[qi])), true, isSet && curDeg == r && curQ == qs[qi]);
        all.addSubMenu (juce::String (kNoteNames[root]) + (r == 0 ? "   (key)" : ""), sub);
    }
    m.addSubMenu ("All chords...", all);
    m.addSeparator();
    {
        const int curMode = [&] { for (int mm = 0; mm < M_Count; ++mm) if (cur.scaleMask == modeMask (cur.rootPc, mm)) return mm; return -1; }();
        const bool modeSet = b0 < (int) bc.size() && bc[(size_t) b0].set && bc[(size_t) b0].mode >= 0;
        m.addSectionHeader ("Scale for these bars (best fits first)");
        m.addItem (499, "Automatic (the song's scale)", true, ! modeSet);
        for (int mm : suggestModes (curQ))
            m.addItem (500 + mm, juce::String (kNoteNames[cur.rootPc % 12]) + " " + modeName (mm), true, modeSet && curMode == mm);
        juce::PopupMenu allModes;
        for (int mm = 0; mm < M_Count; ++mm)
            allModes.addItem (600 + mm, juce::String (kNoteNames[cur.rootPc % 12]) + " " + modeName (mm), true, modeSet && curMode == mm);
        m.addSubMenu ("All scales...", allModes);
        m.addSeparator();
    }
    m.addItem (2, "Copy this chord   (Ctrl+C)");
    m.addItem (3, "Paste chord into the selected bar(s)   (Ctrl+V)", chordClip.has);
    m.addItem (4, "Repeat this chord in the next bar", b1 + 1 < P.bars);
    m.addItem (5, "Repeat this chord in every following bar", b1 + 1 < P.bars);
    m.addSeparator();
    m.addItem (20, "Make these bars busier (+25%)");
    m.addItem (21, "Make these bars calmer (-25%)");
    m.addItem (22, "Busy back to automatic", selectionHasExplicitBusy());
    m.addItem (6, "Re-roll the notes of these bars");
    m.addItem (7, "Re-roll from here to the end");
    m.addItem (8, "Clear the notes of these bars");
    m.addItem (9, "Select the notes of these bars");
    {
        // v3.14: these bars play exactly like another bar (its notes, articulations and hand-drawn keys); edit the copy afterwards
        juce::PopupMenu like;
        for (int b = 0; b < P.bars; ++b)
            if (b < b0 || b > b1)
            {
                int cnt = 0; for (auto& n : raw) if (n.track != 2 && n.start >= b * bb - 1e-6 && n.start < (b + 1) * bb - 1e-6) ++cnt;
                like.addItem (3000 + b, "Bar " + juce::String (b + 1) + (cnt > 0 ? "   (" + juce::String (cnt) + " notes)" : "   (empty)"), cnt > 0);
            }
        m.addSubMenu (b0 == b1 ? "Play this bar like bar..." : "Play these bars like bar...", like, P.bars > 1);
    }
    m.addSeparator();
    m.addItem (10, "Reset every bar to the song's chords");

    const int curRoot = (keyRoot + curDeg) % 12; const int bars = P.bars;
    m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (cell)).withMinimumWidth (220),
                     [safe = juce::Component::SafePointer<RollView> (this), b0, b1, keyRoot, sug, qs, used, curRoot, curQ, bars, bb] (int r)
                     {
                         if (safe == nullptr || r == 0) return;
                         auto* s = safe.getComponent(); auto& pr = s->proc;
                         auto done = [&] { s->selected.clear(); s->refresh (true); };
                         if (r == 1) { pr.setBarChordRange (b0, b1, false, 0, Quality::Min7); done(); }
                         else if (r >= 300 && r < 300 + (int) used.size()) { const auto& u = used[(size_t) (r - 300)]; pr.setBarChordRange (b0, b1, true, u.rootPc, u.q); done(); }
                         else if (r >= 100 && r < 100 + (int) sug.size()) { const auto& c = sug[(size_t) (r - 100)]; pr.setBarChordRange (b0, b1, true, (keyRoot + c.deg) % 12, c.q); done(); }
                         else if (r >= 1000) { const int rel = (r - 1000) / 32, qi = (r - 1000) % 32; if (qi < (int) qs.size()) { pr.setBarChordRange (b0, b1, true, (keyRoot + rel) % 12, qs[(size_t) qi]); done(); } }
                         else if (r == 499) { pr.setBarMode (b0, b1, -1); done(); }
                         else if (r >= 500 && r < 500 + M_Count) { pr.setBarMode (b0, b1, r - 500); done(); }
                         else if (r >= 600 && r < 600 + M_Count) { pr.setBarMode (b0, b1, r - 600); done(); }
                         else if (r == 2) s->copyChord();
                         else if (r == 3) s->pasteChord();
                         else if (r == 4) { pr.setBarChord (b1 + 1, true, curRoot, curQ); done(); }
                         else if (r == 5) { pr.setBarChordRange (b1 + 1, bars - 1, true, curRoot, curQ); done(); }
                         else if (r == 20 || r == 21)
                         {
                             const float v = juce::jlimit (0.f, 1.f, s->busyOfSelection() + (r == 20 ? 0.25f : -0.25f));
                             pr.pushUndo(); s->applyBusyToSelection (v, (uint64_t) juce::Random::getSystemRandom().nextInt64());
                         }
                         else if (r == 22) { pr.pushUndo(); s->applyBusyToSelection (-1.f, (uint64_t) juce::Random::getSystemRandom().nextInt64()); }
                         else if (r >= 3000 && r < 3000 + bars) { s->copyBarPlay (r - 3000, b0, b1); }
                         else if (r == 6) { pr.rerollBars (b0, b1); done(); }
                         else if (r == 7) { pr.rerollBars (b0, bars - 1); done(); }
                         else if (r == 8)
                         {
                             std::vector<Note> v; const double a = b0 * bb, b = (b1 + 1) * bb;
                             for (auto& n : s->raw) if (n.track == 2 || n.start < a - 1e-6 || n.start >= b - 1e-6) v.push_back (n);
                             s->selected.clear(); s->commit (std::move (v));
                         }
                         else if (r == 9)
                         {
                             s->selected.clear(); const double a = b0 * bb, b = (b1 + 1) * bb;
                             for (auto& n : s->raw) if (n.track != 2 && n.start >= a - 1e-6 && n.start < b - 1e-6) s->selected.insert (n.id);
                             s->repaint();
                         }
                         else if (r == 10) { pr.clearBarChords(); done(); }
                     });
}

void RollView::addEditItems (juce::PopupMenu& m, bool withSelection)
{
    (void) withSelection;
    juce::PopupMenu sel;
    sel.addItem (1, "Select all   (Ctrl+A)"); sel.addItem (2, "Select none   (Esc)"); sel.addItem (3, "Invert selection");
    sel.addItem (4, "Same pitch as the selected notes"); sel.addItem (5, "Notes in the selected bars", barSelFirst() >= 0);
    m.addSubMenu ("Select", sel);
    m.addSeparator();
    m.addItem (10, "Cut   (Ctrl+X)"); m.addItem (11, "Copy   (Ctrl+C)"); m.addItem (12, "Paste   (Ctrl+V)", ! clipboard.empty());
    m.addItem (13, "Duplicate   (Ctrl+D)"); m.addItem (14, "Delete   (Del)");
    m.addSeparator();
    m.addItem (20, "Mute   (M)"); m.addItem (21, "Unmute"); m.addItem (22, "Unmute all notes");
    m.addSeparator();
    juce::PopupMenu tm;
    tm.addItem (30, "Quantize to grid   (Q)"); tm.addItem (31, "Quantize halfway (50%)");
    tm.addItem (32, "Legato (each note runs to the next)   (L)"); tm.addItem (33, "Staccato (shorten to half)");
    tm.addItem (34, "Set length to the grid size"); tm.addItem (35, "Split in half"); tm.addItem (36, "Join touching notes of the same pitch");
    tm.addItem (37, "Stretch time x2"); tm.addItem (38, "Stretch time x0.5"); tm.addItem (39, "Reverse in time");
    m.addSubMenu ("Time & length", tm);
    juce::PopupMenu pm;
    pm.addItem (40, "Up a semitone"); pm.addItem (41, "Down a semitone"); pm.addItem (42, "Up an octave"); pm.addItem (43, "Down an octave");
    pm.addItem (44, "Mirror the pitches (invert)");
    m.addSubMenu ("Pitch", pm);
    juce::PopupMenu vm;
    vm.addItem (50, "Soft (55)"); vm.addItem (51, "Medium (80)"); vm.addItem (52, "Loud (105)"); vm.addItem (53, "Max (127)");
    vm.addItem (54, "Crescendo"); vm.addItem (55, "Decrescendo"); vm.addItem (56, "Randomise a little");
    m.addSubMenu ("Velocity", vm);
    {   // v3.12: key-switches you can edit
        const bool asNotes = keySwitchesAreNotes();
        juce::PopupMenu km;
        km.addItem (70, "Edit key-switches as notes (move, stretch, delete, velocity)", ksView().on && ! ksEvents().empty());
        km.addItem (71, "Back to automatic key-switches (removes the key-switch notes)", asNotes);
        m.addSubMenu ("Key-switches", km);
    }
    juce::PopupMenu art;
    for (int a : artList (snap ? snap->pattern.s.instrument : 0)) art.addItem (200 + a, artName (a));
    // v3.8: the instrument's own keys from its mapping profile (Pop, fret positions, Feedback ...), placed by hand
    if (const auto* lib = artLib())
    {
        juce::PopupMenu own; int count = 0;
        for (auto& a : lib->arts) if (enochsolo::isCustomArt (a.art)) { own.addItem (300 + (a.art - enochsolo::kCustomArt), juce::String (artLabel (lib, a.art))); ++count; }
        if (count > 0) { art.addSeparator(); art.addSubMenu (juce::String (lib->name) + " keys", own); }
    }
    m.addSubMenu ("Articulation", art);
    juce::PopupMenu hm;
    hm.addItem (60, "Light"); hm.addItem (61, "Medium   (H)"); hm.addItem (62, "Strong");
    m.addSubMenu ("Humanize (timing + velocity)", hm);
    m.addItem (63, proc.loopRegionOn() ? "Stop looping the selection   (Ctrl+L)" : "Loop the selection with PLAY   (Ctrl+L)", proc.loopRegionOn() || ! selected.empty() || barSelFirst() >= 0);
}

void RollView::showEditMenu (juce::Component& target)
{
    juce::PopupMenu m; addEditItems (m, true);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&target),
                     [safe = juce::Component::SafePointer<RollView> (this)] (int r) { if (safe != nullptr && r != 0) safe->doOp (r); });
}

void RollView::showNoteMenu (juce::Point<int> screenPos, bool onNote, double beat, int pitch)
{
    juce::PopupMenu m;
    if (! onNote) m.addItem (900, "Add a note here");
    addEditItems (m, true);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (juce::Rectangle<int> (screenPos.x, screenPos.y, 1, 1)),
                     [safe = juce::Component::SafePointer<RollView> (this), beat, pitch] (int r)
                     {
                         if (safe == nullptr || r == 0) return;
                         if (r == 900) safe->addNoteAt (beat, pitch); else safe->doOp (r);
                     });
}

void RollView::showLaneMenu (juce::Point<int> screenPos, double beat)
{
    juce::PopupMenu m;
    if (laneSel == 0)
    {
        m.addSectionHeader ("Velocity of the selected notes (or all)");
        m.addItem (50, "Soft (55)"); m.addItem (51, "Medium (80)"); m.addItem (52, "Loud (105)"); m.addItem (53, "Max (127)");
        m.addItem (54, "Crescendo"); m.addItem (55, "Decrescendo"); m.addItem (56, "Randomise a little");
    }
    else
    {
        const int L = ctrlLane();
        m.addSectionHeader (juce::String (laneName (laneSel)).upToFirstOccurrenceOf (" (", false, false) + " - draw a shape over the selected bars (or the whole bass line)");
        if (L == L_Bend) { m.addItem (904, "Vibrato wave (+/-0.3 semitone)"); m.addItem (906, "Rise (+2 semitones)"); m.addItem (907, "Dive (-2 semitones)"); m.addItem (905, "Back to centre"); }
        else
        {
            m.addItem (901, "Ramp up"); m.addItem (902, "Ramp down"); m.addItem (903, "Swell (up and down)");
            m.addItem (904, L == L_Mod ? "Vibrato wave" : "Wave"); m.addItem (905, "Reset");
        }
        m.addSeparator();
        m.addItem (910, "Clear this lane in the selected bars / this bar");
        m.addItem (911, "Clear this whole lane (back to automatic)");
    }
    m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (juce::Rectangle<int> (screenPos.x, screenPos.y, 1, 1)),
                     [safe = juce::Component::SafePointer<RollView> (this), beat] (int r)
                     {
                         if (safe == nullptr || r == 0) return;
                         auto* s = safe.getComponent();
                         if (r < 900) { s->doOp (r); return; }
                         if (! s->snap) return;
                         const double bb = std::max (0.5, s->snap->pattern.barBeats);
                         double t0 = 0, t1 = s->snap->pattern.lengthBeats;
                         if (s->barSelFirst() >= 0) { t0 = s->barSelFirst() * bb; t1 = (s->barSelLast() + 1) * bb; }
                         const int L = s->ctrlLane();
                         if (L < 0) return;
                         if (r >= 901 && r <= 907) s->drawShape (r - 900, L, t0, t1);
                         else if (r == 910)
                         {
                             if (s->barSelFirst() < 0) { const int b = juce::jlimit (0, s->snap->pattern.bars - 1, (int) std::floor (beat / bb)); t0 = b * bb; t1 = (b + 1) * bb; }
                             s->proc.clearCtrl (L, t0 - 1e-6, t1); s->refresh (true);
                         }
                         else if (r == 911) { s->proc.clearCtrl (L, -1.0, s->snap->pattern.lengthBeats + 1.0); s->refresh (true); }
                     });
}

//------------------------------------------------------------------------------ note operations
void RollView::selectAll() { selected.clear(); for (auto& n : raw) if (n.track != 2) selected.insert (n.id); repaint(); }

void RollView::deleteSelected()
{
    if (selected.empty()) return;
    std::vector<Note> v; for (auto& n : raw) if (! selected.count (n.id)) v.push_back (n);
    selected.clear(); commit (std::move (v));
}

void RollView::copySelected()
{
    clipboard.clear(); for (auto& n : raw) if (selected.count (n.id)) clipboard.push_back (n);
}

void RollView::cutSelected() { copySelected(); deleteSelected(); }

void RollView::pasteClipboard()
{
    if (clipboard.empty() || ! snap) return;
    double first = 1e9; for (auto& n : clipboard) first = std::min (first, n.start);
    auto v = raw; selected.clear();
    for (auto n : clipboard)
    {
        n.start = n.start - first + pasteAt;
        if (n.start >= totalBeats()) continue;
        n.length = std::min (n.length, totalBeats() - n.start);
        n.id = proc.allocNoteId(); n.weight = metricWeight (n.start);
        selected.insert (n.id); v.push_back (n);
    }
    commit (std::move (v));
}

void RollView::duplicateSelected()
{
    if (selected.empty()) return;
    double lo = 1e9, hi = 0;
    for (auto& n : raw) if (selected.count (n.id)) { lo = std::min (lo, n.start); hi = std::max (hi, n.start + n.length); }
    double shift = hi - lo; const double gb = gridBeats();
    if (gb > 0) shift = std::ceil (shift / gb - 1e-6) * gb;
    auto v = raw; std::set<int> ns;
    for (auto& n : raw)
        if (selected.count (n.id))
        {
            Note c = n; c.start += shift;
            if (c.start >= totalBeats()) continue;
            c.length = std::min (c.length, totalBeats() - c.start); c.id = proc.allocNoteId(); c.weight = metricWeight (c.start);
            ns.insert (c.id); v.push_back (c);
        }
    selected = ns; commit (std::move (v));
}

void RollView::quantizeSelected()
{
    const double gb = gridBeats() > 0 ? gridBeats() : 0.25;
    const bool all = selected.empty();
    auto v = raw;
    for (auto& n : v)
        if (n.track != 2 && (all || selected.count (n.id)))
        {
            n.start = std::max (0.0, std::round (n.start / gb) * gb);
            n.length = std::max (gb, std::round (n.length / gb) * gb);
            if (n.start + n.length > totalBeats()) n.length = std::max (0.0625, totalBeats() - n.start);
            n.weight = metricWeight (n.start);
        }
    commit (std::move (v));
}

void RollView::transposeSelected (int semis)
{
    if (selected.empty()) return;
    auto v = raw; for (auto& n : v) if (selected.count (n.id)) n.pitch = juce::jlimit (0, 127, n.pitch + semis);
    commit (std::move (v));
}

void RollView::nudge (double dBeat, int dPitch)
{
    if (selected.empty()) return;
    auto v = raw;
    for (auto& n : v)
        if (selected.count (n.id))
        {
            n.start = juce::jlimit (0.0, std::max (0.0, totalBeats() - n.length), n.start + dBeat);
            n.pitch = snapPitch (n.pitch + dPitch, n.start);
            n.weight = metricWeight (n.start);
        }
    if (dPitch != 0) for (auto& n : v) if (selected.count (n.id)) { proc.audition (n.pitch, n.vel); break; }
    commit (std::move (v));
}

void RollView::setArtOnSelected (int art)
{
    if (selected.empty()) return;
    auto v = raw; for (auto& n : v) if (selected.count (n.id)) { n.art = art; n.why = 0; if (art != A_Bend) n.bend = 0; else if (n.bend == 0) n.bend = 1; }
    commit (std::move (v));
}

void RollView::setVelOnSelected (int vel)
{
    if (selected.empty()) return;
    auto v = raw; for (auto& n : v) if (selected.count (n.id)) n.vel = vel;
    commit (std::move (v));
}

void RollView::doUndo() { proc.undo(); selected.clear(); refresh (true); }
void RollView::doRedo() { proc.redo(); selected.clear(); refresh (true); }

void RollView::doOp (int id)
{
    if (id == 70) { editKeySwitchesAsNotes(); return; }
    if (id == 71) { automaticKeySwitches(); return; }
    if (! snap) return;
    const double total = totalBeats();
    const double gb = gridBeats() > 0 ? gridBeats() : 0.25;
    auto targets = [&] (std::vector<Note>& v) { std::vector<Note*> t; for (auto& n : v) if (n.track != 2 && selected.count (n.id)) t.push_back (&n); return t; };
    auto sortByStart = [] (std::vector<Note*>& t) { std::stable_sort (t.begin(), t.end(), [] (const Note* a, const Note* b) { return a->start < b->start; }); };

    if (id >= 200 && id < 260) { setArtOnSelected (id - 200); return; }
    if (id >= 300 && id < 300 + enochsolo::kMaxCustom) { setArtOnSelected (enochsolo::kCustomArt + (id - 300)); return; }   // v3.8 profile keys
    if (id >= 60 && id <= 62) { humanizeSelected (id - 59); return; }
    if (id == 63) { toggleLoopSelection(); return; }
    switch (id)
    {
        case 1: selectAll(); return;
        case 2: selected.clear(); repaint(); return;
        case 3: { std::set<int> s2; for (auto& n : raw) if (n.track != 2 && ! selected.count (n.id)) s2.insert (n.id); selected = s2; repaint(); return; }
        case 4: { std::set<int> pcs; for (auto& n : raw) if (selected.count (n.id)) pcs.insert (n.pitch);
                  for (auto& n : raw) { if (n.track != 2 && pcs.count (n.pitch)) selected.insert (n.id); }
                  repaint(); return; }
        case 5: { const double bb = std::max (0.5, snap->pattern.barBeats); const double a = barSelFirst() * bb, b = (barSelLast() + 1) * bb;
                  selected.clear(); for (auto& n : raw) if (n.track != 2 && n.start >= a - 1e-6 && n.start < b - 1e-6) selected.insert (n.id); repaint(); return; }
        case 10: cutSelected(); return;
        case 11: copySelected(); return;
        case 12: pasteClipboard(); return;
        case 13: duplicateSelected(); return;
        case 14: deleteSelected(); return;
        case 30: quantizeSelected(); return;
        case 40: transposeSelected (1); return;   case 41: transposeSelected (-1); return;
        case 42: transposeSelected (12); return;  case 43: transposeSelected (-12); return;
        case 50: setVelOnSelected (55); return;   case 51: setVelOnSelected (80); return;
        case 52: setVelOnSelected (105); return;  case 53: setVelOnSelected (127); return;
        default: break;
    }

    auto v = raw;
    bool all = selected.empty();
    if (all && (id == 54 || id == 55 || id == 56 || id == 32)) { for (auto& n : v) if (n.track == 0 && n.voice == 0) selected.insert (n.id); }
    auto t = targets (v);
    if (t.empty() && id != 22) return;
    sortByStart (t);
    switch (id)
    {
        case 20: for (auto* n : t) n->mute = true; break;
        case 21: for (auto* n : t) n->mute = false; break;
        case 22: for (auto& n : v) n.mute = false; break;
        case 31: for (auto* n : t) { const double q = std::round (n->start / gb) * gb; n->start = std::max (0.0, n->start + (q - n->start) * 0.5); n->weight = metricWeight (n->start); } break;
        case 32:
        {
            for (auto* n : t)
            {
                double next = total;
                for (auto& o : v) if (o.track == n->track && o.voice == n->voice && n->voice == 0 && o.start > n->start + 1e-6) next = std::min (next, o.start);
                n->length = std::max (0.05, next - n->start - 0.01);
            }
            break;
        }
        case 33: for (auto* n : t) n->length = std::max (0.05, n->length * 0.5); break;
        case 34: for (auto* n : t) n->length = std::min (gb, total - n->start); break;
        case 35:
        {
            std::vector<Note> add;
            for (auto* n : t) if (n->length > 0.06) { Note b = *n; n->length *= 0.5; b.start = n->start + n->length; b.id = proc.allocNoteId(); b.weight = metricWeight (b.start); b.art = A_Sustain; add.push_back (b); }
            v.insert (v.end(), add.begin(), add.end()); break;
        }
        case 36:
        {
            std::set<int> kill;
            for (size_t i = 0; i < t.size(); ++i)
            {
                if (kill.count (t[i]->id)) continue;
                for (size_t j = i + 1; j < t.size(); ++j)
                    if (! kill.count (t[j]->id) && t[j]->pitch == t[i]->pitch && t[j]->track == t[i]->track && t[j]->voice == t[i]->voice
                        && std::abs (t[j]->start - (t[i]->start + t[i]->length)) < 0.03)
                    { t[i]->length = t[j]->start + t[j]->length - t[i]->start; kill.insert (t[j]->id); }
            }
            v.erase (std::remove_if (v.begin(), v.end(), [&] (const Note& n) { return kill.count (n.id) > 0; }), v.end());
            selected.clear(); break;
        }
        case 37: case 38:
        {
            const double f = id == 37 ? 2.0 : 0.5, first = t.front()->start;
            for (auto* n : t) { n->start = std::min (total - 0.05, first + (n->start - first) * f); n->length = std::max (0.05, std::min (n->length * f, total - n->start)); n->weight = metricWeight (n->start); }
            break;
        }
        case 39:
        {
            double lo = 1e9, hi = 0; for (auto* n : t) { lo = std::min (lo, n->start); hi = std::max (hi, n->start + n->length); }
            for (auto* n : t) { n->start = lo + hi - (n->start + n->length); n->weight = metricWeight (n->start); }
            break;
        }
        case 44:
        {
            double mean = 0; for (auto* n : t) mean += n->pitch; mean /= (double) t.size();
            for (auto* n : t) n->pitch = snapPitch ((int) std::lround (2.0 * mean - n->pitch), n->start);
            break;
        }
        case 54: case 55:
        {
            const size_t N = t.size();
            for (size_t i = 0; i < N; ++i) { const double u = N > 1 ? (double) i / (double) (N - 1) : 1.0; t[i]->vel = (int) std::lround (id == 54 ? 40 + 75 * u : 115 - 75 * u); }
            break;
        }
        case 56: { juce::Random r; for (auto* n : t) n->vel = juce::jlimit (1, 127, n->vel + r.nextInt (25) - 12); break; }
        default: return;
    }
    commit (std::move (v));
}

//------------------------------------------------------------------------------ keys
bool RollView::keyPressed (const juce::KeyPress& k)
{
    const auto m = k.getModifiers();
    const double step = gridBeats() > 0 ? gridBeats() : 0.0625;
    if (k == juce::KeyPress::deleteKey || k == juce::KeyPress::backspaceKey) { deleteSelected(); return true; }
    if (m.isCommandDown())
    {
        const auto c = juce::CharacterFunctions::toUpperCase ((juce::juce_wchar) k.getKeyCode());
        if (focusChords && barSelFirst() >= 0 && (c == 'C' || c == 'V')) { if (c == 'C') copyChord(); else pasteChord(); return true; }
        switch (c)
        {
            case 'A': selectAll(); return true;
            case 'C': copySelected(); return true;
            case 'X': cutSelected(); return true;
            case 'V': pasteClipboard(); return true;
            case 'D': duplicateSelected(); return true;
            case 'Z': if (m.isShiftDown()) doRedo(); else doUndo(); return true;
            case 'Y': doRedo(); return true;
            case 'L': toggleLoopSelection(); return true;     // v3.8
            default: break;
        }
        return false;
    }
    const auto ch = juce::CharacterFunctions::toUpperCase (k.getTextCharacter());
    if (ch >= '1' && ch <= '5') { setTool ((Tool) (ch - '1')); return true; }
    if (ch == 'M') { doOp (20); return true; }
    if (ch == 'Q') { quantizeSelected(); return true; }
    if (ch == 'L') { doOp (32); return true; }
    // v3.8: Up / Down step the selected notes (by scale where the bar is scale-locked), Left / Right walk the selection from note to
    // note, Alt + Left / Right move the notes in time (what Left / Right did before)
    if (k.isKeyCode (juce::KeyPress::leftKey))  { if (m.isAltDown()) nudge (-step, 0); else selectNext (-1, m.isShiftDown()); return true; }
    if (k.isKeyCode (juce::KeyPress::rightKey)) { if (m.isAltDown()) nudge (step, 0); else selectNext (1, m.isShiftDown()); return true; }
    if (k.isKeyCode (juce::KeyPress::upKey))    { stepSelected (1, m.isShiftDown()); return true; }
    if (k.isKeyCode (juce::KeyPress::downKey))  { stepSelected (-1, m.isShiftDown()); return true; }
    if (ch == 'H') { humanizeSelected (2); return true; }
    if (k.getTextCharacter() == '?' || (k.getKeyCode() == '/' && m.isShiftDown()) || k.isKeyCode (juce::KeyPress::F1Key)) { setShowShortcuts (! showKeys); return true; }
    if (k == juce::KeyPress::escapeKey) { if (showKeys) { setShowShortcuts (false); return true; } selected.clear(); barA = barB = -1; notifyBars(); repaint(); return true; }
    return false;
}


//------------------------------------------------------------------------------ v3.8
const enochsolo::Library* RollView::artLib() const
{
    // the profile of the instrument the solo / bass line plays on: the routed profile when there is one, else the main panel's
    if (! snap) return nullptr;
    const auto& S = snap->pattern.s;
    if (S.routed && S.routes[0].lib && S.routes[0].lib->inst == S.instrument && ! S.routes[0].lib->standard) return S.routes[0].lib.get();
    if (S.lib && S.lib->inst == S.instrument && ! S.lib->standard) return S.lib.get();
    return nullptr;
}

void RollView::autoScrollFor (juce::Point<float> p)
{
    const auto G = geo();
    bool changed = false;
    if (viewLen > 0 && G.viewLen < G.len - 1e-6)
    {
        float over = 0.f;
        if (p.x > G.notes.getRight()) over = p.x - G.notes.getRight();
        else if (p.x < G.notes.getX()) over = p.x - G.notes.getX();
        if (over != 0.f)
        {
            const double d = (double) juce::jlimit (-240.f, 240.f, over) * 0.25 / (double) G.ppb + (over > 0 ? 1.0 : -1.0) / 64.0;
            const double before = viewStart;
            viewStart = juce::jlimit (0.0, G.len - G.viewLen, G.viewStart + d);
            changed = std::abs (viewStart - before) > 1e-9;
        }
    }
    if (visRows < 128 && (drag == Drag::Rubber || drag == Drag::Move))
    {
        int dp = 0;
        if (p.y < G.notes.getY()) dp = 1 + (int) ((G.notes.getY() - p.y) / 22.f);
        else if (p.y > G.notes.getBottom()) dp = -(1 + (int) ((p.y - G.notes.getBottom()) / 22.f));
        if (dp != 0)
        {
            const int before = topPitch;
            topPitch = juce::jlimit (juce::jlimit (12, 128, visRows) - 1, 127, G.hi + juce::jlimit (-6, 6, dp)); vAuto = false;
            changed = changed || topPitch != before;
        }
    }
    if (changed)
    {
        clampView(); updateScrollBars(); notifyView(); repaint();
        juce::Component::beginDragAutoRepeat (30);       // keeps scrolling while the mouse rests past the edge
    }
}

int RollView::stepPitch (int pitch, double beat, int dir) const
{
    const double bbl = snap ? std::max (0.5, snap->pattern.barBeats) : 4.0;
    const bool locked = proc.isScaleLocked ((int) std::floor (beat / bbl + 1e-6));
    const bool snapOn = proc.apvts.getRawParameterValue ("snapScale")->load() >= 0.5f;
    if ((locked || snapOn) && snap && ! snap->pattern.chords.empty())
    {
        const auto mask = snap->pattern.chordAt (beat).scaleMask;
        if (mask != 0)
            for (int q = pitch + dir; q >= 0 && q <= 127; q += dir)
                if (inMask (mask, q)) return q;                  // the next note of the scale in that direction
    }
    return juce::jlimit (0, 127, pitch + dir);
}

void RollView::stepSelected (int dir, bool octave)
{
    if (selected.empty()) return;
    auto v = raw;
    const auto kvS = ksView();
    bool any = false;
    for (auto& n : v)
        if (selected.count (n.id))
        {
            const int q = octave ? n.pitch + 12 * dir : ((kvS.on && n.voice == kKsVoice) ? n.pitch + dir : stepPitch (n.pitch, n.start, dir));
            if (q < 0 || q > 127) continue;
            n.pitch = q; any = true;
        }
    if (! any) return;
    markKsNotes (v, selected);
    for (auto& n : v) if (selected.count (n.id)) { proc.audition (n.pitch, n.vel); ensureVisible (n); break; }
    commit (std::move (v));
}

void RollView::ensureVisible (const enochsolo::Note& n)
{
    const auto G = geo();
    bool changed = false;
    if (viewLen > 0 && G.viewLen < G.len - 1e-6 && (n.start < G.viewStart || n.start + juce::jmin (n.length, G.viewLen * 0.5) > G.viewStart + G.viewLen))
    { viewStart = juce::jlimit (0.0, G.len - G.viewLen, n.start - G.viewLen * 0.25); changed = true; }
    if (n.pitch > G.hi || n.pitch < G.lo)
    { topPitch = juce::jlimit (juce::jlimit (12, 128, visRows) - 1, 127, n.pitch + visRows / 2); vAuto = false; changed = true; }
    if (changed) { clampView(); updateScrollBars(); notifyView(); repaint(); }
}

void RollView::selectNext (int dir, bool extend)
{
    std::vector<const Note*> order;
    for (auto& n : raw) if (n.track != 2) order.push_back (&n);
    if (order.empty()) return;
    std::stable_sort (order.begin(), order.end(), [] (const Note* a, const Note* b) { return std::abs (a->start - b->start) > 1e-9 ? a->start < b->start : a->pitch < b->pitch; });
    int cur = -1;
    for (int i = 0; i < (int) order.size(); ++i)
        if (selected.count (order[(size_t) i]->id)) { if (dir > 0 || cur < 0) cur = i; if (dir < 0) break; }
    if (dir > 0)                                         // the last selected note, walking right
        for (int i = (int) order.size() - 1; i >= 0; --i) if (selected.count (order[(size_t) i]->id)) { cur = i; break; }
    int next;
    if (cur < 0)
    {
        // nothing selected: the first note on the page (or the last one, walking left)
        const auto G = geo(); next = dir > 0 ? 0 : (int) order.size() - 1;
        for (int i = 0; i < (int) order.size(); ++i) if (order[(size_t) i]->start >= G.viewStart - 1e-9) { next = i; break; }
    }
    else next = juce::jlimit (0, (int) order.size() - 1, cur + dir);
    const Note& n = *order[(size_t) next];
    if (! extend) selected.clear();
    selected.insert (n.id); anchorId = n.id;
    proc.audition (n.pitch, n.vel);
    ensureVisible (n);
    repaint();
}

void RollView::humanizeSelected (int amount)
{
    if (! snap) return;
    std::set<int> ids = selected;
    if (ids.empty()) { if (onMessage) onMessage ("Humanize: select some notes first (Ctrl+A for all)."); return; }
    auto v = raw;
    enochx::humanize (v, ids, amount, totalBeats(), juce::jmax (40.0, proc.getHostBpm()), (uint64_t) snap->number * 31ULL + (uint64_t) amount);
    for (auto& n : v) if (ids.count (n.id)) n.weight = metricWeight (n.start);
    commit (std::move (v));
    if (onMessage) onMessage ("Humanized " + juce::String ((int) ids.size()) + " notes (" + (amount <= 1 ? "light" : amount == 2 ? "medium" : "strong") + "). Undo takes it back.");
}

void RollView::toggleLoopSelection()
{
    if (proc.loopRegionOn()) { proc.clearLoopRegion(); repaint(); if (onMessage) onMessage ("Loop selection off."); return; }
    if (! snap) return;
    double a = 1e9, b = -1.0;
    const double bb = std::max (0.5, snap->pattern.barBeats);
    if (! selected.empty()) for (auto& n : raw) if (selected.count (n.id)) { a = std::min (a, n.start); b = std::max (b, n.start + n.length); }
    if (b < 0 && barSelFirst() >= 0) { a = barSelFirst() * bb; b = (barSelLast() + 1) * bb; }
    if (b <= a) { if (onMessage) onMessage ("Loop selection: select notes or bars first."); return; }
    // whole beats around the notes, so the loop keeps the groove
    a = std::floor (a + 1e-6); b = std::min (totalBeats(), std::ceil (b - 1e-6));
    if (b - a < 0.5) b = std::min (totalBeats(), a + 1.0);
    proc.setLoopRegion (a, b);
    repaint();
    if (onMessage) onMessage ("PLAY now loops beats " + juce::String (a + 1.0, 2) + " to " + juce::String (b + 1.0, 2) + ". Ctrl+L again stops it.");
}


//------------------------------------------------------------------------------ v3.9 key-switch notes
RollView::KsView RollView::ksView() const
{
    KsView v;
    const auto* lib = artLib();
    if (! snap || lib == nullptr || lib->noKeySwitches) return v;
    const auto& S = snap->pattern.s;
    if (! (S.ksActive && S.emitKS && S.exportMode == 0)) return v;
    v.on = true; v.lib = lib;
    v.shift = lib->transpose + (S.routed ? S.routes[0].transpose : 0);
    v.row.fill (false);
    const auto mask = enochsolo::libKeyMask (*lib);
    for (int k = 0; k < 128; ++k) if (mask[(size_t) k] && k - v.shift >= 0 && k - v.shift < 128) v.row[(size_t) (k - v.shift)] = true;
    return v;
}

const std::vector<RollView::KsEvent>& RollView::ksEvents() const
{
    const int num = snap ? snap->number : -1;
    if (num == ksCacheNumber) return ksCache;
    ksCacheNumber = num; ksCache.clear();
    if (! snap) return ksCache;
    // pair each key-switch press of the main part with its release (the MIDI that leaves Enoch)
    std::vector<const MidiEv*> open;
    for (auto& e : snap->pattern.events)
    {
        if (! e.ks || e.track != 0 || (e.type != EV_ON && e.type != EV_OFF)) continue;
        if (e.drawn) continue;                              // v3.12: a key-switch you drew / made editable: it is the note itself
        if (e.type == EV_ON) { open.push_back (&e); continue; }
        for (size_t i = 0; i < open.size(); ++i)
            if (open[i]->a == e.a)
            {
                KsEvent k; k.key = e.a; k.vel = open[i]->b; k.noteId = open[i]->noteId; k.t0 = open[i]->t; k.t1 = std::max (open[i]->t + 0.02, e.t); k.src = open[i]->srcId;
                ksCache.push_back (k); open.erase (open.begin() + (long) i); break;
            }
    }
    return ksCache;
}

juce::String RollView::ksName (const enochsolo::Library& l, int key, int vel) const
{
    // the articulation this key (and velocity) stands for in the profile
    const LibArt* best = nullptr; bool exact = false;
    for (auto& a : l.arts)
    {
        for (auto& p : a.press)
            if (p.note == key && ! exact)
            {
                const bool velOk = vel < 0 || std::abs (p.vel - vel) < 8;
                if (best == nullptr || velOk) { best = &a; exact = velOk; }
            }
        if (exact) break;
    }
    if (best != nullptr) return juce::String (artLabel (&l, best->art));
    for (size_t i = 0; i < l.stringKeys.size(); ++i) if (l.stringKeys[i] == key) return "string " + juce::String ((int) (l.stringKeys.size() - i));
    if (l.repKey == key) return "repetition";
    for (auto& p : l.init) if (p.note == key) return "mode key";
    return "key-switch";
}

juce::Rectangle<float> RollView::ksRect (const Geo& G, const KsView& kv, const KsEvent& ev) const
{
    const int r = ev.key - kv.shift;
    if (r < G.lo || r > G.hi) return {};
    const float x0 = xOf (G, ev.t0), x1 = xOf (G, ev.t1);
    return { x0, yOf (G, r) + 1.f, std::max (3.f, x1 - x0), std::max (2.f, G.rowH - 2.f) };
}

int RollView::ksHit (const Geo& G, juce::Point<float> p) const
{
    const auto kv = ksView();
    if (! kv.on) return -1;
    const auto& evs = ksEvents();
    for (int i = (int) evs.size() - 1; i >= 0; --i)
        if (ksRect (G, kv, evs[(size_t) i]).expanded (1.f, 0.5f).contains (p)) return i;
    return -1;
}

void RollView::setArtOnNote (int noteId, int art)
{
    auto v = raw; bool found = false;
    for (auto& n : v) if (n.id == noteId) { n.art = art; n.why = 0; if (art != A_Bend) n.bend = 0; else if (n.bend == 0) n.bend = 1; found = true; }
    if (found) commit (std::move (v));
}

bool RollView::keySwitchesAreNotes() const
{
    for (auto& n : raw) if (n.track == 0 && n.ksManual) return true;
    return false;
}

void RollView::editKeySwitchesAsNotes()
{
    // v3.12: everything Enoch would send as key-switches for this take (articulations, legato runs, strings, repeats, mode keys)
    // becomes ordinary orange notes on the key rows, exactly where and as long as they are now. The notes keep their articulations
    // (legato overlaps and lengths still follow them), but Enoch writes no key-switches of its own for them any more.
    const auto kv = ksView();
    if (! kv.on || kv.lib == nullptr) return;
    const auto evs = ksEvents();
    auto v = raw;
    std::set<int> made;
    for (auto& ev : evs)
    {
        const int r = ev.key - kv.shift;
        if (r < 0 || r > 127) continue;
        Note n; n.start = std::max (0.0, ev.t0); n.length = std::max (0.02, ev.t1 - ev.t0); n.pitch = r; n.vel = juce::jlimit (1, 127, ev.vel);
        n.art = A_Sustain; n.track = 0; n.voice = kKsVoice; n.id = proc.allocNoteId(); n.weight = 0.0;
        v.push_back (n); made.insert (n.id);
        if (ev.src >= 0) for (auto& q : v) if (q.id == ev.src && q.voice != kKsVoice) q.keyPlayed = true;   // a repeat played on the repetition key
    }
    for (auto& n : v) if (n.track == 0 && n.voice != kKsVoice) n.ksManual = true;
    raw = v;
    commit (std::move (v));
    if (onMessage) onMessage ("Key-switches are now notes: move, stretch, delete, copy or change their velocity. Right-click > Key-switches > Back to automatic to undo.");
}

void RollView::automaticKeySwitches()
{
    auto v = raw;
    v.erase (std::remove_if (v.begin(), v.end(), [] (const Note& n) { return n.track == 0 && n.voice == kKsVoice; }), v.end());
    for (auto& n : v) { n.ksManual = false; n.keyPlayed = false; }
    selected.clear();
    commit (std::move (v));
}

void RollView::showKsMenu (juce::Point<int> screenPos, int noteId)
{
    const auto kv = ksView();
    juce::PopupMenu m;
    if (noteId < 0 || kv.lib == nullptr)
    {
        m.addItem (1, "This key chooses the string / sets a mode (it follows the notes)", false);
        m.addItem (50, "Edit key-switches as notes (move, stretch, delete, velocity)");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea ({ screenPos.x, screenPos.y, 1, 1 }),
                         [safe = juce::Component::SafePointer<RollView> (this)] (int r) { if (safe != nullptr && r == 50) safe->editKeySwitchesAsNotes(); });
        return;
    }
    m.addItem (50, "Edit key-switches as notes (move, stretch, delete, velocity)");
    m.addSeparator();
    m.addSectionHeader ("Key-switch for this note");
    m.addItem (1, "Play this note plainly (no articulation)");
    juce::PopupMenu to;
    for (auto& la : kv.lib->arts)
        if (! la.press.empty() && la.art != A_Sustain)
            to.addItem (1000 + la.art, juce::String (artLabel (kv.lib, la.art)) + "   (" + noteName (la.press[0].note) + ")");
    m.addSubMenu ("Change to", to);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea ({ screenPos.x, screenPos.y, 1, 1 }),
                     [safe = juce::Component::SafePointer<RollView> (this), noteId] (int r)
                     {
                         if (safe == nullptr || r == 0) return;
                         if (r == 50) { safe->editKeySwitchesAsNotes(); return; }
                         if (r == 1) safe->setArtOnNote (noteId, A_Sustain);
                         else if (r >= 1000) safe->setArtOnNote (noteId, r - 1000);
                     });
}

void RollView::markKsNotes (std::vector<Note>& v, const std::set<int>& ids) const
{
    // notes placed on one of the instrument's key-switch rows are key-switches (sent at that key, with their own velocity);
    // notes moved off those rows are notes again
    const auto kv = ksView();
    if (! kv.on || kv.lib == nullptr) return;
    for (auto& n : v)
    {
        if (! ids.count (n.id) || n.track != 0) continue;
        const bool onKey = n.pitch >= 0 && n.pitch < 128 && kv.row[(size_t) n.pitch];
        if (onKey && n.voice != kKsVoice)
        {
            n.voice = kKsVoice; n.art = A_Sustain;
            const int key = n.pitch + kv.shift;
            for (auto& la : kv.lib->arts) if (! la.press.empty() && la.press[0].note == key) { n.vel = la.press[0].vel; break; }
        }
        else if (! onKey && n.voice == kKsVoice) n.voice = 0;
    }
}

//------------------------------------------------------------------------------ v3.9 the view, saved with the project
juce::String RollView::getViewState() const
{
    juce::String s;
    s << "vs=" << juce::String (viewStart, 4) << ";vl=" << juce::String (viewLen, 4) << ";rows=" << visRows << ";top=" << topPitch << ";auto=" << (vAuto ? 1 : 0)
      << ";lane=" << laneSel << ";laneH=" << juce::String (laneHpx, 1) << ";tool=" << (int) tool;
    return s;
}

void RollView::setViewState (const juce::String& st)
{
    if (st.isEmpty()) return;
    juce::StringPairArray kv;
    for (auto& tok : juce::StringArray::fromTokens (st, ";", "")) kv.set (tok.upToFirstOccurrenceOf ("=", false, false).trim(), tok.fromFirstOccurrenceOf ("=", false, false).trim());
    if (kv.containsKey ("lane")) setLane (kv["lane"].getIntValue());
    if (kv.containsKey ("laneH")) { setLaneHeight (kv["laneH"].getFloatValue()); if (onLaneHeightChanged) onLaneHeightChanged (laneHpx); }
    if (kv.containsKey ("tool")) setTool ((Tool) juce::jlimit (0, 4, kv["tool"].getIntValue()));
    vAuto = kv["auto"].getIntValue() != 0;
    if (! vAuto) { visRows = juce::jlimit (12, 128, kv["rows"].getIntValue()); topPitch = juce::jlimit (0, 127, kv["top"].getIntValue()); }
    else recomputeRange();
    viewLen = juce::jmax (0.0, kv["vl"].getDoubleValue()); viewStart = juce::jmax (0.0, kv["vs"].getDoubleValue());
    clampView(); updateScrollBars(); repaint(); notifyView();
}
