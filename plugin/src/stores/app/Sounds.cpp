#include "../AppStore.h"
#include "../../util/logging/Logger.h"

namespace Sidechain {
namespace Stores {

void AppStore::loadFeaturedSounds() {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot load featured sounds - network client not set");
    return;
  }

  sliceManager.getSoundSlice()->dispatch([](SoundState &state) { state.isFeaturedLoading = true; });

  networkClient->getTrendingSounds(20, [this](Outcome<juce::var> result) {
    if (result.isOk()) {
      const auto data = result.getValue();
      juce::Array<juce::var> soundsList;

      if (data.isArray()) {
        for (int i = 0; i < data.size(); ++i) {
          soundsList.add(data[i]);
        }
      }

      sliceManager.getSoundSlice()->dispatch([soundsList](SoundState &state) {
        state.featuredSounds = soundsList;
        state.isFeaturedLoading = false;
        state.soundError = "";
        Util::logInfo("AppStore", "Loaded " + juce::String(soundsList.size()) + " featured sounds");
      });
    } else {
      sliceManager.getSoundSlice()->dispatch([result](SoundState &state) {
        state.isFeaturedLoading = false;
        state.soundError = result.getError();
        Util::logError("AppStore", "Failed to load featured sounds: " + result.getError());
      });
    }
  });
}

void AppStore::loadRecentSounds() {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot load recent sounds - network client not set");
    return;
  }

  sliceManager.getSoundSlice()->dispatch([](SoundState &state) { state.isLoading = true; });

  // Use searchSounds with empty query to get recent sounds
  networkClient->searchSounds("", 20, [this](Outcome<juce::var> result) {
    if (result.isOk()) {
      const auto data = result.getValue();
      juce::Array<juce::var> soundsList;

      if (data.isArray()) {
        for (int i = 0; i < data.size(); ++i) {
          soundsList.add(data[i]);
        }
      }

      sliceManager.getSoundSlice()->dispatch([soundsList](SoundState &state) {
        state.recentSounds = soundsList;
        state.isLoading = false;
        state.soundError = "";
        state.recentOffset = soundsList.size();
        Util::logInfo("AppStore", "Loaded " + juce::String(soundsList.size()) + " recent sounds");
      });
    } else {
      sliceManager.getSoundSlice()->dispatch([result](SoundState &state) {
        state.isLoading = false;
        state.soundError = result.getError();
      });
    }
  });
}

void AppStore::loadMoreSounds() {
  if (!networkClient) {
    return;
  }

  auto soundSlice = sliceManager.getSoundSlice();
  const auto &currentState = soundSlice->getState();
  if (currentState.recentSounds.isEmpty()) {
    return;
  }

  networkClient->searchSounds("", 20, [this](Outcome<juce::var> result) {
    if (result.isOk()) {
      const auto data = result.getValue();
      juce::Array<juce::var> newSounds;

      if (data.isArray()) {
        for (int i = 0; i < data.size(); ++i) {
          newSounds.add(data[i]);
        }
      }

      sliceManager.getSoundSlice()->dispatch([newSounds](SoundState &state) {
        for (const auto &sound : newSounds) {
          state.recentSounds.add(sound);
        }
        state.recentOffset += newSounds.size();
      });
    }
  });
}

void AppStore::refreshSounds() {
  Util::logInfo("AppStore", "Refreshing sounds");

  sliceManager.getSoundSlice()->dispatch([](SoundState &state) {
    state.isRefreshing = true;
    state.lastUpdated = juce::Time::getCurrentTime().toMilliseconds();
  });

  // Load both featured and recent sounds
  loadFeaturedSounds();
  loadRecentSounds();
}

} // namespace Stores
} // namespace Sidechain
