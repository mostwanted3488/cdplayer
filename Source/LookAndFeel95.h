#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace w95
{
    const juce::Colour face   { 0xffc0c0c0 };
    const juce::Colour light  { 0xffffffff };
    const juce::Colour light2 { 0xffdfdfdf };
    const juce::Colour shadow { 0xff808080 };
    const juce::Colour dark   { 0xff0a0a0a };
    const juce::Colour navy   { 0xff000080 };
    const juce::Colour title  { 0xff0000a8 };
    const juce::Colour tip    { 0xffffffe1 };

    void raised (juce::Graphics& g, juce::Rectangle<int> r, bool fill = true);
    void sunken (juce::Graphics& g, juce::Rectangle<int> r, juce::Colour fillCol = juce::Colours::white, bool fill = true);
    void thinSunken (juce::Graphics& g, juce::Rectangle<int> r);
    void pressed (juce::Graphics& g, juce::Rectangle<int> r);
    void etchedLine (juce::Graphics& g, int x1, int x2, int y);
    juce::Font uiFont (float size, bool bold = false);
    juce::Font titleFont (float size);
    void drawCdIcon (juce::Graphics& g, juce::Rectangle<float> r);
    void drawCross (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c = juce::Colours::black);
}

class LookAndFeel95 : public juce::LookAndFeel_V4
{
public:
    LookAndFeel95();

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool over, bool down) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>& area, bool isSeparator, bool isActive, bool isHighlighted,
                            bool isTicked, bool hasSubMenu, const juce::String& text, const juce::String& shortcutKeyText,
                            const juce::Drawable* icon, const juce::Colour* textColour) override;
    juce::Font getPopupMenuFont() override;
    void getIdealPopupMenuItemSize (const juce::String& text, bool isSeparator, int standardMenuItemHeight, int& w, int& h) override;
    int getPopupMenuBorderSize() override { return 4; }
    void fillTextEditorBackground (juce::Graphics&, int w, int h, juce::TextEditor&) override;
    void drawTextEditorOutline (juce::Graphics&, int w, int h, juce::TextEditor&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override;
    void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int x, int y, int w, int h, bool vertical, int thumbStart, int thumbSize, bool over, bool down) override;
    void drawTooltip (juce::Graphics&, const juce::String& text, int w, int h) override;
};

// Small square Win95 button with a drawn glyph (close, minimise, maximise, restore, play…)
class IconButton95 : public juce::Button
{
public:
    enum Kind { close, minimise, maximise, restore, play, pause, stop, rew, ff };
    explicit IconButton95 (Kind k) : juce::Button ({}), kind (k) {}
    void setKind (Kind k) { kind = k; repaint(); }
    Kind kind;
    void paintButton (juce::Graphics&, bool over, bool down) override;
};
