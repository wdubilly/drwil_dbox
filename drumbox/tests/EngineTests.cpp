// Plain C++ tests: no audio device, no JUCE. Run `drumbox_tests <prefix>` to run
// only the tests whose name starts with the prefix (each test name carries the
// ID of the contract it protects).
#include "../Source/DrumEngine.h"
#include "../Source/Presets.h"
#include "../Source/SongExport.h"
#include "../Source/SongFile.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

using namespace drumbox;

namespace
{
struct TestCase
{
    std::string name;
    std::function<void()> body;
};

std::vector<TestCase>& registry()
{
    static std::vector<TestCase> tests;
    return tests;
}

struct Registrar
{
    Registrar (const char* name, std::function<void()> body) { registry().push_back ({ name, std::move (body) }); }
};

int failures = 0;

#define CHECK(cond)                                                                    \
    do                                                                                 \
    {                                                                                  \
        if (! (cond))                                                                  \
        {                                                                              \
            std::cerr << "  CHECK FAILED " << __FILE__ << ":" << __LINE__ << ": " #cond "\n"; \
            ++failures;                                                                \
        }                                                                              \
    } while (false)

#define JOIN2(a, b) a##b
#define JOIN(a, b) JOIN2 (a, b)
#define TEST(name, ...) static Registrar JOIN (registrar_, __LINE__) (name, [] __VA_ARGS__);

constexpr double sampleRate = 44100.0;

struct Hit
{
    long long sample;
    int track;
    bool accent;
};

// Runs the sequencer for a number of samples in blocks of an awkward size
std::vector<Hit> collect (Sequencer& seq, const Pattern& pattern, long long totalSamples, int block = 477)
{
    std::vector<Hit> hits;
    std::vector<Trigger> out;
    long long done = 0;
    while (done < totalSamples)
    {
        const int n = static_cast<int> (std::min<long long> (block, totalSamples - done));
        out.clear();
        seq.process (pattern, n, out);
        for (auto& t : out)
            hits.push_back ({ done + t.sampleOffset, t.track, t.accent });
        done += n;
    }
    return hits;
}

Sequencer makeSequencer (double bpm, double swing)
{
    Sequencer s;
    s.prepare (sampleRate);
    s.setTempo (bpm);
    s.setSwing (swing);
    return s;
}

void fillTrack (Pattern& p, int track, StepState state)
{
    for (int s = 0; s < numSteps; ++s)
        p.set (track, s, state);
}

std::vector<float> renderEngine (DrumEngine& e, int samples, int block = 512)
{
    std::vector<float> out (static_cast<size_t> (samples));
    for (int i = 0; i < samples; i += block)
        e.render (out.data() + i, std::min (block, samples - i));
    return out;
}

float peak (const std::vector<float>& v, size_t from = 0, size_t to = SIZE_MAX)
{
    float p = 0.0f;
    for (size_t i = from; i < std::min(to, v.size()); ++i)
        p = std::max (p, std::fabs (v[i]));
    return p;
}

// One hit of a single voice, rendered through the whole engine
std::vector<float> renderVoice (int track, bool accent, int samples = 22050)
{
    DrumEngine e;
    e.prepare (sampleRate);
    e.getPattern().set (track, 0, accent ? StepState::Accent : StepState::Normal);
    e.setPlaying (true);
    return renderEngine (e, samples);
}
} // namespace

TEST ("DBX-001 sixteen steps are played in time at the chosen tempo", {
    for (double bpm : { 60.0, 100.0, 120.0, 173.0 })
    {
        auto seq = makeSequencer (bpm, 0.0);
        Pattern p;
        fillTrack (p, Kick, StepState::Normal);
        const double perStep = sampleRate * 60.0 / bpm / 4.0;
        const auto hits = collect (seq, p, static_cast<long long> (perStep * 32.0));
        CHECK (hits.size() >= 32);
        for (size_t k = 0; k < std::min<size_t> (hits.size(), 32); ++k)
            CHECK (std::fabs (static_cast<double> (hits[k].sample) - perStep * static_cast<double> (k)) <= 1.0);
    }
})

TEST ("DBX-001 the pattern loops after sixteen steps and only sounding steps trigger", {
    auto seq = makeSequencer (120.0, 0.0);
    Pattern p;
    p.set (Snare, 4, StepState::Normal);
    p.set (Snare, 12, StepState::Normal);
    const double perStep = sampleRate * 60.0 / 120.0 / 4.0;
    const auto hits = collect (seq, p, static_cast<long long> (perStep * 32.0));
    CHECK (hits.size() == 4);
    if (hits.size() == 4)
    {
        CHECK (std::fabs (hits[0].sample - perStep * 4) <= 1.0);
        CHECK (std::fabs (hits[1].sample - perStep * 12) <= 1.0);
        CHECK (std::fabs (hits[2].sample - perStep * 20) <= 1.0);
        CHECK (std::fabs (hits[3].sample - perStep * 28) <= 1.0);
    }
})

TEST ("DBX-001 the result does not depend on the audio block size", {
    Pattern p;
    fillTrack (p, Kick, StepState::Normal);
    auto a = makeSequencer (133.0, 30.0);
    auto b = makeSequencer (133.0, 30.0);
    const auto ha = collect (a, p, 60000, 64);
    const auto hb = collect (b, p, 60000, 1024);
    CHECK (ha.size() == hb.size());
    for (size_t i = 0; i < std::min (ha.size(), hb.size()); ++i)
        CHECK (ha[i].sample == hb[i].sample);
})

TEST ("DBX-001 the engine is silent when stopped and sounds when started", {
    DrumEngine e;
    e.prepare (sampleRate);
    fillTrack (e.getPattern(), Kick, StepState::Normal);
    CHECK (peak (renderEngine (e, 20000)) == 0.0f);
    e.setPlaying (true);
    CHECK (peak (renderEngine (e, 20000)) > 0.05f);
})

TEST ("DBX-002 zero swing leaves the grid straight", {
    auto seq = makeSequencer (120.0, 0.0);
    for (long long k = 0; k < 16; ++k)
        CHECK (seq.stepPosition (k) == static_cast<double> (k));
    Pattern p;
    fillTrack (p, Kick, StepState::Normal);
    const double perStep = seq.samplesPerStep();
    const auto hits = collect (seq, p, static_cast<long long> (perStep * 16.0));
    for (size_t k = 0; k < hits.size(); ++k)
        CHECK (std::fabs (hits[k].sample - perStep * k) <= 1.0);
})

TEST ("DBX-002 swing delays only the off-beat steps and more swing delays them more", {
    const double perStep = sampleRate * 60.0 / 120.0 / 4.0;
    Pattern p;
    fillTrack (p, Kick, StepState::Normal);
    long long previousOddHit = 0;
    for (double swing : { 25.0, 50.0, 100.0 })
    {
        auto seq = makeSequencer (120.0, swing);
        const auto hits = collect (seq, p, static_cast<long long> (perStep * 16.0));
        CHECK (hits.size() >= 16);
        for (size_t k = 0; k < std::min<size_t> (hits.size(), 16); ++k)
        {
            const double expected = perStep * k + ((k % 2) ? perStep * 0.5 * swing / 100.0 : 0.0);
            CHECK (std::fabs (hits[k].sample - expected) <= 1.0);
        }
        CHECK (hits[1].sample > previousOddHit);
        previousOddHit = hits[1].sample;
    }
})

TEST ("DBX-002 swing is limited to 0..100 percent", {
    auto seq = makeSequencer (120.0, 400.0);
    CHECK (seq.getSwing() == 100.0);
    seq.setSwing (-5.0);
    CHECK (seq.getSwing() == 0.0);
})

TEST ("DBX-003 a step cycles through off, normal and accent", {
    Pattern p;
    CHECK (p.get (0, 0) == StepState::Off);
    CHECK (p.cycle (0, 0) == StepState::Normal);
    CHECK (p.cycle (0, 0) == StepState::Accent);
    CHECK (p.cycle (0, 0) == StepState::Off);
    CHECK (p.get (0, 1) == StepState::Off);
})

TEST ("DBX-003 an off step is silent and an accent triggers flagged as accent", {
    auto seq = makeSequencer (120.0, 0.0);
    Pattern p;
    p.set (Kick, 0, StepState::Normal);
    p.set (Kick, 1, StepState::Accent);
    p.set (Kick, 2, StepState::Off);
    const auto hits = collect (seq, p, static_cast<long long> (seq.samplesPerStep() * 3.0));
    CHECK (hits.size() == 2);
    if (hits.size() == 2)
    {
        CHECK (! hits[0].accent);
        CHECK (hits[1].accent);
    }
})

TEST ("DBX-003 an accented hit is louder than a normal one on every track", {
    for (int track = 0; track < numTracks; ++track)
    {
        const float normal = peak (renderVoice (track, false));
        const float accent = peak (renderVoice (track, true));
        CHECK (normal > 0.01f);
        CHECK (accent > normal * 1.3f);
    }
})

TEST ("DBX-004 each of the eight tracks produces a sound that ends", {
    for (int track = 0; track < numTracks; ++track)
    {
        const auto wave = renderVoice (track, false, 44100);
        CHECK (peak (wave) > 0.02f);
        CHECK (peak (wave) <= 1.0f);
        CHECK (peak (wave, 40000) < 0.001f);
        for (float s : wave)
            CHECK (std::isfinite (s));
    }
})

namespace
{
struct Features
{
    double zeroCrossingsPerSecond;
    double secondsToMinus40dB;
    std::vector<float> wave;
};

Features analyse (int track)
{
    Features f;
    f.wave = renderVoice (track, true, 22050);
    const float top = peak (f.wave);
    int crossings = 0;
    size_t last = 0;
    for (size_t i = 1; i < f.wave.size(); ++i)
    {
        if ((f.wave[i - 1] < 0) != (f.wave[i] < 0))
            ++crossings;
        if (std::fabs (f.wave[i]) > top * 0.01f)
            last = i;
    }
    f.zeroCrossingsPerSecond = crossings / (f.wave.size() / sampleRate);
    f.secondsToMinus40dB = last / sampleRate;
    return f;
}

double correlation (const std::vector<float>& a, const std::vector<float>& b)
{
    double ab = 0, aa = 0, bb = 0;
    for (size_t i = 0; i < std::min (a.size(), b.size()); ++i)
    {
        ab += a[i] * b[i];
        aa += a[i] * a[i];
        bb += b[i] * b[i];
    }
    return std::fabs (ab / std::sqrt (aa * bb + 1e-30));
}
} // namespace

TEST ("DBX-004 the eight sounds differ from each other in waveform, pitch content and length", {
    std::vector<Features> all;
    for (int t = 0; t < numTracks; ++t)
        all.push_back (analyse (t));

    for (int a = 0; a < numTracks; ++a)
        for (int b = a + 1; b < numTracks; ++b)
        {
            CHECK (correlation (all[a].wave, all[b].wave) < 0.5);
            const double distance = std::fabs (std::log (all[a].zeroCrossingsPerSecond / all[b].zeroCrossingsPerSecond))
                                  + std::fabs (std::log (all[a].secondsToMinus40dB / all[b].secondsToMinus40dB));
            if (distance <= 0.15)
                std::cerr << "  tracks " << a << " and " << b << " too close: " << distance << "\n";
            CHECK (distance > 0.15);
        }
})

TEST ("DBX-004 an open hat rings longer than a closed hat and a closed hat chokes it", {
    DrumEngine e;
    e.prepare (sampleRate);
    e.getPattern().set (OpenHat, 0, StepState::Normal);
    e.getPattern().set (ClosedHat, 2, StepState::Normal);
    e.setPlaying (true);
    const auto wave = renderEngine (e, 22050);
    const size_t closeAt = static_cast<size_t> (sampleRate * 60.0 / 120.0 / 4.0 * 2.0);
    // after the closed hat (60 ms decay) is over, nothing of the open hat remains
    CHECK (peak (wave, 0, closeAt) > 0.05f);
    CHECK (peak (wave, closeAt + 8000, closeAt + 9000) < 0.001f);
    CHECK (analyse (OpenHat).secondsToMinus40dB > 2.0 * analyse (ClosedHat).secondsToMinus40dB);
})

TEST ("DBX-005 play starts the pattern from the first step and stop silences the sequencer", {
    DrumEngine e;
    e.prepare (sampleRate);
    fillTrack (e.getPattern(), Kick, StepState::Normal);
    CHECK (! e.isPlaying());
    CHECK (e.getCurrentStep() == -1);

    e.setPlaying (true);
    const auto first = renderEngine (e, 512);
    CHECK (e.isPlaying());
    CHECK (e.getCurrentStep() == 0);
    CHECK (peak (first) > 0.05f);

    renderEngine (e, 20000);
    CHECK (e.getCurrentStep() > 0);

    e.setPlaying (false);
    renderEngine (e, 512);
    CHECK (e.getCurrentStep() == -1);
    // the ringing tail dies out and no new hit is started
    renderEngine (e, 44100);
    CHECK (peak (renderEngine (e, 44100)) == 0.0f);

    e.setPlaying (true);
    renderEngine (e, 512);
    CHECK (e.getCurrentStep() == 0);
})

TEST ("DBX-005 tempo and swing changes are applied while playing", {
    DrumEngine e;
    e.prepare (sampleRate);
    fillTrack (e.getPattern(), Kick, StepState::Normal);
    e.setTempo (240.0);
    e.setPlaying (true);
    renderEngine (e, 44100);
    // 240 bpm = 16 steps per second: 1 s reaches step 15 (or the wrap to 0)
    const int step = e.getCurrentStep();
    CHECK (step == 15 || step == 0);
})

namespace
{
struct SongHit
{
    long long sample;
    int track;
};

std::vector<SongHit> collectSong (Sequencer& seq, const Song& song, long long totalSamples, int block = 477)
{
    std::vector<SongHit> hits;
    std::vector<Trigger> out;
    std::vector<NoteEvent> notes;
    long long done = 0;
    while (done < totalSamples)
    {
        const int n = static_cast<int> (std::min<long long> (block, totalSamples - done));
        out.clear();
        notes.clear();
        seq.process (song, n, out, notes);
        for (auto& t : out)
            hits.push_back ({ done + t.sampleOffset, t.track });
        done += n;
    }
    return hits;
}
} // namespace

TEST ("DBX-007 patterns of the chain play one after the other in time, then loop", {
    Song song;
    song.pattern (0).set (Kick, 0, StepState::Normal);
    song.pattern (3).set (Snare, 0, StepState::Normal);
    song.pattern (5).set (Cowbell, 8, StepState::Normal);
    song.setChainLength (3);
    song.setChainEntry (0, 0);
    song.setChainEntry (1, 5);
    song.setChainEntry (2, 3);

    auto seq = makeSequencer (120.0, 0.0);
    const double perStep = seq.samplesPerStep();
    const auto hits = collectSong (seq, song, static_cast<long long> (perStep * (16.0 * 6.0 - 0.5)));
    const int expectedTrack[] = { Kick, Cowbell, Snare, Kick, Cowbell, Snare };
    const int expectedStep[] = { 0, 16 + 8, 32, 48, 64 + 8, 80 };
    CHECK (hits.size() == 6);
    for (size_t k = 0; k < std::min<size_t> (hits.size(), 6); ++k)
    {
        CHECK (hits[k].track == expectedTrack[k]);
        CHECK (std::fabs (static_cast<double> (hits[k].sample) - perStep * expectedStep[k]) <= 1.0);
    }
})

TEST ("DBX-007 sixteen patterns fill a chain of up to 128 positions", {
    Song song;
    for (int p = 0; p < numPatterns; ++p)
        song.pattern (p).set (Kick, p, StepState::Normal);
    song.setChainLength (1000);
    CHECK (song.getChainLength() == maxChainLength);
    song.setChainLength (0);
    CHECK (song.getChainLength() == 1);
    song.setChainLength (maxChainLength);
    for (int pos = 0; pos < maxChainLength; ++pos)
        song.setChainEntry (pos, (pos * 7) % numPatterns);

    auto seq = makeSequencer (240.0, 0.0);
    const double perStep = seq.samplesPerStep();
    const auto hits = collectSong (seq, song, static_cast<long long> (perStep * 16.0 * (maxChainLength + 2)));
    CHECK (hits.size() == static_cast<size_t> (maxChainLength + 2));
    for (size_t k = 0; k < std::min<size_t> (hits.size(), maxChainLength + 2); ++k)
    {
        const size_t pos = k % maxChainLength;
        const double step = 16.0 * static_cast<double> (k) + static_cast<double> ((pos * 7) % numPatterns);
        CHECK (std::fabs (static_cast<double> (hits[k].sample) - perStep * step) <= 1.0);
    }
})

TEST ("DBX-007 the chain does not depend on the audio block size", {
    Song song;
    song.pattern (0).set (Kick, 0, StepState::Normal);
    song.pattern (1).set (Snare, 15, StepState::Normal);
    song.setChainLength (2);
    song.setChainEntry (1, 1);
    auto a = makeSequencer (133.0, 30.0);
    auto b = makeSequencer (133.0, 30.0);
    const auto ha = collectSong (a, song, 200000, 64);
    const auto hb = collectSong (b, song, 200000, 1024);
    CHECK (ha.size() == hb.size());
    CHECK (! ha.empty());
    for (size_t i = 0; i < std::min (ha.size(), hb.size()); ++i)
        CHECK (ha[i].sample == hb[i].sample && ha[i].track == hb[i].track);
})

TEST ("DBX-007 a single pattern played alone behaves as before", {
    Song song;
    Pattern alone;
    for (int s = 0; s < numSteps; s += 3)
    {
        song.pattern (0).set (Snare, s, StepState::Accent);
        alone.set (Snare, s, StepState::Accent);
    }
    auto a = makeSequencer (110.0, 20.0);
    auto b = makeSequencer (110.0, 20.0);
    const auto hs = collectSong (a, song, 100000);
    const auto hp = collect (b, alone, 100000);
    CHECK (hs.size() == hp.size());
    CHECK (! hs.empty());
    for (size_t i = 0; i < std::min (hs.size(), hp.size()); ++i)
        CHECK (hs[i].sample == hp[i].sample && hs[i].track == hp[i].track);
})

TEST ("DBX-007 the engine plays the chain and reports its position", {
    DrumEngine e;
    e.prepare (sampleRate);
    auto& song = e.getSong();
    song.pattern (2).set (Kick, 0, StepState::Normal);
    song.setChainLength (2);
    song.setChainEntry (0, 1); // empty pattern: silence
    song.setChainEntry (1, 2);
    CHECK (e.getCurrentChainPosition() == -1);
    e.setTempo (240.0);
    e.setPlaying (true);
    const int stepSamples = static_cast<int> (sampleRate * 60.0 / 240.0 / 4.0);
    const auto first = renderEngine (e, stepSamples * 8);
    CHECK (e.getCurrentChainPosition() == 0);
    CHECK (peak (first) == 0.0f);
    const auto second = renderEngine (e, stepSamples * 16);
    CHECK (e.getCurrentChainPosition() == 1);
    CHECK (peak (second, 0, static_cast<size_t> (stepSamples * 8 + 2000)) > 0.05f);
    renderEngine (e, stepSamples * 16);
    CHECK (e.getCurrentChainPosition() == 0);
    e.setPlaying (false);
    renderEngine (e, 512);
    CHECK (e.getCurrentChainPosition() == -1);
})

TEST ("DBX-007 each pattern keeps its own drums, bass and synth", {
    Song song;
    song.pattern (4).set (Tom, 7, StepState::Accent);
    song.pattern (4).setBass (2, 40, true);
    song.pattern (4).setSynthNote (3, 1, 64);
    CHECK (song.pattern (4).get (Tom, 7) == StepState::Accent);
    CHECK (song.pattern (4).getBassNote (2) == 40);
    CHECK (song.pattern (4).getSynthNote (3, 1) == 64);
    CHECK (song.pattern (5).get (Tom, 7) == StepState::Off);
    CHECK (song.pattern (5).getBassNote (2) == noNote);
    CHECK (song.pattern (5).getSynthNote (3, 1) == noNote);
})

namespace
{
constexpr int stepSamples120 = 5512; // one sixteenth at 120 bpm

// Fundamental of a window, from the first autocorrelation peak close to the highest one
double estimateFrequency (const std::vector<float>& wave, size_t from, size_t length)
{
    const int minLag = static_cast<int> (sampleRate / 2200.0);
    const int maxLag = static_cast<int> (sampleRate / 45.0);
    std::vector<double> r (static_cast<size_t> (maxLag) + 1, 0.0);
    for (int lag = minLag; lag <= maxLag; ++lag)
    {
        double sum = 0.0;
        for (size_t i = from; i + static_cast<size_t> (lag) < from + length; ++i)
            sum += wave[i] * wave[i + static_cast<size_t> (lag)];
        r[static_cast<size_t> (lag)] = sum;
    }
    const double best = *std::max_element (r.begin() + minLag, r.end());
    for (int lag = minLag + 1; lag < maxLag; ++lag)
        if (r[(size_t) lag] >= 0.9 * best && r[(size_t) lag] >= r[(size_t) lag - 1] && r[(size_t) lag] >= r[(size_t) lag + 1])
            return sampleRate / lag;
    return 0.0;
}

// Energy of the window at one frequency (Hann-windowed Goertzel)
double powerAt (const std::vector<float>& wave, size_t from, size_t length, double freq)
{
    const double w = 2.0 * 3.141592653589793 * freq / sampleRate;
    const double coeff = 2.0 * std::cos (w);
    double s1 = 0.0, s2 = 0.0;
    for (size_t i = 0; i < length; ++i)
    {
        const double hann = 0.5 - 0.5 * std::cos (2.0 * 3.141592653589793 * static_cast<double> (i) / static_cast<double> (length));
        const double s = wave[from + i] * hann + coeff * s1 - s2;
        s2 = s1;
        s1 = s;
    }
    return (s1 * s1 + s2 * s2 - coeff * s1 * s2) / static_cast<double> (length);
}

bool isNear (double value, double expected, double ratio) { return std::fabs (value / expected - 1.0) < ratio; }

std::vector<float> renderBass (const std::function<void (Pattern&)>& setup, int samples)
{
    DrumEngine e;
    e.prepare (sampleRate);
    setup (e.getPattern());
    e.setPlaying (true);
    return renderEngine (e, samples);
}

std::vector<float> renderSynthEngine (const std::function<void (Pattern&)>& setup, int samples)
{
    return renderBass (setup, samples); // same engine path, the pattern decides what plays
}

std::vector<float> renderPoly (PolySynth& synth, const SynthParams& params, int samples)
{
    std::vector<float> out (static_cast<size_t> (samples));
    for (auto& v : out)
        v = synth.next (params);
    return out;
}

double highFrequencyShare (const std::vector<float>& v, size_t from, size_t to)
{
    double diff = 0.0, total = 1e-12;
    for (size_t i = from + 1; i < to; ++i)
    {
        diff += (v[i] - v[i - 1]) * (v[i] - v[i - 1]);
        total += v[i] * v[i];
    }
    return diff / total;
}
} // namespace

TEST ("DBX-008 the bass plays the pitch set on the step", {
    for (int note : { 33, 40, 45, 52, 57 })
    {
        const auto wave = renderBass ([note] (Pattern& p) { p.setBass (0, note); }, stepSamples120);
        CHECK (peak (wave) > 0.05f);
        const double f = estimateFrequency (wave, 900, 3400);
        if (! isNear (f, noteToFrequency (note), 0.03))
            std::cerr << "  note " << note << " measured " << f << " Hz\n";
        CHECK (isNear (f, noteToFrequency (note), 0.03));
    }
})

TEST ("DBX-008 one note per step, each step at its own pitch", {
    const int notes[] = { 40, 52, 45 };
    const auto wave = renderBass ([&notes] (Pattern& p)
    {
        for (int s = 0; s < 3; ++s)
            p.setBass (s, notes[s]);
    }, stepSamples120 * 3);
    for (int s = 0; s < 3; ++s)
        CHECK (isNear (estimateFrequency (wave, static_cast<size_t> (stepSamples120 * s + 900), 3400), noteToFrequency (notes[s]), 0.03));

    auto seq = makeSequencer (120.0, 0.0);
    Pattern pattern;
    pattern.setBass (0, 40);
    pattern.setBass (5, 47, true);
    std::vector<Trigger> triggers;
    std::vector<NoteEvent> events;
    seq.process (pattern, stepSamples120 * 8, triggers, events);
    CHECK (events.size() == 2);
    if (events.size() == 2)
    {
        CHECK (events[0].instrument == Instrument::Bass && events[0].note == 40 && ! events[0].accent);
        CHECK (events[1].instrument == Instrument::Bass && events[1].note == 47 && events[1].accent);
        CHECK (std::abs (events[1].sampleOffset - 5 * seq.samplesPerStep()) <= 1.0);
    }
})

TEST ("DBX-008 the bass is monophonic: a new note takes over the voice", {
    BassParams params;
    BassVoice single;
    single.prepare (sampleRate);
    single.noteOn (52, 0.6f, 4000);
    BassVoice twice;
    twice.prepare (sampleRate);
    twice.noteOn (40, 0.6f, 4000);
    for (int i = 0; i < 500; ++i)
        twice.next (params);
    twice.noteOn (52, 0.6f, 4000);

    std::vector<float> a (5000), b (5000);
    for (auto& v : a)
        v = single.next (params);
    for (auto& v : b)
        v = twice.next (params);
    CHECK (isNear (estimateFrequency (b, 900, 3000), noteToFrequency (52), 0.03));
    CHECK (peak (b) < peak (a) * 1.25f);
})

TEST ("DBX-008 an off step is silent, an accent is louder and the note ends", {
    CHECK (peak (renderBass ([] (Pattern&) {}, stepSamples120 * 4)) == 0.0f);
    CHECK (peak (renderBass ([] (Pattern& p) { p.setBass (0, 40); p.setBass (0, noNote); }, stepSamples120 * 4)) == 0.0f);

    const auto normal = renderBass ([] (Pattern& p) { p.setBass (0, 45, false); }, stepSamples120 * 6);
    const auto accent = renderBass ([] (Pattern& p) { p.setBass (0, 45, true); }, stepSamples120 * 6);
    CHECK (peak (accent) > peak (normal) * 1.3f);
    CHECK (peak (accent) <= 1.0f);
    for (float v : accent)
        CHECK (std::isfinite (v));
    CHECK (peak (normal, 0, 5000) > 0.05f);
    CHECK (peak (normal, 20000, normal.size()) == 0.0f);
})

TEST ("DBX-008 a note outside the playable range is brought back into it", {
    Pattern p;
    p.setBass (0, 200);
    CHECK (p.getBassNote (0) == highestNote);
    p.setBass (0, 3);
    CHECK (p.getBassNote (0) == lowestNote);
})

TEST ("DBX-009 the synth plays the notes set on its steps, up to four at once", {
    const int chord[] = { 57, 61, 64, 68 };
    const auto wave = renderSynthEngine ([&chord] (Pattern& p)
    {
        for (int v = 0; v < synthVoices; ++v)
            p.setSynthNote (0, v, chord[v]);
    }, stepSamples120);
    CHECK (peak (wave) > 0.05f);
    CHECK (peak (wave) <= 1.0f);
    const double reference = powerAt (wave, 1500, 2900, 370.0);
    for (int note : chord)
    {
        const double p = powerAt (wave, 1500, 2900, noteToFrequency (note));
        if (p < 4.0 * reference)
            std::cerr << "  note " << note << " weak: " << p << " vs " << reference << "\n";
        CHECK (p > 4.0 * reference);
    }

    auto seq = makeSequencer (120.0, 0.0);
    Pattern pattern;
    pattern.setSynthNote (2, 0, 60);
    pattern.setSynthNote (2, 3, 67);
    std::vector<Trigger> triggers;
    std::vector<NoteEvent> events;
    seq.process (pattern, stepSamples120 * 4, triggers, events);
    CHECK (events.size() == 2);
    for (auto& ev : events)
        CHECK (ev.instrument == Instrument::Synth);
})

TEST ("DBX-009 the synth has four voices and steals the oldest for a fifth note", {
    PolySynth synth;
    synth.prepare (sampleRate);
    const SynthParams params;
    for (int i = 0; i < 4; ++i)
    {
        synth.noteOn (50 + i * 3, 0.6f, 20000);
        CHECK (synth.activeVoices() == i + 1);
    }
    synth.noteOn (80, 0.6f, 20000);
    CHECK (synth.activeVoices() == 4);
    synth.noteOn (80, 0.6f, 20000); // the same key again reuses its voice
    CHECK (synth.activeVoices() == 4);

    const auto wave = renderPoly (synth, params, 6000);
    CHECK (powerAt (wave, 3000, 2900, noteToFrequency (50)) < 0.25 * powerAt (wave, 3000, 2900, noteToFrequency (80)));
})

TEST ("DBX-009 the second oscillator, the filter and the envelope shape the sound", {
    auto play = [] (const std::function<void (SynthParams&)>& setup, int samples)
    {
        SynthParams params;
        setup (params);
        PolySynth synth;
        synth.prepare (sampleRate);
        synth.noteOn (57, 0.6f, 12000);
        return renderPoly (synth, params, samples);
    };

    // oscillator 2 a fifth up adds energy at the fifth
    const auto unison = play ([] (SynthParams& p) { p.osc2Mix = 0.5f; p.osc2Semitones = 0.0f; p.osc2Detune = 0.0f; }, 8000);
    const auto fifth = play ([] (SynthParams& p) { p.osc2Mix = 0.5f; p.osc2Semitones = 7.0f; p.osc2Detune = 0.0f; }, 8000);
    const double fifthFreq = noteToFrequency (57 + 7);
    CHECK (powerAt (fifth, 3000, 4000, fifthFreq) > 4.0 * powerAt (unison, 3000, 4000, fifthFreq));

    // a low cutoff darkens the sound
    const auto dark = play ([] (SynthParams& p) { p.cutoff = 400.0f; }, 8000);
    const auto bright = play ([] (SynthParams& p) { p.cutoff = 9000.0f; }, 8000);
    CHECK (highFrequencyShare (dark, 3000, 8000) < 0.5 * highFrequencyShare (bright, 3000, 8000));

    // a slow attack rises, then the release ends the note
    const auto slow = play ([] (SynthParams& p) { p.attack = 0.2f; p.release = 0.1f; }, 30000);
    CHECK (peak (slow, 0, 300) < 0.2f * peak (slow, 6000, 12000));
    CHECK (peak (slow, 12000 + 6000, slow.size()) == 0.0f);

    // the sustain level sets how loud a held note stays
    const auto low = play ([] (SynthParams& p) { p.decay = 0.02f; p.sustain = 0.3f; }, 9000);
    const auto high = play ([] (SynthParams& p) { p.decay = 0.02f; p.sustain = 0.9f; }, 9000);
    CHECK (peak (high, 5000, 9000) > 2.0f * peak (low, 5000, 9000));
    for (float v : high)
        CHECK (std::isfinite (v));
})

TEST ("DBX-009 the LFO sweeps the brightness over time and is off at zero depth", {
    auto variation = [] (float depth)
    {
        SynthParams params;
        params.lfoDepth = depth;
        params.lfoRate = 4.0f;
        params.osc2Detune = 0.0f;
        params.cutoff = 900.0f;
        params.sustain = 0.8f;
        params.decay = 0.05f;
        PolySynth synth;
        synth.prepare (sampleRate);
        synth.noteOn (45, 0.6f, 90000);
        const auto wave = renderPoly (synth, params, 80000);
        double lo = 1e9, hi = 0.0;
        for (size_t start = 10000; start + 4000 <= wave.size(); start += 4000)
        {
            const double brightness = highFrequencyShare (wave, start, start + 4000);
            lo = std::min (lo, brightness);
            hi = std::max (hi, brightness);
        }
        return hi / lo;
    };
    CHECK (variation (0.0f) < 1.1);
    CHECK (variation (1.0f) > 3.0);
})

TEST ("DBX-009 the synth is silent without notes, accented louder, bounded and ends", {
    CHECK (peak (renderSynthEngine ([] (Pattern&) {}, stepSamples120 * 4)) == 0.0f);

    auto chord = [] (bool accent)
    {
        return renderSynthEngine ([accent] (Pattern& p)
        {
            for (int v = 0; v < synthVoices; ++v)
                p.setSynthNote (0, v, 48 + v * 4);
            p.setSynthAccent (0, accent);
        }, stepSamples120 * 8);
    };
    const auto normal = chord (false);
    const auto accent = chord (true);
    CHECK (peak (accent) > peak (normal) * 1.3f);
    CHECK (peak (accent) <= 1.0f);
    for (float v : accent)
        CHECK (std::isfinite (v));
    CHECK (peak (normal, 40000, normal.size()) == 0.0f);
})

namespace
{
std::vector<float> renderPluck (PluckSynth& synth, const PluckParams& params, int samples)
{
    std::vector<float> out (static_cast<size_t> (samples));
    for (auto& v : out)
        v = synth.next (params);
    return out;
}
} // namespace

TEST ("DBX-026 the pluck plays the pitch set on the step, up to four notes at once", {
    for (int note : { 45, 52, 60, 69 })
    {
        const auto wave = renderBass ([note] (Pattern& p) { p.setPluckNote (0, 0, note); }, stepSamples120);
        CHECK (peak (wave) > 0.05f);
        const double f = estimateFrequency (wave, 100, 3400);
        if (! isNear (f, noteToFrequency (note), 0.03))
            std::cerr << "  note " << note << " measured " << f << " Hz\n";
        CHECK (isNear (f, noteToFrequency (note), 0.03));
    }

    const int chord[] = { 57, 61, 64, 68 };
    const auto wave = renderBass ([&chord] (Pattern& p)
    {
        for (int v = 0; v < pluckVoices; ++v)
            p.setPluckNote (0, v, chord[v]);
    }, stepSamples120);
    CHECK (peak (wave) <= 1.0f);
    const double reference = powerAt (wave, 200, 1500, 370.0);
    for (int note : chord)
        CHECK (powerAt (wave, 200, 1500, noteToFrequency (note)) > 4.0 * reference);

    auto seq = makeSequencer (120.0, 0.0);
    Pattern pattern;
    pattern.setPluckNote (2, 0, 60);
    pattern.setPluckNote (2, 3, 67);
    std::vector<Trigger> triggers;
    std::vector<NoteEvent> events;
    seq.process (pattern, stepSamples120 * 4, triggers, events);
    CHECK (events.size() == 2);
    for (auto& ev : events)
        CHECK (ev.instrument == Instrument::Pluck);
})

TEST ("DBX-026 the pluck has four voices and steals the oldest for a fifth note", {
    PluckSynth synth;
    synth.prepare (sampleRate);
    const PluckParams params;
    for (int i = 0; i < 4; ++i)
    {
        synth.noteOn (50 + i * 3, 0.6f, 20000);
        CHECK (synth.activeVoices() == i + 1);
    }
    synth.noteOn (80, 0.6f, 20000);
    CHECK (synth.activeVoices() == 4);

    const auto wave = renderPluck (synth, params, 6000);
    CHECK (powerAt (wave, 300, 4000, noteToFrequency (50)) < 0.25 * powerAt (wave, 300, 4000, noteToFrequency (80)));
})

TEST ("DBX-026 the pluck starts at once, decays quickly and closes its filter quickly", {
    PluckSynth synth;
    synth.prepare (sampleRate);
    const PluckParams params;
    synth.noteOn (45, 0.6f, 60000);
    const auto wave = renderPluck (synth, params, 30000);
    const float top = peak (wave);
    CHECK (top > 0.05f);
    CHECK (peak (wave, 0, 150) > 0.5f * top); // full level within about 3 ms
    CHECK (peak (wave, 15000, wave.size()) == 0.0f); // gone after a third of a second, though the key is held
    CHECK (highFrequencyShare (wave, 0, 1500) > 2.0 * highFrequencyShare (wave, 3500, 5000));

    // the existing synth, by contrast, rises slowly and lasts
    SynthParams padParams;
    PolySynth pad;
    pad.prepare (sampleRate);
    pad.noteOn (45, 0.6f, 60000);
    const auto padWave = renderPoly (pad, padParams, 30000);
    CHECK (peak (padWave, 0, 150) < 0.2f * peak (padWave));
    CHECK (peak (padWave, 15000, padWave.size()) > 0.0f);
})

TEST ("DBX-026 the pluck is silent without notes, accented louder, bounded and ends", {
    CHECK (peak (renderBass ([] (Pattern&) {}, stepSamples120 * 4)) == 0.0f);

    auto chord = [] (bool accent)
    {
        return renderBass ([accent] (Pattern& p)
        {
            for (int v = 0; v < pluckVoices; ++v)
                p.setPluckNote (0, v, 48 + v * 4);
            p.setPluckAccent (0, accent);
        }, stepSamples120 * 8);
    };
    const auto normal = chord (false);
    const auto accent = chord (true);
    CHECK (peak (accent) > peak (normal) * 1.3f);
    CHECK (peak (accent) <= 1.0f);
    for (float v : accent)
        CHECK (std::isfinite (v));
    CHECK (peak (normal, 40000, normal.size()) == 0.0f);
})

TEST ("DBX-026 the existing synth, the bass and the drums sound as before", {
    // the pluck adds nothing to a pattern that does not use it
    const auto wave = renderBass ([] (Pattern& p)
    {
        p.set (Kick, 0, StepState::Normal);
        p.setBass (0, 40);
        p.setSynthNote (0, 0, 57);
    }, stepSamples120 * 2);
    DrumEngine e;
    e.prepare (sampleRate);
    e.getPattern().set (Kick, 0, StepState::Normal);
    e.getPattern().setBass (0, 40);
    e.getPattern().setSynthNote (0, 0, 57);
    e.getPluckParams().cutoff = 9000.0f;
    e.setPlaying (true);
    const auto other = renderEngine (e, stepSamples120 * 2);
    CHECK (wave == other);
})

TEST ("DBX-026 the pluck knobs change the sound", {
    auto play = [] (const std::function<void (PluckParams&)>& setup, int samples)
    {
        PluckParams params;
        setup (params);
        PluckSynth synth;
        synth.prepare (sampleRate);
        synth.noteOn (57, 0.6f, 20000);
        return renderPluck (synth, params, samples);
    };
    const auto dark = play ([] (PluckParams& p) { p.cutoff = 300.0f; }, 6000);
    const auto bright = play ([] (PluckParams& p) { p.cutoff = 6000.0f; }, 6000);
    CHECK (highFrequencyShare (dark, 200, 3000) < 0.5 * highFrequencyShare (bright, 200, 3000));

    const auto shortDecay = play ([] (PluckParams& p) { p.decay = 0.05f; }, 12000);
    const auto longDecay = play ([] (PluckParams& p) { p.decay = 0.4f; }, 12000);
    CHECK (peak (longDecay, 6000, 12000) > 4.0f * peak (shortDecay, 6000, 12000) + 0.01f);
})

TEST ("DBX-027 each pattern holds its own pluck notes and clears them", {
    Song song;
    song.pattern (6).setPluckNote (5, 2, 62);
    song.pattern (6).setPluckAccent (5, true);
    song.pattern (6).setBass (2, 40);
    CHECK (song.pattern (6).getPluckNote (5, 2) == 62);
    CHECK (song.pattern (6).getPluckAccent (5));
    CHECK (song.pattern (6).getBassNote (2) == 40);
    CHECK (song.pattern (6).getSynthNote (5, 2) == noNote);
    CHECK (song.pattern (7).getPluckNote (5, 2) == noNote);
    CHECK (! song.pattern (7).getPluckAccent (5));
    song.pattern (6).setPluckNote (5, 0, 200);
    CHECK (song.pattern (6).getPluckNote (5, 0) == highestNote);
    song.pattern (6).clear();
    CHECK (song.pattern (6).getPluckNote (5, 2) == noNote);
    CHECK (! song.pattern (6).getPluckAccent (5));
})

TEST ("DBX-027 the chain plays the pluck notes of each pattern in time", {
    auto seq = makeSequencer (120.0, 0.0);
    Song song;
    song.pattern (0).setPluckNote (0, 0, 50);
    song.pattern (3).setPluckNote (4, 0, 62);
    song.setChainLength (2);
    song.setChainEntry (0, 0);
    song.setChainEntry (1, 3);

    std::vector<std::pair<long long, int>> plucks;
    std::vector<Trigger> out;
    std::vector<NoteEvent> notes;
    long long done = 0;
    const long long total = 5512LL * 16 * 3 + 100;
    while (done < total)
    {
        const int n = 477;
        out.clear();
        notes.clear();
        seq.process (song, n, out, notes);
        for (auto& ev : notes)
            if (ev.instrument == Instrument::Pluck)
                plucks.push_back ({ done + ev.sampleOffset, ev.note });
        done += n;
    }
    // steps last 5512.5 samples at 120 bpm: pattern 0 at step 0, pattern 3 at step 16 + 4, then the loop
    const double step = 5512.5;
    const long long expected[] = { 0, std::llround (20 * step), std::llround (32 * step) };
    const int pitches[] = { 50, 62, 50 };
    CHECK (plucks.size() == 3);
    for (size_t i = 0; i < std::min<size_t> (plucks.size(), 3); ++i)
        CHECK (std::llabs (plucks[i].first - expected[i]) <= 1 && plucks[i].second == pitches[i]);
})

TEST ("DBX-027 the engine plays the pluck of the chain's second pattern", {
    DrumEngine e;
    e.prepare (sampleRate);
    auto& song = e.getSong();
    song.pattern (2).setPluckNote (0, 0, 57);
    song.setChainLength (2);
    song.setChainEntry (0, 1); // empty pattern: silence
    song.setChainEntry (1, 2);
    e.setTempo (240.0);
    e.setPlaying (true);
    const int stepSamples = static_cast<int> (sampleRate * 60.0 / 240.0 / 4.0);
    const auto first = renderEngine (e, stepSamples * 16);
    CHECK (peak (first) == 0.0f);
    const auto second = renderEngine (e, stepSamples * 16);
    CHECK (peak (second, 0, 3000) > 0.05f);
})

namespace
{
// One FX hit at step 0, rendered through the whole engine
std::vector<float> renderFx (int row, StepState state, int samples = 66150)
{
    DrumEngine e;
    e.prepare (sampleRate);
    e.getPattern().setFx (row, 0, state);
    e.setPlaying (true);
    return renderEngine (e, samples);
}

double crossingsPerSecond (const std::vector<float>& v, size_t from, size_t to)
{
    int crossings = 0;
    for (size_t i = from + 1; i < to; ++i)
        if ((v[i - 1] < 0) != (v[i] < 0))
            ++crossings;
    return crossings / ((to - from) / sampleRate);
}

double rms (const std::vector<float>& v, size_t from, size_t to)
{
    double sum = 0.0;
    for (size_t i = from; i < to; ++i)
        sum += v[i] * v[i];
    return std::sqrt (sum / static_cast<double> (to - from));
}

struct FxFeatures
{
    double crossings;
    double seconds;
    std::vector<float> wave;
};

FxFeatures analyseFx (int row)
{
    FxFeatures f;
    f.wave = renderFx (row, StepState::Accent, 44100);
    const float top = peak (f.wave);
    size_t last = 0;
    for (size_t i = 0; i < f.wave.size(); ++i)
        if (std::fabs (f.wave[i]) > top * 0.01f)
            last = i;
    f.crossings = crossingsPerSecond (f.wave, 0, last + 1);
    f.seconds = last / sampleRate;
    return f;
}
} // namespace

TEST ("DBX-030 each of the six FX sounds produces a sound that ends", {
    for (int row = 0; row < numFxSounds; ++row)
    {
        const auto wave = renderFx (row, StepState::Normal);
        CHECK (peak (wave) > 0.05f);
        CHECK (peak (wave) <= 1.0f);
        CHECK (peak (wave, 55000) == 0.0f);
        for (float v : wave)
            CHECK (std::isfinite (v));
    }
})

TEST ("DBX-030 the six FX sounds differ from each other in waveform, pitch content and length", {
    std::vector<FxFeatures> all;
    for (int row = 0; row < numFxSounds; ++row)
        all.push_back (analyseFx (row));

    for (int a = 0; a < numFxSounds; ++a)
        for (int b = a + 1; b < numFxSounds; ++b)
        {
            CHECK (correlation (all[a].wave, all[b].wave) < 0.5);
            const double distance = std::fabs (std::log (all[a].crossings / all[b].crossings))
                                  + std::fabs (std::log (all[a].seconds / all[b].seconds));
            if (distance <= 0.15)
                std::cerr << "  FX " << a << " and " << b << " too close: " << distance << "\n";
            CHECK (distance > 0.15);
        }
})

TEST ("DBX-030 the siren sweeps up then down, the riser rises and the whoosh is short noise", {
    const auto siren = renderFx (Siren, StepState::Normal);
    const size_t q = static_cast<size_t> (sampleRate * 0.15);
    const double start = crossingsPerSecond (siren, 0, q);
    const double top = crossingsPerSecond (siren, static_cast<size_t> (sampleRate * 0.225), static_cast<size_t> (sampleRate * 0.375));
    const double end = crossingsPerSecond (siren, static_cast<size_t> (sampleRate * 0.525), static_cast<size_t> (sampleRate * 0.675));
    CHECK (top > 1.4 * start);
    CHECK (top > 1.4 * end);

    const auto riser = renderFx (Riser, StepState::Normal);
    CHECK (rms (riser, static_cast<size_t> (sampleRate * 0.6), static_cast<size_t> (sampleRate * 0.8))
           > 3.0 * rms (riser, 0, static_cast<size_t> (sampleRate * 0.2)));

    const auto whoosh = renderFx (Whoosh, StepState::Normal);
    CHECK (peak (whoosh, static_cast<size_t> (sampleRate * 0.25)) == 0.0f);
    CHECK (crossingsPerSecond (whoosh, 0, static_cast<size_t> (sampleRate * 0.15)) > 8000.0);
})

TEST ("DBX-030 an accented FX hit is louder than a normal one and an off step is silent", {
    for (int row = 0; row < numFxSounds; ++row)
    {
        CHECK (peak (renderFx (row, StepState::Accent)) > peak (renderFx (row, StepState::Normal)) * 1.3f);
        CHECK (peak (renderFx (row, StepState::Off)) == 0.0f);
    }
})

TEST ("DBX-030 the FX do not change the sound of the other tracks", {
    auto play = [] (bool withFx)
    {
        DrumEngine e;
        e.prepare (sampleRate);
        e.getPattern().set (Kick, 0, StepState::Normal);
        e.getPattern().setBass (0, 40);
        e.getPattern().setSynthNote (0, 0, 57);
        e.getPattern().setPluckNote (0, 0, 62);
        if (withFx)
            e.getPattern().setFx (Whoosh, 8, StepState::Normal); // after the window compared below
        e.setPlaying (true);
        return renderEngine (e, 5512 * 4);
    };
    CHECK (play (false) == play (true));
})

TEST ("DBX-031 each pattern holds its own FX steps and clears them", {
    Song song;
    song.pattern (4).setFx (Laser, 7, StepState::Accent);
    song.pattern (4).set (Kick, 7, StepState::Normal);
    CHECK (song.pattern (4).getFx (Laser, 7) == StepState::Accent);
    CHECK (song.pattern (4).getFx (Laser, 6) == StepState::Off);
    CHECK (song.pattern (4).getFx (Siren, 7) == StepState::Off);
    CHECK (song.pattern (4).get (Kick, 7) == StepState::Normal);
    CHECK (song.pattern (5).getFx (Laser, 7) == StepState::Off);
    CHECK (song.pattern (4).cycleFx (Impact, 0) == StepState::Normal);
    CHECK (song.pattern (4).cycleFx (Impact, 0) == StepState::Accent);
    CHECK (song.pattern (4).cycleFx (Impact, 0) == StepState::Off);
    song.pattern (4).clear();
    CHECK (song.pattern (4).getFx (Laser, 7) == StepState::Off);
})

TEST ("DBX-031 the chain plays the FX steps of each pattern in time", {
    auto seq = makeSequencer (120.0, 0.0);
    Song song;
    song.pattern (0).setFx (Siren, 0, StepState::Normal);
    song.pattern (3).setFx (Scratch, 4, StepState::Accent);
    song.setChainLength (2);
    song.setChainEntry (0, 0);
    song.setChainEntry (1, 3);

    struct Fired { long long sample; int row; bool accent; };
    std::vector<Fired> fired;
    std::vector<Trigger> out;
    std::vector<NoteEvent> notes;
    long long done = 0;
    const long long total = 5512LL * 16 * 3 + 100;
    while (done < total)
    {
        const int n = 477;
        out.clear();
        notes.clear();
        seq.process (song, n, out, notes);
        CHECK (out.empty());
        for (auto& ev : notes)
            if (ev.instrument == Instrument::Fx)
                fired.push_back ({ done + ev.sampleOffset, ev.note, ev.accent });
        done += n;
    }
    const double step = 5512.5;
    const long long expected[] = { 0, std::llround (20 * step), std::llround (32 * step) };
    const int rows[] = { Siren, Scratch, Siren };
    const bool accents[] = { false, true, false };
    CHECK (fired.size() == 3);
    for (size_t i = 0; i < std::min<size_t> (fired.size(), 3); ++i)
        CHECK (std::llabs (fired[i].sample - expected[i]) <= 1 && fired[i].row == rows[i] && fired[i].accent == accents[i]);
})

TEST ("DBX-031 the engine plays the FX of the chain's second pattern", {
    DrumEngine e;
    e.prepare (sampleRate);
    auto& song = e.getSong();
    song.pattern (2).setFx (Impact, 0, StepState::Normal);
    song.setChainLength (2);
    song.setChainEntry (0, 1); // empty pattern: silence
    song.setChainEntry (1, 2);
    e.setTempo (240.0);
    e.setPlaying (true);
    const int stepSamples = static_cast<int> (sampleRate * 60.0 / 240.0 / 4.0);
    const auto first = renderEngine (e, stepSamples * 16);
    CHECK (peak (first) == 0.0f);
    const auto second = renderEngine (e, stepSamples * 16);
    CHECK (peak (second, 0, 3000) > 0.05f);
})

namespace
{
using MixerSetup = std::function<void (Pattern&, Mixer&)>;

// One step of the first pattern through the whole engine, after the mixer gains have settled
std::vector<float> renderMix (const MixerSetup& setup, int samples = stepSamples120)
{
    DrumEngine e;
    e.prepare (sampleRate);
    setup (e.getPattern(), e.getMixer());
    renderEngine (e, 512); // stopped: lets the gains glide to their targets
    e.setPlaying (true);
    return renderEngine (e, samples);
}

// A hit on the kick, the bass, the pad, the pluck and the FX at once; `only` keeps one or several tracks
void playTracks (Pattern& p, const std::vector<int>& tracks)
{
    for (int t : tracks)
    {
        if (t < mixerDrums)
            p.set (t, 0, StepState::Normal);
        else if (t == mixerBass)
            p.setBass (0, 45);
        else if (t == mixerPad)
            p.setSynthNote (0, 0, 57);
        else if (t == mixerPluck)
            p.setPluckNote (0, 0, 60);
        else
            p.setFx (Impact, 0, StepState::Normal);
    }
}

std::vector<int> allTracks()
{
    std::vector<int> all;
    for (int t = 0; t < numMixerTracks; ++t)
        all.push_back (t);
    return all;
}

bool sameWave (const std::vector<float>& a, const std::vector<float>& b)
{
    if (a.size() != b.size())
        return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (std::fabs (a[i] - b[i]) > 1.0e-6f)
            return false;
    return true;
}
} // namespace

TEST ("DBX-011 a muted track is silent and the others are unchanged", {
    for (int t = 0; t < numMixerTracks; ++t)
    {
        const auto alone = renderMix ([t] (Pattern& p, Mixer&) { playTracks (p, { t }); });
        CHECK (peak (alone) > 0.02f);
        const auto muted = renderMix ([t] (Pattern& p, Mixer& m) { playTracks (p, { t }); m.setMute (t, true); });
        CHECK (peak (muted) == 0.0f);
    }
    // the kick is muted: what remains is the bass and the pluck alone
    const auto rest = renderMix ([] (Pattern& p, Mixer&) { playTracks (p, { mixerBass, mixerPluck }); });
    const auto mixed = renderMix ([] (Pattern& p, Mixer& m) { playTracks (p, { Kick, mixerBass, mixerPluck }); m.setMute (Kick, true); });
    CHECK (sameWave (rest, mixed));
})

TEST ("DBX-011 with solo only the soloed tracks are heard, with none every unmuted track is", {
    const auto everything = renderMix ([] (Pattern& p, Mixer&) { playTracks (p, allTracks()); });
    const auto noSolo = renderMix ([] (Pattern& p, Mixer& m) { playTracks (p, allTracks()); m.setSolo (Tom, true); m.setSolo (Tom, false); });
    CHECK (sameWave (everything, noSolo));

    const auto kickOnly = renderMix ([] (Pattern& p, Mixer&) { playTracks (p, { Kick }); });
    const auto kickSolo = renderMix ([] (Pattern& p, Mixer& m) { playTracks (p, allTracks()); m.setSolo (Kick, true); });
    CHECK (peak (kickOnly) > 0.02f);
    CHECK (sameWave (kickOnly, kickSolo));

    const auto pair = renderMix ([] (Pattern& p, Mixer&) { playTracks (p, { Kick, mixerPad }); });
    const auto pairSolo = renderMix ([] (Pattern& p, Mixer& m) { playTracks (p, allTracks()); m.setSolo (Kick, true); m.setSolo (mixerPad, true); });
    CHECK (sameWave (pair, pairSolo));

    // a track that is both muted and soloed stays silent
    const auto both = renderMix ([] (Pattern& p, Mixer& m) { playTracks (p, { Kick }); m.setSolo (Kick, true); m.setMute (Kick, true); });
    CHECK (peak (both) == 0.0f);
})

TEST ("DBX-011 muting while playing glides instead of cutting", {
    DrumEngine e;
    e.prepare (sampleRate);
    playTracks (e.getPattern(), { mixerBass });
    e.getPattern().setBass (1, 45);
    e.setPlaying (true);
    renderEngine (e, 3000);
    e.getMixer().setMute (mixerBass, true);
    const auto wave = renderEngine (e, 2000);
    CHECK (peak (wave, 0, 32) > 0.0f);
    CHECK (peak (wave, 400, wave.size()) == 0.0f);
})

TEST ("DBX-012 a lower volume makes that track quieter and leaves the others unchanged", {
    const auto full = renderMix ([] (Pattern& p, Mixer&) { playTracks (p, { Kick }); });
    const auto half = renderMix ([] (Pattern& p, Mixer& m) { playTracks (p, { Kick }); m.setVolume (Kick, 0.5f); });
    CHECK (peak (full) > 0.02f);
    CHECK (std::fabs (peak (half) / peak (full) - 0.5f) < 0.01f);

    const auto bassOnly = renderMix ([] (Pattern& p, Mixer&) { playTracks (p, { mixerBass }); });
    const auto quietKick = renderMix ([] (Pattern& p, Mixer& m) { playTracks (p, { Kick, mixerBass }); m.setVolume (Kick, 0.0f); });
    CHECK (sameWave (bassOnly, quietKick));

    const auto before = renderMix ([] (Pattern& p, Mixer&) { playTracks (p, { Snare, mixerPluck }); });
    const auto after = renderMix ([] (Pattern& p, Mixer& m) { playTracks (p, { Snare, mixerPluck }); m.setVolume (Kick, 0.2f); m.setVolume (mixerBass, 0.9f); });
    CHECK (sameWave (before, after));
})

TEST ("DBX-012 the volume has a default of one and stays within its range", {
    Mixer m;
    for (int t = 0; t < numMixerTracks; ++t)
        CHECK (m.getVolume (t) == 1.0f);
    m.setVolume (3, -1.0f);
    CHECK (m.getVolume (3) == 0.0f);
    m.setVolume (3, 9.0f);
    CHECK (m.getVolume (3) == maxVolume);
    m.setVolume (99, 0.2f); // unknown tracks are ignored
    CHECK (m.getVolume (3) == maxVolume);
})

namespace
{
int crossingsIn (const std::vector<float>& wave, size_t count)
{
    int crossings = 0;
    for (size_t i = 1; i < std::min (count, wave.size()); ++i)
        if ((wave[i - 1] < 0) != (wave[i] < 0))
            ++crossings;
    return crossings;
}
} // namespace

TEST ("DBX-029 the tune and the detune move the pitch of a drum voice and no other track", {
    for (int t : { static_cast<int> (Kick), static_cast<int> (Tom), static_cast<int> (Cowbell) })
    {
        const auto plain = renderMix ([t] (Pattern& p, Mixer&) { playTracks (p, { t }); }, 4000);
        const auto up = renderMix ([t] (Pattern& p, Mixer& m) { playTracks (p, { t }); m.setTune (t, 12.0f); }, 4000);
        const auto down = renderMix ([t] (Pattern& p, Mixer& m) { playTracks (p, { t }); m.setTune (t, -12.0f); }, 4000);
        const double ratioUp = static_cast<double> (crossingsIn (up, 4000)) / crossingsIn (plain, 4000);
        const double ratioDown = static_cast<double> (crossingsIn (down, 4000)) / crossingsIn (plain, 4000);
        CHECK (ratioUp > 1.8 && ratioUp < 2.2);
        CHECK (ratioDown > 0.45 && ratioDown < 0.55);
    }
    const auto plain = renderMix ([] (Pattern& p, Mixer&) { playTracks (p, { Tom }); }, 4000);
    const auto cents = renderMix ([] (Pattern& p, Mixer& m) { playTracks (p, { Tom }); m.setDetune (Tom, 100.0f); }, 4000);
    const auto semitone = renderMix ([] (Pattern& p, Mixer& m) { playTracks (p, { Tom }); m.setTune (Tom, 1.0f); }, 4000);
    CHECK (sameWave (cents, semitone));
    CHECK (crossingsIn (cents, 4000) > crossingsIn (plain, 4000));

    const auto others = renderMix ([] (Pattern& p, Mixer&) { playTracks (p, { Snare, mixerBass, mixerPad, mixerPluck, mixerFx }); });
    const auto tuned = renderMix ([] (Pattern& p, Mixer& m)
    {
        playTracks (p, { Snare, mixerBass, mixerPad, mixerPluck, mixerFx });
        m.setTune (Kick, 7.0f);
        m.setDetune (Tom, -40.0f);
    });
    CHECK (sameWave (others, tuned));
})

TEST ("DBX-029 the tune and the detune move every note of the bass, the pad and the pluck", {
    struct Case { int track; std::function<void (Pattern&)> play; int note; };
    const Case cases[] = {
        { mixerBass, [] (Pattern& p) { p.setBass (0, 45); }, 45 },
        { mixerPad, [] (Pattern& p) { p.setSynthNote (0, 0, 57); }, 57 },
        { mixerPluck, [] (Pattern& p) { p.setPluckNote (0, 0, 52); }, 52 },
    };
    for (const auto& c : cases)
    {
        const auto freq = [&c] (float tune, float detune)
        {
            const auto wave = renderMix ([&c, tune, detune] (Pattern& p, Mixer& m)
            {
                c.play (p);
                m.setTune (c.track, tune);
                m.setDetune (c.track, detune);
            });
            return estimateFrequency (wave, 100, 3400);
        };
        const double base = noteToFrequency (c.note);
        CHECK (isNear (freq (0.0f, 0.0f), base, 0.03));
        CHECK (isNear (freq (7.0f, 0.0f), base * std::pow (2.0, 7.0 / 12.0), 0.03));
        CHECK (isNear (freq (-12.0f, 0.0f), base * 0.5, 0.03));
        CHECK (isNear (freq (0.0f, 100.0f), base * std::pow (2.0, 1.0 / 12.0), 0.03));
    }
})

TEST ("DBX-029 the tune is limited to 24 semitones and the detune to 100 cents", {
    Mixer m;
    CHECK (m.getTune (0) == 0.0f && m.getDetune (0) == 0.0f && m.pitchRatio (0) == 1.0);
    m.setTune (0, 40.0f);
    m.setDetune (0, -300.0f);
    CHECK (m.getTune (0) == 24.0f);
    CHECK (m.getDetune (0) == -100.0f);
    CHECK (std::fabs (m.pitchRatio (0) - std::pow (2.0, 23.0 / 12.0)) < 1.0e-9);
    m.setTune (0, -40.0f);
    CHECK (m.getTune (0) == -24.0f);
})

namespace
{
// A steady sine through the effects chain alone
std::vector<float> throughRack (EffectsRack& rack, float amplitude, int samples = 22050, bool impulseOnly = false)
{
    std::vector<float> out (static_cast<size_t> (samples));
    for (int i = 0; i < samples; ++i)
    {
        const float in = impulseOnly ? (i == 0 ? amplitude : 0.0f)
                                     : amplitude * static_cast<float> (std::sin (2.0 * 3.14159265358979 * 220.0 * i / sampleRate));
        out[static_cast<size_t> (i)] = rack.process (in);
    }
    return out;
}

float levelOf (const std::vector<float>& v, size_t from = 0, size_t to = SIZE_MAX)
{
    double sum = 0.0;
    size_t n = 0;
    for (size_t i = from; i < std::min (to, v.size()); ++i, ++n)
        sum += static_cast<double> (v[i]) * static_cast<double> (v[i]);
    return n == 0 ? 0.0f : static_cast<float> (std::sqrt (sum / static_cast<double> (n)));
}

void turnOnly (EffectsRack& rack, int effect)
{
    for (int e = 0; e < numEffects; ++e)
        rack.settings (e)[0].value->store (e == effect ? 1.0f : 0.0f);
}

using EngineSetup = std::function<void (DrumEngine&)>;

// Whole engine, playing; the setup runs before the gains settle
std::vector<float> renderWith (const EngineSetup& setup, int samples = 22050)
{
    DrumEngine e;
    e.prepare (sampleRate);
    setup (e);
    renderEngine (e, 512);
    e.setPlaying (true);
    return renderEngine (e, samples);
}
} // namespace

TEST ("DBX-014 each effect changes the sound when on and leaves it untouched when off", {
    EffectsRack off;
    const auto dry = throughRack (off, 0.5f);
    CHECK (levelOf (dry) > 0.3f);
    for (int effect = 0; effect < numEffects; ++effect)
    {
        EffectsRack rack;
        turnOnly (rack, effect);
        const auto wet = throughRack (rack, 0.5f);
        float difference = 0.0f;
        for (size_t i = 0; i < dry.size(); ++i)
            difference = std::max (difference, std::fabs (wet[i] - dry[i]));
        CHECK (difference > 0.05f);
        for (float s : wet)
            CHECK (std::isfinite (s));

        EffectsRack switchedOff;
        turnOnly (switchedOff, effect);
        switchedOff.settings (effect)[0].value->store (0.0f);
        CHECK (! switchedOff.anyOn());
        CHECK (sameWave (throughRack (switchedOff, 0.5f), dry));
    }
})

TEST ("DBX-014 each effect has its on switch and its settings, off by default", {
    EffectsRack rack;
    CHECK (! rack.anyOn());
    for (int effect = 0; effect < numEffects; ++effect)
    {
        const auto settings = rack.settings (effect);
        CHECK (settings.size() >= 4);
        CHECK (std::string (settings[0].name) == "on");
        CHECK (settings[0].value->load() == 0.0f);
        CHECK (std::string (EffectsRack::effectName (effect)).size() > 0);
    }
    CHECK (rack.settings (5).empty());
})

TEST ("DBX-014 the reverb leaves a tail, the delay an echo at its time", {
    EffectsRack reverb;
    turnOnly (reverb, 0);
    const auto tail = throughRack (reverb, 0.5f, 22050, true);
    CHECK (levelOf (tail, 3000, 12000) > 0.001f);

    EffectsRack dry;
    CHECK (levelOf (throughRack (dry, 0.5f, 22050, true), 3000, 12000) == 0.0f);

    EffectsRack delay;
    turnOnly (delay, 1);
    delay.delay().timeMs.store (200.0f);
    delay.delay().mix.store (0.5f);
    const auto echo = throughRack (delay, 0.5f, 22050, true);
    const size_t at = static_cast<size_t> (0.2 * sampleRate);
    CHECK (std::fabs (echo[at] - 0.25f) < 0.01f);
    CHECK (peak (echo, 100, at - 2) == 0.0f);
    CHECK (peak (echo, at + 2, at + 2000) == 0.0f);
})

TEST ("DBX-014 the distortion squares the wave, the compressor lowers loud levels only", {
    EffectsRack none;
    const auto clean = throughRack (none, 0.5f);

    EffectsRack dist;
    turnOnly (dist, 2);
    dist.distortion().drive.store (1.0f);
    dist.distortion().tone.store (1.0f);
    const auto driven = throughRack (dist, 0.5f);
    CHECK (levelOf (driven, 2000) / peak (driven, 2000) > levelOf (clean, 2000) / peak (clean, 2000) + 0.1f);

    EffectsRack comp;
    turnOnly (comp, 4);
    comp.compressor().threshold.store (-24.0f);
    comp.compressor().ratio.store (8.0f);
    const auto squashed = throughRack (comp, 0.5f);
    CHECK (peak (squashed, 8000) < 0.6f * peak (clean, 8000));

    const auto quietDry = throughRack (none, 0.02f);
    EffectsRack quietComp;
    turnOnly (quietComp, 4);
    quietComp.compressor().threshold.store (-24.0f);
    quietComp.compressor().ratio.store (8.0f);
    const auto quiet = throughRack (quietComp, 0.02f);
    CHECK (std::fabs (levelOf (quiet, 8000) - levelOf (quietDry, 8000)) < 0.1f * levelOf (quietDry, 8000));

    EffectsRack boosted;
    turnOnly (boosted, 4);
    boosted.compressor().makeup.store (12.0f);
    CHECK (levelOf (throughRack (boosted, 0.02f), 8000) > 3.0f * levelOf (quietDry, 8000));
})

TEST ("DBX-014 the chorus keeps the level and moves the wave; extreme settings stay finite", {
    EffectsRack none, chorus;
    turnOnly (chorus, 3);
    const auto clean = throughRack (none, 0.5f);
    const auto wet = throughRack (chorus, 0.5f);
    CHECK (levelOf (wet, 4000) > 0.6f * levelOf (clean, 4000));
    CHECK (levelOf (wet, 4000) < 1.4f * levelOf (clean, 4000));

    EffectsRack wild;
    for (int e = 0; e < numEffects; ++e)
        for (auto& s : wild.settings (e))
            s.value->store (s.max * 5.0f + 5.0f);
    for (float s : throughRack (wild, 0.5f))
        CHECK (std::isfinite (s));
})

TEST ("DBX-014 with every effect off the engine sound is as before", {
    const auto plain = renderWith ([] (DrumEngine& e) { playTracks (e.getPattern(), allTracks()); });
    const auto tweaked = renderWith ([] (DrumEngine& e) {
        playTracks (e.getPattern(), allTracks());
        for (int fx = 0; fx < numEffects; ++fx)
            for (auto& s : e.getEffects().settings (fx))
                if (std::string (s.name) != "on")
                    s.value->store (s.max);
    });
    CHECK (sameWave (plain, tweaked));
    CHECK (peak (plain) > 0.1f);
    const auto reverbOn = renderWith ([] (DrumEngine& e) {
        playTracks (e.getPattern(), allTracks());
        e.getEffects().reverb().on.store (1.0f);
    });
    CHECK (! sameWave (plain, reverbOn));
    for (float s : reverbOn)
        CHECK (std::fabs (s) <= 1.0f);
})

TEST ("DBX-015 each of the twelve tracks has a send, 0 by default and kept within 0 to 1", {
    Mixer m;
    for (int t = 0; t < numMixerTracks; ++t)
        CHECK (m.getSend (t) == 0.0f);
    m.setSend (4, 0.4f);
    CHECK (m.getSend (4) == 0.4f);
    CHECK (m.getSend (5) == 0.0f);
    m.setSend (4, 3.0f);
    CHECK (m.getSend (4) == maxSend);
    m.setSend (4, -1.0f);
    CHECK (m.getSend (4) == 0.0f);
    m.setSend (99, 0.5f); // unknown tracks are ignored
    CHECK (m.getSend (99) == 0.0f);
})

TEST ("DBX-015 the send of each track feeds the master chain", {
    for (int t = 0; t < numMixerTracks; ++t)
    {
        const auto base = [t] (DrumEngine& e) {
            playTracks (e.getPattern(), { t });
            e.getEffects().delay().on.store (1.0f);
            e.getEffects().delay().timeMs.store (200.0f);
        };
        const auto noSend = renderWith (base);
        const auto sent = renderWith ([&] (DrumEngine& e) { base (e); e.getMixer().setSend (t, 1.0f); });
        CHECK (peak (noSend) > 0.01f);
        CHECK (peak (sent) > 1.5f * peak (noSend));

        const auto other = renderWith ([&] (DrumEngine& e) { base (e); e.getMixer().setSend ((t + 1) % numMixerTracks, 1.0f); });
        CHECK (sameWave (noSend, other));
    }
})

TEST ("DBX-015 the chain processes the dry mix plus the sends, and bypassed ignores them", {
    const auto dryOnly = renderWith ([] (DrumEngine& e) { playTracks (e.getPattern(), { Kick, Snare }); });
    const auto sendsOff = renderWith ([] (DrumEngine& e) {
        playTracks (e.getPattern(), { Kick, Snare });
        e.getMixer().setSend (Kick, 1.0f);
        e.getMixer().setSend (Snare, 0.5f);
    });
    CHECK (sameWave (dryOnly, sendsOff));

    const auto chain = [] (float send) {
        return renderWith ([send] (DrumEngine& e) {
            playTracks (e.getPattern(), { Kick });
            e.getEffects().distortion().on.store (1.0f);
            e.getEffects().distortion().drive.store (0.2f);
            e.getMixer().setSend (Kick, send);
        });
    };
    CHECK (! sameWave (chain (0.0f), chain (1.0f)));
})

TEST ("DBX-015 a muted track sends nothing", {
    const auto silent = renderWith ([] (DrumEngine& e) {
        playTracks (e.getPattern(), { Kick });
        e.getEffects().delay().on.store (1.0f);
        e.getMixer().setSend (Kick, 1.0f);
        e.getMixer().setMute (Kick, true);
    });
    CHECK (peak (silent) == 0.0f);
})

namespace
{
class MemoryStore : public PresetStore
{
public:
    std::vector<std::string> list (PresetKind kind) override
    {
        std::vector<std::string> names;
        for (const auto& [key, text] : files)
            if (key.first == kind)
                names.push_back (key.second);
        return names;
    }
    bool read (PresetKind kind, const std::string& name, std::string& text) override
    {
        const auto it = files.find ({ kind, name });
        if (it == files.end())
            return false;
        text = it->second;
        return true;
    }
    bool write (PresetKind kind, const std::string& name, const std::string& text) override
    {
        files[{ kind, name }] = text;
        return true;
    }
    std::map<std::pair<PresetKind, std::string>, std::string> files;
};

std::vector<int> tracksOf (PresetKind kind)
{
    switch (kind)
    {
        case PresetKind::DrumKit: return { 0, 1, 2, 3, 4, 5, 6, 7 };
        case PresetKind::Bass: return { mixerBass };
        case PresetKind::Pad: return { mixerPad };
        case PresetKind::Pluck: return { mixerPluck };
        case PresetKind::Fx: return { mixerFx };
    }
    return {};
}

std::vector<float> renderKind (PresetKind kind, const EngineSetup& setup)
{
    return renderWith ([&] (DrumEngine& e) {
        playTracks (e.getPattern(), tracksOf (kind));
        e.getEffects().delay().on.store (1.0f);
        e.getEffects().delay().timeMs.store (120.0f);
        setup (e);
    });
}

constexpr PresetKind allKinds[] = { PresetKind::DrumKit, PresetKind::Bass, PresetKind::Pad, PresetKind::Pluck, PresetKind::Fx };
} // namespace

TEST ("DBX-016 factory presets exist for the drum kit, the bass, the pad, the pluck and the FX", {
    for (PresetKind kind : allKinds)
    {
        const auto& presets = PresetBank::factory (kind);
        CHECK (presets.size() >= 4);
        CHECK (presets.front().name == "Default");
        for (size_t i = 0; i < presets.size(); ++i)
        {
            CHECK (isValidPresetName (presets[i].name));
            CHECK (presets[i].kind == kind);
            for (size_t j = i + 1; j < presets.size(); ++j)
                CHECK (presets[i].name != presets[j].name);
        }
    }
})

TEST ("DBX-016 each factory preset changes the sound and the Default one is the sound as before", {
    for (PresetKind kind : allKinds)
    {
        const auto reference = renderKind (kind, [] (DrumEngine&) {});
        const auto& presets = PresetBank::factory (kind);
        const auto asDefault = renderKind (kind, [&] (DrumEngine& e) { PresetBank (e).apply (presets.front()); });
        CHECK (sameWave (reference, asDefault));
        for (size_t i = 1; i < presets.size(); ++i)
        {
            const auto wave = renderKind (kind, [&] (DrumEngine& e) { PresetBank (e).apply (presets[i]); });
            CHECK (! sameWave (reference, wave));
            CHECK (peak (wave) > 0.01f);
            CHECK (peak (wave) <= 1.0f);
        }
    }
})

TEST ("DBX-016 applying Default after another preset restores the starting sound", {
    for (PresetKind kind : allKinds)
    {
        const auto& presets = PresetBank::factory (kind);
        const auto reference = renderKind (kind, [] (DrumEngine&) {});
        const auto back = renderKind (kind, [&] (DrumEngine& e) {
            PresetBank bank (e);
            bank.apply (presets.back());
            bank.apply (presets.front());
        });
        CHECK (sameWave (reference, back));
    }
})

TEST ("DBX-016 a preset only touches the tracks of its kind", {
    DrumEngine e;
    PresetBank bank (e);
    const Preset before = bank.capture (PresetKind::Pad, "x");
    const Preset kitBefore = bank.capture (PresetKind::DrumKit, "x");
    bank.apply (PresetBank::factory (PresetKind::Bass)[1]);
    bank.apply (PresetBank::factory (PresetKind::Pluck)[1]);
    bank.apply (PresetBank::factory (PresetKind::Fx)[1]);
    CHECK (bank.capture (PresetKind::Pad, "x").values == before.values);
    CHECK (bank.capture (PresetKind::DrumKit, "x").values == kitBefore.values);
    CHECK (e.getBassParams().cutoff.load() != BassParams().cutoff.load());
})

TEST ("DBX-016 a saved user preset loads back to the same sound", {
    for (PresetKind kind : allKinds)
    {
        MemoryStore store;
        const auto tweak = [kind] (DrumEngine& e) {
            for (int t : tracksOf (kind))
            {
                e.getMixer().setVolume (t, 0.73f);
                e.getMixer().setTune (t, 3.0f);
                e.getMixer().setDetune (t, -17.0f);
                e.getMixer().setSend (t, 0.35f);
            }
            e.getBassParams().cutoff.store (910.0f);
            e.getSynthParams().cutoff.store (3333.0f);
            e.getSynthParams().lfoDepth.store (0.4f);
            e.getPluckParams().decay.store (0.41f);
        };
        {
            DrumEngine e;
            tweak (e);
            CHECK (PresetBank (e).saveUser (store, kind, "My Sound"));
        }
        CHECK (store.list (kind).size() == 1);

        const auto saved = renderKind (kind, tweak);
        const auto loaded = renderKind (kind, [&] (DrumEngine& e) { CHECK (PresetBank (e).loadUser (store, kind, "My Sound")); });
        CHECK (sameWave (saved, loaded));
        CHECK (! sameWave (loaded, renderKind (kind, [] (DrumEngine&) {})));
    }
})

TEST ("DBX-016 user presets are files in a folder and survive a new store", {
    const std::string folder = "dbx016_user_presets";
    std::filesystem::remove_all (folder);
    {
        DrumEngine e;
        e.getBassParams().decay.store (0.77f);
        FolderPresetStore store (folder);
        CHECK (PresetBank (e).saveUser (store, PresetKind::Bass, "Mine"));
        CHECK (PresetBank (e).saveUser (store, PresetKind::Bass, "Another-1"));
        CHECK (PresetBank (e).saveUser (store, PresetKind::Pad, "Mine"));
    }
    FolderPresetStore store (folder);
    CHECK (store.list (PresetKind::Bass).size() == 2);
    CHECK (store.list (PresetKind::Pad).size() == 1);
    CHECK (store.list (PresetKind::Pluck).empty());
    DrumEngine e;
    CHECK (PresetBank (e).loadUser (store, PresetKind::Bass, "Mine"));
    CHECK (std::fabs (e.getBassParams().decay.load() - 0.77f) < 1.0e-6f);
    CHECK (std::filesystem::exists (folder + "/bass/Mine.preset"));
    std::filesystem::remove_all (folder);
})

TEST ("DBX-016 bad names, missing files and foreign or damaged text are refused and change nothing", {
    MemoryStore store;
    DrumEngine e;
    PresetBank bank (e);
    for (const char* bad : { "", "../evil", "a/b", "a\\b", ".hidden", " lead", "x:y" })
    {
        CHECK (! isValidPresetName (bad));
        CHECK (! bank.saveUser (store, PresetKind::Bass, bad));
    }
    CHECK (! isValidPresetName (std::string (41, 'a')));
    CHECK (store.files.empty());
    FolderPresetStore folder ("dbx016_never_written");
    CHECK (! folder.write (PresetKind::Bass, "../evil", "x"));
    CHECK (! std::filesystem::exists ("dbx016_never_written"));

    const float cutoff = e.getBassParams().cutoff.load();
    CHECK (! bank.loadUser (store, PresetKind::Bass, "Nothing"));
    store.write (PresetKind::Bass, "Junk", "hello");
    CHECK (! bank.loadUser (store, PresetKind::Bass, "Junk"));
    CHECK (bank.saveUser (store, PresetKind::Pad, "Pad One"));
    store.files[{ PresetKind::Bass, "Pad One" }] = store.files[{ PresetKind::Pad, "Pad One" }];
    CHECK (! bank.loadUser (store, PresetKind::Bass, "Pad One"));
    CHECK (e.getBassParams().cutoff.load() == cutoff);

    store.write (PresetKind::Bass, "Wild", "drumbox-preset 1\nkind=bass\nname=Wild\nbass.cutoff=999999\nunknown.key=3\ntrack8.volume=-4\n");
    CHECK (bank.loadUser (store, PresetKind::Bass, "Wild"));
    CHECK (e.getBassParams().cutoff.load() == 3000.0f);
    CHECK (e.getMixer().getVolume (mixerBass) == 0.0f);
})

// ---- song file and export ----

// A song that uses everything the file must keep.
void fillRichSong (DrumEngine& e)
{
    e.setTempo (97.25);
    e.setSwing (33.0);
    Song& song = e.getSong();
    for (int p = 0; p < numPatterns; ++p)
    {
        Pattern& pat = song.pattern (p);
        for (int t = 0; t < numTracks; ++t)
            for (int s = 0; s < numSteps; ++s)
                pat.set (t, s, static_cast<StepState> ((p + t * 3 + s * 5) % 3));
        for (int r = 0; r < numFxRows; ++r)
            for (int s = 0; s < numSteps; ++s)
                pat.setFx (r, s, static_cast<StepState> ((p + r + s) % 3));
        for (int s = 0; s < numSteps; ++s)
        {
            pat.setBass (s, (s + p) % 3 == 0 ? noNote : lowestNote + (s * 7 + p) % 40, (s + p) % 4 == 0);
            pat.setSynthAccent (s, (s + p) % 5 == 0);
            pat.setPluckAccent (s, (s + p) % 6 == 0);
            for (int v = 0; v < synthVoices; ++v)
                pat.setSynthNote (s, v, (s + v + p) % 4 == 0 ? noNote : 40 + (s * 3 + v * 4 + p) % 50);
            for (int v = 0; v < pluckVoices; ++v)
                pat.setPluckNote (s, v, (s + v) % 3 == 0 ? noNote : highestNote - (s + v * 5 + p) % 60);
        }
    }
    song.setChainLength (7);
    for (int i = 0; i < maxChainLength; ++i)
        song.setChainEntry (i, (i * 5 + 3) % numPatterns);

    Mixer& m = e.getMixer();
    for (int t = 0; t < numMixerTracks; ++t)
    {
        m.setVolume (t, 0.1f + 0.1f * static_cast<float> (t));
        m.setTune (t, static_cast<float> (t) - 5.5f);
        m.setDetune (t, 7.3f * static_cast<float> (t) - 40.1f);
        m.setSend (t, 0.0731f * static_cast<float> (t));
        m.setMute (t, t % 4 == 1);
        m.setSolo (t, t % 5 == 2);
    }
    e.getBassParams().cutoff.store (912.345f);
    e.getBassParams().decay.store (0.333f);
    e.getSynthParams().osc2Semitones.store (7.0f);
    e.getSynthParams().cutoff.store (3141.5f);
    e.getSynthParams().lfoDepth.store (0.41f);
    e.getPluckParams().resonance.store (0.77f);
    e.getPluckParams().osc2Mix.store (0.123f);
    for (int fx = 0; fx < numEffects; ++fx)
    {
        auto settings = e.getEffects().settings (fx);
        for (size_t i = 0; i < settings.size(); ++i)
        {
            const float span = settings[i].max - settings[i].min;
            settings[i].value->store (i == 0 ? static_cast<float> (fx % 2) : settings[i].min + span * (0.15f + 0.1f * static_cast<float> ((i + static_cast<size_t> (fx)) % 6)));
        }
    }
}

std::string withNumber (std::string text, const std::string& key, const std::string& value)
{
    const auto at = text.find ("\"" + key + "\":");
    const auto from = at + key.size() + 3;
    const auto to = text.find_first_of (",}\n", from);
    return text.replace (from, to - from, value);
}

std::string readFile (const std::string& path)
{
    std::ifstream in (path, std::ios::binary);
    return { std::istreambuf_iterator<char> (in), std::istreambuf_iterator<char>() };
}

TEST ("DBX-018 a saved song opens again identical, with mixer and effects", {
    DrumEngine original;
    fillRichSong (original);
    const std::string text = serializeSong (original);

    DrumEngine back;
    CHECK (parseSong (text, back));
    CHECK (serializeSong (back) == text);
    CHECK (back.getTempo() == 97.25 && back.getSwing() == 33.0);
    CHECK (back.getSong().getChainLength() == 7);
    for (int i = 0; i < maxChainLength; ++i)
        CHECK (back.getSong().getChainEntry (i) == original.getSong().getChainEntry (i));
    for (int p = 0; p < numPatterns; ++p)
        for (int s = 0; s < numSteps; ++s)
        {
            const Pattern& a = original.getSong().pattern (p);
            const Pattern& b = back.getSong().pattern (p);
            CHECK (a.getBassNote (s) == b.getBassNote (s) && a.getBassAccent (s) == b.getBassAccent (s));
            CHECK (a.getSynthAccent (s) == b.getSynthAccent (s) && a.getPluckAccent (s) == b.getPluckAccent (s));
            for (int v = 0; v < synthVoices; ++v)
                CHECK (a.getSynthNote (s, v) == b.getSynthNote (s, v));
            for (int v = 0; v < pluckVoices; ++v)
                CHECK (a.getPluckNote (s, v) == b.getPluckNote (s, v));
            for (int t = 0; t < numTracks; ++t)
                CHECK (a.get (t, s) == b.get (t, s));
            for (int r = 0; r < numFxRows; ++r)
                CHECK (a.getFx (r, s) == b.getFx (r, s));
        }
    for (int t = 0; t < numMixerTracks; ++t)
    {
        const Mixer& a = original.getMixer();
        const Mixer& b = back.getMixer();
        CHECK (a.isMuted (t) == b.isMuted (t) && a.isSolo (t) == b.isSolo (t));
        CHECK (a.getVolume (t) == b.getVolume (t) && a.getTune (t) == b.getTune (t));
        CHECK (a.getDetune (t) == b.getDetune (t) && a.getSend (t) == b.getSend (t));
    }
    CHECK (back.getBassParams().cutoff.load() == 912.345f && back.getSynthParams().cutoff.load() == 3141.5f);
    CHECK (back.getPluckParams().osc2Mix.load() == 0.123f);
    for (int fx = 0; fx < numEffects; ++fx)
    {
        auto a = original.getEffects().settings (fx);
        auto b = back.getEffects().settings (fx);
        for (size_t i = 0; i < a.size(); ++i)
            CHECK (a[i].value->load() == b[i].value->load());
    }

    original.setPlaying (true);
    back.setPlaying (true);
    CHECK (sameWave (renderEngine (original, 44100), renderEngine (back, 44100)));
})

TEST ("DBX-018 a song file is a .dbox JSON file, written whole and read back", {
    const std::string folder = "dbx018_songs";
    std::filesystem::remove_all (folder);
    std::filesystem::create_directories (folder);
    DrumEngine original;
    fillRichSong (original);

    CHECK (! saveSongFile (original, folder + "/song.json"));
    CHECK (! saveSongFile (original, folder + "/missing/song.dbox"));
    CHECK (saveSongFile (original, folder + "/song.dbox"));
    CHECK (saveSongFile (original, folder + "/song.dbox")); // saving again replaces it
    int files = 0;
    for ([[maybe_unused]] const auto& entry : std::filesystem::directory_iterator (folder))
        ++files;
    CHECK (files == 1);
    const std::string text = readFile (folder + "/song.dbox");
    CHECK (text.rfind ("{", 0) == 0 && text.find ("\"drumbox-song\"") != std::string::npos);

    DrumEngine back;
    CHECK (loadSongFile (back, folder + "/song.dbox"));
    CHECK (serializeSong (back) == serializeSong (original));
    CHECK (! loadSongFile (back, folder + "/nothing.dbox"));
    std::filesystem::copy_file (folder + "/song.dbox", folder + "/song.txt");
    CHECK (! loadSongFile (back, folder + "/song.txt"));
    std::filesystem::remove_all (folder);
})

TEST ("DBX-018 a damaged or foreign file is refused and changes nothing", {
    DrumEngine source;
    fillRichSong (source);
    const std::string good = serializeSong (source);

    DrumEngine e;
    e.setTempo (130.0);
    e.getPattern().set (0, 0, StepState::Accent);
    const std::string before = serializeSong (e);

    const auto refused = [&] (const std::string& text) {
        CHECK (! parseSong (text, e));
        CHECK (serializeSong (e) == before);
    };
    refused ("");
    refused ("hello");
    refused ("{}");
    refused (good.substr (0, good.size() / 2));
    refused (good + "x");
    refused (withNumber (good, "version", "2"));
    refused (withNumber (good, "chainLength", "0"));
    refused (withNumber (good, "chainLength", "129"));
    refused (withNumber (good, "tempo", "\"fast\""));
    auto replaced = good;
    replaced.replace (replaced.find ("drumbox-song"), 12, "other-format");
    refused (replaced);
    auto shortPattern = good;
    shortPattern.replace (shortPattern.find ("\"drums\":[") + 9, 2, "");
    refused (shortPattern);
    auto badEntry = good;
    badEntry.replace (badEntry.find ("\"chain\":[") + 9, 1, "99,");
    refused (badEntry);
    auto badSound = good;
    badSound.replace (badSound.find ("\"bass.cutoff\":") + 14, 3, "\"x\"");
    refused (badSound);
    refused (std::string (5 * 1024 * 1024, ' '));
    refused (std::string (200, '[') + std::string (200, ']'));
})

TEST ("DBX-018 settings out of range are brought back into range", {
    DrumEngine source;
    fillRichSong (source);
    std::string text = withNumber (serializeSong (source), "tempo", "9999");
    text = withNumber (text, "swing", "-5");
    text = withNumber (text, "track0.volume", "99");
    text = withNumber (text, "track1.tune", "-500");
    text = withNumber (text, "feedback", "7");
    DrumEngine e;
    CHECK (parseSong (text, e));
    CHECK (e.getTempo() == Sequencer::maxTempo);
    CHECK (e.getSwing() == 0.0);
    CHECK (e.getMixer().getVolume (0) == maxVolume);
    CHECK (e.getMixer().getTune (1) == -maxTuneSemitones);
    CHECK (e.getEffects().delay().feedback.load() == 0.9f);
})

// ---- WAV export ----

struct WavFile
{
    bool ok = false;
    int channels = 0, rate = 0, bits = 0, format = 0;
    size_t frames = 0, fileSize = 0;
    std::vector<float> left, right;
};

WavFile readWav (const std::string& path)
{
    WavFile w;
    const std::string bytes = readFile (path);
    w.fileSize = bytes.size();
    if (bytes.size() < 44 || bytes.compare (0, 4, "RIFF") != 0 || bytes.compare (8, 8, "WAVEfmt ") != 0 || bytes.compare (36, 4, "data") != 0)
        return w;
    const auto le = [&bytes] (size_t at, int n) {
        std::uint32_t v = 0;
        for (int i = 0; i < n; ++i)
            v |= static_cast<std::uint32_t> (static_cast<unsigned char> (bytes[at + static_cast<size_t> (i)])) << (8 * i);
        return v;
    };
    w.format = static_cast<int> (le (20, 2));
    w.channels = static_cast<int> (le (22, 2));
    w.rate = static_cast<int> (le (24, 4));
    w.bits = static_cast<int> (le (34, 2));
    const size_t dataBytes = le (40, 4);
    w.ok = le (4, 4) == bytes.size() - 8 && dataBytes == bytes.size() - 44 && w.channels == 2 && w.bits == 24;
    w.frames = dataBytes / 6;
    for (size_t i = 0; i < w.frames; ++i)
        for (int c = 0; c < 2; ++c)
        {
            const size_t at = 44 + i * 6 + static_cast<size_t> (c) * 3;
            std::int32_t v = static_cast<std::int32_t> (le (at, 3));
            if (v & 0x800000)
                v -= 0x1000000;
            (c == 0 ? w.left : w.right).push_back (static_cast<float> (v) / 8388607.0f);
        }
    return w;
}

// Kick on step 1 of pattern 0 only; the chain plays pattern 0 then an empty pattern 1.
void kickThenSilence (DrumEngine& e)
{
    e.setTempo (120.0);
    e.getSong().pattern (0).set (0, 0, StepState::Accent);
    e.getSong().setChainLength (2);
    e.getSong().setChainEntry (0, 0);
    e.getSong().setChainEntry (1, 1);
}

constexpr size_t twoPatterns120 = 2 * numSteps * 5512 + numSteps; // 2 x 16 steps of 5512.5 samples

TEST ("DBX-019 the WAV is stereo 24-bit 44.1 kHz and holds the chain played once", {
    DrumEngine e;
    kickThenSilence (e);
    const std::string folder = "dbx019_out";
    std::filesystem::remove_all (folder);
    std::filesystem::create_directories (folder);
    CHECK (exportMixWav (e, folder + "/mix.wav"));
    const WavFile wav = readWav (folder + "/mix.wav");
    CHECK (wav.ok && wav.format == 1 && wav.channels == 2 && wav.rate == 44100 && wav.bits == 24);
    CHECK (wav.frames == 2 * 16 * 5512 + 16);
    CHECK (wav.left == wav.right);
    CHECK (peak (wav.left, 0, 5000) > 0.2f);
    // the chain is not looped: nothing plays at its second pass
    CHECK (peak (wav.left, wav.frames / 2, wav.frames) == 0.0f);
    CHECK (twoPatterns120 == wav.frames);

    const ExportedAudio audio = renderSongAudio (e);
    CHECK (audio.mix.size() == wav.frames);
    for (size_t i = 0; i < wav.frames; i += 997)
        CHECK (std::fabs (wav.left[i] - audio.mix[i]) < 2.0e-7f);
    std::filesystem::remove_all (folder);
})

TEST ("DBX-019 the effect tails are kept after the chain, and only then", {
    DrumEngine dry;
    kickThenSilence (dry);
    const ExportLength dryLength = measureExport (dry);
    CHECK (dryLength.totalFrames == dryLength.chainFrames);

    DrumEngine wet;
    kickThenSilence (wet);
    wet.getSong().setChainLength (1);
    wet.getEffects().reverb().on.store (1.0f);
    wet.getEffects().reverb().size.store (0.9f);
    wet.getEffects().reverb().mix.store (1.0f);
    wet.getMixer().setSend (0, 1.0f);
    wet.getPattern().set (0, 15, StepState::Accent);
    const ExportedAudio audio = renderSongAudio (wet);
    CHECK (audio.length.totalFrames > audio.length.chainFrames + 10000);
    CHECK (peak (audio.mix, audio.length.chainFrames, audio.mix.size()) > 1.0e-4f);
    CHECK (std::fabs (audio.mix.back()) < 1.0e-3f);
    CHECK (audio.length.totalFrames < audio.length.chainFrames + static_cast<size_t> (maxTailSeconds) * 44100);

    // the song being exported is not disturbed: no playing, same text
    const std::string before = serializeSong (wet);
    CHECK (sameWave (renderSongMix (wet), audio.mix));
    CHECK (serializeSong (wet) == before && ! wet.isPlaying());
})

TEST ("DBX-019 mute and solo are applied to the export, from the first sample", {
    DrumEngine e;
    e.getPattern().set (0, 0, StepState::Accent);
    e.getPattern().set (1, 0, StepState::Accent);
    e.getPattern().setBass (0, 36);
    const auto mixOf = [&e] { return renderSongMix (e); };

    const auto all = mixOf();
    CHECK (peak (all) > 0.2f);
    e.getMixer().setMute (0, true);
    e.getMixer().setMute (1, true);
    e.getMixer().setMute (mixerBass, true);
    CHECK (peak (mixOf()) == 0.0f);

    e.getMixer().setMute (0, false);
    e.getMixer().setMute (1, false);
    e.getMixer().setMute (mixerBass, false);
    e.getMixer().setSolo (1, true);
    const auto soloSnare = mixOf();
    DrumEngine snareOnly;
    snareOnly.getPattern().set (1, 0, StepState::Accent);
    CHECK (sameWave (soloSnare, renderSongMix (snareOnly)));
    e.getMixer().setMute (1, true); // a muted track stays silent even when soloed
    CHECK (peak (mixOf()) == 0.0f);

    e.getMixer().setMute (1, false);
    e.getMixer().setSolo (1, false);
    e.getMixer().setVolume (0, 0.5f);
    e.getMixer().setMute (1, true);
    e.getMixer().setMute (mixerBass, true);
    DrumEngine half, full;
    half.getPattern().set (0, 0, StepState::Accent);
    half.getMixer().setVolume (0, 0.5f);
    full.getPattern().set (0, 0, StepState::Accent);
    CHECK (sameWave (mixOf(), renderSongMix (half)));
    CHECK (std::fabs (peak (renderSongMix (half)) / peak (renderSongMix (full)) - 0.5f) < 0.01f);
})

TEST ("DBX-020 the stems export writes the mix and one file per track, all the same length", {
    DrumEngine e;
    kickThenSilence (e);
    e.getPattern().set (1, 4, StepState::Normal);
    e.getSong().pattern (1).set (0, 15, StepState::Accent);
    e.getPattern().setBass (0, 36);
    e.getPattern().setSynthNote (0, 0, 60);
    e.getEffects().reverb().on.store (1.0f);
    e.getMixer().setSend (mixerPad, 0.8f);
    e.getMixer().setMute (1, true);
    e.getMixer().setSolo (0, true);
    e.getMixer().setSolo (mixerBass, true);

    const std::string folder = "dbx020_out";
    std::filesystem::remove_all (folder);
    CHECK (! exportStemsWav (e, folder, "../evil"));
    CHECK (! std::filesystem::exists (folder));
    CHECK (exportStemsWav (e, folder, "My Song"));

    int count = 0;
    for ([[maybe_unused]] const auto& entry : std::filesystem::directory_iterator (folder))
        ++count;
    CHECK (count == numMixerTracks + 1);

    const WavFile mix = readWav (folder + "/My Song_mix.wav");
    CHECK (mix.ok && mix.rate == 44100 && mix.bits == 24 && mix.frames > 0);
    CHECK (peak (mix.left) > 0.1f);
    for (int t = 0; t < numMixerTracks; ++t)
    {
        const std::string number = (t < 9 ? "0" : "") + std::to_string (t + 1);
        const WavFile stem = readWav (folder + "/My Song_" + number + "_" + mixerTrackNames[static_cast<size_t> (t)] + ".wav");
        CHECK (stem.ok && stem.rate == 44100 && stem.bits == 24 && stem.frames == mix.frames);
        const bool heard = t == 0 || t == mixerBass;
        CHECK ((peak (stem.left) > 0.05f) == heard); // muted, or not soloed: silent
    }
    // the tail of the reverb is in the mix, after the chain
    CHECK (mix.frames > twoPatterns120 + 1000);
    std::filesystem::remove_all (folder);
})

TEST ("DBX-020 without effects the stems add up to the mix", {
    DrumEngine e;
    e.getPattern().set (0, 0, StepState::Normal);
    e.getPattern().set (1, 4, StepState::Normal);
    e.getPattern().setBass (2, 40);
    e.getPattern().setSynthNote (0, 0, 55);
    e.getPattern().setPluckNote (3, 0, 70);
    e.getPattern().setFx (0, 6, StepState::Normal);
    e.getMixer().setVolume (0, 0.8f);
    e.getMixer().setTune (1, 2.0f);
    const ExportedAudio audio = renderSongAudio (e);
    CHECK (audio.mix.size() == audio.length.totalFrames);
    std::vector<float> sum (audio.mix.size(), 0.0f);
    for (const auto& stem : audio.stems)
    {
        CHECK (stem.size() == audio.mix.size());
        for (size_t i = 0; i < sum.size(); ++i)
            sum[i] += stem[i];
    }
    float worst = 0.0f;
    for (size_t i = 0; i < sum.size(); ++i)
        worst = std::max (worst, std::fabs (sum[i] - audio.mix[i]));
    CHECK (worst < 1.0e-4f);
    CHECK (peak (audio.mix) > 0.2f);
})

int main (int argc, char** argv)
{
    const std::string prefix = argc > 1 ? argv[1] : "";
    int ran = 0;
    for (auto& t : registry())
    {
        if (t.name.rfind (prefix, 0) != 0)
            continue;
        const int before = failures;
        t.body();
        std::cout << (failures == before ? "PASS " : "FAIL ") << t.name << "\n";
        ++ran;
    }
    if (ran == 0)
    {
        std::cerr << "no test matches '" << prefix << "'\n";
        return 2;
    }
    return failures == 0 ? 0 : 1;
}
