#pragma once

#include "../network/NetworkClient.h"
#include "../util/logging/Logger.h"
#include "Store.h"
#include <JuceHeader.h>

namespace Sidechain {
namespace Stores {

/**
 * StoryData - Story information for display
 */
struct StoryData {
  juce::String id;
  juce::String userId;
  juce::String username;
  juce::String userAvatarUrl;
  juce::String audioUrl;
  juce::String filename;
  juce::String midiFilename;
  float audioDuration = 0.0f;
  juce::var midiData;
  juce::String midiPatternId;
  int viewCount = 0;
  bool viewed = false;
  juce::Time expiresAt;
  juce::Time createdAt;

  bool isExpired() const {
    return juce::Time::getCurrentTime() > expiresAt;
  }

  bool hasDownloadableMIDI() const {
    return midiPatternId.isNotEmpty();
  }

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

/**
 * UserStories - Group of stories from a single user
 */
struct UserStories {
  juce::String userId;
  juce::String username;
  juce::String avatarUrl;
  juce::Array<StoryData> stories;
  bool hasUnviewed = false;
};

/**
 * StoriesState - Immutable state for stories (manages all story-related data)
 */
struct StoriesState {
  // Stories feed (stories from other users)
  juce::Array<UserStories> feedUserStories;
  bool feedHasOwnStory = false;
  bool feedIsLoading = false;

  // Current user's stories archive (for viewing/managing own stories)
  juce::Array<StoryData> myStories;
  bool myStoriesIsLoading = false;

  // Story highlights
  juce::Array<StoryData> highlights;
  bool highlightsIsLoading = false;

  // Current user context
  juce::String currentUserId;
  juce::String currentUserAvatarUrl;

  // Error tracking
  juce::String errorMessage;
  int64_t lastUpdated = 0;
};

/**
 * StoriesStore - Reactive store for managing all stories data (7.5.4.1.1)
 *
 * Manages stories data for multiple components:
 * - StoriesFeed - horizontal scrollable feed of stories from other users
 * - StoryArchive - list of current user's stories
 * - StoryHighlights - list of story highlights
 * - Any other story-related component
 *
 * Features:
 * - Load stories feed from network
 * - Load current user's stories archive
 * - Load story highlights
 * - Track loading state and errors
 * - Group stories by user
 *
 * Usage:
 * ```cpp
 * auto storiesStore = std::make_shared<StoriesStore>(networkClient);
 * storiesStore->subscribe([this](const StoriesState& state) {
 *   updateStoriesFeed(state.feedUserStories);
 *   updateMyStories(state.myStories);
 * });
 * storiesStore->loadStoriesFeed(currentUserId, currentAvatarUrl);
 * storiesStore->loadMyStories(currentUserId);
 * storiesStore->loadHighlights(currentUserId);
 * ```
 */
class StoriesStore : public Store<StoriesState> {
public:
  explicit StoriesStore(NetworkClient *client);
  ~StoriesStore() override = default;

  //==============================================================================
  // Stories Feed Loading
  void loadStoriesFeed(const juce::String &currentUserId, const juce::String &currentUserAvatarUrl);
  void refreshStoriesFeed();

  //==============================================================================
  // My Stories (archive) Loading
  void loadMyStories(const juce::String &userId);
  void refreshMyStories();

  //==============================================================================
  // Story Highlights Loading
  void loadHighlights(const juce::String &userId);
  void refreshHighlights();

  //==============================================================================
  // Current State Access - Feed
  bool isFeedLoading() const {
    return getState().feedIsLoading;
  }

  juce::Array<UserStories> getFeedUserStories() const {
    return getState().feedUserStories;
  }

  bool hasFeedOwnStory() const {
    return getState().feedHasOwnStory;
  }

  //==============================================================================
  // Current State Access - My Stories
  bool isMyStoriesLoading() const {
    return getState().myStoriesIsLoading;
  }

  juce::Array<StoryData> getMyStories() const {
    return getState().myStories;
  }

  //==============================================================================
  // Current State Access - Highlights
  bool isHighlightsLoading() const {
    return getState().highlightsIsLoading;
  }

  juce::Array<StoryData> getHighlights() const {
    return getState().highlights;
  }

  //==============================================================================
  // Current State Access - General
  juce::String getCurrentUserId() const {
    return getState().currentUserId;
  }

  juce::String getError() const {
    return getState().errorMessage;
  }

protected:
  //==============================================================================
  // Helper methods
  void updateStoriesFeed(const juce::Array<UserStories> &stories, bool hasOwnStory);
  void updateMyStories(const juce::Array<StoryData> &stories);
  void updateHighlights(const juce::Array<StoryData> &stories);

private:
  NetworkClient *networkClient;

  //==============================================================================
  // Network callbacks
  void handleStoriesFeedLoaded(Outcome<juce::var> result);
  void handleMyStoriesLoaded(Outcome<juce::var> result);
  void handleHighlightsLoaded(Outcome<juce::var> result);

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StoriesStore)
};

} // namespace Stores
} // namespace Sidechain
