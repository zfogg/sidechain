#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "stores/UserDataStore.h"
#include "stores/ChatStore.h"
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
#include "ui/profile/TwoFactorSettings.h"
#include "ui/profile/ActivityStatusSettings.h"
#include "ui/profile/EditProfile.h"
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
#include "ui/messages/UserPickerDialog.h"
#include "ui/stories/StoryRecording.h"
#include "ui/stories/StoryViewer.h"
#include "ui/stories/CreateHighlightDialog.h"
#include "ui/stories/SelectHighlightDialog.h"
#include "ui/messages/ShareToMessageDialog.h"
#include "ui/synth/HiddenSynth.h"
#include "ui/playlists/Playlists.h"
#include "ui/playlists/PlaylistDetail.h"
#include "ui/sounds/SoundPage.h"
#include "network/StreamChatClient.h"
#include "ui/animations/ViewTransitionManager.h"

// Using declarations for animations
using Sidechain::UI::Animations::ViewTransitionManager;

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
    enum class AppView { Authentication, ProfileSetup, PostsFeed, Recording, Upload, Drafts, Discovery, Profile, Search, Messages, MessageThread, StoryRecording, StoryViewer, HiddenSynth, Playlists, PlaylistDetail, SoundPage, MidiChallenges, MidiChallengeDetail, SavedPosts, ArchivedPosts };
    AppView currentView = AppView::Authentication;

    // Navigation direction for animated transitions
    enum class NavigationDirection { Forward, Backward, None };

    // Navigation stack for back button support
    juce::Array<AppView> navigationStack;
    juce::String profileUserIdToView;  // User ID for Profile view
    juce::String messageChannelType;   // Channel type for MessageThread view
    juce::String messageChannelId;     // Channel ID for MessageThread view
    juce::String playlistIdToView;    // Playlist ID for PlaylistDetail view
    juce::String soundIdToView;       // Sound ID for SoundPage view
    juce::String challengeIdToView;   // Challenge ID for MidiChallengeDetail view

    // View transition manager for smooth view transitions
    std::shared_ptr<ViewTransitionManager> viewTransitionManager;

    /** Get the component for a given view
     * @param view The view to get the component for
     * @return Pointer to the component, or nullptr if not found
     */
    juce::Component* getComponentForView(AppView view);

    /** Show a specific view in the editor with optional animation
     * @param view The view to display
     * @param direction Navigation direction for animation (Forward, Backward, or None)
     */
    void showView(AppView view, NavigationDirection direction = NavigationDirection::Forward);

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

    /** Navigate to sound page view
     * @param soundId The sound ID to display
     */
    void showSoundPage(const juce::String& soundId);

    /** Navigate to saved posts view
     */
    void showSavedPosts();

    /** Navigate to archived posts view
     */
    void showArchivedPosts();

    /** Navigate to drafts view
     */
    void showDrafts();

    /** Save current upload as draft and return to drafts view
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

    /** Show two-factor authentication settings dialog
     */
    void showTwoFactorSettings();

    /** Show activity status settings dialog
     */
    void showActivityStatusSettings();

    /** Show edit profile / settings dialog
     */
    void showEditProfile();

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

    /** Handle logout button click from profile page
     */
    void handleLogout();

    //==============================================================================
    // Centralized stores
    std::unique_ptr<UserDataStore> userDataStore;
    // ChatStore is a singleton, accessed via getInstance()

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

    // Draft storage
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
    std::unique_ptr<SoundPage> soundPageComponent;
    std::unique_ptr<MidiChallenges> midiChallengesComponent;
    std::unique_ptr<MidiChallengeDetail> midiChallengeDetailComponent;
    std::unique_ptr<SavedPosts> savedPostsComponent;
    std::unique_ptr<ArchivedPosts> archivedPostsComponent;

    // Story highlight dialogs
    std::unique_ptr<CreateHighlightDialog> createHighlightDialog;
    std::unique_ptr<SelectHighlightDialog> selectHighlightDialog;

    // Share to message dialog
    std::unique_ptr<ShareToMessageDialog> shareToMessageDialog;

    // User picker dialog for creating new conversations
    std::unique_ptr<class UserPickerDialog> userPickerDialog;

    // Settings dialogs
    std::unique_ptr<NotificationSettings> notificationSettingsDialog;
    std::unique_ptr<TwoFactorSettings> twoFactorSettingsDialog;
    std::unique_ptr<ActivityStatusSettings> activityStatusDialog;
    std::unique_ptr<EditProfile> editProfileDialog;

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
    // DPI Scaling

    /** Apply system DPI scaling for HiDPI displays
     * Detects the system scale factor and applies it to the plugin window
     */
    void applySystemDpiScaling();

    //==============================================================================
    // Plugin dimensions optimized for MacBook Pro displays (~75% of screen)
    static constexpr int PLUGIN_WIDTH = 1400;
    static constexpr int PLUGIN_HEIGHT = 900;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SidechainAudioProcessorEditor)
};