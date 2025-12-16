#pragma once

#include "../models/Playlist.h"
#include "../network/NetworkClient.h"
#include "../util/logging/Logger.h"
#include "Store.h"
#include <JuceHeader.h>

namespace Sidechain {
namespace Stores {

/**
 * PlaylistState - Immutable state for playlists
 */
struct PlaylistState {
  enum class FilterType { All, Owned, Collaborated, Public };

  juce::Array<Playlist> allPlaylists;
  juce::Array<Playlist> filteredPlaylists;
  FilterType currentFilter = FilterType::All;
  bool isLoading = false;
  juce::String errorMessage;
  int64_t lastUpdated = 0;
};

/**
 * PlaylistStore - Reactive store for managing user playlists (R.3.1.3.1)
 *
 * Features:
 * - Load user's playlists (owned + collaborated)
 * - Filter playlists by type (All, Owned, Collaborated, Public)
 * - Track loading state and errors
 * - Pagination support (if needed)
 *
 * Usage:
 * ```cpp
 * auto playlistStore = std::make_shared<PlaylistStore>(networkClient);
 * playlistStore->subscribe([this](const PlaylistState& state) {
 *   updatePlaylistsUI(state.filteredPlaylists);
 * });
 * playlistStore->loadPlaylists();
 * playlistStore->filterPlaylists(PlaylistState::FilterType::Owned);
 * ```
 */
class PlaylistStore : public Store<PlaylistState> {
public:
  explicit PlaylistStore(NetworkClient *client);
  ~PlaylistStore() override = default;

  //==============================================================================
  // Data Loading
  void loadPlaylists();
  void refreshPlaylists();

  //==============================================================================
  // Filtering
  void filterPlaylists(PlaylistState::FilterType filterType);

  //==============================================================================
  // Current State Access
  bool isLoading() const {
    return getState().isLoading;
  }

  juce::Array<Playlist> getAllPlaylists() const {
    return getState().allPlaylists;
  }

  juce::Array<Playlist> getFilteredPlaylists() const {
    return getState().filteredPlaylists;
  }

  PlaylistState::FilterType getCurrentFilter() const {
    return getState().currentFilter;
  }

  juce::String getError() const {
    return getState().errorMessage;
  }

  int getPlaylistCount() const {
    return getState().filteredPlaylists.size();
  }

protected:
  //==============================================================================
  // Helper methods
  void updatePlaylists(const juce::Array<Playlist> &playlists);

private:
  NetworkClient *networkClient;

  //==============================================================================
  // Network callbacks
  void handlePlaylistsLoaded(Outcome<juce::var> result);

  //==============================================================================
  // Filtering logic
  juce::Array<Playlist> applyPlaylistFilter(const juce::Array<Playlist> &playlists, PlaylistState::FilterType filter);

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PlaylistStore)
};

} // namespace Stores
} // namespace Sidechain
