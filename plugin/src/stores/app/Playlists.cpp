#include "../AppStore.h"
#include "../../util/logging/Logger.h"
#include "../../util/rx/JuceScheduler.h"
#include <nlohmann/json.hpp>

namespace Sidechain {
namespace Stores {

rxcpp::observable<std::vector<Playlist>> AppStore::getPlaylistsObservable() {
  return rxcpp::sources::create<std::vector<Playlist>>([this](auto observer) {
           if (!networkClient) {
             Util::logError("AppStore", "Cannot get playlists - network client not set");
             observer.on_error(std::make_exception_ptr(std::runtime_error("Network client not set")));
             return;
           }

           Util::logInfo("AppStore", "Fetching playlists");

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
                   Util::logInfo("AppStore", "Loaded " + juce::String(playlists.size()) + " playlists");
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
               Util::logError("AppStore", "Failed to get playlists: " + result.getError());
               observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
             }
           });
         })
      .observe_on(Rx::observe_on_juce_thread());
}

void AppStore::loadPlaylists() {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot load playlists - network client not set");
    return;
  }

  PlaylistState newState = stateManager.playlists->getState();
  newState.isLoading = true;
  stateManager.playlists->setState(newState);

  loadPlaylistsObservable().subscribe(
      [this](const std::vector<Playlist> &playlists) {
        // Convert value types to shared_ptr for state
        std::vector<std::shared_ptr<Playlist>> playlistsList;
        playlistsList.reserve(playlists.size());
        for (const auto &p : playlists) {
          playlistsList.push_back(std::make_shared<Playlist>(p));
        }

        PlaylistState successState = stateManager.playlists->getState();
        successState.playlists = std::move(playlistsList);
        successState.isLoading = false;
        successState.playlistError = "";
        stateManager.playlists->setState(successState);
      },
      [this](std::exception_ptr ep) {
        juce::String errorMsg;
        try {
          std::rethrow_exception(ep);
        } catch (const std::exception &e) {
          errorMsg = e.what();
        }
        PlaylistState errorState = stateManager.playlists->getState();
        errorState.isLoading = false;
        errorState.playlistError = errorMsg;
        Util::logError("AppStore", "Failed to load playlists: " + errorMsg);
        stateManager.playlists->setState(errorState);
      });
}

void AppStore::createPlaylist(const juce::String &name, const juce::String &description) {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot create playlist - network client not set");
    return;
  }

  Util::logInfo("AppStore", "Creating playlist: " + name);

  createPlaylistObservable(name, description)
      .subscribe(
          [this](const Playlist &) {
            Util::logInfo("AppStore", "Playlist created successfully");
            // Reload playlists to get the new one
            loadPlaylists();
          },
          [this](std::exception_ptr ep) {
            juce::String errorMsg;
            try {
              std::rethrow_exception(ep);
            } catch (const std::exception &e) {
              errorMsg = e.what();
            }
            PlaylistState createErrorState = stateManager.playlists->getState();
            createErrorState.playlistError = errorMsg;
            Util::logError("AppStore", "Failed to create playlist: " + errorMsg);
            stateManager.playlists->setState(createErrorState);
          });
}

void AppStore::deletePlaylist(const juce::String &playlistId) {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot delete playlist - network client not set");
    return;
  }

  Util::logInfo("AppStore", "Deleting playlist: " + playlistId);

  // deletePlaylistObservable handles optimistic update and rollback
  deletePlaylistObservable(playlistId)
      .subscribe([playlistId](int) { Util::logInfo("AppStore", "Playlist deleted on server: " + playlistId); },
                 [this](std::exception_ptr ep) {
                   juce::String errorMsg;
                   try {
                     std::rethrow_exception(ep);
                   } catch (const std::exception &e) {
                     errorMsg = e.what();
                   }
                   PlaylistState errorState = stateManager.playlists->getState();
                   errorState.playlistError = errorMsg;
                   stateManager.playlists->setState(errorState);
                   Util::logError("AppStore", "Failed to delete playlist: " + errorMsg);
                 });
}

void AppStore::addPostToPlaylist(const juce::String &postId, const juce::String &playlistId) {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot add post to playlist - network client not set");
    return;
  }

  Util::logInfo("AppStore", "Adding post " + postId + " to playlist " + playlistId);

  addPostToPlaylistObservable(postId, playlistId)
      .subscribe([](int) { Util::logInfo("AppStore", "Post added to playlist successfully"); },
                 [this](std::exception_ptr ep) {
                   juce::String errorMsg;
                   try {
                     std::rethrow_exception(ep);
                   } catch (const std::exception &e) {
                     errorMsg = e.what();
                   }
                   PlaylistState errorState = stateManager.playlists->getState();
                   errorState.playlistError = errorMsg;
                   stateManager.playlists->setState(errorState);
                   Util::logError("AppStore", "Failed to add post to playlist: " + errorMsg);
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

rxcpp::observable<AppStore::PlaylistDetailResult> AppStore::getPlaylistObservable(const juce::String &playlistId) {
  return rxcpp::sources::create<PlaylistDetailResult>([this, playlistId](auto observer) {
           if (!networkClient) {
             Util::logError("AppStore", "Cannot get playlist - network client not set");
             observer.on_error(std::make_exception_ptr(std::runtime_error("Network client not set")));
             return;
           }

           Util::logInfo("AppStore", "Getting playlist via observable: " + playlistId);

           networkClient->getPlaylist(playlistId, [observer, playlistId](Outcome<juce::var> result) {
             if (result.isOk()) {
               Util::logInfo("AppStore", "Got playlist via observable");
               PlaylistDetailResult detailResult;

               auto response = result.getValue();

               // Parse playlist
               detailResult.playlist = Playlist::fromJSON(response);

               // Parse entries
               if (response.hasProperty("entries")) {
                 auto entriesArray = response["entries"];
                 if (entriesArray.isArray()) {
                   detailResult.entries.reserve(static_cast<size_t>(entriesArray.size()));
                   for (int i = 0; i < entriesArray.size(); ++i) {
                     detailResult.entries.push_back(PlaylistEntry::fromJSON(entriesArray[i]));
                   }
                 }
               }

               // Parse collaborators
               if (response.hasProperty("collaborators")) {
                 auto collabsArray = response["collaborators"];
                 if (collabsArray.isArray()) {
                   detailResult.collaborators.reserve(static_cast<size_t>(collabsArray.size()));
                   for (int i = 0; i < collabsArray.size(); ++i) {
                     detailResult.collaborators.push_back(PlaylistCollaborator::fromJSON(collabsArray[i]));
                   }
                 }
               }

               observer.on_next(detailResult);
               observer.on_completed();
             } else {
               Util::logError("AppStore", "Failed to get playlist: " + result.getError());
               observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
             }
           });
         })
      .observe_on(Rx::observe_on_juce_thread());
}

rxcpp::observable<int> AppStore::removePlaylistEntryObservable(const juce::String &playlistId,
                                                               const juce::String &entryId) {
  return rxcpp::sources::create<int>([this, playlistId, entryId](auto observer) {
           if (!networkClient) {
             Util::logError("AppStore", "Cannot remove playlist entry - network client not set");
             observer.on_error(std::make_exception_ptr(std::runtime_error("Network client not set")));
             return;
           }

           Util::logInfo("AppStore", "Removing playlist entry via observable: " + entryId);

           networkClient->removePlaylistEntry(playlistId, entryId, [observer, entryId](Outcome<juce::var> result) {
             if (result.isOk()) {
               Util::logInfo("AppStore", "Removed playlist entry via observable: " + entryId);
               observer.on_next(0);
               observer.on_completed();
             } else {
               Util::logError("AppStore", "Failed to remove playlist entry: " + result.getError());
               observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
             }
           });
         })
      .observe_on(Rx::observe_on_juce_thread());
}

} // namespace Stores
} // namespace Sidechain
