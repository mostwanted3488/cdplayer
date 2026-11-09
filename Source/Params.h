#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// Parameter IDs and helpers shared by the processor and the editor.
namespace pid
{
    static constexpr const char* damage  = "damage";
    static constexpr const char* skip    = "skip";
    static constexpr const char* scratch = "scratch";
    static constexpr const char* battery = "battery";
    static constexpr const char* tape    = "tape";
    static constexpr const char* crush   = "crush";
    static constexpr const char* bass    = "bass";
    static constexpr const char* hp      = "hp";
    static constexpr const char* width   = "width";
    static constexpr const char* vol     = "vol";
    static constexpr const char* esp     = "esp";
    static constexpr const char* room    = "room";
    static constexpr const char* gate    = "gate";
    static constexpr const char* speed   = "speed";
    static constexpr const char* pitch   = "pitch";
    static constexpr const char* mix     = "mix";
    static constexpr const char* bpm     = "bpm";
    static constexpr const char* rev     = "rev";
    static constexpr const char* quant   = "quant";
    static constexpr const char* slip    = "slip";
    static constexpr const char* espDiv  = "espDiv";
    static constexpr const char* roomType= "roomType";
    static constexpr const char* gateDiv = "gateDiv";
    static constexpr const char* gateShape="gateShape";
    inline juce::String eq (int i) { return "eq" + juce::String (i); }
}

static constexpr int kNumEq = 10;
static const float kEqFreqs[kNumEq] = { 31, 62, 125, 250, 500, 1000, 2000, 4000, 8000, 16000 };
static const float kEspDivs[]  = { 0.75f, 0.5f, 0.25f, 1.0f, 0.3333f };
static const int   kGateDivs[] = { 8, 16, 32, 12 };

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto pct = [] (float v, int) { return String (roundToInt (v * 100.0f)) + "%"; };
    auto addF = [&] (const char* id, const char* name, float mn, float mx, float def, std::function<String (float, int)> fmt = nullptr, float step = 0.001f)
    {
        AudioParameterFloatAttributes at;
        if (fmt) at = at.withStringFromValueFunction (fmt);
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (mn, mx, step), def, at));
    };
    addF (pid::damage,  "Damage",         0, 1, 0, pct);
    addF (pid::skip,    "CD Skip",        0, 1, 0, pct);
    addF (pid::scratch, "Scratched Disc", 0, 1, 0, pct);
    addF (pid::battery, "Low Battery",    0, 1, 0, pct);
    addF (pid::tape,    "Tape Stop",      0, 2, 0.45f, [] (float v, int) { return v < 0.02f ? String ("off") : String (v, 2) + "s"; });
    addF (pid::crush,   "Lo-Fi Laser",    0, 1, 0, pct);
    addF (pid::bass,    "Mega Bass",      0, 1, 0, [] (float v, int) { return v < 0.01f ? String ("off") : "+" + String (roundToInt (v * 15)) + "dB"; });
    addF (pid::hp,      "Headphones",     0, 1, 0, pct);
    addF (pid::width,   "Stereo Width",   0, 2, 1, pct);
    addF (pid::vol,     "Volume",         0, 1, 0.85f, pct);
    addF (pid::esp,     "Anti-Shock",     0, 1, 0, pct);
    addF (pid::room,    "Room",           0, 1, 0, pct);
    addF (pid::gate,    "Gate",           0, 1, 0, pct);
    addF (pid::speed,   "Speed",          0.5f, 1.5f, 1, pct);
    addF (pid::pitch,   "Pitch",          -12, 12, 0, [] (float v, int) { auto n = roundToInt (v); return (n > 0 ? "+" : "") + String (n) + " st"; }, 1.0f);
    addF (pid::mix,     "Dry/Wet",        0, 1, 1, pct);
    addF (pid::bpm,     "Fallback BPM",   60, 180, 120, [] (float v, int) { return String (roundToInt (v)); }, 1.0f);
    for (int i = 0; i < kNumEq; ++i)
    {
        auto f = kEqFreqs[i];
        auto nm = "EQ " + (f >= 1000 ? String (f / 1000.0f) + "k" : String ((int) f));
        AudioParameterFloatAttributes at;
        at = at.withStringFromValueFunction ([] (float v, int) { return (v > 0 ? "+" : "") + String (v, 1) + " dB"; });
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::eq (i), 1 }, nm, NormalisableRange<float> (-12, 12, 0.5f), 0.0f, at));
    }
    l.add (std::make_unique<AudioParameterBool> (ParameterID { pid::rev, 1 },   "Reverse", false));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { pid::quant, 1 }, "Snap to grid", true));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { pid::slip, 1 },  "Slip mode", true));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::espDiv, 1 },   "ESP Echo",   StringArray { "dotted 1/8", "1/8", "1/16", "1/4", "1/8 triplet" }, 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::roomType, 1 }, "Room Space", StringArray { "Bedroom", "Car", "Hall", "Bathroom" }, 1));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::gateDiv, 1 },  "Gate Rate",  StringArray { "1/8", "1/16", "1/32", "1/8 triplet" }, 1));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::gateShape, 1 },"Gate Shape", StringArray { "Square", "Ramp", "Sine" }, 0));
    return l;
}

// The "sound" parameters RESET puts back (not volume, BPM, slip, snap).
inline juce::StringArray resetParamIds()
{
    juce::StringArray a { pid::damage, pid::skip, pid::scratch, pid::battery, pid::crush, pid::bass, pid::hp,
                          pid::esp, pid::room, pid::gate, pid::speed, pid::pitch, pid::width, pid::mix, pid::rev };
    for (int i = 0; i < kNumEq; ++i) a.add (pid::eq (i));
    return a;
}

// Factory presets: param id -> value (unlisted reset params go back to default).
struct FactoryPreset { const char* name; std::vector<std::pair<juce::String, float>> values; };
inline const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> p {
        { "Clean", {} },
        { "Discman in pocket", { { pid::skip, .35f }, { pid::battery, .3f }, { pid::bass, .65f }, { pid::hp, .45f }, { pid::scratch, .08f }, { pid::tape, .6f } } },
        { "Car CD 2003", { { pid::bass, .9f }, { pid::room, .35f }, { pid::crush, .12f }, { pid::skip, .12f }, { pid::roomType, 1 },
                           { "eq0", 5 }, { "eq1", 4 }, { "eq2", 2 }, { "eq4", -1 }, { "eq6", 1 }, { "eq7", 2 }, { "eq8", 1 } } },
        { "Broken laser", { { pid::skip, .8f }, { pid::scratch, .88f }, { pid::crush, .5f }, { pid::gate, .3f }, { pid::gateDiv, 2 } } },
        { "Sunday morning", { { pid::speed, .86f }, { pid::room, .4f }, { pid::crush, .3f }, { pid::hp, .2f }, { pid::roomType, 0 } } },
        { "Rewind time", { { pid::rev, 1 }, { pid::room, .5f }, { pid::esp, .4f }, { pid::roomType, 2 } } },
        { "Chopped & screwed", { { pid::speed, .72f }, { pid::esp, .35f }, { pid::room, .3f }, { pid::bass, .5f }, { pid::roomType, 2 } } },
        { "Nightcore", { { pid::speed, 1.25f }, { pid::bass, .3f } } },
        { "Pitch up", { { pid::pitch, 4 } } },
        { "Trance gate", { { pid::gate, .9f }, { pid::gateDiv, 1 }, { pid::gateShape, 0 }, { pid::esp, .3f }, { pid::room, .25f }, { pid::roomType, 2 } } },
    };
    return p;
}
