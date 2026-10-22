#include "Settings.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>

namespace drumbox
{
namespace
{
bool parseBounded (const std::string& value, int min, int max, int& out)
{
    if (value.empty() || value.size() > 6 || ! std::all_of (value.begin(), value.end(), [] (char c) { return c >= '0' && c <= '9'; }))
        return false;
    const int number = std::stoi (value);
    if (number != 0 && (number < min || number > max))
        return false;
    out = number;
    return true;
}
} // namespace

std::string serializeSettings (const AppSettings& settings)
{
    std::ostringstream out;
    out << "drumbox-settings=1\n"
        << "skin=" << (settings.skin == SkinId::Funk ? "funk" : "dark") << "\n"
        << "width=" << settings.windowWidth << "\n"
        << "height=" << settings.windowHeight << "\n"
        << "firstLaunch=" << (settings.firstLaunch ? "true" : "false") << "\n";
    return out.str();
}

bool parseSettings (const std::string& text, AppSettings& settings)
{
    AppSettings parsed;
    std::istringstream in (text);
    std::string line;
    bool header = false, skin = false, width = false, height = false, first = false;
    while (std::getline (in, line))
    {
        if (! line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty())
            continue;
        const auto eq = line.find ('=');
        if (eq == std::string::npos)
            return false;
        const auto key = line.substr (0, eq);
        const auto value = line.substr (eq + 1);
        if (key == "drumbox-settings")
            header = value == "1";
        else if (key == "skin" && (value == "dark" || value == "funk"))
        {
            parsed.skin = value == "funk" ? SkinId::Funk : SkinId::Dark;
            skin = true;
        }
        else if (key == "width")
            width = parseBounded (value, minWindowWidth, maxWindowSize, parsed.windowWidth);
        else if (key == "height")
            height = parseBounded (value, minWindowHeight, maxWindowSize, parsed.windowHeight);
        else if (key == "firstLaunch" && (value == "true" || value == "false"))
        {
            parsed.firstLaunch = value == "true";
            first = true;
        }
        else
            return false;
    }
    if (! (header && skin && width && height && first) || (parsed.windowWidth == 0) != (parsed.windowHeight == 0))
        return false;
    settings = parsed;
    return true;
}

AppSettings loadSettings (const std::string& path)
{
    AppSettings settings;
    std::ifstream file (path, std::ios::binary);
    if (! file)
        return settings;
    std::ostringstream text;
    text << file.rdbuf();
    if (! parseSettings (text.str(), settings))
        return AppSettings {};
    return settings;
}

bool saveSettings (const AppSettings& settings, const std::string& path)
{
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path target (path);
    if (target.has_parent_path())
        fs::create_directories (target.parent_path(), ec);
    const fs::path partial = target.string() + ".tmp";
    {
        std::ofstream out (partial, std::ios::binary | std::ios::trunc);
        out << serializeSettings (settings);
        out.flush();
        if (! out)
        {
            out.close();
            fs::remove (partial, ec);
            return false;
        }
    }
    fs::rename (partial, target, ec);
    if (ec)
    {
        fs::remove (partial, ec);
        return false;
    }
    return true;
}

WindowSize fitWindowSize (const AppSettings& settings, int screenWidth, int screenHeight)
{
    int width = settings.windowWidth > 0 ? settings.windowWidth : screenWidth * 8 / 10;
    int height = settings.windowHeight > 0 ? settings.windowHeight : screenHeight * 8 / 10;
    width = std::max (minWindowWidth, std::min (width, screenWidth));
    height = std::max (minWindowHeight, std::min (height, screenHeight));
    return { width, height };
}
} // namespace drumbox
