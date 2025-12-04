#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
SidechainAudioProcessorEditor::SidechainAudioProcessorEditor(SidechainAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setSize(PLUGIN_WIDTH, PLUGIN_HEIGHT);

    // Initialize network client with development config
    networkClient = std::make_unique<NetworkClient>(NetworkClient::Config::development());

    // Create connection indicator
    connectionIndicator = std::make_unique<ConnectionIndicator>();
    connectionIndicator->onReconnectClicked = [this]() {
        if (networkClient)
            networkClient->checkConnection();
    };
    addAndMakeVisible(connectionIndicator.get());

    // Set up connection status callback
    networkClient->setConnectionStatusCallback([this](NetworkClient::ConnectionStatus status) {
        if (connectionIndicator)
            connectionIndicator->setStatus(status);
    });

    // Check connection on startup
    networkClient->checkConnection();

    //==========================================================================
    // Create AuthComponent
    authComponent = std::make_unique<AuthComponent>();
    authComponent->setNetworkClient(networkClient.get());
    authComponent->onLoginSuccess = [this](const juce::String& user, const juce::String& mail, const juce::String& token) {
        onLoginSuccess(user, mail, token);
    };
    authComponent->onOAuthRequested = [this](const juce::String& provider) {
        // Open OAuth URL in system browser
        juce::String oauthUrl = "http://localhost:8787/api/v1/auth/" + provider;
        juce::URL(oauthUrl).launchInDefaultBrowser();
    };
    addChildComponent(authComponent.get());

    //==========================================================================
    // Create ProfileSetupComponent
    profileSetupComponent = std::make_unique<ProfileSetupComponent>();
    profileSetupComponent->onSkipSetup = [this]() { showView(AppView::PostsFeed); };
    profileSetupComponent->onCompleteSetup = [this]() { showView(AppView::PostsFeed); };
    profileSetupComponent->onProfilePicSelected = [this](const juce::String& localPath) {
        juce::File imageFile(localPath);
        if (imageFile.existsAsFile() && networkClient)
        {
            networkClient->uploadProfilePicture(imageFile, [this, localPath](bool success, const juce::String& s3Url) {
                if (success)
                {
                    profilePicUrl = s3Url;
                    saveLoginState();
                }
                else
                {
                    profilePicUrl = localPath;
                    saveLoginState();
                }
            });
        }
    };
    profileSetupComponent->onLogout = [this]() { logout(); };
    addChildComponent(profileSetupComponent.get());

    //==========================================================================
    // Create PostsFeedComponent
    postsFeedComponent = std::make_unique<PostsFeedComponent>();
    postsFeedComponent->setNetworkClient(networkClient.get());
    postsFeedComponent->setAudioPlayer(&audioProcessor.getAudioPlayer());
    postsFeedComponent->onGoToProfile = [this]() { showView(AppView::ProfileSetup); };
    postsFeedComponent->onLogout = [this]() { logout(); };
    postsFeedComponent->onStartRecording = [this]() { showView(AppView::Recording); };
    addChildComponent(postsFeedComponent.get());

    //==========================================================================
    // Create RecordingComponent
    recordingComponent = std::make_unique<RecordingComponent>(audioProcessor);
    recordingComponent->onRecordingComplete = [this](const juce::AudioBuffer<float>& recordedAudio) {
        if (uploadComponent)
        {
            uploadComponent->setAudioToUpload(recordedAudio, audioProcessor.getCurrentSampleRate());
            showView(AppView::Upload);
        }
    };
    recordingComponent->onRecordingDiscarded = [this]() { showView(AppView::PostsFeed); };
    addChildComponent(recordingComponent.get());

    //==========================================================================
    // Create UploadComponent
    uploadComponent = std::make_unique<UploadComponent>(audioProcessor, *networkClient);
    uploadComponent->onUploadComplete = [this]() {
        uploadComponent->reset();
        showView(AppView::PostsFeed);
    };
    uploadComponent->onCancel = [this]() {
        uploadComponent->reset();
        showView(AppView::Recording);
    };
    addChildComponent(uploadComponent.get());

    //==========================================================================
    // Load persistent state and show appropriate view
    loadLoginState();
}

SidechainAudioProcessorEditor::~SidechainAudioProcessorEditor()
{
}

//==============================================================================
void SidechainAudioProcessorEditor::paint(juce::Graphics& g)
{
    // Dark background - each component handles its own painting
    g.fillAll(juce::Colour::fromRGB(26, 26, 30));
}

void SidechainAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    // Position connection indicator in top-right corner
    if (connectionIndicator)
        connectionIndicator->setBounds(getWidth() - 28, 8, 16, 16);

    // All view components fill the entire window
    if (authComponent)
        authComponent->setBounds(bounds);

    if (profileSetupComponent)
        profileSetupComponent->setBounds(bounds);

    if (postsFeedComponent)
        postsFeedComponent->setBounds(bounds);

    if (recordingComponent)
        recordingComponent->setBounds(bounds);

    if (uploadComponent)
        uploadComponent->setBounds(bounds);
}

//==============================================================================
void SidechainAudioProcessorEditor::showView(AppView view)
{
    // Hide all view components
    if (authComponent)
        authComponent->setVisible(false);
    if (profileSetupComponent)
        profileSetupComponent->setVisible(false);
    if (postsFeedComponent)
        postsFeedComponent->setVisible(false);
    if (recordingComponent)
        recordingComponent->setVisible(false);
    if (uploadComponent)
        uploadComponent->setVisible(false);

    currentView = view;

    switch (view)
    {
        case AppView::Authentication:
            if (authComponent)
            {
                authComponent->reset();
                authComponent->setVisible(true);
            }
            break;

        case AppView::ProfileSetup:
            if (profileSetupComponent)
            {
                profileSetupComponent->setUserInfo(username, email, profilePicUrl);
                profileSetupComponent->setVisible(true);
            }
            break;

        case AppView::PostsFeed:
            if (postsFeedComponent)
            {
                postsFeedComponent->setUserInfo(username, email, profilePicUrl);
                postsFeedComponent->setVisible(true);
                postsFeedComponent->loadFeed();
            }
            break;

        case AppView::Recording:
            if (recordingComponent)
                recordingComponent->setVisible(true);
            break;

        case AppView::Upload:
            if (uploadComponent)
                uploadComponent->setVisible(true);
            break;
    }

    repaint();
}

void SidechainAudioProcessorEditor::onLoginSuccess(const juce::String& user, const juce::String& mail, const juce::String& token)
{
    username = user;
    email = mail;
    authToken = token;

    // Set auth token on network client
    if (networkClient && !token.isEmpty())
        networkClient->setAuthToken(token);

    saveLoginState();
    showView(AppView::ProfileSetup);
}

void SidechainAudioProcessorEditor::logout()
{
    // Clear user state
    username = "";
    email = "";
    profilePicUrl = "";
    authToken = "";

    // Clear network client auth
    if (networkClient)
        networkClient->setAuthToken("");

    // Clear saved state
    auto properties = juce::PropertiesFile::Options();
    properties.applicationName = "Sidechain";
    properties.filenameSuffix = ".settings";
    properties.folderName = "SidechainPlugin";

    auto appProperties = std::make_unique<juce::PropertiesFile>(properties);
    appProperties->setValue("isLoggedIn", false);
    appProperties->removeValue("username");
    appProperties->removeValue("email");
    appProperties->removeValue("profilePicUrl");
    appProperties->removeValue("authToken");
    appProperties->save();

    showView(AppView::Authentication);
}

//==============================================================================
void SidechainAudioProcessorEditor::saveLoginState()
{
    auto properties = juce::PropertiesFile::Options();
    properties.applicationName = "Sidechain";
    properties.filenameSuffix = ".settings";
    properties.folderName = "SidechainPlugin";

    auto appProperties = std::make_unique<juce::PropertiesFile>(properties);

    if (!username.isEmpty())
    {
        appProperties->setValue("isLoggedIn", true);
        appProperties->setValue("username", username);
        appProperties->setValue("email", email);
        appProperties->setValue("profilePicUrl", profilePicUrl);
        appProperties->setValue("authToken", authToken);
    }
    else
    {
        appProperties->setValue("isLoggedIn", false);
    }

    appProperties->save();
}

void SidechainAudioProcessorEditor::loadLoginState()
{
    auto properties = juce::PropertiesFile::Options();
    properties.applicationName = "Sidechain";
    properties.filenameSuffix = ".settings";
    properties.folderName = "SidechainPlugin";

    auto appProperties = std::make_unique<juce::PropertiesFile>(properties);

    bool isLoggedIn = appProperties->getBoolValue("isLoggedIn", false);

    if (isLoggedIn)
    {
        username = appProperties->getValue("username", "");
        email = appProperties->getValue("email", "");
        profilePicUrl = appProperties->getValue("profilePicUrl", "");
        authToken = appProperties->getValue("authToken", "");

        // Set auth token on network client
        if (!authToken.isEmpty() && networkClient)
            networkClient->setAuthToken(authToken);

        showView(AppView::ProfileSetup);
    }
    else
    {
        showView(AppView::Authentication);
    }
}
