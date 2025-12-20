#include "../AppStore.h"
#include "../../util/logging/Logger.h"
#include <nlohmann/json.hpp>

namespace Sidechain {
namespace Stores {

rxcpp::observable<juce::Array<juce::var>> AppStore::getPlaylistsObservable() {
  return rxcpp::sources::create<juce::Array<juce::var>>([this](auto observer) {
    if (!networkClient) {
      Util::logError("AppStore", "Cannot get playlists - network client not set");
      observer.on_error(std::make_exception_ptr(std::runtime_error("Network client not set")));
      return;
    }

    Util::logInfo("AppStore", "Fetching playlists");

    networkClient->getPlaylists("all", [observer](Outcome<juce::var> result) {
      if (result.isOk()) {
        const auto data = result.getValue();
        juce::Array<juce::var> playlistsList;

        if (data.isArray()) {
          for (int i = 0; i < data.size(); ++i) {
            playlistsList.add(data[i]);
          }
        } else if (data.hasProperty("playlists")) {
          auto playlistsArray = data["playlists"];
          if (playlistsArray.isArray()) {
            for (int i = 0; i < playlistsArray.size(); ++i) {
              playlistsList.add(playlistsArray[i]);
            }
          }
        }

        Util::logInfo("AppStore", "Loaded " + juce::String(playlistsList.size()) + " playlists");
        observer.on_next(playlistsList);
        observer.on_completed();
      } else {
        Util::logError("AppStore", "Failed to get playlists: " + result.getError());
        observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
      }
    });
  });
}

void AppStore::loadPlaylists() {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot load playlists - network client not set");
    return;
  }

  sliceManager.getPlaylistSlice()->dispatch([](PlaylistState &state) { state.isLoading = true; });

  networkClient->getPlaylists("all", [this](Outcome<juce::var> result) {
    if (result.isOk()) {
      const auto data = result.getValue();

      // Parse juce::var response into shared_ptr Playlist objects
      juce::String jsonString = juce::JSON::toString(data, false);
      try {
        auto jsonArray = nlohmann::json::parse(jsonString.toStdString());
        auto parseResult = Playlist::createFromJsonArray(jsonArray);

        if (parseResult.isOk()) {
          auto playlistsList = parseResult.getValue();
          sliceManager.getPlaylistSlice()->dispatch([playlistsList](PlaylistState &state) {
            state.playlists = playlistsList;
            state.isLoading = false;
            state.playlistError = "";
            Util::logInfo("AppStore", "Loaded " + juce::String(playlistsList.size()) + " playlists");
          });
        } else {
          sliceManager.getPlaylistSlice()->dispatch([parseResult](PlaylistState &state) {
            state.isLoading = false;
            state.playlistError = parseResult.getError();
            Util::logError("AppStore", "Failed to parse playlists: " + parseResult.getError());
          });
        }
      } catch (const std::exception &e) {
        sliceManager.getPlaylistSlice()->dispatch([e](PlaylistState &state) {
          state.isLoading = false;
          state.playlistError = juce::String(e.what());
          Util::logError("AppStore", "JSON parse error: " + juce::String(e.what()));
        });
      }
    } else {
      sliceManager.getPlaylistSlice()->dispatch([result](PlaylistState &state) {
        state.isLoading = false;
        state.playlistError = result.getError();
        Util::logError("AppStore", "Failed to load playlists: " + result.getError());
      });
    }
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
      sliceManager.getPlaylistSlice()->dispatch([result](PlaylistState &state) {
        state.playlistError = result.getError();
        Util::logError("AppStore", "Failed to create playlist: " + result.getError());
      });
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
      sliceManager.getPlaylistSlice()->dispatch([result](PlaylistState &state) {
        state.playlistError = result.getError();
        Util::logError("AppStore", "Failed to add post to playlist: " + result.getError());
      });
    }
  });
}

} // namespace Stores
} // namespace Sidechain
