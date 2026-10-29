// Enoch - shared hardware look ("Enoch Studio" series): brushed gunmetal chassis with walnut end cheeks, machined knobs with knurled
// grips and spun-aluminium caps, sculpted faders, recessed LCD displays, chrome bat switches and illuminated push buttons.
// One light source everywhere: top-left. The LED / lamp colour follows the instrument.
// Used by Enoch Solo and Enoch Bass (identical file in both projects).
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <map>
#include "EnochFonts.h"

#ifndef ENOCH_DEFAULT_THEME
 #define ENOCH_DEFAULT_THEME 0
#endif

//==============================================================================
/** Makes every font in the plugin (including popup menus, tooltips and dialogs) use the embedded Inter family.
    Held by each editor through a SharedResourcePointer: the first editor installs it, the last one removes it. */
struct EnochFontHost
{
    struct Faces
    {
        juce::Typeface::Ptr regular, medium, semi, bold, extra;
        Faces()
        {
            regular = juce::Typeface::createSystemTypefaceFor (EnochFonts::interRegular, EnochFonts::interRegularSize);
            medium  = juce::Typeface::createSystemTypefaceFor (EnochFonts::interMedium, EnochFonts::interMediumSize);
            semi    = juce::Typeface::createSystemTypefaceFor (EnochFonts::interSemiBold, EnochFonts::interSemiBoldSize);
            bold    = juce::Typeface::createSystemTypefaceFor (EnochFonts::interBold, EnochFonts::interBoldSize);
            extra   = juce::Typeface::createSystemTypefaceFor (EnochFonts::interExtraBold, EnochFonts::interExtraBoldSize);
        }
        juce::Typeface::Ptr pick (const juce::Font& f) const
        {
            const auto st = f.getTypefaceStyle();
            juce::Typeface::Ptr t;
            if (st.containsIgnoreCase ("Extra") || st.containsIgnoreCase ("Black")) t = extra;
            else if (st.containsIgnoreCase ("Semi")) t = semi;
            else if (st.containsIgnoreCase ("Medium")) t = medium;
            else if (f.isBold()) t = bold;
            else t = regular;
            return t != nullptr ? t : regular;
        }
    };
    static const Faces& faces() { static const Faces f; return f; }

    struct FontLAF : public juce::LookAndFeel_V4
    {
        juce::Typeface::Ptr getTypefaceForFont (const juce::Font& f) override
        {
            if (f.getTypefaceName() == juce::Font::getDefaultSansSerifFontName() || f.getTypefaceName().isEmpty())
                if (auto t = faces().pick (f)) return t;
            return juce::LookAndFeel_V4::getTypefaceForFont (f);
        }
    };

    EnochFontHost() { juce::LookAndFeel::setDefaultLookAndFeel (&lf); }
    ~EnochFontHost() { juce::LookAndFeel::setDefaultLookAndFeel (nullptr); }
    FontLAF lf;
};

//==============================================================================
/** procedural surface textures, made once per size and cached */
struct EnochTextures
{
    /** brushed metal: long horizontal grain. base = mid colour of the metal */
    static juce::Image brushed (int w, int h, juce::Colour base, float grain = 1.f, juce::uint32 seed = 7)
    {
        w = juce::jmax (8, w); h = juce::jmax (8, h);
        juce::Image img (juce::Image::RGB, w, h, false);
        juce::Random rnd ((juce::int64) seed);
        juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
        std::vector<float> row ((size_t) w);
        float slow = 0.f;
        const float br = base.getFloatRed(), bgc = base.getFloatGreen(), bb = base.getFloatBlue();
        for (int y = 0; y < h; ++y)
        {
            slow += (rnd.nextFloat() - 0.5f) * 0.05f; slow *= 0.97f;
            float v = rnd.nextFloat() - 0.5f;
            const float k = 0.035f + 0.02f * rnd.nextFloat();
            for (int x = 0; x < w; ++x) { v += ((rnd.nextFloat() - 0.5f) - v) * k; row[(size_t) x] = v; }
            for (int x = 0; x < w; ++x)
            {
                const float n = (row[(size_t) x] * 0.16f + slow + (rnd.nextFloat() - 0.5f) * 0.035f) * grain;
                bd.setPixelColour (x, y, juce::Colour::fromFloatRGBA (juce::jlimit (0.f, 1.f, br * (1.f + n)), juce::jlimit (0.f, 1.f, bgc * (1.f + n)),
                                                                      juce::jlimit (0.f, 1.f, bb * (1.f + n)), 1.f));
            }
        }
        return img;
    }
    /** oiled walnut with vertical grain */
    static juce::Image walnut (int w, int h, juce::uint32 seed = 11)
    {
        w = juce::jmax (4, w); h = juce::jmax (8, h);
        juce::Image img (juce::Image::RGB, w, h, false);
        juce::Random rnd ((juce::int64) seed);
        juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
        const float ph1 = rnd.nextFloat() * 6.f, ph2 = rnd.nextFloat() * 6.f;
        std::vector<float> fine ((size_t) w);
        for (auto& f : fine) f = rnd.nextFloat();
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
            {
                const float fx = (float) x, fy = (float) y;
                const float warp = 3.5f * std::sin (fy * 0.011f + ph1) + 1.8f * std::sin (fy * 0.037f + ph2 + fx * 0.05f);
                const float ring = 0.5f + 0.5f * std::sin ((fx + warp) * 0.62f + 1.7f * std::sin (fy * 0.004f + ph2));
                const float streak = fine[(size_t) x] * 0.10f + (rnd.nextFloat() - 0.5f) * 0.05f;
                const float t = juce::jlimit (0.f, 1.f, 0.30f + 0.38f * std::pow (ring, 3.f) + streak);
                const auto c = juce::Colour (0xff2a160b).interpolatedWith (juce::Colour (0xff6b3f22), t);
                bd.setPixelColour (x, y, c);
            }
        return img;
    }
};

//==============================================================================
/** Look and feel with one LED colour per instrument (guitar = amber, sax = rose, violin = violet, piano = ice blue, trumpet = emerald, bass = ember). */
struct EnochLookAndFeel : public juce::LookAndFeel_V4
{
    struct Theme { juce::Colour accent, bg, panel, panel2, border, accent2; };

    static juce::Colour text()  { return juce::Colour (0xffece6d8); }    // silkscreen ink
    static juce::Colour dim()   { return juce::Colour (0xffa39d90); }
    static juce::Colour green() { return juce::Colour (0xff3ddc97); }
    static juce::Colour red()   { return juce::Colour (0xffff5a5f); }
    static juce::Colour metal() { return juce::Colour (0xff3a3d43); }     // faceplate gunmetal
    static constexpr float lightAngle = -0.78f;                            // light from the top-left (JUCE angles: 0 = up, clockwise)

    /** font helper: style is "Regular", "Medium", "SemiBold", "Bold" or "ExtraBold" */
    static juce::Font fnt (float size, const char* style = "Regular", float kerning = 0.f)
    {
        return juce::Font (juce::FontOptions (size).withStyle (style).withKerningFactor (kerning));
    }

    static Theme makeTheme (int inst)
    {
        auto mk = [] (juce::uint32 led, juce::uint32 led2)
        {
            return Theme { juce::Colour (led), juce::Colour (0xff141518), juce::Colour (0xff24262b), juce::Colour (0xff34373e), juce::Colour (0xff4a4e57), juce::Colour (led2) };
        };
        switch (inst)
        {
            case 1:  return mk (0xffff6f91, 0xffd9466e);   // sax: rose
            case 2:  return mk (0xffb69bff, 0xff8a6af0);   // violin: violet
            case 3:  return mk (0xff5cc8ff, 0xff2f9be0);   // piano: ice blue
            case 4:  return mk (0xff45e3a6, 0xff1fb582);   // trumpet: emerald
            case 5:  return mk (0xffff9447, 0xffe0662a);   // bass: ember
            default: return mk (0xffffb84d, 0xffeb8b26);   // guitar: amber
        }
    }
    static const Theme& of (const juce::Component& c)
    {
        if (auto* l = dynamic_cast<EnochLookAndFeel*> (&c.getLookAndFeel())) return l->theme;
        static const Theme d = makeTheme (ENOCH_DEFAULT_THEME);
        return d;
    }

    Theme theme = makeTheme (ENOCH_DEFAULT_THEME);

    EnochLookAndFeel() { applyColours(); }
    void setInstrument (int inst) { theme = makeTheme (inst); applyColours(); }

    juce::Typeface::Ptr getTypefaceForFont (const juce::Font& f) override
    {
        if (f.getTypefaceName() == juce::Font::getDefaultSansSerifFontName() || f.getTypefaceName().isEmpty())
            if (auto t = EnochFontHost::faces().pick (f)) return t;
        return juce::LookAndFeel_V4::getTypefaceForFont (f);
    }

    void applyColours()
    {
        auto scheme = getDarkColourScheme();
        scheme.setUIColour (ColourScheme::windowBackground, theme.panel);
        scheme.setUIColour (ColourScheme::widgetBackground, theme.panel2);
        scheme.setUIColour (ColourScheme::menuBackground, theme.panel);
        scheme.setUIColour (ColourScheme::outline, theme.border);
        scheme.setUIColour (ColourScheme::defaultText, text());
        scheme.setUIColour (ColourScheme::highlightedFill, theme.accent);
        setColourScheme (scheme);
        setColour (juce::ResizableWindow::backgroundColourId, theme.bg);
        setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff0b0d0d));
        setColour (juce::ComboBox::textColourId, lcdText());
        setColour (juce::ComboBox::outlineColourId, theme.border);
        setColour (juce::ComboBox::arrowColourId, theme.accent);
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1c1e22));
        setColour (juce::PopupMenu::textColourId, text());
        setColour (juce::PopupMenu::headerTextColourId, theme.accent);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, theme.accent.withAlpha (0.2f));
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour (juce::TextButton::buttonColourId, theme.panel2);
        setColour (juce::TextButton::buttonOnColourId, theme.accent);
        setColour (juce::TextButton::textColourOffId, text());
        setColour (juce::TextButton::textColourOnId, juce::Colour (0xff1a1206));
        setColour (juce::Slider::thumbColourId, theme.accent);
        setColour (juce::Slider::trackColourId, theme.accent);
        setColour (juce::Slider::rotarySliderFillColourId, theme.accent);
        setColour (juce::Slider::rotarySliderOutlineColourId, theme.border);
        setColour (juce::Slider::backgroundColourId, theme.border);
        setColour (juce::Slider::textBoxTextColourId, lcdText());
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff0b0d0d));
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Label::textColourId, text());
        setColour (juce::ToggleButton::textColourId, text());
        setColour (juce::ToggleButton::tickColourId, theme.accent);
        setColour (juce::ScrollBar::thumbColourId, theme.accent.withAlpha (0.55f));
        setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xff1c1e22));
        setColour (juce::TooltipWindow::textColourId, text());
        setColour (juce::TooltipWindow::outlineColourId, theme.border);
        setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff0b0d0d));
        setColour (juce::TextEditor::textColourId, lcdText());
        setColour (juce::TextEditor::outlineColourId, theme.border);
        setColour (juce::TextEditor::focusedOutlineColourId, theme.accent);
        setColour (juce::TextEditor::highlightColourId, theme.accent.withAlpha (0.35f));
        setColour (juce::CaretComponent::caretColourId, theme.accent);
        setColour (juce::AlertWindow::backgroundColourId, theme.panel);
        setColour (juce::AlertWindow::textColourId, text());
        setColour (juce::AlertWindow::outlineColourId, theme.border);
        setColour (juce::BubbleComponent::backgroundColourId, juce::Colour (0xff0b0d0d));
        setColour (juce::BubbleComponent::outlineColourId, theme.accent.withAlpha (0.6f));
    }
    juce::Colour lcdText() const { return theme.accent.interpolatedWith (juce::Colours::white, 0.12f); }

    //================================================================== shared painting helpers (hardware parts)
    static juce::ColourGradient accentGradient (const Theme& t, juce::Rectangle<float> r, bool vertical = false)
    {
        return vertical ? juce::ColourGradient (t.accent.brighter (0.15f), r.getX(), r.getY(), t.accent2, r.getX(), r.getBottom(), false)
                        : juce::ColourGradient (t.accent.brighter (0.15f), r.getX(), r.getY(), t.accent2, r.getRight(), r.getBottom(), false);
    }
    /** a machined screw (cross-head), lit from the top-left */
    static void screw (juce::Graphics& g, juce::Point<float> c, float r, float turn = 0.4f)
    {
        juce::Path hole; hole.addEllipse (c.x - r - 1.f, c.y - r - 1.f, (r + 1.f) * 2.f, (r + 1.f) * 2.f);
        g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillPath (hole);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd9dce1), c.x - r, c.y - r, juce::Colour (0xff3b3e44), c.x + r, c.y + r, false));
        g.fillEllipse (c.x - r, c.y - r, r * 2.f, r * 2.f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff9a9fa7), c.x - r, c.y - r, juce::Colour (0xff5c6068), c.x + r, c.y + r, false));
        g.fillEllipse (c.x - r * 0.8f, c.y - r * 0.8f, r * 1.6f, r * 1.6f);
        for (int k = 0; k < 2; ++k)
        {
            const float a = turn + (float) k * juce::MathConstants<float>::halfPi;
            const auto d = juce::Point<float> (std::cos (a), std::sin (a)) * (r * 0.62f);
            g.setColour (juce::Colours::black.withAlpha (0.7f)); g.drawLine ({ c - d, c + d }, juce::jmax (1.f, r * 0.26f));
            g.setColour (juce::Colours::white.withAlpha (0.25f)); g.drawLine ({ c - d + juce::Point<float> (0.6f, 0.6f), c + d + juce::Point<float> (0.6f, 0.6f) }, 0.6f);
        }
    }
    /** a raised, machined plate with a bevelled edge and an engraved groove around it */
    static void plate (juce::Graphics& g, juce::Rectangle<float> r, const juce::Image& tex, float radius = 6.f, bool screws = true, float tone = 0.f)
    {
        if (r.isEmpty()) return;
        juce::Path p; p.addRoundedRectangle (r, radius);
        juce::DropShadow (juce::Colours::black.withAlpha (0.75f), 10, { 2, 4 }).drawForPath (g, p);
        {
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (p);
            if (tex.isValid()) g.drawImage (tex, r, juce::RectanglePlacement::stretchToFit);
            if (tone != 0.f) { g.setColour (tone > 0 ? juce::Colours::white.withAlpha (tone) : juce::Colours::black.withAlpha (-tone)); g.fillPath (p); }
            // soft top-left light falloff
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.07f), r.getX(), r.getY(), juce::Colours::black.withAlpha (0.18f), r.getRight(), r.getBottom(), false));
            g.fillPath (p);
        }
        // bevel: bright top-left edge, dark bottom-right edge
        juce::Path in; in.addRoundedRectangle (r.reduced (1.f), radius - 1.f);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.30f), r.getX(), r.getY(), juce::Colours::white.withAlpha (0.0f), r.getX() + 60.f, r.getY() + 60.f, false));
        g.strokePath (in, juce::PathStrokeType (1.2f));
        g.setColour (juce::Colours::black.withAlpha (0.85f)); g.strokePath (p, juce::PathStrokeType (1.f));
        if (screws && r.getWidth() > 60 && r.getHeight() > 40)
        {
            const float s = 8.5f, sr = 3.6f;
            screw (g, { r.getX() + s, r.getY() + s }, sr, 0.3f);  screw (g, { r.getRight() - s, r.getY() + s }, sr, 1.1f);
            screw (g, { r.getX() + s, r.getBottom() - s }, sr, 0.8f); screw (g, { r.getRight() - s, r.getBottom() - s }, sr, 0.1f);
        }
    }
    /** a recessed window (LCD / screen): dark glass sunk into the panel */
    static void recess (juce::Graphics& g, juce::Rectangle<float> r, float radius, juce::Colour glass, bool sheen = true)
    {
        juce::Path p; p.addRoundedRectangle (r, radius);
        g.setColour (glass); g.fillPath (p);
        {
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (p);
            // inner shadow from the top-left lip
            juce::Path ring; ring.addRectangle (r.expanded (8.f)); ring.addRoundedRectangle (r, radius); ring.setUsingNonZeroWinding (false);
            juce::DropShadow (juce::Colours::black.withAlpha (0.9f), 6, { 1, 2 }).drawForPath (g, ring);
            if (sheen)
            {
                juce::Path sh; sh.addRoundedRectangle (r.withHeight (r.getHeight() * 0.48f), radius);
                g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.065f), r.getX(), r.getY(), juce::Colours::white.withAlpha (0.f), r.getX(), r.getY() + r.getHeight() * 0.48f, false));
                g.fillPath (sh);
            }
        }
        // lip: dark top edge, light bottom edge
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.8f), r.getX(), r.getY(), juce::Colours::white.withAlpha (0.16f), r.getX(), r.getBottom(), false));
        g.strokePath (p, juce::PathStrokeType (1.2f));
    }
    /** silkscreen print on metal: crisp ink with the faintest shadow */
    static void ink (juce::Graphics& g, const juce::String& s, juce::Rectangle<float> r, const juce::Font& f, juce::Justification j, juce::Colour c = text())
    {
        g.setFont (f);
        g.setColour (juce::Colours::black.withAlpha (0.55f)); g.drawText (s, r.translated (0.f, 1.f), j, true);
        g.setColour (c); g.drawText (s, r, j, true);
    }
    /** engraved lettering: cut into the metal (dark top-left, light bottom-right) */
    static void engrave (juce::Graphics& g, const juce::String& s, juce::Rectangle<float> r, const juce::Font& f, juce::Justification j, juce::Colour fill)
    {
        g.setFont (f);
        g.setColour (juce::Colours::white.withAlpha (0.22f)); g.drawText (s, r.translated (0.8f, 1.0f), j, true);
        g.setColour (juce::Colours::black.withAlpha (0.8f)); g.drawText (s, r.translated (-0.6f, -0.8f), j, true);
        g.setColour (fill); g.drawText (s, r, j, true);
    }
    /** LED glow text for displays */
    static void ledText (juce::Graphics& g, const juce::String& s, juce::Rectangle<float> r, const juce::Font& f, juce::Justification j, juce::Colour c, int lines = 1)
    {
        g.setFont (f);
        g.setColour (c.withAlpha (0.18f));
        for (auto d : { juce::Point<float> (-0.8f, 0.f), juce::Point<float> (0.8f, 0.f), juce::Point<float> (0.f, -0.8f), juce::Point<float> (0.f, 0.8f) })
            g.drawFittedText (s, r.translated (d.x, d.y).toNearestInt(), j, lines, 0.8f);
        g.setColour (c); g.drawFittedText (s, r.toNearestInt(), j, lines, 0.8f);
    }
    /** a round LED lamp */
    static void led (juce::Graphics& g, juce::Point<float> c, float r, juce::Colour col, bool on)
    {
        g.setColour (juce::Colours::black.withAlpha (0.7f)); g.fillEllipse (c.x - r - 1.f, c.y - r - 1.f, (r + 1.f) * 2.f, (r + 1.f) * 2.f);
        if (on)
        {
            juce::Path p; p.addEllipse (c.x - r, c.y - r, r * 2.f, r * 2.f);
            juce::DropShadow (col.withAlpha (0.9f), (int) (r * 3.f), {}).drawForPath (g, p);
            g.setGradientFill (juce::ColourGradient (juce::Colours::white, c.x - r * 0.3f, c.y - r * 0.4f, col, c.x + r * 0.6f, c.y + r * 0.7f, true));
        }
        else g.setGradientFill (juce::ColourGradient (col.withMultipliedBrightness (0.35f).withSaturation (0.5f), c.x - r * 0.3f, c.y - r * 0.4f, juce::Colour (0xff17181a), c.x + r, c.y + r, true));
        g.fillEllipse (c.x - r, c.y - r, r * 2.f, r * 2.f);
        g.setColour (juce::Colours::white.withAlpha (on ? 0.8f : 0.25f)); g.fillEllipse (c.x - r * 0.45f, c.y - r * 0.55f, r * 0.5f, r * 0.4f);
    }

    // kept for the code that still uses the earlier helper names
    static void card (juce::Graphics& g, juce::Rectangle<float> r, const Theme&, float radius = 10.f, bool = true, float = 0.f) { recess (g, r, radius, juce::Colour (0xff0d0f10)); }
    static void caption (juce::Graphics& g, const juce::String& s, juce::Rectangle<int> r, const Theme&) { ink (g, s.toUpperCase(), r.toFloat(), fnt (10.5f, "SemiBold", 0.10f), juce::Justification::centredLeft); }
    static void gradientText (juce::Graphics& g, const juce::String& s, juce::Rectangle<float> r, const Theme& t, const juce::Font& f, juce::Justification j)
    {
        juce::GlyphArrangement ga; ga.addFittedText (f, s, r.getX(), r.getY(), r.getWidth(), r.getHeight(), j, 1, 1.f);
        juce::Path p; ga.createPath (p);
        g.setGradientFill (accentGradient (t, p.getBounds()));
        g.fillPath (p);
    }

    //================================================================== knobs
    /** machined knob: printed scale, backlit value channel, knurled skirt, spun-aluminium cap, LED pointer, value display underneath */
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s) override
    {
        const bool en = s.isEnabled();
        if (! en) g.beginTransparencyLayer (0.45f);
        const bool lcd = (bool) s.getProperties()["lcd"];
        auto area = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
        const float lcdH = lcd ? juce::jlimit (13.f, 17.f, (float) h * 0.2f) : 0.f;
        auto ka = area.withTrimmedBottom (lcd ? lcdH + 2.f : 0.f);
        const float D = juce::jmin (ka.getWidth(), ka.getHeight());
        const auto c = ka.getCentre();
        const float R = D * 0.5f;
        const float ang = a0 + pos * (a1 - a0);
        const float pi = juce::MathConstants<float>::pi;

        // printed scale: ticks (long every fifth) on the faceplate
        const int ticks = 11;
        for (int i = 0; i < ticks; ++i)
        {
            const float t = a0 + (a1 - a0) * (float) i / (float) (ticks - 1);
            const bool major = (i % 5 == 0);
            const auto p0 = c.getPointOnCircumference (R * (major ? 0.84f : 0.88f), t), p1 = c.getPointOnCircumference (R * 0.985f, t);
            g.setColour (juce::Colours::black.withAlpha (0.5f)); g.drawLine ({ p0.translated (0.f, 0.8f), p1.translated (0.f, 0.8f) }, major ? 1.6f : 1.f);
            g.setColour (text().withAlpha (major ? 0.85f : 0.55f)); g.drawLine ({ p0, p1 }, major ? 1.5f : 0.9f);
        }
        // backlit value channel
        {
            const float cr = R * 0.785f, cw = juce::jmax (2.f, R * 0.055f);
            juce::Path ch; ch.addCentredArc (c.x, c.y, cr, cr, 0.f, a0, a1, true);
            g.setColour (juce::Colours::black.withAlpha (0.85f)); g.strokePath (ch, juce::PathStrokeType (cw + 1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            if (pos > 0.003f)
            {
                juce::Path lit; lit.addCentredArc (c.x, c.y, cr, cr, 0.f, a0, ang, true);
                juce::Path ls; juce::PathStrokeType (cw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded).createStrokedPath (ls, lit);
                if (en) juce::DropShadow (theme.accent.withAlpha (0.75f), (int) juce::jmax (4.f, R * 0.22f), {}).drawForPath (g, ls);
                g.setColour (theme.accent); g.fillPath (ls);
                g.setColour (juce::Colours::white.withAlpha (0.35f)); g.strokePath (lit, juce::PathStrokeType (cw * 0.3f));
            }
        }
        // knob body
        const float kR = R * 0.70f;
        juce::Path body; body.addEllipse (c.x - kR, c.y - kR, kR * 2.f, kR * 2.f);
        juce::DropShadow (juce::Colours::black.withAlpha (0.85f), (int) juce::jmax (6.f, kR * 0.55f), { (int) juce::jmax (1.f, kR * 0.10f), (int) juce::jmax (2.f, kR * 0.22f) }).drawForPath (g, body);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff5b5f68), c.x - kR, c.y - kR, juce::Colour (0xff101113), c.x + kR, c.y + kR, false));
        g.fillPath (body);
        // knurled grip: ridges turn with the knob; each catches light by its facing
        {
            const int N = juce::jlimit (28, 60, (int) (kR * 1.5f));
            const float r0 = kR * 0.80f, r1 = kR * 0.985f;
            const float wdt = juce::jmax (0.8f, 2.f * pi * kR / (float) N * 0.48f);
            for (int i = 0; i < N; ++i)
            {
                const float t = ang + 2.f * pi * (float) i / (float) N;
                float l = 0.5f + 0.5f * std::cos (t - lightAngle);
                l = std::pow (l, 1.6f);
                g.setColour (juce::Colour (0xff0c0d0f).interpolatedWith (juce::Colour (0xffa9aeb7), l));
                g.drawLine ({ c.getPointOnCircumference (r0, t), c.getPointOnCircumference (r1, t) }, wdt);
            }
            g.setColour (juce::Colours::black.withAlpha (0.6f)); g.drawEllipse (c.x - kR, c.y - kR, kR * 2.f, kR * 2.f, 1.f);
        }
        // shoulder between the grip and the cap
        {
            const float sR = kR * 0.80f;
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1a1b1e), c.x - sR, c.y - sR, juce::Colour (0xff6c7079), c.x + sR, c.y + sR, false));
            g.fillEllipse (c.x - sR, c.y - sR, sR * 2.f, sR * 2.f);
        }
        // spun-aluminium cap: conical reflections
        {
            const float cR = kR * 0.72f;
            const int M = 72;
            for (int i = 0; i < M; ++i)
            {
                const float t0 = 2.f * pi * (float) i / (float) M, t1 = 2.f * pi * (float) (i + 1) / (float) M, tm = 0.5f * (t0 + t1);
                const float b = 0.50f + 0.20f * std::cos (2.f * (tm - lightAngle)) + 0.10f * std::cos (tm - lightAngle) + 0.03f * std::sin (7.f * tm + 0.5f);
                juce::Path wdg; wdg.startNewSubPath (c); wdg.lineTo (c.getPointOnCircumference (cR, t0)); wdg.lineTo (c.getPointOnCircumference (cR, t1 + 0.004f)); wdg.closeSubPath();
                g.setColour (juce::Colour::fromFloatRGBA (0.80f * b + 0.05f, 0.82f * b + 0.05f, 0.86f * b + 0.05f, 1.f));
                g.fillPath (wdg);
            }
            // concentric spin marks
            for (float rr = cR * 0.2f; rr < cR; rr += juce::jmax (1.2f, cR * 0.09f))
            { g.setColour (juce::Colours::black.withAlpha (0.045f)); g.drawEllipse (c.x - rr, c.y - rr, rr * 2.f, rr * 2.f, 0.6f); }
            // specular hotspot
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.45f), c.x - cR * 0.38f, c.y - cR * 0.42f, juce::Colours::white.withAlpha (0.f), c.x + cR * 0.3f, c.y + cR * 0.3f, true));
            g.fillEllipse (c.x - cR, c.y - cR, cR * 2.f, cR * 2.f);
            // chamfer
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.85f), c.x - cR, c.y - cR, juce::Colours::black.withAlpha (0.7f), c.x + cR * 0.7f, c.y + cR * 0.7f, false));
            g.drawEllipse (c.x - cR, c.y - cR, cR * 2.f, cR * 2.f, juce::jmax (1.f, cR * 0.07f));
        }
        // pointer: engraved groove with an LED line in it
        {
            const auto p0 = c.getPointOnCircumference (kR * 0.22f, ang), p1 = c.getPointOnCircumference (kR * 0.96f, ang);
            const float pw = juce::jmax (1.6f, kR * 0.085f);
            g.setColour (juce::Colours::black.withAlpha (0.75f)); g.drawLine ({ p0, p1 }, pw + 1.6f);
            juce::Path pl; pl.startNewSubPath (p0); pl.lineTo (p1);
            juce::Path ps; juce::PathStrokeType (pw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded).createStrokedPath (ps, pl);
            if (en) juce::DropShadow (theme.accent.withAlpha (0.8f), (int) juce::jmax (3.f, pw * 2.5f), {}).drawForPath (g, ps);
            g.setColour (theme.accent.interpolatedWith (juce::Colours::white, 0.25f)); g.fillPath (ps);
        }
        // low / high legends at the ends of the scale
        const juce::String lo = s.getProperties()["lo"].toString(), hi = s.getProperties()["hi"].toString();
        if (lo.isNotEmpty() || hi.isNotEmpty())
        {
            const auto f = fnt (juce::jlimit (7.5f, 8.5f, R * 0.2f), "Bold", 0.04f);
            const auto pl = c.getPointOnCircumference (R * 0.92f, a0), ph = c.getPointOnCircumference (R * 0.92f, a1);
            const float lw = juce::jmax (36.f, R * 1.1f);
            ink (g, lo, juce::Rectangle<float> (pl.x - lw * 0.5f, pl.y + 2.f, lw, 10.f), f, juce::Justification::centred, dim());
            ink (g, hi, juce::Rectangle<float> (ph.x - lw * 0.5f, ph.y + 2.f, lw, 10.f), f, juce::Justification::centred, dim());
        }
        // value display
        if (lcd)
        {
            const float lw = juce::jmin (area.getWidth() - 2.f, juce::jmax (40.f, R * 1.45f));
            auto lr = juce::Rectangle<float> (c.x - lw * 0.5f, area.getBottom() - lcdH, lw, lcdH);
            recess (g, lr, 3.f, juce::Colour (0xff070809));
            ledText (g, s.getTextFromValue (s.getValue()), lr.reduced (2.f, 0.f), fnt (juce::jmin (12.f, lcdH * 0.72f), "SemiBold"), juce::Justification::centred, lcdText());
        }
        if (! en) g.endTransparencyLayer();
    }

    //================================================================== faders
    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           const juce::Slider::SliderStyle style, juce::Slider& s) override
    {
        if (style != juce::Slider::LinearHorizontal && style != juce::Slider::LinearVertical)
        { juce::LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, minPos, maxPos, style, s); return; }
        const bool horiz = style == juce::Slider::LinearHorizontal, en = s.isEnabled();
        if (! en) g.beginTransparencyLayer (0.45f);
        const float th = 5.f;
        const float capL = juce::jlimit (9.f, 15.f, (horiz ? (float) h : (float) w) * 0.55f);           // along the travel
        const float capT = juce::jlimit (12.f, 24.f, (horiz ? (float) h : (float) w) - 2.f);            // across
        juce::Rectangle<float> slot = horiz ? juce::Rectangle<float> ((float) x + capL * 0.5f, (float) y + (float) h * 0.5f - th * 0.5f, (float) w - capL, th)
                                            : juce::Rectangle<float> ((float) x + (float) w * 0.5f - th * 0.5f, (float) y + capL * 0.5f, th, (float) h - capL);
        // scale marks beside the slot
        if ((horiz ? h : w) >= 18)
            for (int i = 0; i <= 10; ++i)
            {
                const float f = (float) i / 10.f;
                g.setColour (text().withAlpha (i % 5 == 0 ? 0.55f : 0.28f));
                if (horiz) { const float xx = slot.getX() + slot.getWidth() * f; g.fillRect (xx - 0.5f, slot.getY() - (i % 5 == 0 ? 5.f : 3.5f), 1.f, i % 5 == 0 ? 3.5f : 2.f); }
                else       { const float yy = slot.getBottom() - slot.getHeight() * f; g.fillRect (slot.getX() - (i % 5 == 0 ? 5.f : 3.5f), yy - 0.5f, i % 5 == 0 ? 3.5f : 2.f, 1.f); }
            }
        recess (g, slot, th * 0.5f, juce::Colour (0xff050607), false);
        // lit travel
        auto lit = horiz ? slot.withRight (juce::jlimit (slot.getX(), slot.getRight(), pos)).reduced (0.f, 1.2f)
                         : slot.withTop (juce::jlimit (slot.getY(), slot.getBottom(), pos)).reduced (1.2f, 0.f);
        if (! lit.isEmpty()) { g.setColour (theme.accent.withAlpha (0.85f)); g.fillRoundedRectangle (lit, 1.5f); }
        // fader cap
        juce::Rectangle<float> cap = horiz ? juce::Rectangle<float> (pos - capL * 0.5f, (float) y + (float) h * 0.5f - capT * 0.5f, capL, capT)
                                           : juce::Rectangle<float> ((float) x + (float) w * 0.5f - capT * 0.5f, pos - capL * 0.5f, capT, capL);
        juce::Path cp; cp.addRoundedRectangle (cap, 2.5f);
        juce::DropShadow (juce::Colours::black.withAlpha (0.8f), 6, { 1, 3 }).drawForPath (g, cp);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe9ebee), cap.getX(), cap.getY(), juce::Colour (0xff6d727a), cap.getRight(), cap.getBottom(), false));
        g.fillPath (cp);
        // grip ridges + LED line
        g.setColour (juce::Colours::black.withAlpha (0.28f));
        if (horiz) for (int i = -1; i <= 1; i += 2) g.fillRect (cap.getCentreX() + (float) i * capL * 0.28f - 0.5f, cap.getY() + 3.f, 1.f, cap.getHeight() - 6.f);
        else       for (int i = -1; i <= 1; i += 2) g.fillRect (cap.getX() + 3.f, cap.getCentreY() + (float) i * capL * 0.28f - 0.5f, cap.getWidth() - 6.f, 1.f);
        g.setColour (theme.accent.darker (0.2f));
        if (horiz) g.fillRect (cap.getCentreX() - 1.f, cap.getY() + 2.f, 2.f, cap.getHeight() - 4.f); else g.fillRect (cap.getX() + 2.f, cap.getCentreY() - 1.f, cap.getWidth() - 4.f, 2.f);
        g.setColour (juce::Colours::white.withAlpha (0.7f)); g.drawLine (cap.getX() + 1.5f, cap.getY() + 1.f, cap.getRight() - 1.5f, cap.getY() + 1.f, 0.8f);
        g.setColour (juce::Colours::black.withAlpha (0.8f)); g.drawRoundedRectangle (cap, 2.5f, 0.8f);
        if (! en) g.endTransparencyLayer();
    }
    int getSliderThumbRadius (juce::Slider& s) override { return juce::jlimit (5, 8, (s.isHorizontal() ? s.getHeight() : s.getWidth()) / 4); }
    juce::Label* createSliderTextBox (juce::Slider& s) override
    {
        auto* l = juce::LookAndFeel_V4::createSliderTextBox (s);
        l->setFont (fnt (12.f, "SemiBold"));
        l->setColour (juce::Label::backgroundColourId, juce::Colour (0xff070809));
        l->setColour (juce::Label::textColourId, lcdText());
        l->setColour (juce::Label::outlineColourId, juce::Colours::black);
        return l;
    }

    //================================================================== buttons: machined push buttons, illuminated when on
    static bool isPrimary (juce::Button& b)
    {
        if (b.getProperties()["seg"]) return false;
        const auto c = b.findColour (juce::TextButton::buttonColourId);
        return c.getSaturation() > 0.35f && c.getBrightness() > 0.45f;
    }
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (2.f, 2.f);
        const float rad = juce::jmin (5.f, r.getHeight() * 0.3f);
        const bool lit = b.getToggleState() || isPrimary (b), en = b.isEnabled();
        if (! en) g.beginTransparencyLayer (0.5f);
        juce::Path p; p.addRoundedRectangle (r, rad);
        // housing hole + contact shadow
        juce::Path hole; hole.addRoundedRectangle (r.expanded (1.5f), rad + 1.f);
        g.setColour (juce::Colours::black.withAlpha (0.65f)); g.fillPath (hole);
        if (! down) juce::DropShadow (juce::Colours::black.withAlpha (0.7f), 5, { 1, 2 }).drawForPath (g, p);
        if (down) { r.translate (0.f, 0.8f); p.applyTransform (juce::AffineTransform::translation (0.f, 0.8f)); }
        if (lit)
        {
            const auto c = b.getToggleState() ? theme.accent : b.findColour (juce::TextButton::buttonColourId);
            if (en) juce::DropShadow (c.withAlpha (over ? 0.7f : 0.5f), 14, {}).drawForPath (g, p);
            g.setGradientFill (juce::ColourGradient (c.brighter (over ? 0.55f : 0.4f), r.getX(), r.getY(), c.darker (0.35f), r.getX(), r.getBottom(), false));
            g.fillPath (p);
            // light diffusing through the translucent cap
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.35f), r.getCentreX(), r.getY() + r.getHeight() * 0.35f, juce::Colours::white.withAlpha (0.f), r.getCentreX(), r.getBottom() + r.getHeight(), true));
            g.fillPath (p);
        }
        else
        {
            g.setGradientFill (juce::ColourGradient (juce::Colour (over ? 0xff595e67 : 0xff4b4f57), r.getX(), r.getY(), juce::Colour (0xff25272c), r.getX(), r.getBottom(), false));
            g.fillPath (p);
            if (down) { g.setColour (juce::Colours::black.withAlpha (0.25f)); g.fillPath (p); }
        }
        // bevel highlights
        g.setColour (juce::Colours::white.withAlpha (lit ? 0.6f : 0.22f)); g.drawLine (r.getX() + rad, r.getY() + 0.8f, r.getRight() - rad, r.getY() + 0.8f, 1.f);
        g.setColour (juce::Colours::black.withAlpha (0.55f)); g.drawLine (r.getX() + rad, r.getBottom() - 0.6f, r.getRight() - rad, r.getBottom() - 0.6f, 1.f);
        g.setColour (juce::Colours::black.withAlpha (0.9f)); g.strokePath (p, juce::PathStrokeType (0.8f));
        if (! en) g.endTransparencyLayer();
    }
    juce::Font getTextButtonFont (juce::TextButton& b, int h) override
    {
        if (b.getProperties()["seg"]) return fnt (juce::jmin (14.f, (float) h * 0.42f), "Bold", 0.06f);
        return fnt (juce::jmin (13.f, (float) h * 0.48f), "SemiBold", 0.03f);
    }
    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool down) override
    {
        const bool lit = b.getToggleState() || isPrimary (b);
        auto col = lit ? juce::Colour (0xff1a1206) : b.findColour (juce::TextButton::textColourOffId);
        if (! b.isEnabled()) col = col.withMultipliedAlpha (0.5f);
        auto r = b.getLocalBounds().reduced (6, 2).toFloat();
        if (down) r.translate (0.f, 0.8f);
        const juce::String icon = b.getProperties()["icon"].toString();
        if (icon == "play")
        {
            const bool stop = b.getButtonText().startsWithIgnoreCase ("STOP");
            const auto f = fnt (juce::jmin (16.f, (float) b.getHeight() * 0.32f), "Bold", 0.10f);
            const float tw = (float) juce::GlyphArrangement::getStringWidthInt (f, b.getButtonText());
            const float iz = juce::jmin (15.f, (float) b.getHeight() * 0.3f), gap = 9.f;
            const float x0 = r.getCentreX() - (iz + gap + tw) * 0.5f, cy = r.getCentreY();
            g.setColour (col);
            if (stop) g.fillRoundedRectangle (x0, cy - iz * 0.45f, iz * 0.9f, iz * 0.9f, 1.5f);
            else { juce::Path t; t.addTriangle (x0, cy - iz * 0.55f, x0, cy + iz * 0.55f, x0 + iz * 0.95f, cy); g.fillPath (t); }
            g.setFont (f); g.drawText (b.getButtonText(), juce::Rectangle<float> (x0 + iz + gap, r.getY(), tw + 4.f, r.getHeight()), juce::Justification::centredLeft, false);
            return;
        }
        g.setFont (getTextButtonFont (b, b.getHeight()));
        if (! lit) { g.setColour (juce::Colours::black.withAlpha (0.6f)); g.drawFittedText (b.getButtonText(), r.translated (0.f, 1.f).toNearestInt(), juce::Justification::centred, 1, 0.75f); }
        g.setColour (col);
        g.drawFittedText (b.getButtonText(), r.toNearestInt(), juce::Justification::centred, 1, 0.75f);
    }

    //================================================================== toggles: chrome bat switch + status LED
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool) override
    {
        const auto bounds = b.getLocalBounds().toFloat();
        const bool on = b.getToggleState(), en = b.isEnabled();
        if (! en) g.beginTransparencyLayer (0.45f);
        const float h = juce::jlimit (16.f, 26.f, bounds.getHeight() + 2.f);
        const auto c = juce::Point<float> (bounds.getX() + h * 0.5f, bounds.getCentreY());
        const float nr = h * 0.34f;
        // hex nut on a threaded bushing
        juce::Path nut; nut.addPolygon (c, 6, nr, 0.26f);
        juce::DropShadow (juce::Colours::black.withAlpha (0.85f), 4, { 1, 2 }).drawForPath (g, nut);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffeef0f3), c.x - nr, c.y - nr, juce::Colour (0xff444850), c.x + nr, c.y + nr, false));
        g.fillPath (nut);
        g.setColour (juce::Colours::black.withAlpha (0.75f)); g.strokePath (nut, juce::PathStrokeType (0.7f));
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1f2125), c.x - nr, c.y - nr, juce::Colour (0xffb2b7bf), c.x + nr, c.y + nr, false));
        g.fillEllipse (c.x - nr * 0.6f, c.y - nr * 0.6f, nr * 1.2f, nr * 1.2f);
        // chrome bat lever: thrown up = on, down = off
        const float dir = on ? -1.f : 1.f;
        const auto tip = c + juce::Point<float> (0.f, dir * h * 0.40f);
        const float bw = nr * 0.62f, tw = nr * 0.9f;
        juce::Path lever;
        lever.startNewSubPath (c.x - bw * 0.5f, c.y); lever.lineTo (tip.x - tw * 0.42f, tip.y); lever.lineTo (tip.x + tw * 0.42f, tip.y); lever.lineTo (c.x + bw * 0.5f, c.y); lever.closeSubPath();
        juce::DropShadow (juce::Colours::black.withAlpha (0.75f), 3, { 2, 2 }).drawForPath (g, lever);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff5b6068), c.x - tw * 0.5f, c.y, juce::Colour (0xffffffff), c.x + tw * 0.05f, c.y, false));
        g.fillPath (lever);
        g.setColour (juce::Colours::black.withAlpha (0.45f)); g.strokePath (lever, juce::PathStrokeType (0.6f));
        g.setGradientFill (juce::ColourGradient (juce::Colours::white, tip.x - tw * 0.25f, tip.y - tw * 0.25f, juce::Colour (0xff6a6f77), tip.x + tw * 0.55f, tip.y + tw * 0.55f, true));
        g.fillEllipse (tip.x - tw * 0.5f, tip.y - tw * 0.5f, tw, tw);
        g.setColour (juce::Colours::black.withAlpha (0.55f)); g.drawEllipse (tip.x - tw * 0.5f, tip.y - tw * 0.5f, tw, tw, 0.7f);
        // status LED
        const float lx = c.x + h * 0.78f;
        led (g, { lx, c.y }, juce::jmax (2.5f, h * 0.14f), theme.accent, on);
        // label
        auto tr = bounds.withTrimmedLeft (lx - bounds.getX() + h * 0.3f);
        ink (g, b.getButtonText(), tr, fnt (juce::jmin (13.f, bounds.getHeight() * 0.58f), "SemiBold"), juce::Justification::centredLeft,
             (over ? text() : text().withAlpha (on ? 1.f : 0.8f)));
        if (! en) g.endTransparencyLayer();
    }

    //================================================================== combo boxes: LCD windows
    void drawComboBox (juce::Graphics& g, int w, int h, bool down, int, int, int, int, juce::ComboBox& c) override
    {
        juce::Rectangle<float> r (1.f, 1.f, (float) w - 2.f, (float) h - 2.f);
        const bool en = c.isEnabled();
        recess (g, r, 4.f, juce::Colour (0xff0a0c0c).interpolatedWith (theme.accent, 0.035f));
        if (c.isMouseOver (true) && en) { g.setColour (theme.accent.withAlpha (0.35f)); g.drawRoundedRectangle (r.reduced (1.f), 3.f, 1.f); }
        if (down) { g.setColour (theme.accent.withAlpha (0.15f)); g.fillRoundedRectangle (r.reduced (1.5f), 3.f); }
        // selector arrow
        const float cx = (float) w - 11.f, cy = (float) h * 0.5f, s = juce::jmin (3.6f, (float) h * 0.14f);
        juce::Path tri; tri.addTriangle (cx - s, cy - s * 0.55f, cx + s, cy - s * 0.55f, cx, cy + s * 0.75f);
        g.setColour (theme.accent.withAlpha (en ? 0.9f : 0.35f)); g.fillPath (tri);
    }
    juce::Font getComboBoxFont (juce::ComboBox& c) override { return fnt (juce::jmin (13.5f, (float) c.getHeight() * 0.52f), "SemiBold"); }
    void positionComboBoxText (juce::ComboBox& c, juce::Label& l) override
    {
        l.setBounds (5, 1, c.getWidth() - 24, c.getHeight() - 2);
        l.setFont (getComboBoxFont (c));
        l.setMinimumHorizontalScale (0.75f);
    }
    void drawLabel (juce::Graphics& g, juce::Label& l) override
    {
        // the text inside combo boxes glows like an LCD; every other label is silkscreen on metal
        if (dynamic_cast<juce::ComboBox*> (l.getParentComponent()) != nullptr && ! l.isBeingEdited())
        {
            const float a = l.isEnabled() ? 1.f : 0.45f;
            ledText (g, l.getText(), l.getLocalBounds().toFloat().reduced (2.f, 0.f), getLabelFont (l), l.getJustificationType(), lcdText().withMultipliedAlpha (a));
            return;
        }
        if (! l.isBeingEdited())
        {
            g.fillAll (l.findColour (juce::Label::backgroundColourId));
            const float a = l.isEnabled() ? 1.f : 0.5f;
            const auto f = getLabelFont (l);
            g.setFont (f);
            const auto tr = getLabelBorderSize (l).subtractedFrom (l.getLocalBounds());
            const int lines = juce::jmax (1, (int) ((float) tr.getHeight() / f.getHeight()));
            g.setColour (juce::Colours::black.withAlpha (0.55f * a));
            g.drawFittedText (l.getText(), tr.translated (0, 1), l.getJustificationType(), lines, l.getMinimumHorizontalScale());
            g.setColour (l.findColour (juce::Label::textColourId).withMultipliedAlpha (a));
            g.drawFittedText (l.getText(), tr, l.getJustificationType(), lines, l.getMinimumHorizontalScale());
            g.setColour (l.findColour (juce::Label::outlineColourId)); g.drawRect (l.getLocalBounds());
        }
        else juce::LookAndFeel_V4::drawLabel (g, l);
    }
    juce::Font getLabelFont (juce::Label& l) override { return l.getFont(); }

    //================================================================== popup menus
    void drawPopupMenuBackground (juce::Graphics& g, int w, int h) override
    {
        g.fillAll (juce::Colour (0xff1b1d21));
        g.setColour (juce::Colours::white.withAlpha (0.02f));
        for (int y = 0; y < h; y += 3) g.drawHorizontalLine (y, 0.f, (float) w);
        g.setColour (theme.border); g.drawRect (0, 0, w, h, 1);
    }
    juce::Font getPopupMenuFont() override { return fnt (14.f, "Regular"); }
    void drawPopupMenuSectionHeader (juce::Graphics& g, const juce::Rectangle<int>& area, const juce::String& name) override
    {
        auto r = area.reduced (10, 0).withTrimmedTop (5);
        g.setColour (theme.accent.withAlpha (0.25f)); g.fillRect (r.getX(), r.getBottom() - 2, r.getWidth(), 1);
        g.setColour (theme.accent); g.setFont (fnt (10.5f, "Bold", 0.14f));
        g.drawFittedText (name.toUpperCase(), r, juce::Justification::centredLeft, 1);
    }
    void drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator, bool isActive, bool isHighlighted, bool isTicked,
                            bool hasSubMenu, const juce::String& txt, const juce::String& shortcut, const juce::Drawable* icon, const juce::Colour* textCol) override
    {
        if (isSeparator) { g.setColour (theme.border); g.fillRect (area.reduced (10, 0).withSizeKeepingCentre (area.getWidth() - 20, 1)); return; }
        auto r = area.reduced (4, 1);
        if (isHighlighted && isActive)
        {
            g.setColour (theme.accent.withAlpha (0.18f)); g.fillRoundedRectangle (r.toFloat(), 4.f);
            g.setColour (theme.accent); g.fillRoundedRectangle ((float) r.getX(), (float) r.getY() + 4.f, 3.f, (float) r.getHeight() - 8.f, 1.5f);
        }
        auto col = textCol != nullptr ? *textCol : text();
        if (! isActive) col = dim().withAlpha (0.6f);
        if (isHighlighted && isActive) col = juce::Colours::white;
        auto tr = r.withTrimmedLeft (22).withTrimmedRight (8);
        if (isTicked) led (g, { (float) r.getX() + 11.f, (float) r.getCentreY() }, 3.5f, theme.accent, true);
        if (icon != nullptr) icon->drawWithin (g, r.removeFromLeft (r.getHeight()).toFloat().reduced (4.f), juce::RectanglePlacement::centred, 1.f);
        g.setColour (col); g.setFont (getPopupMenuFont());
        if (hasSubMenu)
        {
            const float cx = (float) tr.getRight() - 4.f, cy = (float) tr.getCentreY();
            juce::Path ch; ch.startNewSubPath (cx - 3.f, cy - 4.f); ch.lineTo (cx + 1.f, cy); ch.lineTo (cx - 3.f, cy + 4.f);
            g.strokePath (ch, juce::PathStrokeType (1.5f));
            tr.removeFromRight (12);
        }
        if (shortcut.isNotEmpty()) { g.setColour (dim()); g.drawText (shortcut, tr, juce::Justification::centredRight, true); g.setColour (col); }
        g.drawFittedText (txt, tr, juce::Justification::centredLeft, 1, 0.8f);
    }
    void getIdealPopupMenuItemSize (const juce::String& t, bool sep, int standardHeight, int& w, int& h) override
    {
        juce::LookAndFeel_V4::getIdealPopupMenuItemSize (t, sep, standardHeight, w, h);
        if (! sep) h = juce::jmax (h, 25);
        w += 26;
    }

    //================================================================== text editors (LCD), scrollbars, tooltips
    void fillTextEditorBackground (juce::Graphics& g, int w, int h, juce::TextEditor&) override
    {
        recess (g, juce::Rectangle<float> (1.f, 1.f, (float) w - 2.f, (float) h - 2.f), 4.f, juce::Colour (0xff0a0c0c).interpolatedWith (theme.accent, 0.035f));
    }
    void drawTextEditorOutline (juce::Graphics& g, int w, int h, juce::TextEditor& e) override
    {
        if (e.hasKeyboardFocus (true)) { g.setColour (theme.accent.withAlpha (0.6f)); g.drawRoundedRectangle (1.5f, 1.5f, (float) w - 3.f, (float) h - 3.f, 3.5f, 1.f); }
    }
    void drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int w, int h, bool vertical, int ts, int tsz, bool over, bool down) override
    {
        auto track = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (1.f);
        recess (g, track, juce::jmin (track.getWidth(), track.getHeight()) * 0.5f, juce::Colour (0xff08090a), false);
        juce::Rectangle<float> t = vertical ? juce::Rectangle<float> ((float) x + 2.f, (float) ts + 1.f, (float) w - 4.f, (float) tsz - 2.f)
                                            : juce::Rectangle<float> ((float) ts + 1.f, (float) y + 2.f, (float) tsz - 2.f, (float) h - 4.f);
        const float rr = juce::jmin (t.getWidth(), t.getHeight()) * 0.5f;
        g.setGradientFill (juce::ColourGradient (juce::Colour (down ? 0xffe9ebee : (over ? 0xffd5d8dd : 0xffb9bdc4)), t.getX(), t.getY(), juce::Colour (0xff5d6168), t.getRight(), t.getBottom(), false));
        g.fillRoundedRectangle (t, rr);
        g.setColour (theme.accent.withAlpha (0.8f));
        if (vertical) g.fillRect (t.getCentreX() - 0.5f, t.getCentreY() - 6.f, 1.f, 12.f); else g.fillRect (t.getCentreX() - 6.f, t.getCentreY() - 0.5f, 12.f, 1.f);
    }
    void drawTooltip (juce::Graphics& g, const juce::String& t, int w, int h) override
    {
        juce::Rectangle<float> r (0.f, 0.f, (float) w, (float) h);
        g.setColour (juce::Colour (0xff15171a)); g.fillRoundedRectangle (r, 5.f);
        g.setColour (theme.accent.withAlpha (0.55f)); g.drawRoundedRectangle (r.reduced (0.5f), 5.f, 1.f);
        juce::AttributedString s; s.setJustification (juce::Justification::centredLeft); s.append (t, fnt (13.5f), text());
        juce::TextLayout tl; tl.createLayoutWithBalancedLineLengths (s, (float) w - 16.f);
        tl.draw (g, r.reduced (8.f, 5.f));
    }
    juce::Rectangle<int> getTooltipBounds (const juce::String& t, juce::Point<int> pos, juce::Rectangle<int> parent) override
    {
        juce::AttributedString s; s.append (t, fnt (13.5f), text());
        juce::TextLayout tl; tl.createLayoutWithBalancedLineLengths (s, 420.f);
        const int w = (int) (tl.getWidth() + 18.f), h = (int) (tl.getHeight() + 12.f);
        return juce::Rectangle<int> (pos.x > parent.getCentreX() ? pos.x - (w + 12) : pos.x + 24, pos.y > parent.getCentreY() ? pos.y - (h + 6) : pos.y + 6, w, h).constrainedWithin (parent);
    }
};
