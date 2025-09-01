#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
SidechainAudioProcessorEditor::SidechainAudioProcessorEditor (SidechainAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setSize (PLUGIN_WIDTH, PLUGIN_HEIGHT);
    
    DBG("Creating Sidechain Editor with size: " + juce::String(PLUGIN_WIDTH) + "x" + juce::String(PLUGIN_HEIGHT));
    
    // Enable keyboard focus
    setWantsKeyboardFocus(true);
    
    // Status label
    statusLabel = std::make_unique<juce::Label>("status", "🎵 Sidechain - Social VST for Producers");
    statusLabel->setJustificationType(juce::Justification::centred);
    statusLabel->setFont(juce::Font(18.0f));
    statusLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    statusLabel->setColour(juce::Label::backgroundColourId, juce::Colours::darkgrey); // Make background visible
    addAndMakeVisible(statusLabel.get());
    
    DBG("Status label created and added");
    
    // Initialize view components
    profileSetupComponent = std::make_unique<ProfileSetupComponent>();
    profileSetupComponent->onSkipSetup = [this]() { showView(AppView::PostsFeed); };
    profileSetupComponent->onCompleteSetup = [this]() { showView(AppView::PostsFeed); };
    profileSetupComponent->onProfilePicSelected = [this](const juce::String& picUrl) { 
        profilePicUrl = picUrl; 
        saveLoginState(); // Save the updated profile pic
        DBG("Main editor received profile pic: " + picUrl);
    };
    
    postsFeedComponent = std::make_unique<PostsFeedComponent>();
    postsFeedComponent->onGoToProfile = [this]() { showView(AppView::ProfileSetup); };
    
    // Load persistent state after components are initialized
    loadLoginState();
    
    // Connect button with bright colors
    connectButton = std::make_unique<juce::TextButton>("Connect Account");
    connectButton->addListener(this);
    connectButton->setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(0, 212, 255)); // Sidechain blue
    connectButton->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    connectButton->setColour(juce::TextButton::buttonOnColourId, juce::Colour::fromRGB(0, 180, 220));
    addAndMakeVisible(connectButton.get());
    
    DBG("Connect button created and added");
}

SidechainAudioProcessorEditor::~SidechainAudioProcessorEditor()
{
}

//==============================================================================
void SidechainAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Dark theme background  
    g.fillAll (juce::Colour::fromRGB(32, 32, 36));
    
    // Only show auth UI when in Authentication view
    if (currentView != AppView::Authentication)
        return;
    
    // Title text
    g.setColour(juce::Colours::white);
    g.setFont(24.0f);
    g.drawText("🎵 Sidechain", getLocalBounds().removeFromTop(60), juce::Justification::centred);
    
    // Subtitle
    g.setColour(juce::Colours::lightgrey);
    g.setFont(14.0f);
    g.drawText("Social VST for Music Producers", getLocalBounds().withY(60).withHeight(40), juce::Justification::centred);
    
    // Debug info visible in UI
    g.setColour(juce::Colours::yellow);
    g.setFont(12.0f);
    juce::String debugInfo = "Size: " + juce::String(getWidth()) + "x" + juce::String(getHeight());
    debugInfo += " | Components: " + juce::String(statusLabel != nullptr ? "Label✓" : "Label✗");
    debugInfo += " " + juce::String(connectButton != nullptr ? "Button✓" : "Button✗");
    g.drawText(debugInfo, getLocalBounds().withY(100).withHeight(20), juce::Justification::centred);
    
    // Draw UI based on authentication state
    switch (authState)
    {
        case AuthState::Disconnected:
        {
            // Single "Connect Account" button
            auto buttonArea = getLocalBounds().withSizeKeepingCentre(200, 50).withY(150);
            drawFocusedButton(g, buttonArea, "Connect Account", juce::Colour::fromRGB(0, 212, 255), 
                            currentFocusIndex == 0);
            break;
        }
        
        case AuthState::ChoosingMethod:
        {
            // Authentication options
            g.setColour(juce::Colours::lightgrey);
            g.setFont(14.0f);
            g.drawText("Choose how to connect:", getLocalBounds().withY(110).withHeight(20), juce::Justification::centred);
            
            // Email Login button
            auto emailButton = getButtonArea(0, 4);
            drawFocusedButton(g, emailButton, "📧 Login with Email", juce::Colour::fromRGB(0, 212, 255), 
                            currentFocusIndex == 0);
            
            // Email Signup button  
            auto signupButton = getButtonArea(1, 4);
            drawFocusedButton(g, signupButton, "✨ Create Account", juce::Colour::fromRGB(0, 180, 216), 
                            currentFocusIndex == 1);
            
            // Google OAuth button
            auto googleButton = getButtonArea(2, 4);
            drawFocusedButton(g, googleButton, "🔗 Google", juce::Colour::fromRGB(234, 67, 53), 
                            currentFocusIndex == 2);
            
            // Discord OAuth button
            auto discordButton = getButtonArea(3, 4);
            drawFocusedButton(g, discordButton, "🎮 Discord", juce::Colour::fromRGB(88, 101, 242), 
                            currentFocusIndex == 3);
            break;
        }
        
        case AuthState::EmailLogin:
        {
            // Login form with proper spacing
            g.setColour(juce::Colours::white);
            g.setFont(18.0f);
            g.drawText("Login to Sidechain", getLocalBounds().withY(80).withHeight(30), juce::Justification::centred);
            
            // Form fields with proper spacing (only email and password for login)
            auto formArea = getLocalBounds().withSizeKeepingCentre(350, 150).withY(120);
            const int fieldSpacing = 8;
            const int fieldHeight = 35;
            
            auto emailArea = formArea.removeFromTop(fieldHeight);
            drawFormField(g, "Email:", email, emailArea, false, activeField == 0);
            formArea.removeFromTop(fieldSpacing);
            
            auto passwordArea = formArea.removeFromTop(fieldHeight);
            drawFormField(g, "Password:", password, passwordArea, true, activeField == 1);
            
            // Buttons with proper spacing below form
            auto buttonY = 120 + 2 * (fieldHeight + fieldSpacing) + 20; // Below email and password fields
            const int buttonWidth = 120;
            const int buttonSpacing = 20;
            auto submitButton = juce::Rectangle<int>(getWidth()/2 - buttonWidth - buttonSpacing/2, buttonY, buttonWidth, 36);
            auto cancelButton = juce::Rectangle<int>(getWidth()/2 + buttonSpacing/2, buttonY, buttonWidth, 36);
            
            // Submit button (focus index 2 for EmailLogin)
            drawFocusedButton(g, submitButton, "Login", juce::Colour::fromRGB(40, 167, 69), 
                            currentFocusIndex == 2);
            
            // Cancel button (focus index 3 for EmailLogin)
            drawFocusedButton(g, cancelButton, "Cancel", juce::Colour::fromRGB(108, 117, 125), 
                            currentFocusIndex == 3);
            break;
        }
        
        case AuthState::EmailSignup:
        {
            // Signup form with proper spacing
            g.setColour(juce::Colours::white);
            g.setFont(18.0f);
            g.drawText("Create Sidechain Account", getLocalBounds().withY(80).withHeight(30), juce::Justification::centred);
            
            // Form fields with proper spacing
            auto formArea = getLocalBounds().withSizeKeepingCentre(350, 250).withY(120);
            const int fieldSpacing = 8;
            const int fieldHeight = 35;
            
            auto emailArea = formArea.removeFromTop(fieldHeight);
            drawFormField(g, "Email:", email, emailArea, false, activeField == 0);
            formArea.removeFromTop(fieldSpacing);
            
            auto usernameArea = formArea.removeFromTop(fieldHeight);
            drawFormField(g, "Username:", username, usernameArea, false, activeField == 1);
            formArea.removeFromTop(fieldSpacing);
            
            auto displayNameArea = formArea.removeFromTop(fieldHeight);
            drawFormField(g, "Display Name:", displayName, displayNameArea, false, activeField == 2);
            formArea.removeFromTop(fieldSpacing);
            
            auto passwordArea = formArea.removeFromTop(fieldHeight);
            drawFormField(g, "Password:", password, passwordArea, true, activeField == 3);
            formArea.removeFromTop(fieldSpacing);
            
            auto confirmPasswordArea = formArea.removeFromTop(fieldHeight);
            drawFormField(g, "Confirm:", confirmPassword, confirmPasswordArea, true, activeField == 4);
            
            // Buttons with proper spacing below form
            auto buttonY = 120 + 5 * (fieldHeight + fieldSpacing) + 20; // Below all fields
            const int buttonWidth = 120;
            const int buttonSpacing = 20;
            auto submitButton = juce::Rectangle<int>(getWidth()/2 - buttonWidth - buttonSpacing/2, buttonY, buttonWidth, 36);
            auto cancelButton = juce::Rectangle<int>(getWidth()/2 + buttonSpacing/2, buttonY, buttonWidth, 36);
            
            // Submit button (focus index 5 for EmailSignup)
            drawFocusedButton(g, submitButton, "Create Account", juce::Colour::fromRGB(40, 167, 69), 
                            currentFocusIndex == 5);
            
            // Cancel button (focus index 6 for EmailSignup)
            drawFocusedButton(g, cancelButton, "Cancel", juce::Colour::fromRGB(108, 117, 125), 
                            currentFocusIndex == 6);
            break;
        }
        
        case AuthState::Connecting:
        {
            // Connecting state
            auto buttonArea = getLocalBounds().withSizeKeepingCentre(200, 50).withY(150);
            g.setColour(juce::Colour::fromRGB(255, 193, 7)); // Yellow
            g.fillRoundedRectangle(buttonArea.toFloat(), 8.0f);
            
            g.setColour(juce::Colours::black);
            g.setFont(16.0f);
            g.drawText("Connecting...", buttonArea, juce::Justification::centred);
            break;
        }
        
        case AuthState::Error:
        {
            // Error state with retry option
            g.setColour(juce::Colours::red);
            g.setFont(14.0f);
            g.drawText("❌ " + errorMessage, getLocalBounds().withY(120).withHeight(25), juce::Justification::centred);
            
            auto retryButton = getLocalBounds().withSizeKeepingCentre(150, 40).withY(160);
            g.setColour(juce::Colour::fromRGB(220, 53, 69)); // Red
            g.fillRoundedRectangle(retryButton.toFloat(), 6.0f);
            
            g.setColour(juce::Colours::white);
            g.setFont(14.0f);
            g.drawText("Try Again", retryButton, juce::Justification::centred);
            break;
        }
        
        case AuthState::Connected:
        {
            // Connected state
            auto buttonArea = getLocalBounds().withSizeKeepingCentre(250, 50).withY(150);
            g.setColour(juce::Colour::fromRGB(40, 167, 69)); // Green
            g.fillRoundedRectangle(buttonArea.toFloat(), 8.0f);
            
            g.setColour(juce::Colours::white);
            g.setFont(16.0f);
            g.drawText("✅ Connected as " + (username.isEmpty() ? "DemoUser" : username), buttonArea, juce::Justification::centred);
            break;
        }
        
        default:
            break;
    }
}

void SidechainAudioProcessorEditor::resized()
{
    DBG("Editor resized to: " + juce::String(getWidth()) + "x" + juce::String(getHeight()));
    
    auto bounds = getLocalBounds();
    DBG("Local bounds: " + juce::String(bounds.getWidth()) + "x" + juce::String(bounds.getHeight()));
    
    bounds.reduce(20, 20);
    
    // Status label at top (with null check)
    if (statusLabel != nullptr)
    {
        auto labelBounds = bounds.removeFromTop(40);
        statusLabel->setBounds(labelBounds);
        DBG("Status label bounds: " + juce::String(labelBounds.getX()) + "," + juce::String(labelBounds.getY()) + " " + 
            juce::String(labelBounds.getWidth()) + "x" + juce::String(labelBounds.getHeight()));
    }
    
    bounds.removeFromTop(20); // spacing
    
    // Center the connect button (with null check)
    if (connectButton != nullptr)
    {
        auto buttonArea = bounds.removeFromTop(40);
        auto buttonWidth = 200;
        auto buttonX = (buttonArea.getWidth() - buttonWidth) / 2;
        auto buttonBounds = juce::Rectangle<int>(buttonX, buttonArea.getY(), buttonWidth, buttonArea.getHeight());
        connectButton->setBounds(buttonBounds);
        DBG("Connect button bounds: " + juce::String(buttonBounds.getX()) + "," + juce::String(buttonBounds.getY()) + " " + 
            juce::String(buttonBounds.getWidth()) + "x" + juce::String(buttonBounds.getHeight()));
    }
}

void SidechainAudioProcessorEditor::buttonClicked (juce::Button* button)
{
    if (button == connectButton.get() && connectButton != nullptr)
    {
        if (connectButton->getButtonText() == "Connect Account")
        {
            showAuthenticationDialog();
        }
        else
        {
            // Disconnect
            connectButton->setButtonText("Connect Account");
            if (statusLabel != nullptr)
                statusLabel->setText("🎵 Sidechain - Social VST for Producers", juce::dontSendNotification);
        }
    }
}

void SidechainAudioProcessorEditor::mouseUp (const juce::MouseEvent& event)
{
    switch (authState)
    {
        case AuthState::Disconnected:
        {
            auto buttonArea = getLocalBounds().withSizeKeepingCentre(200, 50).withY(150);
            if (buttonArea.contains(event.getPosition()))
            {
                authState = AuthState::ChoosingMethod;
                repaint();
            }
            break;
        }
        
        case AuthState::ChoosingMethod:
        {
            // Check which authentication method was clicked
            auto emailButton = getButtonArea(0, 4);
            auto signupButton = getButtonArea(1, 4);
            auto googleButton = getButtonArea(2, 4);
            auto discordButton = getButtonArea(3, 4);
            
            if (emailButton.contains(event.getPosition()))
            {
                authState = AuthState::EmailLogin;
                email = ""; // Clear fields for login
                password = "";
                activeField = -1;
                repaint();
            }
            else if (signupButton.contains(event.getPosition()))
            {
                authState = AuthState::EmailSignup;
                email = "";
                username = "";
                displayName = "";
                password = "";
                confirmPassword = "";
                repaint();
            }
            else if (googleButton.contains(event.getPosition()))
            {
                handleOAuthLogin("google");
            }
            else if (discordButton.contains(event.getPosition()))
            {
                handleOAuthLogin("discord");
            }
            break;
        }
        
        case AuthState::EmailLogin:
        {
            // Handle login form clicks
            const int fieldHeight = 35;
            const int fieldSpacing = 8;
            auto buttonY = 120 + 2 * (fieldHeight + fieldSpacing) + 20; // Below email and password fields
            const int buttonWidth = 120;
            const int buttonSpacing = 20;
            auto submitButton = juce::Rectangle<int>(getWidth()/2 - buttonWidth - buttonSpacing/2, buttonY, buttonWidth, 36);
            auto cancelButton = juce::Rectangle<int>(getWidth()/2 + buttonSpacing/2, buttonY, buttonWidth, 36);
            
            if (submitButton.contains(event.getPosition()))
            {
                handleEmailLogin();
            }
            else if (cancelButton.contains(event.getPosition()))
            {
                authState = AuthState::ChoosingMethod;
                activeField = -1;
                repaint();
            }
            else
            {
                // Check if clicked on form fields to make them active (only email and password for login)
                auto formArea = getLocalBounds().withSizeKeepingCentre(350, 150).withY(120);
                
                for (int i = 0; i < 2; ++i) // Only 2 fields for login: email (0) and password (1)
                {
                    auto fieldArea = formArea.withY(120 + i * (fieldHeight + fieldSpacing)).withHeight(fieldHeight);
                    if (fieldArea.contains(event.getPosition()))
                    {
                        activeField = i;
                        setWantsKeyboardFocus(true);
                        grabKeyboardFocus();
                        repaint();
                        break;
                    }
                }
            }
            break;
        }
        
        case AuthState::EmailSignup:
        {
            // Handle signup form clicks with corrected button positions
            const int fieldHeight = 35;
            const int fieldSpacing = 8;
            auto buttonY = 120 + 5 * (fieldHeight + fieldSpacing) + 20;
            const int buttonWidth = 120;
            const int buttonSpacing = 20;
            auto submitButton = juce::Rectangle<int>(getWidth()/2 - buttonWidth - buttonSpacing/2, buttonY, buttonWidth, 36);
            auto cancelButton = juce::Rectangle<int>(getWidth()/2 + buttonSpacing/2, buttonY, buttonWidth, 36);
            
            if (submitButton.contains(event.getPosition()))
            {
                handleEmailSignup();
            }
            else if (cancelButton.contains(event.getPosition()))
            {
                authState = AuthState::ChoosingMethod;
                activeField = -1;
                repaint();
            }
            else
            {
                // Check if clicked on form fields to make them active
                auto formArea = getLocalBounds().withSizeKeepingCentre(350, 250).withY(120);
                
                for (int i = 0; i < 5; ++i)
                {
                    auto fieldArea = formArea.withY(120 + i * (fieldHeight + fieldSpacing)).withHeight(fieldHeight);
                    if (fieldArea.contains(event.getPosition()))
                    {
                        activeField = i;
                        setWantsKeyboardFocus(true);
                        grabKeyboardFocus();
                        repaint();
                        break;
                    }
                }
            }
            break;
        }
        
        case AuthState::Connected:
        {
            // Click to disconnect
            auto buttonArea = getLocalBounds().withSizeKeepingCentre(250, 50).withY(150);
            if (buttonArea.contains(event.getPosition()))
            {
                authState = AuthState::Disconnected;
                username = "";
                repaint();
            }
            break;
        }
        
        default:
            break;
    }
}

bool SidechainAudioProcessorEditor::keyPressed (const juce::KeyPress& key)
{
    // Handle global keys first
    if (key.getKeyCode() == juce::KeyPress::tabKey)
    {
        handleTabNavigation(key.getModifiers().isShiftDown());
        return true;
    }
    
    if (key.getKeyCode() == juce::KeyPress::returnKey)
    {
        return handleEnterKey();
    }
    
    // Handle text input for form fields
    if ((authState != AuthState::EmailSignup && authState != AuthState::EmailLogin) || activeField == -1)
        return false;
    
    juce::String* targetField = nullptr;
    
    // Select which field to edit based on activeField and auth state
    if (authState == AuthState::EmailLogin)
    {
        // Login mode: only email (0) and password (1)
        switch (activeField)
        {
            case 0: targetField = &email; break;
            case 1: targetField = &password; break;
            default: return false;
        }
    }
    else // EmailSignup
    {
        // Signup mode: email (0), username (1), displayName (2), password (3), confirmPassword (4)
        switch (activeField)
        {
            case 0: targetField = &email; break;
            case 1: targetField = &username; break;
            case 2: targetField = &displayName; break;
            case 3: targetField = &password; break;
            case 4: targetField = &confirmPassword; break;
            default: return false;
        }
    }
    
    // Handle key input
    if (key.getKeyCode() == juce::KeyPress::backspaceKey)
    {
        if (targetField->isNotEmpty())
        {
            *targetField = targetField->dropLastCharacters(1);
            repaint();
        }
    }
    else if (key.getKeyCode() == juce::KeyPress::returnKey)
    {
        // Move to next field or submit
        activeField++;
        if (authState == AuthState::EmailLogin)
        {
            if (activeField >= 2) // Login has only 2 fields
            {
                handleEmailLogin();
            }
            else
            {
                repaint();
            }
        }
        else // EmailSignup
        {
            if (activeField >= 5) // Signup has 5 fields
            {
                handleEmailSignup();
            }
            else
            {
                repaint();
            }
        }
    }
    else if (key.isKeyCurrentlyDown(juce::KeyPress::escapeKey))
    {
        activeField = -1;
        repaint();
    }
    else if (key.getTextCharacter() != 0 && key.getTextCharacter() > 31)
    {
        juce::String keyChar = juce::String::charToString(key.getTextCharacter());
        *targetField += keyChar;
        repaint();
    }
    
    return true;
}

void SidechainAudioProcessorEditor::showAuthenticationDialog()
{
    // For now, simulate email/password authentication
    authState = AuthState::Connecting;
    repaint();
    
    // Simulate authentication delay
    juce::Timer::callAfterDelay(1500, [this]() {
        authState = AuthState::Connected;
        username = "EmailUser";
        repaint();
    });
}

void SidechainAudioProcessorEditor::handleOAuthLogin(const juce::String& provider)
{
    authState = AuthState::Connecting;
    repaint();
    
    // Open OAuth URL in system browser
    juce::String oauthUrl = "http://localhost:8787/api/v1/auth/" + provider;
    juce::URL(oauthUrl).launchInDefaultBrowser();
    
    // Simulate OAuth completion
    juce::Timer::callAfterDelay(2000, [this, provider]() {
        authState = AuthState::Connected;
        username = provider == "google" ? "GoogleUser" : "DiscordUser";
        repaint();
    });
}

void SidechainAudioProcessorEditor::showSignupDialog()
{
    authState = AuthState::EmailSignup;
    repaint();
}

void SidechainAudioProcessorEditor::onLoginSuccess()
{
    // Hide auth UI and show profile setup
    showView(AppView::ProfileSetup);
}

void SidechainAudioProcessorEditor::showView(AppView view)
{
    // Hide all view components first
    if (profileSetupComponent)
        removeChildComponent(profileSetupComponent.get());
    if (postsFeedComponent)
        removeChildComponent(postsFeedComponent.get());
    
    // Hide or show auth UI based on view
    bool showAuthUI = (view == AppView::Authentication);
    statusLabel->setVisible(showAuthUI);
    if (connectButton)
        connectButton->setVisible(showAuthUI);
    
    currentView = view;
    
    switch (view)
    {
        case AppView::Authentication:
            // Auth UI already handled by existing paint method
            break;
            
        case AppView::ProfileSetup:
            if (profileSetupComponent)
            {
                profileSetupComponent->setUserInfo(username, email, profilePicUrl);
                addAndMakeVisible(profileSetupComponent.get());
                profileSetupComponent->setBounds(getLocalBounds());
            }
            else
            {
                DBG("ProfileSetupComponent is null!");
            }
            break;
            
        case AppView::PostsFeed:
            if (postsFeedComponent)
            {
                postsFeedComponent->setUserInfo(username, email, profilePicUrl);
                addAndMakeVisible(postsFeedComponent.get());
                postsFeedComponent->setBounds(getLocalBounds());
            }
            else
            {
                DBG("PostsFeedComponent is null!");
            }
            break;
    }
    
    repaint();
}

void SidechainAudioProcessorEditor::saveLoginState()
{
    // Save login state to application properties
    auto properties = juce::PropertiesFile::Options();
    properties.applicationName = "Sidechain";
    properties.filenameSuffix = ".settings";
    properties.folderName = "SidechainPlugin";
    
    auto appProperties = std::make_unique<juce::PropertiesFile>(properties);
    
    if (authState == AuthState::Connected)
    {
        appProperties->setValue("isLoggedIn", true);
        appProperties->setValue("username", username);
        appProperties->setValue("email", email);
        appProperties->setValue("profilePicUrl", profilePicUrl);
    }
    else
    {
        appProperties->setValue("isLoggedIn", false);
    }
    
    appProperties->save();
}

void SidechainAudioProcessorEditor::loadLoginState()
{
    // Load login state from application properties
    auto properties = juce::PropertiesFile::Options();
    properties.applicationName = "Sidechain";
    properties.filenameSuffix = ".settings";
    properties.folderName = "SidechainPlugin";
    
    auto appProperties = std::make_unique<juce::PropertiesFile>(properties);
    
    bool isLoggedIn = appProperties->getBoolValue("isLoggedIn", false);
    
    if (isLoggedIn)
    {
        // Restore user data
        username = appProperties->getValue("username", "");
        email = appProperties->getValue("email", "");
        profilePicUrl = appProperties->getValue("profilePicUrl", "");
        
        // Set logged in state and show profile setup
        authState = AuthState::Connected;
        showView(AppView::ProfileSetup);
    }
    else
    {
        // Start with authentication view
        authState = AuthState::Disconnected;
        currentView = AppView::Authentication;
    }
}

void SidechainAudioProcessorEditor::updateFocusIndicators()
{
    // Update maxFocusIndex based on current auth state
    switch (authState)
    {
        case AuthState::Disconnected:
            maxFocusIndex = 1; // Just the Connect Account button
            break;
            
        case AuthState::ChoosingMethod:
            maxFocusIndex = 4; // Email Login, Signup, Google, Discord buttons
            break;
            
        case AuthState::EmailLogin:
            maxFocusIndex = 4; // Email field, Password field, Login button, Cancel button
            break;
            
        case AuthState::EmailSignup:
            maxFocusIndex = 7; // 5 fields + 2 buttons
            break;
            
        default:
            maxFocusIndex = 0;
            break;
    }
    
    // Clamp current focus index
    if (currentFocusIndex >= maxFocusIndex)
        currentFocusIndex = maxFocusIndex - 1;
}

void SidechainAudioProcessorEditor::handleTabNavigation(bool reverse)
{
    updateFocusIndicators();
    
    if (maxFocusIndex <= 0)
        return;
        
    if (reverse)
    {
        currentFocusIndex--;
        if (currentFocusIndex < 0)
            currentFocusIndex = maxFocusIndex - 1;
    }
    else
    {
        currentFocusIndex++;
        if (currentFocusIndex >= maxFocusIndex)
            currentFocusIndex = 0;
    }
    
    // Update active field for form inputs
    if (authState == AuthState::EmailLogin)
    {
        if (currentFocusIndex < 2)
            activeField = currentFocusIndex; // 0=email, 1=password
        else
            activeField = -1; // buttons
    }
    else if (authState == AuthState::EmailSignup)
    {
        if (currentFocusIndex < 5)
            activeField = currentFocusIndex; // 0-4 = form fields
        else
            activeField = -1; // buttons
    }
    
    repaint();
}

bool SidechainAudioProcessorEditor::handleEnterKey()
{
    updateFocusIndicators();
    
    // Handle Enter key based on current focus and auth state
    switch (authState)
    {
        case AuthState::Disconnected:
            if (currentFocusIndex == 0)
            {
                authState = AuthState::ChoosingMethod;
                currentFocusIndex = 0;
                repaint();
                return true;
            }
            break;
            
        case AuthState::ChoosingMethod:
            if (currentFocusIndex == 0)
            {
                // Email Login
                authState = AuthState::EmailLogin;
                email = "";
                password = "";
                activeField = -1;
                currentFocusIndex = 0;
                repaint();
            }
            else if (currentFocusIndex == 1)
            {
                // Email Signup  
                authState = AuthState::EmailSignup;
                email = "";
                username = "";
                displayName = "";
                password = "";
                confirmPassword = "";
                activeField = -1;
                currentFocusIndex = 0;
                repaint();
            }
            return true;
            
        case AuthState::EmailLogin:
            if (currentFocusIndex == 2) // Login button
            {
                handleEmailLogin();
            }
            else if (currentFocusIndex == 3) // Cancel button
            {
                authState = AuthState::ChoosingMethod;
                activeField = -1;
                currentFocusIndex = 0;
                repaint();
            }
            else
            {
                // Submit form from any input field
                handleEmailLogin();
            }
            return true;
            
        case AuthState::EmailSignup:
            if (currentFocusIndex == 5) // Submit button
            {
                handleEmailSignup();
            }
            else if (currentFocusIndex == 6) // Cancel button
            {
                authState = AuthState::ChoosingMethod;
                activeField = -1;
                currentFocusIndex = 0;
                repaint();
            }
            else
            {
                // Submit form from any input field
                handleEmailSignup();
            }
            return true;
            
        default:
            break;
    }
    
    return false;
}

void SidechainAudioProcessorEditor::handleEmailLogin()
{
    // Validate form fields
    if (email.isEmpty() || password.isEmpty())
    {
        errorMessage = "Email and password are required";
        authState = AuthState::Error;
        repaint();
        return;
    }
    
    // Show connecting state
    authState = AuthState::Connecting;
    repaint();
    
    // Make real API call to backend
    juce::Thread::launch([this]() {
        juce::var loginData = juce::var(new juce::DynamicObject());
        loginData.getDynamicObject()->setProperty("email", email);
        loginData.getDynamicObject()->setProperty("password", password);
        
        juce::String jsonData = juce::JSON::toString(loginData);
        
        // Create HTTP request
        juce::URL url("http://localhost:8787/api/v1/auth/login");
        auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                           .withExtraHeaders("Content-Type: application/json\r\n")
                           .withConnectionTimeoutMs(10000);
        
        url = url.withPOSTData(jsonData);
        auto stream = url.createInputStream(options);
        
        if (stream != nullptr)
        {
            juce::String response = stream->readEntireStreamAsString();
            
            juce::MessageManager::callAsync([this, response]() {
                // Parse response JSON
                juce::var responseData = juce::JSON::parse(response);
                if (responseData.isObject() && responseData.hasProperty("auth"))
                {
                    // Success - extract token and user info from auth object
                    juce::var authData = responseData["auth"];
                    if (authData.isObject() && authData.hasProperty("token"))
                    {
                        // Extract username from user object or use email
                        if (authData.hasProperty("user"))
                        {
                            juce::var userData = authData["user"];
                            username = userData.hasProperty("username") 
                                      ? userData["username"].toString() 
                                      : email.upToFirstOccurrenceOf("@", false, false);
                        }
                        else
                        {
                            username = email.upToFirstOccurrenceOf("@", false, false);
                        }
                        authState = AuthState::Connected;
                        saveLoginState(); // Save persistent state
                        onLoginSuccess();
                    }
                    else
                    {
                        errorMessage = "Invalid authentication response";
                        authState = AuthState::Error;
                    }
                }
                else
                {
                    // Login failed
                    errorMessage = responseData.hasProperty("error") 
                                  ? responseData["error"].toString() 
                                  : "Login failed";
                    authState = AuthState::Error;
                }
                repaint();
            });
        }
        else
        {
            juce::MessageManager::callAsync([this]() {
                // Connection failed
                errorMessage = "Connection to server failed";
                authState = AuthState::Error;
                repaint();
            });
        }
    });
}

void SidechainAudioProcessorEditor::handleEmailSignup()
{
    // Validate form fields
    if (email.isEmpty() || username.isEmpty() || password.isEmpty() || confirmPassword.isEmpty() || displayName.isEmpty())
    {
        errorMessage = "All fields are required";
        authState = AuthState::Error;
        repaint();
        return;
    }
    
    if (password != confirmPassword)
    {
        errorMessage = "Passwords do not match";
        authState = AuthState::Error;
        repaint();
        return;
    }
    
    if (password.length() < 8)
    {
        errorMessage = "Password must be at least 8 characters";
        authState = AuthState::Error;
        repaint();
        return;
    }
    
    // Show connecting state
    authState = AuthState::Connecting;
    repaint();
    
    // Make real API call to backend
    juce::Thread::launch([this]() {
        juce::var registerData = juce::var(new juce::DynamicObject());
        registerData.getDynamicObject()->setProperty("email", email);
        registerData.getDynamicObject()->setProperty("username", username);
        registerData.getDynamicObject()->setProperty("password", password);
        registerData.getDynamicObject()->setProperty("display_name", displayName);
        
        juce::String jsonData = juce::JSON::toString(registerData);
        
        // Create HTTP request
        juce::URL url("http://localhost:8787/api/v1/auth/register");
        auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                           .withExtraHeaders("Content-Type: application/json\r\n")
                           .withConnectionTimeoutMs(10000);
        
        url = url.withPOSTData(jsonData);
        auto stream = url.createInputStream(options);
        
        if (stream != nullptr)
        {
            auto response = stream->readEntireStreamAsString();
            auto result = juce::JSON::parse(response);
            
            // Handle response on message thread
            juce::MessageManager::callAsync([this, result]() {
                if (result.isObject())
                {
                    auto authData = result.getProperty("auth", juce::var());
                    if (authData.isObject())
                    {
                        auto user = authData.getProperty("user", juce::var());
                        if (user.isObject())
                        {
                            // Success!
                            username = user.getProperty("username", "").toString();
                            authState = AuthState::Connected;
                            saveLoginState(); // Save persistent state
                            onLoginSuccess();
                            return;
                        }
                    }
                }
                
                // Error response
                errorMessage = result.getProperty("message", "Registration failed").toString();
                authState = AuthState::Error;
                repaint();
            });
        }
        else
        {
            // Network error
            juce::MessageManager::callAsync([this]() {
                errorMessage = "Cannot connect to Sidechain server";
                authState = AuthState::Error;
                repaint();
            });
        }
    });
}

juce::Rectangle<int> SidechainAudioProcessorEditor::getButtonArea(int index, int totalButtons) const
{
    const int buttonWidth = totalButtons == 4 ? 130 : 180; // Smaller buttons for 4 options
    const int buttonHeight = 35;
    const int spacing = 8;
    const int startY = 140;
    
    int totalWidth = (buttonWidth * totalButtons) + (spacing * (totalButtons - 1));
    int startX = (getWidth() - totalWidth) / 2;
    
    int x = startX + index * (buttonWidth + spacing);
    
    return juce::Rectangle<int>(x, startY, buttonWidth, buttonHeight);
}

void SidechainAudioProcessorEditor::drawFormField(juce::Graphics& g, const juce::String& label, 
                                                 const juce::String& value, juce::Rectangle<int> area, bool isPassword, bool isActive)
{
    // Label
    g.setColour(isActive ? juce::Colours::white : juce::Colours::lightgrey);
    g.setFont(12.0f);
    g.drawText(label, area.removeFromLeft(100), juce::Justification::centredLeft);
    
    // Field background (brighter when active)
    g.setColour(isActive ? juce::Colour::fromRGB(60, 60, 64) : juce::Colour::fromRGB(50, 50, 54));
    g.fillRoundedRectangle(area.toFloat(), 4.0f);
    
    // Field border (blue when active)
    g.setColour(isActive ? juce::Colour::fromRGB(0, 212, 255) : juce::Colour::fromRGB(100, 100, 104));
    g.drawRoundedRectangle(area.toFloat(), 4.0f, isActive ? 2.0f : 1.0f);
    
    // Field value
    g.setColour(juce::Colours::white);
    g.setFont(13.0f);
    juce::String displayValue = isPassword ? juce::String::repeatedString("•", value.length()) : value;
    
    // Add cursor for active field
    if (isActive && displayValue.isEmpty())
    {
        displayValue = "|"; // Show cursor when empty and active
    }
    else if (isActive)
    {
        displayValue += "|"; // Show cursor at end when active
    }
    
    g.drawText(displayValue, area.reduced(8, 0), juce::Justification::centredLeft);
}

void SidechainAudioProcessorEditor::drawFocusedButton(juce::Graphics& g, juce::Rectangle<int> area, 
                                                     const juce::String& text, juce::Colour bgColor, bool isFocused)
{
    // Button background
    g.setColour(bgColor);
    g.fillRoundedRectangle(area.toFloat(), 8.0f);
    
    // Focus indicator
    if (isFocused)
    {
        g.setColour(juce::Colour::fromRGB(0, 212, 255)); // Bright blue focus ring
        g.drawRoundedRectangle(area.expanded(2).toFloat(), 10.0f, 3.0f);
    }
    
    // Button text
    g.setColour(juce::Colours::white);
    g.setFont(15.0f);
    g.drawText(text, area, juce::Justification::centred);
}

