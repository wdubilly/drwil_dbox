#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>

namespace drumbox
{
// Mixer tracks: the eight drums (same order as `Track`), then the bass, pad, pluck and FX.
constexpr int mixerDrums = 8;
constexpr int mixerBass = 8;
constexpr int mixerPad = 9;
constexpr int mixerPluck = 10;
constexpr int mixerFx = 11;
constexpr int numMixerTracks = 12;

constexpr std::array<const char*, numMixerTracks> mixerTrackNames { "KICK", "SNARE", "C.HAT", "O.HAT", "CLAP", "TOM",
                                                                    "RIM", "COWBELL", "BASS", "PAD", "PLUCK", "FX" };

constexpr float maxVolume = 1.5f;
constexpr float maxTuneSemitones = 24.0f;
constexpr float maxDetuneCents = 100.0f;

// Song-wide mixer settings, not part of any pattern. The UI writes, the audio thread reads.
class Mixer
{
public:
    void setMute (int track, bool on) noexcept { if (valid (track)) slots[idx (track)].mute.store (on); }
    void setSolo (int track, bool on) noexcept { if (valid (track)) slots[idx (track)].solo.store (on); }
    void setVolume (int track, float volume) noexcept { if (valid (track)) slots[idx (track)].volume.store (std::clamp (volume, 0.0f, maxVolume)); }
    void setTune (int track, float semitones) noexcept { if (valid (track)) slots[idx (track)].tune.store (std::clamp (semitones, -maxTuneSemitones, maxTuneSemitones)); }
    void setDetune (int track, float cents) noexcept { if (valid (track)) slots[idx (track)].detune.store (std::clamp (cents, -maxDetuneCents, maxDetuneCents)); }

    bool isMuted (int track) const noexcept { return valid (track) && slots[idx (track)].mute.load(); }
    bool isSolo (int track) const noexcept { return valid (track) && slots[idx (track)].solo.load(); }
    float getVolume (int track) const noexcept { return valid (track) ? slots[idx (track)].volume.load() : 1.0f; }
    float getTune (int track) const noexcept { return valid (track) ? slots[idx (track)].tune.load() : 0.0f; }
    float getDetune (int track) const noexcept { return valid (track) ? slots[idx (track)].detune.load() : 0.0f; }

    bool anySolo() const noexcept
    {
        return std::any_of (slots.begin(), slots.end(), [] (const Slot& s) { return s.solo.load(); });
    }

    // A muted track is silent even when soloed; with no solo, every unmuted track is heard.
    float targetGain (int track, bool soloActive) const noexcept
    {
        if (! valid (track) || isMuted (track) || (soloActive && ! isSolo (track)))
            return 0.0f;
        return getVolume (track);
    }

    // Frequency ratio of the tune plus the detune.
    double pitchRatio (int track) const noexcept
    {
        return std::pow (2.0, (static_cast<double> (getTune (track)) + static_cast<double> (getDetune (track)) / 100.0) / 12.0);
    }

private:
    struct Slot
    {
        std::atomic<bool> mute { false };
        std::atomic<bool> solo { false };
        std::atomic<float> volume { 1.0f };
        std::atomic<float> tune { 0.0f };
        std::atomic<float> detune { 0.0f };
    };

    static bool valid (int track) noexcept { return track >= 0 && track < numMixerTracks; }
    static size_t idx (int track) noexcept { return static_cast<size_t> (track); }

    std::array<Slot, numMixerTracks> slots;
};
} // namespace drumbox
