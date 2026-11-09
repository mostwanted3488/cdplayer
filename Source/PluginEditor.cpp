#include "PluginEditor.h"

using namespace juce;

namespace
{
    struct QuickFx { const char* id; const char* name; char key; float on; };
    const QuickFx quickFx[] = {
        { pid::skip, "CD Skip", 'Z', 0.45f }, { pid::scratch, "Scratched Disc", 'X', 0.4f }, { pid::battery, "Low Battery", 'C', 0.5f },
        { pid::crush, "Lo-Fi Laser", 'V', 0.55f }, { pid::bass, "MEGA BASS", 'B', 0.7f }, { pid::esp, "Anti-Shock (ESP)", 'A', 0.45f },
        { pid::hp, "Headphones", 'H', 0.6f }, { pid::room, "Room", 'W', 0.4f }, { pid::gate, "Gate", 'T', 0.8f } };
    const char* fxShort[] = { "SKIP", "SCR", "BATT", "LOFI", "BASS", "ESP", "PHONES", "ROOM", "GATE" };
    const Colour cueCols[8] = { Colour (0xffff3b30), Colour (0xffff9500), Colour (0xffffd60a), Colour (0xff34c759),
                                Colour (0xff00c7be), Colour (0xff0a84ff), Colour (0xffbf5af2), Colour (0xffff2d92) };
    String fmtTime (double s) { s = jmax (0.0, s); return String ((int) (s / 60)).paddedLeft ('0', 2) + ":" + String ((int) std::fmod (s, 60.0)).paddedLeft ('0', 2); }
    PopupMenu::Item mItem (const String& text, int id, const String& key = {}, bool ticked = false)
    {
        PopupMenu::Item it (text); it.itemID = id; it.shortcutKeyDescription = key; it.isTicked = ticked; return it;
    }
}

// ======================= TitleBar =======================
TitleBar::TitleBar() { for (auto* b : { &minBtn, &maxBtn, &closeBtn }) addAndMakeVisible (b); }
void TitleBar::paint (Graphics& g)
{
    g.fillAll (w95::title);
    w95::drawCdIcon (g, Rectangle<float> (6, 4, 24, 24));
    g.setColour (Colours::white); g.setFont (w95::titleFont (19));
    g.drawText ("CD Player", 38, 0, 300, getHeight(), Justification::centredLeft);
}
void TitleBar::resized()
{
    int y = (getHeight() - 23) / 2, x = getWidth() - 4 - 26;
    closeBtn.setBounds (x, y, 26, 23); x -= 31;
    maxBtn.setBounds (x, y, 26, 23); x -= 29;
    minBtn.setBounds (x, y, 26, 23);
}

// ======================= MenuBar95 =======================
static const char* menuNames[] = { "Disc", "View", "Options", "Help" };
Rectangle<int> MenuBar95::itemRect (int i) const
{
    int x = 2;
    for (int k = 0; k < 4; ++k)
    {
        int w = GlyphArrangement::getStringWidthInt (w95::uiFont (15), menuNames[k]) + 18;
        if (k == i) return { x, 2, w, getHeight() - 4 };
        x += w;
    }
    return {};
}
void MenuBar95::paint (Graphics& g)
{
    g.setFont (w95::uiFont (15));
    for (int k = 0; k < 4; ++k)
    {
        auto r = itemRect (k);
        bool open = k == openIndex;
        if (open) { g.setColour (w95::navy); g.fillRect (r); }
        g.setColour (open ? Colours::white : Colours::black);
        g.drawText (menuNames[k], r, Justification::centred);
        // underline the first letter (Win95 accelerator)
        int fw = GlyphArrangement::getStringWidthInt (g.getCurrentFont(), String::charToString (menuNames[k][0]));
        int tw = GlyphArrangement::getStringWidthInt (g.getCurrentFont(), menuNames[k]);
        g.fillRect (r.getCentreX() - tw / 2, r.getCentreY() + 8, fw, 1);
    }
    g.setFont (w95::uiFont (12.5f, true));
    auto cr = getLocalBounds().removeFromRight (150).withTrimmedRight (8);
    g.setColour (Colours::white); g.drawText ("@buyorbealoser", cr.translated (1, 1), Justification::centredRight);
    g.setColour (w95::shadow);    g.drawText ("@buyorbealoser", cr, Justification::centredRight);
}
void MenuBar95::mouseDown (const MouseEvent& e)
{
    for (int k = 0; k < 4; ++k)
        if (itemRect (k).contains (e.getPosition()) && getMenu)
        {
            openIndex = k; repaint();
            auto m = getMenu (k);
            m.setLookAndFeel (&getLookAndFeel());
            m.showMenuAsync (PopupMenu::Options().withTargetComponent (this).withTargetScreenArea (localAreaToGlobal (itemRect (k))),
                             [safe = Component::SafePointer<MenuBar95> (this), k] (int r)
                             {
                                 if (safe == nullptr) return;
                                 safe->openIndex = -1; safe->repaint();
                                 if (r > 0 && safe->onPick) safe->onPick (k, r);
                             });
            return;
        }
}

// ======================= SeekBar =======================
void SeekBar::paint (Graphics& g)
{
    g.fillAll (w95::face);
    auto rail = Rectangle<int> (8, getHeight() / 2 - 2, getWidth() - 16, 6);
    w95::sunken (g, rail, w95::face, false);
    double newest = (double) proc.aNewest.load(), oldest = newest - proc.aCapacity.load() + 8;
    if (newest <= 0) return;
    oldest = jmax (0.0, oldest);
    auto xOf = [&] (double idx) { return 10.0f + (float) ((idx - oldest) / jmax (1.0, newest - oldest)) * (getWidth() - 20); };
    for (int k = 0; k < 8; ++k)
    {
        auto c = proc.aCues[(size_t) k].load();
        if (c >= 0) { g.setColour (cueCols[k]); g.fillRect (xOf ((double) c) - 1, 2.0f, 2.0f, 10.0f); }
    }
    if (proc.aAbOn.load())
    {
        float a = xOf (proc.aAbA.load()), b = xOf (proc.aAbB.load());
        g.setColour (w95::navy); g.fillRect (a, (float) getHeight() - 12, jmax (2.0f, b - a), 4.0f);
    }
    // live point
    float xl = xOf (newest - proc.aLookahead.load());
    g.setColour (w95::shadow); g.fillRect (xl - 0.5f, (float) getHeight() / 2 + 6, 1.0f, 6.0f);
    float x = xOf (proc.aPos.load());
    w95::raised (g, Rectangle<int> ((int) x - 10, 4, 20, getHeight() - 8));
}
void SeekBar::seekTo (float x)
{
    double newest = (double) proc.aNewest.load(), oldest = jmax (0.0, newest - proc.aCapacity.load() + 8);
    double f = jlimit (0.0, 1.0, (double) (x - 10) / jmax (1, getWidth() - 20));
    proc.sendCommand (CDPlayerAudioProcessor::Jump, 0, oldest + f * (newest - oldest));
}
void SeekBar::mouseDown (const MouseEvent& e) { proc.seekHold = true; seekTo ((float) e.x); }
void SeekBar::mouseDrag (const MouseEvent& e) { seekTo ((float) e.x); }
void SeekBar::mouseUp (const MouseEvent&) { proc.seekHold = false; }

// ======================= Pad =======================
void Pad::paint (Graphics& g)
{
    auto r = getLocalBounds();
    if (down) w95::pressed (g, r); else w95::raised (g, r);
    if (down) r.translate (1, 1);
    g.setColour (Colours::black);
    g.setFont (w95::uiFont (14, true));
    g.drawText (String (index + 1), r.withTrimmedBottom (14).withTrimmedTop (2), Justification::centred);
    g.setFont (w95::uiFont (10.5f));
    g.setColour (Colours::black.withAlpha (0.75f));
    g.drawText (timeText, r.withTrimmedTop (18).withTrimmedBottom (6), Justification::centred);
    if (set) { g.setColour (cueCols[index]); g.fillRect (r.getX() + 5, r.getBottom() - 9, r.getWidth() - 10, 4); }
}
void Pad::mouseDown (const MouseEvent& e)
{
    down = true; repaint();
    if (onPress) onPress (index, e.mods.isPopupMenu() || e.mods.isShiftDown());
}
void Pad::mouseUp (const MouseEvent&) { down = false; repaint(); if (onRelease) onRelease (index); }

// ======================= StatusBar =======================
StatusBar::StatusBar (CDPlayerAudioProcessor& p)
{
    mix.setSliderStyle (Slider::LinearHorizontal);
    mix.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    mix.setTooltip ("Dry / Wet - double-click = 100%");
    addAndMakeVisible (mix);
    mixAtt = std::make_unique<SliderAtt> (p.apvts, pid::mix, mix);
    mix.setDoubleClickReturnValue (true, 1.0);
    mixParam = p.apvts.getParameter (pid::mix);
    mixVal.setFont (w95::uiFont (12, true)); mixVal.setJustificationType (Justification::centredRight);
    addAndMakeVisible (mixVal);
    auto upd = [this] { mixVal.setText (mixParam->getCurrentValueAsText(), dontSendNotification); };
    mix.onValueChange = upd; upd();
}
void StatusBar::resized()
{
    auto r = getLocalBounds();
    r.removeFromRight (20);
    auto mp = r.removeFromRight (214);
    mixVal.setBounds (mp.getRight() - 40, 0, 36, getHeight());
    mix.setBounds (mp.getX() + 40, 0, 104, getHeight());
}
void StatusBar::paint (Graphics& g)
{
    auto r = getLocalBounds();
    auto grip = r.removeFromRight (18);
    r.removeFromRight (2);
    auto mp = r.removeFromRight (214); r.removeFromRight (4);
    auto p1 = r.removeFromLeft (178); r.removeFromLeft (4);
    auto p2 = r;
    for (auto pr : { p1, p2, mp }) w95::thinSunken (g, pr);
    g.setColour (Colours::black); g.setFont (w95::uiFont (15));
    g.drawText (left, p1.reduced (8, 0), Justification::centredLeft);
    g.drawText (right, p2.reduced (8, 0), Justification::centredLeft);
    g.setFont (w95::uiFont (11.5f, true));
    g.drawText ("DRY", mp.getX() + 6, 0, 34, getHeight(), Justification::centredLeft);
    g.drawText ("WET", mp.getX() + 146, 0, 30, getHeight(), Justification::centredLeft);
    for (int k = 0; k < 3; ++k)
    {
        int o = 4 + k * 5;
        g.setColour (Colours::white); g.drawLine ((float) grip.getRight() - o, (float) grip.getBottom() - 1, (float) grip.getRight() - 1, (float) grip.getBottom() - o, 1.0f);
        g.setColour (w95::shadow); g.drawLine ((float) grip.getRight() - o + 1, (float) grip.getBottom() - 1, (float) grip.getRight() - 1, (float) grip.getBottom() - o + 1, 1.5f);
    }
}

// ======================= Editor =======================
CDPlayerAudioProcessorEditor::CDPlayerAudioProcessorEditor (CDPlayerAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p), display (p), seek (p), status (p),
      fxDlg (p), eqDlg (p), presetDlg (p), colorsDlg (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);
    for (auto* c : std::initializer_list<Component*> { &titleBar, &menuBar, &display, &playBtn, &stopBtn, &rewBtn, &seek, &ffBtn, &resetBtn, &status })
        content.addAndMakeVisible (c);
    for (int i = 0; i < 8; ++i)
    {
        auto* pd = pads.add (new Pad (i));
        content.addAndMakeVisible (pd);
        pd->onPress = [this] (int k, bool clear) { padPress (k, clear); };
        pd->onRelease = [this] (int k) { padRelease (k); };
    }
    for (auto* d : std::initializer_list<Win95Dialog*> { &fxDlg, &eqDlg, &presetDlg, &errDlg, &colorsDlg, &aboutDlg })
        content.addChildComponent (d);

    // window buttons
    titleBar.minBtn.onClick = [this] { shaded = true; applyWindowSize(); };
    titleBar.closeBtn.onClick = [this] { shaded = true; applyWindowSize(); };
    titleBar.maxBtn.onClick = [this] { if (shaded) shaded = false; else big = ! big; applyWindowSize(); };
    titleBar.onDouble = [this] { titleBar.maxBtn.onClick(); };
    titleBar.closeBtn.setTooltip ("Roll up (the host owns the real window)");

    // menus
    menuBar.getMenu = [this] (int i) { return buildMenu (i); };
    menuBar.onPick = [this] (int m, int id) { menuPick (m, id); };

    // transport
    playBtn.onClick = [this] { proc.motor = ! proc.motor.load(); };
    playBtn.setTooltip ("Play / pause (tape stop & spin-up)  [Space]");
    stopBtn.onClick = [this] { proc.motor = false; proc.sendCommand (CDPlayerAudioProcessor::StopNow); };
    stopBtn.setTooltip ("Stop now");
    rewBtn.onStateChange = [this] { proc.ffDir = rewBtn.isDown() ? -1 : (proc.ffDir.load() == -1 ? 0 : proc.ffDir.load()); };
    ffBtn.onStateChange  = [this] { proc.ffDir = ffBtn.isDown() ? 1 : (proc.ffDir.load() == 1 ? 0 : proc.ffDir.load()); };
    rewBtn.setTooltip ("Hold: audible rewind"); ffBtn.setTooltip ("Hold: fast-forward (up to the lookahead)");
    resetBtn.onClick = [this] { resetAll(); };
    resetBtn.setTooltip ("Reset all FX  [0]");
    resetBtn.setColour (TextButton::textColourOffId, Colour (0xffa00000));

    // dialogs
    fxDlg.onShuffle = [this] { shuffle(); };
    fxDlg.onReset = [this] { resetAll(); };
    eqDlg.onToast = [this] (const String& t) { toast (t); };
    presetDlg.onToast = [this] (const String& t) { toast (t); };
    errDlg.onRetry = [this] { proc.sendCommand (CDPlayerAudioProcessor::Burst); closeError (3 + Random::getSystemRandom().nextDouble() * 3); };
    errDlg.onIgnore = [this] { closeError (14); };
    errDlg.onCancel = [this] { closeError (6); setParam (pid::scratch, 0.5f); };
    display.onZoom = [this] (double z) { toast ("Zoom: " + String (z, 1) + "s"); };

    setWantsKeyboardFocus (true);
    setResizable (true, true);
    setResizeLimits (300, 30, 1400, 1240);
    getConstrainer()->setFixedAspectRatio ((double) baseW / baseH);
    setSize (baseW, baseH);
    startTimerHz (60);
}

CDPlayerAudioProcessorEditor::~CDPlayerAudioProcessorEditor()
{
    proc.scrubbing = false; proc.ffDir = 0; proc.holdCue = -1; proc.freezeDiv = 0; proc.seekHold = false;
    setLookAndFeel (nullptr);
}

void CDPlayerAudioProcessorEditor::paint (Graphics& g)
{
    g.fillAll (w95::face);
    float s = (float) getWidth() / baseW;
    g.addTransform (AffineTransform::scale (s));
    w95::raised (g, { 0, 0, baseW, shaded ? titleH : baseH }, false);
    if (! shaded) w95::sunken (g, { 8, 67, 574, 326 }, Colours::black, false);
}

void CDPlayerAudioProcessorEditor::resized()
{
    float s = (float) getWidth() / baseW;
    content.setBounds (0, 0, baseW, baseH);
    content.setTransform (AffineTransform::scale (s));
    layoutContent();
}

void CDPlayerAudioProcessorEditor::applyWindowSize()
{
    double s = big ? 1.35 : 1.0;
    if (shaded)
    {
        getConstrainer()->setFixedAspectRatio (0);
        setSize (roundToInt (baseW * s), roundToInt (titleH * s));
    }
    else
    {
        getConstrainer()->setFixedAspectRatio ((double) baseW / baseH);
        setSize (roundToInt (baseW * s), roundToInt (baseH * s));
    }
    titleBar.maxBtn.setKind (shaded || big ? IconButton95::restore : IconButton95::maximise);
    if (shaded) for (auto* d : std::initializer_list<Win95Dialog*> { &fxDlg, &eqDlg, &presetDlg, &colorsDlg, &aboutDlg }) d->setVisible (false);
    repaint();
}

void CDPlayerAudioProcessorEditor::layoutContent()
{
    titleBar.setBounds (5, 5, 580, 32);
    menuBar.setBounds (5, 37, 580, 28);
    display.setBounds (12, 71, 566, 318);
    playBtn.setBounds (8, 399, 128, 40);
    stopBtn.setBounds (138, 399, 30, 40);
    rewBtn.setBounds (168, 399, 34, 40);
    seek.setBounds (204, 399, 336, 40);
    ffBtn.setBounds (542, 399, 40, 40);
    int x = 8;
    for (auto* pd : pads) { pd->setBounds (x, 445, 58, 38); x += 62; }
    resetBtn.setBounds (504, 445, 78, 38);
    status.setBounds (7, 489, 576, 30);
    bool vis = ! shaded;
    for (auto* c : std::initializer_list<Component*> { &menuBar, &display, &playBtn, &stopBtn, &rewBtn, &seek, &ffBtn, &resetBtn, &status })
        c->setVisible (vis);
    for (auto* pd : pads) pd->setVisible (vis);
}

void CDPlayerAudioProcessorEditor::openDialog (Win95Dialog& d, int x, int y)
{
    if (! d.isVisible() && d.getProperties()["placed"].isVoid()) { d.setTopLeftPosition (x, y); d.getProperties().set ("placed", true); }
    d.open();
}

// ---------------- params helpers ----------------
void CDPlayerAudioProcessorEditor::setParam (const char* id, float v)
{
    if (auto* prm = proc.apvts.getParameter (id)) prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}
float CDPlayerAudioProcessorEditor::getParam (const char* id) const { return proc.apvts.getRawParameterValue (id)->load(); }

void CDPlayerAudioProcessorEditor::toggleFx (const char* id)
{
    for (auto& q : quickFx)
        if (String (q.id) == id)
        {
            bool on = getParam (id) > 0.01f;
            setParam (id, on ? 0.0f : q.on);
            toast (String (q.name) + (on ? ": off" : ": " + proc.apvts.getParameter (id)->getCurrentValueAsText()));
        }
}

void CDPlayerAudioProcessorEditor::shuffle()
{
    auto& r = Random::getSystemRandom();
    for (auto& q : quickFx) setParam (q.id, r.nextFloat() < 0.55f ? 0.0f : r.nextFloat() * 0.8f);
    setParam (pid::speed, r.nextFloat() < 0.6f ? 1.0f : 0.75f + r.nextFloat() * 0.4f);
    setParam (pid::gateDiv, (float) r.nextInt (3));
    setParam (pid::roomType, (float) r.nextInt (4));
    auto fx = activeFx();
    toast ("Shuffle: " + (fx.isEmpty() ? String ("clean") : fx));
}

void CDPlayerAudioProcessorEditor::resetAll()
{
    proc.resetSound();
    if (errDlg.isVisible()) closeError (6);
    toast ("All FX reset");
}

String CDPlayerAudioProcessorEditor::activeFx() const
{
    StringArray a;
    for (int i = 0; i < 9; ++i) if (proc.effective (quickFx[i].id) > 0.01f) a.add (fxShort[i]);
    float sp = getParam (pid::speed);
    if (std::abs (sp - 1.0f) > 0.01f) a.add (String (roundToInt (sp * 100)) + "%");
    if (getParam (pid::rev) > 0.5f) a.add ("REV");
    return a.joinIntoString (" ");
}

void CDPlayerAudioProcessorEditor::setSkin (const String& s)
{
    proc.uiState.setProperty ("skin", s, nullptr);
    toast ("Skin: " + Display::skinNames[Display::skinIds.indexOf (s)]);
}

// ---------------- pads / cues ----------------
void CDPlayerAudioProcessorEditor::padPress (int i, bool clear)
{
    if (clear) { proc.sendCommand (CDPlayerAudioProcessor::ClearCue, i); toast ("Cue " + String (i + 1) + " cleared"); return; }
    if (proc.aCues[(size_t) i].load() < 0) { proc.sendCommand (CDPlayerAudioProcessor::SetCue, i); toast ("Cue " + String (i + 1) + " set"); }
    else { proc.holdCue = i; toast ("Cue " + String (i + 1)); }
}
void CDPlayerAudioProcessorEditor::padRelease (int i) { if (proc.holdCue.load() == i) proc.holdCue = -1; }

// ---------------- errors ----------------
void CDPlayerAudioProcessorEditor::showError()
{
    static const char* msgs[] = { "Cannot read from drive D:\\\nThe disc may be scratched or dirty.",
                                  "CD Player has encountered a read error on track 01.\nSector 0x00A3F1 is unreadable.",
                                  "The device is not ready.\nPlease insert a disc or clean the laser lens." };
    errDlg.setMessage (msgs[Random::getSystemRandom().nextInt (3)]);
    errDlg.setTopLeftPosition (85, 130);
    errDlg.open();
}
void CDPlayerAudioProcessorEditor::closeError (double delaySeconds)
{
    errDlg.setVisible (false);
    nextErr = Time::getMillisecondCounterHiRes() / 1000.0 + delaySeconds;
}

// ---------------- timer ----------------
void CDPlayerAudioProcessorEditor::timerCallback()
{
    if (! shaded) display.tick();
    seek.repaint();
    playBtn.setKind (proc.motor.load() ? IconButton95::pause : IconButton95::play);

    for (int i = 0; i < 8; ++i)
    {
        auto c = proc.aCues[(size_t) i].load();
        bool set = c >= 0;
        String t = set ? fmtTime (((double) proc.aNewest.load() - (double) c) / proc.aSr.load()) : String ("empty");
        if (set) t = "-" + t;
        if (pads[i]->set != set || pads[i]->timeText != t) { pads[i]->set = set; pads[i]->timeText = t; pads[i]->repaint(); }
    }

    String left = "Total Play: " + fmtTime (proc.aHostSeconds.load()) + " m:s";
    String fx = activeFx();
    String right = String (roundToInt (proc.aBpm.load())) + " BPM" + (fx.isNotEmpty() ? String::fromUTF8 ("  \xc2\xb7  ") + fx : String());
    if (left != status.left || right != status.right) { status.left = left; status.right = right; status.repaint(); }

    // Windows read errors when the disc is badly scratched
    double now = Time::getMillisecondCounterHiRes() / 1000.0;
    if (proc.effective (pid::scratch) >= 0.85f && proc.motor.load() && ! shaded && ! errDlg.isVisible() && now > nextErr)
    {
        if (nextErr == 0 || now - nextErr > 20) nextErr = now + 2.5 + Random::getSystemRandom().nextDouble() * 4;
        else showError();
    }
}

// ---------------- menus ----------------
PopupMenu CDPlayerAudioProcessorEditor::buildMenu (int index)
{
    PopupMenu m;
    if (index == 0)
    {
        m.addItem (mItem ("Presets...", 1, "P"));
        m.addSeparator();
        PopupMenu la;
        const char* ln[] = { "Off (record)", "0.5 s", "1 s", "2 s" };
        for (int i = 0; i < 4; ++i) la.addItem (10 + i, ln[i], true, proc.getLookaheadIndex() == i);
        m.addSubMenu ("Lookahead", la);
        m.addItem (20, "Back to live");
    }
    else if (index == 1)
    {
        PopupMenu skins;
        auto cur = proc.uiState.getProperty ("skin").toString();
        for (int i = 0; i < Display::skinIds.size(); ++i) skins.addItem (100 + i, Display::skinNames[i], true, cur == Display::skinIds[i]);
        skins.addSeparator();
        skins.addItem (150, "Custom colors...");
        m.addSubMenu ("Skin", skins);
        PopupMenu styles;
        const char* sn[] = { "Classic waveform", "Mirror", "Bars" }; const char* sid[] = { "classic", "mirror", "bars" };
        auto st = proc.uiState.getProperty ("style").toString();
        for (int i = 0; i < 3; ++i) styles.addItem (200 + i, sn[i], true, st == sid[i]);
        m.addSubMenu ("Style", styles);
        m.addItem (mItem ("Next skin", 300, "S"));
        m.addSeparator();
        m.addItem (301, "Beat grid", true, (bool) proc.uiState.getProperty ("grid", true));
        m.addItem (mItem ("Zoom in", 302, "+"));
        m.addItem (mItem ("Zoom out", 303, "-"));
    }
    else if (index == 2)
    {
        m.addItem (mItem ("Effects...", 1, "E"));
        m.addItem (mItem ("Equalizer...", 2, "G"));
        m.addSeparator();
        PopupMenu q;
        for (int i = 0; i < 9; ++i)
            q.addItem (mItem (quickFx[i].name, 10 + i, String::charToString (quickFx[i].key), getParam (quickFx[i].id) > 0.01f));
        m.addSubMenu ("Quick toggles", q);
        m.addItem (mItem ("Reverse", 30, "Q", getParam (pid::rev) > 0.5f));
        m.addItem (mItem ("Slip mode", 31, {}, getParam (pid::slip) > 0.5f));
        m.addSeparator();
        m.addItem (mItem (String::fromUTF8 ("Clear A\xe2\x80\x93" "B repeat"), 40, "Esc"));
        m.addItem (41, "Clear cue points");
        m.addSeparator();
        m.addItem (mItem ("Shuffle FX", 50, "R"));
        m.addItem (mItem ("Reset FX", 51, "0"));
    }
    else
    {
        m.addItem (mItem ("How to play...", 1, "F1"));
    }
    return m;
}

void CDPlayerAudioProcessorEditor::menuPick (int menu, int id)
{
    if (menu == 0)
    {
        if (id == 1) { presetDlg.refresh(); openDialog (presetDlg, 95, 70); }
        else if (id >= 10 && id < 14) { proc.setLookaheadIndex (id - 10); toast ("Lookahead: " + String (CDPlayerAudioProcessor::lookaheadSeconds (id - 10), 1) + "s"); }
        else if (id == 20) proc.sendCommand (CDPlayerAudioProcessor::Resync);
    }
    else if (menu == 1)
    {
        if (id >= 100 && id < 100 + Display::skinIds.size()) setSkin (Display::skinIds[id - 100]);
        else if (id == 150) { proc.uiState.setProperty ("skin", "custom", nullptr); openDialog (colorsDlg, 130, 110); }
        else if (id >= 200 && id < 203) { const char* sid[] = { "classic", "mirror", "bars" }; proc.uiState.setProperty ("style", sid[id - 200], nullptr); }
        else if (id == 300) setSkin (Display::skinIds[(Display::skinIds.indexOf (proc.uiState.getProperty ("skin").toString()) + 1) % Display::skinIds.size()]);
        else if (id == 301) proc.uiState.setProperty ("grid", ! (bool) proc.uiState.getProperty ("grid", true), nullptr);
        else if (id == 302 || id == 303)
        {
            double z = (double) proc.uiState.getProperty ("zoom", 5.0);
            z = jlimit (1.5, 14.0, id == 302 ? z / 1.3 : z * 1.3);
            proc.uiState.setProperty ("zoom", z, nullptr);
            toast ("Zoom: " + String (z, 1) + "s");
        }
    }
    else if (menu == 2)
    {
        if (id == 1) openDialog (fxDlg, 70, 30);
        else if (id == 2) openDialog (eqDlg, 60, 90);
        else if (id >= 10 && id < 19) toggleFx (quickFx[id - 10].id);
        else if (id == 30) { bool r = getParam (pid::rev) > 0.5f; setParam (pid::rev, r ? 0.0f : 1.0f); toast (r ? "Reverse OFF" : "Reverse ON"); }
        else if (id == 31) { bool s = getParam (pid::slip) > 0.5f; setParam (pid::slip, s ? 0.0f : 1.0f); toast (s ? "Slip OFF" : "Slip ON"); }
        else if (id == 40) { proc.sendCommand (CDPlayerAudioProcessor::ClearAB); toast (String::fromUTF8 ("A\xe2\x80\x93" "B cleared")); }
        else if (id == 41) { proc.sendCommand (CDPlayerAudioProcessor::ClearCues); toast ("Cues cleared"); }
        else if (id == 50) shuffle();
        else if (id == 51) resetAll();
    }
    else if (id == 1) openDialog (aboutDlg, 55, 70);
}

// ---------------- keyboard ----------------
bool CDPlayerAudioProcessorEditor::keyPressed (const KeyPress& k)
{
    auto mods = k.getModifiers();
    if (mods.isCommandDown() || mods.isCtrlDown() || mods.isAltDown()) return false;
    if (k == KeyPress::spaceKey) { proc.motor = ! proc.motor.load(); return true; }
    if (k == KeyPress::escapeKey)
    {
        proc.sendCommand (CDPlayerAudioProcessor::ClearAB);
        for (auto* d : std::initializer_list<Win95Dialog*> { &fxDlg, &eqDlg, &presetDlg, &colorsDlg, &aboutDlg }) d->setVisible (false);
        return true;
    }
    if (k == KeyPress::F1Key) { openDialog (aboutDlg, 55, 70); return true; }
    if (k.getKeyCode() == KeyPress::leftKey || k.getKeyCode() == KeyPress::rightKey) return true;
    auto c = (juce_wchar) CharacterFunctions::toLowerCase (k.getTextCharacter());
    if (c >= '1' && c <= '8') return true; // handled as hold in keyStateChanged
    for (auto& q : quickFx) if (c == (juce_wchar) CharacterFunctions::toLowerCase ((juce_wchar) q.key)) { toggleFx (q.id); return true; }
    switch (c)
    {
        case 'q': menuPick (2, 30); return true;
        case 'r': shuffle(); return true;
        case '0': resetAll(); return true;
        case 'e': openDialog (fxDlg, 70, 30); return true;
        case 'g': openDialog (eqDlg, 60, 90); return true;
        case 'p': menuPick (0, 1); return true;
        case 's': menuPick (1, 300); return true;
        case '+': case '=': menuPick (1, 302); return true;
        case '-': menuPick (1, 303); return true;
        case 'f': return true;
        default: break;
    }
    return false;
}

bool CDPlayerAudioProcessorEditor::keyStateChanged (bool)
{
    bool f = KeyPress::isKeyCurrentlyDown ('F') || KeyPress::isKeyCurrentlyDown ('f');
    if (f != fDown) { fDown = f; if (f) display.startFreezeFromY (0.4f); else display.stopFreeze(); }
    bool l = KeyPress::isKeyCurrentlyDown (KeyPress::leftKey), r = KeyPress::isKeyCurrentlyDown (KeyPress::rightKey);
    if (l != arrowL || r != arrowR) { arrowL = l; arrowR = r; proc.ffDir = l ? -1 : (r ? 1 : 0); }
    for (int i = 0; i < 8; ++i)
    {
        bool d = KeyPress::isKeyCurrentlyDown ('1' + i);
        if (d && ! keyCueDown[i]) padPress (i, ModifierKeys::currentModifiers.isShiftDown());
        if (! d && keyCueDown[i]) padRelease (i);
        keyCueDown[i] = d;
    }
    return false;
}
