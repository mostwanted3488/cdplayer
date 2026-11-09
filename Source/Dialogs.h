#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"
#include "LookAndFeel95.h"

using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

// Win95 window living inside the plugin editor (draggable by its title bar).
class Win95Dialog : public juce::Component
{
public:
    Win95Dialog (const juce::String& title, bool closable = true);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    juce::Rectangle<int> content() const { return getLocalBounds().reduced (6).withTrimmedTop (30); }
    virtual void layout() {}
    void open();
    std::function<void()> onClose;
    juce::String title;
protected:
    IconButton95 closeBtn { IconButton95::close };
    juce::ComponentDragger dragger;
    bool draggingTitle = false;
};

// A label + Win95 slider + value box, bound to one parameter.
class ParamRow : public juce::Component
{
public:
    ParamRow (CDPlayerAudioProcessor& p, const juce::String& id, const juce::String& name, bool bold = false);
    void resized() override;
    void paint (juce::Graphics&) override;
    juce::Slider slider;
private:
    juce::Label nameL, valueL;
    juce::RangedAudioParameter* param;
    std::unique_ptr<SliderAtt> att;
};

class HintBox : public juce::Component
{
public:
    explicit HintBox (const juce::String& t) : text (t) {}
    void paint (juce::Graphics&) override;
    juce::String text;
};

class FxDialog : public Win95Dialog
{
public:
    explicit FxDialog (CDPlayerAudioProcessor& p);
    void layout() override;
    void paint (juce::Graphics&) override;
    std::function<void()> onShuffle, onReset;
private:
    CDPlayerAudioProcessor& proc;
    juce::OwnedArray<juce::TextButton> tabs;
    juce::OwnedArray<juce::Component> panes;
    juce::OwnedArray<ParamRow> rows;
    juce::OwnedArray<juce::ComboBox> combos;
    juce::OwnedArray<juce::Label> labels;
    juce::OwnedArray<juce::ToggleButton> toggles;
    juce::OwnedArray<HintBox> hints;
    juce::OwnedArray<juce::Component> hintsOwner;
    std::vector<std::unique_ptr<ComboAtt>> comboAtts;
    std::vector<std::unique_ptr<ButtonAtt>> btnAtts;
    juce::ComboBox lookCombo;
    juce::TextButton shuffleBtn { "Shuffle" }, resetBtn { "Reset" }, okBtn { "OK" };
    int current = 0;
    void showTab (int i);
    juce::Rectangle<int> sheet;
};

class EqDialog : public Win95Dialog
{
public:
    explicit EqDialog (CDPlayerAudioProcessor& p);
    void layout() override;
    void paint (juce::Graphics&) override;
    std::function<void (const juce::String&)> onToast;
private:
    CDPlayerAudioProcessor& proc;
    juce::OwnedArray<juce::Slider> sliders;
    juce::OwnedArray<juce::Label> freqs;
    std::vector<std::unique_ptr<SliderAtt>> atts;
    juce::OwnedArray<juce::TextButton> presetBtns;
    juce::TextButton okBtn { "OK" };
    juce::Rectangle<int> well;
};

class PresetsDialog : public Win95Dialog, private juce::ListBoxModel
{
public:
    explicit PresetsDialog (CDPlayerAudioProcessor& p);
    void layout() override;
    void refresh();
    std::function<void (const juce::String&)> onToast;
private:
    CDPlayerAudioProcessor& proc;
    juce::ListBox list;
    juce::TextEditor name;
    juce::Label nameL;
    juce::TextButton loadBtn { "Load" }, saveBtn { "Save" }, delBtn { "Delete" }, closeB { "Close" };
    juce::StringArray items; int numFactory = 0;
    static juce::File folder();
    int getNumRows() override { return items.size(); }
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
    void loadSelected();
};

class ErrorDialog : public Win95Dialog
{
public:
    ErrorDialog();
    void layout() override;
    void paint (juce::Graphics&) override;
    void setMessage (const juce::String& m) { msg = m; repaint(); }
    std::function<void()> onRetry, onIgnore, onCancel;
private:
    juce::String msg;
    juce::TextButton retry { "Retry" }, ignore { "Ignore" }, cancel { "Cancel" };
};

class ColorsDialog : public Win95Dialog
{
public:
    explicit ColorsDialog (CDPlayerAudioProcessor& p);
    void layout() override;
    void paint (juce::Graphics&) override;
private:
    CDPlayerAudioProcessor& proc;
    juce::TextButton bgBtn { "Background" }, waveBtn { "Waveform" }, headBtn { "Playhead / grid" }, okBtn { "OK" };
    void pick (const juce::String& key, juce::Component& anchor);
};

class AboutDialog : public Win95Dialog
{
public:
    AboutDialog();
    void layout() override;
    void paint (juce::Graphics&) override;
private:
    juce::TextButton okBtn { "OK" };
};
