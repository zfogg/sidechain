#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "util/Colors.h"
#include "util/Constants.h"
#include "util/Json.h"
#include "stores/ImageCache.h"
#include "util/Log.h"
#include "util/Result.h"

//==============================================================================
SidechainAudioProcessorEditor::SidechainAudioProcessorEditor(SidechainAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setSize(PLUGIN_WIDTH, PLUGIN_HEIGHT);

    // Initialize centralized user data store
    userDataStore = std::make_unique<UserDataStore>();
    userDataStore->addChangeListener(this);

    // Initialize network client with development config
    networkClient = std::make_unique<NetworkClient>(NetworkClient::Config::development());

    // Wire up UserDataStore with NetworkClient
    userDataStore->setNetworkClient(networkClient.get());

    // Set up ImageLoader with NetworkClient
    ImageLoader::setNetworkClient(networkClient.get());

    // Set up AudioPlayer with NetworkClient
    audioProcessor.getAudioPlayer().setNetworkClient(networkClient.get());

    // Initialize WebSocket client
    webSocketClient = std::make_unique<WebSocketClient>(WebSocketClient::Config::development());
    webSocketClient->onMessage = [this](const WebSocketClient::Message& msg) {
        handleWebSocketMessage(msg);
    };
    webSocketClient->onStateChanged = [this](WebSocketClient::ConnectionState wsState) {
        handleWebSocketStateChange(wsState);
    };
    webSocketClient->onError = [](const juce::String& error) {
        Log::error("WebSocket error: " + error);
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
    authComponent = std::make_unique<Auth>();
    authComponent->setNetworkClient(networkClient.get());
    authComponent->onLoginSuccess = [this](const juce::String& user, const juce::String& mail, const juce::String& token) {
        onLoginSuccess(user, mail, token);
    };
    authComponent->onOAuthRequested = [this](const juce::String& provider) {
        // Generate a unique session ID for this OAuth attempt
        juce::String sessionId = juce::Uuid().toString().removeCharacters("-");

        // Open OAuth URL in system browser with session_id
        juce::String oauthUrl = juce::String(Constants::Endpoints::DEV_BASE_URL) + Constants::Endpoints::API_VERSION + "/auth/" + provider + "?session_id=" + sessionId;
        juce::URL(oauthUrl).launchInDefaultBrowser();

        // Start polling for OAuth completion
        startOAuthPolling(sessionId, provider);
    };
    addChildComponent(authComponent.get());

    //==========================================================================
    // Create ProfileSetup
    profileSetupComponent = std::make_unique<ProfileSetup>();
    profileSetupComponent->onSkipSetup = [this]() { showView(AppView::PostsFeed); };
    profileSetupComponent->onCompleteSetup = [this]() { showView(AppView::PostsFeed); };
    profileSetupComponent->onProfilePicSelected = [this](const juce::String& localPath) {
        juce::File imageFile(localPath);
        if (imageFile.existsAsFile() && networkClient)
        {
            // Store local path temporarily for preview
            profileSetupComponent->setLocalPreviewPath(localPath);

            // Also set local preview in UserDataStore
            if (userDataStore)
                userDataStore->setLocalPreviewImage(imageFile);

            networkClient->uploadProfilePicture(imageFile, [this](Outcome<juce::String> result) {
                juce::MessageManager::callAsync([this, result]() {
                    if (result.isOk() && result.getValue().isNotEmpty())
                    {
                        auto s3Url = result.getValue();
                        // Update UserDataStore with the S3 URL (will trigger image download)
                        if (userDataStore)
                        {
                            userDataStore->setProfilePictureUrl(s3Url);
                            userDataStore->saveToSettings();
                        }

                        // Update legacy state
                        profilePicUrl = s3Url;
                        saveLoginState();

                        // Update profile setup component with the S3 URL
                        if (profileSetupComponent)
                            profileSetupComponent->setProfilePictureUrl(s3Url);
                    }
                    else
                    {
                        // On failure, don't store local path - just show error
                        Log::error("Profile picture upload failed");
                    }
                });
            });
        }
    };
    profileSetupComponent->onLogout = [this]() { confirmAndLogout(); };
    addChildComponent(profileSetupComponent.get());

    //==========================================================================
    // Create PostsFeed
    postsFeedComponent = std::make_unique<PostsFeed>();
    postsFeedComponent->setNetworkClient(networkClient.get());
    postsFeedComponent->setAudioPlayer(&audioProcessor.getAudioPlayer());
    // Note: StreamChatClient will be set after it's created (below)
    postsFeedComponent->onGoToProfile = [this]() { showView(AppView::ProfileSetup); };
    postsFeedComponent->onNavigateToProfile = [this](const juce::String& userId) {
        showProfile(userId);
    };
    postsFeedComponent->onLogout = [this]() { confirmAndLogout(); };
    postsFeedComponent->onStartRecording = [this]() { showView(AppView::Recording); };
    postsFeedComponent->onGoToDiscovery = [this]() { showView(AppView::Discovery); };
    addChildComponent(postsFeedComponent.get());

    //==========================================================================
    // Create RecordingComponent
    recordingComponent = std::make_unique<Recording>(audioProcessor);
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
    // Create Upload
    uploadComponent = std::make_unique<Upload>(audioProcessor, *networkClient);
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
    userDiscoveryComponent = std::make_unique<UserDiscovery>();
    userDiscoveryComponent->setNetworkClient(networkClient.get());
    // Note: StreamChatClient will be set after it's created (below)
    userDiscoveryComponent->onBackPressed = [this]() {
        navigateBack();
    };
    userDiscoveryComponent->onUserSelected = [this](const DiscoveredUser& user) {
        // Navigate to user profile
        showProfile(user.id);
    };
    addChildComponent(userDiscoveryComponent.get());

    //==========================================================================
    // Create Search
    searchComponent = std::make_unique<Search>();
    searchComponent->setNetworkClient(networkClient.get());
    if (userDataStore)
        searchComponent->setCurrentUserId(userDataStore->getUserId());
    searchComponent->onBackPressed = [this]() {
        navigateBack();
    };
    searchComponent->onUserSelected = [this](const juce::String& userId) {
        showProfile(userId);
    };
    searchComponent->onPostSelected = [this](const FeedPost& post) {
        // Handle notification click action.
        // NOT YET IMPLEMENTED - Navigation to post details or playback.
        // See notes/PLAN.md for post detail view implementation plan.
        // TODO: Navigate to post details or play post - see PLAN.md Phase 4
        audioProcessor.getAudioPlayer().loadAndPlay(post.id, post.audioUrl);
    };
    addChildComponent(searchComponent.get());

    //==========================================================================
    // Create StoryRecording
    storyRecordingComponent = std::make_unique<StoryRecording>(audioProcessor);
    storyRecordingComponent->onRecordingComplete = [this](const juce::AudioBuffer<float>& recordedAudio, const juce::var& midiData, int bpm, const juce::String& key, const juce::StringArray& genres) {
        // Upload story
        if (networkClient && recordedAudio.getNumSamples() > 0)
        {
            networkClient->uploadStory(recordedAudio, audioProcessor.getCurrentSampleRate(), midiData, bpm, key, genres,
                [this](Outcome<juce::var> result) {
                    juce::MessageManager::callAsync([this, result]() {
                        if (result.isOk())
                        {
                            Log::info("Story uploaded successfully");
                            // Navigate back to feed
                            showView(AppView::PostsFeed);
                        }
                        else
                        {
                            Log::error("Story upload failed: " + result.getError());
                            juce::MessageManager::callAsync([result]() {
                                juce::AlertWindow::showMessageBoxAsync(
                                    juce::MessageBoxIconType::WarningIcon,
                                    "Upload Error",
                                    "Failed to upload story: " + result.getError());
                            });
                        }
                    });
                });
        }
    };
    storyRecordingComponent->onRecordingDiscarded = [this]() {
        showView(AppView::PostsFeed);
    };
    storyRecordingComponent->onCancel = [this]() {
        showView(AppView::PostsFeed);
    };
    addChildComponent(storyRecordingComponent.get());

    //==========================================================================
    // Create StreamChatClient for getstream.io messaging
    streamChatClient = std::make_unique<StreamChatClient>(networkClient.get(), StreamChatClient::Config::development());

    // Wire up unread count callback to update header badge
    streamChatClient->setUnreadCountCallback([this](int totalUnread) {
        if (headerComponent)
            headerComponent->setUnreadMessageCount(totalUnread);
    });

    // Wire up presence changed callback to update UI components in real-time
    streamChatClient->setPresenceChangedCallback([this](const StreamChatClient::UserPresence& presence) {
        // Update presence in all components that display user status
        if (postsFeedComponent)
            postsFeedComponent->updateUserPresence(presence.userId, presence.online, presence.status);
        if (profileComponent)
            profileComponent->updateUserPresence(presence.userId, presence.online, presence.status);
        if (userDiscoveryComponent)
            userDiscoveryComponent->updateUserPresence(presence.userId, presence.online, presence.status);
        if (searchComponent)
            searchComponent->updateUserPresence(presence.userId, presence.online, presence.status);
    });

    // Wire StreamChatClient to components that need presence queries
    if (postsFeedComponent)
        postsFeedComponent->setStreamChatClient(streamChatClient.get());
    if (profileComponent)
        profileComponent->setStreamChatClient(streamChatClient.get());
    if (userDiscoveryComponent)
        userDiscoveryComponent->setStreamChatClient(streamChatClient.get());
    if (searchComponent)
        searchComponent->setStreamChatClient(streamChatClient.get());

    //==========================================================================
    // Create MessagesList
    messagesListComponent = std::make_unique<MessagesList>();
    messagesListComponent->setStreamChatClient(streamChatClient.get());
    messagesListComponent->setNetworkClient(networkClient.get());
    messagesListComponent->onChannelSelected = [this](const juce::String& channelType, const juce::String& channelId) {
        showMessageThread(channelType, channelId);
    };
    messagesListComponent->onNewMessage = [this]() {
        // Handle new message action from header.
        // NOT YET IMPLEMENTED - See notes/PLAN.md Phase 7 for messaging features.
        // Should open user search/picker dialog to start new conversation.
        // TODO: Open new message dialog / user search - see PLAN.md Phase 7
        showView(AppView::Discovery);
    };
    messagesListComponent->onGoToDiscovery = [this]() {
        showView(AppView::Discovery);
    };
    messagesListComponent->onCreateGroup = [this]() {
        // Handle create group action from header.
        // NOT YET IMPLEMENTED - See notes/PLAN.md Phase 7 for group messaging.
        // Should open dialog with user picker to create new group channel.
        // TODO: Open group creation dialog with user picker - see PLAN.md Phase 7
        // For now, navigate to discovery to select users
        showView(AppView::Discovery);
        Log::info("PluginEditor: Create Group clicked - navigating to Discovery (full UI pending)");
    };
    addChildComponent(messagesListComponent.get());

    //==========================================================================
    // Create MessageThread
    messageThreadComponent = std::make_unique<MessageThread>();
    messageThreadComponent->setStreamChatClient(streamChatClient.get());
    messageThreadComponent->setNetworkClient(networkClient.get());
    messageThreadComponent->setAudioProcessor(&audioProcessor);
    messageThreadComponent->onBackPressed = [this]() {
        showView(AppView::Messages);
    };
    addChildComponent(messageThreadComponent.get());

    //==========================================================================
    // Create Profile
    profileComponent = std::make_unique<Profile>();
    profileComponent->setNetworkClient(networkClient.get());
    // Note: StreamChatClient will be set after it's created (below)
    profileComponent->onBackPressed = [this]() {
        navigateBack();
    };
    profileComponent->onEditProfile = [this]() {
        // Switch to profile setup for editing
        showView(AppView::ProfileSetup);
    };
    profileComponent->onPlayClicked = [this](const FeedPost& post) {
        audioProcessor.getAudioPlayer().loadAndPlay(post.id, post.audioUrl);
    };
    profileComponent->onPauseClicked = [this](const FeedPost& /*post*/) {
        audioProcessor.getAudioPlayer().stop();
    };
    profileComponent->onMessageClicked = [this](const juce::String& userId) {
        // Create direct channel with user and navigate to message thread
        if (streamChatClient && streamChatClient->isAuthenticated())
        {
            streamChatClient->createDirectChannel(userId, [this](Outcome<StreamChatClient::Channel> result) {
                juce::MessageManager::callAsync([this, result]() {
                    if (result.isOk())
                    {
                        auto channel = result.getValue();
                        showMessageThread(channel.type, channel.id);
                    }
                    else
                    {
                        Log::error("PluginEditor: Failed to create DM channel: " + result.getError());
                    }
                });
            });
        }
        else
        {
            // Fall back to messages view if stream chat not ready
            showView(AppView::Messages);
        }
    };
    addChildComponent(profileComponent.get());

    //==========================================================================
    // Setup notifications
    setupNotifications();

    //==========================================================================
    // Create central header component (shown on all post-login pages)
    headerComponent = std::make_unique<Header>();
    headerComponent->setNetworkClient(networkClient.get());
    headerComponent->onLogoClicked = [this]() {
        showView(AppView::PostsFeed);
    };
    headerComponent->onSearchClicked = [this]() {
        showView(AppView::Search);
    };
    headerComponent->onProfileClicked = [this]() {
        // Show current user's profile
        if (userDataStore && !userDataStore->getUserId().isEmpty())
            showProfile(userDataStore->getUserId());
        else
            showView(AppView::ProfileSetup);  // Fallback to setup if no user ID
    };
    headerComponent->onRecordClicked = [this]() {
        showView(AppView::Recording);
    };
    headerComponent->onStoryClicked = [this]() {
        showView(AppView::StoryRecording);
    };
    headerComponent->onMessagesClicked = [this]() {
        showView(AppView::Messages);
    };
    addChildComponent(headerComponent.get()); // Initially hidden until logged in

    //==========================================================================
    // Check for previous crash before loading state
    checkForPreviousCrash();

    //==========================================================================
    // Load persistent state and show appropriate view
    loadLoginState();
}

SidechainAudioProcessorEditor::~SidechainAudioProcessorEditor()
{
    // Mark clean shutdown before destroying components
    markCleanShutdown();

    // Stop OAuth polling
    stopOAuthPolling();

    // Stop notification polling
    stopNotificationPolling();

    // Remove as listener from UserDataStore
    if (userDataStore)
        userDataStore->removeChangeListener(this);

    // Disconnect WebSocket before destruction
    if (webSocketClient)
        webSocketClient->disconnect();
}

//==============================================================================
void SidechainAudioProcessorEditor::paint(juce::Graphics& g)
{
    // Dark background - each component handles its own painting
    g.fillAll(SidechainColors::background());
}

void SidechainAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    auto headerHeight = Header::HEADER_HEIGHT;

    // Position central header at top (for post-login views)
    if (headerComponent)
        headerComponent->setBounds(bounds.removeFromTop(headerHeight));

    // Bounds for content below header (used by post-login views)
    auto contentBounds = getLocalBounds().withTrimmedTop(headerHeight);

    // Position notification bell in header area (right side)
    if (notificationBell)
        notificationBell->setBounds(getWidth() - 70, (headerHeight - NotificationBell::PREFERRED_SIZE) / 2,
                                    NotificationBell::PREFERRED_SIZE, NotificationBell::PREFERRED_SIZE);

    // Position connection indicator in header area (far right)
    if (connectionIndicator)
        connectionIndicator->setBounds(getWidth() - 28, (headerHeight - 16) / 2, 16, 16);

    // Position notification panel as dropdown from bell
    if (notificationList)
    {
        int panelX = getWidth() - NotificationList::PREFERRED_WIDTH - 10;
        int panelY = headerHeight + 5;
        int panelHeight = juce::jmin(NotificationList::MAX_HEIGHT, getHeight() - panelY - 20);
        notificationList->setBounds(panelX, panelY, NotificationList::PREFERRED_WIDTH, panelHeight);
    }

    // Auth component fills entire window (no header)
    if (authComponent)
        authComponent->setBounds(getLocalBounds());

    // Post-login views: use content bounds (below header)
    if (profileSetupComponent)
        profileSetupComponent->setBounds(contentBounds);

    if (postsFeedComponent)
        postsFeedComponent->setBounds(contentBounds);

    if (recordingComponent)
        recordingComponent->setBounds(contentBounds);

    if (uploadComponent)
        uploadComponent->setBounds(contentBounds);

    if (userDiscoveryComponent)
        userDiscoveryComponent->setBounds(contentBounds);

    if (profileComponent)
        profileComponent->setBounds(contentBounds);

    if (searchComponent)
        searchComponent->setBounds(contentBounds);

    if (messagesListComponent)
        messagesListComponent->setBounds(contentBounds);

    if (messageThreadComponent)
        messageThreadComponent->setBounds(contentBounds);

    if (storyRecordingComponent)
        storyRecordingComponent->setBounds(contentBounds);
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
    if (profileComponent)
        profileComponent->setVisible(false);
    if (searchComponent)
        searchComponent->setVisible(false);
    if (messagesListComponent)
        messagesListComponent->setVisible(false);
    if (messageThreadComponent)
        messageThreadComponent->setVisible(false);
    if (storyRecordingComponent)
        storyRecordingComponent->setVisible(false);

    // Push current view to navigation stack (except when going back)
    if (currentView != view && currentView != AppView::Authentication)
    {
        navigationStack.add(currentView);
        // Keep stack reasonable size
        while (navigationStack.size() > 10)
            navigationStack.remove(0);
    }

    currentView = view;

    // Show header for all post-login views
    bool showHeader = (view != AppView::Authentication);
    if (headerComponent)
    {
        headerComponent->setVisible(showHeader);
        if (showHeader && userDataStore)
        {
            headerComponent->setUserInfo(userDataStore->getUsername(), userDataStore->getProfilePictureUrl());

            // Use cached image from UserDataStore if available
            if (userDataStore->hasProfileImage())
            {
                headerComponent->setProfileImage(userDataStore->getProfileImage());
            }
            headerComponent->toFront(false);
        }
    }

    // Show/hide notification components based on login state
    if (notificationBell)
        notificationBell->setVisible(showHeader);

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
                // Pass cached profile image from UserDataStore (downloaded via HTTP proxy)
                if (userDataStore && userDataStore->hasProfileImage())
                {
                    profileSetupComponent->setProfileImage(userDataStore->getProfileImage());
                }
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

        case AppView::StoryRecording:
            if (storyRecordingComponent)
                storyRecordingComponent->setVisible(true);
            break;

        case AppView::Discovery:
            if (userDiscoveryComponent)
            {
                userDiscoveryComponent->setCurrentUserId(authToken);  // Use auth token or actual user ID
                userDiscoveryComponent->setVisible(true);
                userDiscoveryComponent->loadDiscoveryData();
            }
            break;

        case AppView::Profile:
            if (profileComponent)
            {
                // Set current user ID for "is own profile" checks
                if (userDataStore)
                    profileComponent->setCurrentUserId(userDataStore->getUserId());

                // Load the profile for the specified user
                profileComponent->loadProfile(profileUserIdToView);
                profileComponent->setVisible(true);
            }
            break;

        case AppView::Search:
            if (searchComponent)
            {
                if (userDataStore)
                    searchComponent->setCurrentUserId(userDataStore->getUserId());
                searchComponent->setVisible(true);
                searchComponent->focusSearchInput();
            }
            break;

        case AppView::Messages:
            if (messagesListComponent)
            {
                messagesListComponent->setVisible(true);
                messagesListComponent->loadChannels();
            }
            break;

        case AppView::MessageThread:
            if (messageThreadComponent)
            {
                if (userDataStore)
                    messageThreadComponent->setCurrentUserId(userDataStore->getUserId());
                messageThreadComponent->loadChannel(messageChannelType, messageChannelId);
                messageThreadComponent->setVisible(true);
            }
            break;
    }

    repaint();
}

void SidechainAudioProcessorEditor::showProfile(const juce::String& userId)
{
    profileUserIdToView = userId;
    showView(AppView::Profile);
}

void SidechainAudioProcessorEditor::showMessageThread(const juce::String& channelType, const juce::String& channelId)
{
    messageChannelType = channelType;
    messageChannelId = channelId;
    showView(AppView::MessageThread);
}

void SidechainAudioProcessorEditor::navigateBack()
{
    if (navigationStack.isEmpty())
    {
        // Default to feed if no history
        showView(AppView::PostsFeed);
        return;
    }

    // Pop last view from stack without adding current to stack
    AppView previousView = navigationStack.getLast();
    navigationStack.removeLast();

    // Set currentView to prevent double-push
    currentView = previousView;

    // Actually show the view
    // Hide all views first
    if (authComponent) authComponent->setVisible(false);
    if (profileSetupComponent) profileSetupComponent->setVisible(false);
    if (postsFeedComponent) postsFeedComponent->setVisible(false);
    if (recordingComponent) recordingComponent->setVisible(false);
    if (uploadComponent) uploadComponent->setVisible(false);
    if (userDiscoveryComponent) userDiscoveryComponent->setVisible(false);
    if (profileComponent) profileComponent->setVisible(false);

    // Show the previous view
    switch (previousView)
    {
        case AppView::PostsFeed:
            if (postsFeedComponent)
            {
                postsFeedComponent->setVisible(true);
            }
            break;
        case AppView::Discovery:
            if (userDiscoveryComponent)
            {
                userDiscoveryComponent->setVisible(true);
            }
            break;
        case AppView::Profile:
            if (profileComponent)
            {
                profileComponent->setVisible(true);
            }
            break;
        case AppView::Recording:
            if (recordingComponent)
            {
                recordingComponent->setVisible(true);
            }
            break;
        case AppView::ProfileSetup:
            if (profileSetupComponent)
            {
                profileSetupComponent->setVisible(true);
            }
            break;
        case AppView::Search:
            if (searchComponent)
            {
                searchComponent->setVisible(true);
            }
            break;
        case AppView::Messages:
            if (messagesListComponent)
            {
                messagesListComponent->setVisible(true);
            }
            break;
        case AppView::MessageThread:
            if (messageThreadComponent)
            {
                if (userDataStore)
                    messageThreadComponent->setCurrentUserId(userDataStore->getUserId());
                messageThreadComponent->loadChannel(messageChannelType, messageChannelId);
                messageThreadComponent->setVisible(true);
            }
            break;
        default:
            if (postsFeedComponent)
            {
                postsFeedComponent->setVisible(true);
            }
            break;
    }

    repaint();
}

void SidechainAudioProcessorEditor::onLoginSuccess(const juce::String& user, const juce::String& mail, const juce::String& token)
{
    // Update legacy state (for backwards compatibility during migration)
    username = user;
    email = mail;
    authToken = token;

    // Update centralized UserDataStore
    if (userDataStore)
    {
        userDataStore->setAuthToken(token);
        userDataStore->setBasicUserInfo(user, mail);
    }

    // Set auth token on network client
    if (networkClient && !token.isEmpty())
        networkClient->setAuthToken(token);

    // Fetch getstream.io chat token for messaging
    if (streamChatClient && !token.isEmpty())
    {
        streamChatClient->fetchToken(token, [](Outcome<StreamChatClient::TokenResult> result) {
            if (result.isOk())
            {
                Log::info("Stream chat token fetched successfully for user: " + result.getValue().userId);
            }
            else
            {
                Log::warn("Failed to fetch stream chat token: " + result.getError());
            }
        });
    }

    // Connect WebSocket with auth token
    connectWebSocket();

    // Start notification polling
    startNotificationPolling();

    // Fetch full user profile (including profile picture) via UserDataStore
    if (userDataStore)
    {
        userDataStore->fetchUserProfile([this](bool /*success*/) {
            juce::MessageManager::callAsync([this]() {
                // Sync legacy state from UserDataStore
                if (userDataStore)
                {
                    profilePicUrl = userDataStore->getProfilePictureUrl();
                }

                saveLoginState();

                // Show header now that user is logged in
                if (headerComponent)
                    headerComponent->setVisible(true);

                // If user has a profile picture, skip setup and go straight to feed
                if (userDataStore && !userDataStore->getProfilePictureUrl().isEmpty())
                {
                    showView(AppView::PostsFeed);
                }
                else
                {
                    showView(AppView::ProfileSetup);
                }
            });
        });
    }
    else
    {
        saveLoginState();
        showView(AppView::ProfileSetup);
    }
}

void SidechainAudioProcessorEditor::logout()
{
    // Stop OAuth polling if in progress
    stopOAuthPolling();

    // Stop notification polling
    stopNotificationPolling();

    // Disconnect WebSocket
    disconnectWebSocket();

    // Clear user state via UserDataStore (also clears persisted settings)
    if (userDataStore)
    {
        userDataStore->clearAll();
    }

    // Clear legacy state
    username = "";
    email = "";
    profilePicUrl = "";
    authToken = "";

    // Clear network client auth
    if (networkClient)
        networkClient->setAuthToken("");

    // Hide header when logged out
    if (headerComponent)
        headerComponent->setVisible(false);

    showView(AppView::Authentication);
}

void SidechainAudioProcessorEditor::confirmAndLogout()
{
    juce::AlertWindow::showOkCancelBox(
        juce::MessageBoxIconType::QuestionIcon,
        "Logout",
        "Are you sure you want to logout?",
        "Logout",
        "Cancel",
        nullptr,
        juce::ModalCallbackFunction::create([this](int result) {
            if (result == 1) // OK button
            {
                logout();
            }
        })
    );
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
    // Load user data from UserDataStore (which handles persistence)
    if (userDataStore)
    {
        userDataStore->loadFromSettings();

        Log::debug("loadLoginState: isLoggedIn=" + juce::String(userDataStore->isLoggedIn() ? "true" : "false") +
            ", username=" + userDataStore->getUsername() +
            ", profilePicUrl=" + userDataStore->getProfilePictureUrl());

        if (userDataStore->isLoggedIn())
        {
            // Sync legacy state from UserDataStore
            authToken = userDataStore->getAuthToken();
            username = userDataStore->getUsername();
            email = userDataStore->getEmail();
            profilePicUrl = userDataStore->getProfilePictureUrl();

            Log::debug("loadLoginState: authToken loaded, length=" + juce::String(authToken.length()));

            // Set auth token on network client
            if (!authToken.isEmpty() && networkClient)
                networkClient->setAuthToken(authToken);

            // Fetch getstream.io chat token for messaging
            if (streamChatClient && !authToken.isEmpty())
            {
                streamChatClient->fetchToken(authToken, [](Outcome<StreamChatClient::TokenResult> result) {
                    if (result.isOk())
                    {
                        Log::info("Stream chat token fetched successfully for user: " + result.getValue().userId);
                    }
                    else
                    {
                        Log::warn("Failed to fetch stream chat token: " + result.getError());
                    }
                });
            }

            // Connect WebSocket with saved auth token
            connectWebSocket();

            // Start notification polling
            startNotificationPolling();

            // Show header for logged-in users
            if (headerComponent)
            {
                headerComponent->setVisible(true);
                // Set initial user info (image will be updated via changeListenerCallback when downloaded)
                headerComponent->setUserInfo(username, profilePicUrl);
                if (userDataStore->hasProfileImage())
                {
                    headerComponent->setProfileImage(userDataStore->getProfileImage());
                }
            }

            // Fetch fresh profile to get latest data (including profile picture)
            userDataStore->fetchUserProfile([this](bool success) {
                Log::debug("fetchUserProfile callback: success=" + juce::String(success ? "true" : "false"));
                juce::MessageManager::callAsync([this]() {
                    // Sync profile URL from UserDataStore
                    if (userDataStore)
                    {
                        profilePicUrl = userDataStore->getProfilePictureUrl();
                        Log::debug("After fetchUserProfile: profilePicUrl=" + profilePicUrl +
                            ", hasImage=" + juce::String(userDataStore->hasProfileImage() ? "true" : "false"));
                    }

                    // If user has a profile picture, skip setup and go straight to feed
                    if (userDataStore && !userDataStore->getProfilePictureUrl().isEmpty())
                    {
                        showView(AppView::PostsFeed);
                    }
                    else
                    {
                        showView(AppView::ProfileSetup);
                    }
                });
            });
        }
        else
        {
            showView(AppView::Authentication);
        }
    }
    else
    {
        showView(AppView::Authentication);
    }
}

//==============================================================================
// Crash detection
void SidechainAudioProcessorEditor::checkForPreviousCrash()
{
    auto properties = juce::PropertiesFile::Options();
    properties.applicationName = "Sidechain";
    properties.filenameSuffix = ".settings";
    properties.folderName = "SidechainPlugin";

    auto appProperties = std::make_unique<juce::PropertiesFile>(properties);

    // Check if clean shutdown flag exists (if it doesn't exist, this is first run)
    if (appProperties->containsKey("cleanShutdown"))
    {
        // Flag exists - check its value
        bool cleanShutdown = appProperties->getBoolValue("cleanShutdown", false);

        if (!cleanShutdown)
        {
            // App didn't shut down cleanly - it likely crashed
            // Show notification after a short delay to ensure UI is ready
            juce::MessageManager::callAsync([]() {
                juce::AlertWindow::showMessageBoxAsync(
                    juce::MessageBoxIconType::WarningIcon,
                    "Previous Session Ended Unexpectedly",
                    "The plugin did not shut down cleanly during the last session. "
                    "This may indicate a crash or unexpected termination.\n\n"
                    "If this happens frequently, please report it with details about what you were doing.",
                    "OK"
                );
            });

            Log::warn("Detected previous crash - clean shutdown flag was not set");
        }
    }
    // If flag doesn't exist, this is the first run - no crash to report

    // Clear the flag now (we'll set it again on clean shutdown)
    appProperties->setValue("cleanShutdown", false);
    appProperties->save();
}

void SidechainAudioProcessorEditor::markCleanShutdown()
{
    auto properties = juce::PropertiesFile::Options();
    properties.applicationName = "Sidechain";
    properties.filenameSuffix = ".settings";
    properties.folderName = "SidechainPlugin";

    auto appProperties = std::make_unique<juce::PropertiesFile>(properties);

    // Set clean shutdown flag
    appProperties->setValue("cleanShutdown", true);
    appProperties->save();

    Log::debug("Marked clean shutdown");
}

//==============================================================================
// WebSocket handling
void SidechainAudioProcessorEditor::connectWebSocket()
{
    if (!webSocketClient)
        return;

    if (authToken.isEmpty())
    {
        Log::warn("Cannot connect WebSocket: no auth token");
        return;
    }

    webSocketClient->setAuthToken(authToken);
    webSocketClient->connect();
    Log::info("WebSocket connection initiated");
}

void SidechainAudioProcessorEditor::disconnectWebSocket()
{
    if (webSocketClient)
    {
        webSocketClient->clearAuthToken();
        webSocketClient->disconnect();
        Log::info("WebSocket disconnected");
    }
}

void SidechainAudioProcessorEditor::handleWebSocketMessage(const WebSocketClient::Message& message)
{
    Log::debug("WebSocket message received - type: " + message.typeString);

    switch (message.type)
    {
        case WebSocketClient::MessageType::NewPost:
        {
            // A new post was created - show toast notification (5.5.1, 5.5.2)
            if (postsFeedComponent && postsFeedComponent->isVisible())
            {
                auto payload = message.getProperty("payload");
                postsFeedComponent->handleNewPostNotification(payload);
            }
            break;
        }

        case WebSocketClient::MessageType::Like:
        case WebSocketClient::MessageType::LikeCountUpdate:
        {
            // Update like count on the affected post (5.5.3)
            auto payload = message.getProperty("payload");
            auto postId = payload.getProperty("post_id", juce::var()).toString();
            auto likeCount = static_cast<int>(payload.getProperty("like_count", juce::var(0)));

            if (postsFeedComponent && !postId.isEmpty() && likeCount >= 0)
            {
                postsFeedComponent->handleLikeCountUpdate(postId, likeCount);
            }
            break;
        }

        case WebSocketClient::MessageType::Follow:
        case WebSocketClient::MessageType::FollowerCountUpdate:
        {
            // Follower count updated (5.5.4)
            auto payload = message.getProperty("payload");
            auto userId = payload.getProperty("followee_id", juce::var()).toString();
            auto followerCount = static_cast<int>(payload.getProperty("follower_count", juce::var(0)));

            if (postsFeedComponent && !userId.isEmpty() && followerCount >= 0)
            {
                postsFeedComponent->handleFollowerCountUpdate(userId, followerCount);
            }
            break;
        }

        case WebSocketClient::MessageType::PlayCount:
        {
            // Play count updated for a post
            auto activityId = message.getProperty("activity_id").toString();
            auto playCount = message.getProperty("play_count");
            Log::debug("Play count update for post: " + activityId);
            break;
        }

        case WebSocketClient::MessageType::Notification:
        {
            // Generic notification - could show a badge or toast
            Log::debug("Notification received: " + juce::JSON::toString(message.data));
            break;
        }

        case WebSocketClient::MessageType::PresenceUpdate:
        {
            // User online/offline status changed
            auto userId = message.getProperty("user_id").toString();
            auto isOnline = message.getProperty("is_online");
            Log::debug("Presence update - user: " + userId + " online: " + (isOnline ? "yes" : "no"));
            break;
        }

        case WebSocketClient::MessageType::Error:
        {
            auto errorMsg = message.getProperty("message").toString();
            Log::error("WebSocket error message: " + errorMsg);
            break;
        }

        case WebSocketClient::MessageType::Heartbeat:
            // Heartbeat response - connection is alive
            break;

        default:
            Log::warn("Unknown WebSocket message type: " + message.typeString);
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
                Log::debug("WebSocket connected - indicator green");
                break;

            case WebSocketClient::ConnectionState::Connecting:
            case WebSocketClient::ConnectionState::Reconnecting:
                connectionIndicator->setStatus(NetworkClient::ConnectionStatus::Connecting);
                Log::debug("WebSocket connecting - indicator yellow");
                break;

            case WebSocketClient::ConnectionState::Disconnected:
                connectionIndicator->setStatus(NetworkClient::ConnectionStatus::Disconnected);
                Log::debug("WebSocket disconnected - indicator red");
                break;
        }
    }
}

//==============================================================================
// ChangeListener - for UserDataStore updates
void SidechainAudioProcessorEditor::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == userDataStore.get())
    {
        Log::debug("changeListenerCallback: UserDataStore changed!"
            " hasImage=" + juce::String(userDataStore->hasProfileImage() ? "true" : "false") +
            " profilePicUrl=" + userDataStore->getProfilePictureUrl());

        // UserDataStore changed - update components with new data
        if (headerComponent && userDataStore)
        {
            headerComponent->setUserInfo(userDataStore->getUsername(), userDataStore->getProfilePictureUrl());

            // If UserDataStore has a cached image, use it directly (avoids re-downloading)
            if (userDataStore->hasProfileImage())
            {
                Log::debug("changeListenerCallback: Setting profile image on header");
                headerComponent->setProfileImage(userDataStore->getProfileImage());
            }
        }

        // Update ProfileSetup with cached image
        if (profileSetupComponent && userDataStore && userDataStore->hasProfileImage())
        {
            Log::debug("changeListenerCallback: Setting profile image on ProfileSetup");
            profileSetupComponent->setProfileImage(userDataStore->getProfileImage());
        }

        // Also sync to legacy state variables during migration
        if (userDataStore)
        {
            username = userDataStore->getUsername();
            email = userDataStore->getEmail();
            profilePicUrl = userDataStore->getProfilePictureUrl();
        }
    }
}

//==============================================================================
// Notification handling
//==============================================================================

/**
 * NotificationPollTimer - Helper timer for polling notifications
 *
 * Simple timer wrapper that calls a callback on each tick.
 * Used for periodic notification updates from the server.
 */
class NotificationPollTimer : public juce::Timer
{
public:
    /**
     * Create a notification poll timer with callback
     * @param callback Function to call on each timer tick
     */
    NotificationPollTimer(std::function<void()> callback) : onTick(callback) {}

    void timerCallback() override { if (onTick) onTick(); }

    /** Callback function called on each timer tick */
    std::function<void()> onTick;
};

void SidechainAudioProcessorEditor::setupNotifications()
{
    // Create notification bell component
    notificationBell = std::make_unique<NotificationBell>();
    notificationBell->onBellClicked = [this]() {
        toggleNotificationPanel();
    };
    addAndMakeVisible(notificationBell.get());

    // Create notification list component (initially hidden)
    notificationList = std::make_unique<NotificationList>();
    notificationList->onNotificationClicked = [this](const NotificationItem& item) {
        Log::debug("Notification clicked: " + item.getDisplayText());
        hideNotificationPanel();

        // Navigate based on notification type
        if (item.verb == "follow" && item.actorId.isNotEmpty())
        {
            // Navigate to the follower's profile
            showProfile(item.actorId);
        }
        else if ((item.verb == "like" || item.verb == "comment" || item.verb == "mention") && item.targetId.isNotEmpty())
        {
            // Navigate to posts feed and show the post (via comments panel)
            if (item.targetType == "loop" || item.targetType == "comment")
            {
                // Navigate to posts feed first
                showView(AppView::PostsFeed);

                // After a brief delay, load the specific post and show comments
                juce::Timer::callAfterDelay(200, [this, postId = item.targetId]() {
                    if (postsFeedComponent)
                    {
                        // Find the post in the feed and show its comments
                        postsFeedComponent->refreshFeed();
                        // Note: Full post navigation would require loading the post by ID
                        // For now, refreshing the feed will show recent posts including this one
                        Log::debug("PluginEditor: Notification clicked - refreshing feed to show post: " + postId);
                    }
                });
            }
        }
        else if (item.targetType == "user" && item.targetId.isNotEmpty())
        {
            // Navigate to user profile
            showProfile(item.targetId);
        }
    };
    notificationList->onMarkAllReadClicked = [this]() {
        if (networkClient)
        {
            networkClient->markNotificationsRead([this](Outcome<juce::var> response) {
                if (response.isOk())
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
        networkClient->markNotificationsSeen([this](Outcome<juce::var> response) {
            if (response.isOk() && notificationBell)
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

    networkClient->getNotifications(20, 0, [this](Outcome<NetworkClient::NotificationResult> result) {
        if (result.isError())
        {
            if (notificationList)
                notificationList->setError("Failed to load notifications");
            return;
        }

        auto notifResult = result.getValue();

        // Update counts
        if (notificationBell)
        {
            notificationBell->setUnseenCount(notifResult.unseen);
            notificationBell->setUnreadCount(notifResult.unread);
        }
        if (notificationList)
        {
            notificationList->setUnseenCount(notifResult.unseen);
            notificationList->setUnreadCount(notifResult.unread);
        }

        // Parse notification groups
        juce::Array<NotificationItem> items;
        if (notifResult.notifications.isArray())
        {
            for (int i = 0; i < notifResult.notifications.size(); ++i)
            {
                items.add(NotificationItem::fromJson(notifResult.notifications[i]));
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
        static_cast<NotificationPollTimer*>(notificationPollTimer.get())->startTimer(Constants::Api::DEFAULT_TIMEOUT_MS);

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

//==============================================================================
// OAuth Polling for plugin-based OAuth flow
//==============================================================================

/**
 * OAuthPollTimer - Helper timer for OAuth authentication polling
 *
 * Simple timer wrapper that calls a callback on each tick.
 * Used during OAuth flow to periodically check if authentication completed.
 */
class OAuthPollTimer : public juce::Timer
{
public:
    /**
     * Create an OAuth poll timer with callback
     * @param callback Function to call on each timer tick
     */
    OAuthPollTimer(std::function<void()> callback) : onTick(callback) {}

    void timerCallback() override { if (onTick) onTick(); }

    /** Callback function called on each timer tick */
    std::function<void()> onTick;
};

void SidechainAudioProcessorEditor::startOAuthPolling(const juce::String& sessionId, const juce::String& provider)
{
    // Store session info
    oauthSessionId = sessionId;
    oauthProvider = provider;
    oauthPollCount = 0;

    // Show loading state in auth component
    if (authComponent)
    {
        authComponent->showError("Waiting for " + provider + " authentication...");
    }

    // Create and start polling timer
    oauthPollTimer = std::make_unique<OAuthPollTimer>([this]() {
        pollOAuthStatus();
    });
    static_cast<OAuthPollTimer*>(oauthPollTimer.get())->startTimer(1000); // Poll every 1 second

    Log::info("Started OAuth polling for session: " + sessionId);
}

void SidechainAudioProcessorEditor::stopOAuthPolling()
{
    if (oauthPollTimer)
    {
        static_cast<OAuthPollTimer*>(oauthPollTimer.get())->stopTimer();
        oauthPollTimer.reset();
    }
    oauthSessionId = "";
    oauthProvider = "";
    oauthPollCount = 0;
}

void SidechainAudioProcessorEditor::pollOAuthStatus()
{
    if (oauthSessionId.isEmpty())
    {
        stopOAuthPolling();
        return;
    }

    oauthPollCount++;

    // Check if we've exceeded max polls (5 minutes)
    if (oauthPollCount > MAX_OAUTH_POLLS)
    {
        stopOAuthPolling();
        if (authComponent)
        {
            authComponent->showError("Authentication timed out. Please try again.");
        }
        return;
    }

    // Make polling request
    if (networkClient == nullptr)
    {
        Log::warn("OAuth poll: NetworkClient not available");
        return;
    }

    juce::String endpoint = juce::String(Constants::Endpoints::AUTH_OAUTH_POLL) + "?session_id=" + oauthSessionId;
    networkClient->get(endpoint, [this, capturedSessionId = oauthSessionId](Outcome<juce::var> responseOutcome) {
        // Check if this is still the active session
        if (oauthSessionId != capturedSessionId)
            return;

        if (responseOutcome.isError() || !responseOutcome.getValue().isObject())
        {
            Log::warn("OAuth poll: connection failed or invalid response");
            return; // Keep polling, might be temporary network issue
        }

        auto responseData = responseOutcome.getValue();
        juce::String status = Json::getString(responseData, "status");

            if (status == "complete")
            {
                // Success! Extract auth data
                stopOAuthPolling();

                juce::var authData = responseData["auth"];
                if (authData.isObject() && authData.hasProperty("token"))
                {
                    juce::String token = authData["token"].toString();
                    juce::String userEmail = "";
                    juce::String userName = "";

                    if (authData.hasProperty("user"))
                    {
                        juce::var userData = authData["user"];
                        if (userData.hasProperty("email"))
                            userEmail = userData["email"].toString();
                        if (userData.hasProperty("username"))
                            userName = userData["username"].toString();
                        else if (userData.hasProperty("display_name"))
                            userName = userData["display_name"].toString();
                    }

                    if (userName.isEmpty() && userEmail.isNotEmpty())
                        userName = userEmail.upToFirstOccurrenceOf("@", false, false);

                    Log::info("OAuth success! User: " + userName);

                    if (authComponent)
                        authComponent->clearError();

                    onLoginSuccess(userName, userEmail, token);
                }
            }
            else if (status == "error")
            {
                // OAuth failed
                stopOAuthPolling();
                juce::String errorMsg = Json::getString(responseData, "message", "Authentication failed");
                if (authComponent)
                    authComponent->showError(errorMsg);
            }
            else if (status == "expired" || status == "not_found")
            {
                // Session expired or invalid
                stopOAuthPolling();
                if (authComponent)
                    authComponent->showError("Authentication session expired. Please try again.");
            }
        // status == "pending" -> keep polling
    });
}
