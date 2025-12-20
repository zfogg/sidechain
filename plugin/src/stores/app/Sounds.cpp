#include "../AppStore.h"
#include "../../util/logging/Logger.h"
#include <nlohmann/json.hpp>

namespace Sidechain {
namespace Stores {

void AppStore::loadFeaturedSounds() {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot load featured sounds - network client not set");
    return;
  }

  SoundState newState = sliceManager.getSoundSlice()->getState();
  newState.isFeaturedLoading = true;
  sliceManager.getSoundSlice()->setState(newState);

  networkClient->getTrendingSounds(20, [this](Outcome<juce::var> result) {
    if (result.isOk()) {
      const auto data = result.getValue();
      std::vector<std::shared_ptr<Sidechain::Sound>> soundsList;

      if (data.isArray()) {
        for (int i = 0; i < data.size(); ++i) {
          try {
            auto jsonStr = juce::JSON::toString(data[i]);
            auto jsonObj = nlohmann::json::parse(jsonStr.toStdString());
            auto soundResult = Sidechain::Sound::createFromJson(jsonObj);
            if (soundResult.isOk()) {
              soundsList.push_back(soundResult.getValue());
            }
          } catch (...) {
            // Skip invalid items
          }
        }
      }

      SoundState successState = sliceManager.getSoundSlice()->getState();
      successState.featuredSounds = soundsList;
      successState.isFeaturedLoading = false;
      successState.soundError = "";
      Util::logInfo("AppStore", "Loaded " + juce::String(soundsList.size()) + " featured sounds");
      sliceManager.getSoundSlice()->setState(successState);
    } else {
      SoundState errorState = sliceManager.getSoundSlice()->getState();
      errorState.isFeaturedLoading = false;
      errorState.soundError = result.getError();
      Util::logError("AppStore", "Failed to load featured sounds: " + result.getError());
      sliceManager.getSoundSlice()->setState(errorState);
    }
  });
}

void AppStore::loadRecentSounds() {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot load recent sounds - network client not set");
    return;
  }

  SoundState loadState = sliceManager.getSoundSlice()->getState();
  loadState.isLoading = true;
  sliceManager.getSoundSlice()->setState(loadState);

  // Use searchSounds with empty query to get recent sounds
  networkClient->searchSounds("", 20, [this](Outcome<juce::var> result) {
    if (result.isOk()) {
      const auto data = result.getValue();
      std::vector<std::shared_ptr<Sidechain::Sound>> soundsList;

      if (data.isArray()) {
        for (int i = 0; i < data.size(); ++i) {
          try {
            auto jsonStr = juce::JSON::toString(data[i]);
            auto jsonObj = nlohmann::json::parse(jsonStr.toStdString());
            auto soundResult = Sidechain::Sound::createFromJson(jsonObj);
            if (soundResult.isOk()) {
              soundsList.push_back(soundResult.getValue());
            }
          } catch (...) {
            // Skip invalid items
          }
        }
      }

      SoundState recentState = sliceManager.getSoundSlice()->getState();
      recentState.recentSounds = soundsList;
      recentState.isLoading = false;
      recentState.soundError = "";
      recentState.recentOffset = static_cast<int>(soundsList.size());
      Util::logInfo("AppStore", "Loaded " + juce::String(soundsList.size()) + " recent sounds");
      sliceManager.getSoundSlice()->setState(recentState);
    } else {
      SoundState recentErrorState = sliceManager.getSoundSlice()->getState();
      recentErrorState.isLoading = false;
      recentErrorState.soundError = result.getError();
      sliceManager.getSoundSlice()->setState(recentErrorState);
    }
  });
}

void AppStore::loadMoreSounds() {
  if (!networkClient) {
    return;
  }

  auto soundSlice = sliceManager.getSoundSlice();
  const auto &currentState = soundSlice->getState();
  if (currentState.recentSounds.empty()) {
    return;
  }

  networkClient->searchSounds("", 20, [this](Outcome<juce::var> result) {
    if (result.isOk()) {
      const auto data = result.getValue();
      std::vector<std::shared_ptr<Sidechain::Sound>> newSounds;

      if (data.isArray()) {
        for (int i = 0; i < data.size(); ++i) {
          try {
            auto jsonStr = juce::JSON::toString(data[i]);
            auto jsonObj = nlohmann::json::parse(jsonStr.toStdString());
            auto soundResult = Sidechain::Sound::createFromJson(jsonObj);
            if (soundResult.isOk()) {
              newSounds.push_back(soundResult.getValue());
            }
          } catch (...) {
            // Skip invalid items
          }
        }
      }

      SoundState moreState = sliceManager.getSoundSlice()->getState();
      for (const auto &sound : newSounds) {
        moreState.recentSounds.push_back(sound);
      }
      moreState.recentOffset += newSounds.size();
      sliceManager.getSoundSlice()->setState(moreState);
    }
  });
}

void AppStore::refreshSounds() {
  Util::logInfo("AppStore", "Refreshing sounds");

  SoundState refreshState = sliceManager.getSoundSlice()->getState();
  refreshState.isRefreshing = true;
  refreshState.lastUpdated = juce::Time::getCurrentTime().toMilliseconds();
  sliceManager.getSoundSlice()->setState(refreshState);

  // Load both featured and recent sounds
  loadFeaturedSounds();
  loadRecentSounds();
}

} // namespace Stores
} // namespace Sidechain
