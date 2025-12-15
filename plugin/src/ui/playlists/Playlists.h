#pragma once

#include "../../models/Playlist.h"
#include "../../util/Colors.h"
#include <JuceHeader.h>

class NetworkClient;

//==============================================================================
/**
 * Playlists displays user's playlists and allows creating new ones (R.3.1.3.1)
 *
 * Features:
 * - List of user's playlists (owned + collaborated)
 * - "Create Playlist" button
 * - Filter tabs (All, Owned, Collaborated, Public)
 * - Click playlist → Open playlist detail
 */
class Playlists : public juce::Component, public juce::ScrollBar::Listener {
public:
  Playlists();
  ~Playlists() override;

  //==============================================================================
  void paint(juce::Graphics &) override;
  void resized() override;
  void mouseUp(const juce::MouseEvent &event) override;
  void mouseWheelMove(const juce::MouseEvent &event, const juce::MouseWheelDetails &wheel) override;

  // ScrollBar::Listener
  void scrollBarMoved(juce::ScrollBar *scrollBar, double newRangeStart) override;

  //==============================================================================
  // Network client integration
  void setNetworkClient(NetworkClient *client);
  void setCurrentUserId(const juce::String &userId) {
    currentUserId = userId;
  }

  // Load playlists
  void loadPlaylists();
  void refresh();

  //==============================================================================
  // Callbacks
  std::function<void()> onBackPressed;
  std::function<void(const juce::String &playlistId)> onPlaylistSelected; // Navigate to playlist detail
  std::function<void()> onCreatePlaylist;                                 // Show create playlist dialog

private:
  //==============================================================================
  // Filter types
  enum class FilterType { All, Owned, Collaborated, Public };

  FilterType currentFilter = FilterType::All;

  //==============================================================================
  // Data
  NetworkClient *networkClient = nullptr;
  juce::String currentUserId;
  juce::Array<Playlist> playlists;
  bool isLoading = false;
  juce::String errorMessage;

  //==============================================================================
  // UI Components
  juce::ScrollBar scrollBar{true}; // vertical

  // Scroll state
  int scrollOffset = 0;

  //==============================================================================
  // Layout constants
  static constexpr int HEADER_HEIGHT = 60;
  static constexpr int FILTER_TAB_HEIGHT = 40;
  static constexpr int PLAYLIST_CARD_HEIGHT = 80;
  static constexpr int CREATE_BUTTON_HEIGHT = 60;
  static constexpr int PADDING = 16;

  //==============================================================================
  // Drawing methods
  void drawHeader(juce::Graphics &g);
  void drawFilterTabs(juce::Graphics &g, juce::Rectangle<int> &bounds);
  void drawCreateButton(juce::Graphics &g, juce::Rectangle<int> &bounds);
  void drawPlaylistCard(juce::Graphics &g, juce::Rectangle<int> bounds, const Playlist &playlist);
  void drawLoadingState(juce::Graphics &g, juce::Rectangle<int> bounds);
  void drawEmptyState(juce::Graphics &g, juce::Rectangle<int> bounds);
  void drawErrorState(juce::Graphics &g, juce::Rectangle<int> bounds);

  //==============================================================================
  // Hit testing helpers
  juce::Rectangle<int> getBackButtonBounds() const;
  juce::Rectangle<int> getFilterTabBounds(FilterType filter) const;
  juce::Rectangle<int> getCreateButtonBounds() const;
  juce::Rectangle<int> getContentBounds() const;
  juce::Rectangle<int> getPlaylistCardBounds(int index) const;

  //==============================================================================
  // Network operations
  void fetchPlaylists(FilterType filter);

  //==============================================================================
  // Helper methods
  int calculateContentHeight() const;
  void updateScrollBounds();

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Playlists)
};
