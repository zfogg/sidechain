#pragma once

#include "../../models/FeedPost.h"
#include "../../stores/AppStore.h"
#include "../common/AppStoreComponent.h"
#include <JuceHeader.h>
#include <memory>

class NetworkClient;

//==============================================================================
/**
 * Story data structure for displaying in the stories feed
 */
struct StoryData {
  juce::String id;
  juce::String userId;
  juce::String username;
  juce::String userAvatarUrl;
  juce::String audioUrl;
  juce::String filename;     // Display filename for audio
  juce::String midiFilename; // Display filename for MIDI
  float audioDuration = 0.0f;
  juce::var midiData;
  juce::String midiPatternId; // ID of standalone MIDI pattern (for download)
  int viewCount = 0;
  bool viewed = false;
  juce::Time expiresAt;
  juce::Time createdAt;

  // Check if story is expired
  bool isExpired() const {
    return juce::Time::getCurrentTime() > expiresAt;
  }

  // Check if story has downloadable MIDI
  bool hasDownloadableMIDI() const {
    return midiPatternId.isNotEmpty();
  }

  // Get time until expiration
  juce::String getExpirationText() const {
    auto now = juce::Time::getCurrentTime();
    auto diff = expiresAt - now;
    int hours = static_cast<int>(diff.inHours());

    if (hours < 1) {
      int minutes = static_cast<int>(diff.inMinutes());
      return juce::String(minutes) + "m left";
    }
    return juce::String(hours) + "h left";
  }
};

//==============================================================================
/**
 * StoriesFeed displays a horizontal scrollable list of story circles
 * (7.5.4.1.1)
 *
 * Features:
 * - Horizontal scroll with story avatar circles
 * - Ring indicator for unviewed stories
 * - "Your Story" circle at start
 * - Story count badge for multiple stories
 * - Tap to open story viewer
 */
class StoriesFeed : public Sidechain::UI::AppStoreComponent<Sidechain::Stores::StoriesState>, public juce::Timer {
public:
  StoriesFeed(Sidechain::Stores::AppStore *store = nullptr);
  ~StoriesFeed() override;

  //==============================================================================
  void paint(juce::Graphics &) override;
  void resized() override;
  void mouseUp(const juce::MouseEvent &event) override;
  void mouseWheelMove(const juce::MouseEvent &event, const juce::MouseWheelDetails &wheel) override;

  //==============================================================================
  // Timer callback for scroll animation
  void timerCallback() override;

  //==============================================================================
  // Data management
  void setNetworkClient(NetworkClient *client) {
    networkClient = client;
  }
  void setCurrentUserId(const juce::String &userId) {
    currentUserId = userId;
  }
  void setCurrentUserAvatarUrl(const juce::String &url) {
    currentUserAvatarUrl = url;
  }

  // Load stories from network
  void loadStories();

  // Set stories data directly
  void setStories(const std::vector<StoryData> &newStories);

  // Check if user has active story
  bool hasOwnStory() const;

  //==============================================================================
  // Callbacks
  std::function<void()> onCreateStory;
  std::function<void(const juce::String &userId, int storyIndex)> onStoryTapped;

protected:
  void onAppStateChanged(const Sidechain::Stores::StoriesState &state) override;
  void subscribeToAppStore() override;

private:
  //==============================================================================
  NetworkClient *networkClient = nullptr;
  juce::String currentUserId;
  juce::String currentUserAvatarUrl;

  // Story data grouped by user
  struct UserStories {
    juce::String userId;
    juce::String username;
    juce::String avatarUrl;
    std::vector<StoryData> stories;
    bool hasUnviewed = false;
  };

  std::vector<UserStories> userStoriesGroups;

  // Scroll state
  float scrollOffset = 0.0f;
  float targetScrollOffset = 0.0f;
  float maxScrollOffset = 0.0f;

  // UI constants
  static constexpr int CIRCLE_SIZE = 64;
  static constexpr int CIRCLE_PADDING = 12;
  static constexpr int RING_THICKNESS = 3;
  static constexpr int LABEL_HEIGHT = 20;

  // Cached avatar images
  std::map<juce::String, juce::Image> avatarCache;

  // Drawing helpers
  void drawCreateStoryCircle(juce::Graphics &g, juce::Rectangle<int> bounds);
  void drawStoryCircle(juce::Graphics &g, juce::Rectangle<int> bounds, const UserStories &userStories, int index);

  // Calculate total content width
  int calculateContentWidth() const;

  // Get circle bounds at index (0 = create story, 1+ = user stories)
  juce::Rectangle<int> getCircleBounds(int index) const;

  // Download avatar image
  void loadAvatarImage(const juce::String &userId, const juce::String &avatarUrl);

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StoriesFeed)
};
