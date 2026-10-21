#include "SongFile.h"

#include "Presets.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <locale>
#include <map>
#include <memory>
#include <optional>
#include <sstream>

namespace drumbox
{
namespace
{
constexpr size_t maxSongBytes = 4u * 1024u * 1024u;
constexpr int maxJsonDepth = 12;

// ---- a small JSON reader: just what the song needs ----

struct Json
{
    enum class Type { Null, Bool, Number, String, Array, Object };
    Type type = Type::Null;
    bool boolean = false;
    double number = 0.0;
    std::string text;
    std::vector<Json> items;
    std::map<std::string, Json> members;

    const Json* find (const std::string& key) const
    {
        const auto it = members.find (key);
        return type == Type::Object && it != members.end() ? &it->second : nullptr;
    }
};

class JsonReader
{
public:
    explicit JsonReader (const std::string& source) : s (source) {}

    std::optional<Json> read()
    {
        Json root;
        skip();
        if (! value (root, 0))
            return std::nullopt;
        skip();
        if (pos != s.size())
            return std::nullopt;
        return root;
    }

private:
    void skip()
    {
        while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\n' || s[pos] == '\r' || s[pos] == '\t'))
            ++pos;
    }

    bool literal (const char* word)
    {
        const std::string w (word);
        if (s.compare (pos, w.size(), w) != 0)
            return false;
        pos += w.size();
        return true;
    }

    bool value (Json& out, int depth)
    {
        if (depth > maxJsonDepth || pos >= s.size())
            return false;
        const char c = s[pos];
        if (c == '{')
            return object (out, depth);
        if (c == '[')
            return array (out, depth);
        if (c == '"')
        {
            out.type = Json::Type::String;
            return string (out.text);
        }
        if (c == 't' || c == 'f')
        {
            out.type = Json::Type::Bool;
            out.boolean = c == 't';
            return literal (out.boolean ? "true" : "false");
        }
        if (c == 'n')
            return literal ("null");
        return number (out);
    }

    bool number (Json& out)
    {
        const size_t start = pos;
        if (pos < s.size() && s[pos] == '-')
            ++pos;
        const auto digits = [this] {
            const size_t from = pos;
            while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9')
                ++pos;
            return pos > from;
        };
        if (! digits())
            return false;
        if (pos < s.size() && s[pos] == '.')
        {
            ++pos;
            if (! digits())
                return false;
        }
        if (pos < s.size() && (s[pos] == 'e' || s[pos] == 'E'))
        {
            ++pos;
            if (pos < s.size() && (s[pos] == '+' || s[pos] == '-'))
                ++pos;
            if (! digits())
                return false;
        }
        std::istringstream in (s.substr (start, pos - start));
        in.imbue (std::locale::classic());
        double d = 0.0;
        if (! (in >> d) || ! std::isfinite (d))
            return false;
        out.type = Json::Type::Number;
        out.number = d;
        return true;
    }

    bool string (std::string& out)
    {
        ++pos; // opening quote
        while (pos < s.size())
        {
            const char c = s[pos++];
            if (c == '"')
                return true;
            if (static_cast<unsigned char> (c) < 0x20)
                return false;
            if (c != '\\')
            {
                out += c;
                continue;
            }
            if (pos >= s.size())
                return false;
            const char e = s[pos++];
            switch (e)
            {
                case '"': case '\\': case '/': out += e; break;
                case 'n': out += '\n'; break;
                case 't': out += '\t'; break;
                case 'r': out += '\r'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'u':
                {
                    if (pos + 4 > s.size())
                        return false;
                    unsigned code = 0;
                    for (int i = 0; i < 4; ++i)
                    {
                        const char h = s[pos++];
                        code <<= 4;
                        if (h >= '0' && h <= '9') code |= static_cast<unsigned> (h - '0');
                        else if (h >= 'a' && h <= 'f') code |= static_cast<unsigned> (h - 'a' + 10);
                        else if (h >= 'A' && h <= 'F') code |= static_cast<unsigned> (h - 'A' + 10);
                        else return false;
                    }
                    out += code < 0x80 ? static_cast<char> (code) : '?'; // the song holds no text beyond ASCII
                    break;
                }
                default: return false;
            }
        }
        return false;
    }

    bool array (Json& out, int depth)
    {
        out.type = Json::Type::Array;
        ++pos;
        skip();
        if (pos < s.size() && s[pos] == ']')
        {
            ++pos;
            return true;
        }
        while (true)
        {
            skip();
            Json item;
            if (! value (item, depth + 1))
                return false;
            out.items.push_back (std::move (item));
            skip();
            if (pos >= s.size())
                return false;
            if (s[pos] == ',')
            {
                ++pos;
                continue;
            }
            if (s[pos] == ']')
            {
                ++pos;
                return true;
            }
            return false;
        }
    }

    bool object (Json& out, int depth)
    {
        out.type = Json::Type::Object;
        ++pos;
        skip();
        if (pos < s.size() && s[pos] == '}')
        {
            ++pos;
            return true;
        }
        while (true)
        {
            skip();
            std::string key;
            if (pos >= s.size() || s[pos] != '"' || ! string (key))
                return false;
            skip();
            if (pos >= s.size() || s[pos] != ':')
                return false;
            ++pos;
            skip();
            Json item;
            if (! value (item, depth + 1))
                return false;
            out.members[key] = std::move (item);
            skip();
            if (pos >= s.size())
                return false;
            if (s[pos] == ',')
            {
                ++pos;
                continue;
            }
            if (s[pos] == '}')
            {
                ++pos;
                return true;
            }
            return false;
        }
    }

    const std::string& s;
    size_t pos = 0;
};

// ---- the song as plain data, so a text is fully checked before the engine is touched ----

struct PatternData
{
    std::array<std::uint8_t, numTracks * numSteps> drums {};
    std::array<std::uint8_t, numFxRows * numSteps> fx {};
    std::array<std::uint8_t, numSteps> bassNotes {};
    std::array<bool, numSteps> bassAccents {};
    std::array<std::uint8_t, numSteps * synthVoices> synthNotes {};
    std::array<bool, numSteps> synthAccents {};
    std::array<std::uint8_t, numSteps * pluckVoices> pluckNotes {};
    std::array<bool, numSteps> pluckAccents {};
};

struct SongData
{
    double tempo = 120.0;
    double swing = 0.0;
    int chainLength = 1;
    std::array<std::uint8_t, maxChainLength> chain {};
    std::array<PatternData, numPatterns> patterns;
    std::array<bool, numMixerTracks> mute {};
    std::array<bool, numMixerTracks> solo {};
    std::array<Preset, numPresetKinds> sounds;
    std::map<std::string, float> effects;
};

std::string effectKey (int effect, const char* setting)
{
    return std::string (EffectsRack::effectName (effect)) + "." + setting;
}

// ---- writing ----

class Writer
{
public:
    Writer() { out.imbue (std::locale::classic()); }

    std::string str() const { return out.str(); }

    void raw (const char* text) { out << text; }
    void integer (int v) { out << v; }
    void boolean (bool v) { out << (v ? "true" : "false"); }
    void quoted (const std::string& v)
    {
        out << '"';
        for (const char c : v)
        {
            if (c == '"' || c == '\\')
                out << '\\';
            out << c;
        }
        out << '"';
    }
    void real (double v, int digits)
    {
        out << std::setprecision (digits) << (std::isfinite (v) ? v : 0.0);
    }

    template <class Range, class Item>
    void list (const Range& range, Item&& item)
    {
        out << '[';
        bool first = true;
        for (const auto& v : range)
        {
            if (! first)
                out << ',';
            first = false;
            item (v);
        }
        out << ']';
    }

    void numbers (const std::uint8_t* data, size_t count, size_t perRow)
    {
        out << '[';
        for (size_t i = 0; i < count; ++i)
        {
            if (i > 0)
                out << ',';
            if (perRow > 0 && i > 0 && i % perRow == 0)
                out << '\n';
            out << static_cast<int> (data[i]);
        }
        out << ']';
    }

private:
    std::ostringstream out;
};

template <size_t N>
void writeBools (Writer& w, const std::array<bool, N>& bits)
{
    w.list (bits, [&w] (bool b) { w.boolean (b); });
}

PatternData capturePattern (const Pattern& p)
{
    PatternData d;
    for (int t = 0; t < numTracks; ++t)
        for (int s = 0; s < numSteps; ++s)
            d.drums[static_cast<size_t> (t * numSteps + s)] = static_cast<std::uint8_t> (p.get (t, s));
    for (int r = 0; r < numFxRows; ++r)
        for (int s = 0; s < numSteps; ++s)
            d.fx[static_cast<size_t> (r * numSteps + s)] = static_cast<std::uint8_t> (p.getFx (r, s));
    for (int s = 0; s < numSteps; ++s)
    {
        d.bassNotes[static_cast<size_t> (s)] = static_cast<std::uint8_t> (p.getBassNote (s));
        d.bassAccents[static_cast<size_t> (s)] = p.getBassAccent (s);
        d.synthAccents[static_cast<size_t> (s)] = p.getSynthAccent (s);
        d.pluckAccents[static_cast<size_t> (s)] = p.getPluckAccent (s);
        for (int v = 0; v < synthVoices; ++v)
            d.synthNotes[static_cast<size_t> (s * synthVoices + v)] = static_cast<std::uint8_t> (p.getSynthNote (s, v));
        for (int v = 0; v < pluckVoices; ++v)
            d.pluckNotes[static_cast<size_t> (s * pluckVoices + v)] = static_cast<std::uint8_t> (p.getPluckNote (s, v));
    }
    return d;
}

void writePattern (Writer& w, const PatternData& d)
{
    w.raw ("{\"drums\":");
    w.numbers (d.drums.data(), d.drums.size(), numSteps);
    w.raw (",\"fx\":");
    w.numbers (d.fx.data(), d.fx.size(), numSteps);
    w.raw (",\"bass\":{\"notes\":");
    w.numbers (d.bassNotes.data(), d.bassNotes.size(), 0);
    w.raw (",\"accents\":");
    writeBools (w, d.bassAccents);
    w.raw ("},\"pad\":{\"notes\":");
    w.numbers (d.synthNotes.data(), d.synthNotes.size(), 0);
    w.raw (",\"accents\":");
    writeBools (w, d.synthAccents);
    w.raw ("},\"pluck\":{\"notes\":");
    w.numbers (d.pluckNotes.data(), d.pluckNotes.size(), 0);
    w.raw (",\"accents\":");
    writeBools (w, d.pluckAccents);
    w.raw ("}}");
}

// ---- reading ----

bool asInt (const Json* j, int min, int max, int& out)
{
    if (j == nullptr || j->type != Json::Type::Number || j->number != std::floor (j->number) || j->number < min || j->number > max)
        return false;
    out = static_cast<int> (j->number);
    return true;
}

bool asNumber (const Json* j, double& out)
{
    if (j == nullptr || j->type != Json::Type::Number)
        return false;
    out = j->number;
    return true;
}

bool readBytes (const Json* j, size_t count, std::uint8_t* out, int max)
{
    if (j == nullptr || j->type != Json::Type::Array || j->items.size() != count)
        return false;
    for (size_t i = 0; i < count; ++i)
    {
        int v = 0;
        if (! asInt (&j->items[i], 0, 255, v))
            return false;
        out[i] = static_cast<std::uint8_t> (std::min (v, max));
    }
    return true;
}

template <size_t N>
bool readBools (const Json* j, std::array<bool, N>& out)
{
    if (j == nullptr || j->type != Json::Type::Array || j->items.size() != N)
        return false;
    for (size_t i = 0; i < N; ++i)
    {
        if (j->items[i].type != Json::Type::Bool)
            return false;
        out[i] = j->items[i].boolean;
    }
    return true;
}

bool readPattern (const Json& j, PatternData& d)
{
    const Json* bass = j.find ("bass");
    const Json* pad = j.find ("pad");
    const Json* pluck = j.find ("pluck");
    if (bass == nullptr || pad == nullptr || pluck == nullptr)
        return false;
    // a note is 0 (none) or a pitch; the pattern brings a pitch outside the keyboard back into it
    return readBytes (j.find ("drums"), d.drums.size(), d.drums.data(), 2)
           && readBytes (j.find ("fx"), d.fx.size(), d.fx.data(), 2)
           && readBytes (bass->find ("notes"), d.bassNotes.size(), d.bassNotes.data(), 255)
           && readBools (bass->find ("accents"), d.bassAccents)
           && readBytes (pad->find ("notes"), d.synthNotes.size(), d.synthNotes.data(), 255)
           && readBools (pad->find ("accents"), d.synthAccents)
           && readBytes (pluck->find ("notes"), d.pluckNotes.size(), d.pluckNotes.data(), 255)
           && readBools (pluck->find ("accents"), d.pluckAccents);
}

bool readSounds (const Json* j, std::array<Preset, numPresetKinds>& sounds)
{
    if (j == nullptr || j->type != Json::Type::Object)
        return false;
    for (int k = 0; k < numPresetKinds; ++k)
    {
        const auto kind = static_cast<PresetKind> (k);
        const Json* group = j->find (presetKindName (kind));
        if (group == nullptr || group->type != Json::Type::Object)
            return false;
        Preset preset { kind, "song", {} };
        for (const auto& [key, v] : group->members)
        {
            if (v.type != Json::Type::Number)
                return false;
            preset.values[key] = static_cast<float> (v.number);
        }
        sounds[static_cast<size_t> (k)] = std::move (preset);
    }
    return true;
}

bool readEffects (const Json* j, std::map<std::string, float>& effects)
{
    if (j == nullptr || j->type != Json::Type::Object)
        return false;
    for (int e = 0; e < numEffects; ++e)
    {
        const Json* group = j->find (EffectsRack::effectName (e));
        if (group == nullptr || group->type != Json::Type::Object)
            return false;
        for (const auto& [key, v] : group->members)
        {
            if (v.type != Json::Type::Number)
                return false;
            effects[std::string (EffectsRack::effectName (e)) + "." + key] = static_cast<float> (v.number);
        }
    }
    return true;
}

bool readSong (const Json& root, SongData& d)
{
    if (root.type != Json::Type::Object)
        return false;
    const Json* format = root.find ("format");
    int version = 0;
    if (format == nullptr || format->type != Json::Type::String || format->text != "drumbox-song"
        || ! asInt (root.find ("version"), 1, songFormatVersion, version))
        return false;

    double tempo = 0.0, swing = 0.0;
    if (! asNumber (root.find ("tempo"), tempo) || ! asNumber (root.find ("swing"), swing))
        return false;
    d.tempo = std::clamp (tempo, Sequencer::minTempo, Sequencer::maxTempo);
    d.swing = std::clamp (swing, 0.0, Sequencer::maxSwingPercent);

    if (! asInt (root.find ("chainLength"), 1, maxChainLength, d.chainLength))
        return false;
    const Json* chain = root.find ("chain");
    if (chain == nullptr || chain->type != Json::Type::Array || chain->items.size() != static_cast<size_t> (maxChainLength))
        return false;
    for (size_t i = 0; i < d.chain.size(); ++i)
    {
        int entry = 0;
        if (! asInt (&chain->items[i], 0, numPatterns - 1, entry))
            return false;
        d.chain[i] = static_cast<std::uint8_t> (entry);
    }

    const Json* patterns = root.find ("patterns");
    if (patterns == nullptr || patterns->type != Json::Type::Array || patterns->items.size() != static_cast<size_t> (numPatterns))
        return false;
    for (size_t i = 0; i < d.patterns.size(); ++i)
        if (! readPattern (patterns->items[i], d.patterns[i]))
            return false;

    const Json* mixer = root.find ("mixer");
    if (mixer == nullptr || ! readBools (mixer->find ("mute"), d.mute) || ! readBools (mixer->find ("solo"), d.solo))
        return false;

    return readSounds (root.find ("sounds"), d.sounds) && readEffects (root.find ("effects"), d.effects);
}

void applySong (const SongData& d, DrumEngine& engine)
{
    engine.setTempo (d.tempo);
    engine.setSwing (d.swing);

    Song& song = engine.getSong();
    for (int p = 0; p < numPatterns; ++p)
    {
        const PatternData& src = d.patterns[static_cast<size_t> (p)];
        Pattern& dst = song.pattern (p);
        for (int t = 0; t < numTracks; ++t)
            for (int s = 0; s < numSteps; ++s)
                dst.set (t, s, static_cast<StepState> (src.drums[static_cast<size_t> (t * numSteps + s)]));
        for (int r = 0; r < numFxRows; ++r)
            for (int s = 0; s < numSteps; ++s)
                dst.setFx (r, s, static_cast<StepState> (src.fx[static_cast<size_t> (r * numSteps + s)]));
        for (int s = 0; s < numSteps; ++s)
        {
            const auto si = static_cast<size_t> (s);
            dst.setBass (s, src.bassNotes[si], src.bassAccents[si]);
            dst.setSynthAccent (s, src.synthAccents[si]);
            dst.setPluckAccent (s, src.pluckAccents[si]);
            for (int v = 0; v < synthVoices; ++v)
                dst.setSynthNote (s, v, src.synthNotes[static_cast<size_t> (s * synthVoices + v)]);
            for (int v = 0; v < pluckVoices; ++v)
                dst.setPluckNote (s, v, src.pluckNotes[static_cast<size_t> (s * pluckVoices + v)]);
        }
    }
    for (int i = 0; i < maxChainLength; ++i)
        song.setChainEntry (i, d.chain[static_cast<size_t> (i)]);
    song.setChainLength (d.chainLength);

    PresetBank bank (engine);
    for (const Preset& sound : d.sounds)
        bank.apply (sound);
    for (int t = 0; t < numMixerTracks; ++t)
    {
        engine.getMixer().setMute (t, d.mute[static_cast<size_t> (t)]);
        engine.getMixer().setSolo (t, d.solo[static_cast<size_t> (t)]);
    }

    // a setting the file does not hold goes back to its default
    EffectsRack defaults;
    for (int e = 0; e < numEffects; ++e)
    {
        auto live = engine.getEffects().settings (e);
        const auto reference = defaults.settings (e);
        for (size_t i = 0; i < live.size(); ++i)
        {
            const auto it = d.effects.find (effectKey (e, live[i].name));
            const float value = it != d.effects.end() ? it->second : reference[i].value->load();
            live[i].value->store (std::clamp (value, live[i].min, live[i].max));
        }
    }
}

bool hasSongExtension (const std::string& path)
{
    return std::filesystem::path (path).extension() == songFileExtension;
}
} // namespace

std::string serializeSong (DrumEngine& engine)
{
    Writer w;
    w.raw ("{\n\"format\":\"drumbox-song\",\"version\":");
    w.integer (songFormatVersion);
    w.raw (",\n\"tempo\":");
    w.real (engine.getTempo(), 17);
    w.raw (",\"swing\":");
    w.real (engine.getSwing(), 17);

    Song& song = engine.getSong();
    w.raw (",\n\"chainLength\":");
    w.integer (song.getChainLength());
    w.raw (",\n\"chain\":[");
    for (int i = 0; i < maxChainLength; ++i)
    {
        if (i > 0)
            w.raw (",");
        w.integer (song.getChainEntry (i));
    }
    w.raw ("],\n\"patterns\":[\n");
    for (int p = 0; p < numPatterns; ++p)
    {
        if (p > 0)
            w.raw (",\n");
        writePattern (w, capturePattern (song.pattern (p)));
    }

    Mixer& mixer = engine.getMixer();
    std::array<bool, numMixerTracks> mute {}, solo {};
    for (int t = 0; t < numMixerTracks; ++t)
    {
        mute[static_cast<size_t> (t)] = mixer.isMuted (t);
        solo[static_cast<size_t> (t)] = mixer.isSolo (t);
    }
    w.raw ("\n],\n\"mixer\":{\"mute\":");
    writeBools (w, mute);
    w.raw (",\"solo\":");
    writeBools (w, solo);

    w.raw ("},\n\"sounds\":{");
    PresetBank bank (engine);
    for (int k = 0; k < numPresetKinds; ++k)
    {
        const auto kind = static_cast<PresetKind> (k);
        w.raw (k > 0 ? ",\n" : "\n");
        w.quoted (presetKindName (kind));
        w.raw (":{");
        bool first = true;
        for (const auto& [key, value] : bank.capture (kind, "song").values)
        {
            w.raw (first ? "" : ",");
            first = false;
            w.quoted (key);
            w.raw (":");
            w.real (value, 9);
        }
        w.raw ("}");
    }

    w.raw ("},\n\"effects\":{");
    for (int e = 0; e < numEffects; ++e)
    {
        w.raw (e > 0 ? ",\n" : "\n");
        w.quoted (EffectsRack::effectName (e));
        w.raw (":{");
        bool first = true;
        for (const auto& setting : engine.getEffects().settings (e))
        {
            w.raw (first ? "" : ",");
            first = false;
            w.quoted (setting.name);
            w.raw (":");
            w.real (setting.value->load(), 9);
        }
        w.raw ("}");
    }
    w.raw ("}\n}\n");
    return w.str();
}

bool parseSong (const std::string& text, DrumEngine& engine)
{
    if (text.size() > maxSongBytes)
        return false;
    const auto root = JsonReader (text).read();
    if (! root)
        return false;
    auto data = std::make_unique<SongData>();
    if (! readSong (*root, *data))
        return false;
    applySong (*data, engine);
    return true;
}

bool saveSongFile (DrumEngine& engine, const std::string& path)
{
    if (! hasSongExtension (path))
        return false;
    const std::string text = serializeSong (engine);
    const std::string partial = path + ".part";
    {
        std::ofstream out (partial, std::ios::binary | std::ios::trunc);
        out << text;
        out.flush();
        if (! out)
        {
            out.close();
            std::error_code ignored;
            std::filesystem::remove (partial, ignored);
            return false;
        }
    }
    std::error_code ec;
    std::filesystem::rename (partial, path, ec);
    if (ec)
    {
        std::filesystem::remove (partial, ec);
        return false;
    }
    return true;
}

bool loadSongFile (DrumEngine& engine, const std::string& path)
{
    if (! hasSongExtension (path))
        return false;
    std::ifstream in (path, std::ios::binary);
    if (! in)
        return false;
    std::ostringstream content;
    content << in.rdbuf();
    return parseSong (content.str(), engine);
}
} // namespace drumbox
