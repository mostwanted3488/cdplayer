#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <array>
#include "Params.h"

// Small allocation-free RBJ biquad (stereo).
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double z1[2] {}, z2[2] {};
    void reset() { z1[0] = z1[1] = z2[0] = z2[1] = 0; }
    inline float process (float x, int ch)
    {
        double y = b0 * x + z1[ch];
        z1[ch] = b1 * x - a1 * y + z2[ch];
        z2[ch] = b2 * x - a2 * y;
        return (float) y;
    }
    void set (double nb0, double nb1, double nb2, double na0, double na1, double na2)
    {
        b0 = nb0 / na0; b1 = nb1 / na0; b2 = nb2 / na0; a1 = na1 / na0; a2 = na2 / na0;
    }
    void lowShelf (double sr, double f, double gainDb)
    {
        double A = std::pow (10.0, gainDb / 40.0), w = juce::MathConstants<double>::twoPi * f / sr, c = std::cos (w), s = std::sin (w);
        double al = s / 2.0 * std::sqrt (2.0), sq = 2.0 * std::sqrt (A) * al;
        set (A * ((A + 1) - (A - 1) * c + sq), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sq),
             (A + 1) + (A - 1) * c + sq, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sq);
    }
    void peak (double sr, double f, double q, double gainDb)
    {
        double A = std::pow (10.0, gainDb / 40.0), w = juce::MathConstants<double>::twoPi * f / sr, c = std::cos (w), al = std::sin (w) / (2 * q);
        set (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
    void lowPass (double sr, double f, double q)
    {
        double w = juce::MathConstants<double>::twoPi * f / sr, c = std::cos (w), al = std::sin (w) / (2 * q);
        set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void highPass (double sr, double f, double q)
    {
        double w = juce::MathConstants<double>::twoPi * f / sr, c = std::cos (w), al = std::sin (w) / (2 * q);
        set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
    }
};

class CDPlayerAudioProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    CDPlayerAudioProcessor();
    ~CDPlayerAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "CD Player FX"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override { return (int) factoryPresets().size(); }
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---------- shared with the editor ----------
    juce::AudioProcessorValueTreeState apvts;
    juce::ValueTree uiState { "UI" }; // skin, style, zoom, colours, lookahead…

    void applyPresetValues (const std::vector<std::pair<juce::String, float>>& values);
    void resetSound();                       // RESET button (message thread)
    void setLookaheadIndex (int idx);        // 0 = off, 1 = 0.5 s, 2 = 1 s, 3 = 2 s (message thread)
    int  getLookaheadIndex() const { return lookaheadIdx.load(); }
    static double lookaheadSeconds (int idx) { static const double s[] = { 0.0, 0.5, 1.0, 2.0 }; return s[juce::jlimit (0, 3, idx)]; }
    float effective (const char* id) const;  // param value including the DAMAGE macro

    // UI -> audio (continuous controls)
    std::atomic<bool>   scrubbing { false };
    std::atomic<double> scrubTarget { 0 };
    std::atomic<int>    ffDir { 0 };
    std::atomic<bool>   motor { true };
    std::atomic<int>    freezeDiv { 0 };   // 0 = off, else 4/8/16/32
    std::atomic<int>    holdCue { -1 };
    std::atomic<bool>   seekHold { false };

    enum CmdType { SetCue, ClearCue, ClearCues, SetAB, ClearAB, Burst, StopNow, Jump, Resync };
    void sendCommand (int type, int i = 0, double a = 0, double b = 0);

    // audio -> UI (snapshot for the display)
    std::atomic<double>  aPos { 0 }, aRate { 1 }, aSr { 44100 }, aBpm { 120 }, aRefPpq { 0 }, aHostSeconds { 0 };
    std::atomic<juce::int64> aNewest { 0 }, aRefIdx { 0 };
    std::atomic<int>     aLookahead { 0 }, aCapacity { 1 };
    std::atomic<double>  aFreezeStart { 0 }, aFreezeLen { 0 }, aAbA { 0 }, aAbB { 0 };
    std::atomic<int>     aFreezeDivShown { 0 };
    std::atomic<bool>    aAbOn { false }, aEngaged { false }, aHostPlaying { false };
    std::array<std::atomic<juce::int64>, 8> aCues;
    std::atomic<int>     aSkipCount { 0 }, aCutSide { 0 }, aCutCount { 0 };
    std::array<std::atomic<double>, 32> aGlitchPos;
    std::atomic<int>     aGlitchCount { 0 };
    std::atomic<float>   aGateGain { 1 };

    // history (written by the audio thread, read by the display — benign races)
    juce::AudioBuffer<float> hist;
    int capacity = 1, capMask = 0;
    static constexpr int kBinShift = 7; // 128 samples per peak bin
    std::vector<float> pkMin, pkMax;
    int binMask = 0;
    float readHistMono (double absPos) const;

private:
    // ---------------- engine ----------------
    double sr = 44100;
    juce::int64 W = 0;           // samples written so far
    int L = 0;                   // lookahead (samples)
    std::atomic<int> lookaheadIdx { 2 };
    std::atomic<bool> latencyDirty { false };

    double pos = 0, rate = 1;
    bool engaged = false;
    struct Loop { double start = 0, len = 0, c = 0; int n = 0; double jump = 0; bool on = false; int div = 0; };
    Loop freeze, sk;
    struct Glitch { int t = 0, n = 0, c = 0, seg = 0; double start = 0; bool on = false; } gl;
    struct Cut { int side = 0, n = 0, c = 0; bool on = false; } cut;
    bool abOn = false; double abA = 0, abB = 0;
    juce::int64 cues[8];
    int burst = 0;
    int lastHold = -1;
    double xfOld = 0; int xfN = 0; static constexpr int kXf = 384;
    double wob = 0, drift = 0, driftTarget = 0; int driftT = 0;
    int hc = 0; float hl = 0, hr = 0;
    float gg = 1;
    double wowDelay = 0;
    // pitch shifter grains
    struct Grain { double a = 0; double ph = 0; };
    Grain grains[4]; bool grainsInit = false;
    // peaks accumulation
    float accMin = 0, accMax = 0;
    // block timing
    double refPpq = 0, bpmNow = 120; juce::int64 refIdx = 0;
    juce::Random rng;

    // effects after the core
    Biquad shelf, hpf, pk, lp, eqs[kNumEq];
    std::vector<float> espBuf[2]; int espW = 0; double espDelay = 0, espLfo = 0; float espLpz[2] {};
    juce::dsp::Convolution conv;
    juce::AudioBuffer<float> roomIn, wetBuf, dryBuf;
    juce::dsp::Compressor<float> comp;
    int roomTypeLoaded = -1;
    juce::SmoothedValue<float> sVol, sMix;

    // commands
    struct Cmd { int type = 0, i = 0; double a = 0, b = 0; };
    juce::AbstractFifo fifo { 256 };
    std::array<Cmd, 256> cmdBuf;
    void handleCommands();

    int currentProgram = 0;

    void loadRoomIR (int type);
    void timerCallback() override;
    inline float rd (int ch, double p) const;
    double newestIdx() const { return (double) (W - 1); }
    double oldestIdx() const { return (double) (W - capacity + 8); }
    double ppqAt (double idx) const { return refPpq + (idx - (double) refIdx) * bpmNow / 60.0 / sr; }
    double spb() const { return 60.0 / bpmNow * sr; }
    void resyncToLive (double live);

    std::atomic<float>* p (const juce::String& id) const { return apvts.getRawParameterValue (id); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CDPlayerAudioProcessor)
};
