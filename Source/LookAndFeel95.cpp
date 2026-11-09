#include "LookAndFeel95.h"

using namespace juce;

namespace w95
{
    void raised (Graphics& g, Rectangle<int> r, bool fill)
    {
        if (fill) { g.setColour (face); g.fillRect (r); }
        auto x = r.getX(), y = r.getY(), w = r.getWidth(), h = r.getHeight();
        g.setColour (light);  g.fillRect (x, y, w - 1, 2);   g.fillRect (x, y, 2, h - 1);
        g.setColour (dark);   g.fillRect (x, y + h - 2, w, 2); g.fillRect (x + w - 2, y, 2, h);
        g.setColour (light2); g.fillRect (x + 2, y + 2, w - 5, 2); g.fillRect (x + 2, y + 2, 2, h - 5);
        g.setColour (shadow); g.fillRect (x + 2, y + h - 4, w - 4, 2); g.fillRect (x + w - 4, y + 2, 2, h - 4);
    }
    void pressed (Graphics& g, Rectangle<int> r)
    {
        g.setColour (face); g.fillRect (r);
        auto x = r.getX(), y = r.getY(), w = r.getWidth(), h = r.getHeight();
        g.setColour (dark);   g.fillRect (x, y, w - 1, 2);   g.fillRect (x, y, 2, h - 1);
        g.setColour (light);  g.fillRect (x, y + h - 2, w, 2); g.fillRect (x + w - 2, y, 2, h);
        g.setColour (shadow); g.fillRect (x + 2, y + 2, w - 5, 2); g.fillRect (x + 2, y + 2, 2, h - 5);
        g.setColour (light2); g.fillRect (x + 2, y + h - 4, w - 4, 2); g.fillRect (x + w - 4, y + 2, 2, h - 4);
    }
    void sunken (Graphics& g, Rectangle<int> r, Colour fillCol, bool fill)
    {
        if (fill) { g.setColour (fillCol); g.fillRect (r); }
        auto x = r.getX(), y = r.getY(), w = r.getWidth(), h = r.getHeight();
        g.setColour (shadow); g.fillRect (x, y, w - 1, 2);   g.fillRect (x, y, 2, h - 1);
        g.setColour (light);  g.fillRect (x, y + h - 2, w, 2); g.fillRect (x + w - 2, y, 2, h);
        g.setColour (dark);   g.fillRect (x + 2, y + 2, w - 5, 2); g.fillRect (x + 2, y + 2, 2, h - 5);
        g.setColour (light2); g.fillRect (x + 2, y + h - 4, w - 4, 2); g.fillRect (x + w - 4, y + 2, 2, h - 4);
    }
    void thinSunken (Graphics& g, Rectangle<int> r)
    {
        auto x = r.getX(), y = r.getY(), w = r.getWidth(), h = r.getHeight();
        g.setColour (shadow); g.fillRect (x, y, w - 1, 2); g.fillRect (x, y, 2, h - 1);
        g.setColour (light);  g.fillRect (x, y + h - 2, w, 2); g.fillRect (x + w - 2, y, 2, h);
    }
    void etchedLine (Graphics& g, int x1, int x2, int y)
    {
        g.setColour (shadow); g.fillRect (x1, y, x2 - x1, 2);
        g.setColour (light);  g.fillRect (x1, y + 2, x2 - x1, 2);
    }
    // first installed font from a list (Tahoma / Arial exist on Windows and macOS)
    static String pickFont (std::initializer_list<const char*> names)
    {
        static StringArray installed = Font::findAllTypefaceNames();
        for (auto* n : names) if (installed.contains (n, true)) return n;
        return {};
    }
    Font uiFont (float size, bool bold)
    {
        static const String name = pickFont ({ "Tahoma", "Verdana", "Segoe UI", "Arial", "DejaVu Sans", "Liberation Sans" });
        auto o = FontOptions().withHeight (size).withStyle (bold ? "Bold" : "Regular");
        return Font (name.isNotEmpty() ? o.withName (name) : o);
    }
    Font titleFont (float size)
    {
        static const String name = pickFont ({ "Arial", "Helvetica", "Liberation Sans", "DejaVu Sans" });
        auto o = FontOptions().withHeight (size).withStyle ("Bold");
        return Font (name.isNotEmpty() ? o.withName (name) : o);
    }
    void drawCdIcon (Graphics& g, Rectangle<float> r)
    {
        auto c = r.getCentre();
        ColourGradient cg (Colour (0xfff8fff4), r.getX() + r.getWidth() * 0.4f, r.getY() + r.getHeight() * 0.35f,
                           Colour (0xff7cc890), r.getRight(), r.getBottom(), true);
        cg.addColour (0.35, Colour (0xff9fe0a8));
        cg.addColour (0.6, Colour (0xff58b0d8));
        cg.addColour (0.8, Colour (0xffd9a0e8));
        g.setGradientFill (cg);
        g.fillEllipse (r);
        g.setColour (Colour (0xff2a4a2a)); g.drawEllipse (r, 1.2f);
        Path sheen; sheen.addPieSegment (r, 0.0f, MathConstants<float>::halfPi, 0.42f);
        g.setColour (Colours::white.withAlpha (0.55f)); g.fillPath (sheen);
        auto hub = Rectangle<float> (r.getWidth() * 0.32f, r.getHeight() * 0.32f).withCentre (c);
        g.setColour (face); g.fillEllipse (hub);
        g.setColour (Colour (0xff333333)); g.drawEllipse (hub, 1.0f);
        g.setColour (title); g.fillEllipse (Rectangle<float> (r.getWidth() * 0.12f, r.getHeight() * 0.12f).withCentre (c));
    }
    void drawCross (Graphics& g, Rectangle<float> r, Colour c)
    {
        g.setColour (c);
        g.drawLine (r.getX(), r.getY(), r.getRight(), r.getBottom(), 2.4f);
        g.drawLine (r.getRight(), r.getY(), r.getX(), r.getBottom(), 2.4f);
    }
}

LookAndFeel95::LookAndFeel95()
{
    setColour (PopupMenu::backgroundColourId, w95::face);
    setColour (PopupMenu::textColourId, Colours::black);
    setColour (PopupMenu::highlightedBackgroundColourId, w95::navy);
    setColour (PopupMenu::highlightedTextColourId, Colours::white);
    setColour (TextButton::textColourOffId, Colours::black);
    setColour (TextButton::textColourOnId, Colours::black);
    setColour (ToggleButton::textColourId, Colours::black);
    setColour (ComboBox::textColourId, Colours::black);
    setColour (ComboBox::backgroundColourId, Colours::white);
    setColour (TextEditor::textColourId, Colours::black);
    setColour (TextEditor::backgroundColourId, Colours::white);
    setColour (TextEditor::highlightColourId, w95::navy);
    setColour (TextEditor::highlightedTextColourId, Colours::white);
    setColour (CaretComponent::caretColourId, Colours::black);
    setColour (Label::textColourId, Colours::black);
    setColour (ListBox::backgroundColourId, Colours::white);
    setColour (ScrollBar::thumbColourId, w95::face);
    setColour (TooltipWindow::backgroundColourId, w95::tip);
    setColour (TooltipWindow::textColourId, Colours::black);
    setColour (TooltipWindow::outlineColourId, Colours::black);
    if (auto tf = w95::uiFont (14).getTypefacePtr()) setDefaultSansSerifTypeface (tf);
}

void LookAndFeel95::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool, bool down)
{
    auto r = b.getLocalBounds();
    if (down || b.getToggleState()) w95::pressed (g, r); else w95::raised (g, r);
}

Font LookAndFeel95::getTextButtonFont (TextButton&, int h) { return w95::uiFont (jmin (14.0f, h * 0.5f)); }

void LookAndFeel95::drawButtonText (Graphics& g, TextButton& b, bool, bool down)
{
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour (b.findColour (TextButton::textColourOffId));
    auto r = b.getLocalBounds().reduced (4, 2);
    if (down || b.getToggleState()) r.translate (1, 1);
    g.drawFittedText (b.getButtonText(), r, Justification::centred, 1);
}

void LookAndFeel95::drawToggleButton (Graphics& g, ToggleButton& b, bool, bool)
{
    auto box = Rectangle<int> (0, (b.getHeight() - 18) / 2, 18, 18);
    w95::sunken (g, box, Colours::white);
    if (b.getToggleState())
    {
        Path tick;
        tick.startNewSubPath ((float) box.getX() + 4.5f, (float) box.getCentreY());
        tick.lineTo ((float) box.getX() + 8.0f, (float) box.getBottom() - 5.0f);
        tick.lineTo ((float) box.getRight() - 4.5f, (float) box.getY() + 5.0f);
        g.setColour (Colours::black);
        g.strokePath (tick, PathStrokeType (2.2f));
    }
    g.setColour (Colours::black);
    g.setFont (w95::uiFont (14));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().withTrimmedLeft (26), Justification::centredLeft, 2);
}

void LookAndFeel95::drawLinearSlider (Graphics& g, int x, int y, int w, int h, float pos, float, float, Slider::SliderStyle style, Slider&)
{
    if (style == Slider::LinearVertical)
    {
        int cx = x + w / 2;
        w95::sunken (g, { cx - 3, y, 6, h }, Colours::black.withAlpha (0.0f), false);
        g.setColour (Colours::black); g.fillRect (x, y + h / 2, w, 1);
        auto t = Rectangle<int> (x + 1, (int) pos - 6, w - 2, 12);
        w95::raised (g, t);
        return;
    }
    int cy = y + h / 2 - 2;
    w95::sunken (g, { x, cy - 2, w, 6 }, w95::face, false);
    // ticks
    g.setColour (Colours::black);
    for (int k = 0; k <= 10; ++k) { int tx = x + (int) std::round (k * (w - 1) / 10.0); g.fillRect (tx, y + h - 4, 1, 3); }
    // pointed thumb
    float tx = pos;
    Path th;
    float tw = 7.0f, top = (float) cy - 10, bot = (float) cy + 8;
    th.startNewSubPath (tx - tw, top); th.lineTo (tx + tw, top); th.lineTo (tx + tw, bot - 5); th.lineTo (tx, bot + 1); th.lineTo (tx - tw, bot - 5); th.closeSubPath();
    g.setColour (w95::face); g.fillPath (th);
    g.setColour (Colours::white); g.drawLine (tx - tw, top, tx + tw - 1, top, 2.0f); g.drawLine (tx - tw, top, tx - tw, bot - 5, 2.0f);
    g.setColour (w95::dark); g.drawLine (tx + tw, top, tx + tw, bot - 5, 2.0f); g.drawLine (tx + tw, bot - 5, tx, bot + 1, 2.0f);
    g.setColour (w95::shadow); g.drawLine (tx - tw, bot - 5, tx, bot + 1, 1.5f);
}

void LookAndFeel95::drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox&)
{
    w95::sunken (g, { 0, 0, w, h }, Colours::white);
    auto btn = Rectangle<int> (w - h + 2, 2, h - 4, h - 4);
    w95::raised (g, btn);
    Path tri; auto c = btn.getCentre().toFloat();
    tri.addTriangle (c.x - 4, c.y - 2, c.x + 4, c.y - 2, c.x, c.y + 3);
    g.setColour (Colours::black); g.fillPath (tri);
}
Font LookAndFeel95::getComboBoxFont (ComboBox&) { return w95::uiFont (14); }
void LookAndFeel95::positionComboBoxText (ComboBox& box, Label& label)
{
    label.setBounds (4, 1, box.getWidth() - box.getHeight() - 4, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

void LookAndFeel95::drawPopupMenuBackground (Graphics& g, int w, int h) { w95::raised (g, { 0, 0, w, h }); }

Font LookAndFeel95::getPopupMenuFont() { return w95::uiFont (14); }

void LookAndFeel95::getIdealPopupMenuItemSize (const String& text, bool isSeparator, int, int& w, int& h)
{
    if (isSeparator) { w = 50; h = 10; return; }
    h = 26;
    GlyphArrangement ga; ga.addLineOfText (getPopupMenuFont(), text, 0, 0);
    w = (int) ga.getBoundingBox (0, -1, true).getWidth() + 80;
}

void LookAndFeel95::drawPopupMenuItem (Graphics& g, const Rectangle<int>& area, bool isSeparator, bool isActive, bool isHighlighted,
                                       bool isTicked, bool hasSubMenu, const String& text, const String& shortcutKeyText,
                                       const Drawable*, const Colour*)
{
    if (isSeparator) { w95::etchedLine (g, area.getX() + 2, area.getRight() - 2, area.getCentreY() - 2); return; }
    auto r = area.reduced (2, 0);
    if (isHighlighted && isActive) { g.setColour (w95::navy); g.fillRect (r); }
    auto tc = ! isActive ? w95::shadow : (isHighlighted ? Colours::white : Colours::black);
    g.setColour (tc);
    g.setFont (getPopupMenuFont());
    if (isTicked)
    {
        Path tick; float cx = (float) r.getX() + 13, cy = (float) r.getCentreY();
        tick.startNewSubPath (cx - 5, cy); tick.lineTo (cx - 1.5f, cy + 4); tick.lineTo (cx + 5, cy - 5);
        g.strokePath (tick, PathStrokeType (2.0f));
    }
    auto tr = r.withTrimmedLeft (28).withTrimmedRight (hasSubMenu ? 24 : 12);
    g.drawFittedText (text, tr, Justification::centredLeft, 1);
    if (shortcutKeyText.isNotEmpty()) g.drawText (shortcutKeyText, tr, Justification::centredRight);
    if (hasSubMenu)
    {
        Path tri; float x = (float) r.getRight() - 14, y = (float) r.getCentreY();
        tri.addTriangle (x, y - 4, x, y + 4, x + 4, y);
        g.fillPath (tri);
    }
}

void LookAndFeel95::fillTextEditorBackground (Graphics& g, int w, int h, TextEditor&) { w95::sunken (g, { 0, 0, w, h }, Colours::white); }
void LookAndFeel95::drawTextEditorOutline (Graphics&, int, int, TextEditor&) {}

void LookAndFeel95::drawScrollbar (Graphics& g, ScrollBar&, int x, int y, int w, int h, bool vertical, int thumbStart, int thumbSize, bool, bool)
{
    g.setColour (Colour (0xffe0e0e0)); g.fillRect (x, y, w, h);
    if (thumbSize > 0)
        w95::raised (g, vertical ? Rectangle<int> (x, thumbStart, w, thumbSize) : Rectangle<int> (thumbStart, y, thumbSize, h));
}

void LookAndFeel95::drawTooltip (Graphics& g, const String& text, int w, int h)
{
    g.fillAll (w95::tip);
    g.setColour (Colours::black); g.drawRect (0, 0, w, h);
    g.setFont (w95::uiFont (13));
    g.drawFittedText (text, Rectangle<int> (0, 0, w, h).reduced (4, 2), Justification::centredLeft, 3);
}

void IconButton95::paintButton (Graphics& g, bool, bool down)
{
    auto r = getLocalBounds();
    if (down || getToggleState()) w95::pressed (g, r); else w95::raised (g, r);
    auto c = r.toFloat().getCentre();
    if (down) c += Point<float> (1, 1);
    g.setColour (Colours::black);
    switch (kind)
    {
        case close:    w95::drawCross (g, Rectangle<float> (9, 8).withCentre (c)); break;
        case minimise: g.fillRect (c.x - 5, c.y + 3, 8.0f, 2.4f); break;
        case maximise: g.drawRect (Rectangle<float> (11, 10).withCentre (c), 1.6f); g.fillRect (Rectangle<float> (11, 2.6f).withPosition (c.x - 5.5f, c.y - 5)); break;
        case restore:
        {
            auto a = Rectangle<float> (8, 6).withPosition (c.x - 2, c.y - 5), b = Rectangle<float> (8, 6).withPosition (c.x - 6, c.y - 1);
            g.drawRect (a, 1.3f); g.fillRect (a.withHeight (2.0f));
            g.setColour (w95::face); g.fillRect (b); g.setColour (Colours::black); g.drawRect (b, 1.3f); g.fillRect (b.withHeight (2.0f));
            break;
        }
        case play:  { Path t; t.addTriangle (c.x - 5, c.y - 7, c.x - 5, c.y + 7, c.x + 6, c.y); g.fillPath (t); break; }
        case pause: g.fillRect (c.x - 6, c.y - 6, 4.0f, 12.0f); g.fillRect (c.x + 1, c.y - 6, 4.0f, 12.0f); break;
        case stop:  g.setColour (w95::shadow); g.fillRect (Rectangle<float> (8, 8).withCentre (c)); break;
        case rew:
        {
            Path t; t.addTriangle (c.x + 1, c.y - 6, c.x + 1, c.y + 6, c.x - 6, c.y); t.addTriangle (c.x + 8, c.y - 6, c.x + 8, c.y + 6, c.x + 1, c.y);
            g.fillPath (t); g.fillRect (c.x - 8, c.y - 6, 2.0f, 12.0f); break;
        }
        case ff:
        {
            Path t; t.addTriangle (c.x - 8, c.y - 6, c.x - 8, c.y + 6, c.x - 1, c.y); t.addTriangle (c.x - 1, c.y - 6, c.x - 1, c.y + 6, c.x + 6, c.y);
            g.fillPath (t); g.fillRect (c.x + 6, c.y - 6, 2.0f, 12.0f); break;
        }
    }
}
