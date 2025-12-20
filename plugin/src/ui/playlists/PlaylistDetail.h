#pragma once

#include "../../models/FeedPost.h"
#include "../../models/Playlist.h"
#include "../../stores/AppStore.h"
#include "../common/AppStoreComponent.h"
#include "../../util/Colors.h"
#include <JuceHeader.h>

class NetworkClient;

// ==============================================================================
/**
 * PlaylistDetail displays a single playlist with entries
 *
 * Features:
 * - Show playlist name, description, collaborators
 * - List entries (posts) in order
 * - Play button (play all entries sequentially)
 * - "Add Track" button (if user has edit permission)
 * - Remove entry button (if user has edit permission)
 * - Reorder entries (drag and drop - future enhancement)
 */
class PlaylistDetail : public Sidechain::UI::AppStoreComponent<Sidechain::Stores::PlaylistState>,
                       public juce::ScrollBar::Listener {
public:
  PlaylistDetail(Sidechain::Stores::AppStore *store = nullptr);
  ~PlaylistDetail() override;

  // ==============================================================================
  void paint(juce::Graphics &) override;
  void resized() override;
  void mouseUp(const juce::MouseEvent &event) override;
  void mouseWheelMove(const juce::MouseEvent &event, const juce::MouseWheelDetails &wheel) override;

  // ScrollBar::Listener
  void scrollBarMoved(juce::ScrollBar *scrollBar, double newRangeStart) override;

  // ==============================================================================
  // Network client integration
  void setNetworkClient(NetworkClient *client);
  void setCurrentUserId(const juce::String &userId) {
    currentUserId = userId;
  }

  // Load playlist
  void loadPlaylist(const juce::String &playlistId);
  void refresh();

  // ==============================================================================
  // Callbacks
  std::function<void()> onBackPressed;
  std::function<void(const juce::String &postId)> onPostSelected;      // Navigate to post
  std::function<void()> onAddTrack;                                    // Show add track dialog
  std::function<void()> onPlayPlaylist;                                // Play all tracks sequentially
  std::function<void(const juce::String &playlistId)> onSharePlaylist; // Share playlist link

protected:
  void onAppStateChanged(const Sidechain::Stores::PlaylistState &state) override;
  void subscribeToAppStore() override;

private:
  // ==============================================================================
  // Data
  NetworkClient *networkClient = nullptr;
  juce::String currentUserId;
  juce::String playlistId;
  Sidechain::Playlist playlist;
  juce::Array<Sidechain::PlaylistEntry> entries;
  juce::Array<Sidechain::PlaylistCollaborator> collaborators;
  bool isLoading = false;
  juce::String errorMessage;

  // ==============================================================================
  // UI Components
  juce::ScrollBar scrollBar{true}; // vertical

  // Scroll state
  int scrollOffset = 0;

  // ==============================================================================
  // Layout constants
  static constexpr int HEADER_HEIGHT = 60;
  static constexpr int INFO_HEIGHT = 120;
  static constexpr int ENTRY_CARD_HEIGHT = 70;
  static constexpr int BUTTON_HEIGHT = 44;
  static constexpr int PADDING = 16;

  // ==============================================================================
  // Drawing methods
  void drawHeader(juce::Graphics &g);
  void drawPlaylistInfo(juce::Graphics &g, juce::Rectangle<int> &bounds);
  void drawActionButtons(juce::Graphics &g, juce::Rectangle<int> &bounds);
  void drawEntryCard(juce::Graphics &g, juce::Rectangle<int> bounds, const Sidechain::PlaylistEntry &entry, int index);
  void drawLoadingState(juce::Graphics &g, juce::Rectangle<int> bounds);
  void drawErrorState(juce::Graphics &g, juce::Rectangle<int> bounds);
  void drawEmptyState(juce::Graphics &g, juce::Rectangle<int> bounds);

  // ==============================================================================
  // Hit testing helpers
  juce::Rectangle<int> getBackButtonBounds() const;
  juce::Rectangle<int> getPlayButtonBounds() const;
  juce::Rectangle<int> getAddTrackButtonBounds() const;
  juce::Rectangle<int> getShareButtonBounds() const;
  juce::Rectangle<int> getContentBounds() const;
  juce::Rectangle<int> getEntryCardBounds(int index) const;
  juce::Rectangle<int> getRemoveEntryButtonBounds(int index) const;

  // ==============================================================================
  // Network operations
  void fetchPlaylist();
  void removeEntry(const juce::String &entryId);

  // ==============================================================================
  // Helper methods
  int calculateContentHeight() const;
  void updateScrollBounds();
  bool canEdit() const;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PlaylistDetail)
};
