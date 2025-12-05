#pragma once

#include <JuceHeader.h>

class NetworkClient;

//==============================================================================
/**
 * AuthComponent provides a professional login/signup interface
 *
 * Features:
 * - Clean, card-based layout
 * - Proper TextEditor inputs with styling
 * - Form validation with inline error messages
 * - OAuth provider buttons
 * - Smooth state transitions
 * - Keyboard navigation support
 */
class AuthComponent : public juce::Component,
                      public juce::Button::Listener,
                      public juce::TextEditor::Listener
{
public:
    AuthComponent();
    ~AuthComponent() override;

    //==============================================================================
    // Callbacks
    std::function<void(const juce::String& username, const juce::String& email, const juce::String& token)> onLoginSuccess;
    std::function<void(const juce::String& provider)> onOAuthRequested;

    //==============================================================================
    // Network client for API calls
    void setNetworkClient(NetworkClient* client);

    //==============================================================================
    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;
    void buttonClicked(juce::Button* button) override;
    void textEditorReturnKeyPressed(juce::TextEditor& editor) override;
    void textEditorTextChanged(juce::TextEditor& editor) override;

    //==============================================================================
    // State management
    void reset();
    void showError(const juce::String& message);
    void clearError();

private:
    //==============================================================================
    // Auth modes
    enum class AuthMode
    {
        Welcome,    // Initial state with login/signup options
        Login,      // Email login form
        Signup      // Account creation form
    };

    AuthMode currentMode = AuthMode::Welcome;
    bool isLoading = false;
    juce::String errorMessage;

    //==============================================================================
    // Network client
    NetworkClient* networkClient = nullptr;

    //==============================================================================
    // Welcome screen components
    std::unique_ptr<juce::TextButton> loginButton;
    std::unique_ptr<juce::TextButton> signupButton;
    std::unique_ptr<juce::TextButton> googleButton;
    std::unique_ptr<juce::TextButton> discordButton;

    //==============================================================================
    // Login form components
    std::unique_ptr<juce::TextEditor> loginEmailEditor;
    std::unique_ptr<juce::TextEditor> loginPasswordEditor;
    std::unique_ptr<juce::TextButton> loginSubmitButton;
    std::unique_ptr<juce::TextButton> loginBackButton;

    //==============================================================================
    // Signup form components
    std::unique_ptr<juce::TextEditor> signupEmailEditor;
    std::unique_ptr<juce::TextEditor> signupUsernameEditor;
    std::unique_ptr<juce::TextEditor> signupDisplayNameEditor;
    std::unique_ptr<juce::TextEditor> signupPasswordEditor;
    std::unique_ptr<juce::TextEditor> signupConfirmPasswordEditor;
    std::unique_ptr<juce::TextButton> signupSubmitButton;
    std::unique_ptr<juce::TextButton> signupBackButton;

    //==============================================================================
    // Layout helpers
    void setupWelcomeComponents();
    void setupLoginComponents();
    void setupSignupComponents();

    void showWelcome();
    void showLogin();
    void showSignup();

    void hideAllComponents();

    void handleLogin();
    void handleSignup();

    // Styling helpers
    void styleTextEditor(juce::TextEditor& editor, const juce::String& placeholder, bool isPassword = false);
    void stylePrimaryButton(juce::TextButton& button);
    void styleSecondaryButton(juce::TextButton& button);
    void styleOAuthButton(juce::TextButton& button, const juce::String& text, juce::Colour color);

    // Drawing helpers
    void drawCard(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawLogo(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawDivider(juce::Graphics& g, int y, const juce::String& text);

    // Layout constants
    static constexpr int CARD_WIDTH = 420;
    static constexpr int CARD_PADDING = 40;
    static constexpr int FIELD_HEIGHT = 48;
    static constexpr int FIELD_SPACING = 16;
    static constexpr int BUTTON_HEIGHT = 48;

    //==============================================================================
    // Colors
    struct Colors
    {
        static inline juce::Colour background { 0xff1a1a1e };
        static inline juce::Colour cardBackground { 0xff252529 };
        static inline juce::Colour cardBorder { 0xff3a3a3e };
        static inline juce::Colour inputBackground { 0xff2d2d32 };
        static inline juce::Colour inputBorder { 0xff4a4a4e };
        static inline juce::Colour inputBorderFocused { 0xff00d4ff };
        static inline juce::Colour inputText { 0xffffffff };
        static inline juce::Colour inputPlaceholder { 0xff808080 };
        static inline juce::Colour primaryButton { 0xff00d4ff };
        static inline juce::Colour primaryButtonHover { 0xff00b8e0 };
        static inline juce::Colour secondaryButton { 0xff3a3a3e };
        static inline juce::Colour textPrimary { 0xffffffff };
        static inline juce::Colour textSecondary { 0xffa0a0a0 };
        static inline juce::Colour errorRed { 0xffff4757 };
        static inline juce::Colour google { 0xffea4335 };
        static inline juce::Colour discord { 0xff5865f2 };
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AuthComponent)
};
