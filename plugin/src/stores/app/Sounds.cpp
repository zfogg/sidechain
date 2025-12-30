#include "../AppStore.h"
#include "../util/StoreUtils.h"
#include "../../util/logging/Logger.h"
#include "../../util/rx/JuceScheduler.h"
#include <nlohmann/json.hpp>

namespace Sidechain {
namespace Stores {

using Utils::NetworkClientGuard;
using Utils::NlohmannJsonArrayParser;

void AppStore::loadFeaturedSounds() {
  if (!NetworkClientGuard::check(networkClient, "load featured sounds")) {
    return;
  }

  SoundState newState = stateManager.sounds->getState();
  newState.isFeaturedLoading = true;
  stateManager.sounds->setState(newState);

  loadFeaturedSoundsObservable().subscribe(
      [this](const std::vector<Sound> &sounds) {
        // Convert to shared_ptr for state
        std::vector<std::shared_ptr<Sound>> soundsList;
        soundsList.reserve(sounds.size());
        for (const auto &s : sounds) {
          soundsList.push_back(std::make_shared<Sound>(s));
        }

        SoundState successState = stateManager.sounds->getState();
        successState.featuredSounds = std::move(soundsList);
        successState.isFeaturedLoading = false;
        successState.soundError = "";
        Util::logInfo("AppStore", "Loaded " + juce::String(successState.featuredSounds.size()) + " featured sounds");
        stateManager.sounds->setState(successState);
      },
      [this](std::exception_ptr ep) {
        juce::String errorMsg;
        try {
          std::rethrow_exception(ep);
        } catch (const std::exception &e) {
          errorMsg = e.what();
        }
        SoundState errorState = stateManager.sounds->getState();
        errorState.isFeaturedLoading = false;
        errorState.soundError = errorMsg;
        Util::logError("AppStore", "Failed to load featured sounds: " + errorMsg);
        stateManager.sounds->setState(errorState);
      });
}

void AppStore::loadRecentSounds() {
  if (!NetworkClientGuard::check(networkClient, "load recent sounds")) {
    return;
  }

  SoundState loadState = stateManager.sounds->getState();
  loadState.isLoading = true;
  stateManager.sounds->setState(loadState);

  loadRecentSoundsObservable().subscribe(
      [this](const std::vector<Sound> &sounds) {
        // Convert to shared_ptr for state
        std::vector<std::shared_ptr<Sound>> soundsList;
        soundsList.reserve(sounds.size());
        for (const auto &s : sounds) {
          soundsList.push_back(std::make_shared<Sound>(s));
        }

        SoundState recentState = stateManager.sounds->getState();
        recentState.recentSounds = std::move(soundsList);
        recentState.isLoading = false;
        recentState.soundError = "";
        recentState.recentOffset = static_cast<int>(recentState.recentSounds.size());
        Util::logInfo("AppStore", "Loaded " + juce::String(recentState.recentSounds.size()) + " recent sounds");
        stateManager.sounds->setState(recentState);
      },
      [this](std::exception_ptr ep) {
        juce::String errorMsg;
        try {
          std::rethrow_exception(ep);
        } catch (const std::exception &e) {
          errorMsg = e.what();
        }
        SoundState recentErrorState = stateManager.sounds->getState();
        recentErrorState.isLoading = false;
        recentErrorState.soundError = errorMsg;
        stateManager.sounds->setState(recentErrorState);
      });
}

void AppStore::loadMoreSounds() {
  if (!NetworkClientGuard::checkSilent(networkClient)) {
    return;
  }

  auto soundState = stateManager.sounds;
  const auto &currentState = soundState->getState();
  if (currentState.recentSounds.empty()) {
    return;
  }

  loadRecentSoundsObservable().subscribe(
      [this](const std::vector<Sound> &sounds) {
        // Convert to shared_ptr for state
        std::vector<std::shared_ptr<Sound>> newSounds;
        newSounds.reserve(sounds.size());
        for (const auto &s : sounds) {
          newSounds.push_back(std::make_shared<Sound>(s));
        }

        SoundState moreState = stateManager.sounds->getState();
        for (const auto &sound : newSounds) {
          moreState.recentSounds.push_back(sound);
        }
        moreState.recentOffset += static_cast<int>(newSounds.size());
        stateManager.sounds->setState(moreState);
      },
      [](std::exception_ptr) {
        // Silent failure for pagination
      });
}

void AppStore::refreshSounds() {
  Util::logInfo("AppStore", "Refreshing sounds");

  SoundState refreshState = stateManager.sounds->getState();
  refreshState.isRefreshing = true;
  refreshState.lastUpdated = juce::Time::getCurrentTime().toMilliseconds();
  stateManager.sounds->setState(refreshState);

  // Load both featured and recent sounds
  loadFeaturedSounds();
  loadRecentSounds();
}

// ==============================================================================
// Reactive Sound Methods (Phase 7)

rxcpp::observable<std::vector<Sound>> AppStore::loadFeaturedSoundsObservable() {
  return rxcpp::sources::create<std::vector<Sound>>([this](auto observer) {
           if (!networkClient) {
             Util::logError("AppStore", "Cannot load featured sounds - network client not set");
             observer.on_error(std::make_exception_ptr(std::runtime_error("Network client not set")));
             return;
           }

           Util::logInfo("AppStore", "Loading featured sounds observable");

           networkClient->getTrendingSounds(20, [observer](Outcome<nlohmann::json> result) {
             if (result.isOk()) {
               const auto data = result.getValue();

               // Parse using NlohmannJsonArrayParser and convert to value types
               auto sharedSounds = NlohmannJsonArrayParser<Sound>::parse(data, "featured sounds");
               std::vector<Sound> sounds;
               sounds.reserve(sharedSounds.size());
               for (const auto &ptr : sharedSounds) {
                 if (ptr) {
                   sounds.push_back(*ptr);
                 }
               }

               Util::logInfo("AppStore", "Loaded " + juce::String(sounds.size()) + " featured sounds via observable");
               observer.on_next(sounds);
               observer.on_completed();
             } else {
               Util::logError("AppStore", "Failed to load featured sounds: " + result.getError());
               observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
             }
           });
         })
      .observe_on(Rx::observe_on_juce_thread());
}

rxcpp::observable<std::vector<Sound>> AppStore::loadRecentSoundsObservable() {
  return rxcpp::sources::create<std::vector<Sound>>([this](auto observer) {
           if (!networkClient) {
             Util::logError("AppStore", "Cannot load recent sounds - network client not set");
             observer.on_error(std::make_exception_ptr(std::runtime_error("Network client not set")));
             return;
           }

           Util::logInfo("AppStore", "Loading recent sounds observable");

           // Use searchSounds with empty query to get recent sounds
           networkClient->searchSounds("", 20, [observer](Outcome<nlohmann::json> result) {
             if (result.isOk()) {
               const auto data = result.getValue();

               // Parse using NlohmannJsonArrayParser and convert to value types
               auto sharedSounds = NlohmannJsonArrayParser<Sound>::parse(data, "recent sounds");
               std::vector<Sound> sounds;
               sounds.reserve(sharedSounds.size());
               for (const auto &ptr : sharedSounds) {
                 if (ptr) {
                   sounds.push_back(*ptr);
                 }
               }

               Util::logInfo("AppStore", "Loaded " + juce::String(sounds.size()) + " recent sounds via observable");
               observer.on_next(sounds);
               observer.on_completed();
             } else {
               Util::logError("AppStore", "Failed to load recent sounds: " + result.getError());
               observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
             }
           });
         })
      .observe_on(Rx::observe_on_juce_thread());
}

rxcpp::observable<Sound> AppStore::getSoundObservable(const juce::String &soundId) {
  return rxcpp::sources::create<Sound>([this, soundId](auto observer) {
           if (!networkClient) {
             Util::logError("AppStore", "Cannot get sound - network client not set");
             observer.on_error(std::make_exception_ptr(std::runtime_error("Network client not set")));
             return;
           }

           Util::logInfo("AppStore", "Getting sound via observable: " + soundId);

           networkClient->getSound(soundId, [observer](Outcome<nlohmann::json> result) {
             if (result.isOk()) {
               try {
                 const auto &soundJson = result.getValue();
                 auto soundResult = Sound::createFromJson(soundJson);
                 if (soundResult.isOk() && soundResult.getValue()) {
                   Util::logInfo("AppStore", "Got sound via observable");
                   observer.on_next(*soundResult.getValue());
                   observer.on_completed();
                 } else {
                   observer.on_error(std::make_exception_ptr(std::runtime_error("Failed to parse sound")));
                 }
               } catch (const std::exception &e) {
                 observer.on_error(std::make_exception_ptr(std::runtime_error(e.what())));
               }
             } else {
               Util::logError("AppStore", "Failed to get sound: " + result.getError());
               observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
             }
           });
         })
      .observe_on(Rx::observe_on_juce_thread());
}

rxcpp::observable<Sound> AppStore::getSoundForPostObservable(const juce::String &postId) {
  return rxcpp::sources::create<Sound>([this, postId](auto observer) {
           if (!networkClient) {
             Util::logError("AppStore", "Cannot get sound for post - network client not set");
             observer.on_error(std::make_exception_ptr(std::runtime_error("Network client not set")));
             return;
           }

           Util::logInfo("AppStore", "Getting sound for post via observable: " + postId);

           networkClient->getSoundForPost(postId, [observer](Outcome<nlohmann::json> result) {
             if (result.isOk()) {
               try {
                 const auto &soundJson = result.getValue();
                 auto soundResult = Sound::createFromJson(soundJson);
                 if (soundResult.isOk() && soundResult.getValue()) {
                   Util::logInfo("AppStore", "Got sound for post via observable");
                   observer.on_next(*soundResult.getValue());
                   observer.on_completed();
                 } else {
                   observer.on_error(std::make_exception_ptr(std::runtime_error("Failed to parse sound")));
                 }
               } catch (const std::exception &e) {
                 observer.on_error(std::make_exception_ptr(std::runtime_error(e.what())));
               }
             } else {
               Util::logError("AppStore", "Failed to get sound for post: " + result.getError());
               observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
             }
           });
         })
      .observe_on(Rx::observe_on_juce_thread());
}

rxcpp::observable<std::vector<SoundPost>> AppStore::getSoundPostsObservable(const juce::String &soundId, int limit,
                                                                            int offset) {
  return rxcpp::sources::create<std::vector<SoundPost>>([this, soundId, limit, offset](auto observer) {
           if (!networkClient) {
             Util::logError("AppStore", "Cannot get sound posts - network client not set");
             observer.on_error(std::make_exception_ptr(std::runtime_error("Network client not set")));
             return;
           }

           Util::logInfo("AppStore", "Getting sound posts via observable: " + soundId);

           networkClient->getSoundPosts(soundId, limit, offset, [observer](Outcome<nlohmann::json> result) {
             if (result.isOk()) {
               const auto &response = result.getValue();

               std::vector<SoundPost> posts;
               if (response.contains("posts") && response["posts"].is_array()) {
                 for (const auto &postJson : response["posts"]) {
                   try {
                     auto postResult = SoundPost::createFromJson(postJson);
                     if (postResult.isOk() && postResult.getValue()) {
                       posts.push_back(*postResult.getValue());
                     }
                   } catch (...) {
                     // Skip invalid posts
                   }
                 }
               }

               Util::logInfo("AppStore", "Got " + juce::String(posts.size()) + " sound posts via observable");
               observer.on_next(posts);
               observer.on_completed();
             } else {
               Util::logError("AppStore", "Failed to get sound posts: " + result.getError());
               observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
             }
           });
         })
      .observe_on(Rx::observe_on_juce_thread());
}

} // namespace Stores
} // namespace Sidechain
