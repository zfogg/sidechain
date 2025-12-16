#include "../AppStore.h"
#include "../../util/logging/Logger.h"

namespace Sidechain {
namespace Stores {

void AppStore::loadStoriesFeed() {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot load stories feed - network client not set");
    return;
  }

  updateState([](AppState &state) { state.stories.isFeedLoading = true; });

  networkClient->getStoriesFeed([this](Outcome<juce::var> result) {
    if (result.isOk()) {
      const auto data = result.getValue();
      juce::Array<juce::var> storiesList;

      if (data.isArray()) {
        for (int i = 0; i < data.size(); ++i) {
          storiesList.add(data[i]);
        }
      }

      updateState([storiesList](AppState &state) {
        state.stories.feedUserStories = storiesList;
        state.stories.isFeedLoading = false;
        state.stories.storiesError = "";
        Util::logInfo("AppStore", "Loaded " + juce::String(storiesList.size()) + " stories from feed");
      });
    } else {
      updateState([result](AppState &state) {
        state.stories.isFeedLoading = false;
        state.stories.storiesError = result.getError();
        Util::logError("AppStore", "Failed to load stories feed: " + result.getError());
      });
    }

    notifyObservers();
  });
}

void AppStore::loadMyStories() {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot load my stories - network client not set");
    return;
  }

  updateState([](AppState &state) { state.stories.isMyStoriesLoading = true; });

  // TODO: Implement loading user's own stories when NetworkClient provides an API
  // For now, just mark as not loading
  updateState([](AppState &state) {
    state.stories.isMyStoriesLoading = false;
    Util::logInfo("AppStore", "My stories endpoint not yet available");
  });

  notifyObservers();
}

void AppStore::markStoryAsViewed(const juce::String &storyId) {
  if (!networkClient) {
    return;
  }

  Util::logInfo("AppStore", "Marking story as viewed: " + storyId);

  networkClient->viewStory(storyId, [this](Outcome<juce::var> result) {
    if (!result.isOk()) {
      Util::logError("AppStore", "Failed to mark story as viewed: " + result.getError());
    }
  });
}

void AppStore::deleteStory(const juce::String &storyId) {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot delete story - network client not set");
    return;
  }

  Util::logInfo("AppStore", "Deleting story: " + storyId);

  networkClient->deleteStory(storyId, [this, storyId](Outcome<juce::var> result) {
    if (result.isOk()) {
      updateState([storyId](AppState &state) {
        // Remove from my stories
        for (int i = state.stories.myStories.size() - 1; i >= 0; --i) {
          auto story = state.stories.myStories[i];
          if (story.hasProperty("id") && story.getProperty("id", juce::var()).toString() == storyId) {
            state.stories.myStories.remove(i);
            Util::logInfo("AppStore", "Story deleted: " + storyId);
            break;
          }
        }
      });
      notifyObservers();
    } else {
      updateState([result](AppState &state) {
        state.stories.storiesError = result.getError();
        Util::logError("AppStore", "Failed to delete story: " + result.getError());
      });
      notifyObservers();
    }
  });
}

void AppStore::createHighlight(const juce::String &name, const juce::Array<juce::String> &storyIds) {
  // TODO: Implement highlight creation when NetworkClient provides createHighlight API
  // For now, this is a stub that logs the request
  Util::logInfo("AppStore", "Creating highlight: " + name + " with " + juce::String(storyIds.size()) + " stories");

  updateState([](AppState &state) { state.stories.storiesError = "Highlight creation not yet supported"; });

  notifyObservers();
}

} // namespace Stores
} // namespace Sidechain
