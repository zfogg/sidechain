#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "AuthComponent.h"
#include "ProfileSetupComponent.h"
#include "PostsFeedComponent.h"
#include "RecordingComponent.h"
#include "UploadComponent.h"
#include "NetworkClient.h"
#include "WebSocketClient.h"
#include "ConnectionIndicator.h"
#include "NotificationBellComponent.h"
#include "NotificationListComponent.h"
#include "UserDiscoveryComponent.h"

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
class SidechainAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    SidechainAudioProcessorEditor(SidechainAudioProcessor&);
    ~SidechainAudioProcessorEditor() override;

    //==============================================================================
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    SidechainAudioProcessor& audioProcessor;

    //==============================================================================
    // View management
    enum class AppView { Authentication, ProfileSetup, PostsFeed, Recording, Upload, Discovery };
    AppView currentView = AppView::Authentication;

    void showView(AppView view);
    void onLoginSuccess(const juce::String& username, const juce::String& email, const juce::String& token);
    void logout();

    //==============================================================================
    // User state
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