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
  if (!networkClient) {
    Util::logError("AppStore", "Cannot create highlight - network client not set");
    return;
  }

  if (name.isEmpty()) {
    Util::logError("AppStore", "Cannot create highlight - name cannot be empty");
    return;
  }

  Util::logInfo("AppStore", "Creating highlight: " + name + " with " + juce::String(storyIds.size()) + " stories");

  // Create highlight with name and description
  networkClient->createHighlight(name, "", [this, name, storyIds](Outcome<juce::var> result) {
    if (result.isOk()) {
      const auto data = result.getValue();
      const auto highlightId = data.getProperty("id", juce::var()).toString();

      Util::logInfo("AppStore", "Highlight created successfully: " + highlightId);

      // If we have stories to add, add them one by one
      if (!storyIds.isEmpty()) {
        Util::logInfo("AppStore", "Adding " + juce::String(storyIds.size()) + " stories to highlight");

        // Add each story to the highlight
        for (const auto &storyId : storyIds) {
          networkClient->addStoryToHighlight(highlightId, storyId, [this, storyId, highlightId](Outcome<juce::var> addResult) {
            if (addResult.isOk()) {
              Util::logInfo("AppStore", "Added story " + storyId + " to highlight " + highlightId);
            } else {
              Util::logError("AppStore", "Failed to add story to highlight: " + addResult.getError());
            }
          });
        }
      }

      updateState([](AppState &state) { state.stories.storiesError = ""; });
      notifyObservers();
    } else {
      updateState([result](AppState &state) {
        state.stories.storiesError = result.getError();
        Util::logError("AppStore", "Failed to create highlight: " + result.getError());
      });
      notifyObservers();
    }
  });
}

} // namespace Stores
} // namespace Sidechain
