#include "Presets.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <locale>
#include <sstream>

namespace drumbox
{
namespace
{
using Values = std::map<std::string, float>;

std::string trackKey (int track, const char* what) { return "track" + std::to_string (track) + "." + what; }

std::string fileFor (const std::string& folder, PresetKind kind, const std::string& name)
{
    return (std::filesystem::path (folder) / presetKindName (kind) / (name + ".preset")).string();
}

const std::vector<Preset>& makeFactory (PresetKind kind)
{
    static const auto build = [] {
        std::array<std::vector<Preset>, numPresetKinds> all;
        const auto add = [&all] (PresetKind k, const char* name, Values values) {
            all[static_cast<size_t> (k)].push_back ({ k, name, std::move (values) });
        };
        // each kit entry: tracks 0 kick, 1 snare, 2 closed hat, 3 open hat, 4 clap, 5 tom, 6 rim, 7 cowbell
        add (PresetKind::DrumKit, "Default", {});
        add (PresetKind::DrumKit, "Tight Funk",
             { { trackKey (0, "tune"), -2.0f }, { trackKey (1, "tune"), 1.0f }, { trackKey (2, "volume"), 0.8f },
               { trackKey (3, "volume"), 0.7f }, { trackKey (5, "tune"), 3.0f }, { trackKey (1, "send"), 0.2f } });
        add (PresetKind::DrumKit, "Deep Soul",
             { { trackKey (0, "tune"), -5.0f }, { trackKey (1, "tune"), -3.0f }, { trackKey (5, "tune"), -7.0f },
               { trackKey (2, "volume"), 0.7f }, { trackKey (4, "send"), 0.3f }, { trackKey (1, "send"), 0.3f } });
        add (PresetKind::DrumKit, "Disco Floor",
             { { trackKey (0, "volume"), 1.2f }, { trackKey (2, "volume"), 1.2f }, { trackKey (3, "volume"), 1.1f },
               { trackKey (4, "volume"), 1.2f }, { trackKey (7, "tune"), 2.0f }, { trackKey (4, "send"), 0.25f } });
        add (PresetKind::DrumKit, "Dusty Break",
             { { trackKey (0, "detune"), -25.0f }, { trackKey (1, "detune"), 30.0f }, { trackKey (2, "detune"), -15.0f },
               { trackKey (5, "tune"), -2.0f }, { trackKey (2, "volume"), 0.6f }, { trackKey (6, "volume"), 0.8f } });

        add (PresetKind::Bass, "Default", {});
        add (PresetKind::Bass, "Fat Funk", { { "bass.cutoff", 520.0f }, { "bass.decay", 0.35f } });
        add (PresetKind::Bass, "Slap", { { "bass.cutoff", 1500.0f }, { "bass.decay", 0.12f }, { trackKey (8, "volume"), 1.1f } });
        add (PresetKind::Bass, "Sub", { { "bass.cutoff", 250.0f }, { "bass.decay", 0.45f }, { trackKey (8, "tune"), -12.0f } });
        add (PresetKind::Bass, "Wah Wah", { { "bass.cutoff", 1000.0f }, { "bass.decay", 0.2f }, { trackKey (8, "send"), 0.2f } });

        add (PresetKind::Pad, "Default", {});
        add (PresetKind::Pad, "Strings",
             { { "synth.attack", 0.35f }, { "synth.release", 0.8f }, { "synth.sustain", 0.85f }, { "synth.cutoff", 3200.0f },
               { "synth.osc2Detune", 14.0f }, { trackKey (9, "send"), 0.3f } });
        add (PresetKind::Pad, "Brass Stab",
             { { "synth.attack", 0.01f }, { "synth.decay", 0.2f }, { "synth.sustain", 0.5f }, { "synth.release", 0.12f },
               { "synth.cutoff", 3500.0f }, { "synth.resonance", 0.35f } });
        add (PresetKind::Pad, "Warm Organ",
             { { "synth.osc2Semitones", 12.0f }, { "synth.osc2Mix", 0.4f }, { "synth.cutoff", 1400.0f }, { "synth.sustain", 0.9f },
               { "synth.attack", 0.02f } });
        add (PresetKind::Pad, "Wobble",
             { { "synth.lfoRate", 3.0f }, { "synth.lfoDepth", 0.6f }, { "synth.cutoff", 1800.0f }, { "synth.resonance", 0.45f } });

        add (PresetKind::Pluck, "Default", {});
        add (PresetKind::Pluck, "Clav", { { "pluck.cutoff", 2400.0f }, { "pluck.decay", 0.1f }, { "pluck.osc2Mix", 0.8f } });
        add (PresetKind::Pluck, "Mellow", { { "pluck.cutoff", 700.0f }, { "pluck.decay", 0.3f }, { "pluck.osc2Mix", 0.1f } });
        add (PresetKind::Pluck, "Glass", { { "pluck.cutoff", 3500.0f }, { "pluck.decay", 0.25f }, { "pluck.resonance", 0.6f },
                                           { trackKey (10, "tune"), 12.0f }, { trackKey (10, "send"), 0.3f } });

        add (PresetKind::Fx, "Default", {});
        add (PresetKind::Fx, "Far Away", { { trackKey (11, "volume"), 0.7f }, { trackKey (11, "send"), 0.6f } });
        add (PresetKind::Fx, "Bright", { { trackKey (11, "tune"), 7.0f }, { trackKey (11, "volume"), 0.9f } });
        add (PresetKind::Fx, "Low Rumble", { { trackKey (11, "tune"), -12.0f }, { trackKey (11, "volume"), 1.2f } });
        return all;
    }();
    return build[static_cast<size_t> (kind)];
}
} // namespace

const char* presetKindName (PresetKind kind) noexcept
{
    static constexpr const char* names[numPresetKinds] = { "drums", "bass", "pad", "pluck", "fx" };
    return names[static_cast<size_t> (kind)];
}

bool isValidPresetName (const std::string& name)
{
    if (name.empty() || name.size() > 40 || name.front() == '.' || name.front() == ' ' || name.back() == ' ')
        return false;
    return std::all_of (name.begin(), name.end(), [] (char c) {
        return std::isalnum (static_cast<unsigned char> (c)) != 0 || c == ' ' || c == '-' || c == '_' || c == '.';
    });
}

std::string serialize (const Preset& preset)
{
    std::ostringstream out;
    out.imbue (std::locale::classic());
    out.precision (9);
    out << "drumbox-preset 1\nkind=" << presetKindName (preset.kind) << "\nname=" << preset.name << "\n";
    for (const auto& [key, value] : preset.values)
        out << key << "=" << value << "\n";
    return out.str();
}

bool parse (const std::string& text, PresetKind kind, Preset& preset)
{
    std::istringstream in (text);
    in.imbue (std::locale::classic());
    std::string line;
    if (! std::getline (in, line) || line.rfind ("drumbox-preset 1", 0) != 0)
        return false;
    Preset result;
    result.kind = kind;
    bool kindSeen = false;
    while (std::getline (in, line))
    {
        if (! line.empty() && line.back() == '\r')
            line.pop_back();
        const auto eq = line.find ('=');
        if (eq == std::string::npos)
            continue;
        const std::string key = line.substr (0, eq), value = line.substr (eq + 1);
        if (key == "kind")
        {
            if (value != presetKindName (kind))
                return false;
            kindSeen = true;
        }
        else if (key == "name")
            result.name = value;
        else
        {
            std::istringstream number (value);
            number.imbue (std::locale::classic());
            float f = 0.0f;
            if (number >> f)
                result.values[key] = f;
        }
    }
    if (! kindSeen)
        return false;
    preset = std::move (result);
    return true;
}

std::vector<std::string> FolderPresetStore::list (PresetKind kind)
{
    std::vector<std::string> names;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator (std::filesystem::path (folder) / presetKindName (kind), ec))
    {
        const auto& path = entry.path();
        if (entry.is_regular_file (ec) && path.extension() == ".preset" && isValidPresetName (path.stem().string()))
            names.push_back (path.stem().string());
    }
    std::sort (names.begin(), names.end());
    return names;
}

bool FolderPresetStore::read (PresetKind kind, const std::string& name, std::string& text)
{
    if (! isValidPresetName (name))
        return false;
    std::ifstream in (fileFor (folder, kind, name), std::ios::binary);
    if (! in)
        return false;
    std::ostringstream content;
    content << in.rdbuf();
    text = content.str();
    return true;
}

bool FolderPresetStore::write (PresetKind kind, const std::string& name, const std::string& text)
{
    if (! isValidPresetName (name))
        return false;
    std::error_code ec;
    std::filesystem::create_directories (std::filesystem::path (folder) / presetKindName (kind), ec);
    if (ec)
        return false;
    std::ofstream out (fileFor (folder, kind, name), std::ios::binary | std::ios::trunc);
    out << text;
    return static_cast<bool> (out);
}

const std::vector<Preset>& PresetBank::factory (PresetKind kind) { return makeFactory (kind); }

std::vector<PresetBank::Slot> PresetBank::slots (PresetKind kind)
{
    std::vector<Slot> result;
    const auto addTrack = [&] (int track) {
        Mixer& m = engine.getMixer();
        result.push_back ({ trackKey (track, "volume"), 0.0f, maxVolume, 1.0f, [&m, track] { return m.getVolume (track); },
                            [&m, track] (float v) { m.setVolume (track, v); } });
        result.push_back ({ trackKey (track, "tune"), -maxTuneSemitones, maxTuneSemitones, 0.0f, [&m, track] { return m.getTune (track); },
                            [&m, track] (float v) { m.setTune (track, v); } });
        result.push_back ({ trackKey (track, "detune"), -maxDetuneCents, maxDetuneCents, 0.0f, [&m, track] { return m.getDetune (track); },
                            [&m, track] (float v) { m.setDetune (track, v); } });
        result.push_back ({ trackKey (track, "send"), 0.0f, maxSend, 0.0f, [&m, track] { return m.getSend (track); },
                            [&m, track] (float v) { m.setSend (track, v); } });
    };
    // initial values come from a freshly built set of parameters
    const auto addParam = [&] (const char* key, std::atomic<float>& value, const std::atomic<float>& initial, float lo, float hi) {
        std::atomic<float>* p = &value;
        result.push_back ({ key, lo, hi, initial.load(), [p] { return p->load(); }, [p] (float v) { p->store (v); } });
    };

    switch (kind)
    {
        case PresetKind::DrumKit:
            for (int t = 0; t < mixerDrums; ++t)
                addTrack (t);
            break;
        case PresetKind::Bass:
        {
            BassParams d;
            auto& p = engine.getBassParams();
            addParam ("bass.cutoff", p.cutoff, d.cutoff, 100.0f, 3000.0f);
            addParam ("bass.decay", p.decay, d.decay, 0.05f, 1.0f);
            addTrack (mixerBass);
            break;
        }
        case PresetKind::Pad:
        {
            SynthParams d;
            auto& p = engine.getSynthParams();
            addParam ("synth.osc2Semitones", p.osc2Semitones, d.osc2Semitones, -12.0f, 12.0f);
            addParam ("synth.osc2Detune", p.osc2Detune, d.osc2Detune, 0.0f, 50.0f);
            addParam ("synth.osc2Mix", p.osc2Mix, d.osc2Mix, 0.0f, 1.0f);
            addParam ("synth.cutoff", p.cutoff, d.cutoff, 100.0f, 8000.0f);
            addParam ("synth.resonance", p.resonance, d.resonance, 0.0f, 1.0f);
            addParam ("synth.attack", p.attack, d.attack, 0.001f, 2.0f);
            addParam ("synth.decay", p.decay, d.decay, 0.01f, 2.0f);
            addParam ("synth.sustain", p.sustain, d.sustain, 0.0f, 1.0f);
            addParam ("synth.release", p.release, d.release, 0.01f, 3.0f);
            addParam ("synth.lfoRate", p.lfoRate, d.lfoRate, 0.1f, 20.0f);
            addParam ("synth.lfoDepth", p.lfoDepth, d.lfoDepth, 0.0f, 1.0f);
            addTrack (mixerPad);
            break;
        }
        case PresetKind::Pluck:
        {
            PluckParams d;
            auto& p = engine.getPluckParams();
            addParam ("pluck.cutoff", p.cutoff, d.cutoff, 100.0f, 8000.0f);
            addParam ("pluck.decay", p.decay, d.decay, 0.03f, 1.0f);
            addParam ("pluck.resonance", p.resonance, d.resonance, 0.0f, 1.0f);
            addParam ("pluck.osc2Mix", p.osc2Mix, d.osc2Mix, 0.0f, 1.0f);
            addTrack (mixerPluck);
            break;
        }
        case PresetKind::Fx:
            addTrack (mixerFx);
            break;
    }
    return result;
}

Preset PresetBank::capture (PresetKind kind, const std::string& name)
{
    Preset preset { kind, name, {} };
    for (const auto& s : slots (kind))
        preset.values[s.key] = s.get();
    return preset;
}

void PresetBank::apply (const Preset& preset)
{
    for (const auto& s : slots (preset.kind))
    {
        const auto it = preset.values.find (s.key);
        const float value = it != preset.values.end() ? it->second : s.initial;
        s.set (std::clamp (value, s.min, s.max));
    }
}

bool PresetBank::saveUser (PresetStore& store, PresetKind kind, const std::string& name)
{
    return isValidPresetName (name) && store.write (kind, name, serialize (capture (kind, name)));
}

bool PresetBank::loadUser (PresetStore& store, PresetKind kind, const std::string& name)
{
    std::string text;
    Preset preset;
    if (! store.read (kind, name, text) || ! parse (text, kind, preset))
        return false;
    preset.name = name;
    apply (preset);
    return true;
}
} // namespace drumbox
