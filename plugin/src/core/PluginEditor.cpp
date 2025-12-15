#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "audio/NotificationSound.h"
#include <cstdlib>  // for std::getenv (DPI detection on Linux)
#include "util/Async.h"
#include "util/Colors.h"
#include "util/Constants.h"
#include "util/Json.h"
#include "stores/ImageCache.h"
#include "util/Log.h"
#include "util/logging/Logger.h"
#include "util/PropertiesFileUtils.h"
#include "util/Result.h"
#include "util/OSNotification.h"
#ifdef NDEBUG
#include "security/SecureTokenStore.h"
#endif
#include "util/error/ErrorTracking.h"

//==============================================================================
SidechainAudioProcessorEditor::SidechainAudioProcessorEditor(SidechainAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setSize(PLUGIN_WIDTH, PLUGIN_HEIGHT);

    // Apply system DPI scaling for HiDPI displays
    applySystemDpiScaling();

    // Initialize view transition manager for smooth view transitions
    viewTransitionManager = ViewTransitionManager::create(this);
    viewTransitionManager->setDefaultDuration(300);  // < 350ms requirement

    // Initialize centralized user data store
    userDataStore = std::make_unique<UserDataStore>();
    userDataStore->addChangeListener(this);

    // ChatStore is a singleton - no need to create it here
    // It will be accessed via ChatStore::getInstance()

    // Initialize draft storage
    draftStorage = std::make_unique<DraftStorage>();

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

    // Add ToastManager to component hierarchy (for transient error notifications)
    auto& toastManager = ToastManager::getInstance();
    addAndMakeVisible(&toastManager);

    // Set up critical error alerts (Task 4.19)
    using namespace Sidechain::Util::Error;
    auto errorTracker = ErrorTracker::getInstance();
    errorTracker->setOnCriticalError([](const ErrorInfo& error) {
        // Show critical error as toast notification on main thread
        juce::MessageManager::callAsync([error]() {
            auto& toastMgr = ToastManager::getInstance();
            toastMgr.showToast(
                "Critical Error: " + error.message,
                ToastNotification::ToastType::Error,
                5000  // Show for 5 seconds
            );

            // Also log to system log
            Log::error("CRITICAL ERROR: " + error.message +
                      " (Source: " + ErrorInfo::sourceToString(error.source) + ")");
        });
    });

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
    postsFeedComponent->onAuthenticationRequired = [this]() {
        Log::warn("PluginEditor: Authentication required - redirecting to auth screen");
        // Clear stored credentials and redirect to auth
        if (userDataStore)
        {
            userDataStore->clearAll();
        }
        if (networkClient)
        {
            networkClient->setAuthToken("");
        }
        showView(AppView::Authentication);
    };
    postsFeedComponent->onStartRecording = [this]() { showView(AppView::Recording); };
    postsFeedComponent->onGoToDiscovery = [this]() { showView(AppView::Discovery); };
    postsFeedComponent->onSendPostToMessage = [this](const FeedPost& post) {
        showSharePostToMessage(post);
    };
    postsFeedComponent->onSoundClicked = [this](const juce::String& soundId) {
        showSoundPage(soundId);
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
    recordingComponent->onViewDrafts = [this]() { showDrafts(); };
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
    uploadComponent->onSaveAsDraft = [this]() {
        saveCurrentUploadAsDraft();
    };
    addChildComponent(uploadComponent.get());

    //==========================================================================
    // Create DraftsView
    draftsViewComponent = std::make_unique<DraftsView>();
    draftsViewComponent->setDraftStorage(draftStorage.get());
    draftsViewComponent->onClose = [this]() {
        navigateBack();
    };
    draftsViewComponent->onNewRecording = [this]() {
        showView(AppView::Recording);
    };
    draftsViewComponent->onDraftSelected = [this](const Draft& draft) {
        // Load draft into Upload component and navigate there
        if (uploadComponent && draftStorage)
        {
            juce::AudioBuffer<float> audioBuffer;
            Draft loadedDraft = draftStorage->loadDraft(draft.id, audioBuffer);
            if (audioBuffer.getNumSamples() > 0)
            {
                uploadComponent->setAudioToUpload(audioBuffer, loadedDraft.sampleRate, loadedDraft.midiData);
                uploadComponent->loadFromDraft(loadedDraft.filename, loadedDraft.bpm, loadedDraft.keyIndex, loadedDraft.genreIndex, loadedDraft.commentAudienceIndex);
                showView(AppView::Upload);
            }
        }
    };
    addChildComponent(draftsViewComponent.get());

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
    storyViewerComponent->onDeleteClicked = [this]([[maybe_unused]] const juce::String& storyId) {
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
    // Create SoundPage component (Feature #15 - Sound/Sample Pages)
    soundPageComponent = std::make_unique<SoundPage>();
    soundPageComponent->setNetworkClient(networkClient.get());
    soundPageComponent->onBackPressed = [this]() {
        navigateBack();
    };
    soundPageComponent->onPostSelected = [](const juce::String& postId) {
        // TODO: Navigate to post detail view when implemented
        Log::info("Post selected from sound page: " + postId);
    };
    soundPageComponent->onUserSelected = [this](const juce::String& userId) {
        showProfile(userId);
    };
    addChildComponent(soundPageComponent.get());

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
    // Create ArchivedPosts component (Post Archive)
    archivedPostsComponent = std::make_unique<ArchivedPosts>();
    archivedPostsComponent->setNetworkClient(networkClient.get());
    if (userDataStore)
        archivedPostsComponent->setCurrentUserId(userDataStore->getUserId());
    archivedPostsComponent->onBackPressed = [this]() {
        navigateBack();
    };
    archivedPostsComponent->onPostClicked = [this](const FeedPost& post) {
        // Navigate to user profile when post is clicked
        showProfile(post.userId);
    };
    archivedPostsComponent->onPlayClicked = [this](const FeedPost& post) {
        if (!post.audioUrl.isEmpty())
        {
            audioProcessor.getAudioPlayer().loadAndPlay(post.audioUrl, post.id);
            archivedPostsComponent->setCurrentlyPlayingPost(post.id);
        }
    };
    archivedPostsComponent->onPauseClicked = [this](const FeedPost& /*post*/) {
        audioProcessor.getAudioPlayer().stop();
        archivedPostsComponent->clearPlayingState();
    };
    archivedPostsComponent->onUserClicked = [this](const juce::String& userId) {
        showProfile(userId);
    };
    addChildComponent(archivedPostsComponent.get());

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
    selectHighlightDialog->onHighlightSelected = [](const juce::String& highlightId) {
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
    shareToMessageDialog->onShared = [](int conversationCount) {
        Log::info("PluginEditor: Content shared to " + juce::String(conversationCount) + " conversation(s)");
        // Optionally show success message
    };
    shareToMessageDialog->onClosed = []() {
        Log::debug("PluginEditor: Share to DM closed");
    };
    // Not added as child - shown as modal overlay when needed

    //==========================================================================
    // Create UserPickerDialog for creating new conversations
    userPickerDialog = std::make_unique<UserPickerDialog>();
    userPickerDialog->setNetworkClient(networkClient.get());
    userPickerDialog->setStreamChatClient(streamChatClient.get());
    if (userDataStore)
        userPickerDialog->setCurrentUserId(userDataStore->getUserId());

    userPickerDialog->onUserSelected = [this](const juce::String& userId) {
        // Hide dialog immediately
        if (userPickerDialog)
            userPickerDialog->setVisible(false);

        // Create direct channel with selected user
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
                        Log::error("PluginEditor: Failed to create direct channel - " + result.getError());
                        juce::AlertWindow::showMessageBoxAsync(
                            juce::MessageBoxIconType::WarningIcon,
                            "Error",
                            "Failed to create conversation: " + result.getError());
                    }
                });
            });
        }
    };

    userPickerDialog->onGroupCreated = [this](const std::vector<juce::String>& userIds, const juce::String& groupName) {
        // Hide dialog immediately
        if (userPickerDialog)
            userPickerDialog->setVisible(false);

        // Create group channel with selected users
        if (streamChatClient && streamChatClient->isAuthenticated())
        {
            // Generate unique channel ID
            juce::String channelId = "group_" + juce::String(juce::Time::currentTimeMillis());

            streamChatClient->createGroupChannel(channelId, groupName, userIds, [this](Outcome<StreamChatClient::Channel> result) {
                juce::MessageManager::callAsync([this, result]() {
                    if (result.isOk())
                    {
                        auto channel = result.getValue();
                        showMessageThread(channel.type, channel.id);
                    }
                    else
                    {
                        Log::error("PluginEditor: Failed to create group channel - " + result.getError());
                        juce::AlertWindow::showMessageBoxAsync(
                            juce::MessageBoxIconType::WarningIcon,
                            "Error",
                            "Failed to create group: " + result.getError());
                    }
                });
            });
        }
    };

    userPickerDialog->onCancelled = [this]() {
        Log::debug("PluginEditor: User picker cancelled");
        if (userPickerDialog)
            userPickerDialog->setVisible(false);
    };
    // Not added as child - shown as modal overlay when needed

    //==========================================================================
    // Create NotificationSettings dialog
    notificationSettingsDialog = std::make_unique<NotificationSettings>();
    notificationSettingsDialog->setNetworkClient(networkClient.get());
    if (userDataStore)
        notificationSettingsDialog->setUserDataStore(userDataStore.get());
    notificationSettingsDialog->onClose = []() {
        // Dialog handles its own cleanup - callback is just a notification
        Log::debug("NotificationSettings dialog closed");
    };
    // Not added as child - shown as modal overlay when needed

    //==========================================================================
    // Create TwoFactorSettings dialog
    twoFactorSettingsDialog = std::make_unique<TwoFactorSettings>();
    twoFactorSettingsDialog->setNetworkClient(networkClient.get());
    twoFactorSettingsDialog->onClose = []() {
        // Dialog handles its own cleanup - callback is just a notification
        // DO NOT call closeDialog() here - it causes recursive crash!
        Log::debug("TwoFactorSettings dialog closed");
    };
    // Not added as child - shown as modal overlay when needed

    //==========================================================================
    // Create ActivityStatusSettings dialog
    activityStatusDialog = std::make_unique<ActivityStatusSettings>();
    activityStatusDialog->setNetworkClient(networkClient.get());
    activityStatusDialog->onClose = []() {
        Log::debug("ActivityStatusSettings dialog closed");
    };
    // Not added as child - shown as modal overlay when needed

    //==========================================================================
    // Create EditProfile dialog (Settings page) (Task 2.4: Use UserStore)
    editProfileDialog = std::make_unique<EditProfile>();
    editProfileDialog->setNetworkClient(networkClient.get());
    editProfileDialog->setUserStore(&Sidechain::Stores::UserStore::getInstance());
    // Task 2.4: Profile save is now handled via UserStore subscription in EditProfile
    // Callbacks removed: onCancel, onSave, onProfilePicSelected
    editProfileDialog->onActivityStatusClicked = [this]() {
        showActivityStatusSettings();
    };
    editProfileDialog->onMutedUsersClicked = []() {
        // TODO: Implement MutedUsers component
        Log::info("EditProfile: Muted users clicked - not yet implemented");
    };
    editProfileDialog->onTwoFactorClicked = [this]() {
        showTwoFactorSettings();
    };
    editProfileDialog->onProfileSetupClicked = [this]() {
        editProfileDialog->closeDialog();
        showView(AppView::ProfileSetup);
    };
    editProfileDialog->onLogoutClicked = [this]() {
        editProfileDialog->closeDialog();
        handleLogout();
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

    // Set StreamChatClient on ChatStore so it can send/receive messages
    Sidechain::Stores::ChatStore::getInstance().setStreamChatClient(streamChatClient.get());

    // Wire up message notification callback to check OS notification setting
    streamChatClient->setMessageNotificationCallback([this](const juce::String& title, const juce::String& message) {
        // Check if OS notifications are enabled before showing
        if (userDataStore && userDataStore->isOSNotificationsEnabled())
        {
            OSNotification::show(title, message, "", userDataStore->isNotificationSoundEnabled());
        }
    });

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
        // Show user picker dialog to create new conversation
        if (userPickerDialog)
        {
            userPickerDialog->setNetworkClient(networkClient.get());
            userPickerDialog->setStreamChatClient(streamChatClient.get());
            if (userDataStore)
                userPickerDialog->setCurrentUserId(userDataStore->getUserId());

            // Load recent conversations and suggested users
            userPickerDialog->loadRecentConversations();
            userPickerDialog->loadSuggestedUsers();

            userPickerDialog->showModal(this);
            Log::info("MessagesList: onNewMessage - showing UserPickerDialog");
        }
    };
    messagesListComponent->onGoToDiscovery = [this]() {
        showView(AppView::Discovery);
    };
    messagesListComponent->onCreateGroup = [this]() {
        // Show user picker dialog to create new group
        if (userPickerDialog)
        {
            userPickerDialog->setNetworkClient(networkClient.get());
            userPickerDialog->setStreamChatClient(streamChatClient.get());
            if (userDataStore)
                userPickerDialog->setCurrentUserId(userDataStore->getUserId());

            // Load recent conversations and suggested users
            userPickerDialog->loadRecentConversations();
            userPickerDialog->loadSuggestedUsers();

            userPickerDialog->showModal(this);
            Log::info("PluginEditor: Create Group clicked - showing UserPickerDialog");
        }
    };
    addChildComponent(messagesListComponent.get());

    //==========================================================================
    // Create MessageThread
    messageThreadComponent = std::make_unique<MessageThread>();
    messageThreadComponent->setStreamChatClient(streamChatClient.get());
    messageThreadComponent->setNetworkClient(networkClient.get());
    messageThreadComponent->setAudioProcessor(&audioProcessor);
    messageThreadComponent->setChatStore(&Sidechain::Stores::ChatStore::getInstance());
    messageThreadComponent->onBackPressed = [this]() {
        showView(AppView::Messages);
    };
    messageThreadComponent->onSharedPostClicked = [this](const juce::String& postId) {
        // Navigate to feed and show the post (would need to add scrollToPost functionality)
        Log::info("MessageThread: Shared post clicked - " + postId);
        showView(AppView::PostsFeed);
        // TODO: Implement scrollToPost(postId) in PostsFeed to jump to specific post
    };
    messageThreadComponent->onSharedStoryClicked = [this](const juce::String& storyId) {
        // Extract user ID from story ID (format: userId_timestamp)
        juce::String userId = storyId.upToFirstOccurrenceOf("_", false, false);
        if (userId.isNotEmpty())
            showUserStory(userId);
    };
    addChildComponent(messageThreadComponent.get());

    //==========================================================================
    // Create Profile
    profileComponent = std::make_unique<Profile>();
    profileComponent->setNetworkClient(networkClient.get());
    profileComponent->setFeedStore(&Sidechain::Stores::FeedStore::getInstance());  // Task 2.4: Set FeedStore for follow/mute
    profileComponent->setUserStore(&Sidechain::Stores::UserStore::getInstance());  // Task 2.4.2: Set UserStore for own profile
    // Note: StreamChatClient will be set after it's created (below)
    profileComponent->onBackPressed = [this]() {
        navigateBack();
    };
    profileComponent->onEditProfile = [this]() {
        // Show the Settings/Edit Profile dialog
        showEditProfile();
    };
    profileComponent->onSavedPostsClicked = [this]() {
        // Navigate to saved posts view
        showSavedPosts();
    };
    profileComponent->onArchivedPostsClicked = [this]() {
        // Navigate to archived posts view
        showArchivedPosts();
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
    profileComponent->onNotificationSettingsClicked = [this]() {
        showNotificationSettings();
    };
    profileComponent->onTwoFactorSettingsClicked = [this]() {
        showTwoFactorSettings();
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

    //==========================================================================
    // Trigger initial layout now that all components are created
    resized();
}

SidechainAudioProcessorEditor::~SidechainAudioProcessorEditor()
{
    // Shutdown async system first to prevent pending callbacks from accessing
    // destroyed objects and to allow detached threads to exit cleanly
    Async::shutdown();

    // Auto-save unsaved upload as draft
    if (uploadComponent && draftStorage)
    {
        const auto& audioBuffer = uploadComponent->getAudioBuffer();
        if (audioBuffer.getNumSamples() > 0)
        {
            // Create auto-recovery draft
            Draft draft;
            draft.id = "_auto_recovery";
            draft.filename = uploadComponent->getFilename();
            draft.bpm = uploadComponent->getBpm();
            draft.keyIndex = uploadComponent->getKeyIndex();
            draft.genreIndex = uploadComponent->getGenreIndex();
            draft.commentAudienceIndex = uploadComponent->getCommentAudienceIndex();
            draft.sampleRate = uploadComponent->getSampleRate();
            draft.midiData = uploadComponent->getMidiData();
            draft.createdAt = juce::Time::getCurrentTime();
            draft.updatedAt = juce::Time::getCurrentTime();

            Log::info("PluginEditor: Auto-saving upload as recovery draft");
            draftStorage->saveAutoRecoveryDraft(draft, audioBuffer);
        }
    }

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

    Log::debug("PluginEditor::resized: Resizing to " + juce::String(getWidth()) + "x" + juce::String(getHeight()));

    // Position central header at top (for post-login views)
    if (headerComponent)
    {
        auto headerBounds = bounds.removeFromTop(headerHeight);
        Log::debug("PluginEditor::resized: Setting header bounds to " + juce::String(headerBounds.getWidth()) + "x" + juce::String(headerBounds.getHeight()));
        headerComponent->setBounds(headerBounds);
    }

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

    if (draftsViewComponent)
        draftsViewComponent->setBounds(contentBounds);

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

    if (soundPageComponent)
        soundPageComponent->setBounds(contentBounds);

    if (midiChallengesComponent)
        midiChallengesComponent->setBounds(contentBounds);

    if (midiChallengeDetailComponent)
        midiChallengeDetailComponent->setBounds(contentBounds);

    if (savedPostsComponent)
        savedPostsComponent->setBounds(contentBounds);

    if (archivedPostsComponent)
        archivedPostsComponent->setBounds(contentBounds);

    // ToastManager covers entire window (transparent, toasts positioned within)
    auto& toastManager = ToastManager::getInstance();
    toastManager.setBounds(getLocalBounds());
}

//==============================================================================
juce::Component* SidechainAudioProcessorEditor::getComponentForView(AppView view)
{
    switch (view)
    {
        case AppView::Authentication:     return authComponent.get();
        case AppView::ProfileSetup:       return profileSetupComponent.get();
        case AppView::PostsFeed:          return postsFeedComponent.get();
        case AppView::Recording:          return recordingComponent.get();
        case AppView::Upload:             return uploadComponent.get();
        case AppView::Drafts:             return draftsViewComponent.get();
        case AppView::StoryRecording:     return storyRecordingComponent.get();
        case AppView::StoryViewer:        return storyViewerComponent.get();
        case AppView::HiddenSynth:        return hiddenSynthComponent.get();
        case AppView::Playlists:          return playlistsComponent.get();
        case AppView::PlaylistDetail:     return playlistDetailComponent.get();
        case AppView::SoundPage:          return soundPageComponent.get();
        case AppView::MidiChallenges:     return midiChallengesComponent.get();
        case AppView::MidiChallengeDetail: return midiChallengeDetailComponent.get();
        case AppView::SavedPosts:         return savedPostsComponent.get();
        case AppView::ArchivedPosts:      return archivedPostsComponent.get();
        case AppView::Discovery:          return userDiscoveryComponent.get();
        case AppView::Profile:            return profileComponent.get();
        case AppView::Search:             return searchComponent.get();
        case AppView::Messages:           return messagesListComponent.get();
        case AppView::MessageThread:      return messageThreadComponent.get();
        default:                          return nullptr;
    }
}

void SidechainAudioProcessorEditor::showView(AppView view, NavigationDirection direction)
{
    Log::info("showView: entering, view=" + juce::String(static_cast<int>(view)) + ", currentView=" + juce::String(static_cast<int>(currentView)));

    // If already on this view, still need to refresh/reload it (don't skip)
    // This handles cases where the user clicks the same view button again
    bool isSameView = (currentView == view);
    if (isSameView)
        Log::info("showView: Already on this view, will refresh it");

    // Get the component to show and hide using helper function
    juce::Component* componentToShow = getComponentForView(view);
    juce::Component* componentToHide = getComponentForView(currentView);

    Log::info("showView: componentToShow=" + juce::String(componentToShow != nullptr ? "valid" : "null") +
              ", componentToHide=" + juce::String(componentToHide != nullptr ? "valid" : "null"));

    // SET UP THE VIEW FIRST (before animation) so content is ready to display
    // This ensures that when the animation starts, the view is already prepared
    if (view == AppView::PostsFeed && postsFeedComponent)
    {
        Log::debug("showView: Setting up PostsFeed BEFORE animation - calling loadFeed()");
        postsFeedComponent->setUserInfo(username, email, profilePicUrl);
        postsFeedComponent->loadFeed();
    }

    // Determine if we should animate the transition
    // Don't animate: auth transitions, same view, missing components, or explicitly no animation
    //
    // TODO (Task 4.21): Fix ViewTransitionManager.slideLeft animation for PostsFeed
    // ISSUE: When navigating TO PostsFeed, the slideLeft animation starts but the completion
    // callback never fires, causing the component to never appear on screen until a second click.
    // This is specific to PostsFeed - other views animate correctly.
    // WORKAROUND: Skip animation when navigating to PostsFeed, use immediate non-animated path.
    // This makes first-click navigation work correctly but loses the animation smoothness.
    // ROOT CAUSE: Unknown - likely an issue in ViewTransitionManager or component lifecycle
    // during the specific Profile->PostsFeed transition.
    bool shouldAnimate = componentToShow && componentToHide
                         && componentToShow != componentToHide
                         && !isSameView  // Don't animate if already on same view
                         && currentView != AppView::Authentication
                         && view != AppView::Authentication
                         && view != AppView::PostsFeed  // TEMP: Skip animation to PostsFeed (TODO 4.21)
                         && direction != NavigationDirection::None;

    if (shouldAnimate)
    {
        Log::info("showView: starting slide animation, direction=" + juce::String(direction == NavigationDirection::Forward ? "Forward" : "Backward"));

        // Use content bounds (below header) for post-login views
        auto bounds = getLocalBounds().withTrimmedTop(Header::HEADER_HEIGHT);

        // Set bounds and visibility for both components involved in transition
        // Must make both visible BEFORE animation starts so they render correctly
        if (componentToShow)
        {
            componentToShow->setBounds(bounds);
            componentToShow->setVisible(true);
        }
        if (componentToHide)
        {
            componentToHide->setBounds(bounds);
            componentToHide->setVisible(true);
        }

        // Hide all other components immediately (not involved in animation)
        for (auto appView : { AppView::Authentication, AppView::ProfileSetup, AppView::PostsFeed, AppView::Recording,
                              AppView::Upload, AppView::Drafts, AppView::Discovery, AppView::Profile, AppView::Search,
                              AppView::Messages, AppView::MessageThread, AppView::StoryRecording, AppView::StoryViewer,
                              AppView::HiddenSynth, AppView::Playlists, AppView::PlaylistDetail, AppView::MidiChallenges,
                              AppView::MidiChallengeDetail, AppView::SavedPosts, AppView::ArchivedPosts })
        {
            auto* comp = getComponentForView(appView);
            if (comp && comp != componentToShow && comp != componentToHide)
                comp->setVisible(false);
        }

        // Use ViewTransitionManager for smooth transitions
        // Forward navigation: slide left (new from right, old to left)
        // Backward navigation: slide right (new from left, old to right)

        // Track timing for performance metrics (< 350ms requirement)
        auto startTime = juce::Time::getMillisecondCounterHiRes();
        auto onTransitionComplete = [this, componentToShow, componentToHide, startTime]() {
            auto elapsed = juce::Time::getMillisecondCounterHiRes() - startTime;
            Log::info("View transition completed in " + juce::String(elapsed, 1) + "ms");

            // After animation: ensure new view is visible, old view is hidden
            if (componentToShow)
                componentToShow->setVisible(true);
            if (componentToHide)
                componentToHide->setVisible(false);

#ifdef NDEBUG
            jassert(elapsed < 350);  // Verify < 350ms requirement (Release builds only)
#else
            // Debug builds: just log if slow, don't crash
            if (elapsed >= 350)
                Log::warn("View transition slow: " + juce::String(elapsed, 1) + "ms (expected < 350ms)");
#endif
        };

        if (direction == NavigationDirection::Forward)
        {
            viewTransitionManager->slideLeft(componentToHide, componentToShow, 300, onTransitionComplete);
        }
        else // Backward
        {
            viewTransitionManager->slideRight(componentToHide, componentToShow, 300, onTransitionComplete);
        }

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
            // Force repaint if on the same view (user clicked it again)
            if (isSameView && view == AppView::PostsFeed)
                postsFeedComponent->repaint();
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
        if (draftsViewComponent)
        {
            draftsViewComponent->setBounds(contentBounds);
            draftsViewComponent->setVisible(view == AppView::Drafts);
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
        if (soundPageComponent)
        {
            soundPageComponent->setBounds(contentBounds);
            soundPageComponent->setVisible(view == AppView::SoundPage);
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
        if (archivedPostsComponent)
        {
            archivedPostsComponent->setBounds(contentBounds);
            archivedPostsComponent->setVisible(view == AppView::ArchivedPosts);
        }
        if (hiddenSynthComponent)
        {
            hiddenSynthComponent->setBounds(contentBounds);
            hiddenSynthComponent->setVisible(view == AppView::HiddenSynth);
        }
    }

    // Push current view to navigation stack (except when going back or during auth)
    // When navigating backward, we've already popped from stack, so don't push
    if (currentView != view && currentView != AppView::Authentication && direction != NavigationDirection::Backward)
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

    // Set up components BEFORE animation (so they're ready when animation renders)
    // This runs regardless of whether we animate or not
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
            // PostsFeed is set up BEFORE animation (see above) to ensure content is ready
            break;

        case AppView::Recording:
            
            break;

        case AppView::Upload:

            break;

        case AppView::Drafts:

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

        case AppView::SoundPage:
            if (soundPageComponent && soundIdToView.isNotEmpty())
            {
                soundPageComponent->loadSound(soundIdToView);
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

        case AppView::ArchivedPosts:
            if (archivedPostsComponent)
            {
                if (userDataStore)
                    archivedPostsComponent->setCurrentUserId(userDataStore->getUserId());
                archivedPostsComponent->loadArchivedPosts();
            }
            break;

        case AppView::Discovery:
            Log::info("showView: initializing Discovery component");
            if (userDiscoveryComponent)
            {
                Log::info("showView: calling setCurrentUserId");
                // Get user ID from UserDataStore instead of deprecated authToken
                juce::String currentUserId = userDataStore ? userDataStore->getUserId() : "";
                userDiscoveryComponent->setCurrentUserId(currentUserId);
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

        case AppView::StoryViewer:
            // StoryViewer is set up separately via showUserStory() or showHighlightStories()
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

void SidechainAudioProcessorEditor::showSoundPage(const juce::String& soundId)
{
    soundIdToView = soundId;
    showView(AppView::SoundPage);
}

void SidechainAudioProcessorEditor::showSavedPosts()
{
    showView(AppView::SavedPosts);
}

void SidechainAudioProcessorEditor::showArchivedPosts()
{
    showView(AppView::ArchivedPosts);
}

void SidechainAudioProcessorEditor::showDrafts()
{
    if (draftsViewComponent)
        draftsViewComponent->refresh();
    showView(AppView::Drafts);
}

void SidechainAudioProcessorEditor::saveCurrentUploadAsDraft()
{
    if (!uploadComponent || !draftStorage)
        return;

    // Create draft from current upload state
    Draft draft;
    draft.id = juce::Uuid().toString();
    draft.filename = uploadComponent->getFilename();
    draft.bpm = uploadComponent->getBpm();
    draft.keyIndex = uploadComponent->getKeyIndex();
    draft.genreIndex = uploadComponent->getGenreIndex();
    draft.commentAudienceIndex = uploadComponent->getCommentAudienceIndex();
    draft.sampleRate = uploadComponent->getSampleRate();
    draft.midiData = uploadComponent->getMidiData();
    draft.createdAt = juce::Time::getCurrentTime();
    draft.updatedAt = juce::Time::getCurrentTime();

    const auto& audioBuffer = uploadComponent->getAudioBuffer();
    if (audioBuffer.getNumSamples() > 0)
    {
        draftStorage->saveDraft(draft, audioBuffer);
        uploadComponent->reset();
        showDrafts();
    }
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

//==============================================================================
// DPI Scaling

void SidechainAudioProcessorEditor::applySystemDpiScaling()
{
    // IMPORTANT: Only apply manual DPI scaling on Linux.
    // On macOS and Windows, JUCE handles DPI/Retina scaling automatically.
    // Manually calling setScaleFactor() on these platforms causes double-scaling.
#if JUCE_LINUX
    double systemScale = 1.0;
    juce::String scaleSource = "default";

    // First, try JUCE's display API
    if (auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
    {
        if (display->scale > 1.0)
        {
            systemScale = display->scale;
            scaleSource = "JUCE Display API";
        }
    }

    // On Linux, JUCE often doesn't detect the scale correctly.
    // Check common environment variables as fallback.
    if (systemScale <= 1.0)
    {
        // Check GDK_SCALE (GTK apps, GNOME)
        if (const char* gdkScale = std::getenv("GDK_SCALE"))
        {
            double scale = juce::String(gdkScale).getDoubleValue();
            if (scale > 1.0)
            {
                systemScale = scale;
                scaleSource = "GDK_SCALE";
            }
        }
    }

    if (systemScale <= 1.0)
    {
        // Check QT_SCALE_FACTOR (Qt apps, KDE)
        if (const char* qtScale = std::getenv("QT_SCALE_FACTOR"))
        {
            double scale = juce::String(qtScale).getDoubleValue();
            if (scale > 1.0)
            {
                systemScale = scale;
                scaleSource = "QT_SCALE_FACTOR";
            }
        }
    }

    if (systemScale <= 1.0)
    {
        // Check QT_AUTO_SCREEN_SCALE_FACTOR for fractional scaling
        if (const char* qtAutoScale = std::getenv("QT_AUTO_SCREEN_SCALE_FACTOR"))
        {
            if (juce::String(qtAutoScale) == "1")
            {
                // Qt auto-scaling is enabled - try to detect from DPI
                if (auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
                {
                    // Standard DPI is 96 on Linux
                    double dpiScale = display->dpi / 96.0;
                    if (dpiScale > 1.0)
                    {
                        systemScale = dpiScale;
                        scaleSource = "DPI-based (Qt auto-scale)";
                    }
                }
            }
        }
    }

    if (systemScale <= 1.0)
    {
        // Check PLASMA_SCALE_FACTOR (KDE Plasma specific)
        if (const char* plasmaScale = std::getenv("PLASMA_SCALE_FACTOR"))
        {
            double scale = juce::String(plasmaScale).getDoubleValue();
            if (scale > 1.0)
            {
                systemScale = scale;
                scaleSource = "PLASMA_SCALE_FACTOR";
            }
        }
    }

    if (systemScale <= 1.0)
    {
        // Try DPI-based detection as final fallback
        if (auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        {
            // Standard DPI is 96 on Linux, 72 on macOS
            double standardDpi = 96.0;
            double dpiScale = display->dpi / standardDpi;
            Log::debug("DPI detection: display->dpi = " + juce::String(display->dpi, 1) +
                      ", calculated scale = " + juce::String(dpiScale, 2));
            if (dpiScale > 1.1)  // Use 1.1 threshold to avoid false positives
            {
                systemScale = dpiScale;
                scaleSource = "DPI-based";
            }
        }
    }

    // Apply the scale factor if above 1.0
    if (systemScale > 1.0)
    {
        setScaleFactor(static_cast<float>(systemScale));
        Log::info("Applied DPI scale factor: " + juce::String(systemScale, 2) + " (source: " + scaleSource + ")");
    }
    else
    {
        Log::debug("Standard DPI display detected (scale = 1.00)");
    }
#else
    // macOS and Windows: JUCE handles DPI/Retina scaling automatically
    Log::debug("Using JUCE automatic DPI handling (macOS/Windows)");
#endif
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
    shareToMessageDialog->setPost(post);

    // Show the dialog
    shareToMessageDialog->showModal(this);

    Log::info("PluginEditor: Showing share post to message dialog");
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

    // Convert StoryData to Story
    Story storyModel;
    storyModel.id = story.id;
    storyModel.userId = story.userId;
    storyModel.username = story.username;
    storyModel.userAvatarUrl = story.userAvatarUrl;
    storyModel.audioUrl = story.audioUrl;
    storyModel.filename = story.filename;
    storyModel.midiFilename = story.midiFilename;
    storyModel.audioDuration = story.audioDuration;
    storyModel.midiData = story.midiData;
    storyModel.midiPatternId = story.midiPatternId;
    storyModel.viewCount = story.viewCount;
    storyModel.viewed = story.viewed;
    storyModel.expiresAt = story.expiresAt;
    storyModel.createdAt = story.createdAt;

    // Set the story to share
    shareToMessageDialog->setStoryToShare(storyModel);

    // Show the dialog
    shareToMessageDialog->showModal(this);

    Log::info("PluginEditor: Showing share story to message dialog");
}

void SidechainAudioProcessorEditor::showNotificationSettings()
{
    if (!notificationSettingsDialog)
        return;

    // Ensure UserDataStore is set (in case it wasn't set during construction)
    if (userDataStore)
        notificationSettingsDialog->setUserDataStore(userDataStore.get());

    // Show the dialog
    notificationSettingsDialog->showModal(this);
}

void SidechainAudioProcessorEditor::showTwoFactorSettings()
{
    if (!twoFactorSettingsDialog)
        return;

    // Load current 2FA status and show the dialog
    twoFactorSettingsDialog->loadStatus();
    twoFactorSettingsDialog->showModal(this);
}

void SidechainAudioProcessorEditor::showActivityStatusSettings()
{
    if (!activityStatusDialog)
        return;

    // Show the dialog (loadSettings is called by showModal)
    activityStatusDialog->showModal(this);
}

void SidechainAudioProcessorEditor::showEditProfile()
{
    if (!editProfileDialog)
        return;

    // Task 2.4: Use reactive pattern - showWithCurrentProfile reads from UserStore
    editProfileDialog->showWithCurrentProfile(this);
}

void SidechainAudioProcessorEditor::handleLogout()
{
    // Show confirmation dialog before logging out
    confirmAndLogout();
}

void SidechainAudioProcessorEditor::navigateBack()
{
    if (navigationStack.isEmpty())
    {
        // Default to feed if no history (with no animation since there's no "from" view)
        showView(AppView::PostsFeed, NavigationDirection::None);
        return;
    }

    // Pop last view from stack
    AppView previousView = navigationStack.getLast();
    navigationStack.removeLast();

    // Use showView with Backward direction for smooth slide-from-left animation
    // showView will handle all visibility, positioning, and setup
    showView(previousView, NavigationDirection::Backward);
}

void SidechainAudioProcessorEditor::onLoginSuccess(const juce::String& user, const juce::String& mail, const juce::String& token)
{
    using namespace Sidechain::Util;
    using namespace Sidechain::Security;

    // Log authentication success
    Sidechain::Util::Logger::getInstance().log(
        LogLevel::Info,
        "Security",
        "User authentication successful: " + user
    );

    // Store token securely using platform-specific secure storage (Release builds only)
#ifdef NDEBUG
    auto* secureStore = SecureTokenStore::getInstance();
    if (secureStore && secureStore->isAvailable())
    {
        if (secureStore->saveToken("auth_token", token))
        {
            Sidechain::Util::Logger::getInstance().log(
                LogLevel::Info,
                "Security",
                "Auth token stored securely in " + secureStore->getBackendType()
            );
        }
        else
        {
            Sidechain::Util::Logger::getInstance().log(
                LogLevel::Error,
                "Security",
                "Failed to save auth token to secure storage"
            );
        }
    }
    else
    {
        Sidechain::Util::Logger::getInstance().log(
            LogLevel::Warning,
            "Security",
            "Secure storage not available, token not persisted"
        );
    }
#else
    // Debug build - store token insecurely in local settings (no Keychain) for persistence
    if (userDataStore)
    {
        userDataStore->setAuthToken(token);
        userDataStore->saveToSettings();
        Sidechain::Util::Logger::getInstance().log(
            LogLevel::Info,
            "Security",
            "Debug build - token stored insecurely in local settings (not using Keychain)"
        );
    }
    else
    {
        Sidechain::Util::Logger::getInstance().log(
            LogLevel::Warning,
            "Security",
            "Debug build - UserDataStore unavailable, token not persisted"
        );
    }
#endif

    // Update legacy state (for backwards compatibility during migration)
    username = user;
    email = mail;
    authToken = "";  // Deprecated - token now stored securely, not in plain text

    // Update centralized UserDataStore
    if (userDataStore)
    {
        userDataStore->setAuthToken(token);  // TODO: Update UserDataStore to use SecureTokenStore
        userDataStore->setBasicUserInfo(user, mail);
    }

    // Set auth token on network client (NetworkClient needs token in memory for API requests)
    if (networkClient && !token.isEmpty())
        networkClient->setAuthToken(token);

    // Fetch getstream.io chat token for messaging
    if (streamChatClient && !token.isEmpty())
    {
        streamChatClient->fetchToken(token, [](::Outcome<StreamChatClient::TokenResult> result) {
            if (result.isOk())
            {
                auto tokenResult = result.getValue();
                Log::info("Stream chat token fetched successfully for user: " + tokenResult.userId);

                // Also set authentication on ChatStore so it can send messages
                Sidechain::Stores::ChatStore::getInstance().setAuthentication(
                    tokenResult.token,
                    tokenResult.apiKey,
                    tokenResult.userId
                );
                Log::debug("ChatStore authenticated with stream token");
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

    // Clear auth token from secure storage (Release builds only)
#ifdef NDEBUG
    auto* secureStore = Sidechain::Security::SecureTokenStore::getInstance();
    if (secureStore && secureStore->isAvailable())
    {
        if (secureStore->deleteToken("auth_token"))
        {
            Sidechain::Util::Logger::getInstance().log(
                Sidechain::Util::LogLevel::Info,
                "Security",
                "Auth token cleared from secure storage"
            );
        }
    }
#else
    // Debug build - no secure storage to clear
    Sidechain::Util::Logger::getInstance().log(
        Sidechain::Util::LogLevel::Info,
        "Security",
        "Debug build - no Keychain token to clear"
    );
#endif

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
    // DEPRECATED: This method is no longer used as token storage is handled by SecureTokenStore
    // and other user data is persisted by UserDataStore.
    // Keeping method for backwards compatibility, but it no longer saves the auth token.

    auto appProperties = std::make_unique<juce::PropertiesFile>(
        Sidechain::Util::PropertiesFileUtils::getStandardOptions());

    if (!username.isEmpty())
    {
        appProperties->setValue("isLoggedIn", true);
        appProperties->setValue("username", username);
        appProperties->setValue("email", email);
        appProperties->setValue("profilePicUrl", profilePicUrl);
        // authToken is NO LONGER saved here - it's stored securely via SecureTokenStore
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
            // Load auth token from secure storage (Release) or local settings (Debug)
            juce::String loadedToken;
#ifdef NDEBUG
            auto* secureStore = Sidechain::Security::SecureTokenStore::getInstance();

            if (secureStore && secureStore->isAvailable())
            {
                if (auto tokenOpt = secureStore->loadToken("auth_token"))
                {
                    loadedToken = tokenOpt.value();
                    Sidechain::Util::Logger::getInstance().log(
                        Sidechain::Util::LogLevel::Info,
                        "Security",
                        "Auth token loaded from " + secureStore->getBackendType()
                    );
                }
                else
                {
                    Sidechain::Util::Logger::getInstance().log(
                        Sidechain::Util::LogLevel::Warning,
                        "Security",
                        "No auth token found in secure storage"
                    );
                }
            }
#else
            // Debug build - load token from local settings (insecure but persistent)
            loadedToken = userDataStore->getAuthToken();
            if (loadedToken.isNotEmpty())
            {
                Sidechain::Util::Logger::getInstance().log(
                    Sidechain::Util::LogLevel::Info,
                    "Security",
                    "Debug build - token loaded from local settings (insecure storage)"
                );
            }
            else
            {
                Sidechain::Util::Logger::getInstance().log(
                    Sidechain::Util::LogLevel::Warning,
                    "Security",
                    "Debug build - no auth token found in local settings"
                );
            }
#endif

            // Sync legacy state from UserDataStore
            authToken = "";  // Deprecated - using SecureTokenStore
            username = userDataStore->getUsername();
            email = userDataStore->getEmail();
            profilePicUrl = userDataStore->getProfilePictureUrl();

            Log::debug("loadLoginState: authToken loaded from secure storage, length=" + juce::String(loadedToken.length()));

            // Set auth token on network client
            if (!loadedToken.isEmpty() && networkClient)
                networkClient->setAuthToken(loadedToken);

            // Fetch getstream.io chat token for messaging
            if (streamChatClient && !loadedToken.isEmpty())
            {
                streamChatClient->fetchToken(loadedToken, [](Outcome<StreamChatClient::TokenResult> result) {
                    if (result.isOk())
                    {
                        auto tokenResult = result.getValue();
                        Log::info("Stream chat token fetched successfully for user: " + tokenResult.userId);

                        // Also set authentication on ChatStore so it can send messages
                        Sidechain::Stores::ChatStore::getInstance().setAuthentication(
                            tokenResult.token,
                            tokenResult.apiKey,
                            tokenResult.userId
                        );
                        Log::debug("ChatStore authenticated with stream token (from saved state)");
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
                Log::info("fetchUserProfile callback: success=" + juce::String(success ? "true" : "false"));
                juce::MessageManager::callAsync([this, success]() {
                    // If profile fetch failed (e.g., auth token expired/invalid), redirect to auth
                    if (!success)
                    {
                        Log::warn("Profile fetch failed - auth token may be invalid. Redirecting to auth screen.");
                        // Clear stored login state
                        if (userDataStore)
                        {
                            userDataStore->clearAll();
                        }
                        if (networkClient)
                        {
                            networkClient->setAuthToken("");
                        }
                        showView(AppView::Authentication);
                        return;
                    }

                    // Sync profile URL from UserDataStore
                    if (userDataStore)
                    {
                        profilePicUrl = userDataStore->getProfilePictureUrl();
                        Log::info("After fetchUserProfile: profilePicUrl=" + profilePicUrl +
                            ", hasImage=" + juce::String(userDataStore->hasProfileImage() ? "true" : "false"));

                        // Force update header with latest profile image
                        if (headerComponent)
                        {
                            headerComponent->setUserInfo(userDataStore->getUsername(), profilePicUrl);
                            if (userDataStore->hasProfileImage())
                            {
                                Log::info("Updating header with fetched profile image");
                                headerComponent->setProfileImage(userDataStore->getProfileImage());
                            }
                            else
                            {
                                Log::warn("No profile image available after fetch");
                            }
                        }
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
    auto appProperties = std::make_unique<juce::PropertiesFile>(
        Sidechain::Util::PropertiesFileUtils::getStandardOptions());

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
    auto appProperties = std::make_unique<juce::PropertiesFile>(
        Sidechain::Util::PropertiesFileUtils::getStandardOptions());

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

    // Load auth token from secure storage (Release builds only)
    juce::String token;

#ifdef NDEBUG
    auto* secureStore = Sidechain::Security::SecureTokenStore::getInstance();

    if (secureStore && secureStore->isAvailable())
    {
        if (auto tokenOpt = secureStore->loadToken("auth_token"))
        {
            token = tokenOpt.value();
        }
    }
#else
    // Debug build - try to get token from network client instead of Keychain
    if (networkClient)
    {
        token = networkClient->getAuthToken();
        Log::debug("Debug build - using in-memory token for WebSocket");
    }
#endif

    if (token.isEmpty())
    {
        Log::warn("Cannot connect WebSocket: no auth token available");
        return;
    }

    webSocketClient->setAuthToken(token);
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

        case WebSocketClient::MessageType::Unknown:
        case WebSocketClient::MessageType::Comment:
            Log::warn("Unhandled WebSocket message type: " + message.typeString);
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
        if (item.group.verb == "follow" && item.actorId.isNotEmpty())
        {
            // Navigate to the follower's profile
            showProfile(item.actorId);
        }
        else if ((item.group.verb == "like" || item.group.verb == "comment" || item.group.verb == "mention") && item.targetId.isNotEmpty())
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
            // Check if OS notifications are enabled
            if (userDataStore && userDataStore->isOSNotificationsEnabled())
            {
                // Get the first (most recent) notification to show
                const auto& latestNotification = items.getFirst();
                juce::String notificationTitle = "Sidechain";
                juce::String notificationMessage = latestNotification.getDisplayText();
                
                // Show desktop notification (checks isSupported internally)
                OSNotification::show(notificationTitle, notificationMessage, "", 
                                   userDataStore->isNotificationSoundEnabled());
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

    // Fetch regular notification counts
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
        if (newNotifications && userDataStore && userDataStore->isOSNotificationsEnabled())
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

    // Fetch pending follow request count (for private account feature)
    networkClient->getFollowRequestCount([this](int count) {
        if (notificationBell)
        {
            notificationBell->setFollowRequestCount(count);
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
        notificationPollTimer.reset();
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
