#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "audio/NotificationSound.h"
#include "util/Async.h"
#include "util/Colors.h"
#include "util/Constants.h"
#include "util/Json.h"
#include "stores/ImageCache.h"
#include "util/Log.h"
#include "util/Result.h"
#include "util/OSNotification.h"

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
    // Create tooltip window for the entire plugin
    // This automatically displays tooltips for any child component that provides one
    tooltipWindow = std::make_unique<juce::TooltipWindow>(this, 500);  // 500ms delay

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

        // Open OAuth URL in system browser with session_id (8.3.11.12)
        juce::String oauthUrl = juce::String(Constants::Endpoints::DEV_BASE_URL) + Constants::Endpoints::API_VERSION + "/auth/" + provider + "?session_id=" + sessionId;
        juce::URL(oauthUrl).launchInDefaultBrowser();

        // Start polling for OAuth completion
        startOAuthPolling(sessionId, provider);
    };

    // Handle OAuth cancellation (8.3.11.11)
    authComponent->onOAuthCancelled = [this]() {
        Log::info("OAuth flow cancelled by user");
        stopOAuthPolling();
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

            // Show uploading state (8.3.11.6)
            if (profileSetupComponent)
                profileSetupComponent->setUploadProgress(0.1f);  // Start at 10%

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
                        {
                            profileSetupComponent->setProfilePictureUrl(s3Url);
                            profileSetupComponent->setUploadComplete(true);  // Show success (8.3.11.7)
                        }

                        Log::info("Profile picture uploaded successfully: " + s3Url);
                    }
                    else
                    {
                        // On failure, show error state
                        Log::error("Profile picture upload failed");
                        if (profileSetupComponent)
                            profileSetupComponent->setUploadComplete(false);  // Show failure
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
    postsFeedComponent->onSendPostToMessage = [this](const FeedPost& post) {
        showSharePostToMessage(post);
    };
    addChildComponent(postsFeedComponent.get());

    //==========================================================================
    // Create RecordingComponent
    recordingComponent = std::make_unique<Recording>(audioProcessor);
    recordingComponent->onRecordingComplete = [this](const juce::AudioBuffer<float>& recordedAudio, const juce::var& midiData) {
        if (uploadComponent)
        {
            // Use MIDI data passed from Recording (either captured or imported) (R.3.3)
            uploadComponent->setAudioToUpload(recordedAudio, audioProcessor.getCurrentSampleRate(), midiData);
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
    // Create StoryViewer
    storyViewerComponent = std::make_unique<StoryViewer>();
    storyViewerComponent->setNetworkClient(networkClient.get());
    if (userDataStore)
        storyViewerComponent->setCurrentUserId(userDataStore->getUserId());
    storyViewerComponent->onClose = [this]() {
        navigateBack();
    };
    storyViewerComponent->onDeleteClicked = [this](const juce::String& storyId) {
        // Story was deleted, refresh story indicators
        checkForActiveStories();
        // If we're viewing the profile, refresh it too
        if (currentView == AppView::Profile && profileComponent)
        {
            profileComponent->refresh();
        }
    };
    storyViewerComponent->onAddToHighlightClicked = [this](const juce::String& storyId) {
        showSelectHighlightDialog(storyId);
    };
    storyViewerComponent->onSendStoryToMessage = [this](const StoryData& story) {
        showShareStoryToMessage(story);
    };
    addChildComponent(storyViewerComponent.get());

    //==========================================================================
    // Create HiddenSynth easter egg (R.2.1)
    hiddenSynthComponent = std::make_unique<HiddenSynth>(audioProcessor.getSynthEngine());
    hiddenSynthComponent->onBackPressed = [this]() {
        audioProcessor.setSynthEnabled(false);
        showView(AppView::PostsFeed);
    };
    addChildComponent(hiddenSynthComponent.get());

    //==========================================================================
    // Create Playlists
    playlistsComponent = std::make_unique<Playlists>();
    playlistsComponent->setNetworkClient(networkClient.get());
    if (userDataStore)
        playlistsComponent->setCurrentUserId(userDataStore->getUserId());
    playlistsComponent->onBackPressed = [this]() {
        navigateBack();
    };
    playlistsComponent->onPlaylistSelected = [this](const juce::String& playlistId) {
        playlistIdToView = playlistId;
        showView(AppView::PlaylistDetail);
    };
    playlistsComponent->onCreatePlaylist = [this]() {
        // Show create playlist dialog with text input
        auto* dialog = new juce::AlertWindow("Create Playlist", "Enter playlist name:", juce::MessageBoxIconType::QuestionIcon);
        dialog->addTextEditor("name", "", "Playlist Name");
        dialog->addButton("Create", 1);
        dialog->addButton("Cancel", 0);
        dialog->enterModalState(true, juce::ModalCallbackFunction::create([this, dialog](int result) {
            if (result == 1)
            {
                juce::String playlistName = dialog->getTextEditorContents("name").trim();
                if (playlistName.isEmpty())
                {
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::MessageBoxIconType::WarningIcon,
                        "Error",
                        "Playlist name cannot be empty.");
                    delete dialog;
                    return;
                }

                if (networkClient)
                {
                    networkClient->createPlaylist(playlistName, "", false, true, [this](Outcome<juce::var> createResult) {
                        juce::MessageManager::callAsync([this, createResult]() {
                            if (createResult.isOk())
                            {
                                playlistsComponent->refresh();
                            }
                            else
                            {
                                juce::AlertWindow::showMessageBoxAsync(
                                    juce::MessageBoxIconType::WarningIcon,
                                    "Error",
                                    "Failed to create playlist: " + createResult.getError());
                            }
                        });
                    });
                }
            }
            delete dialog;
        }));
    };
    addChildComponent(playlistsComponent.get());

    //==========================================================================
    // Create PlaylistDetail
    playlistDetailComponent = std::make_unique<PlaylistDetail>();
    playlistDetailComponent->setNetworkClient(networkClient.get());
    if (userDataStore)
        playlistDetailComponent->setCurrentUserId(userDataStore->getUserId());
    playlistDetailComponent->onBackPressed = [this]() {
        navigateBack();
    };
    playlistDetailComponent->onPostSelected = [](const juce::String& postId) {
        // Navigate to post (or play post)
        // For now, just play the post
        // TODO: Navigate to post detail view when implemented
        (void)postId;  // Suppress unused parameter warning
    };
    playlistDetailComponent->onAddTrack = [this]() {
        // Show add track dialog - navigate to feed or show post picker
        // For now, navigate to feed
        showView(AppView::PostsFeed);
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::InfoIcon,
            "Add Track",
            "Click 'Add to Playlist' on any post to add it to this playlist.");
    };
    playlistDetailComponent->onPlayPlaylist = []() {
        // Play all tracks in playlist sequentially
        // TODO: Implement playlist playback
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::InfoIcon,
            "Play Playlist",
            "Playlist playback coming soon!");
    };
    playlistDetailComponent->onSharePlaylist = [](const juce::String& playlistId) {
        // Generate shareable playlist link
        // For now, use a simple format: sidechain://playlist/{id}
        // In production, this would be a web URL like https://sidechain.app/playlist/{id}
        juce::String shareLink = "sidechain://playlist/" + playlistId;
        
        // Copy to clipboard
        juce::SystemClipboard::copyTextToClipboard(shareLink);
        
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::InfoIcon,
            "Playlist Link Copied",
            "Playlist link copied to clipboard:\n" + shareLink);
    };
    addChildComponent(playlistDetailComponent.get());

    //==========================================================================
    // Create MidiChallenges component (R.2.2.4.1)
    midiChallengesComponent = std::make_unique<MidiChallenges>();
    midiChallengesComponent->setNetworkClient(networkClient.get());
    if (userDataStore)
        midiChallengesComponent->setCurrentUserId(userDataStore->getUserId());
    midiChallengesComponent->onBackPressed = [this]() {
        navigateBack();
    };
    midiChallengesComponent->onChallengeSelected = [this](const juce::String& challengeId) {
        challengeIdToView = challengeId;
        showView(AppView::MidiChallengeDetail);
    };
    addChildComponent(midiChallengesComponent.get());

    //==========================================================================
    // Create MidiChallengeDetail component (R.2.2.4.2)
    midiChallengeDetailComponent = std::make_unique<MidiChallengeDetail>();
    midiChallengeDetailComponent->setNetworkClient(networkClient.get());
    midiChallengeDetailComponent->setAudioPlayer(&audioProcessor.getAudioPlayer());
    if (userDataStore)
        midiChallengeDetailComponent->setCurrentUserId(userDataStore->getUserId());
    midiChallengeDetailComponent->onBackPressed = [this]() {
        navigateBack();
    };
    midiChallengeDetailComponent->onSubmitEntry = [this]() {
        // Navigate to recording view with challenge context
        // TODO: Pass challenge ID to recording component for constraint validation
        showView(AppView::Recording);
    };
    midiChallengeDetailComponent->onEntrySelected = [](const juce::String& entryId) {
        // TODO: Navigate to entry/post detail
        Log::info("Entry selected: " + entryId);
    };
    addChildComponent(midiChallengeDetailComponent.get());

    //==========================================================================
    // Create SavedPosts component (P0 Social Feature)
    savedPostsComponent = std::make_unique<SavedPosts>();
    savedPostsComponent->setNetworkClient(networkClient.get());
    if (userDataStore)
        savedPostsComponent->setCurrentUserId(userDataStore->getUserId());
    savedPostsComponent->onBackPressed = [this]() {
        navigateBack();
    };
    savedPostsComponent->onPostClicked = [this](const FeedPost& post) {
        // Navigate to user profile when post is clicked
        showProfile(post.userId);
    };
    savedPostsComponent->onPlayClicked = [this](const FeedPost& post) {
        if (!post.audioUrl.isEmpty())
        {
            audioProcessor.getAudioPlayer().loadAndPlay(post.audioUrl, post.id);
            savedPostsComponent->setCurrentlyPlayingPost(post.id);
        }
    };
    savedPostsComponent->onPauseClicked = [this](const FeedPost& /*post*/) {
        audioProcessor.getAudioPlayer().stop();
        savedPostsComponent->clearPlayingState();
    };
    savedPostsComponent->onUserClicked = [this](const juce::String& userId) {
        showProfile(userId);
    };
    addChildComponent(savedPostsComponent.get());

    //==========================================================================
    // Create Story Highlight dialogs
    createHighlightDialog = std::make_unique<CreateHighlightDialog>();
    createHighlightDialog->setNetworkClient(networkClient.get());
    createHighlightDialog->onHighlightCreated = [this](const juce::String& highlightId) {
        Log::info("PluginEditor: Highlight created: " + highlightId);
        // Refresh profile to show new highlight
        if (currentView == AppView::Profile && profileComponent)
        {
            profileComponent->refresh();
        }
    };
    // Not added as child - shown as modal overlay when needed

    selectHighlightDialog = std::make_unique<SelectHighlightDialog>();
    selectHighlightDialog->setNetworkClient(networkClient.get());
    selectHighlightDialog->onHighlightSelected = [this](const juce::String& highlightId) {
        Log::info("PluginEditor: Story added to highlight: " + highlightId);
        // Show success message
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::InfoIcon,
            "Success",
            "Story added to highlight!");
    };
    selectHighlightDialog->onCreateNewClicked = [this]() {
        // Show create dialog, then after creation add the story
        showCreateHighlightDialog();
    };
    // Not added as child - shown as modal overlay when needed

    //==========================================================================
    // Create ShareToMessageDialog for sharing posts/stories to DMs
    shareToMessageDialog = std::make_unique<ShareToMessageDialog>();
    shareToMessageDialog->onShareComplete = [this]() {
        Log::info("PluginEditor: Content shared to DM successfully");
        // Optionally show success message
    };
    shareToMessageDialog->onCancelled = [this]() {
        Log::debug("PluginEditor: Share to DM cancelled");
    };
    // Not added as child - shown as modal overlay when needed

    // Setup synth unlock callback
    audioProcessor.onSynthUnlocked = [this]() {
        juce::MessageManager::callAsync([this]() {
            Log::info("PluginEditor: Synth unlocked! Showing synth view");
            audioProcessor.setSynthEnabled(true);
            showView(AppView::HiddenSynth);
            if (hiddenSynthComponent)
                hiddenSynthComponent->playUnlockAnimation();
        });
    };

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
        // Handle new message action - navigate to discovery to select user
        Log::info("MessagesList: onNewMessage - about to show Discovery view");
        showView(AppView::Discovery);
        Log::info("MessagesList: onNewMessage - showView returned");
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
    profileComponent->onSavedPostsClicked = [this]() {
        // Navigate to saved posts view
        showSavedPosts();
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
    profileComponent->onViewStoryClicked = [this](const juce::String& userId) {
        showUserStory(userId);
    };
    profileComponent->onNavigateToProfile = [this](const juce::String& userId) {
        showProfile(userId);
    };
    profileComponent->onHighlightClicked = [this](const StoryHighlight& highlight) {
        showHighlightStories(highlight);
    };
    profileComponent->onCreateHighlightClicked = [this]() {
        showCreateHighlightDialog();
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
        {
            juce::String userId = userDataStore->getUserId();
            if (userId.isNotEmpty())
            {
                Log::info("Header::onProfileClicked: Showing profile for user: " + userId);
                showProfile(userId);
            }
            else
            {
                Log::warn("Header::onProfileClicked: User ID is empty, showing ProfileSetup");
                showView(AppView::ProfileSetup);
            }
        }
        else
        {
            Log::warn("Header::onProfileClicked: userDataStore is null or user not logged in, showing ProfileSetup");
            showView(AppView::ProfileSetup);  // Fallback to setup if no user ID
        }
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
    headerComponent->onProfileStoryClicked = [this]() {
        // Show current user's story
        if (userDataStore && !userDataStore->getUserId().isEmpty())
            showUserStory(userDataStore->getUserId());
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
    // Shutdown async system first to prevent pending callbacks from accessing
    // destroyed objects and to allow detached threads to exit cleanly
    Async::shutdown();

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

    // Disconnect StreamChat WebSocket
    if (streamChatClient)
        streamChatClient->disconnectWebSocket();
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

    if (hiddenSynthComponent)
        hiddenSynthComponent->setBounds(contentBounds);

    if (playlistsComponent)
        playlistsComponent->setBounds(contentBounds);

    if (playlistDetailComponent)
        playlistDetailComponent->setBounds(contentBounds);

    if (midiChallengesComponent)
        midiChallengesComponent->setBounds(contentBounds);

    if (midiChallengeDetailComponent)
        midiChallengeDetailComponent->setBounds(contentBounds);

    if (savedPostsComponent)
        savedPostsComponent->setBounds(contentBounds);
}

//==============================================================================
void SidechainAudioProcessorEditor::showView(AppView view)
{
    Log::info("showView: entering, view=" + juce::String(static_cast<int>(view)) + ", currentView=" + juce::String(static_cast<int>(currentView)));

    // Get the component to show
    juce::Component* componentToShow = nullptr;
    juce::Component* componentToHide = nullptr;

    // Determine which component to show and which to hide
    switch (view)
    {
        case AppView::Authentication:
            componentToShow = authComponent.get();
            break;
        case AppView::ProfileSetup:
            componentToShow = profileSetupComponent.get();
            break;
        case AppView::PostsFeed:
            componentToShow = postsFeedComponent.get();
            break;
        case AppView::Recording:
            componentToShow = recordingComponent.get();
            break;
        case AppView::Upload:
            componentToShow = uploadComponent.get();
            break;
        case AppView::StoryRecording:
            componentToShow = storyRecordingComponent.get();
            break;
        case AppView::StoryViewer:
            componentToShow = storyViewerComponent.get();
            break;
        case AppView::HiddenSynth:
            componentToShow = hiddenSynthComponent.get();
            break;
        case AppView::Playlists:
            componentToShow = playlistsComponent.get();
            break;
        case AppView::PlaylistDetail:
            componentToShow = playlistDetailComponent.get();
            break;
        case AppView::MidiChallenges:
            componentToShow = midiChallengesComponent.get();
            break;
        case AppView::MidiChallengeDetail:
            componentToShow = midiChallengeDetailComponent.get();
            break;
        case AppView::SavedPosts:
            componentToShow = savedPostsComponent.get();
            break;
        case AppView::Discovery:
            componentToShow = userDiscoveryComponent.get();
            break;
        case AppView::Profile:
            componentToShow = profileComponent.get();
            break;
        case AppView::Search:
            componentToShow = searchComponent.get();
            break;
        case AppView::Messages:
            componentToShow = messagesListComponent.get();
            break;
        case AppView::MessageThread:
            componentToShow = messageThreadComponent.get();
            break;
    }

    // Determine which component to hide (current view)
    switch (currentView)
    {
        case AppView::Authentication:
            componentToHide = authComponent.get();
            break;
        case AppView::ProfileSetup:
            componentToHide = profileSetupComponent.get();
            break;
        case AppView::PostsFeed:
            componentToHide = postsFeedComponent.get();
            break;
        case AppView::Recording:
            componentToHide = recordingComponent.get();
            break;
        case AppView::Upload:
            componentToHide = uploadComponent.get();
            break;
        case AppView::StoryRecording:
            componentToHide = storyRecordingComponent.get();
            break;
        case AppView::HiddenSynth:
            componentToHide = hiddenSynthComponent.get();
            break;
        case AppView::Playlists:
            componentToHide = playlistsComponent.get();
            break;
        case AppView::PlaylistDetail:
            componentToHide = playlistDetailComponent.get();
            break;
        case AppView::MidiChallenges:
            componentToHide = midiChallengesComponent.get();
            break;
        case AppView::MidiChallengeDetail:
            componentToHide = midiChallengeDetailComponent.get();
            break;
        case AppView::SavedPosts:
            componentToHide = savedPostsComponent.get();
            break;
        case AppView::Discovery:
            componentToHide = userDiscoveryComponent.get();
            break;
        case AppView::Profile:
            componentToHide = profileComponent.get();
            break;
        case AppView::Search:
            componentToHide = searchComponent.get();
            break;
        case AppView::Messages:
            componentToHide = messagesListComponent.get();
            break;
        case AppView::MessageThread:
            componentToHide = messageThreadComponent.get();
            break;
    }

    Log::info("showView: componentToShow=" + juce::String(componentToShow != nullptr ? "valid" : "null") +
              ", componentToHide=" + juce::String(componentToHide != nullptr ? "valid" : "null"));

    // TEMPORARILY DISABLED: Animate slide transition - testing if animation causes crash
    // if (componentToShow && componentToHide && componentToShow != componentToHide && currentView != AppView::Authentication)
    if (false)
    {
        Log::info("showView: starting animation");
        // Set up initial positions for slide animation
        // Use content bounds (below header) for post-login views
        auto bounds = getLocalBounds().withTrimmedTop(Header::HEADER_HEIGHT);
        
        // Hide all other components immediately (no animation)
        if (authComponent && authComponent.get() != componentToShow && authComponent.get() != componentToHide)
            authComponent->setVisible(false);
        if (profileSetupComponent && profileSetupComponent.get() != componentToShow && profileSetupComponent.get() != componentToHide)
            profileSetupComponent->setVisible(false);
        if (postsFeedComponent && postsFeedComponent.get() != componentToShow && postsFeedComponent.get() != componentToHide)
            postsFeedComponent->setVisible(false);
        if (recordingComponent && recordingComponent.get() != componentToShow && recordingComponent.get() != componentToHide)
            recordingComponent->setVisible(false);
        if (uploadComponent && uploadComponent.get() != componentToShow && uploadComponent.get() != componentToHide)
            uploadComponent->setVisible(false);
        if (userDiscoveryComponent && userDiscoveryComponent.get() != componentToShow && userDiscoveryComponent.get() != componentToHide)
            userDiscoveryComponent->setVisible(false);
        if (profileComponent && profileComponent.get() != componentToShow && profileComponent.get() != componentToHide)
            profileComponent->setVisible(false);
        if (searchComponent && searchComponent.get() != componentToShow && searchComponent.get() != componentToHide)
            searchComponent->setVisible(false);
        if (messagesListComponent && messagesListComponent.get() != componentToShow && messagesListComponent.get() != componentToHide)
            messagesListComponent->setVisible(false);
        if (messageThreadComponent && messageThreadComponent.get() != componentToShow && messageThreadComponent.get() != componentToHide)
            messageThreadComponent->setVisible(false);
        if (storyRecordingComponent && storyRecordingComponent.get() != componentToShow && storyRecordingComponent.get() != componentToHide)
            storyRecordingComponent->setVisible(false);
        if (playlistsComponent && playlistsComponent.get() != componentToShow && playlistsComponent.get() != componentToHide)
            playlistsComponent->setVisible(false);
        if (playlistDetailComponent && playlistDetailComponent.get() != componentToShow && playlistDetailComponent.get() != componentToHide)
            playlistDetailComponent->setVisible(false);
        if (midiChallengesComponent && midiChallengesComponent.get() != componentToShow && midiChallengesComponent.get() != componentToHide)
            midiChallengesComponent->setVisible(false);
        if (midiChallengeDetailComponent && midiChallengeDetailComponent.get() != componentToShow && midiChallengeDetailComponent.get() != componentToHide)
            midiChallengeDetailComponent->setVisible(false);
        if (savedPostsComponent && savedPostsComponent.get() != componentToShow && savedPostsComponent.get() != componentToHide)
            savedPostsComponent->setVisible(false);

        // Position new component off-screen to the right
        Log::info("showView: positioning components for animation");
        componentToShow->setBounds(bounds.withX(bounds.getRight()));
        componentToShow->setVisible(true);
        componentToShow->toFront(false);

        // Animate: slide old component to the left, new component from the right
        Log::info("showView: calling animateComponent");
        viewAnimator.animateComponent(componentToHide, bounds.withX(-bounds.getWidth()), 1.0f, 250, false, 1.0, 1.0);
        viewAnimator.animateComponent(componentToShow, bounds, 1.0f, 250, false, 1.0, 1.0);
        Log::info("showView: animation started");
    }
    else
    {
        // No animation - just show/hide immediately (for first view or authentication)
        auto contentBounds = getLocalBounds().withTrimmedTop(Header::HEADER_HEIGHT);

        if (authComponent)
            authComponent->setVisible(view == AppView::Authentication);
        if (profileSetupComponent)
            profileSetupComponent->setVisible(view == AppView::ProfileSetup);
        if (postsFeedComponent)
        {
            postsFeedComponent->setBounds(contentBounds);
            postsFeedComponent->setVisible(view == AppView::PostsFeed);
        }
        if (recordingComponent)
        {
            recordingComponent->setBounds(contentBounds);
            recordingComponent->setVisible(view == AppView::Recording);
        }
        if (uploadComponent)
        {
            uploadComponent->setBounds(contentBounds);
            uploadComponent->setVisible(view == AppView::Upload);
        }
        if (userDiscoveryComponent)
        {
            userDiscoveryComponent->setBounds(contentBounds);
            userDiscoveryComponent->setVisible(view == AppView::Discovery);
        }
        if (profileComponent)
        {
            profileComponent->setBounds(contentBounds);
            profileComponent->setVisible(view == AppView::Profile);
        }
        if (searchComponent)
        {
            searchComponent->setBounds(contentBounds);
            searchComponent->setVisible(view == AppView::Search);
        }
        if (messagesListComponent)
        {
            messagesListComponent->setBounds(contentBounds);
            messagesListComponent->setVisible(view == AppView::Messages);
        }
        if (messageThreadComponent)
        {
            messageThreadComponent->setBounds(contentBounds);
            messageThreadComponent->setVisible(view == AppView::MessageThread);
        }
        if (storyRecordingComponent)
        {
            storyRecordingComponent->setBounds(contentBounds);
            storyRecordingComponent->setVisible(view == AppView::StoryRecording);
        }
        if (storyViewerComponent)
        {
            storyViewerComponent->setBounds(contentBounds);
            storyViewerComponent->setVisible(view == AppView::StoryViewer);
        }
        if (playlistsComponent)
        {
            playlistsComponent->setBounds(contentBounds);
            playlistsComponent->setVisible(view == AppView::Playlists);
        }
        if (playlistDetailComponent)
        {
            playlistDetailComponent->setBounds(contentBounds);
            playlistDetailComponent->setVisible(view == AppView::PlaylistDetail);
        }
        if (midiChallengesComponent)
        {
            midiChallengesComponent->setBounds(contentBounds);
            midiChallengesComponent->setVisible(view == AppView::MidiChallenges);
        }
        if (midiChallengeDetailComponent)
        {
            midiChallengeDetailComponent->setBounds(contentBounds);
            midiChallengeDetailComponent->setVisible(view == AppView::MidiChallengeDetail);
        }
        if (savedPostsComponent)
        {
            savedPostsComponent->setBounds(contentBounds);
            savedPostsComponent->setVisible(view == AppView::SavedPosts);
        }
        if (hiddenSynthComponent)
        {
            hiddenSynthComponent->setBounds(contentBounds);
            hiddenSynthComponent->setVisible(view == AppView::HiddenSynth);
        }
    }

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

    // Set up components (this runs regardless of animation)
    switch (view)
    {
        case AppView::Authentication:
            if (authComponent)
            {
                authComponent->reset();
                
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
                
            }
            break;

        case AppView::PostsFeed:
            if (postsFeedComponent)
            {
                postsFeedComponent->setUserInfo(username, email, profilePicUrl);
                postsFeedComponent->loadFeed();
            }
            break;

        case AppView::Recording:
            
            break;

        case AppView::Upload:
            
            break;

        case AppView::StoryRecording:

            break;

        case AppView::HiddenSynth:
            // Synth is self-contained, no additional setup needed
            break;

        case AppView::Playlists:
            if (playlistsComponent)
            {
                playlistsComponent->loadPlaylists();
            }
            break;

        case AppView::PlaylistDetail:
            if (playlistDetailComponent && playlistIdToView.isNotEmpty())
            {
                playlistDetailComponent->loadPlaylist(playlistIdToView);
            }
            break;

        case AppView::MidiChallenges:
            if (midiChallengesComponent)
            {
                midiChallengesComponent->loadChallenges();
            }
            break;
        case AppView::MidiChallengeDetail:
            if (midiChallengeDetailComponent && challengeIdToView.isNotEmpty())
            {
                midiChallengeDetailComponent->loadChallenge(challengeIdToView);
            }
            break;

        case AppView::SavedPosts:
            if (savedPostsComponent)
            {
                if (userDataStore)
                    savedPostsComponent->setCurrentUserId(userDataStore->getUserId());
                savedPostsComponent->loadSavedPosts();
            }
            break;

        case AppView::Discovery:
            Log::info("showView: initializing Discovery component");
            if (userDiscoveryComponent)
            {
                Log::info("showView: calling setCurrentUserId");
                userDiscoveryComponent->setCurrentUserId(authToken);  // Use auth token or actual user ID
                Log::info("showView: calling loadDiscoveryData");
                userDiscoveryComponent->loadDiscoveryData();
                Log::info("showView: Discovery initialization complete");
            }
            break;

        case AppView::Profile:
            if (profileComponent)
            {
                // Set current user ID for "is own profile" checks
                if (userDataStore)
                    profileComponent->setCurrentUserId(userDataStore->getUserId());

                // Ensure we have a valid user ID to load
                if (profileUserIdToView.isEmpty())
                {
                    Log::error("PluginEditor::showView: profileUserIdToView is empty, cannot load profile");
                    // Fallback to current user's profile if available
                    if (userDataStore && !userDataStore->getUserId().isEmpty())
                    {
                        profileUserIdToView = userDataStore->getUserId();
                        Log::info("PluginEditor::showView: Using current user ID as fallback: " + profileUserIdToView);
                    }
                    else
                    {
                        Log::error("PluginEditor::showView: No user ID available, cannot show profile");
                        // Navigate back or show error
                        navigateBack();
                        break;
                    }
                }

                // Load the profile for the specified user
                profileComponent->loadProfile(profileUserIdToView);
                
            }
            break;

        case AppView::Search:
            if (searchComponent)
            {
                if (userDataStore)
                    searchComponent->setCurrentUserId(userDataStore->getUserId());
                
                searchComponent->focusSearchInput();
            }
            break;

        case AppView::Messages:
            if (messagesListComponent)
            {
                messagesListComponent->loadChannels();
            }
            break;

        case AppView::MessageThread:
            if (messageThreadComponent)
            {
                if (userDataStore)
                    messageThreadComponent->setCurrentUserId(userDataStore->getUserId());
                messageThreadComponent->loadChannel(messageChannelType, messageChannelId);
            }
            break;
    }

    repaint();
}

void SidechainAudioProcessorEditor::showProfile(const juce::String& userId)
{
    if (userId.isEmpty())
    {
        Log::error("PluginEditor::showProfile: userId is empty");
        // Fallback to current user's profile if available
        if (userDataStore && !userDataStore->getUserId().isEmpty())
        {
            profileUserIdToView = userDataStore->getUserId();
            Log::info("PluginEditor::showProfile: Using current user ID as fallback: " + profileUserIdToView);
        }
        else
        {
            Log::error("PluginEditor::showProfile: No user ID available, cannot show profile");
            return;
        }
    }
    else
    {
        profileUserIdToView = userId.trim();
        if (profileUserIdToView.isEmpty())
        {
            Log::error("PluginEditor::showProfile: userId is empty after trimming");
            return;
        }
    }
    
    Log::info("PluginEditor::showProfile: Showing profile for user: " + profileUserIdToView);
    showView(AppView::Profile);
}

void SidechainAudioProcessorEditor::showMessageThread(const juce::String& channelType, const juce::String& channelId)
{
    messageChannelType = channelType;
    messageChannelId = channelId;
    showView(AppView::MessageThread);
}

void SidechainAudioProcessorEditor::showPlaylists()
{
    showView(AppView::Playlists);
}

void SidechainAudioProcessorEditor::showPlaylistDetail(const juce::String& playlistId)
{
    playlistIdToView = playlistId;
    showView(AppView::PlaylistDetail);
}

void SidechainAudioProcessorEditor::showSavedPosts()
{
    showView(AppView::SavedPosts);
}

void SidechainAudioProcessorEditor::checkForActiveStories()
{
    if (!networkClient || !userDataStore || userDataStore->getUserId().isEmpty())
        return;

    juce::String currentUserId = userDataStore->getUserId();

    // Fetch stories feed and check if current user has any active stories
    networkClient->getStoriesFeed([this, currentUserId](Outcome<juce::var> result) {
        juce::MessageManager::callAsync([this, result, currentUserId]() {
            bool hasStory = false;

            if (result.isOk() && result.getValue().isObject())
            {
                auto response = result.getValue();
                if (response.hasProperty("stories"))
                {
                    auto* storiesArray = response["stories"].getArray();
                    if (storiesArray)
                    {
                        // Check if any story belongs to current user and is not expired
                        for (const auto& storyVar : *storiesArray)
                        {
                            juce::String storyUserId = storyVar["user_id"].toString();
                            if (storyUserId == currentUserId)
                            {
                                // Check expiration
                                juce::String expiresAtStr = storyVar["expires_at"].toString();
                                if (expiresAtStr.isNotEmpty())
                                {
                                    juce::Time expiresAt = juce::Time::fromISO8601(expiresAtStr);
                                    if (expiresAt.toMilliseconds() > 0 && juce::Time::getCurrentTime() < expiresAt)
                                    {
                                        hasStory = true;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // Update header
            if (headerComponent)
            {
                headerComponent->setHasStories(hasStory);
            }
        });
    });
}

void SidechainAudioProcessorEditor::showUserStory(const juce::String& userId)
{
    if (!networkClient || userId.isEmpty())
        return;

    // Fetch stories for this user
    networkClient->getStoriesFeed([this, userId](Outcome<juce::var> result) {
        juce::MessageManager::callAsync([this, result, userId]() {
            if (!result.isOk() || !result.getValue().isObject())
            {
                Log::error("PluginEditor: Failed to fetch stories: " + result.getError());
                return;
            }

            auto response = result.getValue();
            if (!response.hasProperty("stories"))
            {
                Log::warn("PluginEditor: No stories in response");
                return;
            }

            auto* storiesArray = response["stories"].getArray();
            if (!storiesArray)
            {
                Log::warn("PluginEditor: Stories array is null");
                return;
            }

            // Filter stories for this user
            std::vector<StoryData> userStories;
            int startIndex = 0;
            for (int i = 0; i < storiesArray->size(); ++i)
            {
                const auto& storyVar = (*storiesArray)[i];
                juce::String storyUserId = storyVar["user_id"].toString();
                
                if (storyUserId == userId)
                {
                    StoryData story;
                    story.id = storyVar["id"].toString();
                    story.userId = storyUserId;
                    story.username = storyVar.hasProperty("user") ? storyVar["user"]["username"].toString() : "";
                    story.userAvatarUrl = storyVar.hasProperty("user") ? storyVar["user"]["avatar_url"].toString() : "";
                    story.audioUrl = storyVar["audio_url"].toString();
                    story.audioDuration = static_cast<float>(storyVar["audio_duration"]);
                    story.midiData = storyVar["midi_data"];
                    story.midiPatternId = storyVar["midi_pattern_id"].toString();
                    story.viewCount = static_cast<int>(storyVar["view_count"]);
                    story.viewed = static_cast<bool>(storyVar["viewed"]);

                    // Parse timestamps
                    juce::String expiresAtStr = storyVar["expires_at"].toString();
                    if (expiresAtStr.isNotEmpty())
                        story.expiresAt = juce::Time::fromISO8601(expiresAtStr);
                    else
                        story.expiresAt = juce::Time::getCurrentTime() + juce::RelativeTime::hours(24);

                    juce::String createdAtStr = storyVar["created_at"].toString();
                    if (createdAtStr.isNotEmpty())
                        story.createdAt = juce::Time::fromISO8601(createdAtStr);
                    else
                        story.createdAt = juce::Time::getCurrentTime();

                    if (startIndex == 0)
                        startIndex = static_cast<int>(userStories.size());
                    
                    userStories.push_back(story);
                }
            }

            if (userStories.empty())
            {
                Log::info("PluginEditor: No active stories for user: " + userId);
                return;
            }

            // Set stories and show viewer
            if (storyViewerComponent)
            {
                storyViewerComponent->setStories(userStories, startIndex);
                showView(AppView::StoryViewer);
            }
        });
    });
}

void SidechainAudioProcessorEditor::showHighlightStories(const StoryHighlight& highlight)
{
    if (!networkClient || highlight.id.isEmpty())
        return;

    // Fetch the highlight with its stories
    networkClient->getHighlight(highlight.id, [this, highlightName = highlight.name](Outcome<juce::var> result) {
        juce::MessageManager::callAsync([this, result, highlightName]() {
            if (!result.isOk() || !result.getValue().isObject())
            {
                Log::error("PluginEditor: Failed to fetch highlight: " + result.getError());
                return;
            }

            auto response = result.getValue();

            // Parse stories from the highlight response
            std::vector<StoryData> highlightStories;

            // The response may have stories nested in different ways
            juce::var storiesVar;
            if (response.hasProperty("stories"))
                storiesVar = response["stories"];
            else if (response.hasProperty("highlighted_stories"))
                storiesVar = response["highlighted_stories"];

            if (storiesVar.isArray())
            {
                auto* storiesArray = storiesVar.getArray();
                for (int i = 0; i < storiesArray->size(); ++i)
                {
                    const auto& storyVar = (*storiesArray)[i];

                    // Handle nested "story" property from highlighted_stories join table
                    juce::var storyData = storyVar.hasProperty("story") ? storyVar["story"] : storyVar;

                    StoryData story;
                    story.id = storyData["id"].toString();
                    story.userId = storyData["user_id"].toString();
                    story.username = storyData.hasProperty("user") ? storyData["user"]["username"].toString() : "";
                    story.userAvatarUrl = storyData.hasProperty("user") ? storyData["user"]["avatar_url"].toString() : "";
                    story.audioUrl = storyData["audio_url"].toString();
                    story.audioDuration = static_cast<float>(storyData["audio_duration"]);
                    story.midiData = storyData["midi_data"];
                    story.midiPatternId = storyData["midi_pattern_id"].toString();
                    story.viewCount = static_cast<int>(storyData["view_count"]);
                    story.viewed = true; // Highlights are already "viewed" stories

                    // Parse timestamps - highlights don't expire
                    story.expiresAt = juce::Time::getCurrentTime() + juce::RelativeTime::days(365 * 10);
                    juce::String createdAtStr = storyData["created_at"].toString();
                    if (createdAtStr.isNotEmpty())
                        story.createdAt = juce::Time::fromISO8601(createdAtStr);
                    else
                        story.createdAt = juce::Time::getCurrentTime();

                    highlightStories.push_back(story);
                }
            }

            if (highlightStories.empty())
            {
                Log::info("PluginEditor: No stories in highlight: " + highlightName);
                juce::AlertWindow::showMessageBoxAsync(
                    juce::MessageBoxIconType::InfoIcon,
                    "Empty Highlight",
                    "This highlight has no stories yet.");
                return;
            }

            Log::info("PluginEditor: Showing " + juce::String(highlightStories.size()) + " stories from highlight: " + highlightName);

            // Set stories and show viewer
            if (storyViewerComponent)
            {
                storyViewerComponent->setStories(highlightStories, 0);
                showView(AppView::StoryViewer);
            }
        });
    });
}

void SidechainAudioProcessorEditor::showCreateHighlightDialog()
{
    if (!createHighlightDialog)
        return;

    createHighlightDialog->showModal(this);
}

void SidechainAudioProcessorEditor::showSelectHighlightDialog(const juce::String& storyId)
{
    if (!selectHighlightDialog || storyId.isEmpty())
        return;

    if (userDataStore)
        selectHighlightDialog->setCurrentUserId(userDataStore->getUserId());
    selectHighlightDialog->setStoryId(storyId);
    selectHighlightDialog->showModal(this);
}

void SidechainAudioProcessorEditor::showSharePostToMessage(const FeedPost& post)
{
    if (!shareToMessageDialog)
        return;

    // Set up the dialog with required clients
    shareToMessageDialog->setNetworkClient(networkClient.get());
    shareToMessageDialog->setStreamChatClient(streamChatClient.get());
    if (userDataStore)
        shareToMessageDialog->setCurrentUserId(userDataStore->getUserId());

    // Set the post to share
    shareToMessageDialog->setPostToShare(post);

    // Show the dialog
    shareToMessageDialog->showModal(this);
}

void SidechainAudioProcessorEditor::showShareStoryToMessage(const StoryData& story)
{
    if (!shareToMessageDialog)
        return;

    // Set up the dialog with required clients
    shareToMessageDialog->setNetworkClient(networkClient.get());
    shareToMessageDialog->setStreamChatClient(streamChatClient.get());
    if (userDataStore)
        shareToMessageDialog->setCurrentUserId(userDataStore->getUserId());

    // Set the story to share
    shareToMessageDialog->setStoryToShare(story);

    // Show the dialog
    shareToMessageDialog->showModal(this);
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
    if (searchComponent) searchComponent->setVisible(false);
    if (messagesListComponent) messagesListComponent->setVisible(false);
    if (messageThreadComponent) messageThreadComponent->setVisible(false);
    if (storyRecordingComponent) storyRecordingComponent->setVisible(false);
    if (storyViewerComponent) storyViewerComponent->setVisible(false);
    if (hiddenSynthComponent) hiddenSynthComponent->setVisible(false);
    if (playlistsComponent) playlistsComponent->setVisible(false);
    if (playlistDetailComponent) playlistDetailComponent->setVisible(false);
    if (midiChallengesComponent) midiChallengesComponent->setVisible(false);
    if (midiChallengeDetailComponent) midiChallengeDetailComponent->setVisible(false);
    if (savedPostsComponent) savedPostsComponent->setVisible(false);

    // Content bounds for positioning
    auto contentBounds = getLocalBounds().withTrimmedTop(Header::HEADER_HEIGHT);

    // Show the previous view with proper bounds
    switch (previousView)
    {
        case AppView::PostsFeed:
            if (postsFeedComponent)
            {
                postsFeedComponent->setBounds(contentBounds);
                postsFeedComponent->setVisible(true);
            }
            break;
        case AppView::Discovery:
            if (userDiscoveryComponent)
            {
                userDiscoveryComponent->setCurrentUserId(authToken);
                userDiscoveryComponent->loadDiscoveryData();
                userDiscoveryComponent->setBounds(contentBounds);
                userDiscoveryComponent->setVisible(true);
            }
            break;
        case AppView::Profile:
            if (profileComponent)
            {
                profileComponent->setBounds(contentBounds);
                profileComponent->setVisible(true);
            }
            break;
        case AppView::Recording:
            if (recordingComponent)
            {
                recordingComponent->setBounds(contentBounds);
                recordingComponent->setVisible(true);
            }
            break;
        case AppView::ProfileSetup:
            if (profileSetupComponent)
            {
                profileSetupComponent->setBounds(contentBounds);
                profileSetupComponent->setVisible(true);
            }
            break;
        case AppView::Search:
            if (searchComponent)
            {
                searchComponent->setBounds(contentBounds);
                searchComponent->setVisible(true);
            }
            break;
        case AppView::Messages:
            if (messagesListComponent)
            {
                messagesListComponent->setBounds(contentBounds);
                messagesListComponent->setVisible(true);
            }
            break;
        case AppView::MessageThread:
            if (messageThreadComponent)
            {
                if (userDataStore)
                    messageThreadComponent->setCurrentUserId(userDataStore->getUserId());
                messageThreadComponent->loadChannel(messageChannelType, messageChannelId);
                messageThreadComponent->setBounds(contentBounds);
                messageThreadComponent->setVisible(true);
            }
            break;
        case AppView::Playlists:
            if (playlistsComponent)
            {
                playlistsComponent->setBounds(contentBounds);
                playlistsComponent->setVisible(true);
            }
            break;
        case AppView::MidiChallenges:
            if (midiChallengesComponent)
            {
                midiChallengesComponent->setBounds(contentBounds);
                midiChallengesComponent->setVisible(true);
            }
            break;
        case AppView::SavedPosts:
            if (savedPostsComponent)
            {
                savedPostsComponent->setBounds(contentBounds);
                savedPostsComponent->setVisible(true);
            }
            break;
        default:
            if (postsFeedComponent)
            {
                postsFeedComponent->setBounds(contentBounds);
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

                    // Check if user has active stories and update header
                    checkForActiveStories();

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
            // Only show popup in release builds, not in debug mode
#if !JUCE_DEBUG
            juce::MessageManager::callAsync([]() {
                juce::AlertWindow::showMessageBoxAsync(
                    juce::MessageBoxIconType::WarningIcon,
                    "Previous Sidechain Session Ended Unexpectedly",
                    "The plugin did not shut down cleanly during the last session. "
                    "This may indicate a crash or unexpected termination.\n\n"
                    "If this happens frequently, please report it with details about what you were doing.",
                    "OK"
                );
            });
#endif

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

            // Check for active stories
            checkForActiveStories();
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

        // Check if new notifications arrived (unseen count increased)
        static int previousUnseenCount = -1;
        bool newNotifications = notifResult.unseen > previousUnseenCount && previousUnseenCount >= 0;
        previousUnseenCount = notifResult.unseen;
        
        // Update counts
        if (notificationBell)
        {
            notificationBell->setUnseenCount(notifResult.unseen);
            notificationBell->setUnreadCount(notifResult.unread);
        }
        
        // Play notification sound if enabled and new notifications arrived
        if (newNotifications && userDataStore && userDataStore->isNotificationSoundEnabled())
        {
            NotificationSound::playBeep();
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

        // Show OS notification for new notifications (most recent first)
        if (newNotifications && items.size() > 0)
        {
            // Get the first (most recent) notification to show
            const auto& latestNotification = items.getFirst();
            juce::String notificationTitle = "Sidechain";
            juce::String notificationMessage = latestNotification.getDisplayText();
            
            // Show desktop notification (checks isSupported internally)
            OSNotification::show(notificationTitle, notificationMessage, "", 
                               userDataStore && userDataStore->isNotificationSoundEnabled());
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
        // Check if new notifications arrived (unseen count increased)
        static int previousUnseenCount = 0;
        bool newNotifications = unseen > previousUnseenCount && previousUnseenCount >= 0;
        previousUnseenCount = unseen;
        
        if (notificationBell)
        {
            notificationBell->setUnseenCount(unseen);
            notificationBell->setUnreadCount(unread);
        }
        
        // Play notification sound if enabled and new notifications arrived
        if (newNotifications && userDataStore && userDataStore->isNotificationSoundEnabled())
        {
            NotificationSound::playBeep();
        }
        
        // Fetch and show OS notification for new notifications
        if (newNotifications)
        {
            // Fetch the most recent notification to show in OS notification
            networkClient->getNotifications(1, 0, [this](Outcome<NetworkClient::NotificationResult> result) {
                if (result.isOk())
                {
                    auto notifResult = result.getValue();
                    if (notifResult.notifications.isArray() && notifResult.notifications.size() > 0)
                    {
                        auto latestNotification = NotificationItem::fromJson(notifResult.notifications[0]);
                        juce::String notificationTitle = "Sidechain";
                        juce::String notificationMessage = latestNotification.getDisplayText();
                        
                        // Show desktop notification (checks isSupported internally)
                        OSNotification::show(notificationTitle, notificationMessage, "",
                                           userDataStore && userDataStore->isNotificationSoundEnabled());
                    }
                }
            });
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

    // Show OAuth waiting UI with animated spinner and countdown (8.3.11.9-12)
    if (authComponent)
    {
        authComponent->showOAuthWaiting(provider, MAX_OAUTH_POLLS);  // 300 seconds = 5 minutes
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

    // Update countdown timer in Auth component (8.3.11.10)
    int secondsRemaining = MAX_OAUTH_POLLS - oauthPollCount;
    if (authComponent)
    {
        authComponent->updateOAuthCountdown(secondsRemaining);
    }

    // Check if we've exceeded max polls (5 minutes)
    if (oauthPollCount > MAX_OAUTH_POLLS)
    {
        stopOAuthPolling();
        if (authComponent)
        {
            authComponent->hideOAuthWaiting();
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

                    // Hide OAuth waiting screen before transitioning
                    if (authComponent)
                        authComponent->hideOAuthWaiting();

                    onLoginSuccess(userName, userEmail, token);
                }
            }
            else if (status == "error")
            {
                // OAuth failed
                stopOAuthPolling();
                juce::String errorMsg = Json::getString(responseData, "message", "Authentication failed");
                if (authComponent)
                {
                    authComponent->hideOAuthWaiting();
                    authComponent->showError(errorMsg);
                }
            }
            else if (status == "expired" || status == "not_found")
            {
                // Session expired or invalid
                stopOAuthPolling();
                if (authComponent)
                {
                    authComponent->hideOAuthWaiting();
                    authComponent->showError("Authentication session expired. Please try again.");
                }
            }
        // status == "pending" -> keep polling
    });
}
