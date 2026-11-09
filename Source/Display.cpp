#include "Display.h"
#include "LookAndFeel95.h"

using namespace juce;

const StringArray Display::skinIds   { "classic", "winamp", "scope", "lcd", "wmp", "blueprint", "custom" };
const StringArray Display::skinNames { "Classic", "Winamp", "Oscilloscope", "LCD Discman", "Media Player 98", "Blueprint", "Custom" };

namespace
{
    struct SkinCols
    {
        Colour bar, beat, head, sel, gate, txt, flag, wave, stroke;
        bool streak = false, dots = false, paper = false, hasStroke = false, ghost = false;
    };

    Colour col (const var& v, Colour fb) { auto s = v.toString(); return s.isEmpty() ? fb : Colour::fromString (s); }

    SkinCols colsFor (const String& s, Colour cBg, Colour cWave, Colour cHead)
    {
        SkinCols c;
        if (s == "winamp")
        {
            c.bar = Colour (50, 140, 50).withAlpha (0.55f); c.beat = Colour (40, 100, 40).withAlpha (0.22f); c.dots = true;
            c.head = Colours::white; c.sel = Colour (0, 90, 255).withAlpha (0.38f); c.gate = Colours::black; c.txt = Colour (0xff3dff2a); c.flag = Colour (0xffffd200);
        }
        else if (s == "scope")
        {
            c.bar = Colour (70, 170, 90).withAlpha (0.25f); c.beat = Colour (70, 170, 90).withAlpha (0.1f);
            c.head = Colour (0xff6dff8a); c.sel = Colour (80, 255, 120).withAlpha (0.18f); c.gate = Colour (2, 16, 6); c.txt = Colour (0xff6dff8a); c.flag = c.txt;
        }
        else if (s == "lcd")
        {
            c.bar = Colour (30, 45, 25).withAlpha (0.4f); c.beat = Colour (30, 45, 25).withAlpha (0.14f); c.wave = Colour (0xff1f2b18);
            c.head = c.wave; c.sel = c.wave.withAlpha (0.25f); c.gate = Colour (170, 190, 140); c.txt = c.wave; c.flag = c.wave; c.ghost = true;
        }
        else if (s == "wmp")
        {
            c.bar = Colour (140, 170, 255).withAlpha (0.28f); c.beat = Colour (140, 170, 255).withAlpha (0.09f);
            c.head = Colour (0xffcfe0ff); c.sel = Colour (160, 120, 255).withAlpha (0.3f); c.gate = Colour (5, 8, 35); c.txt = c.head; c.flag = Colour (0xffc7a2ff);
        }
        else if (s == "blueprint")
        {
            c.bar = Colours::white.withAlpha (0.4f); c.beat = Colours::white.withAlpha (0.14f); c.wave = Colours::white.withAlpha (0.1f);
            c.stroke = Colours::white; c.hasStroke = true; c.head = Colour (0xffffe066); c.sel = Colours::white.withAlpha (0.15f);
            c.gate = Colour (27, 76, 143); c.txt = Colours::white; c.flag = c.head; c.paper = true;
        }
        else if (s == "custom")
        {
            c.bar = cHead.withAlpha (0.4f); c.beat = cHead.withAlpha (0.13f); c.wave = cWave; c.head = cHead;
            c.sel = cWave.withAlpha (0.3f); c.gate = cBg; c.txt = cHead; c.flag = cWave;
        }
        else
        {
            c.bar = Colour (35, 35, 38).withAlpha (0.75f); c.beat = Colour (40, 40, 44).withAlpha (0.28f); c.streak = true;
            c.wave = Colour (0xffd8d8da); c.head = Colour (215, 215, 218).withAlpha (0.85f); c.sel = Colour (0, 0, 128).withAlpha (0.5f);
            c.gate = Colour (85, 85, 88); c.txt = Colours::white; c.flag = Colour (0xffffff00);
        }
        return c;
    }

    const Colour cueCols[8] = { Colour (0xffff3b30), Colour (0xffff9500), Colour (0xffffd60a), Colour (0xff34c759),
                                Colour (0xff00c7be), Colour (0xff0a84ff), Colour (0xffbf5af2), Colour (0xffff2d92) };
}

Display::Display (CDPlayerAudioProcessor& p) : proc (p)
{
    setOpaque (true);
    setMouseCursor (MouseCursor::DraggingHandCursor);
}

void Display::toast (const String& t) { toastText = t; toastTime = Time::getMillisecondCounterHiRes() / 1000.0; }

float Display::freezeDivFromY (float yFrac) const
{
    static const int divs[] = { 32, 16, 8, 4 };
    return (float) divs[jlimit (0, 3, (int) std::floor (yFrac * 4.0f))];
}

void Display::startFreezeFromY (float yFrac)
{
    int d = (int) freezeDivFromY (yFrac);
    if (proc.freezeDiv.load() != d) { proc.freezeDiv = d; toast ("Freeze 1/" + String (d)); }
}

void Display::stopFreeze() { proc.freezeDiv = 0; }

double Display::xToAbs (float x) const
{
    return pos + ((double) x / jmax (1, getWidth()) - lastCx) * zoom * sr;
}

void Display::tick()
{
    int sc = proc.aSkipCount.load();
    if (sc != lastSkipCount) { lastSkipCount = sc; flashSkip = 1.0f; }
    int gc = proc.aGlitchCount.load();
    if (gc - lastGlitchCount > 32) lastGlitchCount = gc - 32;
    while (lastGlitchCount < gc) { glitches.push_back ({ proc.aGlitchPos[(size_t) (lastGlitchCount & 31)].load(), 1.0f }); ++lastGlitchCount; }
    int cc = proc.aCutCount.load();
    if (cc != lastCutCount) { lastCutCount = cc; cutSide = proc.aCutSide.load(); cutTime = Time::getMillisecondCounterHiRes() / 1000.0; }
    repaint();
}

void Display::paint (Graphics& g)
{
    auto b = getLocalBounds();
    const float phys = jmax (1.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
    const String skin = proc.uiState.getProperty ("skin", "classic").toString();
    const float crush = proc.effective (pid::crush);
    float ds = 1.0f - crush * 0.82f;
    if (skin == "lcd") ds = jmin (ds, 0.3f);
    const int w = jmax (40, (int) std::round (b.getWidth() * phys * ds));
    const int h = jmax (30, (int) std::round (b.getHeight() * phys * ds));
    if (! img.isValid() || img.getWidth() != w || img.getHeight() != h)
    {
        img = Image (Image::ARGB, w, h, true);
        scopeImg = Image (Image::ARGB, w, h, true);
        Graphics gs (scopeImg); gs.fillAll (Colour (2, 16, 6));
    }
    {
        Graphics gi (img);
        render (gi, w, h, (float) w / (float) jmax (1, b.getWidth()));
    }
    g.setImageResamplingQuality ((crush > 0.05f || skin == "lcd") ? Graphics::lowResamplingQuality : Graphics::mediumResamplingQuality);
    g.drawImage (img, b.toFloat());

    if (skin == "lcd")
    {
        g.setColour (Colour (175, 195, 145).withAlpha (0.55f));
        float step = (float) b.getWidth() / (float) w, line = 1.0f / phys;
        for (float x = 0; x < (float) b.getWidth(); x += step) g.fillRect (x, 0.0f, line, (float) b.getHeight());
        for (float y = 0; y < (float) b.getHeight(); y += step) g.fillRect (0.0f, y, (float) b.getWidth(), line);
    }

    // toast
    double now = Time::getMillisecondCounterHiRes() / 1000.0;
    if (now - toastTime < 1.3 && toastText.isNotEmpty())
    {
        auto f = w95::uiFont (13);
        int tw = GlyphArrangement::getStringWidthInt (f, toastText) + 16;
        auto tr = Rectangle<int> ((b.getWidth() - tw) / 2, 10, tw, 20);
        float a = (float) jlimit (0.0, 1.0, (1.3 - (now - toastTime)) / 0.25);
        g.setColour (w95::tip.withAlpha (a)); g.fillRect (tr);
        g.setColour (Colours::black.withAlpha (a)); g.drawRect (tr);
        g.setFont (f); g.drawText (toastText, tr, Justification::centred);
    }
}

void Display::render (Graphics& gi, int w, int h, float sx)
{
    // ---------- snapshot ----------
    sr = proc.aSr.load(); pos = proc.aPos.load(); bpm = proc.aBpm.load(); refPpq = proc.aRefPpq.load();
    newest = proc.aNewest.load(); refIdx = proc.aRefIdx.load(); cap = proc.aCapacity.load(); look = proc.aLookahead.load();
    zoom = (double) proc.uiState.getProperty ("zoom", 5.0);
    const String skin = proc.uiState.getProperty ("skin", "classic").toString();
    const String style = skin == "wmp" ? String ("mirror") : proc.uiState.getProperty ("style", "classic").toString();
    const bool grid = (bool) proc.uiState.getProperty ("grid", true);
    const Colour cBg = col (proc.uiState.getProperty ("cBg"), Colour (0xff1a1a2e));
    const Colour cWave = col (proc.uiState.getProperty ("cWave"), Colour (0xffff4fd8));
    const Colour cHead = col (proc.uiState.getProperty ("cHead"), Colours::white);
    const SkinCols sk = colsFor (skin, cBg, cWave, cHead);

    const float vBatt = proc.effective (pid::battery);
    const float vEsp = proc.apvts.getRawParameterValue (pid::esp)->load();
    const float vRoom = proc.apvts.getRawParameterValue (pid::room)->load();
    const float vGate = proc.apvts.getRawParameterValue (pid::gate)->load();
    const bool rev = proc.apvts.getRawParameterValue (pid::rev)->load() > 0.5f;
    const int gateDiv = kGateDivs[jlimit (0, 3, (int) proc.apvts.getRawParameterValue (pid::gateDiv)->load())];
    const int gateShape = (int) proc.apvts.getRawParameterValue (pid::gateShape)->load();
    const float espDiv = kEspDivs[jlimit (0, 4, (int) proc.apvts.getRawParameterValue (pid::espDiv)->load())];

    const double oldest = (double) (newest - cap + 8);
    const double spp = zoom * sr / w;
    const double spbS = 60.0 / bpm * sr;
    auto ppqAt = [&] (double idx) { return refPpq + (idx - (double) refIdx) / spbS; };
    auto idxAtPpq = [&] (double q) { return (double) refIdx + (q - refPpq) * spbS; };

    float jit = 0;
    if (flashSkip > 0) { jit = (rng.nextFloat() - 0.5f) * 14 * sx * flashSkip; flashSkip *= 0.82f; if (flashSkip < 0.05f) flashSkip = 0; }
    const double nowS = Time::getMillisecondCounterHiRes() / 1000.0;
    const float wobble = vBatt > 0 ? (float) std::sin (nowS * 1000.0 / 180.0) * vBatt * 5 * sx : 0.0f;
    // playhead position: centred when there is lookahead to show on the right, pushed to the right when there is none
    {
        double target = jlimit (0.5, 0.94, 1.0 - ((double) look / sr) / zoom - 0.06);
        headFrac += (target - headFrac) * 0.15;
        if (std::abs (target - headFrac) < 0.0005) headFrac = target;
    }
    const float cx = (float) (w * headFrac) + jit + wobble;
    lastCx = (double) cx / w;
    auto xOf = [&] (double idx) { return (float) (cx + (idx - pos) / spp); };
    const float fs = jmax (8.0f, 13.0f * sx);
    gi.setFont (w95::uiFont (fs, true));

    // ---------- background ----------
    if (skin == "winamp") gi.fillAll (Colours::black);
    else if (skin == "scope") gi.fillAll (Colour (2, 16, 6));
    else if (skin == "lcd") gi.setGradientFill (ColourGradient (Colour (0xffa3b886), 0, 0, Colour (0xffaec290), 0, (float) h, false)), gi.fillAll();
    else if (skin == "wmp") { ColourGradient cg (Colour (0xff03061c), 0, 0, Colour (0xff16205e), 0, (float) h, false); cg.addColour (0.5, Colour (0xff0b1550)); gi.setGradientFill (cg); gi.fillAll(); }
    else if (skin == "blueprint") gi.fillAll (Colour (0xff1b4c8f));
    else if (skin == "custom") { gi.setGradientFill (ColourGradient (cBg, 0, 0, cBg.brighter (0.25f), 0, (float) h, false)); gi.fillAll(); }
    else { ColourGradient cg (Colour (0xff4a4a4c), 0, 0, Colour (0xff7a7a7d), 0, (float) h, false); cg.addColour (0.55, Colour (0xff5f5f62)); gi.setGradientFill (cg); gi.fillAll(); }

    if (newest <= 0) return;

    if (skin == "scope")
    {
        Graphics gs (scopeImg);
        gs.setColour (Colour (2, 16, 6).withAlpha (0.3f)); gs.fillAll();
        const double win = zoom / 150.0 * sr;
        Path tr;
        for (int x = 0; x < w; ++x)
        {
            double idx = pos - win * (1.0 - (double) x / w);
            float v = proc.readHistMono (idx) * proc.aGateGain.load();
            float y = (float) h / 2 - v * (float) h * 0.7f;
            if (x == 0) tr.startNewSubPath (0, y); else tr.lineTo ((float) x, y);
        }
        gs.setColour (Colour (0xff2fff88).withAlpha (0.25f)); gs.strokePath (tr, PathStrokeType (5.0f * sx));
        gs.setColour (Colour (0xff6dff8a)); gs.strokePath (tr, PathStrokeType (jmax (1.0f, 1.8f * sx)));
        gi.drawImageAt (scopeImg, 0, 0);
        gi.setColour (Colour (70, 170, 90).withAlpha (0.18f));
        for (int i = 1; i < 10; ++i) gi.fillRect ((float) w * i / 10, 0.0f, jmax (1.0f, sx), (float) h);
        for (int i = 1; i < 8; ++i)  gi.fillRect (0.0f, (float) h * i / 8, (float) w, jmax (1.0f, sx));
        gi.setColour (Colour (70, 170, 90).withAlpha (0.4f));
        gi.fillRect (0.0f, (float) h / 2, (float) w, jmax (1.0f, sx)); gi.fillRect ((float) w / 2, 0.0f, jmax (1.0f, sx), (float) h);
    }
    else
    {
        if (sk.paper)
        {
            gi.setColour (Colours::white.withAlpha (0.07f));
            float st = jmax (4.0f, 12 * sx);
            for (float x = 0; x < w; x += st) gi.fillRect (x, 0.0f, 1.0f, (float) h);
            for (float y = 0; y < h; y += st) gi.fillRect (0.0f, y, (float) w, 1.0f);
        }
        if (sk.dots)
        {
            gi.setColour (Colour (40, 110, 40).withAlpha (0.35f));
            float st = jmax (4.0f, 10 * sx), off = (float) std::fmod (-(pos / spp), (double) st); if (off < 0) off += st;
            for (float y = st / 2; y < h; y += st) for (float x = off; x < w; x += st) gi.fillRect (x, y, jmax (1.0f, sx * 1.4f), jmax (1.0f, sx * 1.4f));
        }
        // beat grid
        if (grid)
        {
            double qL = ppqAt (pos - cx * spp), qR = ppqAt (pos + (w - cx) * spp);
            for (double k = std::floor (qL); k <= std::ceil (qR); k += 1.0)
            {
                float x = xOf (idxAtPpq (k));
                if (x < 0 || x > w) continue;
                bool bar = ((int64) k % 4 + 4) % 4 == 0;
                gi.setColour (bar ? sk.bar : sk.beat);
                gi.fillRect (std::round (x), 0.0f, jmax (1.0f, (bar ? 2.2f : 1.2f) * sx), (float) h);
                if (sk.streak) { gi.setColour (Colours::white.withAlpha (0.035f)); gi.fillRect (std::round (xOf (idxAtPpq (k + 0.5))), 0.0f, jmax (1.0f, sx), (float) h); }
            }
        }
        // A-B zone
        bool abShow = drag == Drag::ab || proc.aAbOn.load();
        if (abShow)
        {
            double a = drag == Drag::ab ? abA : proc.aAbA.load(), bb = drag == Drag::ab ? abB : proc.aAbB.load();
            float x0 = xOf (a), x1 = xOf (bb);
            gi.setColour (sk.sel); gi.fillRect (x0, 0.0f, x1 - x0, (float) h);
            gi.setColour (sk.txt);
            gi.fillRect (x0, 0.0f, jmax (1.0f, sx * 1.5f), (float) h); gi.fillRect (x1, 0.0f, jmax (1.0f, sx * 1.5f), (float) h);
            gi.drawText ("A", Rectangle<float> (x0 + 4 * sx, h - 22 * sx, 20 * sx, 18 * sx), Justification::centredLeft);
            gi.drawText ("B", Rectangle<float> (x1 - 14 * sx, h - 22 * sx, 20 * sx, 18 * sx), Justification::centredLeft);
        }
        const double fLen = proc.aFreezeLen.load();
        if (fLen > 0)
        {
            float x0 = xOf (proc.aFreezeStart.load()), x1 = xOf (proc.aFreezeStart.load() + fLen);
            gi.setColour (skin == "classic" ? Colour (0, 0, 128).withAlpha (0.55f) : sk.sel);
            gi.fillRect (x0, 0.0f, jmax (2.0f, x1 - x0), (float) h);
        }

        // ---------- waveform ----------
        const bool wmp = skin == "wmp";
        const float mid = wmp ? h * 0.6f : h * 0.52f, amp = wmp ? h * 0.42f : h * 0.27f;
        const bool bars = style == "bars";
        const bool mirror = style != "classic";
        const int step = bars ? jmax (2, (int) std::round (7 * sx)) : 1;
        std::vector<Point<float>> top, bot;
        top.reserve ((size_t) (w / step + 2)); bot.reserve ((size_t) (w / step + 2));
        const int binMask = proc.pkMin.empty() ? 0 : (int) proc.pkMin.size() - 1;
        for (int x = 0; x < w; x += step)
        {
            double i0 = pos + (x - cx) * spp, i1 = i0 + spp * step;
            float a = 0, z = 0;
            if (i1 >= oldest && i0 <= (double) newest && binMask > 0)
            {
                auto b0 = (int64) jmax (oldest, i0) >> CDPlayerAudioProcessor::kBinShift;
                auto b1 = (int64) jmin ((double) newest, i1) >> CDPlayerAudioProcessor::kBinShift;
                if (b1 - b0 > 512) b0 = b1 - 512;
                float mn = 1, mx = -1;
                for (auto bi = b0; bi <= b1; ++bi)
                {
                    mn = jmin (mn, proc.pkMin[(size_t) (bi & binMask)]);
                    mx = jmax (mx, proc.pkMax[(size_t) (bi & binMask)]);
                }
                z = std::pow (jlimit (0.0f, 1.0f, mx), 0.55f);
                a = -std::pow (jlimit (0.0f, 1.0f, -mn), 0.55f);
            }
            if (mirror) { float m = jmax (-a, z); a = -m; z = m; }
            top.push_back ({ (float) x, jmax (z, 0.012f) });
            bot.push_back ({ (float) x, jmin (a, -0.012f) });
        }
        Path shape;
        if (! top.empty())
        {
            if (wmp)
            {
                if (bars) for (auto& t : top) shape.addRectangle (t.x, mid - t.y * amp * 1.8f, step - jmax (1.0f, 2 * sx), t.y * amp * 1.8f);
                else
                {
                    shape.startNewSubPath (top.front().x, mid);
                    for (auto& t : top) shape.lineTo (t.x, mid - t.y * amp * 1.8f);
                    shape.lineTo (top.back().x, mid); shape.closeSubPath();
                }
            }
            else if (bars)
            {
                for (size_t k = 0; k < top.size(); ++k)
                {
                    float y0 = mid - top[k].y * amp, y1 = mid - bot[k].y * amp;
                    shape.addRectangle (top[k].x, y0, step - jmax (1.0f, 2 * sx), y1 - y0);
                }
            }
            else
            {
                shape.startNewSubPath (top.front().x, mid - top.front().y * amp);
                for (auto& t : top) shape.lineTo (t.x, mid - t.y * amp);
                for (auto it = bot.rbegin(); it != bot.rend(); ++it) shape.lineTo (it->x, mid - it->y * amp);
                shape.closeSubPath();
            }
        }
        auto setFill = [&]
        {
            if (skin == "winamp")
            {
                ColourGradient cg (Colour (0xffff2a00), 0, mid - amp, Colour (0xffff2a00), 0, mid + amp, false);
                cg.addColour (0.18, Colour (0xffffd200)); cg.addColour (0.38, Colour (0xff3dff2a)); cg.addColour (0.5, Colour (0xff00c814));
                cg.addColour (0.62, Colour (0xff3dff2a)); cg.addColour (0.82, Colour (0xffffd200));
                gi.setGradientFill (cg);
            }
            else if (wmp)
            {
                ColourGradient cg (Colour (0xffc7a2ff), 0, mid - amp, Colour (0xff3fa4ff), 0, mid, false);
                cg.addColour (0.5, Colour (0xff7b5cff));
                gi.setGradientFill (cg);
            }
            else gi.setColour (sk.wave);
        };
        // anti-shock echoes
        if (vEsp > 0.01f)
        {
            double d = 60.0 / bpm * espDiv * sr / spp;
            for (int e = 1; e <= 3; ++e)
            {
                Graphics::ScopedSaveState ss (gi);
                gi.addTransform (AffineTransform::translation ((float) (d * e * (rev ? -1 : 1)), 0));
                gi.beginTransparencyLayer (vEsp * 0.42f * std::pow (0.62f, (float) (e - 1)));
                setFill(); gi.fillPath (shape);
                gi.endTransparencyLayer();
            }
        }
        // room glow
        if (vRoom > 0.01f)
        {
            Colour gc = skin == "classic" ? Colours::white : (skin == "wmp" ? Colour (0xffa090ff) : sk.wave.withAlpha (1.0f));
            for (int k = 3; k >= 1; --k) { gi.setColour (gc.withAlpha (0.07f * vRoom * 1.5f)); gi.strokePath (shape, PathStrokeType (vRoom * 6.0f * k * sx)); }
        }
        if (sk.ghost) { gi.setColour (Colour (31, 43, 24).withAlpha (0.07f)); gi.fillRect (0.0f, mid - amp * 1.1f, (float) w, amp * 2.2f); }
        setFill(); gi.fillPath (shape);
        if (sk.hasStroke) { gi.setColour (sk.stroke); gi.strokePath (shape, PathStrokeType (jmax (1.0f, 1.4f * sx))); }
        if (wmp)
        {
            Graphics::ScopedSaveState ss (gi);
            gi.addTransform (AffineTransform::scale (1.0f, -0.45f, 0.0f, mid).translated (0, 2 * sx));
            gi.beginTransparencyLayer (0.28f);
            gi.setGradientFill (ColourGradient (Colour (0x003fa4ff), 0, mid - amp, Colour (0xff3fa4ff), 0, mid, false));
            gi.fillPath (shape);
            gi.endTransparencyLayer();
        }
        if (wmp) { gi.setColour (Colour (160, 190, 255).withAlpha (0.35f)); gi.fillRect (0.0f, mid, (float) w, jmax (1.0f, sx)); }

        // live edge marker (where the lookahead ends) — anything right of it has not arrived yet
        {
            float xe = xOf ((double) newest);
            if (xe < w) { gi.setColour (sk.txt.withAlpha (0.18f)); gi.fillRect (xe, 0.0f, (float) w - xe, (float) h); }
        }

        // gate pattern
        if (vGate > 0.01f)
        {
            const double unitQ = 4.0 / gateDiv;
            const float gy0 = wmp ? h * 0.1f : h * 0.2f, gh = wmp ? h * 0.55f : h * 0.64f;
            double qL = ppqAt (pos - cx * spp) / unitQ, qR = ppqAt (pos + (w - cx) * spp) / unitQ;
            for (double k = std::floor (qL); k <= std::ceil (qR); k += 1.0)
            {
                double t0 = k * unitQ;
                float xa = xOf (idxAtPpq (t0)), xb = xOf (idxAtPpq (t0 + unitQ));
                if (gateShape == 0)
                {
                    float xm = xOf (idxAtPpq (t0 + 0.55 * unitQ));
                    gi.setColour (sk.gate.withAlpha (vGate * 0.7f)); gi.fillRect (xm, gy0, xb - xm, gh);
                }
                else
                {
                    ColourGradient cg (sk.gate.withAlpha (0.0f), xa, 0, gateShape == 1 ? sk.gate.withAlpha (vGate * 0.62f) : sk.gate.withAlpha (0.0f), xb, 0, false);
                    if (gateShape == 2) cg.addColour (0.5, sk.gate.withAlpha (vGate * 0.62f));
                    gi.setGradientFill (cg); gi.fillRect (xa, gy0, xb - xa, gh);
                }
            }
        }
        if (fLen > 0)
        {
            gi.setColour (sk.txt);
            gi.drawText ("1/" + String (proc.aFreezeDivShown.load()), Rectangle<float> (xOf (proc.aFreezeStart.load()) + 5 * sx, 4 * sx, 60 * sx, 16 * sx), Justification::centredLeft);
        }
    }

    // ---------- cues ----------
    for (int k = 0; k < 8; ++k)
    {
        auto c = proc.aCues[(size_t) k].load();
        if (c < 0) continue;
        float x = xOf ((double) c);
        if (x < -30 * sx || x > w + 5) continue;
        Colour fc = skin == "lcd" ? Colour (0xff1f2b18) : cueCols[k];
        gi.setColour (fc);
        gi.fillRect (std::round (x), 0.0f, jmax (1.0f, 1.5f * sx), (float) h);
        gi.fillRect (x, h - 22 * sx, 18 * sx, 16 * sx);
        gi.setColour (skin == "lcd" ? Colour (0xffa9bb8c) : Colours::black);
        gi.drawText (String (k + 1), Rectangle<float> (x, h - 22 * sx, 18 * sx, 16 * sx), Justification::centred);
    }

    // ---------- scratch glitches ----------
    for (int i = (int) glitches.size() - 1; i >= 0; --i)
    {
        auto& gl = glitches[(size_t) i];
        gl.life -= 0.04f;
        if (gl.life <= 0) { glitches.erase (glitches.begin() + i); continue; }
        float x = xOf (gl.p), bw = jmax (2.0f, (3 + rng.nextFloat() * 10) * sx);
        for (float y = 0; y < h; y += jmax (2.0f, 3 * sx))
        {
            gi.setColour (rng.nextBool() ? Colours::black.withAlpha (gl.life * 0.8f) : Colours::white.withAlpha (gl.life * 0.7f));
            gi.fillRect (x + (rng.nextFloat() - 0.5f) * 6 * sx, y, bw, jmax (1.0f, 2 * sx));
        }
    }

    // ---------- playhead ----------
    if (skin != "scope")
    {
        gi.setColour (flashSkip > 0.2f ? Colours::white : sk.head);
        if (skin == "blueprint") for (float y = 0; y < h; y += 8 * sx) gi.fillRect (std::round (cx) - sx, y, jmax (1.0f, 2 * sx), 4 * sx);
        else gi.fillRect (std::round (cx) - sx, 0.0f, jmax (1.0f, 2 * sx + flashSkip * 4 * sx), (float) h);
    }

    // ---------- labels ----------
    gi.setColour (sk.txt);
    float lx = 6 * sx;
    StringArray tags;
    if (rev) tags.add (String::fromUTF8 ("\xe2\x97\x80 REV"));
    float pitch = proc.apvts.getRawParameterValue (pid::pitch)->load(), speed = proc.apvts.getRawParameterValue (pid::speed)->load();
    if (std::abs (pitch) > 0.01f) tags.add ((pitch > 0 ? "+" : "") + String (roundToInt (pitch)) + " st");
    if (std::abs (speed - 1.0f) > 0.01f) tags.add (String (roundToInt (speed * 100)) + "%");
    if (proc.aAbOn.load()) tags.add (String::fromUTF8 ("A\xe2\x80\x93" "B"));
    if (proc.aEngaged.load())
    {
        double off = (pos - ((double) newest - look)) / sr;
        if (std::abs (off) > 0.02) tags.add ((off > 0 ? "+" : "") + String (off, 2) + "s");
    }
    for (auto& t : tags)
    {
        float tw = (float) GlyphArrangement::getStringWidth (gi.getCurrentFont(), t);
        gi.drawText (t, Rectangle<float> (lx, 3 * sx, tw + 4, 16 * sx), Justification::centredLeft);
        lx += tw + 10 * sx;
    }
    if (nowS - cutTime < 0.5)
        gi.drawText (cutSide ? String::fromUTF8 ("R \xe2\x9c\x95") : String::fromUTF8 ("L \xe2\x9c\x95"),
                     Rectangle<float> (cutSide ? w - 40 * sx : lx, 3 * sx, 40 * sx, 16 * sx), Justification::centredLeft);

    if (vBatt > 0) { gi.setColour (Colours::black.withAlpha (vBatt * (0.12f + rng.nextFloat() * 0.18f * vBatt))); gi.fillAll(); }
}

// ---------------- mouse ----------------
void Display::mouseDown (const MouseEvent& e)
{
    const float yf = (float) e.y / jmax (1, getHeight());
    if (e.mods.isPopupMenu()) { drag = Drag::freeze; startFreezeFromY (yf); return; }
    if (e.mods.isShiftDown())
    {
        drag = Drag::ab; abO = abA = abB = xToAbs ((float) e.x);
        return;
    }
    drag = Drag::scrub;
    proc.scrubTarget = proc.aPos.load();
    proc.scrubbing = true;
    lastX = e.x;
    setMouseCursor (MouseCursor::DraggingHandCursor);
}

void Display::mouseDrag (const MouseEvent& e)
{
    if (drag == Drag::freeze) { startFreezeFromY ((float) e.y / jmax (1, getHeight())); return; }
    if (drag == Drag::ab)
    {
        double p = abO + (double) (e.x - e.getMouseDownX()) / jmax (1, getWidth()) * zoom * sr;
        abA = jmin (abO, p); abB = jmax (abO, p);
        return;
    }
    if (drag == Drag::scrub)
    {
        int dx = e.x - lastX; lastX = e.x;
        proc.scrubTarget = proc.scrubTarget.load() - (double) dx / jmax (1, getWidth()) * zoom * sr;
    }
}

void Display::mouseUp (const MouseEvent&)
{
    if (drag == Drag::freeze) stopFreeze();
    else if (drag == Drag::ab)
    {
        if (abB - abA > sr * 0.05)
        {
            double a = abA, b = abB;
            if (proc.apvts.getRawParameterValue (pid::quant)->load() > 0.5f)
            {
                double spbS = 60.0 / bpm * sr;
                auto snap = [&] (double idx) { double q = refPpq + (idx - (double) refIdx) / spbS; q = std::round (q / 0.25) * 0.25; return (double) refIdx + (q - refPpq) * spbS; };
                a = snap (a); b = jmax (a + spbS / 4, snap (b));
            }
            proc.sendCommand (CDPlayerAudioProcessor::SetAB, 0, a, b);
            toast (String::fromUTF8 ("Repeat A\xe2\x80\x93" "B"));
        }
    }
    else if (drag == Drag::scrub) proc.scrubbing = false;
    drag = Drag::none;
}

void Display::mouseWheelMove (const MouseEvent&, const MouseWheelDetails& wd)
{
    double z = (double) proc.uiState.getProperty ("zoom", 5.0);
    z *= wd.deltaY < 0 ? 1.12 : 1.0 / 1.12;
    z = jlimit (1.5, 14.0, z);
    proc.uiState.setProperty ("zoom", z, nullptr);
    if (onZoom) onZoom (z);
}
