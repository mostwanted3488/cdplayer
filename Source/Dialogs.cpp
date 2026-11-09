#include "Dialogs.h"

using namespace juce;

// ===================== Win95Dialog =====================
Win95Dialog::Win95Dialog (const String& t, bool closable) : title (t)
{
    if (closable)
    {
        addAndMakeVisible (closeBtn);
        closeBtn.onClick = [this] { setVisible (false); if (onClose) onClose(); };
    }
    addMouseListener (this, true);
    setVisible (false);
}

void Win95Dialog::paint (Graphics& g)
{
    w95::raised (g, getLocalBounds());
    auto tb = Rectangle<int> (5, 5, getWidth() - 10, 26);
    g.setColour (w95::title); g.fillRect (tb);
    g.setColour (Colours::white); g.setFont (w95::titleFont (16));
    g.drawText (title, tb.withTrimmedLeft (6), Justification::centredLeft);
}

void Win95Dialog::resized()
{
    closeBtn.setBounds (getWidth() - 34, 8, 24, 20);
    layout();
}

void Win95Dialog::mouseDown (const MouseEvent& e)
{
    toFront (true);
    auto le = e.getEventRelativeTo (this);
    draggingTitle = e.eventComponent == this && le.y < 32;
    if (draggingTitle) dragger.startDraggingComponent (this, le);
}

void Win95Dialog::mouseDrag (const MouseEvent& e)
{
    if (! draggingTitle) return;
    ComponentBoundsConstrainer c; c.setMinimumOnscreenAmounts (30, 80, 30, 80);
    dragger.dragComponent (this, e.getEventRelativeTo (this), &c);
}

void Win95Dialog::open() { setVisible (true); toFront (true); }

// ===================== ParamRow =====================
ParamRow::ParamRow (CDPlayerAudioProcessor& p, const String& id, const String& name, bool bold)
{
    param = p.apvts.getParameter (id);
    nameL.setText (name, dontSendNotification);
    nameL.setFont (w95::uiFont (14, bold));
    addAndMakeVisible (nameL);
    slider.setSliderStyle (Slider::LinearHorizontal);
    slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible (slider);
    att = std::make_unique<SliderAtt> (p.apvts, id, slider);
    slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
    valueL.setJustificationType (Justification::centredRight);
    valueL.setFont (w95::uiFont (13));
    addAndMakeVisible (valueL);
    auto upd = [this] { valueL.setText (param->getCurrentValueAsText(), dontSendNotification); };
    slider.onValueChange = upd;
    upd();
}

void ParamRow::resized()
{
    auto r = getLocalBounds();
    nameL.setBounds (r.removeFromLeft (122));
    valueL.setBounds (r.removeFromRight (60).reduced (2, 3));
    slider.setBounds (r.reduced (4, 0));
}

void ParamRow::paint (Graphics& g)
{
    auto vb = getLocalBounds().removeFromRight (60).reduced (0, 2);
    g.setColour (Colours::white); g.fillRect (vb);
    w95::thinSunken (g, vb);
}

void HintBox::paint (Graphics& g)
{
    g.fillAll (w95::tip);
    g.setColour (Colours::black); g.drawRect (getLocalBounds());
    g.setFont (w95::uiFont (12.5f));
    g.setColour (Colour (0xff202020));
    g.drawFittedText (text, getLocalBounds().reduced (7, 5), Justification::topLeft, 6);
}

namespace
{
    // stacks its children vertically
    struct Pane : public Component
    {
        std::vector<std::pair<Component*, int>> items;
        void add (Component& c, int h) { addAndMakeVisible (c); items.push_back ({ &c, h }); }
        void resized() override { int y = 0; for (auto& it : items) { it.first->setBounds (0, y, getWidth(), it.second); y += it.second + 6; } }
    };
    struct Sep : public Component { void paint (Graphics& g) override { w95::etchedLine (g, 0, getWidth(), getHeight() / 2 - 2); } };
    // "Label [combo]  Label [combo]" indented under a slider
    struct ComboLine : public Component
    {
        std::vector<std::pair<Label*, ComboBox*>> pairs;
        void resized() override
        {
            int x = 126;
            for (auto& pr : pairs)
            {
                int lw = GlyphArrangement::getStringWidthInt (w95::uiFont (14), pr.first->getText()) + 8;
                pr.first->setBounds (x, 0, lw, getHeight()); x += lw;
                pr.second->setBounds (x, 0, 128, getHeight()); x += 138;
            }
        }
    };
}

// ===================== FxDialog =====================
FxDialog::FxDialog (CDPlayerAudioProcessor& p) : Win95Dialog ("Effects Properties"), proc (p)
{
    const char* tabNames[] = { "Disc damage", "Sound", "Space", "Play" };
    for (int i = 0; i < 4; ++i)
    {
        auto* t = tabs.add (new TextButton (tabNames[i]));
        t->setClickingTogglesState (false);
        t->onClick = [this, i] { showTab (i); };
        addAndMakeVisible (t);
        auto* pane = panes.add (new Pane());
        addChildComponent (pane);
    }
    auto pane = [this] (int i) -> Pane& { return *static_cast<Pane*> (panes[i]); };
    auto row = [this, &p] (const char* id, const char* nm, bool bold = false) -> ParamRow& { return *rows.add (new ParamRow (p, id, nm, bold)); };
    auto hint = [this] (const char* t) -> HintBox& { return *hints.add (new HintBox (String::fromUTF8 (t))); };
    auto combo = [this, &p] (const char* id) -> ComboBox&
    {
        auto* c = combos.add (new ComboBox());
        if (auto* prm = dynamic_cast<AudioParameterChoice*> (p.apvts.getParameter (id)))
            c->addItemList (prm->choices, 1);
        comboAtts.push_back (std::make_unique<ComboAtt> (p.apvts, id, *c));
        return *c;
    };
    auto label = [this] (const char* t) -> Label& { auto* l = labels.add (new Label ({}, t)); l->setFont (w95::uiFont (14)); return *l; };
    auto toggle = [this, &p] (const char* id, const char* t) -> ToggleButton&
    {
        auto* b = toggles.add (new ToggleButton (t));
        btnAtts.push_back (std::make_unique<ButtonAtt> (p.apvts, id, *b));
        return *b;
    };
    auto line = [this] (std::vector<std::pair<Label*, ComboBox*>> prs) -> Component&
    {
        auto* cl = new ComboLine(); cl->pairs = prs;
        for (auto& pr : prs) { cl->addAndMakeVisible (pr.first); cl->addAndMakeVisible (pr.second); }
        hintsOwner.add (cl);
        return *cl;
    };

    // Disc damage
    pane (0).add (row (pid::damage, "DAMAGE", true), 30);
    pane (0).add (*hintsOwner.add (new Sep()), 8);
    pane (0).add (row (pid::skip, "CD Skip"), 30);
    pane (0).add (row (pid::scratch, "Scratched Disc"), 30);
    pane (0).add (row (pid::battery, "Low Battery"), 30);
    pane (0).add (row (pid::tape, "Tape stop"), 30);
    pane (0).add (hint ("DAMAGE wrecks the disc progressively (skip + scratches + battery + lo-fi + headphones). Push Scratched Disc past 85% and Windows starts complaining\xe2\x80\xa6"), 62);
    // Sound
    pane (1).add (row (pid::crush, "Lo-Fi Laser"), 30);
    pane (1).add (row (pid::bass, "MEGA BASS"), 30);
    pane (1).add (row (pid::hp, "Headphones"), 30);
    pane (1).add (row (pid::width, "Stereo width"), 30);
    pane (1).add (row (pid::vol, "Volume"), 30);
    pane (1).add (row (pid::mix, "Dry / Wet"), 30);
    pane (1).add (hint ("Headphones = cheap wired earbuds: thin, narrow, crackling \xe2\x80\x94 and a loose jack that cuts one side now and then."), 46);
    // Space
    pane (2).add (row (pid::esp, "Anti-Shock (ESP)"), 30);
    pane (2).add (line ({ { &label ("Echo"), &combo (pid::espDiv) } }), 26);
    pane (2).add (row (pid::room, "Room"), 30);
    pane (2).add (line ({ { &label ("Space"), &combo (pid::roomType) } }), 26);
    pane (2).add (row (pid::gate, "Gate / Tremolo"), 30);
    pane (2).add (line ({ { &label ("Rate"), &combo (pid::gateDiv) } }), 26);
    pane (2).add (line ({ { &label ("Shape"), &combo (pid::gateShape) } }), 26);
    // Play
    pane (3).add (row (pid::speed, "Speed"), 30);
    pane (3).add (row (pid::pitch, "Pitch"), 30);
    pane (3).add (toggle (pid::rev, "Reverse playback"), 24);
    pane (3).add (toggle (pid::slip, "Slip mode (return in time after scratch / cue / freeze)"), 24);
    pane (3).add (toggle (pid::quant, "Snap freeze / skip / cues to the beat grid"), 24);
    pane (3).add (row (pid::bpm, "BPM (no host)"), 30);
    lookCombo.addItemList ({ "Off (0 ms)", "0.5 s", "1 s", "2 s" }, 1);
    lookCombo.setSelectedItemIndex (p.getLookaheadIndex(), dontSendNotification);
    lookCombo.onChange = [this] { proc.setLookaheadIndex (lookCombo.getSelectedItemIndex()); };
    pane (3).add (line ({ { &label ("Lookahead"), &lookCombo } }), 26);
    pane (3).add (hint ("In FL Studio the tempo stays locked: Speed bends the pitch like a tape. Lookahead (off by default = zero latency) lets you see and scratch ahead; FL then delays the project by that amount."), 62);

    for (auto* b : { &shuffleBtn, &resetBtn, &okBtn }) addAndMakeVisible (b);
    shuffleBtn.onClick = [this] { if (onShuffle) onShuffle(); };
    resetBtn.onClick = [this] { if (onReset) onReset(); };
    okBtn.onClick = [this] { setVisible (false); };
    setSize (452, 492);
    showTab (0);
}

void FxDialog::showTab (int i)
{
    current = i;
    for (int k = 0; k < panes.size(); ++k) panes[k]->setVisible (k == i);
    for (int k = 0; k < tabs.size(); ++k) tabs[k]->setToggleState (k == i, dontSendNotification);
    layout();
    repaint();
}

void FxDialog::layout()
{
    auto c = content();
    int x = c.getX() + 8;
    for (int k = 0; k < tabs.size(); ++k)
    {
        int tw = GlyphArrangement::getStringWidthInt (w95::uiFont (14), tabs[k]->getButtonText()) + 24;
        bool on = k == current;
        tabs[k]->setBounds (x, c.getY() + 8 + (on ? 0 : 3), tw, on ? 30 : 27);
        x += tw - 2;
    }
    sheet = Rectangle<int> (c.getX() + 8, c.getY() + 36, c.getWidth() - 16, c.getHeight() - 36 - 48);
    for (auto* p : panes) p->setBounds (sheet.reduced (14, 12));
    int by = c.getBottom() - 38;
    okBtn.setBounds (c.getRight() - 104, by, 96, 32);
    resetBtn.setBounds (c.getRight() - 208, by, 96, 32);
    shuffleBtn.setBounds (c.getRight() - 312, by, 96, 32);
}

void FxDialog::paint (Graphics& g)
{
    Win95Dialog::paint (g);
    w95::raised (g, sheet);
}

// ===================== EqDialog =====================
EqDialog::EqDialog (CDPlayerAudioProcessor& p) : Win95Dialog ("Graphic Equalizer"), proc (p)
{
    for (int i = 0; i < kNumEq; ++i)
    {
        auto* s = sliders.add (new Slider (Slider::LinearVertical, Slider::NoTextBox));
        addAndMakeVisible (s);
        atts.push_back (std::make_unique<SliderAtt> (p.apvts, pid::eq (i), *s));
        s->setDoubleClickReturnValue (true, 0.0);
        auto f = kEqFreqs[i];
        auto* l = freqs.add (new Label ({}, f >= 1000 ? String (f / 1000.0f) + "k" : String ((int) f)));
        l->setJustificationType (Justification::centred); l->setFont (w95::uiFont (12));
        addAndMakeVisible (l);
        s->onDragEnd = [this, i] { if (onToast) onToast ("EQ " + freqs[i]->getText() + " Hz: " + proc.apvts.getParameter (pid::eq (i))->getCurrentValueAsText()); };
    }
    struct P { const char* n; float v[kNumEq]; };
    static const P ps[] = { { "Flat", { 0,0,0,0,0,0,0,0,0,0 } }, { "Smile", { 6,5,3,0,-2,-2,0,3,5,6 } },
                            { "Radio", { -12,-10,-6,0,4,6,4,-4,-10,-12 } }, { "Sub", { 9,8,4,0,-1,0,0,0,1,2 } } };
    for (auto& pp : ps)
    {
        auto* b = presetBtns.add (new TextButton (pp.n));
        addAndMakeVisible (b);
        const P* ptr = &pp;
        b->onClick = [this, ptr]
        {
            for (int i = 0; i < kNumEq; ++i)
                if (auto* prm = proc.apvts.getParameter (pid::eq (i))) prm->setValueNotifyingHost (prm->convertTo0to1 (ptr->v[i]));
            if (onToast) onToast (String ("EQ: ") + ptr->n);
        };
    }
    addAndMakeVisible (okBtn);
    okBtn.onClick = [this] { setVisible (false); };
    setSize (470, 330);
}

void EqDialog::layout()
{
    auto c = content();
    well = Rectangle<int> (c.getX() + 8, c.getY() + 10, c.getWidth() - 16, 220);
    auto inner = well.reduced (12, 10).withTrimmedLeft (30);
    int colW = inner.getWidth() / kNumEq;
    for (int i = 0; i < kNumEq; ++i)
    {
        sliders[i]->setBounds (inner.getX() + i * colW + (colW - 26) / 2, inner.getY(), 26, 170);
        freqs[i]->setBounds (inner.getX() + i * colW, inner.getY() + 174, colW, 18);
    }
    int by = c.getBottom() - 38, x = c.getX() + 8;
    for (auto* b : presetBtns) { b->setBounds (x, by, 66, 32); x += 72; }
    okBtn.setBounds (c.getRight() - 90, by, 82, 32);
}

void EqDialog::paint (Graphics& g)
{
    Win95Dialog::paint (g);
    w95::sunken (g, well, w95::face);
    g.setColour (Colours::black); g.setFont (w95::uiFont (11));
    auto sc = Rectangle<int> (well.getX() + 6, well.getY() + 10, 30, 170);
    g.drawText ("+12", sc.withHeight (14), Justification::centredRight);
    g.drawText ("0", sc.withY (sc.getCentreY() - 7).withHeight (14), Justification::centredRight);
    g.drawText (String::fromUTF8 ("\xe2\x88\x92" "12"), sc.withY (sc.getBottom() - 14).withHeight (14), Justification::centredRight);
}

// ===================== PresetsDialog =====================
File PresetsDialog::folder()
{
    return File::getSpecialLocation (File::userApplicationDataDirectory).getChildFile ("CD Player FX").getChildFile ("Presets");
}

PresetsDialog::PresetsDialog (CDPlayerAudioProcessor& p) : Win95Dialog ("Presets"), proc (p)
{
    list.setModel (this);
    list.setRowHeight (22);
    list.setColour (ListBox::outlineColourId, Colours::transparentBlack);
    addAndMakeVisible (list);
    nameL.setText ("Name:", dontSendNotification); nameL.setFont (w95::uiFont (14));
    addAndMakeVisible (nameL);
    name.setFont (w95::uiFont (14));
    name.setTextToShowWhenEmpty ("My preset", w95::shadow);
    addAndMakeVisible (name);
    for (auto* b : { &loadBtn, &saveBtn, &delBtn, &closeB }) addAndMakeVisible (b);
    loadBtn.onClick = [this] { loadSelected(); };
    saveBtn.onClick = [this]
    {
        auto n = File::createLegalFileName (name.getText().trim());
        if (n.isEmpty()) { if (onToast) onToast ("Type a name first"); return; }
        for (auto& fp : factoryPresets()) if (n == fp.name) { if (onToast) onToast ("Factory name - pick another"); return; }
        folder().createDirectory();
        auto st = proc.apvts.copyState();
        if (auto xml = st.createXml()) xml->writeTo (folder().getChildFile (n + ".cdfx"));
        refresh();
        list.selectRow (items.indexOf (n));
        if (onToast) onToast ("Saved: " + n);
    };
    delBtn.onClick = [this]
    {
        int r = list.getSelectedRow();
        if (r < numFactory) { if (onToast) onToast ("Factory presets stay"); return; }
        folder().getChildFile (items[r] + ".cdfx").deleteFile();
        refresh();
    };
    closeB.onClick = [this] { setVisible (false); };
    setSize (400, 360);
    refresh();
}

void PresetsDialog::refresh()
{
    items.clear();
    for (auto& fp : factoryPresets()) items.add (fp.name);
    numFactory = items.size();
    auto files = folder().findChildFiles (File::findFiles, false, "*.cdfx");
    files.sort();
    for (auto& f : files) items.add (f.getFileNameWithoutExtension());
    list.updateContent(); list.repaint();
}

void PresetsDialog::layout()
{
    auto c = content();
    list.setBounds (c.getX() + 10, c.getY() + 10, c.getWidth() - 20, 200);
    nameL.setBounds (c.getX() + 10, c.getY() + 220, 54, 28);
    name.setBounds (c.getX() + 66, c.getY() + 220, c.getWidth() - 76, 28);
    int by = c.getBottom() - 38, x = c.getRight() - 8;
    for (auto* b : { &closeB, &delBtn, &saveBtn, &loadBtn }) { x -= 84; b->setBounds (x, by, 80, 32); x -= 4; }
}

void PresetsDialog::paintListBoxItem (int row, Graphics& g, int w, int h, bool selected)
{
    if (row < 0 || row >= items.size()) return;
    if (selected) g.fillAll (w95::navy);
    g.setColour (selected ? Colours::white : Colours::black);
    g.setFont (w95::uiFont (14));
    auto t = items[row];
    int tw = GlyphArrangement::getStringWidthInt (g.getCurrentFont(), t);
    g.drawText (t, 6, 0, w - 12, h, Justification::centredLeft);
    if (row < numFactory)
    {
        g.setColour (selected ? Colours::white.withAlpha (0.6f) : Colours::black.withAlpha (0.45f));
        g.setFont (w95::uiFont (12));
        g.drawText ("(factory)", 12 + tw, 0, 80, h, Justification::centredLeft);
    }
}

void PresetsDialog::listBoxItemClicked (int row, const MouseEvent&) { if (row >= 0) name.setText (items[row], false); }
void PresetsDialog::listBoxItemDoubleClicked (int row, const MouseEvent&) { list.selectRow (row); loadSelected(); }

void PresetsDialog::loadSelected()
{
    int r = list.getSelectedRow();
    if (r < 0) return;
    if (r < numFactory) proc.setCurrentProgram (r);
    else
    {
        if (auto xml = XmlDocument::parse (folder().getChildFile (items[r] + ".cdfx")))
        {
            auto st = ValueTree::fromXml (*xml);
            for (int i = 0; i < st.getNumChildren(); ++i)
            {
                auto ch = st.getChild (i);
                auto id = ch.getProperty ("id").toString();
                if (auto* prm = proc.apvts.getParameter (id))
                    prm->setValueNotifyingHost (prm->convertTo0to1 ((float) ch.getProperty ("value")));
            }
        }
    }
    if (onToast) onToast ("Preset: " + items[r]);
}

// ===================== ErrorDialog =====================
ErrorDialog::ErrorDialog() : Win95Dialog ("CD Player")
{
    for (auto* b : { &retry, &ignore, &cancel }) addAndMakeVisible (b);
    retry.onClick = [this] { setVisible (false); if (onRetry) onRetry(); };
    ignore.onClick = [this] { setVisible (false); if (onIgnore) onIgnore(); };
    cancel.onClick = [this] { setVisible (false); if (onCancel) onCancel(); };
    onClose = [this] { if (onCancel) onCancel(); };
    setSize (420, 190);
}

void ErrorDialog::layout()
{
    auto c = content();
    int by = c.getBottom() - 40, w = 92, total = 3 * w + 16, x = c.getCentreX() - total / 2;
    for (auto* b : { &retry, &ignore, &cancel }) { b->setBounds (x, by, w, 32); x += w + 8; }
}

void ErrorDialog::paint (Graphics& g)
{
    Win95Dialog::paint (g);
    auto c = content();
    auto ic = Rectangle<float> ((float) c.getX() + 18, (float) c.getY() + 18, 40, 40);
    g.setColour (Colour (0xffe00000)); g.fillEllipse (ic);
    g.setColour (Colour (0xff660000)); g.drawEllipse (ic, 2.0f);
    g.setColour (Colours::white);
    g.drawLine (ic.getX() + 12, ic.getY() + 12, ic.getRight() - 12, ic.getBottom() - 12, 5.0f);
    g.drawLine (ic.getRight() - 12, ic.getY() + 12, ic.getX() + 12, ic.getBottom() - 12, 5.0f);
    g.setColour (Colours::black); g.setFont (w95::uiFont (14));
    g.drawFittedText (msg, Rectangle<int> (c.getX() + 74, c.getY() + 16, c.getWidth() - 90, 60), Justification::topLeft, 3);
}

// ===================== ColorsDialog =====================
ColorsDialog::ColorsDialog (CDPlayerAudioProcessor& p) : Win95Dialog ("Custom colors"), proc (p)
{
    for (auto* b : { &bgBtn, &waveBtn, &headBtn, &okBtn }) addAndMakeVisible (b);
    bgBtn.onClick = [this] { pick ("cBg", bgBtn); };
    waveBtn.onClick = [this] { pick ("cWave", waveBtn); };
    headBtn.onClick = [this] { pick ("cHead", headBtn); };
    okBtn.onClick = [this] { setVisible (false); };
    setSize (330, 230);
}

void ColorsDialog::layout()
{
    auto c = content();
    int y = c.getY() + 14;
    for (auto* b : { &bgBtn, &waveBtn, &headBtn }) { b->setBounds (c.getX() + 74, y, 170, 30); y += 40; }
    okBtn.setBounds (c.getRight() - 90, c.getBottom() - 38, 82, 32);
}

void ColorsDialog::paint (Graphics& g)
{
    Win95Dialog::paint (g);
    auto c = content();
    int y = c.getY() + 14;
    for (auto* k : { "cBg", "cWave", "cHead" })
    {
        auto r = Rectangle<int> (c.getX() + 16, y, 46, 30);
        w95::sunken (g, r, Colour::fromString (proc.uiState.getProperty (k).toString()));
        y += 40;
    }
}

void ColorsDialog::pick (const String& key, Component& anchor)
{
    auto* cs = new ColourSelector (ColourSelector::showColourspace | ColourSelector::showSliders);
    cs->setSize (300, 280);
    cs->setCurrentColour (Colour::fromString (proc.uiState.getProperty (key).toString()), dontSendNotification);
    struct L : public ChangeListener
    {
        CDPlayerAudioProcessor& p; String k; Component& owner;
        L (CDPlayerAudioProcessor& pp, String kk, Component& o) : p (pp), k (kk), owner (o) {}
        void changeListenerCallback (ChangeBroadcaster* b) override
        {
            if (auto* s = dynamic_cast<ColourSelector*> (b)) { p.uiState.setProperty (k, s->getCurrentColour().toString(), nullptr); owner.repaint(); }
        }
    };
    auto* l = new L (proc, key, *this);
    cs->addChangeListener (l);
    struct Holder : public Component
    {
        std::unique_ptr<ColourSelector> sel; std::unique_ptr<L> lis;
        Holder (ColourSelector* s, L* li) : sel (s), lis (li) { addAndMakeVisible (sel.get()); setSize (s->getWidth(), s->getHeight()); }
        ~Holder() override { sel->removeChangeListener (lis.get()); }
        void resized() override { sel->setBounds (getLocalBounds()); }
    };
    auto holder = std::make_unique<Holder> (cs, l);
    CallOutBox::launchAsynchronously (std::move (holder), anchor.getScreenBounds(), nullptr);
}

// ===================== AboutDialog =====================
AboutDialog::AboutDialog() : Win95Dialog ("How to play CD Player")
{
    addAndMakeVisible (okBtn);
    okBtn.onClick = [this] { setVisible (false); };
    setSize (480, 360);
}

void AboutDialog::layout()
{
    auto c = content();
    okBtn.setBounds (c.getRight() - 90, c.getBottom() - 38, 82, 32);
}

void AboutDialog::paint (Graphics& g)
{
    Win95Dialog::paint (g);
    auto c = content();
    w95::drawCdIcon (g, Rectangle<float> ((float) c.getX() + 14, (float) c.getY() + 14, 44, 44));
    g.setColour (Colours::black);
    g.setFont (w95::uiFont (14, true));
    g.drawText ("CD Player FX", c.getX() + 72, c.getY() + 12, 300, 18, Justification::centredLeft);
    g.setFont (w95::uiFont (13));
    g.setColour (Colour (0xff555555));
    g.drawText ("by @buyorbealoser", c.getX() + 72, c.getY() + 30, 300, 18, Justification::centredLeft);
    g.setColour (Colours::black);
    String t = String::fromUTF8 (
        "\xe2\x80\xa2 Drag the waveform: scratch\n"
        "\xe2\x80\xa2 Shift + drag: A\xe2\x80\x93" "B repeat (Esc clears)\n"
        "\xe2\x80\xa2 Right-click / hold F: freeze (higher = shorter)\n"
        "\xe2\x80\xa2 Pads / keys 1-8: set, then hold to play a cue (Shift / right-click = clear)\n"
        "\xe2\x80\xa2 Hold \xe2\x97\x80\xe2\x97\x80 \xe2\x96\xb6\xe2\x96\xb6: rewind / fast-forward \xc2\xb7 Q reverse\n"
        "\xe2\x80\xa2 \xe2\x96\xb6/pause: tape stop and spin-up\n"
        "\xe2\x80\xa2 Z X C V B A H W T: toggle effects\n"
        "\xe2\x80\xa2 E effects \xc2\xb7 G equalizer \xc2\xb7 P presets \xc2\xb7 S next skin\n"
        "\xe2\x80\xa2 R shuffle \xc2\xb7 0 reset \xc2\xb7 wheel zoom");
    g.drawFittedText (t, Rectangle<int> (c.getX() + 72, c.getY() + 56, c.getWidth() - 84, 200), Justification::topLeft, 12, 1.0f);
}
