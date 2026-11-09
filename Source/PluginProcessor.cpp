#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

CDPlayerAudioProcessor::CDPlayerAudioProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    for (auto& c : aCues) c.store (-1);
    for (auto& g : aGlitchPos) g.store (0);
    for (auto& c : cues) c = -1;
    uiState.setProperty ("skin", "classic", nullptr);
    uiState.setProperty ("style", "classic", nullptr);
    uiState.setProperty ("grid", true, nullptr);
    uiState.setProperty ("zoom", 5.0, nullptr);
    uiState.setProperty ("look", 0, nullptr);
    uiState.setProperty ("cBg", "ff1a1a2e", nullptr);
    uiState.setProperty ("cWave", "ffff4fd8", nullptr);
    uiState.setProperty ("cHead", "ffffffff", nullptr);
    startTimerHz (10);
}

bool CDPlayerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    auto out = layouts.getMainOutputChannelSet();
    if (out != AudioChannelSet::mono() && out != AudioChannelSet::stereo()) return false;
    return layouts.getMainInputChannelSet() == out;
}

void CDPlayerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    capacity = jmin (nextPowerOfTwo ((int) (sr * 60.0)), 1 << 23);
    capMask = capacity - 1;
    hist.setSize (2, capacity);
    hist.clear();
    int bins = capacity >> kBinShift;
    pkMin.assign ((size_t) bins, 0.0f);
    pkMax.assign ((size_t) bins, 0.0f);
    binMask = bins - 1;
    W = 0; pos = 0; rate = 1; engaged = false;
    freeze = {}; sk = {}; gl = {}; cut = {}; abOn = false; xfN = 0; burst = 0; lastHold = -1;
    for (auto& c : cues) c = -1;
    grainsInit = false;
    refIdx = 0; refPpq = 0;

    int la = lookaheadIdx.load();
    L = jmin ((int) std::round (lookaheadSeconds (la) * sr), capacity / 4);
    setLatencySamples (L);

    for (int c = 0; c < 2; ++c) { espBuf[c].assign ((size_t) (sr * 3.2) + 8, 0.0f); espLpz[c] = 0; }
    espW = 0; espDelay = 0.5 * sr;

    dsp::ProcessSpec spec { sr, (uint32) samplesPerBlock, 2 };
    conv.prepare (spec);
    roomTypeLoaded = -1;
    loadRoomIR ((int) p (pid::roomType)->load());
    roomIn.setSize (2, samplesPerBlock);
    wetBuf.setSize (2, samplesPerBlock);
    dryBuf.setSize (2, samplesPerBlock);
    comp.prepare (spec);
    comp.setThreshold (-6.0f); comp.setRatio (6.0f); comp.setAttack (3.0f); comp.setRelease (250.0f);

    shelf.reset(); hpf.reset(); pk.reset(); lp.reset();
    for (auto& e : eqs) e.reset();
    sVol.reset (sr, 0.03); sMix.reset (sr, 0.03);
    sVol.setCurrentAndTargetValue (p (pid::vol)->load());
    sMix.setCurrentAndTargetValue (p (pid::mix)->load());

    aSr = sr; aCapacity = capacity; aLookahead = L;
}

void CDPlayerAudioProcessor::timerCallback()
{
    int rt = (int) p (pid::roomType)->load();
    if (rt != roomTypeLoaded) loadRoomIR (rt);
}

void CDPlayerAudioProcessor::loadRoomIR (int type)
{
    roomTypeLoaded = type;
    struct T { double len, pw, cut; };
    static const T tt[] = { { 0.7, 3.2, 9000 }, { 0.28, 2.2, 3500 }, { 3.4, 2.4, 7000 }, { 1.6, 1.6, 12000 } };
    auto t = tt[jlimit (0, 3, type)];
    int n = (int) (sr * t.len);
    AudioBuffer<float> ir (2, n);
    Random r (1234 + type);
    double a = std::exp (-MathConstants<double>::twoPi * t.cut / sr);
    for (int ch = 0; ch < 2; ++ch)
    {
        auto* d = ir.getWritePointer (ch);
        double lps = 0;
        for (int i = 0; i < n; ++i)
        {
            double x = r.nextDouble() * 2 - 1;
            lps = lps * a + x * (1 - a);
            double env = std::pow (1.0 - (double) i / n, t.pw) * (i < sr * 0.004 ? i / (sr * 0.004) : 1.0);
            d[i] = (float) (lps * env);
        }
        if (type == 3)
            for (int k = 1; k < 6; ++k) { int i = (int) (sr * 0.011 * k * (1 + ch * 0.07)); if (i < n) d[i] += 0.5f / k; }
    }
    conv.loadImpulseResponse (std::move (ir), sr, dsp::Convolution::Stereo::yes, dsp::Convolution::Trim::no, dsp::Convolution::Normalise::yes);
}

float CDPlayerAudioProcessor::effective (const char* id) const
{
    float v = p (id)->load();
    float d = p (pid::damage)->load();
    auto r = [] (float x) { return jlimit (0.0f, 1.0f, x); };
    juce::String s (id);
    if (s == pid::skip)    return jmax (v, r (d * 0.85f));
    if (s == pid::scratch) return jmax (v, r ((d - 0.15f) * 1.05f));
    if (s == pid::battery) return jmax (v, r ((d - 0.3f) * 0.9f));
    if (s == pid::crush)   return jmax (v, r ((d - 0.45f) * 1.2f));
    if (s == pid::hp)      return jmax (v, r ((d - 0.55f) * 1.3f));
    return v;
}

void CDPlayerAudioProcessor::sendCommand (int type, int i, double a, double b)
{
    int s1, b1, s2, b2;
    fifo.prepareToWrite (1, s1, b1, s2, b2);
    if (b1 > 0) cmdBuf[(size_t) s1] = { type, i, a, b };
    else if (b2 > 0) cmdBuf[(size_t) s2] = { type, i, a, b };
    fifo.finishedWrite (b1 + b2);
}

inline float CDPlayerAudioProcessor::rd (int ch, double p0) const
{
    const double nw = (double) (W - 1);
    if (p0 > nw) p0 = nw;
    auto i = (int64) std::floor (p0);
    float f = (float) (p0 - (double) i);
    int64 j = i + 1; if (j > W - 1) j = W - 1;
    const float* d = hist.getReadPointer (ch);
    float a = d[i & capMask], b = d[j & capMask];
    return a + (b - a) * f;
}

float CDPlayerAudioProcessor::readHistMono (double absPos) const
{
    if (absPos < (double) (W - capacity + 8) || absPos > (double) (W - 1) || W == 0) return 0.0f;
    return 0.5f * (rd (0, absPos) + rd (1, absPos));
}

void CDPlayerAudioProcessor::resyncToLive (double live)
{
    xfOld = pos; xfN = kXf;
    pos = live; rate = 1; engaged = false;
}

void CDPlayerAudioProcessor::handleCommands()
{
    int s1, b1, s2, b2;
    int n = fifo.getNumReady();
    if (n <= 0) return;
    fifo.prepareToRead (n, s1, b1, s2, b2);
    auto run = [this] (const Cmd& c)
    {
        double newest = newestIdx(), oldest = oldestIdx();
        switch (c.type)
        {
            case SetCue:
            {
                double cp = pos;
                if (p (pid::quant)->load() > 0.5f)
                {
                    double unit = 0.25; // 1/16 note in quarter notes
                    double q = ppqAt (cp) / unit;
                    cp -= (q - std::floor (q)) * unit * spb();
                }
                cues[c.i & 7] = (int64) jmax (oldest, cp);
                break;
            }
            case ClearCue:  cues[c.i & 7] = -1; break;
            case ClearCues: for (auto& q : cues) q = -1; break;
            case SetAB:
            {
                double a = jlimit (oldest, newest - 2, c.a), b = jlimit (oldest, newest - 1, c.b);
                if (b - a > sr * 0.02) { abA = a; abB = b; abOn = true; engaged = true; if (pos < a || pos >= b) { xfOld = pos; xfN = kXf; pos = a; } }
                break;
            }
            case ClearAB: abOn = false; break;
            case Burst:   burst = 8; break;
            case StopNow: rate = 0; engaged = true; break;
            case Jump:    { double t = jlimit (oldest, newest - 1, c.a); xfOld = pos; xfN = kXf; pos = t; engaged = true; freeze.on = sk.on = false; break; }
            case Resync:  freeze.on = sk.on = false; abOn = false; resyncToLive (newest - L); break;
            default: break;
        }
    };
    for (int k = 0; k < b1; ++k) run (cmdBuf[(size_t) (s1 + k)]);
    for (int k = 0; k < b2; ++k) run (cmdBuf[(size_t) (s2 + k)]);
    fifo.finishedRead (b1 + b2);
}

void CDPlayerAudioProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    const int N = buffer.getNumSamples();
    const int numCh = jmin (2, buffer.getNumChannels());
    if (N == 0 || numCh == 0) return;
    if (wetBuf.getNumSamples() < N) { wetBuf.setSize (2, N, false, false, true); dryBuf.setSize (2, N, false, false, true); roomIn.setSize (2, N, false, false, true); }

    // ---- lookahead / latency ----
    {
        int want = jmin ((int) std::round (lookaheadSeconds (lookaheadIdx.load()) * sr), capacity / 4);
        if (want != L) { L = want; if (! engaged) xfN = 0; }
    }

    // ---- host timing ----
    {
        double prevPpq = ppqAt ((double) W);
        double fallback = p (pid::bpm)->load();
        bool havePpq = false; double hostPpq = 0;
        bool playing = false; double secs = aHostSeconds.load();
        if (auto* ph = getPlayHead())
            if (auto info = ph->getPosition())
            {
                if (auto b = info->getBpm()) fallback = *b;
                if (auto q = info->getPpqPosition()) { hostPpq = *q; havePpq = true; }
                if (auto t = info->getTimeInSeconds()) secs = *t;
                playing = info->getIsPlaying();
            }
        bpmNow = jlimit (20.0, 400.0, fallback);
        refPpq = (havePpq && playing) ? hostPpq : prevPpq;
        refIdx = W;
        aHostPlaying = playing;
        aHostSeconds = secs;
    }

    handleCommands();

    // ---- parameters ----
    const float vSkip = effective (pid::skip), vScratch = effective (pid::scratch), vBatt = effective (pid::battery),
                vCrush = effective (pid::crush), vHp = effective (pid::hp);
    const float vTape = p (pid::tape)->load(), vBass = p (pid::bass)->load(), vWidth = p (pid::width)->load(),
                vEsp = p (pid::esp)->load(), vRoom = p (pid::room)->load(), vGate = p (pid::gate)->load(),
                vSpeed = p (pid::speed)->load(), vPitch = p (pid::pitch)->load();
    const bool rev = p (pid::rev)->load() > 0.5f, quant = p (pid::quant)->load() > 0.5f, slip = p (pid::slip)->load() > 0.5f;
    const int gateDiv = kGateDivs[jlimit (0, 3, (int) p (pid::gateDiv)->load())];
    const int gateShape = (int) p (pid::gateShape)->load();
    const float espDiv = kEspDivs[jlimit (0, 4, (int) p (pid::espDiv)->load())];
    sVol.setTargetValue (p (pid::vol)->load());
    sMix.setTargetValue (p (pid::mix)->load());

    const bool scr = scrubbing.load(), mot = motor.load(), seekH = seekHold.load();
    const int ffd = ffDir.load();
    const double scrT = scrubTarget.load();

    // ---- hot cue hold ----
    {
        int h = holdCue.load();
        if (h != lastHold)
        {
            lastHold = h;
            if (h >= 0 && h < 8 && cues[h] >= 0 && (double) cues[h] >= oldestIdx())
            { xfOld = pos; xfN = kXf; pos = (double) cues[h]; engaged = true; freeze.on = sk.on = false; }
        }
    }
    const bool holding = lastHold >= 0;

    // ---- freeze (right click / F) ----
    {
        int fd = freezeDiv.load();
        if (fd > 0 && (! freeze.on || freeze.div != fd) && W > 0)
        {
            double base = sk.on ? sk.start + sk.c : pos;
            double len = spb() * 4.0 / fd;
            double start = base;
            if (quant) { double u = 4.0 / fd, q = ppqAt (base) / u; start = base - (q - std::floor (q)) * u * spb(); }
            if (start + len > newestIdx() - 1) start -= len;
            freeze.on = true; freeze.div = fd; freeze.start = start; freeze.len = len;
            freeze.c = std::fmod (std::fmod (base - start, len) + len, len);
            sk.on = false; engaged = true;
        }
        else if (fd == 0 && freeze.on)
        {
            freeze.on = false;
            pos = jmin (freeze.start + (rate < 0 ? 0.0 : freeze.len), newestIdx() - 1);
        }
    }

    // ---- CD skip trigger ----
    if (mot && ! scr && ! freeze.on && ! sk.on && ! holding && vSkip > 0 && rng.nextFloat() < vSkip * vSkip * 0.05f + vSkip * 0.012f * (N / 1024.0f))
    {
        static const int divs[] = { 4, 8, 16, 16, 32 };
        double len = spb() / divs[rng.nextInt (5)] * 4.0 / 4.0;
        double base = pos;
        double start = base;
        if (quant) { double u = len / spb(), q = ppqAt (base) / u; start = base - (q - std::floor (q)) * u * spb(); }
        if (start + len > newestIdx() - 1) start -= len;
        sk.on = true; sk.start = start; sk.len = len; sk.c = base - start;
        sk.n = 2 + rng.nextInt (2 + (int) (vSkip * 7));
        sk.jump = rng.nextFloat() < 0.25f ? (rng.nextDouble() - 0.35) * spb() : 0.0;
        engaged = true;
        aSkipCount++;
    }

    // ---- headphones loose jack ----
    if (! cut.on && vHp > 0.05f && mot && rng.nextFloat() < vHp * vHp * 0.012f * (N / 1024.0f))
    {
        cut.on = true; cut.side = rng.nextInt (2); cut.n = (int) (sr * (0.05 + rng.nextDouble() * 0.4)); cut.c = 0;
        aCutSide = cut.side; aCutCount++;
    }

    // battery drift (slow random drag)
    driftT -= N;
    if (driftT <= 0) { driftT = (int) (sr * (0.2 + rng.nextDouble() * 0.8)); driftTarget = rng.nextDouble() * vBatt * 0.03 * sr; }

    const double pr = vSpeed * std::pow (2.0, vPitch / 12.0);
    const bool shift = std::abs (pr - 1.0) > 0.002;
    const double G = 0.085 * sr;
    if (! shift) grainsInit = false;

    const int crushN = 1 + (int) (vCrush * vCrush * 22);
    const float q = std::pow (2.0f, 16.0f - vCrush * 12.0f) / 2.0f;
    const float Wd = vWidth * (1.0f - vHp * 0.75f);
    const double gUnitQ = 4.0 / gateDiv;

    const float* in0 = buffer.getReadPointer (0);
    const float* in1 = numCh > 1 ? buffer.getReadPointer (1) : in0;
    float* w0 = wetBuf.getWritePointer (0); float* w1 = wetBuf.getWritePointer (1);
    float* d0 = dryBuf.getWritePointer (0); float* d1 = dryBuf.getWritePointer (1);
    float* h0 = hist.getWritePointer (0);   float* h1 = hist.getWritePointer (1);

    double lastRp = pos;

    for (int i = 0; i < N; ++i)
    {
        // write the input into the history
        {
            float inL = in0[i], inR = in1[i];
            int wi = (int) (W & capMask);
            h0[wi] = inL; h1[wi] = inR;
            float m = (inL + inR) * 0.5f;
            if ((W & ((1 << kBinShift) - 1)) == 0) { accMin = accMax = m; }
            else { accMin = jmin (accMin, m); accMax = jmax (accMax, m); }
            int bi = (int) ((W >> kBinShift) & binMask);
            pkMin[(size_t) bi] = accMin; pkMax[(size_t) bi] = accMax;
            ++W;
        }
        const double newest = (double) (W - 1);
        const double live = newest - L;

        const bool interacting = scr || ffd != 0 || ! mot || rev || freeze.on || sk.on || holding || abOn || seekH;
        if (interacting) engaged = true;

        double rp;
        double r = 1.0;
        if (! engaged) { pos = live; rate = 1; rp = pos; }
        else
        {
            double target, lin = 0, expo = 0;
            if (scr) { target = jlimit (-12.0, 12.0, (scrT - pos) / (N * 1.6)); expo = 0.004; }
            else if (ffd != 0) { target = ffd * 3.2; lin = 1.0 / (0.12 * sr); }
            else if (! mot) { target = 0; lin = vTape < 0.02f ? 9.0 : 1.0 / (vTape * sr); }
            else
            {
                target = rev ? -1.0 : 1.0;
                lin = vTape < 0.02f ? 9.0 : 1.0 / (vTape * 0.6 * sr);
                if (std::abs (rate - target) > 1.0) lin = jmax (lin, 1.0 / (0.05 * sr)); // let go of a fast scratch: platter catches up quickly
            }
            if (expo > 0) rate += (target - rate) * expo;
            else { double d = target - rate; rate += std::abs (d) < lin ? d : (d > 0 ? lin : -lin); }
            r = rate;

            if (freeze.on)
            {
                freeze.c += r;
                if (freeze.c >= freeze.len) freeze.c -= freeze.len; else if (freeze.c < 0) freeze.c += freeze.len;
                rp = freeze.start + freeze.c;
            }
            else if (sk.on)
            {
                sk.c += r;
                if (sk.c >= sk.len || sk.c < 0)
                {
                    sk.c += sk.c < 0 ? sk.len : -sk.len;
                    if (--sk.n <= 0) { pos = r < 0 ? sk.start : sk.start + sk.len + sk.jump; sk.on = false; }
                }
                rp = sk.on ? sk.start + sk.c : pos;
            }
            else
            {
                pos += r;
                if (abOn && ! scr && (pos >= abB || pos < abA))
                {
                    double Lr = abB - abA;
                    pos = abA + std::fmod (std::fmod (pos - abA, Lr) + Lr, Lr);
                }
                rp = pos;
            }
            // bounds
            if (pos > newest - 1) { pos = newest - 1; if (rate > 1) rate = 1; }
            if (pos < oldestIdx()) { abOn = false; resyncToLive (live); rp = pos; }
            if (rp > newest - 1) rp = newest - 1;

            // release
            if (! interacting && engaged && std::abs (rate - 1.0) < 0.01)
            {
                if (slip) { resyncToLive (live); rp = pos; }
            }
        }

        // battery wow & flutter (modulated delay — stays real time)
        if (vBatt > 0)
        {
            wob += MathConstants<double>::twoPi * (0.6 + vBatt * 2.4) / sr;
            if (wob > MathConstants<double>::twoPi) wob -= MathConstants<double>::twoPi;
            drift += (driftTarget - drift) * 0.00004;
            wowDelay = vBatt * sr * 0.004 * (1.0 + std::sin (wob)) + drift;
            rp -= wowDelay;
        }
        else drift = wowDelay = 0;

        float l, rr;
        if (shift && std::abs (rate) > 0.05)
        {
            const double sg = rate < 0 ? -1.0 : 1.0;
            const double back = G * jmax (0.0, pr - 1.0) + 2.0;
            if (! grainsInit) { for (int g = 0; g < 4; ++g) { grains[g].ph = G * g / 4.0; grains[g].a = rp - back - grains[g].ph * pr * sg; } grainsInit = true; }
            l = rr = 0;
            for (int g = 0; g < 4; ++g)
            {
                auto& gr = grains[g];
                if (gr.ph >= G) { gr.ph -= G; gr.a = rp - back + (rng.nextDouble() - 0.5) * G * 0.15; }
                float wv = (float) (0.5 - 0.5 * std::cos (MathConstants<double>::twoPi * gr.ph / G));
                double pp = gr.a + gr.ph * pr * sg;
                l += wv * rd (0, pp); rr += wv * rd (1, pp);
                gr.ph += 1.0;
            }
            l *= 0.64f; rr *= 0.64f;
        }
        else { l = rd (0, rp); rr = rd (1, rp); }

        if (xfN > 0)
        {
            float mixOld = (float) xfN / kXf;
            l = l * (1 - mixOld) + rd (0, xfOld) * mixOld;
            rr = rr * (1 - mixOld) + rd (1, xfOld) * mixOld;
            xfOld += 1.0; --xfN;
        }
        lastRp = rp;

        // scratched disc
        if (! gl.on && (burst > 0 || (vScratch > 0 && std::abs (r) > 0.2 && rng.nextFloat() < vScratch * vScratch * 0.00012f + vScratch * 0.00002f)))
        {
            if (burst > 0 && rng.nextFloat() > 0.002f) {}
            else
            {
                if (burst > 0) --burst;
                float tp = rng.nextFloat();
                gl.on = true; gl.t = tp < 0.45f ? 0 : tp < 0.85f ? 1 : 2;
                gl.n = (int) (sr * (0.008 + rng.nextDouble() * 0.07)); gl.c = 0; gl.start = rp; gl.seg = 64 + rng.nextInt (600);
                int gc = aGlitchCount.load();
                aGlitchPos[(size_t) (gc & 31)] = rp; aGlitchCount = gc + 1;
            }
        }
        if (gl.on)
        {
            if (gl.t == 0)
            {
                float fd = jmin (1.0f, (float) jmin (gl.c, gl.n - gl.c) / 40.0f);
                l *= 1 - fd; rr *= 1 - fd;
                if (gl.c < 3) { l += 0.5f; rr -= 0.4f; }
            }
            else if (gl.t == 1) { double pp = gl.start + (gl.c % gl.seg); l = rd (0, pp); rr = rd (1, pp); }
            else { l = l * 0.4f + (rng.nextFloat() - 0.5f) * 0.35f; rr = rr * 0.4f + (rng.nextFloat() - 0.5f) * 0.35f; }
            if (++gl.c >= gl.n) gl.on = false;
        }

        // lo-fi laser
        if (vCrush > 0)
        {
            if (++hc >= crushN) { hc = 0; hl = std::round (l * q) / q; hr = std::round (rr * q) / q; }
            l = hl; rr = hr;
        }

        // gate (synced to the host grid)
        if (vGate > 0)
        {
            double ph = ppqAt (rp) / gUnitQ; ph -= std::floor (ph);
            float gv = gateShape == 0 ? (ph < 0.55 ? 1.0f : 0.0f) : gateShape == 1 ? (float) (1.0 - ph) : (float) (0.5 + 0.5 * std::cos (ph * MathConstants<double>::twoPi));
            float tg = 1.0f - vGate * (1.0f - gv);
            gg += (tg - gg) * 0.006f;
            l *= gg; rr *= gg;
        }

        // headphones crackle + loose jack
        if (vHp > 0)
        {
            if (rng.nextFloat() < vHp * 0.0005f) { float c = (rng.nextFloat() - 0.5f) * 0.6f * vHp; l += c; rr += c * 0.7f; }
            if (cut.on)
            {
                float e = jmin (1.0f, (float) jmin (cut.c, cut.n - cut.c) / 80.0f);
                if (cut.side) rr *= 1 - e * 0.95f; else l *= 1 - e * 0.95f;
                if (++cut.c >= cut.n) cut.on = false;
            }
        }

        const float amp = engaged ? (float) jmin (1.0, std::abs (rate) * 3.0) : 1.0f;
        const float m = (l + rr) * 0.5f, s = (l - rr) * 0.5f * Wd;
        w0[i] = (m + s) * amp; w1[i] = (m - s) * amp;

        // dry = the untouched input, aligned with the lookahead
        d0[i] = rd (0, live); d1[i] = rd (1, live);
    }

    // ---- tone section (per block coefficients) ----
    {
        double nyq = sr * 0.45;
        shelf.lowShelf (sr, 95, vBass * 15.0);
        hpf.highPass (sr, 20.0 + vHp * vHp * 420.0, 0.6);
        pk.peak (sr, 2600, 0.9, vHp * 7.0);
        double lpf = jmin (20000.0 * std::pow (1.0 - vCrush, 2.2) + 1600.0, 20000.0 - vHp * 13500.0);
        lp.lowPass (sr, jmin (lpf, nyq), 0.5);
        for (int k = 0; k < kNumEq; ++k)
            eqs[k].peak (sr, jmin ((double) kEqFreqs[k], nyq), 1.4, p (pid::eq (k))->load());
    }
    float* r0 = roomIn.getWritePointer (0); float* r1 = roomIn.getWritePointer (1);
    const double espTarget = jlimit (0.02, 2.9, 60.0 / bpmNow * espDiv) * sr;
    const float espLpA = (float) std::exp (-MathConstants<double>::twoPi * 3200.0 / sr);
    const int espLen = (int) espBuf[0].size();
    const float roomGain = vRoom * (roomTypeLoaded == 2 ? 0.9f : 1.3f);
    const bool useRoom = vRoom > 0.0005f;

    for (int i = 0; i < N; ++i)
    {
        float x[2] = { w0[i], w1[i] };
        for (int c = 0; c < 2; ++c)
        {
            float v = shelf.process (x[c], c);
            v = hpf.process (v, c); v = pk.process (v, c); v = lp.process (v, c);
            for (int k = 0; k < kNumEq; ++k) v = eqs[k].process (v, c);
            x[c] = v;
        }
        // anti-shock echo
        espDelay += (espTarget - espDelay) * 0.0005;
        espLfo += MathConstants<double>::twoPi * 0.35 / sr;
        if (espLfo > MathConstants<double>::twoPi) espLfo -= MathConstants<double>::twoPi;
        double dl = espDelay + std::sin (espLfo) * 0.003 * sr;
        float y[2] = { 0, 0 };
        for (int c = 0; c < 2; ++c)
        {
            double rpos = espW - dl; while (rpos < 0) rpos += espLen;
            int i0 = (int) rpos; float f = (float) (rpos - i0); int i1 = (i0 + 1) % espLen;
            float dv = espBuf[c][(size_t) i0] + (espBuf[c][(size_t) i1] - espBuf[c][(size_t) i0]) * f;
            espLpz[c] = espLpz[c] * espLpA + dv * (1 - espLpA);
            y[c] = espLpz[c];
            espBuf[c][(size_t) espW] = x[c] * vEsp * 0.75f + y[c] * vEsp * 0.62f;
        }
        espW = (espW + 1) % espLen;
        r0[i] = x[0] * roomGain; r1[i] = x[1] * roomGain;
        w0[i] = x[0] + y[0]; w1[i] = x[1] + y[1];
    }
    if (useRoom)
    {
        dsp::AudioBlock<float> rb (roomIn.getArrayOfWritePointers(), 2, (size_t) N);
        conv.process (dsp::ProcessContextReplacing<float> (rb));
        for (int i = 0; i < N; ++i) { w0[i] += r0[i]; w1[i] += r1[i]; }
    }

    // ---- output: wet * mix + dry * (1 - mix), volume, safety compressor ----
    float* o0 = buffer.getWritePointer (0);
    float* o1 = numCh > 1 ? buffer.getWritePointer (1) : nullptr;
    for (int i = 0; i < N; ++i)
    {
        float vol = sVol.getNextValue(), mx = sMix.getNextValue();
        float a = w0[i] * vol * mx + d0[i] * vol * (1 - mx);
        float b = w1[i] * vol * mx + d1[i] * vol * (1 - mx);
        if (o1) { o0[i] = a; o1[i] = b; } else o0[i] = 0.5f * (a + b);
    }
    {
        dsp::AudioBlock<float> ob (buffer.getArrayOfWritePointers(), (size_t) numCh, (size_t) N);
        comp.process (dsp::ProcessContextReplacing<float> (ob));
    }
    for (int c = numCh; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, N);

    // ---- snapshot for the display ----
    aPos = lastRp + wowDelay; aRate = engaged ? rate : 1.0; aNewest = W - 1; aLookahead = L;
    aBpm = bpmNow; aRefPpq = refPpq; aRefIdx = refIdx;
    aFreezeStart = freeze.start; aFreezeLen = freeze.on ? freeze.len : 0.0; aFreezeDivShown = freeze.on ? freeze.div : 0;
    aAbOn = abOn; aAbA = abA; aAbB = abB; aEngaged = engaged; aGateGain = gg;
    for (int k = 0; k < 8; ++k)
    {
        if (cues[k] >= 0 && (double) cues[k] < oldestIdx()) cues[k] = -1;
        aCues[(size_t) k] = cues[k];
    }
}

// ---------------- presets / state ----------------
void CDPlayerAudioProcessor::applyPresetValues (const std::vector<std::pair<String, float>>& values)
{
    for (auto& id : resetParamIds())
        if (auto* prm = apvts.getParameter (id)) prm->setValueNotifyingHost (prm->getDefaultValue());
    for (auto& kv : values)
        if (auto* prm = apvts.getParameter (kv.first)) prm->setValueNotifyingHost (prm->convertTo0to1 (kv.second));
}

void CDPlayerAudioProcessor::resetSound()
{
    for (auto& id : resetParamIds())
        if (auto* prm = apvts.getParameter (id)) prm->setValueNotifyingHost (prm->getDefaultValue());
    freezeDiv = 0;
}

void CDPlayerAudioProcessor::setCurrentProgram (int index)
{
    auto& fp = factoryPresets();
    if (index < 0 || index >= (int) fp.size()) return;
    currentProgram = index;
    applyPresetValues (fp[(size_t) index].values);
}

const String CDPlayerAudioProcessor::getProgramName (int index)
{
    auto& fp = factoryPresets();
    return index >= 0 && index < (int) fp.size() ? String (fp[(size_t) index].name) : String();
}

void CDPlayerAudioProcessor::setLookaheadIndex (int idx)
{
    idx = jlimit (0, 3, idx);
    lookaheadIdx = idx;
    uiState.setProperty ("look", idx, nullptr);
    int newL = jmin ((int) std::round (lookaheadSeconds (idx) * sr), jmax (1, capacity / 4));
    setLatencySamples (newL);
    updateHostDisplay (ChangeDetails().withLatencyChanged (true));
}

void CDPlayerAudioProcessor::getStateInformation (MemoryBlock& destData)
{
    auto state = apvts.copyState();
    auto ui = uiState.createCopy();
    state.removeChild (state.getChildWithName ("UI"), nullptr);
    state.appendChild (ui, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void CDPlayerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto state = ValueTree::fromXml (*xml);
        if (! state.isValid()) return;
        auto ui = state.getChildWithName ("UI");
        if (ui.isValid())
        {
            uiState.copyPropertiesFrom (ui, nullptr);
            state.removeChild (ui, nullptr);
            lookaheadIdx = jlimit (0, 3, (int) uiState.getProperty ("look", 0));
            setLatencySamples (jmin ((int) std::round (lookaheadSeconds (lookaheadIdx.load()) * sr), jmax (1, capacity / 4)));
        }
        apvts.replaceState (state);
    }
}

AudioProcessorEditor* CDPlayerAudioProcessor::createEditor() { return new CDPlayerAudioProcessorEditor (*this); }

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new CDPlayerAudioProcessor(); }
