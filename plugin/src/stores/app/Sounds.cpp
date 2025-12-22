#include "../AppStore.h"
#include "../util/StoreUtils.h"
#include "../../util/logging/Logger.h"
#include <nlohmann/json.hpp>

namespace Sidechain {
namespace Stores {

using Utils::JsonArrayParser;
using Utils::NetworkClientGuard;

void AppStore::loadFeaturedSounds() {
  if (!NetworkClientGuard::check(networkClient.get(), "load featured sounds")) {
    return;
  }

  SoundState newState = sliceManager.sounds->getState();
  newState.isFeaturedLoading = true;
  sliceManager.sounds->setState(newState);

  networkClient->getTrendingSounds(20, [this](Outcome<juce::var> result) {
    if (result.isOk()) {
      const auto data = result.getValue();

      // Use JsonArrayParser
      auto soundsList = JsonArrayParser<Sound>::parse(data, "featured sounds");

      SoundState successState = sliceManager.sounds->getState();
      successState.featuredSounds = std::move(soundsList);
      successState.isFeaturedLoading = false;
      successState.soundError = "";
      Util::logInfo("AppStore", "Loaded " + juce::String(successState.featuredSounds.size()) + " featured sounds");
      sliceManager.sounds->setState(successState);
    } else {
      SoundState errorState = sliceManager.sounds->getState();
      errorState.isFeaturedLoading = false;
      errorState.soundError = result.getError();
      Util::logError("AppStore", "Failed to load featured sounds: " + result.getError());
      sliceManager.sounds->setState(errorState);
    }
  });
}

void AppStore::loadRecentSounds() {
  if (!NetworkClientGuard::check(networkClient.get(), "load recent sounds")) {
    return;
  }

  SoundState loadState = sliceManager.sounds->getState();
  loadState.isLoading = true;
  sliceManager.sounds->setState(loadState);

  // Use searchSounds with empty query to get recent sounds
  networkClient->searchSounds("", 20, [this](Outcome<juce::var> result) {
    if (result.isOk()) {
      const auto data = result.getValue();

      // Use JsonArrayParser
      auto soundsList = JsonArrayParser<Sound>::parse(data, "recent sounds");

      SoundState recentState = sliceManager.sounds->getState();
      recentState.recentSounds = std::move(soundsList);
      recentState.isLoading = false;
      recentState.soundError = "";
      recentState.recentOffset = static_cast<int>(recentState.recentSounds.size());
      Util::logInfo("AppStore", "Loaded " + juce::String(recentState.recentSounds.size()) + " recent sounds");
      sliceManager.sounds->setState(recentState);
    } else {
      SoundState recentErrorState = sliceManager.sounds->getState();
      recentErrorState.isLoading = false;
      recentErrorState.soundError = result.getError();
      sliceManager.sounds->setState(recentErrorState);
    }
  });
}

void AppStore::loadMoreSounds() {
  if (!NetworkClientGuard::checkSilent(networkClient.get())) {
    return;
  }

  auto soundSlice = sliceManager.sounds;
  const auto &currentState = soundSlice->getState();
  if (currentState.recentSounds.empty()) {
    return;
  }

  networkClient->searchSounds("", 20, [this](Outcome<juce::var> result) {
    if (result.isOk()) {
      const auto data = result.getValue();

      // Use JsonArrayParser
      auto newSounds = JsonArrayParser<Sound>::parse(data, "more sounds");

      SoundState moreState = sliceManager.sounds->getState();
      for (const auto &sound : newSounds) {
        moreState.recentSounds.push_back(sound);
      }
      moreState.recentOffset += static_cast<int>(newSounds.size());
      sliceManager.sounds->setState(moreState);
    }
  });
}

void AppStore::refreshSounds() {
  Util::logInfo("AppStore", "Refreshing sounds");

  SoundState refreshState = sliceManager.sounds->getState();
  refreshState.isRefreshing = true;
  refreshState.lastUpdated = juce::Time::getCurrentTime().toMilliseconds();
  sliceManager.sounds->setState(refreshState);

  // Load both featured and recent sounds
  loadFeaturedSounds();
  loadRecentSounds();
}

} // namespace Stores
} // namespace Sidechain
