#include "DemoSong.h"

namespace drumbox
{
void loadDemoSong (DrumEngine& engine)
{
    Song& song = engine.getSong();
    for (int p = 0; p < numPatterns; ++p)
        song.clearPattern (p);

    Pattern& first = song.pattern (0);
    for (int s : { 0, 4, 8, 12 })
        first.set (Kick, s, StepState::Normal);
    first.set (Kick, 0, StepState::Accent);
    for (int s : { 4, 12 })
        first.set (Snare, s, StepState::Accent);
    for (int s = 0; s < numSteps; s += 2)
        first.set (ClosedHat, s, StepState::Normal);
    first.set (OpenHat, 14, StepState::Normal);

    Pattern& second = song.pattern (1);
    second.apply (first.capture());

    const auto addLine = [] (Pattern& p, int root)
    {
        p.setBass (0, root, true);
        p.setBass (3, root);
        p.setBass (6, root + 3);
        p.setBass (8, root, true);
        p.setBass (11, root + 7);
        p.setBass (14, root + 5);
        const int chord[] = { 57, 60, 64, 67 };
        for (int s : { 2, 10 })
            for (int v = 0; v < synthVoices; ++v)
                p.setSynthNote (s, v, chord[v] + (root - 33));
        p.setSynthAccent (10, true);
        const int stab[] = { 69, 72, 76 };
        for (int s : { 4, 7, 12 })
            for (int v = 0; v < 3; ++v)
                p.setPluckNote (s, v, stab[v] + (root - 33));
        p.setPluckAccent (12, true);
    };
    addLine (first, 33);
    addLine (second, 38);

    song.setChainLength (4);
    for (int pos = 0; pos < 4; ++pos)
        song.setChainEntry (pos, pos == 2 ? 1 : 0);

    engine.setTempo (110.0);
    engine.setSwing (0.0);
}
} // namespace drumbox
