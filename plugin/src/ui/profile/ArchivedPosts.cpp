#include "ArchivedPosts.h"
#include "../../network/NetworkClient.h"
#include "../../util/Log.h"
#include "../feed/PostCard.h"

// Local color aliases for this component
namespace {
namespace Colors {
inline juce::Colour background() {
  return SidechainColors::background();
}
inline juce::Colour headerBg() {
  return SidechainColors::backgroundLight();
}
inline juce::Colour textPrimary() {
  return SidechainColors::textPrimary();
}
inline juce::Colour textSecondary() {
  return SidechainColors::textSecondary();
}
inline juce::Colour border() {
  return SidechainColors::border();
}
inline juce::Colour error() {
  return SidechainColors::error();
}
} // namespace Colors
} // namespace

//==============================================================================
ArchivedPosts::ArchivedPosts() {
  addAndMakeVisible(scrollBar);
  scrollBar.addListener(this);
  scrollBar.setRangeLimits(0.0, 1.0);
}

ArchivedPosts::~ArchivedPosts() {
  scrollBar.removeListener(this);
  if (storeUnsubscriber) {
    storeUnsubscriber();
  }
}

//==============================================================================
void ArchivedPosts::paint(juce::Graphics &g) {
  // Background
  g.fillAll(Colors::background());

  // Header
  drawHeader(g);

  // Content area
  auto contentBounds = getContentBounds();

  if (isLoading && archivedPosts.isEmpty()) {
    drawLoadingState(g, contentBounds);
  } else if (!errorMessage.isEmpty()) {
    drawErrorState(g, contentBounds);
  } else if (archivedPosts.isEmpty()) {
    drawEmptyState(g, contentBounds);
  }
  // Posts are rendered via PostCard children
}

void ArchivedPosts::resized() {
  auto bounds = getLocalBounds();

  // Scroll bar on the right
  auto scrollBarWidth = 8;
  scrollBar.setBounds(bounds.removeFromRight(scrollBarWidth));

  // Update post card positions
  updatePostCardPositions();
  updateScrollBounds();
}

//==============================================================================
void ArchivedPosts::mouseUp(const juce::MouseEvent &event) {
  auto pos = event.getPosition();

  // Back button
  if (getBackButtonBounds().contains(pos)) {
    if (onBackPressed)
      onBackPressed();
    return;
  }
}

void ArchivedPosts::mouseWheelMove(const juce::MouseEvent & /*event*/, const juce::MouseWheelDetails &wheel) {
  auto contentHeight = calculateContentHeight();
  auto visibleHeight = getContentBounds().getHeight();

  if (contentHeight <= visibleHeight)
    return;

  int maxScroll = contentHeight - visibleHeight;
  int delta = static_cast<int>(wheel.deltaY * 100);

  scrollOffset = juce::jlimit(0, maxScroll, scrollOffset - delta);

  updatePostCardPositions();
  scrollBar.setCurrentRange(scrollOffset, visibleHeight, juce::dontSendNotification);

  // Load more when near bottom
  loadMoreIfNeeded();

  repaint();
}

void ArchivedPosts::scrollBarMoved(juce::ScrollBar * /*scrollBar*/, double newRangeStart) {
  scrollOffset = static_cast<int>(newRangeStart);
  updatePostCardPositions();

  // Load more when near bottom
  loadMoreIfNeeded();

  repaint();
}

//==============================================================================
void ArchivedPosts::setNetworkClient(NetworkClient *client) {
  networkClient = client;
}

void ArchivedPosts::setArchivedPostsStore(std::shared_ptr<Sidechain::Stores::ArchivedPostsStore> store) {
  // Unsubscribe from old store
  if (storeUnsubscriber) {
    storeUnsubscriber();
  }

  archivedPostsStore = store;

  if (archivedPostsStore) {
    // Subscribe to store updates
    storeUnsubscriber = archivedPostsStore->subscribe([this](const Sidechain::Stores::ArchivedPostsState &state) {
      archivedPosts = state.posts;
      isLoading = state.isLoading;
      errorMessage = state.error;
      rebuildPostCards();
      repaint();
    });
  }
}

void ArchivedPosts::loadArchivedPosts() {
  if (archivedPostsStore) {
    archivedPostsStore->loadArchivedPosts();
  } else if (networkClient) {
    // Fallback to direct network client if store not available
    archivedPosts.clear();
    currentOffset = 0;
    hasMore = true;
    errorMessage.clear();
    postCards.clear();
    fetchArchivedPosts();
  }
}

void ArchivedPosts::refresh() {
  if (archivedPostsStore) {
    archivedPostsStore->refreshArchivedPosts();
  } else if (networkClient) {
    loadArchivedPosts();
  }
}

//==============================================================================
void ArchivedPosts::setCurrentlyPlayingPost(const juce::String &postId) {
  currentlyPlayingPostId = postId;

  for (auto *card : postCards) {
    bool isPlaying = card->getPostId() == postId;
    card->setIsPlaying(isPlaying);
  }
}

void ArchivedPosts::setPlaybackProgress(float progress) {
  currentPlaybackProgress = progress;

  for (auto *card : postCards) {
    if (card->getPostId() == currentlyPlayingPostId) {
      card->setPlaybackProgress(progress);
      break;
    }
  }
}

void ArchivedPosts::clearPlayingState() {
  currentlyPlayingPostId.clear();
  currentPlaybackProgress = 0.0f;

  for (auto *card : postCards) {
    card->setIsPlaying(false);
    card->setPlaybackProgress(0.0f);
  }
}

//==============================================================================
void ArchivedPosts::drawHeader(juce::Graphics &g) {
  auto bounds = getLocalBounds().removeFromTop(HEADER_HEIGHT);

  // Header background
  g.setColour(Colors::headerBg());
  g.fillRect(bounds);

  // Back button
  auto backBounds = getBackButtonBounds();
  g.setColour(Colors::textPrimary());
  g.setFont(juce::FontOptions(20.0f));
  g.drawText("<", backBounds, juce::Justification::centred);

  // Title
  g.setColour(Colors::textPrimary());
  g.setFont(juce::FontOptions(18.0f).withStyle("Bold"));
  g.drawText("Archived Posts", bounds, juce::Justification::centred);

  // Bottom border
  g.setColour(Colors::border());
  g.drawLine(static_cast<float>(bounds.getX()), static_cast<float>(bounds.getBottom() - 1),
             static_cast<float>(bounds.getRight()), static_cast<float>(bounds.getBottom() - 1), 1.0f);
}

void ArchivedPosts::drawLoadingState(juce::Graphics &g, juce::Rectangle<int> bounds) {
  g.setColour(Colors::textSecondary());
  g.setFont(juce::FontOptions(16.0f));
  g.drawText("Loading archived posts...", bounds, juce::Justification::centred);
}

void ArchivedPosts::drawEmptyState(juce::Graphics &g, juce::Rectangle<int> bounds) {
  // Text
  g.setColour(Colors::textSecondary());
  g.setFont(juce::FontOptions(16.0f).withStyle("Bold"));
  g.drawText("No Archived Posts", bounds, juce::Justification::centred);

  g.setFont(juce::FontOptions(14.0f));
  g.drawText("Posts you archive will appear here", bounds.withTrimmedTop(30), juce::Justification::centred);
}

void ArchivedPosts::drawErrorState(juce::Graphics &g, juce::Rectangle<int> bounds) {
  g.setColour(Colors::error());
  g.setFont(juce::FontOptions(16.0f));
  g.drawText(errorMessage, bounds, juce::Justification::centred);
}

//==============================================================================
juce::Rectangle<int> ArchivedPosts::getBackButtonBounds() const {
  return juce::Rectangle<int>(PADDING, 0, 40, HEADER_HEIGHT);
}

juce::Rectangle<int> ArchivedPosts::getContentBounds() const {
  auto bounds = getLocalBounds();
  bounds.removeFromTop(HEADER_HEIGHT);
  bounds.removeFromRight(8); // Scroll bar width
  return bounds;
}

//==============================================================================
void ArchivedPosts::fetchArchivedPosts() {
  if (networkClient == nullptr) {
    errorMessage = "Not connected";
    repaint();
    return;
  }

  isLoading = true;
  repaint();

  networkClient->getArchivedPosts(PAGE_SIZE, currentOffset, [this](Outcome<juce::var> result) {
    isLoading = false;

    if (result.isError()) {
      Log::error("ArchivedPosts: Failed to fetch archived posts: " + result.getError());
      errorMessage = "Failed to load archived posts";
      repaint();
      return;
    }

    auto data = result.getValue();
    auto postsArray = data["posts"];

    if (postsArray.isArray()) {
      for (int i = 0; i < postsArray.size(); ++i) {
        auto postData = postsArray[i];
        auto post = FeedPost::fromJson(postData);
        if (post.isValid()) {
          archivedPosts.add(post);
        }
      }
    }

    // Check for more results
    hasMore = data["has_more"].operator bool();
    currentOffset = archivedPosts.size();

    Log::debug("ArchivedPosts: Loaded " + juce::String(archivedPosts.size()) +
               " archived posts, hasMore: " + (hasMore ? "true" : "false"));

    rebuildPostCards();
    repaint();
  });
}

void ArchivedPosts::loadMoreIfNeeded() {
  if (isLoading)
    return;

  auto contentHeight = calculateContentHeight();
  auto visibleHeight = getContentBounds().getHeight();

  // Load more when scrolled near the bottom
  if (scrollOffset + visibleHeight >= contentHeight - 200) {
    Log::debug("ArchivedPosts: Loading more posts...");
    if (archivedPostsStore) {
      archivedPostsStore->loadMoreArchivedPosts();
    } else if (networkClient) {
      // Fallback to direct network client
      fetchArchivedPosts();
    }
  }
}

//==============================================================================
void ArchivedPosts::rebuildPostCards() {
  postCards.clear();

  for (const auto &post : archivedPosts) {
    auto *card = postCards.add(new PostCard());
    card->setPost(post);
    setupPostCardCallbacks(card);
    addAndMakeVisible(card);

    // Set playing state if applicable
    if (post.id == currentlyPlayingPostId) {
      card->setIsPlaying(true);
      card->setPlaybackProgress(currentPlaybackProgress);
    }
  }

  updatePostCardPositions();
  updateScrollBounds();
}

void ArchivedPosts::updatePostCardPositions() {
  auto contentBounds = getContentBounds();
  int y = contentBounds.getY() + PADDING - scrollOffset;

  for (auto *card : postCards) {
    card->setBounds(contentBounds.getX() + PADDING, y, contentBounds.getWidth() - PADDING * 2, POST_CARD_HEIGHT);
    y += POST_CARD_HEIGHT + POST_CARD_SPACING;
  }
}

int ArchivedPosts::calculateContentHeight() const {
  if (archivedPosts.isEmpty())
    return 0;

  return PADDING + archivedPosts.size() * (POST_CARD_HEIGHT + POST_CARD_SPACING);
}

void ArchivedPosts::updateScrollBounds() {
  auto contentHeight = calculateContentHeight();
  auto visibleHeight = getContentBounds().getHeight();

  if (contentHeight <= visibleHeight) {
    scrollBar.setVisible(false);
    scrollOffset = 0;
  } else {
    scrollBar.setVisible(true);
    scrollBar.setRangeLimits(0.0, contentHeight);
    scrollBar.setCurrentRange(scrollOffset, visibleHeight);
  }
}

void ArchivedPosts::setupPostCardCallbacks(PostCard *card) {
  card->onPlayClicked = [this](const FeedPost &post) {
    if (onPlayClicked)
      onPlayClicked(post);
  };

  card->onPauseClicked = [this](const FeedPost &post) {
    if (onPauseClicked)
      onPauseClicked(post);
  };

  card->onUserClicked = [this](const FeedPost &post) {
    if (onUserClicked)
      onUserClicked(post.userId);
  };

  card->onCardTapped = [this](const FeedPost &post) {
    if (onPostClicked)
      onPostClicked(post);
  };

  // Handle unarchive - restore post to visible
  card->onArchiveToggled = [this](const FeedPost &post, bool archived) {
    if (!archived) {
      if (archivedPostsStore) {
        Log::info("ArchivedPosts: Unarchiving post: " + post.id);
        archivedPostsStore->restorePost(post.id);
      } else if (networkClient != nullptr) {
        Log::info("ArchivedPosts: Unarchiving post: " + post.id);
        networkClient->unarchivePost(post.id, [this, postId = post.id](Outcome<juce::var> result) {
          if (result.isError()) {
            Log::error("ArchivedPosts: Failed to unarchive post: " + result.getError());
            return;
          }
          // Remove from list
          juce::MessageManager::callAsync([this, postId]() {
            for (int i = 0; i < archivedPosts.size(); ++i) {
              if (archivedPosts[i].id == postId) {
                archivedPosts.remove(i);
                rebuildPostCards();
                repaint();
                break;
              }
            }
          });
        });
      }
    }
  };

  // Like functionality
  card->onLikeToggled = [this](const FeedPost &post, bool liked) {
    if (networkClient == nullptr)
      return;

    if (liked) {
      networkClient->likePost(post.id, "", [](Outcome<juce::var> result) {
        if (result.isError())
          Log::error("ArchivedPosts: Like failed: " + result.getError());
      });
    } else {
      networkClient->unlikePost(post.id, [](Outcome<juce::var> result) {
        if (result.isError())
          Log::error("ArchivedPosts: Unlike failed: " + result.getError());
      });
    }
  };
}
