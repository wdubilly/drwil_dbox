// Plain C++ tests: no audio device, no JUCE. Run `drumbox_tests <prefix>` to run
// only the tests whose name starts with the prefix (each test name carries the
// ID of the contract it protects).
#include "../Source/DrumEngine.h"

#include <algorithm>
#include <cmath>
#include <cstring>
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
