#pragma once

#include <JuceHeader.h>

class NetworkClient;

//==============================================================================
/**
 * NotificationSettings provides a UI for managing notification preferences
 *
 * Features:
 * - Toggle for each notification type (likes, comments, follows, etc.)
 * - All changes are persisted to the backend
 * - Loads current preferences on open
 */
class NotificationSettings : public juce::Component,
                              public juce::Button::Listener
{
public:
    NotificationSettings();
    ~NotificationSettings() override;

    //==============================================================================
    // Data binding
    void setNetworkClient(NetworkClient* client) { networkClient = client; }
    void loadPreferences();

    //==============================================================================
    // Callbacks
    std::function<void()> onClose;

    //==============================================================================
    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;
    void buttonClicked(juce::Button* button) override;

private:
    //==============================================================================
    NetworkClient* networkClient = nullptr;

    // State
    bool isLoading = false;
    bool isSaving = false;
    juce::String errorMessage;

    // Preferences state
    bool likesEnabled = true;
    bool commentsEnabled = true;
    bool followsEnabled = true;
    bool mentionsEnabled = true;
    bool dmsEnabled = true;
    bool storiesEnabled = true;
    bool repostsEnabled = true;
    bool challengesEnabled = true;

    //==============================================================================
    // UI Components
    std::unique_ptr<juce::TextButton> closeButton;

    // Toggle buttons for each notification type
    std::unique_ptr<juce::ToggleButton> likesToggle;
    std::unique_ptr<juce::ToggleButton> commentsToggle;
    std::unique_ptr<juce::ToggleButton> followsToggle;
    std::unique_ptr<juce::ToggleButton> mentionsToggle;
    std::unique_ptr<juce::ToggleButton> dmsToggle;
    std::unique_ptr<juce::ToggleButton> storiesToggle;
    std::unique_ptr<juce::ToggleButton> repostsToggle;
    std::unique_ptr<juce::ToggleButton> challengesToggle;

    //==============================================================================
    // Drawing methods
    void drawHeader(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawSection(juce::Graphics& g, const juce::String& title, juce::Rectangle<int> bounds);

    //==============================================================================
    // Helpers
    void setupToggles();
    void populateFromPreferences();
    void handleToggleChange(juce::ToggleButton* toggle);
    void savePreferences();

    //==============================================================================
    // Layout constants
    static constexpr int HEADER_HEIGHT = 60;
    static constexpr int TOGGLE_HEIGHT = 50;
    static constexpr int SECTION_SPACING = 20;
    static constexpr int PADDING = 25;

    //==============================================================================
    // Colors (matching EditProfile style)
    struct Colors
    {
        static inline juce::Colour background { 0xff1a1a1e };
        static inline juce::Colour headerBg { 0xff252529 };
        static inline juce::Colour textPrimary { 0xffffffff };
        static inline juce::Colour textSecondary { 0xffa0a0a0 };
        static inline juce::Colour accent { 0xff00d4ff };
        static inline juce::Colour toggleBg { 0xff2d2d32 };
        static inline juce::Colour toggleBorder { 0xff4a4a4e };
        static inline juce::Colour errorRed { 0xffff4757 };
        static inline juce::Colour closeButton { 0xff3a3a3e };
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NotificationSettings)
};
