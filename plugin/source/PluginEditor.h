#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "stores/UserDataStore.h"
#include "network/NetworkClient.h"
#include "network/WebSocketClient.h"
#include "ui/auth/AuthComponent.h"
#include "ui/profile/ProfileSetupComponent.h"
#include "ui/profile/ProfileComponent.h"
#include "ui/feed/PostsFeedComponent.h"
#include "ui/recording/RecordingComponent.h"
#include "ui/recording/UploadComponent.h"
#include "ui/common/ConnectionIndicator.h"
#include "ui/common/HeaderComponent.h"
#include "ui/notifications/NotificationBellComponent.h"
#include "ui/notifications/NotificationListComponent.h"
#include "ui/social/UserDiscoveryComponent.h"
#include "ui/search/SearchComponent.h"
#include "ui/messages/MessagesListComponent.h"
#include "ui/messages/MessageThreadComponent.h"
#include "ui/stories/StoryRecordingComponent.h"
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
    void paint(juce::Graphics&) override;
    void resized() override;

    // ChangeListener - for UserDataStore updates
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

private:
    SidechainAudioProcessor& audioProcessor;

    //==============================================================================
    // View management
    enum class AppView { Authentication, ProfileSetup, PostsFeed, Recording, Upload, Discovery, Profile, Search, Messages, MessageThread, StoryRecording };
    AppView currentView = AppView::Authentication;

    // Navigation stack for back button support
    juce::Array<AppView> navigationStack;
    juce::String profileUserIdToView;  // User ID for Profile view
    juce::String messageChannelType;   // Channel type for MessageThread view
    juce::String messageChannelId;     // Channel ID for MessageThread view

    void showView(AppView view);
    void showMessageThread(const juce::String& channelType, const juce::String& channelId);
    void showProfile(const juce::String& userId);  // Navigate to a user's profile
    void navigateBack();  // Go back to previous view
    void onLoginSuccess(const juce::String& username, const juce::String& email, const juce::String& token);
    void logout();

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
    std::unique_ptr<AuthComponent> authComponent;
    std::unique_ptr<ProfileSetupComponent> profileSetupComponent;
    std::unique_ptr<PostsFeedComponent> postsFeedComponent;
    std::unique_ptr<RecordingComponent> recordingComponent;
    std::unique_ptr<UploadComponent> uploadComponent;
    std::unique_ptr<UserDiscoveryComponent> userDiscoveryComponent;
    std::unique_ptr<ProfileComponent> profileComponent;
    std::unique_ptr<SearchComponent> searchComponent;
    std::unique_ptr<MessagesListComponent> messagesListComponent;
    std::unique_ptr<MessageThreadComponent> messageThreadComponent;
    std::unique_ptr<StoryRecordingComponent> storyRecordingComponent;

    // StreamChatClient for getstream.io messaging
    std::unique_ptr<StreamChatClient> streamChatClient;

    // Network client for API calls
    std::unique_ptr<NetworkClient> networkClient;

    // WebSocket client for real-time updates
    std::unique_ptr<WebSocketClient> webSocketClient;

    // Connection status indicator
    std::unique_ptr<ConnectionIndicator> connectionIndicator;

    // Notification components
    std::unique_ptr<NotificationBellComponent> notificationBell;
    std::unique_ptr<NotificationListComponent> notificationList;
    bool notificationPanelVisible = false;

    // Central header component (shown on all post-login pages)
    std::unique_ptr<HeaderComponent> headerComponent;

    //==============================================================================
    // Notification handling
    void setupNotifications();
    void showNotificationPanel();
    void hideNotificationPanel();
    void toggleNotificationPanel();
    void fetchNotifications();
    void fetchNotificationCounts();
    void startNotificationPolling();
    void stopNotificationPolling();

    std::unique_ptr<juce::Timer> notificationPollTimer;

    //==============================================================================
    // Persistent state
    void saveLoginState();
    void loadLoginState();

    //==============================================================================
    // Crash detection
    void checkForPreviousCrash();
    void markCleanShutdown();

    //==============================================================================
    // OAuth polling for plugin-based OAuth flow
    void startOAuthPolling(const juce::String& sessionId, const juce::String& provider);
    void stopOAuthPolling();
    void pollOAuthStatus();

    juce::String oauthSessionId;
    juce::String oauthProvider;
    std::unique_ptr<juce::Timer> oauthPollTimer;
    int oauthPollCount = 0;
    static constexpr int MAX_OAUTH_POLLS = 300; // 5 minutes at 1 second intervals

    //==============================================================================
    // WebSocket handling
    void connectWebSocket();
    void disconnectWebSocket();
    void handleWebSocketMessage(const WebSocketClient::Message& message);
    void handleWebSocketStateChange(WebSocketClient::ConnectionState state);

    //==============================================================================
    static constexpr int PLUGIN_WIDTH = 1000;
    static constexpr int PLUGIN_HEIGHT = 800;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SidechainAudioProcessorEditor)
};