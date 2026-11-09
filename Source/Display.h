#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

class Display : public juce::Component
{
public:
    explicit Display (CDPlayerAudioProcessor& p);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    void toast (const juce::String& t);
    void startFreezeFromY (float yFrac);
    void stopFreeze();
    void tick();                       // called by the editor timer (~60 Hz)
    std::function<void (double)> onZoom;

    static const juce::StringArray skinIds, skinNames;

private:
    CDPlayerAudioProcessor& proc;
    juce::Image img, scopeImg;
    juce::Random rng;

    // interaction
    enum class Drag { none, scrub, ab, freeze } drag = Drag::none;
    int lastX = 0;
    double abO = 0, abA = 0, abB = 0;

    // effects on screen
    float flashSkip = 0;
    int lastSkipCount = 0, lastGlitchCount = 0, lastCutCount = 0;
    double cutTime = -10; int cutSide = 0;
    struct Gl { double p; float life; };
    std::vector<Gl> glitches;
    juce::String toastText; double toastTime = -10;

    // frame snapshot
    double sr = 44100, pos = 0, bpm = 120, refPpq = 0, zoom = 5;
    juce::int64 newest = 0, refIdx = 0;
    int cap = 1, look = 0;
    double lastCx = 0.5;

    double xToAbs (float x) const;
    float freezeDivFromY (float yFrac) const;
    void render (juce::Graphics& g, int w, int h, float sx);
};
