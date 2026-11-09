// Headless test: drives the processor with audio and checks every feature for NaN/Inf, level and stability,
// then renders the editor (all skins) into PNG files.
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <iostream>

using namespace juce;

static int failures = 0;
static void check (bool ok, const String& what) { std::cout << (ok ? "OK   " : "FAIL ") << what << std::endl; if (! ok) ++failures; }

struct Runner
{
    CDPlayerAudioProcessor p;
    double sr; int bs; double phase = 0; Random r { 7 };
    Runner (double s, int b) : sr (s), bs (b) { p.setPlayConfigDetails (2, 2, sr, bs); p.prepareToPlay (sr, bs); }
    struct Stats { float peak = 0; double rms = 0; int bad = 0; double ms = 0; };
    Stats run (double seconds, std::function<void (int)> each = nullptr)
    {
        AudioBuffer<float> buf (2, bs); MidiBuffer m; Stats st; double acc = 0; long n = 0;
        int blocks = (int) (seconds * sr / bs);
        for (int k = 0; k < blocks; ++k)
        {
            if (each) each (k);
            for (int i = 0; i < bs; ++i)
            {
                // kick every 0.5 s + chord + noise
                double t = phase / sr; phase += 1;
                double kt = std::fmod (t, 0.5);
                float v = (float) (0.6 * std::sin (2 * MathConstants<double>::pi * (60 + 90 * std::exp (-kt * 30)) * kt) * std::exp (-kt * 8)
                                   + 0.15 * std::sin (2 * MathConstants<double>::pi * 220 * t) + 0.05 * (r.nextFloat() - 0.5f));
                buf.setSample (0, i, v); buf.setSample (1, i, v * 0.9f);
            }
            auto t0 = Time::getMillisecondCounterHiRes();
            p.processBlock (buf, m);
            st.ms = jmax (st.ms, Time::getMillisecondCounterHiRes() - t0);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < bs; ++i)
                {
                    float x = buf.getSample (c, i);
                    if (! std::isfinite (x)) ++st.bad;
                    st.peak = jmax (st.peak, std::abs (x)); acc += (double) x * x; ++n;
                }
        }
        st.rms = std::sqrt (acc / jmax (1L, n));
        return st;
    }
    void set (const char* id, float v) { auto* prm = p.apvts.getParameter (id); prm->setValueNotifyingHost (prm->convertTo0to1 (v)); }
};

int main()
{
    ScopedJuceInitialiser_GUI gui;
    for (double sr : { 44100.0, 48000.0, 96000.0 })
        for (int bs : { 64, 512, 2048 })
        {
            Runner R (sr, bs);
            String tag = "[" + String (sr) + "/" + String (bs) + "] ";
            auto s = R.run (3);
            check (s.bad == 0 && s.rms > 0.02 && s.peak < 2, tag + "plain pass-through rms=" + String (s.rms, 3));
            // all FX max
            for (auto id : { pid::skip, pid::scratch, pid::battery, pid::crush, pid::bass, pid::hp, pid::esp, pid::room, pid::gate }) R.set (id, 1);
            R.set (pid::width, 2); R.set (pid::pitch, 12); R.set (pid::speed, 1.5f);
            for (int i = 0; i < kNumEq; ++i) R.set (pid::eq (i).toRawUTF8(), 12);
            s = R.run (4);
            check (s.bad == 0 && s.peak < 4, tag + "all FX max peak=" + String (s.peak, 2) + " worst block ms=" + String (s.ms, 2));
            R.p.resetSound();
            // scratch back and forth, then release (slip)
            R.p.scrubTarget = R.p.aPos.load(); R.p.scrubbing = true;
            s = R.run (2, [&] (int k) { R.p.scrubTarget = R.p.scrubTarget.load() + std::sin (k * 0.3) * sr * 0.05; });
            R.p.scrubbing = false; R.run (1.0);
            double off = (R.p.aPos.load() - ((double) R.p.aNewest.load() - R.p.aLookahead.load())) / sr;
            check (s.bad == 0 && std::abs (off) < 0.01, tag + "scratch + slip back to live (offset " + String (off, 3) + "s)");
            // freeze while host plays
            R.p.freezeDiv = 16; s = R.run (1); R.p.freezeDiv = 0; R.run (1);
            check (s.bad == 0, tag + "freeze 1/16");
            // cues: set, hold, release
            R.p.sendCommand (CDPlayerAudioProcessor::SetCue, 0); R.run (0.5);
            check (R.p.aCues[0].load() >= 0, tag + "cue set");
            R.run (1.0); R.p.holdCue = 0; s = R.run (0.5); R.p.holdCue = -1; R.run (1.0);
            off = (R.p.aPos.load() - ((double) R.p.aNewest.load() - R.p.aLookahead.load())) / sr;
            check (s.bad == 0 && std::abs (off) < 0.01, tag + "hold cue then slip");
            // A-B loop for 2 s
            double nw = (double) R.p.aNewest.load();
            R.p.sendCommand (CDPlayerAudioProcessor::SetAB, 0, nw - sr * 2, nw - sr * 1.5);
            bool inside = true;
            s = R.run (2, [&] (int k) { if (k > 4) { double ps = R.p.aPos.load(); if (ps < R.p.aAbA.load() - 2 || ps > R.p.aAbB.load() + 2) inside = false; } });
            R.p.sendCommand (CDPlayerAudioProcessor::ClearAB); R.run (1);
            check (s.bad == 0 && inside, tag + "A-B repeat stays inside the zone");
            // reverse 3 s
            R.set (pid::rev, 1); s = R.run (3); R.set (pid::rev, 0); R.run (1);
            check (s.bad == 0, tag + "reverse");
            // tape stop / spin up
            R.p.motor = false; s = R.run (1.5); R.p.motor = true; R.run (2);
            off = (R.p.aPos.load() - ((double) R.p.aNewest.load() - R.p.aLookahead.load())) / sr;
            check (s.bad == 0 && std::abs (off) < 0.01, tag + "tape stop + spin-up returns in time");
            // rewind / fast-forward
            R.p.ffDir = -1; R.run (1); R.p.ffDir = 1; s = R.run (1); R.p.ffDir = 0; R.run (1);
            check (s.bad == 0 && R.p.aPos.load() <= (double) R.p.aNewest.load(), tag + "rew/ff never read the future");
            // skip heavy for 5 s
            R.set (pid::skip, 1); s = R.run (5); R.set (pid::skip, 0); R.run (1);
            check (s.bad == 0, tag + "CD skip 100% for 5 s");
            // lookahead off + all positional moves
            R.p.setLookaheadIndex (0); R.run (0.5);
            R.p.freezeDiv = 4; s = R.run (1); R.p.freezeDiv = 0;
            R.set (pid::skip, 1); auto s2 = R.run (2); R.set (pid::skip, 0);
            R.p.ffDir = 1; auto s3 = R.run (1); R.p.ffDir = 0; R.run (0.5);
            check (s.bad + s2.bad + s3.bad == 0 && R.p.aPos.load() <= (double) R.p.aNewest.load(), tag + "lookahead OFF: freeze/skip/ff safe");
            R.p.setLookaheadIndex (3); R.run (2);
            // dry/wet extremes
            R.set (pid::crush, 1); R.set (pid::mix, 0); auto dry = R.run (1); R.set (pid::mix, 1); auto wet = R.run (1);
            check (dry.bad == 0 && wet.bad == 0 && dry.rms > 0.02, tag + "dry/wet");
            R.p.resetSound();
            // run past the history length to check wrap-around
            s = R.run (70);
            check (s.bad == 0, tag + "70 s continuous (history wrap)");
        }

    // ---------- presets & state ----------
    {
        Runner R (48000, 512);
        for (int i = 0; i < R.p.getNumPrograms(); ++i) { R.p.setCurrentProgram (i); auto s = R.run (0.5); check (s.bad == 0, "preset " + R.p.getProgramName (i)); }
        MemoryBlock mb; R.p.getStateInformation (mb);
        CDPlayerAudioProcessor p2; p2.setStateInformation (mb.getData(), (int) mb.getSize());
        check (std::abs (p2.apvts.getRawParameterValue (pid::speed)->load() - R.p.apvts.getRawParameterValue (pid::speed)->load()) < 1e-4, "state save / load");
    }

    // ---------- editor rendering ----------
    {
        Runner R (48000, 512);
        R.set (pid::esp, 0.5f); R.set (pid::room, 0.5f); R.set (pid::gate, 0.7f); R.set (pid::crush, 0.0f);
        R.run (3);
        R.p.sendCommand (CDPlayerAudioProcessor::SetCue, 2); R.run (0.3);
        double nw = (double) R.p.aNewest.load();
        R.p.sendCommand (CDPlayerAudioProcessor::SetAB, 0, nw - 48000 * 2.5, nw - 48000 * 1.8); R.run (0.5);
        std::unique_ptr<AudioProcessorEditor> ed (R.p.createEditor());
        ed->setSize (590, 522);
        for (auto skin : { "classic", "winamp", "scope", "lcd", "wmp", "blueprint", "custom" })
        {
            R.p.uiState.setProperty ("skin", skin, nullptr);
            R.run (0.1);
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            File f = File::getCurrentWorkingDirectory().getChildFile (String ("shot_") + skin + ".png");
            f.deleteFile();
            FileOutputStream os (f); PNGImageFormat png; png.writeImageToStream (img, os);
            check (img.isValid(), String ("editor render ") + skin);
        }
        ed.reset();
    }
    std::cout << (failures == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED") << std::endl;
    return failures == 0 ? 0 : 1;
}
