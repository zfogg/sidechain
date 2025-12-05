#include "AuthComponent.h"
#include "../../network/NetworkClient.h"
#include "../../util/Validate.h"

//==============================================================================
AuthComponent::AuthComponent()
{
    // Create all UI components BEFORE calling setSize() because setSize() triggers resized()
    setupWelcomeComponents();
    setupLoginComponents();
    setupSignupComponents();

    showWelcome();

    // Set size last - this triggers resized() which requires components to exist
    setSize(1000, 800);
}

AuthComponent::~AuthComponent()
{
}

//==============================================================================
void AuthComponent::setupWelcomeComponents()
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

void AuthComponent::setupLoginComponents()
{
    loginEmailEditor = std::make_unique<juce::TextEditor>();
    styleTextEditor(*loginEmailEditor, "Email address");
    loginEmailEditor->addListener(this);
    addChildComponent(loginEmailEditor.get());

    loginPasswordEditor = std::make_unique<juce::TextEditor>();
    styleTextEditor(*loginPasswordEditor, "Password", true);
    loginPasswordEditor->addListener(this);
    addChildComponent(loginPasswordEditor.get());

    loginSubmitButton = std::make_unique<juce::TextButton>("Sign In");
    stylePrimaryButton(*loginSubmitButton);
    loginSubmitButton->addListener(this);
    addChildComponent(loginSubmitButton.get());

    loginBackButton = std::make_unique<juce::TextButton>("Back");
    styleSecondaryButton(*loginBackButton);
    loginBackButton->addListener(this);
    addChildComponent(loginBackButton.get());
}

void AuthComponent::setupSignupComponents()
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

//==============================================================================
void AuthComponent::styleTextEditor(juce::TextEditor& editor, const juce::String& placeholder, bool isPassword)
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
    editor.setFont(juce::Font(15.0f));
    editor.setJustification(juce::Justification::centredLeft);
    editor.setIndents(16, 0);
}

void AuthComponent::stylePrimaryButton(juce::TextButton& button)
{
    button.setColour(juce::TextButton::buttonColourId, Colors::primaryButton);
    button.setColour(juce::TextButton::buttonOnColourId, Colors::primaryButtonHover);
    button.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    button.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    button.setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void AuthComponent::styleSecondaryButton(juce::TextButton& button)
{
    button.setColour(juce::TextButton::buttonColourId, Colors::secondaryButton);
    button.setColour(juce::TextButton::buttonOnColourId, Colors::secondaryButton.brighter(0.1f));
    button.setColour(juce::TextButton::textColourOffId, Colors::textSecondary);
    button.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    button.setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void AuthComponent::styleOAuthButton(juce::TextButton& button, const juce::String& text, juce::Colour color)
{
    button.setButtonText(text);
    button.setColour(juce::TextButton::buttonColourId, color.withAlpha(0.15f));
    button.setColour(juce::TextButton::buttonOnColourId, color.withAlpha(0.25f));
    button.setColour(juce::TextButton::textColourOffId, color);
    button.setColour(juce::TextButton::textColourOnId, color.brighter(0.2f));
    button.setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

//==============================================================================
void AuthComponent::paint(juce::Graphics& g)
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
    }

    cardBounds = cardBounds.withCentre(getLocalBounds().getCentre());

    // Draw card background
    drawCard(g, cardBounds);

    // Draw logo area
    auto logoArea = cardBounds.removeFromTop(100);
    drawLogo(g, logoArea);

    // Draw title based on mode
    g.setColour(Colors::textPrimary);
    g.setFont(juce::Font(22.0f, juce::Font::bold));

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

    // Draw divider for OAuth in welcome mode
    if (currentMode == AuthMode::Welcome)
    {
        int dividerY = cardBounds.getY() + 160;
        drawDivider(g, dividerY, "or continue with");
    }
}

void AuthComponent::drawCard(juce::Graphics& g, juce::Rectangle<int> bounds)
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

void AuthComponent::drawLogo(juce::Graphics& g, juce::Rectangle<int> bounds)
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
    g.setFont(juce::Font(28.0f, juce::Font::bold));
    g.drawText("Sidechain", bounds.withY(bounds.getBottom() - 30).withHeight(30), juce::Justification::centred);
}

void AuthComponent::drawDivider(juce::Graphics& g, int y, const juce::String& text)
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

//==============================================================================
void AuthComponent::resized()
{
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
            contentBounds.removeFromTop(FIELD_SPACING + 8);
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
            contentBounds.removeFromTop(FIELD_SPACING);
            signupConfirmPasswordEditor->setBounds(contentBounds.removeFromTop(FIELD_HEIGHT));
            contentBounds.removeFromTop(FIELD_SPACING + 8);
            signupSubmitButton->setBounds(contentBounds.removeFromTop(BUTTON_HEIGHT));
            contentBounds.removeFromTop(12);
            signupBackButton->setBounds(contentBounds.removeFromTop(BUTTON_HEIGHT));
            break;
        }
    }
}

//==============================================================================
void AuthComponent::hideAllComponents()
{
    // Welcome components
    loginButton->setVisible(false);
    signupButton->setVisible(false);
    googleButton->setVisible(false);
    discordButton->setVisible(false);

    // Login components
    loginEmailEditor->setVisible(false);
    loginPasswordEditor->setVisible(false);
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
}

void AuthComponent::showWelcome()
{
    currentMode = AuthMode::Welcome;
    hideAllComponents();
    clearError();

    loginButton->setVisible(true);
    signupButton->setVisible(true);
    googleButton->setVisible(true);
    discordButton->setVisible(true);

    resized();
    repaint();
}

void AuthComponent::showLogin()
{
    currentMode = AuthMode::Login;
    hideAllComponents();
    clearError();

    loginEmailEditor->setVisible(true);
    loginPasswordEditor->setVisible(true);
    loginSubmitButton->setVisible(true);
    loginBackButton->setVisible(true);

    loginEmailEditor->clear();
    loginPasswordEditor->clear();
    loginEmailEditor->grabKeyboardFocus();

    resized();
    repaint();
}

void AuthComponent::showSignup()
{
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
}

void AuthComponent::reset()
{
    isLoading = false;
    showWelcome();
}

void AuthComponent::showError(const juce::String& message)
{
    errorMessage = message;
    isLoading = false;
    resized();
    repaint();
}

void AuthComponent::clearError()
{
    errorMessage = "";
    repaint();
}

//==============================================================================
void AuthComponent::buttonClicked(juce::Button* button)
{
    if (button == loginButton.get())
    {
        showLogin();
    }
    else if (button == signupButton.get())
    {
        showSignup();
    }
    else if (button == googleButton.get())
    {
        if (onOAuthRequested)
            onOAuthRequested("google");
    }
    else if (button == discordButton.get())
    {
        if (onOAuthRequested)
            onOAuthRequested("discord");
    }
    else if (button == loginBackButton.get() || button == signupBackButton.get())
    {
        showWelcome();
    }
    else if (button == loginSubmitButton.get())
    {
        handleLogin();
    }
    else if (button == signupSubmitButton.get())
    {
        handleSignup();
    }
}

void AuthComponent::textEditorReturnKeyPressed(juce::TextEditor& editor)
{
    if (currentMode == AuthMode::Login)
    {
        if (&editor == loginEmailEditor.get())
            loginPasswordEditor->grabKeyboardFocus();
        else if (&editor == loginPasswordEditor.get())
            handleLogin();
    }
    else if (currentMode == AuthMode::Signup)
    {
        if (&editor == signupEmailEditor.get())
            signupUsernameEditor->grabKeyboardFocus();
        else if (&editor == signupUsernameEditor.get())
            signupDisplayNameEditor->grabKeyboardFocus();
        else if (&editor == signupDisplayNameEditor.get())
            signupPasswordEditor->grabKeyboardFocus();
        else if (&editor == signupPasswordEditor.get())
            signupConfirmPasswordEditor->grabKeyboardFocus();
        else if (&editor == signupConfirmPasswordEditor.get())
            handleSignup();
    }
}

void AuthComponent::textEditorTextChanged(juce::TextEditor&)
{
    // Clear error when user starts typing
    if (errorMessage.isNotEmpty())
        clearError();
}

//==============================================================================
void AuthComponent::handleLogin()
{
    auto email = loginEmailEditor->getText().trim();
    auto password = loginPasswordEditor->getText();

    // Validation
    if (Validate::isBlank(email))
    {
        showError("Please enter your email address");
        loginEmailEditor->grabKeyboardFocus();
        return;
    }

    if (!Validate::isEmail(email))
    {
        showError("Please enter a valid email address");
        loginEmailEditor->grabKeyboardFocus();
        return;
    }

    if (Validate::isBlank(password))
    {
        showError("Please enter your password");
        loginPasswordEditor->grabKeyboardFocus();
        return;
    }

    // Show loading state
    isLoading = true;
    loginSubmitButton->setEnabled(false);
    repaint();

    // Make API call
    juce::Thread::launch([this, email, password]() {
        juce::var loginData = juce::var(new juce::DynamicObject());
        loginData.getDynamicObject()->setProperty("email", email);
        loginData.getDynamicObject()->setProperty("password", password);

        juce::String jsonData = juce::JSON::toString(loginData);

        juce::URL url("http://localhost:8787/api/v1/auth/login");
        auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                           .withExtraHeaders("Content-Type: application/json\r\n")
                           .withConnectionTimeoutMs(10000);

        url = url.withPOSTData(jsonData);
        auto stream = url.createInputStream(options);

        juce::MessageManager::callAsync([this, stream = std::move(stream), email]() {
            isLoading = false;
            loginSubmitButton->setEnabled(true);

            if (stream != nullptr)
            {
                juce::String response = stream->readEntireStreamAsString();
                juce::var responseData = juce::JSON::parse(response);

                if (responseData.isObject() && responseData.hasProperty("auth"))
                {
                    juce::var authData = responseData["auth"];
                    if (authData.isObject() && authData.hasProperty("token"))
                    {
                        juce::String token = authData["token"].toString();
                        juce::String username = email.upToFirstOccurrenceOf("@", false, false);

                        if (authData.hasProperty("user"))
                        {
                            juce::var userData = authData["user"];
                            if (userData.hasProperty("username"))
                                username = userData["username"].toString();
                        }

                        if (onLoginSuccess)
                            onLoginSuccess(username, email, token);
                        return;
                    }
                }

                juce::String errorMsg = responseData.hasProperty("error")
                                      ? responseData["error"].toString()
                                      : "Invalid email or password";
                showError(errorMsg);
            }
            else
            {
                showError("Unable to connect to server");
            }
            repaint();
        });
    });
}

void AuthComponent::handleSignup()
{
    auto email = signupEmailEditor->getText().trim();
    auto username = signupUsernameEditor->getText().trim();
    auto displayName = signupDisplayNameEditor->getText().trim();
    auto password = signupPasswordEditor->getText();
    auto confirmPassword = signupConfirmPasswordEditor->getText();

    // Validation
    if (Validate::isBlank(email))
    {
        showError("Please enter your email address");
        signupEmailEditor->grabKeyboardFocus();
        return;
    }

    if (!Validate::isEmail(email))
    {
        showError("Please enter a valid email address");
        signupEmailEditor->grabKeyboardFocus();
        return;
    }

    if (Validate::isBlank(username))
    {
        showError("Please choose a username");
        signupUsernameEditor->grabKeyboardFocus();
        return;
    }

    if (!Validate::isUsername(username))
    {
        showError("Username must be 3-30 characters, letters/numbers/underscores only");
        signupUsernameEditor->grabKeyboardFocus();
        return;
    }

    if (Validate::isBlank(displayName))
    {
        showError("Please enter your display name");
        signupDisplayNameEditor->grabKeyboardFocus();
        return;
    }

    if (Validate::isBlank(password))
    {
        showError("Please create a password");
        signupPasswordEditor->grabKeyboardFocus();
        return;
    }

    if (!Validate::lengthInRange(password, 8, 128))
    {
        showError("Password must be at least 8 characters");
        signupPasswordEditor->grabKeyboardFocus();
        return;
    }

    if (password != confirmPassword)
    {
        showError("Passwords do not match");
        signupConfirmPasswordEditor->grabKeyboardFocus();
        return;
    }

    // Show loading state
    isLoading = true;
    signupSubmitButton->setEnabled(false);
    repaint();

    // Make API call
    juce::Thread::launch([this, email, username, displayName, password]() {
        juce::var registerData = juce::var(new juce::DynamicObject());
        registerData.getDynamicObject()->setProperty("email", email);
        registerData.getDynamicObject()->setProperty("username", username);
        registerData.getDynamicObject()->setProperty("display_name", displayName);
        registerData.getDynamicObject()->setProperty("password", password);

        juce::String jsonData = juce::JSON::toString(registerData);

        juce::URL url("http://localhost:8787/api/v1/auth/register");
        auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                           .withExtraHeaders("Content-Type: application/json\r\n")
                           .withConnectionTimeoutMs(10000);

        url = url.withPOSTData(jsonData);
        auto stream = url.createInputStream(options);

        juce::MessageManager::callAsync([this, stream = std::move(stream), email, username]() {
            isLoading = false;
            signupSubmitButton->setEnabled(true);

            if (stream != nullptr)
            {
                juce::String response = stream->readEntireStreamAsString();
                
                // Debug logging
                DBG("Registration response: " + response);
                
                // Handle empty response
                if (response.isEmpty())
                {
                    showError("Server returned empty response");
                    repaint();
                    return;
                }
                
                juce::var responseData = juce::JSON::parse(response);

                // Check if JSON parsing succeeded
                if (!responseData.isObject())
                {
                    // JSON parsing failed - response might be an error string
                    DBG("JSON parse failed, response was: " + response);
                    showError("Invalid server response");
                    repaint();
                    return;
                }

                if (responseData.hasProperty("auth"))
                {
                    juce::var authData = responseData["auth"];
                    if (authData.isObject() && authData.hasProperty("token"))
                    {
                        juce::String token = authData["token"].toString();

                        if (onLoginSuccess)
                            onLoginSuccess(username, email, token);
                        return;
                    }
                }

                // Extract error message from response
                juce::String errorMsg;
                if (responseData.hasProperty("message"))
                    errorMsg = responseData["message"].toString();
                else if (responseData.hasProperty("error"))
                    errorMsg = responseData["error"].toString();
                else
                    errorMsg = "Registration failed";
                    
                showError(errorMsg);
            }
            else
            {
                showError("Unable to connect to server");
            }
            repaint();
        });
    });
}
