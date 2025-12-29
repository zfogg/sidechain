#include "../AppStore.h"
#include "../../util/logging/Logger.h"
#include "../../util/rx/JuceScheduler.h"
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

  PlaylistState newState = stateManager.playlists->getState();
  newState.isLoading = true;
  stateManager.playlists->setState(newState);

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
          PlaylistState successState = stateManager.playlists->getState();
          successState.playlists = playlistsList;
          successState.isLoading = false;
          successState.playlistError = "";
          Util::logInfo("AppStore", "Loaded " + juce::String(playlistsList.size()) + " playlists");
          stateManager.playlists->setState(successState);
        } else {
          PlaylistState parseErrorState = stateManager.playlists->getState();
          parseErrorState.isLoading = false;
          parseErrorState.playlistError = parseResult.getError();
          Util::logError("AppStore", "Failed to parse playlists: " + parseResult.getError());
          stateManager.playlists->setState(parseErrorState);
        }
      } catch (const std::exception &e) {
        PlaylistState exceptionState = stateManager.playlists->getState();
        exceptionState.isLoading = false;
        exceptionState.playlistError = juce::String(e.what());
        Util::logError("AppStore", "JSON parse error: " + juce::String(e.what()));
        stateManager.playlists->setState(exceptionState);
      }
    } else {
      PlaylistState networkErrorState = stateManager.playlists->getState();
      networkErrorState.isLoading = false;
      networkErrorState.playlistError = result.getError();
      Util::logError("AppStore", "Failed to load playlists: " + result.getError());
      stateManager.playlists->setState(networkErrorState);
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
      PlaylistState createErrorState = stateManager.playlists->getState();
      createErrorState.playlistError = result.getError();
      Util::logError("AppStore", "Failed to create playlist: " + result.getError());
      stateManager.playlists->setState(createErrorState);
    }
  });
}

void AppStore::deletePlaylist(const juce::String &playlistId) {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot delete playlist - network client not set");
    return;
  }

  Util::logInfo("AppStore", "Deleting playlist: " + playlistId);

  // Optimistically remove from state
  PlaylistState currentState = stateManager.playlists->getState();
  auto it = std::find_if(currentState.playlists.begin(), currentState.playlists.end(),
                         [&playlistId](const auto &p) { return p->id == playlistId; });

  if (it != currentState.playlists.end()) {
    currentState.playlists.erase(it);
    stateManager.playlists->setState(currentState);
    Util::logInfo("AppStore", "Playlist removed from local state");
  }

  // Make API call to delete on server
  networkClient->deletePlaylist(playlistId, [this, playlistId](Outcome<juce::var> result) {
    if (result.isOk()) {
      Util::logInfo("AppStore", "Playlist deleted on server: " + playlistId);
    } else {
      PlaylistState errorState = stateManager.playlists->getState();
      errorState.playlistError = result.getError();
      stateManager.playlists->setState(errorState);
      Util::logError("AppStore", "Failed to delete playlist: " + result.getError());
    }
  });
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
      PlaylistState errorState = stateManager.playlists->getState();
      errorState.playlistError = result.getError();
      stateManager.playlists->setState(errorState);
      Util::logError("AppStore", "Failed to add post to playlist: " + result.getError());
    }
  });
}

// ==============================================================================
// Reactive Playlist Methods (Phase 7)

rxcpp::observable<std::vector<Playlist>> AppStore::loadPlaylistsObservable() {
  return rxcpp::sources::create<std::vector<Playlist>>([this](auto observer) {
           if (!networkClient) {
             Util::logError("AppStore", "Cannot load playlists - network client not set");
             observer.on_error(std::make_exception_ptr(std::runtime_error("Network client not set")));
             return;
           }

           Util::logInfo("AppStore", "Loading playlists observable");

           networkClient->getPlaylists("all", [observer](Outcome<juce::var> result) {
             if (result.isOk()) {
               const auto data = result.getValue();

               // Parse juce::var response into Playlist value objects
               juce::String jsonString = juce::JSON::toString(data, false);
               try {
                 auto jsonArray = nlohmann::json::parse(jsonString.toStdString());
                 auto parseResult = Playlist::createFromJsonArray(jsonArray);

                 if (parseResult.isOk()) {
                   auto sharedPlaylists = parseResult.getValue();
                   // Convert shared_ptr to value types
                   std::vector<Playlist> playlists;
                   playlists.reserve(sharedPlaylists.size());
                   for (const auto &ptr : sharedPlaylists) {
                     if (ptr) {
                       playlists.push_back(*ptr);
                     }
                   }
                   Util::logInfo("AppStore", "Loaded " + juce::String(playlists.size()) + " playlists via observable");
                   observer.on_next(playlists);
                   observer.on_completed();
                 } else {
                   Util::logError("AppStore", "Failed to parse playlists: " + parseResult.getError());
                   observer.on_error(std::make_exception_ptr(std::runtime_error(parseResult.getError().toStdString())));
                 }
               } catch (const std::exception &e) {
                 Util::logError("AppStore", "JSON parse error: " + juce::String(e.what()));
                 observer.on_error(std::make_exception_ptr(e));
               }
             } else {
               Util::logError("AppStore", "Failed to load playlists: " + result.getError());
               observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
             }
           });
         })
      .observe_on(Rx::observe_on_juce_thread());
}

rxcpp::observable<Playlist> AppStore::createPlaylistObservable(const juce::String &name,
                                                               const juce::String &description) {
  return rxcpp::sources::create<Playlist>([this, name, description](auto observer) {
           if (!networkClient) {
             Util::logError("AppStore", "Cannot create playlist - network client not set");
             observer.on_error(std::make_exception_ptr(std::runtime_error("Network client not set")));
             return;
           }

           Util::logInfo("AppStore", "Creating playlist: " + name);

           networkClient->createPlaylist(name, description, false, true, [observer, name](Outcome<juce::var> result) {
             if (result.isOk()) {
               Util::logInfo("AppStore", "Playlist created successfully: " + name);
               // Parse the created playlist from response
               Playlist playlist = Playlist::fromJSON(result.getValue());
               observer.on_next(playlist);
               observer.on_completed();
             } else {
               Util::logError("AppStore", "Failed to create playlist: " + result.getError());
               observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
             }
           });
         })
      .observe_on(Rx::observe_on_juce_thread());
}

rxcpp::observable<int> AppStore::deletePlaylistObservable(const juce::String &playlistId) {
  return rxcpp::sources::create<int>([this, playlistId](auto observer) {
           if (!networkClient) {
             Util::logError("AppStore", "Cannot delete playlist - network client not set");
             observer.on_error(std::make_exception_ptr(std::runtime_error("Network client not set")));
             return;
           }

           Util::logInfo("AppStore", "Deleting playlist: " + playlistId);

           // Optimistically remove from state
           PlaylistState currentState = stateManager.playlists->getState();
           auto it = std::find_if(currentState.playlists.begin(), currentState.playlists.end(),
                                  [&playlistId](const auto &p) { return p->id == playlistId; });

           std::shared_ptr<Playlist> removedPlaylist;
           if (it != currentState.playlists.end()) {
             removedPlaylist = *it;
             currentState.playlists.erase(it);
             stateManager.playlists->setState(currentState);
             Util::logInfo("AppStore", "Playlist removed from local state");
           }

           networkClient->deletePlaylist(
               playlistId, [this, playlistId, removedPlaylist, observer](Outcome<juce::var> result) {
                 if (result.isOk()) {
                   Util::logInfo("AppStore", "Playlist deleted on server: " + playlistId);
                   observer.on_next(0);
                   observer.on_completed();
                 } else {
                   Util::logError("AppStore", "Failed to delete playlist: " + result.getError());
                   // Rollback optimistic update
                   if (removedPlaylist) {
                     PlaylistState rollbackState = stateManager.playlists->getState();
                     rollbackState.playlists.push_back(removedPlaylist);
                     stateManager.playlists->setState(rollbackState);
                   }
                   observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
                 }
               });
         })
      .observe_on(Rx::observe_on_juce_thread());
}

rxcpp::observable<int> AppStore::addPostToPlaylistObservable(const juce::String &postId,
                                                             const juce::String &playlistId) {
  return rxcpp::sources::create<int>([this, postId, playlistId](auto observer) {
           if (!networkClient) {
             Util::logError("AppStore", "Cannot add post to playlist - network client not set");
             observer.on_error(std::make_exception_ptr(std::runtime_error("Network client not set")));
             return;
           }

           Util::logInfo("AppStore", "Adding post " + postId + " to playlist " + playlistId);

           networkClient->addPlaylistEntry(
               playlistId, postId, -1, [postId, playlistId, observer](Outcome<juce::var> result) {
                 if (result.isOk()) {
                   Util::logInfo("AppStore", "Post " + postId + " added to playlist " + playlistId + " successfully");
                   observer.on_next(0);
                   observer.on_completed();
                 } else {
                   Util::logError("AppStore", "Failed to add post to playlist: " + result.getError());
                   observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
                 }
               });
         })
      .observe_on(Rx::observe_on_juce_thread());
}

} // namespace Stores
} // namespace Sidechain
