#include "MainComponent.h"
#include "Settings.h"

#include <juce_gui_basics/juce_gui_basics.h>

class DrumBoxApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return JUCE_APPLICATION_NAME_STRING; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise (const juce::String&) override
    {
        settingsPath = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                           .getChildFile ("DrumBox")
                           .getChildFile ("settings.txt")
                           .getFullPathName()
                           .toStdString();
        settings = drumbox::loadSettings (settingsPath);
        window = std::make_unique<MainWindow> (getApplicationName(), settings, settingsPath);
    }

    void shutdown() override { window.reset(); }

    void systemRequestedQuit() override { quit(); }

private:
    // Remembers the size of the window: saved a moment after the last resize, and on close.
    class MainWindow : public juce::DocumentWindow,
                       private juce::Timer
    {
    public:
        MainWindow (juce::String name, drumbox::AppSettings& appSettings, const std::string& path)
            : DocumentWindow (name, juce::Colour (0xff1b1d22), DocumentWindow::allButtons), settings (appSettings), settingsPath (path)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new MainComponent (settings, settingsPath), true);
            setResizable (true, true);
            setResizeLimits (drumbox::minWindowWidth, drumbox::minWindowHeight, 4000, 2400);

            const auto area = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->userArea;
            const auto size = drumbox::fitWindowSize (settings, area.getWidth(), area.getHeight());
            centreWithSize (size.width, size.height);
            setVisible (true);
        }

        ~MainWindow() override { saveSize(); }

        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }

        void resized() override
        {
            DocumentWindow::resized();
            if (isVisible())
                startTimer (800);
        }

    private:
        void timerCallback() override
        {
            stopTimer();
            saveSize();
        }

        void saveSize()
        {
            if (isMinimised() || isFullScreen())
                return;
            settings.windowWidth = getWidth();
            settings.windowHeight = getHeight();
            drumbox::saveSettings (settings, settingsPath);
        }

        drumbox::AppSettings& settings;
        std::string settingsPath;
    };

    drumbox::AppSettings settings;
    std::string settingsPath;
    std::unique_ptr<MainWindow> window;
};

START_JUCE_APPLICATION (DrumBoxApplication)
