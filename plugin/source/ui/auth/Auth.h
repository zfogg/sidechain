#pragma once

#include <JuceHeader.h>

class NetworkClient;

//==============================================================================
/**
 * Auth provides a professional login/signup interface
 *
 * Features:
 * - Clean, card-based layout
 * - Proper TextEditor inputs with styling
 * - Form validation with inline error messages
 * - OAuth provider buttons
 * - Smooth state transitions
 * - Keyboard navigation support
 */
class Auth : public juce::Component,
                      public juce::Button::Listener,
                      public juce::TextEditor::Listener
{
public:
    Auth();
    ~Auth() override;

    //==============================================================================
    // Callbacks

    /** Called when login/signup succeeds
     * Parameters: username, email, authentication token
     */
    std::function<void(const juce::String& username, const juce::String& email, const juce::String& token)> onLoginSuccess;

    /** Called when user requests OAuth login
     * Parameter: provider name (e.g., "google", "discord")
     */
    std::function<void(const juce::String& provider)> onOAuthRequested;

    /** Called when user cancels OAuth flow (8.3.11.11)
     */
    std::function<void()> onOAuthCancelled;

    //==============================================================================
    // Network client for API calls

    /** Set the network client for API calls
     * @param client Pointer to the NetworkClient instance
     */
    void setNetworkClient(NetworkClient* client);

    //==============================================================================
    // Component overrides

    /** Paint the component
     * @param g Graphics context for drawing
     */
    void paint(juce::Graphics& g) override;

    /** Handle component resize
     * @param g Graphics context (unused, kept for override signature)
     */
    void resized() override;

    /** Handle button click events
     * @param button The button that was clicked
     */
    void buttonClicked(juce::Button* button) override;

    /** Handle return key press in text editors
     * @param editor The text editor that received the key press
     */
    void textEditorReturnKeyPressed(juce::TextEditor& editor) override;

    /** Handle text changes in editors (for validation)
     * @param editor The text editor that changed
     */
    void textEditorTextChanged(juce::TextEditor& editor) override;

    //==============================================================================
    // State management

    /** Reset the component to initial state
     */
    void reset();

    /** Display an error message to the user
     * @param message The error message to display
     */
    void showError(const juce::String& message);

    /** Clear any displayed error message
     */
    void clearError();

    //==============================================================================
    // OAuth waiting mode (8.3.11.9-12)

    /** Show OAuth waiting screen with provider name and countdown
     * @param provider The OAuth provider name (e.g., "Google", "Discord")
     * @param timeoutSeconds Total seconds before timeout
     */
    void showOAuthWaiting(const juce::String& provider, int timeoutSeconds = 300);

    /** Update the OAuth waiting countdown
     * @param secondsRemaining Seconds until timeout
     */
    void updateOAuthCountdown(int secondsRemaining);

    /** Hide OAuth waiting screen and return to welcome
     */
    void hideOAuthWaiting();

private:
    //==============================================================================
    // Auth modes
    enum class AuthMode
    {
        Welcome,        // Initial state with login/signup options
        Login,          // Email login form
        Signup,         // Account creation form
        OAuthWaiting    // Waiting for OAuth callback (8.3.11.9-12)
    };

    AuthMode currentMode = AuthMode::Welcome;
    bool isLoading = false;
    juce::String errorMessage;

    // OAuth waiting state (8.3.11.9-12)
    juce::String oauthWaitingProvider;    // Provider being waited for
    int oauthSecondsRemaining = 0;        // Countdown seconds
    int oauthAnimationFrame = 0;          // For animated spinner

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
    std::unique_ptr<juce::ToggleButton> rememberMeCheckbox;
    std::unique_ptr<juce::TextButton> loginForgotPasswordLink;
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
    // OAuth waiting components (8.3.11.9-12)
    std::unique_ptr<juce::TextButton> oauthCancelButton;

    //==============================================================================
    // Layout helpers
    void setupWelcomeComponents();
    void setupLoginComponents();
    void setupSignupComponents();
    void setupOAuthWaitingComponents();

    void showWelcome();
    void showLogin();
    void showSignup();

    void hideAllComponents();

    void handleLogin();
    void handleSignup();
    void handleForgotPassword();

    // Styling helpers
    void styleTextEditor(juce::TextEditor& editor, const juce::String& placeholder, bool isPassword = false);
    void stylePrimaryButton(juce::TextButton& button);
    void styleSecondaryButton(juce::TextButton& button);
    void styleOAuthButton(juce::TextButton& button, const juce::String& text, juce::Colour color);

    // Drawing helpers
    void drawCard(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawLogo(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawDivider(juce::Graphics& g, int y, const juce::String& text);
    void drawPasswordStrengthIndicator(juce::Graphics& g, juce::Rectangle<int> bounds);

    // Password strength calculation
    void updatePasswordStrengthIndicator();
    int calculatePasswordStrength(const juce::String& password) const;  // Returns 0-4 (weak to very strong)

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Auth)
};
