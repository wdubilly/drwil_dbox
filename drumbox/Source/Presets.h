#pragma once

#include "DrumEngine.h"

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace drumbox
{
// What a preset sounds like: the drum kit (the eight drums), or one instrument.
enum class PresetKind : int { DrumKit = 0, Bass, Pad, Pluck, Fx };
constexpr int numPresetKinds = 5;

const char* presetKindName (PresetKind kind) noexcept;

// Settings by key ("bass.cutoff", "track8.volume"…). A key a preset does not hold goes back to its default.
struct Preset
{
    PresetKind kind = PresetKind::DrumKit;
    std::string name;
    std::map<std::string, float> values;
};

// Where user presets live. The app uses a folder; the tests replace it with a memory store.
class PresetStore
{
public:
    virtual ~PresetStore() = default;
    virtual std::vector<std::string> list (PresetKind kind) = 0;
    virtual bool read (PresetKind kind, const std::string& name, std::string& text) = 0;
    virtual bool write (PresetKind kind, const std::string& name, const std::string& text) = 0;
};

// One file per preset: <folder>/<kind>/<name>.preset
class FolderPresetStore : public PresetStore
{
public:
    explicit FolderPresetStore (std::string folderPath) : folder (std::move (folderPath)) {}

    std::vector<std::string> list (PresetKind kind) override;
    bool read (PresetKind kind, const std::string& name, std::string& text) override;
    bool write (PresetKind kind, const std::string& name, const std::string& text) override;

private:
    std::string folder;
};

// 1 to 40 letters, digits, spaces, '-', '_' or '.', not starting with a dot or a space: a name is also a file name.
bool isValidPresetName (const std::string& name);

std::string serialize (const Preset& preset);
// False when the text is not a preset of the given kind; unknown keys are ignored.
bool parse (const std::string& text, PresetKind kind, Preset& preset);

// Reads and applies the settings of the engine's tracks for each kind of preset.
class PresetBank
{
public:
    explicit PresetBank (DrumEngine& engineToUse) : engine (engineToUse) {}

    static const std::vector<Preset>& factory (PresetKind kind);

    // The current sound of the kind, under the given name.
    Preset capture (PresetKind kind, const std::string& name);
    void apply (const Preset& preset);

    bool saveUser (PresetStore& store, PresetKind kind, const std::string& name);
    bool loadUser (PresetStore& store, PresetKind kind, const std::string& name);

private:
    struct Slot
    {
        std::string key;
        float min, max, initial;
        std::function<float()> get;
        std::function<void (float)> set;
    };

    std::vector<Slot> slots (PresetKind kind);

    DrumEngine& engine;
};
} // namespace drumbox
