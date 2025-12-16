#include "../AppStore.h"
#include "../../util/logging/Logger.h"

namespace Sidechain {
namespace Stores {

void AppStore::loadPlaylists() {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot load playlists - network client not set");
    return;
  }

  updateState([](AppState &state) { state.playlists.isLoading = true; });

  networkClient->getPlaylists("all", [this](Outcome<juce::var> result) {
    if (result.isOk()) {
      const auto data = result.getValue();
      juce::Array<juce::var> playlistsList;

      if (data.isArray()) {
        for (int i = 0; i < data.size(); ++i) {
          playlistsList.add(data[i]);
        }
      }

      updateState([playlistsList](AppState &state) {
        state.playlists.playlists = playlistsList;
        state.playlists.isLoading = false;
        state.playlists.playlistError = "";
        Util::logInfo("AppStore", "Loaded " + juce::String(playlistsList.size()) + " playlists");
      });
    } else {
      updateState([result](AppState &state) {
        state.playlists.isLoading = false;
        state.playlists.playlistError = result.getError();
        Util::logError("AppStore", "Failed to load playlists: " + result.getError());
      });
    }

    notifyObservers();
  });
}

void AppStore::createPlaylist(const juce::String &name, const juce::String &description) {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot create playlist - network client not set");
    return;
  }

  Util::logInfo("AppStore", "Creating playlist: " + name);

  // createPlaylist signature: name, description="", isCollaborative=false, isPublic=true, callback
  networkClient->createPlaylist(name, description, false, true, [this](Outcome<juce::var> result) {
    if (result.isOk()) {
      Util::logInfo("AppStore", "Playlist created successfully");
      // Reload playlists to get the new one
      loadPlaylists();
    } else {
      updateState([result](AppState &state) {
        state.playlists.playlistError = result.getError();
        Util::logError("AppStore", "Failed to create playlist: " + result.getError());
      });
      notifyObservers();
    }
  });
}

void AppStore::deletePlaylist(const juce::String &playlistId) {
  Util::logInfo("AppStore", "Deleting playlist: " + playlistId);
  // TODO: Implement playlist deletion via NetworkClient
}

void AppStore::addPostToPlaylist(const juce::String &postId, const juce::String &playlistId) {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot add post to playlist - network client not set");
    return;
  }

  Util::logInfo("AppStore", "Adding post " + postId + " to playlist " + playlistId);

  networkClient->addPlaylistEntry(playlistId, postId, -1, [this](Outcome<juce::var> result) {
    if (result.isOk()) {
      Util::logInfo("AppStore", "Post added to playlist successfully");
      // Could reload playlists here if needed
    } else {
      updateState([result](AppState &state) {
        state.playlists.playlistError = result.getError();
        Util::logError("AppStore", "Failed to add post to playlist: " + result.getError());
      });
      notifyObservers();
    }
  });
}

} // namespace Stores
} // namespace Sidechain
