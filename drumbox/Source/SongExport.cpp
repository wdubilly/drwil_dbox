#include "SongExport.h"

#include "Presets.h"
#include "SongFile.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <functional>
#include <memory>

namespace drumbox
{
namespace
{
constexpr int blockSize = 512;
constexpr float silence = 1.0e-6f; // below the 24-bit step
constexpr size_t silentFramesToStop = exportSampleRate; // one second of silence ends the tail

std::unique_ptr<DrumEngine> makeCopy (DrumEngine& source, int track)
{
    auto copy = std::make_unique<DrumEngine>();
    copy->prepare (exportSampleRate);
    parseSong (serializeSong (source), *copy);

    if (track >= 0)
    {
        Mixer& mixer = copy->getMixer();
        const bool soloActive = mixer.anySolo();
        const bool heard = ! mixer.isMuted (track) && (! soloActive || mixer.isSolo (track));
        for (int t = 0; t < numMixerTracks; ++t)
        {
            mixer.setSolo (t, false);
            mixer.setMute (t, t != track || ! heard);
        }
    }
    copy->settleMixer();
    copy->setPlaying (true);
    return copy;
}

size_t chainFramesOf (DrumEngine& engine)
{
    const double tempo = std::clamp (engine.getTempo(), Sequencer::minTempo, Sequencer::maxTempo);
    const double perStep = exportSampleRate * 60.0 / tempo / 4.0;
    return static_cast<size_t> (std::llround (engine.getSong().getChainLength() * numSteps * perStep));
}

using Sink = std::function<void (const float*, int)>;

// Plays the chain, then lets the tails ring. With `untilSilent` it stops after a second of silence
// (or the cap) and returns the length that holds every audible sample; otherwise it renders `limit` frames.
size_t run (DrumEngine& engine, size_t chainFrames, size_t limit, bool untilSilent, const Sink& sink)
{
    std::array<float, blockSize> buffer;
    size_t done = 0, lastLoud = 0;
    while (done < limit)
    {
        if (done >= chainFrames)
            engine.setPlaying (false);
        const size_t room = done < chainFrames ? chainFrames - done : limit - done;
        const int n = static_cast<int> (std::min<size_t> ({ static_cast<size_t> (blockSize), room, limit - done }));
        engine.render (buffer.data(), n);
        sink (buffer.data(), n);
        for (int i = 0; i < n; ++i)
            if (std::fabs (buffer[static_cast<size_t> (i)]) > silence)
                lastLoud = done + static_cast<size_t> (i) + 1;
        done += static_cast<size_t> (n);
        if (untilSilent && done >= chainFrames && done - lastLoud >= silentFramesToStop)
            break;
    }
    return untilSilent ? std::max (chainFrames, lastLoud) : done;
}

void renderTo (DrumEngine& source, int track, const ExportLength& length, const Sink& sink)
{
    auto copy = makeCopy (source, track);
    run (*copy, length.chainFrames, length.totalFrames, false, sink);
}

std::string stemFileName (const std::string& baseName, int track)
{
    const std::string number = track < 9 ? "0" + std::to_string (track + 1) : std::to_string (track + 1);
    return baseName + "_" + number + "_" + mixerTrackNames[static_cast<size_t> (track)] + ".wav";
}

void putLe (std::ofstream& out, std::uint32_t value, int bytes)
{
    for (int i = 0; i < bytes; ++i)
        out.put (static_cast<char> ((value >> (8 * i)) & 0xff));
}
} // namespace

ExportLength measureExport (DrumEngine& engine)
{
    ExportLength length;
    length.chainFrames = chainFramesOf (engine);
    auto copy = makeCopy (engine, -1);
    length.totalFrames = run (*copy, length.chainFrames, length.chainFrames + static_cast<size_t> (maxTailSeconds) * exportSampleRate, true,
                              [] (const float*, int) {});
    return length;
}

std::vector<float> renderSongMix (DrumEngine& engine)
{
    std::vector<float> mix;
    const auto length = measureExport (engine);
    mix.reserve (length.totalFrames);
    renderTo (engine, -1, length, [&mix] (const float* block, int n) { mix.insert (mix.end(), block, block + n); });
    return mix;
}

ExportedAudio renderSongAudio (DrumEngine& engine)
{
    ExportedAudio audio;
    audio.length = measureExport (engine);
    const auto collect = [&audio] (std::vector<float>& into) {
        into.reserve (audio.length.totalFrames);
        return [&into] (const float* block, int n) { into.insert (into.end(), block, block + n); };
    };
    renderTo (engine, -1, audio.length, collect (audio.mix));
    for (int t = 0; t < numMixerTracks; ++t)
        renderTo (engine, t, audio.length, collect (audio.stems[static_cast<size_t> (t)]));
    return audio;
}

bool WavFileWriter::open (const std::string& path, size_t frames)
{
    target = path;
    expected = frames;
    written = 0;
    const std::uint64_t dataBytes = static_cast<std::uint64_t> (frames) * 6;
    if (dataBytes > 0xffffffffu - 36)
        return false;
    file.open (path + ".part", std::ios::binary | std::ios::trunc);
    if (! file)
        return false;
    file.write ("RIFF", 4);
    putLe (file, static_cast<std::uint32_t> (36 + dataBytes), 4);
    file.write ("WAVEfmt ", 8);
    putLe (file, 16, 4);
    putLe (file, 1, 2); // PCM
    putLe (file, 2, 2); // stereo
    putLe (file, exportSampleRate, 4);
    putLe (file, exportSampleRate * 6, 4);
    putLe (file, 6, 2);
    putLe (file, exportBitDepth, 2);
    file.write ("data", 4);
    putLe (file, static_cast<std::uint32_t> (dataBytes), 4);
    return static_cast<bool> (file);
}

void WavFileWriter::write (const float* mono, int numSamples)
{
    if (! file.is_open())
        return;
    for (int i = 0; i < numSamples && written < expected; ++i, ++written)
    {
        const float clipped = std::clamp (mono[i], -1.0f, 1.0f);
        const auto value = static_cast<std::uint32_t> (static_cast<std::int32_t> (std::lrint (static_cast<double> (clipped) * 8388607.0)));
        for (int channel = 0; channel < 2; ++channel)
            putLe (file, value, 3);
    }
}

bool WavFileWriter::finish()
{
    if (! file.is_open())
        return false;
    file.flush();
    const bool complete = static_cast<bool> (file) && written == expected;
    file.close();
    if (! complete)
    {
        discard();
        return false;
    }
    std::error_code ec;
    std::filesystem::rename (target + ".part", target, ec);
    if (ec)
    {
        discard();
        return false;
    }
    return true;
}

void WavFileWriter::discard()
{
    std::error_code ec;
    std::filesystem::remove (target + ".part", ec);
}

WavFileWriter::~WavFileWriter()
{
    if (file.is_open())
    {
        file.close();
        discard();
    }
}

namespace
{
bool writeWav (DrumEngine& engine, int track, const ExportLength& length, const std::string& path)
{
    WavFileWriter writer;
    if (! writer.open (path, length.totalFrames))
        return false;
    renderTo (engine, track, length, [&writer] (const float* block, int n) { writer.write (block, n); });
    return writer.finish();
}
} // namespace

bool exportMixWav (DrumEngine& engine, const std::string& path)
{
    return writeWav (engine, -1, measureExport (engine), path);
}

bool exportStemsWav (DrumEngine& engine, const std::string& folder, const std::string& baseName)
{
    if (! isValidPresetName (baseName))
        return false;
    std::error_code ec;
    std::filesystem::create_directories (folder, ec);
    if (ec)
        return false;
    const auto length = measureExport (engine);
    const std::filesystem::path dir (folder);
    if (! writeWav (engine, -1, length, (dir / (baseName + "_mix.wav")).string()))
        return false;
    for (int t = 0; t < numMixerTracks; ++t)
        if (! writeWav (engine, t, length, (dir / stemFileName (baseName, t)).string()))
            return false;
    return true;
}
} // namespace drumbox
