#include "../AppStore.h"
#include "../util/StoreUtils.h"
#include "../../util/logging/Logger.h"
#include "../../util/rx/JuceScheduler.h"
#include <nlohmann/json.hpp>

namespace Sidechain {
namespace Stores {

using Utils::JsonArrayParser;
using Utils::NetworkClientGuard;

void AppStore::loadFeaturedSounds() {
  if (!NetworkClientGuard::check(networkClient, "load featured sounds")) {
    return;
  }

  SoundState newState = stateManager.sounds->getState();
  newState.isFeaturedLoading = true;
  stateManager.sounds->setState(newState);

  networkClient->getTrendingSounds(20, [this](Outcome<juce::var> result) {
    if (result.isOk()) {
      const auto data = result.getValue();

      // Use JsonArrayParser
      auto soundsList = JsonArrayParser<Sound>::parse(data, "featured sounds");

      SoundState successState = stateManager.sounds->getState();
      successState.featuredSounds = std::move(soundsList);
      successState.isFeaturedLoading = false;
      successState.soundError = "";
      Util::logInfo("AppStore", "Loaded " + juce::String(successState.featuredSounds.size()) + " featured sounds");
      stateManager.sounds->setState(successState);
    } else {
      SoundState errorState = stateManager.sounds->getState();
      errorState.isFeaturedLoading = false;
      errorState.soundError = result.getError();
      Util::logError("AppStore", "Failed to load featured sounds: " + result.getError());
      stateManager.sounds->setState(errorState);
    }
  });
}

void AppStore::loadRecentSounds() {
  if (!NetworkClientGuard::check(networkClient, "load recent sounds")) {
    return;
  }

  SoundState loadState = stateManager.sounds->getState();
  loadState.isLoading = true;
  stateManager.sounds->setState(loadState);

  // Use searchSounds with empty query to get recent sounds
  networkClient->searchSounds("", 20, [this](Outcome<juce::var> result) {
    if (result.isOk()) {
      const auto data = result.getValue();

      // Use JsonArrayParser
      auto soundsList = JsonArrayParser<Sound>::parse(data, "recent sounds");

      SoundState recentState = stateManager.sounds->getState();
      recentState.recentSounds = std::move(soundsList);
      recentState.isLoading = false;
      recentState.soundError = "";
      recentState.recentOffset = static_cast<int>(recentState.recentSounds.size());
      Util::logInfo("AppStore", "Loaded " + juce::String(recentState.recentSounds.size()) + " recent sounds");
      stateManager.sounds->setState(recentState);
    } else {
      SoundState recentErrorState = stateManager.sounds->getState();
      recentErrorState.isLoading = false;
      recentErrorState.soundError = result.getError();
      stateManager.sounds->setState(recentErrorState);
    }
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

  networkClient->searchSounds("", 20, [this](Outcome<juce::var> result) {
    if (result.isOk()) {
      const auto data = result.getValue();

      // Use JsonArrayParser
      auto newSounds = JsonArrayParser<Sound>::parse(data, "more sounds");

      SoundState moreState = stateManager.sounds->getState();
      for (const auto &sound : newSounds) {
        moreState.recentSounds.push_back(sound);
      }
      moreState.recentOffset += static_cast<int>(newSounds.size());
      stateManager.sounds->setState(moreState);
    }
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

           networkClient->getTrendingSounds(20, [observer](Outcome<juce::var> result) {
             if (result.isOk()) {
               const auto data = result.getValue();

               // Parse using JsonArrayParser and convert to value types
               auto sharedSounds = JsonArrayParser<Sound>::parse(data, "featured sounds");
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
           networkClient->searchSounds("", 20, [observer](Outcome<juce::var> result) {
             if (result.isOk()) {
               const auto data = result.getValue();

               // Parse using JsonArrayParser and convert to value types
               auto sharedSounds = JsonArrayParser<Sound>::parse(data, "recent sounds");
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

} // namespace Stores
} // namespace Sidechain
