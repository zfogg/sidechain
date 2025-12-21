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

  StoriesState newState = sliceManager.stories->getState();
  newState.isFeedLoading = true;
  sliceManager.stories->setState(newState);

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

      StoriesState feedState = sliceManager.stories->getState();
      feedState.feedUserStories = storiesList;
      feedState.isFeedLoading = false;
      feedState.storiesError = "";
      Util::logInfo("AppStore", "Loaded " + juce::String(storiesList.size()) + " stories from feed");
      sliceManager.stories->setState(feedState);
    } else {
      StoriesState feedErrorState = sliceManager.stories->getState();
      feedErrorState.isFeedLoading = false;
      feedErrorState.storiesError = result.getError();
      Util::logError("AppStore", "Failed to load stories feed: " + result.getError());
      sliceManager.stories->setState(feedErrorState);
    }
  });
}

void AppStore::loadMyStories() {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot load my stories - network client not set");
    return;
  }

  StoriesState myStoriesStartState = sliceManager.stories->getState();
  myStoriesStartState.isMyStoriesLoading = true;
  sliceManager.stories->setState(myStoriesStartState);

  // Fetch user's own stories from NetworkClient
  networkClient->getStoriesFeed([this](Outcome<juce::var> result) {
    if (!result.isOk()) {
      Util::logError("AppStore", "Failed to load my stories: " + result.getError());
      StoriesState errorState = sliceManager.stories->getState();
      errorState.isMyStoriesLoading = false;
      errorState.myStoriesError = result.getError();
      sliceManager.stories->setState(errorState);
      return;
    }

    // Parse the response - should be an array of stories
    const auto &data = result.getValue();
    StoriesState newState = sliceManager.stories->getState();
    newState.isMyStoriesLoading = false;

    // Get current user ID to filter only user's own stories
    const auto currentAuthState = sliceManager.auth->getState();
    const juce::String currentUserId = currentAuthState.userId;

    if (data.isArray()) {
      // Clear existing stories and populate only with user's own stories
      newState.myStories.clear();
      for (int i = 0; i < data.size(); ++i) {
        const auto &storyData = data[i];
        if (storyData.isObject()) {
          auto *obj = storyData.getDynamicObject();
          if (obj) {
            juce::String storyUserId = obj->getProperty("user_id").toString();
            // Only include stories that belong to the current user
            if (storyUserId == currentUserId) {
              Story story;
              story.id = obj->getProperty("id").toString();
              story.audioUrl = obj->getProperty("audio_url").toString();
              story.createdAt = obj->getProperty("created_at").toString();
              story.userId = storyUserId;
              story.userName = obj->getProperty("user_name").toString();

              newState.myStories.push_back(story);
            }
          }
        }
      }
      Util::logInfo("AppStore", "Loaded " + juce::String(newState.myStories.size()) + " of my stories");
    }

    sliceManager.stories->setState(newState);
  });
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
      StoriesState deleteState = sliceManager.stories->getState();
      // Remove from my stories
      for (int i = static_cast<int>(deleteState.myStories.size()) - 1; i >= 0; --i) {
        auto story = deleteState.myStories[static_cast<size_t>(i)];
        if (story && story->id == storyId) {
          deleteState.myStories.erase(deleteState.myStories.begin() + i);
          Util::logInfo("AppStore", "Story deleted: " + storyId);
          break;
        }
      }
      sliceManager.stories->setState(deleteState);
    } else {
      StoriesState deleteErrorState = sliceManager.stories->getState();
      deleteErrorState.storiesError = result.getError();
      Util::logError("AppStore", "Failed to delete story: " + result.getError());
      sliceManager.stories->setState(deleteErrorState);
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

      StoriesState successState = sliceManager.stories->getState();
      successState.storiesError = "";
      sliceManager.stories->setState(successState);
    } else {
      StoriesState highlightErrorState = sliceManager.stories->getState();
      highlightErrorState.storiesError = result.getError();
      Util::logError("AppStore", "Failed to create highlight: " + result.getError());
      sliceManager.stories->setState(highlightErrorState);
    }
  });
}

} // namespace Stores
} // namespace Sidechain
