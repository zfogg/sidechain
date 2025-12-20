#pragma once

#include "../../models/FeedPost.h"
#include "../../util/Colors.h"
#include "../common/AppStoreComponent.h"
#include <JuceHeader.h>
#include <memory>

class NetworkClient;
class PostCard;

namespace Sidechain {
namespace Stores {
class AppStore;
struct PostsState;
} // namespace Stores
} // namespace Sidechain

using namespace Sidechain::UI;
using namespace Sidechain::Stores;

// ==============================================================================
/**
 * ArchivedPosts displays the user's archived posts (hidden without deletion)
 *
 * Features:
 * - List of archived posts in a scrollable view
 * - Click to play audio
 * - Unarchive functionality (restore to visible)
 */
class ArchivedPosts : public AppStoreComponent<PostsState>, public juce::ScrollBar::Listener {
public:
  ArchivedPosts(AppStore *store = nullptr);
  ~ArchivedPosts() override;

  // ==============================================================================
  void paint(juce::Graphics &) override;
  void resized() override;
  void mouseUp(const juce::MouseEvent &event) override;
  void mouseWheelMove(const juce::MouseEvent &event, const juce::MouseWheelDetails &wheel) override;

  // ScrollBar::Listener
  void scrollBarMoved(juce::ScrollBar *scrollBar, double newRangeStart) override;

  // ==============================================================================
  // Store and network client integration
  void setNetworkClient(NetworkClient *client);
  void setCurrentUserId(const juce::String &userId) {
    currentUserId = userId;
  }

  // Load archived posts
  void loadArchivedPosts();
  void refresh();

  // ==============================================================================
  // Callbacks
  std::function<void()> onBackPressed;
  std::function<void(const Sidechain::FeedPost &)> onPostClicked;
  std::function<void(const Sidechain::FeedPost &)> onPlayClicked;
  std::function<void(const Sidechain::FeedPost &)> onPauseClicked;
  std::function<void(const juce::String &userId)> onUserClicked;

  // ==============================================================================
  // Playback state
  void setCurrentlyPlayingPost(const juce::String &postId);
  void setPlaybackProgress(float progress);
  void clearPlayingState();

protected:
  // ==============================================================================
  // AppStoreComponent overrides
  void onAppStateChanged(const PostsState &state) override;
  void subscribeToAppStore() override;

private:
  // ==============================================================================
  // Data
  NetworkClient *networkClient = nullptr;
  juce::String currentUserId;

  // Cached state from store
  juce::Array<Sidechain::FeedPost> archivedPosts;
  bool isLoading = false;
  juce::String errorMessage;

  // Pagination (for fallback direct NetworkClient usage)
  int currentOffset = 0;
  bool hasMore = true;
  static constexpr int PAGE_SIZE = 20;

  // ==============================================================================
  // UI Components
  juce::ScrollBar scrollBar{true}; // vertical

  // Post cards for rendering posts
  juce::OwnedArray<PostCard> postCards;

  // Playback state
  juce::String currentlyPlayingPostId;
  float currentPlaybackProgress = 0.0f;

  // Scroll state
  int scrollOffset = 0;

  // ==============================================================================
  // Layout constants
  static constexpr int HEADER_HEIGHT = 60;
  static constexpr int POST_CARD_HEIGHT = 120;
  static constexpr int POST_CARD_SPACING = 8;
  static constexpr int PADDING = 16;

  // ==============================================================================
  // Drawing methods
  void drawHeader(juce::Graphics &g);
  void drawLoadingState(juce::Graphics &g, juce::Rectangle<int> bounds);
  void drawEmptyState(juce::Graphics &g, juce::Rectangle<int> bounds);
  void drawErrorState(juce::Graphics &g, juce::Rectangle<int> bounds);

  // ==============================================================================
  // Hit testing helpers
  juce::Rectangle<int> getBackButtonBounds() const;
  juce::Rectangle<int> getContentBounds() const;

  // ==============================================================================
  // Network operations
  void fetchArchivedPosts();

  // ==============================================================================
  // Helper methods
  void rebuildPostCards();
  void updatePostCardPositions();
  int calculateContentHeight() const;
  void updateScrollBounds();
  void setupPostCardCallbacks(PostCard *card);
  void updateStateFromStore();
  void loadMoreIfNeeded();

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ArchivedPosts)
};
