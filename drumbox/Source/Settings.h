#pragma once

#include <string>

namespace drumbox
{
constexpr int minWindowWidth = 960;
constexpr int minWindowHeight = 600;
constexpr int maxWindowSize = 10000;

enum class SkinId
{
    Dark = 0,
    Funk
};

// What the app remembers between runs. A size of 0 means "follow the screen".
struct AppSettings
{
    SkinId skin = SkinId::Dark;
    int windowWidth = 0;
    int windowHeight = 0;
    bool firstLaunch = true;
};

struct WindowSize
{
    int width;
    int height;
};

std::string serializeSettings (const AppSettings& settings);

// False when the text is not a settings file or is damaged; `settings` is then left untouched.
bool parseSettings (const std::string& text, AppSettings& settings);

// The defaults when the file is missing, unreadable or damaged: never fails.
AppSettings loadSettings (const std::string& path);

// Writes the whole file or leaves the old one; false on failure.
bool saveSettings (const AppSettings& settings, const std::string& path);

// The size to open the window with on a screen of the given usable area: 80 % of the screen when none is
// remembered, otherwise the remembered size; never below the minimum nor beyond the screen (the minimum wins on a
// screen smaller than it).
WindowSize fitWindowSize (const AppSettings& settings, int screenWidth, int screenHeight);
} // namespace drumbox
