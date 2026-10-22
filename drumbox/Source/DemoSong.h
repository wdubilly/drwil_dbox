#pragma once

#include "DrumEngine.h"

namespace drumbox
{
// Replaces the song of the engine with the demo: a four-position chain over two patterns with drums, bass,
// pad and pluck, so that the first press of PLAY is music. Playing state is left as it is.
void loadDemoSong (DrumEngine& engine);
} // namespace drumbox
