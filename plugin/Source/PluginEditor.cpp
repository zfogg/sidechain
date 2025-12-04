#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
SidechainAudioProcessorEditor::SidechainAudioProcessorEditor(SidechainAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setSize(PLUGIN_WIDTH, PLUGIN_HEIGHT);

    // Initialize network client with development config
    networkClient = std::make_unique<NetworkClient>(NetworkClient::Config::development());

    // Initialize WebSocket client
    webSocketClient = std::make_unique<WebSocketClient>(WebSocketClient::Config::development());
    webSocketClient->onMessage = [this](const WebSocketClient::Message& msg) {
        handleWebSocketMessage(msg);
    };
    webSocketClient->onStateChanged = [this](WebSocketClient::ConnectionState wsState) {
        handleWebSocketStateChange(wsState);
    };
    webSocketClient->onError = [](const juce::String& error) {
        DBG("WebSocket error: " + error);
    };

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
    postsFeedComponent->onGoToDiscovery = [this]() { showView(AppView::Discovery); };
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
    // Create UserDiscoveryComponent
    userDiscoveryComponent = std::make_unique<UserDiscoveryComponent>();
    userDiscoveryComponent->setNetworkClient(networkClient.get());
    userDiscoveryComponent->onBackPressed = [this]() {
        showView(AppView::PostsFeed);
    };
    userDiscoveryComponent->onUserSelected = [this](const DiscoveredUser& user) {
        // Navigate to user profile - could show ProfileComponent with user data
        DBG("User selected: " + user.username);
        // For now, just go back to feed
        showView(AppView::PostsFeed);
    };
    addChildComponent(userDiscoveryComponent.get());

    //==========================================================================
    // Setup notifications
    setupNotifications();

    //==========================================================================
    // Load persistent state and show appropriate view
    loadLoginState();
}

SidechainAudioProcessorEditor::~SidechainAudioProcessorEditor()
{
    // Stop notification polling
    stopNotificationPolling();

    // Disconnect WebSocket before destruction
    if (webSocketClient)
        webSocketClient->disconnect();
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

    // Position notification bell in top-right corner (to the left of connection indicator)
    if (notificationBell)
        notificationBell->setBounds(getWidth() - 70, 4, NotificationBellComponent::PREFERRED_SIZE, NotificationBellComponent::PREFERRED_SIZE);

    // Position connection indicator in top-right corner
    if (connectionIndicator)
        connectionIndicator->setBounds(getWidth() - 28, 8, 16, 16);

    // Position notification panel as dropdown from bell
    if (notificationList)
    {
        int panelX = getWidth() - NotificationListComponent::PREFERRED_WIDTH - 10;
        int panelY = 40;
        int panelHeight = juce::jmin(NotificationListComponent::MAX_HEIGHT, getHeight() - panelY - 20);
        notificationList->setBounds(panelX, panelY, NotificationListComponent::PREFERRED_WIDTH, panelHeight);
    }

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

    if (userDiscoveryComponent)
        userDiscoveryComponent->setBounds(bounds);
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
    if (userDiscoveryComponent)
        userDiscoveryComponent->setVisible(false);

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

        case AppView::Discovery:
            if (userDiscoveryComponent)
            {
                userDiscoveryComponent->setCurrentUserId(authToken);  // Use auth token or actual user ID
                userDiscoveryComponent->setVisible(true);
                userDiscoveryComponent->loadDiscoveryData();
            }
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

    // Connect WebSocket with auth token
    connectWebSocket();

    // Start notification polling
    startNotificationPolling();

    saveLoginState();
    showView(AppView::ProfileSetup);
}

void SidechainAudioProcessorEditor::logout()
{
    // Stop notification polling
    stopNotificationPolling();

    // Disconnect WebSocket
    disconnectWebSocket();

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

        // Connect WebSocket with saved auth token
        connectWebSocket();

        // Start notification polling
        startNotificationPolling();

        showView(AppView::ProfileSetup);
    }
    else
    {
        showView(AppView::Authentication);
    }
}

//==============================================================================
// WebSocket handling
void SidechainAudioProcessorEditor::connectWebSocket()
{
    if (!webSocketClient)
        return;

    if (authToken.isEmpty())
    {
        DBG("Cannot connect WebSocket: no auth token");
        return;
    }

    webSocketClient->setAuthToken(authToken);
    webSocketClient->connect();
    DBG("WebSocket connection initiated");
}

void SidechainAudioProcessorEditor::disconnectWebSocket()
{
    if (webSocketClient)
    {
        webSocketClient->clearAuthToken();
        webSocketClient->disconnect();
        DBG("WebSocket disconnected");
    }
}

void SidechainAudioProcessorEditor::handleWebSocketMessage(const WebSocketClient::Message& message)
{
    DBG("WebSocket message received - type: " + message.typeString);

    switch (message.type)
    {
        case WebSocketClient::MessageType::NewPost:
        {
            // A new post was created - refresh feed if visible
            if (postsFeedComponent && postsFeedComponent->isVisible())
            {
                // Show a "new posts available" indicator rather than auto-refresh
                // This prevents jarring UI changes while user is scrolling
                DBG("New post notification received");
            }
            break;
        }

        case WebSocketClient::MessageType::Like:
        {
            // Update like count on the affected post
            auto activityId = message.getProperty("activity_id").toString();
            auto likeCount = message.getProperty("like_count");

            if (postsFeedComponent && !activityId.isEmpty())
            {
                // postsFeedComponent could update the specific post's like count
                DBG("Like update for post: " + activityId);
            }
            break;
        }

        case WebSocketClient::MessageType::Follow:
        {
            // Someone followed the current user
            auto followerUsername = message.getProperty("follower_username").toString();
            DBG("New follower: " + followerUsername);
            break;
        }

        case WebSocketClient::MessageType::PlayCount:
        {
            // Play count updated for a post
            auto activityId = message.getProperty("activity_id").toString();
            auto playCount = message.getProperty("play_count");
            DBG("Play count update for post: " + activityId);
            break;
        }

        case WebSocketClient::MessageType::Notification:
        {
            // Generic notification - could show a badge or toast
            DBG("Notification received: " + juce::JSON::toString(message.data));
            break;
        }

        case WebSocketClient::MessageType::PresenceUpdate:
        {
            // User online/offline status changed
            auto userId = message.getProperty("user_id").toString();
            auto isOnline = message.getProperty("is_online");
            DBG("Presence update - user: " + userId + " online: " + (isOnline ? "yes" : "no"));
            break;
        }

        case WebSocketClient::MessageType::Error:
        {
            auto errorMsg = message.getProperty("message").toString();
            DBG("WebSocket error message: " + errorMsg);
            break;
        }

        case WebSocketClient::MessageType::Heartbeat:
            // Heartbeat response - connection is alive
            break;

        default:
            DBG("Unknown WebSocket message type: " + message.typeString);
            break;
    }
}

void SidechainAudioProcessorEditor::handleWebSocketStateChange(WebSocketClient::ConnectionState wsState)
{
    // Map WebSocket state to connection indicator status
    if (connectionIndicator)
    {
        switch (wsState)
        {
            case WebSocketClient::ConnectionState::Connected:
                connectionIndicator->setStatus(NetworkClient::ConnectionStatus::Connected);
                DBG("WebSocket connected - indicator green");
                break;

            case WebSocketClient::ConnectionState::Connecting:
            case WebSocketClient::ConnectionState::Reconnecting:
                connectionIndicator->setStatus(NetworkClient::ConnectionStatus::Connecting);
                DBG("WebSocket connecting - indicator yellow");
                break;

            case WebSocketClient::ConnectionState::Disconnected:
                connectionIndicator->setStatus(NetworkClient::ConnectionStatus::Disconnected);
                DBG("WebSocket disconnected - indicator red");
                break;
        }
    }
}

//==============================================================================
// Notification handling
//==============================================================================

// Helper class for notification polling timer
class NotificationPollTimer : public juce::Timer
{
public:
    NotificationPollTimer(std::function<void()> callback) : onTick(callback) {}
    void timerCallback() override { if (onTick) onTick(); }
    std::function<void()> onTick;
};

void SidechainAudioProcessorEditor::setupNotifications()
{
    // Create notification bell component
    notificationBell = std::make_unique<NotificationBellComponent>();
    notificationBell->onBellClicked = [this]() {
        toggleNotificationPanel();
    };
    addAndMakeVisible(notificationBell.get());

    // Create notification list component (initially hidden)
    notificationList = std::make_unique<NotificationListComponent>();
    notificationList->onNotificationClicked = [this](const NotificationItem& item) {
        DBG("Notification clicked: " + item.getDisplayText());
        hideNotificationPanel();

        // Navigate based on notification type
        if (item.verb == "follow")
        {
            // Could navigate to the follower's profile
        }
        else if (item.verb == "like" || item.verb == "comment")
        {
            // Could navigate to the post
        }
    };
    notificationList->onMarkAllReadClicked = [this]() {
        if (networkClient)
        {
            networkClient->markNotificationsRead([this](bool success, const juce::var&) {
                if (success)
                {
                    // Refresh notifications to update read state
                    fetchNotifications();
                }
            });
        }
    };
    notificationList->onCloseClicked = [this]() {
        hideNotificationPanel();
    };
    notificationList->onRefreshRequested = [this]() {
        fetchNotifications();
    };
    addChildComponent(notificationList.get()); // Initially hidden

    // Create polling timer (will be started on login)
    notificationPollTimer = std::make_unique<NotificationPollTimer>([this]() {
        fetchNotificationCounts();
    });
}

void SidechainAudioProcessorEditor::showNotificationPanel()
{
    if (!notificationList || notificationPanelVisible)
        return;

    notificationPanelVisible = true;
    notificationList->setVisible(true);
    notificationList->toFront(true);

    // Fetch full notifications when panel is shown
    fetchNotifications();

    // Mark notifications as seen (clears badge)
    if (networkClient)
    {
        networkClient->markNotificationsSeen([this](bool success, const juce::var&) {
            if (success && notificationBell)
            {
                notificationBell->clearBadge();
            }
        });
    }
}

void SidechainAudioProcessorEditor::hideNotificationPanel()
{
    if (!notificationList || !notificationPanelVisible)
        return;

    notificationPanelVisible = false;
    notificationList->setVisible(false);
}

void SidechainAudioProcessorEditor::toggleNotificationPanel()
{
    if (notificationPanelVisible)
        hideNotificationPanel();
    else
        showNotificationPanel();
}

void SidechainAudioProcessorEditor::fetchNotifications()
{
    if (!networkClient || !networkClient->isAuthenticated())
        return;

    if (notificationList)
        notificationList->setLoading(true);

    networkClient->getNotifications(20, 0, [this](bool success, const juce::var& groups, int unseen, int unread) {
        if (!success)
        {
            if (notificationList)
                notificationList->setError("Failed to load notifications");
            return;
        }

        // Update counts
        if (notificationBell)
        {
            notificationBell->setUnseenCount(unseen);
            notificationBell->setUnreadCount(unread);
        }
        if (notificationList)
        {
            notificationList->setUnseenCount(unseen);
            notificationList->setUnreadCount(unread);
        }

        // Parse notification groups
        juce::Array<NotificationItem> items;
        if (groups.isArray())
        {
            for (int i = 0; i < groups.size(); ++i)
            {
                items.add(NotificationItem::fromJson(groups[i]));
            }
        }

        if (notificationList)
            notificationList->setNotifications(items);
    });
}

void SidechainAudioProcessorEditor::fetchNotificationCounts()
{
    if (!networkClient || !networkClient->isAuthenticated())
        return;

    networkClient->getNotificationCounts([this](int unseen, int unread) {
        if (notificationBell)
        {
            notificationBell->setUnseenCount(unseen);
            notificationBell->setUnreadCount(unread);
        }
    });
}

void SidechainAudioProcessorEditor::startNotificationPolling()
{
    if (notificationPollTimer)
    {
        // Poll every 30 seconds
        static_cast<NotificationPollTimer*>(notificationPollTimer.get())->startTimer(30000);

        // Also fetch immediately
        fetchNotificationCounts();
    }
}

void SidechainAudioProcessorEditor::stopNotificationPolling()
{
    if (notificationPollTimer)
    {
        static_cast<NotificationPollTimer*>(notificationPollTimer.get())->stopTimer();
    }
}
