#include "../AppStore.h"
#include "../../util/logging/Logger.h"
#include <nlohmann/json.hpp>

namespace Sidechain {
namespace Stores {

void AppStore::loadStoriesFeed() {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot load stories feed - network client not set");
    return;
  }

  StoriesState newState = sliceManager.getStoriesSlice()->getState();
  newState.isFeedLoading = true;
  sliceManager.getStoriesSlice()->setState(newState);

  networkClient->getStoriesFeed([this](Outcome<juce::var> result) {
    if (result.isOk()) {
      const auto data = result.getValue();
      std::vector<std::shared_ptr<Sidechain::Story>> storiesList;

      if (data.isArray()) {
        for (int i = 0; i < data.size(); ++i) {
          try {
            // Convert juce::var to nlohmann::json for new API
            auto jsonStr = juce::JSON::toString(data[i]);
            auto jsonObj = nlohmann::json::parse(jsonStr.toStdString());

            // Use new SerializableModel API
            auto storyResult = Sidechain::SerializableModel<Sidechain::Story>::createFromJson(jsonObj);
            if (storyResult.isOk()) {
              storiesList.push_back(storyResult.getValue());
            } else {
              Util::logError("AppStore", "Failed to parse story: " + storyResult.getError());
            }
          } catch (const std::exception &e) {
            Util::logError("AppStore", "Exception parsing story: " + juce::String(e.what()));
          }
        }
      }

      StoriesState feedState = sliceManager.getStoriesSlice()->getState();
      feedState.feedUserStories = storiesList;
      feedState.isFeedLoading = false;
      feedState.storiesError = "";
      Util::logInfo("AppStore", "Loaded " + juce::String(storiesList.size()) + " stories from feed");
      sliceManager.getStoriesSlice()->setState(feedState);
    } else {
      StoriesState feedErrorState = sliceManager.getStoriesSlice()->getState();
      feedErrorState.isFeedLoading = false;
      feedErrorState.storiesError = result.getError();
      Util::logError("AppStore", "Failed to load stories feed: " + result.getError());
      sliceManager.getStoriesSlice()->setState(feedErrorState);
    }
  });
}

void AppStore::loadMyStories() {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot load my stories - network client not set");
    return;
  }

  StoriesState myStoriesStartState = sliceManager.getStoriesSlice()->getState();
  myStoriesStartState.isMyStoriesLoading = true;
  sliceManager.getStoriesSlice()->setState(myStoriesStartState);

  // TODO: Implement loading user's own stories when NetworkClient provides an API
  // For now, just mark as not loading
  StoriesState myStoriesDoneState = sliceManager.getStoriesSlice()->getState();
  myStoriesDoneState.isMyStoriesLoading = false;
  Util::logInfo("AppStore", "My stories endpoint not yet available");
  sliceManager.getStoriesSlice()->setState(myStoriesDoneState);
}

void AppStore::markStoryAsViewed(const juce::String &storyId) {
  if (!networkClient) {
    return;
  }

  Util::logInfo("AppStore", "Marking story as viewed: " + storyId);

  networkClient->viewStory(storyId, [](Outcome<juce::var> result) {
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
      StoriesState deleteState = sliceManager.getStoriesSlice()->getState();
      // Remove from my stories
      for (int i = static_cast<int>(deleteState.myStories.size()) - 1; i >= 0; --i) {
        auto story = deleteState.myStories[static_cast<size_t>(i)];
        if (story && story->id == storyId) {
          deleteState.myStories.erase(deleteState.myStories.begin() + i);
          Util::logInfo("AppStore", "Story deleted: " + storyId);
          break;
        }
      }
      sliceManager.getStoriesSlice()->setState(deleteState);
    } else {
      StoriesState deleteErrorState = sliceManager.getStoriesSlice()->getState();
      deleteErrorState.storiesError = result.getError();
      Util::logError("AppStore", "Failed to delete story: " + result.getError());
      sliceManager.getStoriesSlice()->setState(deleteErrorState);
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
      if (storyIds.size() > 0) {
        Util::logInfo("AppStore", "Adding " + juce::String(storyIds.size()) + " stories to highlight");

        // Add each story to the highlight
        for (const auto &storyId : storyIds) {
          networkClient->addStoryToHighlight(
              highlightId, storyId, [storyId, highlightId](Outcome<juce::var> addResult) {
                if (addResult.isOk()) {
                  Util::logInfo("AppStore", "Added story " + storyId + " to highlight " + highlightId);
                } else {
                  Util::logError("AppStore", "Failed to add story to highlight: " + addResult.getError());
                }
              });
        }
      }

      StoriesState successState = sliceManager.getStoriesSlice()->getState();
      successState.storiesError = "";
      sliceManager.getStoriesSlice()->setState(successState);
    } else {
      StoriesState highlightErrorState = sliceManager.getStoriesSlice()->getState();
      highlightErrorState.storiesError = result.getError();
      Util::logError("AppStore", "Failed to create highlight: " + result.getError());
      sliceManager.getStoriesSlice()->setState(highlightErrorState);
    }
  });
}

} // namespace Stores
} // namespace Sidechain
