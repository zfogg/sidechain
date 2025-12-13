#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "stores/UserDataStore.h"
#include "network/NetworkClient.h"
#include "network/WebSocketClient.h"
#include "ui/auth/Auth.h"
#include "ui/challenges/MidiChallengeDetail.h"
#include "ui/challenges/MidiChallenges.h"
#include "ui/profile/ProfileSetup.h"
#include "ui/profile/Profile.h"
#include "ui/profile/SavedPosts.h"
#include "ui/profile/ArchivedPosts.h"
#include "ui/profile/NotificationSettings.h"
#include "ui/feed/PostsFeed.h"
#include "ui/recording/Recording.h"
#include "ui/recording/Upload.h"
#include "ui/recording/DraftsView.h"
#include "stores/DraftStorage.h"
#include "ui/common/ConnectionIndicator.h"
#include "ui/common/Header.h"
#include "ui/common/ToastNotification.h"
#include "ui/notifications/NotificationBell.h"
#include "ui/notifications/NotificationList.h"
#include "ui/social/UserDiscovery.h"
#include "ui/search/Search.h"
#include "ui/messages/MessagesList.h"
#include "ui/messages/MessageThread.h"
#include "ui/stories/StoryRecording.h"
#include "ui/stories/StoryViewer.h"
#include "ui/stories/CreateHighlightDialog.h"
#include "ui/stories/SelectHighlightDialog.h"
#include "ui/messages/ShareToMessageDialog.h"
#include "ui/synth/HiddenSynth.h"
#include "ui/playlists/Playlists.h"
#include "ui/playlists/PlaylistDetail.h"
#include "network/StreamChatClient.h"

//==============================================================================
/**
 * Sidechain Audio Plugin Editor
 *
 * Main plugin window that manages views:
 * - Authentication (login/signup)
 * - Profile setup
 * - Posts feed
 * - Recording
 * - Upload
 */
class SidechainAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      public juce::ChangeListener
{
public:
    SidechainAudioProcessorEditor(SidechainAudioProcessor&);
    ~SidechainAudioProcessorEditor() override;

    //==============================================================================
    /** Paint the editor component
     * @param g Graphics context for drawing
     */
    void paint(juce::Graphics&) override;

    /** Handle component resize
     * Updates layout of all child components
     */
    void resized() override;

    //==============================================================================
    // ChangeListener - for UserDataStore updates

    /** Handle change notifications from UserDataStore
     * @param source The ChangeBroadcaster that triggered the change
     */
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

private:
    SidechainAudioProcessor& audioProcessor;

    //==============================================================================
    // View management
    enum class AppView { Authentication, ProfileSetup, PostsFeed, Recording, Upload, Drafts, Discovery, Profile, Search, Messages, MessageThread, StoryRecording, StoryViewer, HiddenSynth, Playlists, PlaylistDetail, MidiChallenges, MidiChallengeDetail, SavedPosts, ArchivedPosts };
    AppView currentView = AppView::Authentication;

    // Navigation stack for back button support
    juce::Array<AppView> navigationStack;
    juce::String profileUserIdToView;  // User ID for Profile view
    juce::String messageChannelType;   // Channel type for MessageThread view
    juce::String messageChannelId;     // Channel ID for MessageThread view
    juce::String playlistIdToView;    // Playlist ID for PlaylistDetail view
    juce::String challengeIdToView;   // Challenge ID for MidiChallengeDetail view

    // Component animator for smooth view transitions
    juce::ComponentAnimator viewAnimator;

    /** Show a specific view in the editor
     * @param view The view to display
     */
    void showView(AppView view);

    /** Show a message thread view
     * @param channelType The type of channel (e.g., "messaging", "team")
     * @param channelId The unique channel identifier
     */
    void showMessageThread(const juce::String& channelType, const juce::String& channelId);

    /** Navigate to a user's profile view
     * @param userId The user ID to display
     */
    void showProfile(const juce::String& userId);

    /** Navigate to playlists view
     */
    void showPlaylists();

    /** Navigate to playlist detail view
     * @param playlistId The playlist ID to display
     */
    void showPlaylistDetail(const juce::String& playlistId);

    /** Navigate to saved posts view
     */
    void showSavedPosts();

    /** Navigate to archived posts view
     */
    void showArchivedPosts();

    /** Navigate to drafts view (Feature #5)
     */
    void showDrafts();

    /** Save current upload as draft and return to drafts view (Feature #5)
     */
    void saveCurrentUploadAsDraft();

    /** Show story viewer for a user's stories
     * @param userId The user ID whose stories to display
     */
    void showUserStory(const juce::String& userId);

    /** Show story viewer for a highlight's stories
     * @param highlight The highlight to display
     */
    void showHighlightStories(const StoryHighlight& highlight);

    /** Show the create highlight dialog
     */
    void showCreateHighlightDialog();

    /** Show the select highlight dialog for adding a story
     * @param storyId The story ID to add to a highlight
     */
    void showSelectHighlightDialog(const juce::String& storyId);

    /** Show share to message dialog for sharing a post to DMs
     * @param post The post to share
     */
    void showSharePostToMessage(const FeedPost& post);

    /** Show share to message dialog for sharing a story to DMs
     * @param story The story to share
     */
    void showShareStoryToMessage(const StoryData& story);

    /** Show notification settings dialog
     */
    void showNotificationSettings();

    /** Navigate back to the previous view in the navigation stack
     */
    void navigateBack();

    /** Handle successful login
     * @param username The authenticated username
     * @param email The user's email address
     * @param token The authentication token
     */
    void onLoginSuccess(const juce::String& username, const juce::String& email, const juce::String& token);

    /** Log out the current user and return to authentication view
     */
    void logout();

    /** Show confirmation dialog before logging out
     */
    void confirmAndLogout();

    //==============================================================================
    // Centralized user data store
    std::unique_ptr<UserDataStore> userDataStore;

    // Legacy user state (to be removed as we migrate to UserDataStore)
    juce::String username;
    juce::String email;
    juce::String profilePicUrl;
    juce::String authToken;

    //==============================================================================
    // Components
    std::unique_ptr<Auth> authComponent;
    std::unique_ptr<ProfileSetup> profileSetupComponent;
    std::unique_ptr<PostsFeed> postsFeedComponent;
    std::unique_ptr<Recording> recordingComponent;
    std::unique_ptr<Upload> uploadComponent;
    std::unique_ptr<DraftsView> draftsViewComponent;

    // Draft storage (Feature #5)
    std::unique_ptr<DraftStorage> draftStorage;
    std::unique_ptr<UserDiscovery> userDiscoveryComponent;
    std::unique_ptr<Profile> profileComponent;
    std::unique_ptr<Search> searchComponent;
    std::unique_ptr<MessagesList> messagesListComponent;
    std::unique_ptr<MessageThread> messageThreadComponent;
    std::unique_ptr<StoryRecording> storyRecordingComponent;
    std::unique_ptr<StoryViewer> storyViewerComponent;
    std::unique_ptr<HiddenSynth> hiddenSynthComponent;
    std::unique_ptr<Playlists> playlistsComponent;
    std::unique_ptr<PlaylistDetail> playlistDetailComponent;
    std::unique_ptr<MidiChallenges> midiChallengesComponent;
    std::unique_ptr<MidiChallengeDetail> midiChallengeDetailComponent;
    std::unique_ptr<SavedPosts> savedPostsComponent;
    std::unique_ptr<ArchivedPosts> archivedPostsComponent;

    // Story highlight dialogs
    std::unique_ptr<CreateHighlightDialog> createHighlightDialog;
    std::unique_ptr<SelectHighlightDialog> selectHighlightDialog;

    // Share to message dialog
    std::unique_ptr<ShareToMessageDialog> shareToMessageDialog;

    // Settings dialogs
    std::unique_ptr<NotificationSettings> notificationSettingsDialog;

    // StreamChatClient for getstream.io messaging
    std::unique_ptr<StreamChatClient> streamChatClient;

    // Network client for API calls
    std::unique_ptr<NetworkClient> networkClient;

    // WebSocket client for real-time updates
    std::unique_ptr<WebSocketClient> webSocketClient;

    // Connection status indicator
    std::unique_ptr<ConnectionIndicator> connectionIndicator;

    // Notification components
    std::unique_ptr<NotificationBell> notificationBell;
    std::unique_ptr<NotificationList> notificationList;
    bool notificationPanelVisible = false;

    // Central header component (shown on all post-login pages)
    std::unique_ptr<Header> headerComponent;

    // Tooltip window for displaying tooltips throughout the app
    std::unique_ptr<juce::TooltipWindow> tooltipWindow;

    //==============================================================================
    // Notification handling

    /** Initialize notification system and components
     */
    void setupNotifications();

    /** Show the notification panel overlay
     */
    void showNotificationPanel();

    /** Hide the notification panel
     */
    void hideNotificationPanel();

    /** Toggle notification panel visibility
     */
    void toggleNotificationPanel();

    /** Fetch notifications from the server
     */
    void fetchNotifications();

    /** Fetch notification counts (unseen/unread)
     */
    void fetchNotificationCounts();

    /** Start polling for notification updates
     */
    void startNotificationPolling();

    /** Stop polling for notification updates
     */
    void stopNotificationPolling();

    std::unique_ptr<juce::Timer> notificationPollTimer;

    //==============================================================================
    // Persistent state

    /** Save login state to persistent storage
     */
    void saveLoginState();

    /** Load login state from persistent storage
     */
    void loadLoginState();

    //==============================================================================
    // Crash detection

    /** Check if the plugin crashed on previous launch
     */
    void checkForPreviousCrash();

    /** Mark that the plugin shut down cleanly
     */
    void markCleanShutdown();

    //==============================================================================
    // OAuth polling for plugin-based OAuth flow

    /** Start polling for OAuth authentication completion
     * @param sessionId The OAuth session identifier
     * @param provider The OAuth provider name (e.g., "google", "discord")
     */
    void startOAuthPolling(const juce::String& sessionId, const juce::String& provider);

    /** Stop OAuth polling
     */
    void stopOAuthPolling();

    /** Poll the OAuth status from the server
     */
    void pollOAuthStatus();

    juce::String oauthSessionId;
    juce::String oauthProvider;
    std::unique_ptr<juce::Timer> oauthPollTimer;
    int oauthPollCount = 0;
    static constexpr int MAX_OAUTH_POLLS = 300; // 5 minutes at 1 second intervals

    //==============================================================================
    // WebSocket handling

    /** Connect to the WebSocket server for real-time updates
     */
    void connectWebSocket();

    /** Disconnect from the WebSocket server
     */
    void disconnectWebSocket();

    /** Handle incoming WebSocket message
     * @param message The received message
     */
    void handleWebSocketMessage(const WebSocketClient::Message& message);

    /** Handle WebSocket connection state change
     * @param state The new connection state
     */
    void handleWebSocketStateChange(WebSocketClient::ConnectionState state);

    /** Check if current user has active stories and update header indicator
     */
    void checkForActiveStories();

    //==============================================================================
    static constexpr int PLUGIN_WIDTH = 1000;
    static constexpr int PLUGIN_HEIGHT = 800;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SidechainAudioProcessorEditor)
};