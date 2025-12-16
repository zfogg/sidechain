#include "StoriesStore.h"
#include "../util/Log.h"

namespace Sidechain {
namespace Stores {

//==============================================================================
StoriesStore::StoriesStore(NetworkClient *client) : networkClient(client) {
  Log::info("StoriesStore: Initializing");
  setState(StoriesState{});
}

//==============================================================================
void StoriesStore::loadStoriesFeed(const juce::String &currentUserId, const juce::String &currentUserAvatarUrl) {
  if (networkClient == nullptr)
    return;

  auto state = getState();
  state.feedIsLoading = true;
  state.currentUserId = currentUserId;
  state.currentUserAvatarUrl = currentUserAvatarUrl;
  setState(state);

  Log::info("StoriesStore: Loading stories feed");

  networkClient->getStoriesFeed([this](Outcome<juce::var> result) { handleStoriesFeedLoaded(result); });
}

//==============================================================================
void StoriesStore::refreshStoriesFeed() {
  auto state = getState();
  if (state.currentUserId.isEmpty())
    return;

  Log::info("StoriesStore: Refreshing stories feed");
  loadStoriesFeed(state.currentUserId, state.currentUserAvatarUrl);
}

//==============================================================================
void StoriesStore::loadMyStories(const juce::String &userId) {
  if (networkClient == nullptr || userId.isEmpty())
    return;

  auto state = getState();
  state.myStoriesIsLoading = true;
  setState(state);

  Log::info("StoriesStore: Loading my stories");

  // Would call networkClient method when available
  // For now, just update state
  updateMyStories(juce::Array<StoryData>());
}

//==============================================================================
void StoriesStore::refreshMyStories() {
  auto state = getState();
  if (state.currentUserId.isEmpty())
    return;

  Log::info("StoriesStore: Refreshing my stories");
  loadMyStories(state.currentUserId);
}

//==============================================================================
void StoriesStore::loadHighlights(const juce::String &userId) {
  if (networkClient == nullptr || userId.isEmpty())
    return;

  auto state = getState();
  state.highlightsIsLoading = true;
  setState(state);

  Log::info("StoriesStore: Loading highlights");

  // Would call networkClient method when available
  // For now, just update state
  updateHighlights(juce::Array<StoryData>());
}

//==============================================================================
void StoriesStore::refreshHighlights() {
  auto state = getState();
  if (state.currentUserId.isEmpty())
    return;

  Log::info("StoriesStore: Refreshing highlights");
  loadHighlights(state.currentUserId);
}

//==============================================================================
void StoriesStore::handleStoriesFeedLoaded(Outcome<juce::var> result) {
  auto state = getState();
  state.feedIsLoading = false;

  if (result.isError()) {
    Log::error("StoriesStore: Failed to load stories feed - " + result.getError());
    state.errorMessage = "Failed to load stories";
    setState(state);
    return;
  }

  auto response = result.getValue();
  juce::Array<UserStories> userStoriesGroups;
  bool hasOwnStory = false;

  if (response.hasProperty("stories")) {
    auto *storiesArray = response["stories"].getArray();
    if (storiesArray) {
      // Group stories by user
      std::map<juce::String, UserStories> groupedStories;

      for (const auto &storyVar : *storiesArray) {
        StoryData story;
        story.id = storyVar["id"].toString();
        story.userId = storyVar["user_id"].toString();
        story.audioUrl = storyVar["audio_url"].toString();
        story.filename = storyVar["filename"].toString();
        story.midiFilename = storyVar["midi_filename"].toString();
        story.audioDuration = static_cast<float>(storyVar["audio_duration"]);
        story.midiData = storyVar["midi_data"];
        story.midiPatternId = storyVar["midi_pattern_id"].toString();
        story.viewCount = static_cast<int>(storyVar["view_count"]);
        story.viewed = static_cast<bool>(storyVar["viewed"]);

        // Parse user info
        if (storyVar.hasProperty("user")) {
          story.username = storyVar["user"]["username"].toString();
          story.userAvatarUrl = storyVar["user"]["avatar_url"].toString();
        }

        // Parse timestamps (ISO 8601 format)
        story.expiresAt = juce::Time::getCurrentTime() + juce::RelativeTime::hours(24);

        // Check if this is user's own story
        if (story.userId == state.currentUserId)
          hasOwnStory = true;

        // Group by user
        if (groupedStories.find(story.userId) == groupedStories.end()) {
          UserStories userStories;
          userStories.userId = story.userId;
          userStories.username = story.username;
          userStories.avatarUrl = story.userAvatarUrl;
          groupedStories[story.userId] = userStories;
        }

        groupedStories[story.userId].stories.add(story);
        if (!story.viewed)
          groupedStories[story.userId].hasUnviewed = true;
      }

      // Convert map to array
      for (const auto &pair : groupedStories) {
        userStoriesGroups.add(pair.second);
      }

      Log::info("StoriesStore: Loaded " + juce::String(userStoriesGroups.size()) + " user stories");
    }
  }

  updateStoriesFeed(userStoriesGroups, hasOwnStory);
}

//==============================================================================
void StoriesStore::handleMyStoriesLoaded(Outcome<juce::var> result) {
  // TODO: implement when backend API available
}

//==============================================================================
void StoriesStore::handleHighlightsLoaded(Outcome<juce::var> result) {
  // TODO: implement when backend API available
}

//==============================================================================
void StoriesStore::updateStoriesFeed(const juce::Array<UserStories> &stories, bool hasOwnStory) {
  auto state = getState();
  state.feedUserStories = stories;
  state.feedHasOwnStory = hasOwnStory;
  state.errorMessage = "";
  state.lastUpdated = juce::Time::getCurrentTime().toMilliseconds();
  setState(state);
}

//==============================================================================
void StoriesStore::updateMyStories(const juce::Array<StoryData> &stories) {
  auto state = getState();
  state.myStories = stories;
  state.myStoriesIsLoading = false;
  state.errorMessage = "";
  state.lastUpdated = juce::Time::getCurrentTime().toMilliseconds();
  setState(state);
}

//==============================================================================
void StoriesStore::updateHighlights(const juce::Array<StoryData> &stories) {
  auto state = getState();
  state.highlights = stories;
  state.highlightsIsLoading = false;
  state.errorMessage = "";
  state.lastUpdated = juce::Time::getCurrentTime().toMilliseconds();
  setState(state);
}

} // namespace Stores
} // namespace Sidechain
