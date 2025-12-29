#include "../AppStore.h"
#include "../../util/logging/Logger.h"
#include "../../util/rx/JuceScheduler.h"
#include "../../models/Story.h"
#include <nlohmann/json.hpp>
#include <rxcpp/rx.hpp>

namespace Sidechain {
namespace Stores {

void AppStore::loadStoriesFeed() {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot load stories feed - network client not set");
    return;
  }

  StoriesState newState = stateManager.stories->getState();
  newState.isFeedLoading = true;
  stateManager.stories->setState(newState);

  loadStoriesFeedObservable().subscribe(
      [this](const std::vector<Story> &stories) {
        // Convert to shared_ptr for state
        std::vector<std::shared_ptr<Sidechain::Story>> storiesList;
        storiesList.reserve(stories.size());
        for (const auto &s : stories) {
          storiesList.push_back(std::make_shared<Sidechain::Story>(s));
        }

        StoriesState feedState = stateManager.stories->getState();
        feedState.feedUserStories = std::move(storiesList);
        feedState.isFeedLoading = false;
        feedState.storiesError = "";
        Util::logInfo("AppStore", "Loaded " + juce::String(feedState.feedUserStories.size()) + " stories from feed");
        stateManager.stories->setState(feedState);
      },
      [this](std::exception_ptr ep) {
        juce::String errorMsg;
        try {
          std::rethrow_exception(ep);
        } catch (const std::exception &e) {
          errorMsg = e.what();
        }
        StoriesState feedErrorState = stateManager.stories->getState();
        feedErrorState.isFeedLoading = false;
        feedErrorState.storiesError = errorMsg;
        Util::logError("AppStore", "Failed to load stories feed: " + errorMsg);
        stateManager.stories->setState(feedErrorState);
      });
}

void AppStore::loadMyStories() {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot load my stories - network client not set");
    return;
  }

  StoriesState myStoriesStartState = stateManager.stories->getState();
  myStoriesStartState.isMyStoriesLoading = true;
  stateManager.stories->setState(myStoriesStartState);

  loadMyStoriesObservable().subscribe(
      [this](const std::vector<Story> &stories) {
        // Convert to shared_ptr for state
        std::vector<std::shared_ptr<Story>> myStoriesList;
        myStoriesList.reserve(stories.size());
        for (const auto &s : stories) {
          myStoriesList.push_back(std::make_shared<Story>(s));
        }

        StoriesState newState = stateManager.stories->getState();
        newState.myStories = std::move(myStoriesList);
        newState.isMyStoriesLoading = false;
        Util::logInfo("AppStore", "Loaded " + juce::String(newState.myStories.size()) + " of my stories");
        stateManager.stories->setState(newState);
      },
      [this](std::exception_ptr ep) {
        juce::String errorMsg;
        try {
          std::rethrow_exception(ep);
        } catch (const std::exception &e) {
          errorMsg = e.what();
        }
        Util::logError("AppStore", "Failed to load my stories: " + errorMsg);
        StoriesState errorState = stateManager.stories->getState();
        errorState.isMyStoriesLoading = false;
        stateManager.stories->setState(errorState);
      });
}

void AppStore::markStoryAsViewed(const juce::String &storyId) {
  if (!networkClient) {
    return;
  }

  Util::logInfo("AppStore", "Marking story as viewed: " + storyId);

  markStoryAsViewedObservable(storyId).subscribe(
      [storyId](int) { Util::logInfo("AppStore", "Story marked as viewed: " + storyId); },
      [storyId](std::exception_ptr ep) {
        juce::String errorMsg;
        try {
          std::rethrow_exception(ep);
        } catch (const std::exception &e) {
          errorMsg = e.what();
        }
        Util::logError("AppStore", "Failed to mark story " + storyId + " as viewed: " + errorMsg);
      });
}

void AppStore::deleteStory(const juce::String &storyId) {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot delete story - network client not set");
    return;
  }

  Util::logInfo("AppStore", "Deleting story: " + storyId);

  // deleteStoryObservable handles the state update
  deleteStoryObservable(storyId).subscribe(
      [storyId](int) { Util::logInfo("AppStore", "Story deleted successfully: " + storyId); },
      [this, storyId](std::exception_ptr ep) {
        juce::String errorMsg;
        try {
          std::rethrow_exception(ep);
        } catch (const std::exception &e) {
          errorMsg = e.what();
        }
        StoriesState deleteErrorState = stateManager.stories->getState();
        deleteErrorState.storiesError = errorMsg;
        Util::logError("AppStore", "Failed to delete story " + storyId + ": " + errorMsg);
        stateManager.stories->setState(deleteErrorState);
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

      StoriesState successState = stateManager.stories->getState();
      successState.storiesError = "";
      stateManager.stories->setState(successState);
    } else {
      StoriesState highlightErrorState = stateManager.stories->getState();
      highlightErrorState.storiesError = result.getError();
      Util::logError("AppStore", "Failed to create highlight: " + result.getError());
      stateManager.stories->setState(highlightErrorState);
    }
  });
}

// ==============================================================================
// Reactive Stories Observables (Phase 6)
// ==============================================================================
//
// These methods return rxcpp::observable with proper model types (Story values,
// not shared_ptr). They use the same network calls as the Redux actions above
// but wrap them in reactive streams for compose-able operations.

rxcpp::observable<std::vector<Story>> AppStore::loadStoriesFeedObservable() {
  using ResultType = std::vector<Story>;
  return rxcpp::sources::create<ResultType>([this](auto observer) {
           if (!networkClient) {
             Util::logError("AppStore", "Network client not initialized");
             observer.on_error(std::make_exception_ptr(std::runtime_error("Network client not initialized")));
             return;
           }

           Util::logDebug("AppStore", "Loading stories feed via observable");

           networkClient->getStoriesFeed([observer](Outcome<juce::var> result) {
             if (result.isOk()) {
               const auto data = result.getValue();
               ResultType stories;

               if (data.isArray()) {
                 for (int i = 0; i < data.size(); ++i) {
                   try {
                     auto jsonStr = juce::JSON::toString(data[i]);
                     auto jsonObj = nlohmann::json::parse(jsonStr.toStdString());
                     Story story;
                     from_json(jsonObj, story);
                     if (story.isValid()) {
                       stories.push_back(std::move(story));
                     }
                   } catch (const std::exception &e) {
                     Util::logWarning("AppStore", "Failed to parse story: " + juce::String(e.what()));
                   }
                 }
               }

               Util::logInfo("AppStore",
                             "Loaded " + juce::String(static_cast<int>(stories.size())) + " stories from feed");
               observer.on_next(std::move(stories));
               observer.on_completed();
             } else {
               Util::logError("AppStore", "Failed to load stories feed: " + result.getError());
               observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
             }
           });
         })
      .observe_on(Rx::observe_on_juce_thread());
}

rxcpp::observable<std::vector<Story>> AppStore::loadMyStoriesObservable() {
  using ResultType = std::vector<Story>;
  return rxcpp::sources::create<ResultType>([this](auto observer) {
           if (!networkClient) {
             Util::logError("AppStore", "Network client not initialized");
             observer.on_error(std::make_exception_ptr(std::runtime_error("Network client not initialized")));
             return;
           }

           Util::logDebug("AppStore", "Loading my stories via observable");

           // Get current user ID to filter only user's own stories
           const auto currentAuthState = stateManager.auth->getState();
           const juce::String currentUserId = currentAuthState.userId;

           networkClient->getStoriesFeed([observer, currentUserId](Outcome<juce::var> result) {
             if (result.isOk()) {
               const auto data = result.getValue();
               ResultType stories;

               if (data.isArray()) {
                 for (int i = 0; i < data.size(); ++i) {
                   try {
                     auto jsonStr = juce::JSON::toString(data[i]);
                     auto jsonObj = nlohmann::json::parse(jsonStr.toStdString());
                     Story story;
                     from_json(jsonObj, story);
                     // Only include stories that belong to the current user
                     if (story.isValid() && story.userId == currentUserId) {
                       stories.push_back(std::move(story));
                     }
                   } catch (const std::exception &e) {
                     Util::logWarning("AppStore", "Failed to parse story: " + juce::String(e.what()));
                   }
                 }
               }

               Util::logInfo("AppStore", "Loaded " + juce::String(static_cast<int>(stories.size())) + " of my stories");
               observer.on_next(std::move(stories));
               observer.on_completed();
             } else {
               Util::logError("AppStore", "Failed to load my stories: " + result.getError());
               observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
             }
           });
         })
      .observe_on(Rx::observe_on_juce_thread());
}

rxcpp::observable<int> AppStore::markStoryAsViewedObservable(const juce::String &storyId) {
  return rxcpp::sources::create<int>([this, storyId](auto observer) {
           if (!networkClient) {
             Util::logError("AppStore", "Network client not initialized");
             observer.on_error(std::make_exception_ptr(std::runtime_error("Network client not initialized")));
             return;
           }

           Util::logDebug("AppStore", "Marking story as viewed via observable: " + storyId);

           networkClient->viewStory(storyId, [observer, storyId](Outcome<juce::var> result) {
             if (result.isOk()) {
               Util::logInfo("AppStore", "Story marked as viewed: " + storyId);
               observer.on_next(0);
               observer.on_completed();
             } else {
               Util::logError("AppStore", "Failed to mark story as viewed: " + result.getError());
               observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
             }
           });
         })
      .observe_on(Rx::observe_on_juce_thread());
}

rxcpp::observable<int> AppStore::deleteStoryObservable(const juce::String &storyId) {
  return rxcpp::sources::create<int>([this, storyId](auto observer) {
           if (!networkClient) {
             Util::logError("AppStore", "Network client not initialized");
             observer.on_error(std::make_exception_ptr(std::runtime_error("Network client not initialized")));
             return;
           }

           Util::logDebug("AppStore", "Deleting story via observable: " + storyId);

           networkClient->deleteStory(storyId, [this, observer, storyId](Outcome<juce::var> result) {
             if (result.isOk()) {
               // Update state
               StoriesState deleteState = stateManager.stories->getState();
               for (int i = static_cast<int>(deleteState.myStories.size()) - 1; i >= 0; --i) {
                 auto story = deleteState.myStories[static_cast<size_t>(i)];
                 if (story && story->id == storyId) {
                   deleteState.myStories.erase(deleteState.myStories.begin() + i);
                   break;
                 }
               }
               stateManager.stories->setState(deleteState);

               Util::logInfo("AppStore", "Story deleted: " + storyId);
               observer.on_next(0);
               observer.on_completed();
             } else {
               Util::logError("AppStore", "Failed to delete story: " + result.getError());
               observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
             }
           });
         })
      .observe_on(Rx::observe_on_juce_thread());
}

} // namespace Stores
} // namespace Sidechain
