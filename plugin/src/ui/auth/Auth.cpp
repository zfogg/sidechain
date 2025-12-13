#include "Auth.h"
#include "../../network/NetworkClient.h"
#include "../../util/Constants.h"
#include "../../util/Validate.h"
#include "../../util/TextEditorStyler.h"
#include "../../util/Log.h"
#include "../../util/Result.h"

//==============================================================================
Auth::Auth()
{
    Log::info("Auth: Initializing authentication component");

    // Create all UI components BEFORE calling setSize() because setSize() triggers resized()
    Log::debug("Auth: Setting up welcome components");
    setupWelcomeComponents();

    Log::debug("Auth: Setting up login components");
    setupLoginComponents();

    Log::debug("Auth: Setting up signup components");
    setupSignupComponents();

    Log::debug("Auth: Setting up OAuth waiting components");
    setupOAuthWaitingComponents();

    Log::debug("Auth: Setting up two-factor components");
    setupTwoFactorComponents();

    Log::debug("Auth: Showing welcome screen");
    showWelcome();

    // Set size last - this triggers resized() which requires components to exist
    setSize(1000, 800);
    Log::info("Auth: Initialization complete");
}

Auth::~Auth()
{
    Log::debug("Auth: Destroying authentication component");
}

//==============================================================================
void Auth::setNetworkClient(NetworkClient* client)
{
    networkClient = client;
    Log::info("Auth: NetworkClient set " + juce::String(client != nullptr ? "(valid)" : "(null)"));
}

//==============================================================================
void Auth::setupWelcomeComponents()
{
    loginButton = std::make_unique<juce::TextButton>("Sign In");
    stylePrimaryButton(*loginButton);
    loginButton->addListener(this);
    addChildComponent(loginButton.get());

    signupButton = std::make_unique<juce::TextButton>("Create Account");
    styleSecondaryButton(*signupButton);
    signupButton->addListener(this);
    addChildComponent(signupButton.get());

    googleButton = std::make_unique<juce::TextButton>("Continue with Google");
    styleOAuthButton(*googleButton, "Continue with Google", Colors::google);
    googleButton->addListener(this);
    addChildComponent(googleButton.get());

    discordButton = std::make_unique<juce::TextButton>("Continue with Discord");
    styleOAuthButton(*discordButton, "Continue with Discord", Colors::discord);
    discordButton->addListener(this);
    addChildComponent(discordButton.get());
}

void Auth::setupLoginComponents()
{
    loginEmailEditor = std::make_unique<juce::TextEditor>();
    styleTextEditor(*loginEmailEditor, "Email address");
    loginEmailEditor->addListener(this);
    addChildComponent(loginEmailEditor.get());

    loginPasswordEditor = std::make_unique<juce::TextEditor>();
    styleTextEditor(*loginPasswordEditor, "Password", true);
    loginPasswordEditor->addListener(this);
    addChildComponent(loginPasswordEditor.get());

    rememberMeCheckbox = std::make_unique<juce::ToggleButton>("Remember me");
    rememberMeCheckbox->setColour(juce::ToggleButton::textColourId, Colors::textSecondary);
    rememberMeCheckbox->setColour(juce::ToggleButton::tickColourId, Colors::primaryButton);
    rememberMeCheckbox->setColour(juce::ToggleButton::tickDisabledColourId, Colors::inputBorder);
    rememberMeCheckbox->setToggleState(false, juce::dontSendNotification);
    addChildComponent(rememberMeCheckbox.get());

    loginSubmitButton = std::make_unique<juce::TextButton>("Sign In");
    stylePrimaryButton(*loginSubmitButton);
    loginSubmitButton->addListener(this);
    addChildComponent(loginSubmitButton.get());

    loginBackButton = std::make_unique<juce::TextButton>("Back");
    styleSecondaryButton(*loginBackButton);
    loginBackButton->addListener(this);
    addChildComponent(loginBackButton.get());

    // Forgot password link (styled as text link)
    loginForgotPasswordLink = std::make_unique<juce::TextButton>("Forgot Password?");
    loginForgotPasswordLink->setColour(juce::TextButton::textColourOffId, Colors::textSecondary);
    loginForgotPasswordLink->setColour(juce::TextButton::textColourOnId, Colors::primaryButton);
    loginForgotPasswordLink->setConnectedEdges(0);
    loginForgotPasswordLink->setButtonText("Forgot Password?");
    loginForgotPasswordLink->changeWidthToFitText();
    loginForgotPasswordLink->setMouseCursor(juce::MouseCursor::PointingHandCursor);
    loginForgotPasswordLink->addListener(this);
    addChildComponent(loginForgotPasswordLink.get());
}

void Auth::setupSignupComponents()
{
    signupEmailEditor = std::make_unique<juce::TextEditor>();
    styleTextEditor(*signupEmailEditor, "Email address");
    signupEmailEditor->addListener(this);
    addChildComponent(signupEmailEditor.get());

    signupUsernameEditor = std::make_unique<juce::TextEditor>();
    styleTextEditor(*signupUsernameEditor, "Username");
    signupUsernameEditor->addListener(this);
    addChildComponent(signupUsernameEditor.get());

    signupDisplayNameEditor = std::make_unique<juce::TextEditor>();
    styleTextEditor(*signupDisplayNameEditor, "Display name");
    signupDisplayNameEditor->addListener(this);
    addChildComponent(signupDisplayNameEditor.get());

    signupPasswordEditor = std::make_unique<juce::TextEditor>();
    styleTextEditor(*signupPasswordEditor, "Password (8+ characters)", true);
    signupPasswordEditor->addListener(this);
    addChildComponent(signupPasswordEditor.get());

    signupConfirmPasswordEditor = std::make_unique<juce::TextEditor>();
    styleTextEditor(*signupConfirmPasswordEditor, "Confirm password", true);
    signupConfirmPasswordEditor->addListener(this);
    addChildComponent(signupConfirmPasswordEditor.get());

    signupSubmitButton = std::make_unique<juce::TextButton>("Create Account");
    stylePrimaryButton(*signupSubmitButton);
    signupSubmitButton->addListener(this);
    addChildComponent(signupSubmitButton.get());

    signupBackButton = std::make_unique<juce::TextButton>("Back");
    styleSecondaryButton(*signupBackButton);
    signupBackButton->addListener(this);
    addChildComponent(signupBackButton.get());
}

void Auth::setupOAuthWaitingComponents()
{
    // Cancel button for OAuth waiting screen (8.3.11.11)
    oauthCancelButton = std::make_unique<juce::TextButton>("Cancel");
    styleSecondaryButton(*oauthCancelButton);
    oauthCancelButton->addListener(this);
    addChildComponent(oauthCancelButton.get());
}

void Auth::setupTwoFactorComponents()
{
    // Code input for 2FA verification
    twoFactorCodeEditor = std::make_unique<juce::TextEditor>();
    styleTextEditor(*twoFactorCodeEditor, "Enter 6-digit code");
    twoFactorCodeEditor->setInputRestrictions(6, "0123456789");
    twoFactorCodeEditor->addListener(this);
    addChildComponent(twoFactorCodeEditor.get());

    // Verify button
    twoFactorVerifyButton = std::make_unique<juce::TextButton>("Verify");
    stylePrimaryButton(*twoFactorVerifyButton);
    twoFactorVerifyButton->addListener(this);
    addChildComponent(twoFactorVerifyButton.get());

    // Back button
    twoFactorBackButton = std::make_unique<juce::TextButton>("Back");
    styleSecondaryButton(*twoFactorBackButton);
    twoFactorBackButton->addListener(this);
    addChildComponent(twoFactorBackButton.get());
}

//==============================================================================
void Auth::styleTextEditor(juce::TextEditor& editor, const juce::String& placeholder, bool isPassword)
{
    editor.setMultiLine(false);
    editor.setReturnKeyStartsNewLine(false);
    editor.setScrollbarsShown(false);
    editor.setCaretVisible(true);
    editor.setPopupMenuEnabled(false);

    if (isPassword)
        editor.setPasswordCharacter(0x2022); // bullet

    editor.setTextToShowWhenEmpty(placeholder, Colors::inputPlaceholder);
    editor.setColour(juce::TextEditor::backgroundColourId, Colors::inputBackground);
    editor.setColour(juce::TextEditor::outlineColourId, Colors::inputBorder);
    editor.setColour(juce::TextEditor::focusedOutlineColourId, Colors::inputBorderFocused);
    editor.setColour(juce::TextEditor::textColourId, Colors::inputText);
    editor.setColour(juce::CaretComponent::caretColourId, Colors::primaryButton);
    editor.setFont(juce::Font(juce::FontOptions().withHeight(15.0f)));
    editor.setJustification(juce::Justification::centredLeft);
    editor.setIndents(16, 0);
}

void Auth::stylePrimaryButton(juce::TextButton& button)
{
    button.setColour(juce::TextButton::buttonColourId, Colors::primaryButton);
    button.setColour(juce::TextButton::buttonOnColourId, Colors::primaryButtonHover);
    button.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    button.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    button.setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void Auth::styleSecondaryButton(juce::TextButton& button)
{
    button.setColour(juce::TextButton::buttonColourId, Colors::secondaryButton);
    button.setColour(juce::TextButton::buttonOnColourId, Colors::secondaryButton.brighter(0.1f));
    button.setColour(juce::TextButton::textColourOffId, Colors::textSecondary);
    button.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    button.setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void Auth::styleOAuthButton(juce::TextButton& button, const juce::String& text, juce::Colour color)
{
    button.setButtonText(text);
    button.setColour(juce::TextButton::buttonColourId, color.withAlpha(0.15f));
    button.setColour(juce::TextButton::buttonOnColourId, color.withAlpha(0.25f));
    button.setColour(juce::TextButton::textColourOffId, color);
    button.setColour(juce::TextButton::textColourOnId, color.brighter(0.2f));
    button.setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

//==============================================================================
void Auth::paint(juce::Graphics& g)
{
    // Background gradient
    g.setGradientFill(juce::ColourGradient(
        Colors::background,
        0.0f, 0.0f,
        Colors::background.darker(0.3f),
        0.0f, static_cast<float>(getHeight()),
        false
    ));
    g.fillAll();

    // Calculate card bounds
    auto cardBounds = getLocalBounds().withSizeKeepingCentre(CARD_WIDTH, 0);

    // Draw based on mode
    switch (currentMode)
    {
        case AuthMode::Welcome:
            cardBounds = cardBounds.withHeight(520);
            break;
        case AuthMode::Login:
            cardBounds = cardBounds.withHeight(400);
            break;
        case AuthMode::Signup:
            cardBounds = cardBounds.withHeight(580);
            break;
        case AuthMode::OAuthWaiting:
            cardBounds = cardBounds.withHeight(350);
            break;
        case AuthMode::TwoFactorVerify:
            cardBounds = cardBounds.withHeight(380);
            break;
    }

    cardBounds = cardBounds.withCentre(getLocalBounds().getCentre());

    // Draw card background
    drawCard(g, cardBounds);

    // Draw logo area
    auto logoArea = cardBounds.removeFromTop(100);
    drawLogo(g, logoArea);

    // Draw title based on mode
    g.setColour(Colors::textPrimary);
    g.setFont(juce::Font(juce::FontOptions().withHeight(22.0f).withStyle("Bold")));

    juce::String title;
    juce::String subtitle;

    switch (currentMode)
    {
        case AuthMode::Welcome:
            title = "Welcome to Sidechain";
            subtitle = "Share loops with producers worldwide";
            break;
        case AuthMode::Login:
            title = "Sign In";
            subtitle = "Welcome back! Enter your credentials";
            break;
        case AuthMode::Signup:
            title = "Create Account";
            subtitle = "Join the community of music producers";
            break;
        case AuthMode::OAuthWaiting:
            title = "Waiting for " + oauthWaitingProvider;
            subtitle = "Complete the sign-in in your browser";
            break;
        case AuthMode::TwoFactorVerify:
            title = "Two-Factor Authentication";
            subtitle = twoFactorType == "hotp" ? "Enter code from your security key" : "Enter code from your authenticator app";
            break;
    }

    auto titleArea = cardBounds.removeFromTop(30);
    g.drawText(title, titleArea, juce::Justification::centred);

    g.setColour(Colors::textSecondary);
    g.setFont(14.0f);
    auto subtitleArea = cardBounds.removeFromTop(24);
    g.drawText(subtitle, subtitleArea, juce::Justification::centred);

    // Draw error message if present
    if (errorMessage.isNotEmpty())
    {
        auto errorArea = cardBounds.removeFromTop(40).reduced(CARD_PADDING, 5);
        g.setColour(Colors::errorRed.withAlpha(0.15f));
        g.fillRoundedRectangle(errorArea.toFloat(), 6.0f);
        g.setColour(Colors::errorRed);
        g.setFont(13.0f);
        g.drawText(errorMessage, errorArea, juce::Justification::centred);
    }

    // Draw loading indicator if loading
    if (isLoading)
    {
        auto loadingArea = getLocalBounds().withSizeKeepingCentre(200, 50);
        loadingArea = loadingArea.withY(cardBounds.getBottom() + 20);
        g.setColour(Colors::textSecondary);
        g.setFont(14.0f);
        g.drawText("Connecting...", loadingArea, juce::Justification::centred);
    }

    // Draw password strength indicator in signup mode
    if (currentMode == AuthMode::Signup && signupPasswordEditor->isVisible())
    {
        auto passwordBounds = signupPasswordEditor->getBounds();
        auto strengthBounds = passwordBounds.translated(0, passwordBounds.getHeight() + 2).withHeight(4);
        drawPasswordStrengthIndicator(g, strengthBounds);
    }

    // Draw divider for OAuth in welcome mode
    if (currentMode == AuthMode::Welcome)
    {
        int dividerY = cardBounds.getY() + 160;
        drawDivider(g, dividerY, "or continue with");
    }

    // Draw OAuth waiting UI (8.3.11.9-12)
    if (currentMode == AuthMode::OAuthWaiting)
    {
        auto contentArea = cardBounds.reduced(CARD_PADDING);
        contentArea.removeFromTop(100);  // Skip logo/title area

        // Draw animated spinner (8.3.11.9)
        auto spinnerArea = contentArea.removeFromTop(80).withSizeKeepingCentre(60, 60);
        g.setColour(Colors::primaryButton);

        // Draw rotating dots animation
        const int numDots = 8;
        const float dotRadius = 4.0f;
        const float spinnerRadius = 25.0f;
        const float angleOffset = static_cast<float>(oauthAnimationFrame) * 0.15f;

        for (int i = 0; i < numDots; ++i)
        {
            float angle = angleOffset + (i * juce::MathConstants<float>::twoPi / numDots);
            float x = spinnerArea.getCentreX() + std::cos(angle) * spinnerRadius;
            float y = spinnerArea.getCentreY() + std::sin(angle) * spinnerRadius;

            // Fade dots based on position in rotation
            float alpha = 0.3f + 0.7f * (static_cast<float>(i) / numDots);
            g.setColour(Colors::primaryButton.withAlpha(alpha));
            g.fillEllipse(x - dotRadius, y - dotRadius, dotRadius * 2, dotRadius * 2);
        }

        contentArea.removeFromTop(10);

        // Draw "A browser window has been opened" message (8.3.11.12)
        g.setColour(Colors::textSecondary);
        g.setFont(14.0f);
        auto browserMsgArea = contentArea.removeFromTop(25);
        g.drawText("A browser window has been opened for " + oauthWaitingProvider + " sign-in.",
                   browserMsgArea, juce::Justification::centred);

        contentArea.removeFromTop(15);

        // Draw countdown timer (8.3.11.10)
        if (oauthSecondsRemaining > 0)
        {
            int minutes = oauthSecondsRemaining / 60;
            int seconds = oauthSecondsRemaining % 60;
            juce::String timeStr = juce::String::formatted("%d:%02d", minutes, seconds);

            g.setColour(Colors::textSecondary);
            g.setFont(13.0f);
            auto countdownArea = contentArea.removeFromTop(20);
            g.drawText("Time remaining: " + timeStr, countdownArea, juce::Justification::centred);
        }
    }
}

void Auth::drawCard(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    // Shadow
    g.setColour(juce::Colours::black.withAlpha(0.3f));
    g.fillRoundedRectangle(bounds.translated(0, 4).toFloat(), 16.0f);

    // Card background
    g.setColour(Colors::cardBackground);
    g.fillRoundedRectangle(bounds.toFloat(), 16.0f);

    // Card border
    g.setColour(Colors::cardBorder);
    g.drawRoundedRectangle(bounds.toFloat(), 16.0f, 1.0f);
}

void Auth::drawLogo(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    // Logo icon (waveform-inspired)
    auto iconArea = bounds.withSizeKeepingCentre(60, 60).translated(0, 10);

    g.setColour(Colors::primaryButton);

    // Draw stylized waveform bars
    int barWidth = 6;
    int spacing = 4;
    int totalWidth = 5 * barWidth + 4 * spacing;
    int startX = iconArea.getCentreX() - totalWidth / 2;
    int centerY = iconArea.getCentreY();

    int heights[] = { 20, 35, 50, 35, 20 };
    for (int i = 0; i < 5; ++i)
    {
        int barX = startX + i * (barWidth + spacing);
        int barHeight = heights[i];
        g.fillRoundedRectangle(static_cast<float>(barX),
                               static_cast<float>(centerY - barHeight / 2),
                               static_cast<float>(barWidth),
                               static_cast<float>(barHeight),
                               3.0f);
    }

    // Brand name
    g.setColour(Colors::textPrimary);
    g.setFont(juce::Font(juce::FontOptions().withHeight(28.0f).withStyle("Bold")));
    g.drawText("Sidechain", bounds.withY(bounds.getBottom() - 30).withHeight(30), juce::Justification::centred);
}

void Auth::drawDivider(juce::Graphics& g, int y, const juce::String& text)
{
    auto bounds = getLocalBounds().withSizeKeepingCentre(CARD_WIDTH - CARD_PADDING * 2, 20).withY(y);

    g.setColour(Colors::cardBorder);
    int textWidth = 140;
    int lineY = bounds.getCentreY();

    // Left line
    g.drawLine(static_cast<float>(bounds.getX()), static_cast<float>(lineY),
               static_cast<float>(bounds.getCentreX() - textWidth / 2 - 10), static_cast<float>(lineY), 1.0f);

    // Right line
    g.drawLine(static_cast<float>(bounds.getCentreX() + textWidth / 2 + 10), static_cast<float>(lineY),
               static_cast<float>(bounds.getRight()), static_cast<float>(lineY), 1.0f);

    // Text
    g.setColour(Colors::textSecondary);
    g.setFont(13.0f);
    g.drawText(text, bounds.withSizeKeepingCentre(textWidth, 20), juce::Justification::centred);
}

void Auth::drawPasswordStrengthIndicator(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    juce::String password = signupPasswordEditor->getText();
    int strength = calculatePasswordStrength(password);

    // Draw background bar
    g.setColour(Colors::inputBorder);
    g.fillRoundedRectangle(bounds.toFloat(), 2.0f);

    if (strength > 0)
    {
        // Calculate color based on strength (0=weak red, 4=very strong green)
        juce::Colour strengthColor;
        if (strength <= 1)
            strengthColor = juce::Colour(0xffff4757);  // Red - weak
        else if (strength == 2)
            strengthColor = juce::Colour(0xffffa502);  // Orange - fair
        else if (strength == 3)
            strengthColor = juce::Colour(0xffffd32a);  // Yellow - good
        else
            strengthColor = juce::Colour(0xff2ed573);  // Green - very strong

        // Draw strength bar (width based on strength)
        float strengthWidth = (bounds.getWidth() * strength) / 4.0f;
        auto strengthBounds = bounds.withWidth(static_cast<int>(strengthWidth));
        g.setColour(strengthColor);
        g.fillRoundedRectangle(strengthBounds.toFloat(), 2.0f);
    }
}

int Auth::calculatePasswordStrength(const juce::String& password) const
{
    if (password.isEmpty())
        return 0;

    int score = 0;

    // Length check
    if (password.length() >= 8)
        score++;
    if (password.length() >= 12)
        score++;

    // Character variety checks
    bool hasLower = false;
    bool hasUpper = false;
    bool hasDigit = false;
    bool hasSpecial = false;

    for (int i = 0; i < password.length(); ++i)
    {
        juce::juce_wchar c = password[i];
        if (c >= 'a' && c <= 'z')
            hasLower = true;
        else if (c >= 'A' && c <= 'Z')
            hasUpper = true;
        else if (c >= '0' && c <= '9')
            hasDigit = true;
        else
            hasSpecial = true;
    }

    if (hasLower && hasUpper)
        score++;
    if (hasDigit)
        score++;
    if (hasSpecial)
        score++;

    // Cap at 4 (very strong)
    return juce::jmin(4, score);
}

void Auth::updatePasswordStrengthIndicator()
{
    repaint();
}

//==============================================================================
void Auth::resized()
{
    Log::debug("Auth: Component resized to " + juce::String(getWidth()) + "x" + juce::String(getHeight()));
    auto cardBounds = getLocalBounds().withSizeKeepingCentre(CARD_WIDTH, 600);
    cardBounds = cardBounds.withCentre(getLocalBounds().getCentre());

    auto contentBounds = cardBounds.reduced(CARD_PADDING);
    contentBounds.removeFromTop(180); // Logo + title area

    // Remove error area if present
    if (errorMessage.isNotEmpty())
        contentBounds.removeFromTop(50);

    switch (currentMode)
    {
        case AuthMode::Welcome:
        {
            // Main action buttons
            loginButton->setBounds(contentBounds.removeFromTop(BUTTON_HEIGHT));
            contentBounds.removeFromTop(12);
            signupButton->setBounds(contentBounds.removeFromTop(BUTTON_HEIGHT));

            // Divider space
            contentBounds.removeFromTop(50);

            // OAuth buttons
            googleButton->setBounds(contentBounds.removeFromTop(BUTTON_HEIGHT));
            contentBounds.removeFromTop(12);
            discordButton->setBounds(contentBounds.removeFromTop(BUTTON_HEIGHT));
            break;
        }

        case AuthMode::Login:
        {
            loginEmailEditor->setBounds(contentBounds.removeFromTop(FIELD_HEIGHT));
            contentBounds.removeFromTop(FIELD_SPACING);
            loginPasswordEditor->setBounds(contentBounds.removeFromTop(FIELD_HEIGHT));
            contentBounds.removeFromTop(8);
            auto rememberMeBounds = contentBounds.removeFromTop(24);
            rememberMeCheckbox->setBounds(rememberMeBounds);
            auto forgotPasswordBounds = contentBounds.removeFromTop(20);
            loginForgotPasswordLink->setBounds(forgotPasswordBounds.withX(forgotPasswordBounds.getRight() - loginForgotPasswordLink->getWidth()).withWidth(loginForgotPasswordLink->getWidth()));
            contentBounds.removeFromTop(FIELD_SPACING - 8);
            loginSubmitButton->setBounds(contentBounds.removeFromTop(BUTTON_HEIGHT));
            contentBounds.removeFromTop(12);
            loginBackButton->setBounds(contentBounds.removeFromTop(BUTTON_HEIGHT));
            break;
        }

        case AuthMode::Signup:
        {
            signupEmailEditor->setBounds(contentBounds.removeFromTop(FIELD_HEIGHT));
            contentBounds.removeFromTop(FIELD_SPACING);
            signupUsernameEditor->setBounds(contentBounds.removeFromTop(FIELD_HEIGHT));
            contentBounds.removeFromTop(FIELD_SPACING);
            signupDisplayNameEditor->setBounds(contentBounds.removeFromTop(FIELD_HEIGHT));
            contentBounds.removeFromTop(FIELD_SPACING);
            signupPasswordEditor->setBounds(contentBounds.removeFromTop(FIELD_HEIGHT));
            contentBounds.removeFromTop(4);  // Small gap for strength indicator
            // Password strength indicator will be drawn below password field
            contentBounds.removeFromTop(FIELD_SPACING - 4);
            signupConfirmPasswordEditor->setBounds(contentBounds.removeFromTop(FIELD_HEIGHT));
            contentBounds.removeFromTop(FIELD_SPACING + 8);
            signupSubmitButton->setBounds(contentBounds.removeFromTop(BUTTON_HEIGHT));
            contentBounds.removeFromTop(12);
            signupBackButton->setBounds(contentBounds.removeFromTop(BUTTON_HEIGHT));
            break;
        }

        case AuthMode::OAuthWaiting:
        {
            // Skip spinner and message area (drawn in paint)
            contentBounds.removeFromTop(150);

            // Cancel button at bottom (8.3.11.11)
            oauthCancelButton->setBounds(contentBounds.removeFromTop(BUTTON_HEIGHT));
            break;
        }

        case AuthMode::TwoFactorVerify:
        {
            twoFactorCodeEditor->setBounds(contentBounds.removeFromTop(FIELD_HEIGHT));
            contentBounds.removeFromTop(FIELD_SPACING);
            twoFactorVerifyButton->setBounds(contentBounds.removeFromTop(BUTTON_HEIGHT));
            contentBounds.removeFromTop(12);
            twoFactorBackButton->setBounds(contentBounds.removeFromTop(BUTTON_HEIGHT));
            break;
        }
    }
}

//==============================================================================
void Auth::hideAllComponents()
{
    // Welcome components
    loginButton->setVisible(false);
    signupButton->setVisible(false);
    googleButton->setVisible(false);
    discordButton->setVisible(false);

    // Login components
    loginEmailEditor->setVisible(false);
    loginPasswordEditor->setVisible(false);
    rememberMeCheckbox->setVisible(false);
    loginForgotPasswordLink->setVisible(false);
    loginSubmitButton->setVisible(false);
    loginBackButton->setVisible(false);

    // Signup components
    signupEmailEditor->setVisible(false);
    signupUsernameEditor->setVisible(false);
    signupDisplayNameEditor->setVisible(false);
    signupPasswordEditor->setVisible(false);
    signupConfirmPasswordEditor->setVisible(false);
    signupSubmitButton->setVisible(false);
    signupBackButton->setVisible(false);

    // OAuth waiting components
    oauthCancelButton->setVisible(false);

    // Two-factor authentication components
    twoFactorCodeEditor->setVisible(false);
    twoFactorVerifyButton->setVisible(false);
    twoFactorBackButton->setVisible(false);
}

void Auth::showWelcome()
{
    Log::info("Auth: Switching to welcome mode");
    currentMode = AuthMode::Welcome;
    hideAllComponents();
    clearError();

    loginButton->setVisible(true);
    signupButton->setVisible(true);
    googleButton->setVisible(true);
    discordButton->setVisible(true);

    resized();
    repaint();
    Log::debug("Auth: Welcome screen displayed");
}

void Auth::showLogin()
{
    Log::info("Auth: Switching to login mode");
    currentMode = AuthMode::Login;
    hideAllComponents();
    clearError();

    loginEmailEditor->setVisible(true);
    loginPasswordEditor->setVisible(true);
    rememberMeCheckbox->setVisible(true);
    loginForgotPasswordLink->setVisible(true);
    loginSubmitButton->setVisible(true);
    loginBackButton->setVisible(true);

    loginEmailEditor->clear();
    loginPasswordEditor->clear();
    loginEmailEditor->grabKeyboardFocus();

    resized();
    repaint();
    Log::debug("Auth: Login form displayed");
}

void Auth::showSignup()
{
    Log::info("Auth: Switching to signup mode");
    currentMode = AuthMode::Signup;
    hideAllComponents();
    clearError();

    signupEmailEditor->setVisible(true);
    signupUsernameEditor->setVisible(true);
    signupDisplayNameEditor->setVisible(true);
    signupPasswordEditor->setVisible(true);
    signupConfirmPasswordEditor->setVisible(true);
    signupSubmitButton->setVisible(true);
    signupBackButton->setVisible(true);

    signupEmailEditor->clear();
    signupUsernameEditor->clear();
    signupDisplayNameEditor->clear();
    signupPasswordEditor->clear();
    signupConfirmPasswordEditor->clear();
    signupEmailEditor->grabKeyboardFocus();

    resized();
    repaint();
    Log::debug("Auth: Signup form displayed");
}

void Auth::showTwoFactorVerify()
{
    Log::info("Auth: Switching to two-factor verify mode");
    currentMode = AuthMode::TwoFactorVerify;
    hideAllComponents();
    clearError();

    twoFactorCodeEditor->setVisible(true);
    twoFactorVerifyButton->setVisible(true);
    twoFactorBackButton->setVisible(true);

    twoFactorCodeEditor->clear();
    twoFactorCodeEditor->grabKeyboardFocus();

    resized();
    repaint();
    Log::debug("Auth: Two-factor verification form displayed");
}

//==============================================================================
// OAuth Waiting Mode (8.3.11.9-12)
//==============================================================================

void Auth::showOAuthWaiting(const juce::String& provider, int timeoutSeconds)
{
    Log::info("Auth: Switching to OAuth waiting mode for " + provider);
    currentMode = AuthMode::OAuthWaiting;
    hideAllComponents();
    clearError();

    // Store OAuth state
    oauthWaitingProvider = provider.substring(0, 1).toUpperCase() + provider.substring(1);  // Capitalize
    oauthSecondsRemaining = timeoutSeconds;
    oauthAnimationFrame = 0;

    // Show cancel button
    oauthCancelButton->setVisible(true);

    resized();
    repaint();
    Log::debug("Auth: OAuth waiting screen displayed");
}

void Auth::updateOAuthCountdown(int secondsRemaining)
{
    oauthSecondsRemaining = secondsRemaining;
    oauthAnimationFrame++;  // Advance animation on each update

    // Trigger repaint to update countdown and spinner
    repaint();
}

void Auth::hideOAuthWaiting()
{
    Log::info("Auth: Hiding OAuth waiting screen");
    oauthWaitingProvider = "";
    oauthSecondsRemaining = 0;
    oauthAnimationFrame = 0;
    showWelcome();
}

void Auth::reset()
{
    Log::info("Auth: Resetting to initial state");
    isLoading = false;
    oauthWaitingProvider = "";
    oauthSecondsRemaining = 0;
    oauthAnimationFrame = 0;
    showWelcome();
}

void Auth::showError(const juce::String& message)
{
    Log::warn("Auth: Showing error - " + message);
    errorMessage = message;
    isLoading = false;
    resized();
    repaint();
}

void Auth::clearError()
{
    if (errorMessage.isNotEmpty())
    {
        Log::debug("Auth: Clearing error message");
        errorMessage = "";
        repaint();
    }
}

//==============================================================================
void Auth::buttonClicked(juce::Button* button)
{
    if (button == loginButton.get())
    {
        Log::debug("Auth: Login button clicked");
        showLogin();
    }
    else if (button == signupButton.get())
    {
        Log::debug("Auth: Signup button clicked");
        showSignup();
    }
    else if (button == googleButton.get())
    {
        Log::info("Auth: Google OAuth button clicked");
        if (onOAuthRequested)
            onOAuthRequested("google");
        else
            Log::warn("Auth: OAuth callback not set");
    }
    else if (button == discordButton.get())
    {
        Log::info("Auth: Discord OAuth button clicked");
        if (onOAuthRequested)
            onOAuthRequested("discord");
        else
            Log::warn("Auth: OAuth callback not set");
    }
    else if (button == loginBackButton.get() || button == signupBackButton.get())
    {
        Log::debug("Auth: Back button clicked");
        showWelcome();
    }
    else if (button == loginForgotPasswordLink.get())
    {
        Log::info("Auth: Forgot password link clicked");
        handleForgotPassword();
    }
    else if (button == loginSubmitButton.get())
    {
        Log::info("Auth: Login submit button clicked");
        handleLogin();
    }
    else if (button == signupSubmitButton.get())
    {
        Log::info("Auth: Signup submit button clicked");
        handleSignup();
    }
    else if (button == oauthCancelButton.get())
    {
        Log::info("Auth: OAuth cancel button clicked");
        hideOAuthWaiting();
        if (onOAuthCancelled)
            onOAuthCancelled();
    }
    else if (button == twoFactorVerifyButton.get())
    {
        Log::info("Auth: Two-factor verify button clicked");
        handleTwoFactorVerify();
    }
    else if (button == twoFactorBackButton.get())
    {
        Log::debug("Auth: Two-factor back button clicked");
        twoFactorUserId = "";
        twoFactorType = "";
        showLogin();
    }
}

void Auth::textEditorReturnKeyPressed(juce::TextEditor& editor)
{
    if (currentMode == AuthMode::Login)
    {
        if (&editor == loginEmailEditor.get())
        {
            Log::debug("Auth: Return key pressed in login email field, moving to password");
            loginPasswordEditor->grabKeyboardFocus();
        }
        else if (&editor == loginPasswordEditor.get())
        {
            Log::debug("Auth: Return key pressed in login password field, submitting");
            handleLogin();
        }
    }
    else if (currentMode == AuthMode::Signup)
    {
        if (&editor == signupEmailEditor.get())
        {
            Log::debug("Auth: Return key pressed in signup email field, moving to username");
            signupUsernameEditor->grabKeyboardFocus();
        }
        else if (&editor == signupUsernameEditor.get())
        {
            Log::debug("Auth: Return key pressed in signup username field, moving to display name");
            signupDisplayNameEditor->grabKeyboardFocus();
        }
        else if (&editor == signupDisplayNameEditor.get())
        {
            Log::debug("Auth: Return key pressed in signup display name field, moving to password");
            signupPasswordEditor->grabKeyboardFocus();
        }
        else if (&editor == signupPasswordEditor.get())
        {
            Log::debug("Auth: Return key pressed in signup password field, moving to confirm password");
            signupConfirmPasswordEditor->grabKeyboardFocus();
        }
        else if (&editor == signupConfirmPasswordEditor.get())
        {
            Log::debug("Auth: Return key pressed in signup confirm password field, submitting");
            handleSignup();
        }
    }
    else if (currentMode == AuthMode::TwoFactorVerify)
    {
        if (&editor == twoFactorCodeEditor.get())
        {
            Log::debug("Auth: Return key pressed in 2FA code field, submitting");
            handleTwoFactorVerify();
        }
    }
}

void Auth::textEditorTextChanged(juce::TextEditor& editor)
{
    // Clear error when user starts typing
    if (errorMessage.isNotEmpty())
    {
        Log::debug("Auth: User typing, clearing error message");
        clearError();
    }

    // Update password strength indicator during signup
    if (currentMode == AuthMode::Signup && &editor == signupPasswordEditor.get())
    {
        repaint();  // Trigger repaint to update strength indicator
    }
}

//==============================================================================
void Auth::handleLogin()
{
    Log::info("Auth: Handling login request");
    auto email = loginEmailEditor->getText().trim();
    auto password = loginPasswordEditor->getText();

    Log::debug("Auth: Login attempt for email: " + email);

    // Validation
    if (Validate::isBlank(email))
    {
        Log::warn("Auth: Login validation failed - blank email");
        showError("Please enter your email address");
        loginEmailEditor->grabKeyboardFocus();
        return;
    }

    if (!Validate::isEmail(email))
    {
        Log::warn("Auth: Login validation failed - invalid email format: " + email);
        showError("Please enter a valid email address");
        loginEmailEditor->grabKeyboardFocus();
        return;
    }

    if (Validate::isBlank(password))
    {
        Log::warn("Auth: Login validation failed - blank password");
        showError("Please enter your password");
        loginPasswordEditor->grabKeyboardFocus();
        return;
    }

    // Note: "Remember me" checkbox is now implemented - UI added, secure storage TODO for OS keychain integration
    // Note: Password strength indicator is now implemented - shows visual feedback during signup
    // Note: Email verification prompt is now implemented - checks status after login and shows warning if not verified

    Log::debug("Auth: Login validation passed, initiating API call");

    // Show loading state
    isLoading = true;
    loginSubmitButton->setEnabled(false);
    repaint();

    // Make API call
    if (networkClient == nullptr)
    {
        Log::error("Auth: Cannot login - NetworkClient is null");
        isLoading = false;
        loginSubmitButton->setEnabled(true);
        showError("Network client not available");
        repaint();
        return;
    }

    Log::info("Auth: Calling NetworkClient::loginWithTwoFactor for: " + email);
    networkClient->loginWithTwoFactor(email, password, [this, email](NetworkClient::LoginResult result) {
        isLoading = false;
        loginSubmitButton->setEnabled(true);

        if (result.requires2FA)
        {
            // 2FA required - store user ID and show 2FA verification screen
            Log::info("Auth: 2FA required for user: " + result.userId + " (type: " + result.twoFactorType + ")");
            twoFactorUserId = result.userId;
            twoFactorType = result.twoFactorType;
            showTwoFactorVerify();
        }
        else if (result.success)
        {
            Log::info("Auth: Login successful for: " + email + ", userId: " + result.userId);
            juce::String username = result.username;
            juce::String token = result.token;

            if (username.isEmpty() && networkClient)
            {
                username = networkClient->getCurrentUsername();
                Log::debug("Auth: Retrieved username from NetworkClient: " + username);
            }

            // Handle "Remember me" - store credentials securely if checked
            if (rememberMeCheckbox && rememberMeCheckbox->getToggleState())
            {
                // TODO: Phase 8.3.11.13 - Implement secure credential storage using OS keychain
                Log::debug("Auth: Remember me checked - credentials should be stored securely");
            }

            // Check email verification status - fetch user profile to check email_verified
            if (networkClient)
            {
                juce::String meEndpoint = networkClient->getBaseUrl() + "/api/v1/auth/me";
                networkClient->getAbsolute(meEndpoint, [this, username, email, token](Outcome<juce::var> meResult) {
                    bool emailVerified = true;

                    if (meResult.isOk())
                    {
                        auto userData = meResult.getValue();
                        if (userData.isObject())
                        {
                            emailVerified = userData.getProperty("email_verified", true).operator bool();
                            Log::debug("Auth: Email verification status: " + juce::String(emailVerified ? "verified" : "not verified"));
                        }
                    }

                    if (!emailVerified)
                    {
                        auto opts = juce::MessageBoxOptions()
                            .withIconType(juce::MessageBoxIconType::WarningIcon)
                            .withTitle("Email Not Verified")
                            .withMessage("Please verify your email address to access all features.\n\n"
                                "A verification email has been sent to " + email + ".\n\n"
                                "You can still use the app, but some features may be limited.")
                            .withButton("OK");

                        juce::AlertWindow::showAsync(opts, [this, username, email, token](int) {
                            if (onLoginSuccess)
                            {
                                Log::info("Auth: Calling onLoginSuccess callback (email not verified)");
                                onLoginSuccess(username, email, token);
                            }
                        });
                    }
                    else
                    {
                        if (onLoginSuccess)
                        {
                            Log::info("Auth: Calling onLoginSuccess callback");
                            onLoginSuccess(username, email, token);
                        }
                    }
                });
            }
            else
            {
                if (onLoginSuccess)
                {
                    Log::info("Auth: Calling onLoginSuccess callback");
                    onLoginSuccess(username, email, token);
                }
            }
        }
        else
        {
            Log::warn("Auth: Login failed - " + result.errorMessage);
            showError(result.errorMessage.isEmpty() ? "Invalid email or password" : result.errorMessage);
        }
        repaint();
    });
}

void Auth::handleForgotPassword()
{
    Log::info("Auth: Handling forgot password request");

    // Get email from login form if available
    juce::String email = loginEmailEditor->getText().trim();

    // Note: Backend endpoint POST /api/v1/auth/reset-password is implemented - creates reset token
    // Note: Password reset email flow is now implemented using AWS SES (see backend/internal/email/ses.go)
    // For now, opens browser URL - in production, email would be sent automatically

    juce::String resetUrl;
    if (networkClient != nullptr)
    {
        // Use the same base URL as the network client
        resetUrl = juce::String(Constants::Endpoints::DEV_BASE_URL) + "/reset-password";
        if (!email.isEmpty())
        {
            resetUrl += "?email=" + juce::URL::addEscapeChars(email, true);
        }
    }
    else
    {
        resetUrl = juce::String(Constants::Endpoints::DEV_BASE_URL) + "/reset-password";
    }

    if (!networkClient)
    {
        showError("Network client not available");
        return;
    }

    // Show loading state
    isLoading = true;
    repaint();

    // Request password reset
    networkClient->requestPasswordReset(email, [this, email](Outcome<juce::var> result) {
        isLoading = false;
        repaint();

        if (result.isOk())
        {
            auto response = result.getValue();
            juce::String token;
            if (response.isObject())
            {
                token = response.getProperty("token", "").toString();
            }

            juce::String message = "Password reset email sent to " + email;
            if (token.isNotEmpty())
            {
                // Development mode - show token for testing
                message += "\n\n(Development mode: Reset token: " + token + ")";
            }

            juce::AlertWindow::showMessageBoxAsync(
                juce::MessageBoxIconType::InfoIcon,
                "Password Reset",
                message + "\n\nPlease check your email for reset instructions.");
        }
        else
        {
            showError("Failed to send reset email. Please try again.");
        }
    });
}

void Auth::handleSignup()
{
    Log::info("Auth: Handling signup request");
    auto email = signupEmailEditor->getText().trim();
    auto username = signupUsernameEditor->getText().trim();
    auto displayName = signupDisplayNameEditor->getText().trim();
    auto password = signupPasswordEditor->getText();
    auto confirmPassword = signupConfirmPasswordEditor->getText();

    Log::debug("Auth: Signup attempt - email: " + email + ", username: " + username + ", displayName: " + displayName);

    // Validation
    if (Validate::isBlank(email))
    {
        Log::warn("Auth: Signup validation failed - blank email");
        showError("Please enter your email address");
        signupEmailEditor->grabKeyboardFocus();
        return;
    }

    if (!Validate::isEmail(email))
    {
        Log::warn("Auth: Signup validation failed - invalid email format: " + email);
        showError("Please enter a valid email address");
        signupEmailEditor->grabKeyboardFocus();
        return;
    }

    if (Validate::isBlank(username))
    {
        Log::warn("Auth: Signup validation failed - blank username");
        showError("Please choose a username");
        signupUsernameEditor->grabKeyboardFocus();
        return;
    }

    if (!Validate::isUsername(username))
    {
        Log::warn("Auth: Signup validation failed - invalid username format: " + username);
        showError("Username must be 3-30 characters, letters/numbers/underscores only");
        signupUsernameEditor->grabKeyboardFocus();
        return;
    }

    if (Validate::isBlank(displayName))
    {
        Log::warn("Auth: Signup validation failed - blank display name");
        showError("Please enter your display name");
        signupDisplayNameEditor->grabKeyboardFocus();
        return;
    }

    if (Validate::isBlank(password))
    {
        Log::warn("Auth: Signup validation failed - blank password");
        showError("Please create a password");
        signupPasswordEditor->grabKeyboardFocus();
        return;
    }

    if (!Validate::lengthInRange(password, 8, 128))
    {
        Log::warn("Auth: Signup validation failed - password too short (length: " + juce::String(password.length()) + ")");
        showError("Password must be at least 8 characters");
        signupPasswordEditor->grabKeyboardFocus();
        return;
    }

    if (password != confirmPassword)
    {
        Log::warn("Auth: Signup validation failed - passwords do not match");
        showError("Passwords do not match");
        signupConfirmPasswordEditor->grabKeyboardFocus();
        return;
    }

    Log::debug("Auth: Signup validation passed, initiating API call");

    // Show loading state
    isLoading = true;
    signupSubmitButton->setEnabled(false);
    repaint();

    // Make API call
    if (networkClient == nullptr)
    {
        Log::error("Auth: Cannot signup - NetworkClient is null");
        isLoading = false;
        signupSubmitButton->setEnabled(true);
        showError("Network client not available");
        repaint();
        return;
    }

    Log::info("Auth: Calling NetworkClient::registerAccount - email: " + email + ", username: " + username);
    networkClient->registerAccount(email, username, password, displayName, [this, email, username](Outcome<std::pair<juce::String, juce::String>> authResult) {
        isLoading = false;
        signupSubmitButton->setEnabled(true);

        if (authResult.isOk())
        {
            auto [token, userId] = authResult.getValue();
            Log::info("Auth: Signup successful - email: " + email + ", username: " + username + ", userId: " + userId);
            if (onLoginSuccess)
            {
                Log::info("Auth: Calling onLoginSuccess callback");
                onLoginSuccess(username, email, token);
            }
            else
            {
                Log::warn("Auth: Signup succeeded but onLoginSuccess callback not set");
            }
        }
        else
        {
            Log::warn("Auth: Signup failed for: " + email + " - " + authResult.getError());
            showError("Registration failed. Please try again.");
        }
        repaint();
    });
}

void Auth::handleTwoFactorVerify()
{
    Log::info("Auth: Handling 2FA verification");
    auto code = twoFactorCodeEditor->getText().trim();

    // Validation
    if (code.isEmpty())
    {
        Log::warn("Auth: 2FA validation failed - blank code");
        showError("Please enter the verification code");
        twoFactorCodeEditor->grabKeyboardFocus();
        return;
    }

    if (code.length() != 6 && !code.contains("-"))
    {
        // Allow 6-digit codes or backup codes (which may contain dashes)
        Log::warn("Auth: 2FA validation failed - invalid code format");
        showError("Please enter a 6-digit code or backup code");
        twoFactorCodeEditor->grabKeyboardFocus();
        return;
    }

    if (twoFactorUserId.isEmpty())
    {
        Log::error("Auth: 2FA user ID is empty - cannot verify");
        showError("Session expired. Please try logging in again.");
        showLogin();
        return;
    }

    Log::debug("Auth: 2FA validation passed, initiating API call");

    // Show loading state
    isLoading = true;
    twoFactorVerifyButton->setEnabled(false);
    repaint();

    if (networkClient == nullptr)
    {
        Log::error("Auth: Cannot verify 2FA - NetworkClient is null");
        isLoading = false;
        twoFactorVerifyButton->setEnabled(true);
        showError("Network client not available");
        repaint();
        return;
    }

    Log::info("Auth: Calling NetworkClient::verify2FALogin for userId: " + twoFactorUserId);
    networkClient->verify2FALogin(twoFactorUserId, code, [this](Outcome<std::pair<juce::String, juce::String>> authResult) {
        isLoading = false;
        twoFactorVerifyButton->setEnabled(true);

        if (authResult.isOk())
        {
            auto [token, userId] = authResult.getValue();
            Log::info("Auth: 2FA verification successful for userId: " + userId);

            juce::String username = "";
            juce::String email = "";
            if (networkClient)
            {
                username = networkClient->getCurrentUsername();
                // Note: Email not available from 2FA login response, but we can proceed
                Log::debug("Auth: Retrieved username from NetworkClient: " + username);
            }

            // Clear 2FA state
            twoFactorUserId = "";
            twoFactorType = "";

            if (onLoginSuccess)
            {
                Log::info("Auth: Calling onLoginSuccess callback after 2FA");
                onLoginSuccess(username, email, token);
            }
        }
        else
        {
            Log::warn("Auth: 2FA verification failed: " + authResult.getError());
            showError("Invalid verification code. Please try again.");
            twoFactorCodeEditor->clear();
            twoFactorCodeEditor->grabKeyboardFocus();
        }
        repaint();
    });
}
