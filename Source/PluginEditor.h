#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "LookAndFeel95.h"
#include "Display.h"
#include "Dialogs.h"

class CDPlayerAudioProcessorEditor;

// ---------- pieces of the main window ----------
class TitleBar : public juce::Component
{
public:
    TitleBar();
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDoubleClick (const juce::MouseEvent&) override { if (onDouble) onDouble(); }
    IconButton95 minBtn { IconButton95::minimise }, maxBtn { IconButton95::maximise }, closeBtn { IconButton95::close };
    std::function<void()> onDouble;
};

class MenuBar95 : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    std::function<juce::PopupMenu (int)> getMenu;
    std::function<void (int, int)> onPick;   // menu index, item id
    int openIndex = -1;
private:
    juce::Rectangle<int> itemRect (int i) const;
};

class SeekBar : public juce::Component
{
public:
    explicit SeekBar (CDPlayerAudioProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
private:
    CDPlayerAudioProcessor& proc;
    void seekTo (float x);
};

class Pad : public juce::Component
{
public:
    Pad (int i) : index (i) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    std::function<void (int, bool)> onPress;   // index, clear?
    std::function<void (int)> onRelease;
    int index; bool set = false, down = false; juce::String timeText { "empty" };
};

class StatusBar : public juce::Component
{
public:
    explicit StatusBar (CDPlayerAudioProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;
    juce::String left, right;
    juce::Slider mix;
private:
    std::unique_ptr<SliderAtt> mixAtt;
    juce::Label mixVal;
    juce::RangedAudioParameter* mixParam;
};

// ---------- the editor ----------
class CDPlayerAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit CDPlayerAudioProcessorEditor (CDPlayerAudioProcessor&);
    ~CDPlayerAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    bool keyStateChanged (bool isKeyDown) override;

private:
    CDPlayerAudioProcessor& proc;
    LookAndFeel95 lnf;
    juce::TooltipWindow tooltips { this, 600 };

    static constexpr int baseW = 590, baseH = 522, titleH = 42;
    juce::Component content;
    TitleBar titleBar;
    MenuBar95 menuBar;
    Display display;
    IconButton95 playBtn { IconButton95::pause }, stopBtn { IconButton95::stop }, rewBtn { IconButton95::rew }, ffBtn { IconButton95::ff };
    SeekBar seek;
    juce::OwnedArray<Pad> pads;
    juce::TextButton resetBtn { "RESET" };
    StatusBar status;

    FxDialog fxDlg;
    EqDialog eqDlg;
    PresetsDialog presetDlg;
    ErrorDialog errDlg;
    ColorsDialog colorsDlg;
    AboutDialog aboutDlg;

    bool shaded = false, big = false;
    double nextErr = 0;
    bool fDown = false, arrowL = false, arrowR = false;
    bool keyCueDown[8] {};

    void timerCallback() override;
    void layoutContent();
    void applyWindowSize();
    juce::PopupMenu buildMenu (int index);
    void menuPick (int menu, int id);
    void openDialog (Win95Dialog& d, int x, int y);
    void toast (const juce::String& t) { display.toast (t); }
    void toggleFx (const char* id);
    void setParam (const char* id, float v);
    float getParam (const char* id) const;
    void shuffle();
    void resetAll();
    void padPress (int i, bool clear);
    void padRelease (int i);
    void setSkin (const juce::String& s);
    void showError();
    void closeError (double delaySeconds);
    juce::String activeFx() const;
};
