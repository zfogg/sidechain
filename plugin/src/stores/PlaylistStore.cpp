#include "PlaylistStore.h"
#include "../util/Json.h"
#include "../util/Log.h"

namespace Sidechain {
namespace Stores {

//==============================================================================
PlaylistStore::PlaylistStore(NetworkClient *client) : networkClient(client) {
  Log::info("PlaylistStore: Initializing");
  setState(PlaylistState{});
}

//==============================================================================
void PlaylistStore::loadPlaylists() {
  if (networkClient == nullptr)
    return;

  auto state = getState();
  state.isLoading = true;
  setState(state);

  Log::info("PlaylistStore: Loading playlists");

  networkClient->getPlaylists("all", [this](Outcome<juce::var> result) { handlePlaylistsLoaded(result); });
}

//==============================================================================
void PlaylistStore::refreshPlaylists() {
  Log::info("PlaylistStore: Refreshing playlists");

  auto state = getState();
  state.allPlaylists.clear();
  state.filteredPlaylists.clear();
  setState(state);

  loadPlaylists();
}

//==============================================================================
void PlaylistStore::filterPlaylists(PlaylistState::FilterType filterType) {
  auto state = getState();

  if (state.currentFilter == filterType)
    return; // No change needed

  state.currentFilter = filterType;
  state.filteredPlaylists = applyPlaylistFilter(state.allPlaylists, filterType);
  setState(state);

  Log::debug("PlaylistStore: Filtered playlists, count: " + juce::String(state.filteredPlaylists.size()));
}

//==============================================================================
void PlaylistStore::handlePlaylistsLoaded(Outcome<juce::var> result) {
  auto state = getState();
  state.isLoading = false;

  if (result.isError()) {
    Log::error("PlaylistStore: Failed to load playlists - " + result.getError());
    state.errorMessage = "Failed to load playlists";
    setState(state);
    return;
  }

  auto response = result.getValue();

  // Parse playlists from response
  juce::Array<Playlist> playlists;

  if (response.hasProperty("playlists")) {
    auto playlistsArray = response["playlists"];
    if (playlistsArray.isArray()) {
      for (int i = 0; i < playlistsArray.size(); ++i) {
        playlists.add(Playlist::fromJSON(playlistsArray[i]));
      }
    }
  }

  updatePlaylists(playlists);
}

//==============================================================================
void PlaylistStore::updatePlaylists(const juce::Array<Playlist> &playlists) {
  auto state = getState();
  state.allPlaylists = playlists;
  state.filteredPlaylists = applyPlaylistFilter(playlists, state.currentFilter);
  state.errorMessage = "";
  state.lastUpdated = juce::Time::getCurrentTime().toMilliseconds();
  setState(state);

  Log::info("PlaylistStore: Loaded " + juce::String(playlists.size()) + " playlists, " +
            juce::String(state.filteredPlaylists.size()) + " after filtering");
}

//==============================================================================
juce::Array<Playlist> PlaylistStore::applyPlaylistFilter(const juce::Array<Playlist> &playlists,
                                                         PlaylistState::FilterType filter) {
  juce::Array<Playlist> filtered;

  if (filter == PlaylistState::FilterType::All) {
    return playlists;
  }

  // Filter based on playlist type
  for (const auto &playlist : playlists) {
    bool includePlaylist = false;

    switch (filter) {
    case PlaylistState::FilterType::Owned:
      includePlaylist = playlist.isOwner();
      break;
    case PlaylistState::FilterType::Collaborated:
      includePlaylist = !playlist.isOwner() && playlist.canEdit();
      break;
    case PlaylistState::FilterType::Public:
      includePlaylist = playlist.isPublic;
      break;
    default:
      includePlaylist = true;
    }

    if (includePlaylist) {
      filtered.add(playlist);
    }
  }

  return filtered;
}

} // namespace Stores
} // namespace Sidechain
