#include "SavedPosts.h"
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
SavedPosts::SavedPosts() {
  addAndMakeVisible(scrollBar);
  scrollBar.addListener(this);
  scrollBar.setRangeLimits(0.0, 1.0);
}

SavedPosts::~SavedPosts() {
  scrollBar.removeListener(this);
  // RAII: Arrays will clean up automatically
}

//==============================================================================
void SavedPosts::paint(juce::Graphics &g) {
  // Background
  g.fillAll(Colors::background());

  // Header
  drawHeader(g);

  // Content area
  auto contentBounds = getContentBounds();

  if (isLoading && savedPosts.isEmpty()) {
    drawLoadingState(g, contentBounds);
  } else if (!errorMessage.isEmpty()) {
    drawErrorState(g, contentBounds);
  } else if (savedPosts.isEmpty()) {
    drawEmptyState(g, contentBounds);
  }
  // Posts are rendered via PostCard children
}

void SavedPosts::resized() {
  auto bounds = getLocalBounds();

  // Scroll bar on the right
  auto scrollBarWidth = 8;
  scrollBar.setBounds(bounds.removeFromRight(scrollBarWidth));

  // Update post card positions
  updatePostCardPositions();
  updateScrollBounds();
}

//==============================================================================
void SavedPosts::mouseUp(const juce::MouseEvent &event) {
  auto pos = event.getPosition();

  // Back button
  if (getBackButtonBounds().contains(pos)) {
    if (onBackPressed)
      onBackPressed();
    return;
  }
}

void SavedPosts::mouseWheelMove(const juce::MouseEvent & /*event*/, const juce::MouseWheelDetails &wheel) {
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

void SavedPosts::scrollBarMoved(juce::ScrollBar * /*scrollBar*/, double newRangeStart) {
  scrollOffset = static_cast<int>(newRangeStart);
  updatePostCardPositions();

  // Load more when near bottom
  loadMoreIfNeeded();

  repaint();
}

//==============================================================================
void SavedPosts::setNetworkClient(NetworkClient *client) {
  networkClient = client;
}

void SavedPosts::loadSavedPosts() {
  // TODO: use AppStore instead of direct NetworkClient
  if (networkClient) {
    fetchSavedPosts();
  }
}

void SavedPosts::refresh() {
  // TODO: use AppStore instead of direct NetworkClient
  if (networkClient) {
    loadSavedPosts();
  }
}

//==============================================================================
void SavedPosts::setCurrentlyPlayingPost(const juce::String &postId) {
  currentlyPlayingPostId = postId;

  for (auto *card : postCards) {
    bool isPlaying = card->getPostId() == postId;
    card->setIsPlaying(isPlaying);
  }
}

void SavedPosts::setPlaybackProgress(float progress) {
  currentPlaybackProgress = progress;

  for (auto *card : postCards) {
    if (card->getPostId() == currentlyPlayingPostId) {
      card->setPlaybackProgress(progress);
      break;
    }
  }
}

void SavedPosts::clearPlayingState() {
  currentlyPlayingPostId.clear();
  currentPlaybackProgress = 0.0f;

  for (auto *card : postCards) {
    card->setIsPlaying(false);
    card->setPlaybackProgress(0.0f);
  }
}

//==============================================================================
void SavedPosts::drawHeader(juce::Graphics &g) {
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
  g.drawText("Saved Posts", bounds, juce::Justification::centred);

  // Bottom border
  g.setColour(Colors::border());
  g.drawLine(static_cast<float>(bounds.getX()), static_cast<float>(bounds.getBottom() - 1),
             static_cast<float>(bounds.getRight()), static_cast<float>(bounds.getBottom() - 1), 1.0f);
}

void SavedPosts::drawLoadingState(juce::Graphics &g, juce::Rectangle<int> bounds) {
  g.setColour(Colors::textSecondary());
  g.setFont(juce::FontOptions(16.0f));
  g.drawText("Loading saved posts...", bounds, juce::Justification::centred);
}

void SavedPosts::drawEmptyState(juce::Graphics &g, juce::Rectangle<int> bounds) {
  // Text
  g.setColour(Colors::textSecondary());
  g.setFont(juce::FontOptions(16.0f).withStyle("Bold"));
  g.drawText("No Saved Posts", bounds, juce::Justification::centred);

  g.setFont(juce::FontOptions(14.0f));
  g.drawText("Posts you save will appear here", bounds.withTrimmedTop(30), juce::Justification::centred);
}

void SavedPosts::drawErrorState(juce::Graphics &g, juce::Rectangle<int> bounds) {
  g.setColour(Colors::error());
  g.setFont(juce::FontOptions(16.0f));
  g.drawText(errorMessage, bounds, juce::Justification::centred);
}

//==============================================================================
juce::Rectangle<int> SavedPosts::getBackButtonBounds() const {
  return juce::Rectangle<int>(PADDING, 0, 40, HEADER_HEIGHT);
}

juce::Rectangle<int> SavedPosts::getContentBounds() const {
  auto bounds = getLocalBounds();
  bounds.removeFromTop(HEADER_HEIGHT);
  bounds.removeFromRight(8); // Scroll bar width
  return bounds;
}

//==============================================================================
void SavedPosts::fetchSavedPosts() {
  if (networkClient == nullptr) {
    errorMessage = "Not connected";
    repaint();
    return;
  }

  isLoading = true;
  repaint();

  networkClient->getSavedPosts(PAGE_SIZE, currentOffset, [this](Outcome<juce::var> result) {
    isLoading = false;

    if (result.isError()) {
      Log::error("SavedPosts: Failed to fetch saved posts: " + result.getError());
      errorMessage = "Failed to load saved posts";
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
          savedPosts.add(post);
        }
      }
    }

    // Check for more results
    hasMore = data["has_more"].operator bool();
    currentOffset = savedPosts.size();

    Log::debug("SavedPosts: Loaded " + juce::String(savedPosts.size()) +
               " saved posts, hasMore: " + (hasMore ? "true" : "false"));

    rebuildPostCards();
    repaint();
  });
}

void SavedPosts::loadMoreIfNeeded() {
  if (isLoading)
    return;

  auto contentHeight = calculateContentHeight();
  auto visibleHeight = getContentBounds().getHeight();

  // Load more when scrolled near the bottom
  if (scrollOffset + visibleHeight >= contentHeight - 200) {
    Log::debug("SavedPosts: Loading more posts...");
    // TODO: use AppStore instead
    if (networkClient) {
      fetchSavedPosts();
    }
  }
}

//==============================================================================
void SavedPosts::rebuildPostCards() {
  postCards.clear();

  for (const auto &post : savedPosts) {
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

void SavedPosts::updatePostCardPositions() {
  auto contentBounds = getContentBounds();
  int y = contentBounds.getY() + PADDING - scrollOffset;

  for (auto *card : postCards) {
    card->setBounds(contentBounds.getX() + PADDING, y, contentBounds.getWidth() - PADDING * 2, POST_CARD_HEIGHT);
    y += POST_CARD_HEIGHT + POST_CARD_SPACING;
  }
}

int SavedPosts::calculateContentHeight() const {
  if (savedPosts.isEmpty())
    return 0;

  return PADDING + savedPosts.size() * (POST_CARD_HEIGHT + POST_CARD_SPACING);
}

void SavedPosts::updateScrollBounds() {
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

void SavedPosts::setupPostCardCallbacks(PostCard *card) {
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

  // Handle unsave - remove from list
  card->onSaveToggled = [this](const FeedPost &post, bool saved) {
    if (!saved && networkClient != nullptr) {
      Log::info("SavedPosts: Unsaving post: " + post.id);
      networkClient->unsavePost(post.id, [this, postId = post.id](Outcome<juce::var> result) {
        if (result.isError()) {
          Log::error("SavedPosts: Failed to unsave post: " + result.getError());
          return;
        }
        // Remove from list
        juce::MessageManager::callAsync([this, postId]() {
          for (int i = 0; i < savedPosts.size(); ++i) {
            if (savedPosts[i].id == postId) {
              savedPosts.remove(i);
              rebuildPostCards();
              repaint();
              break;
            }
          }
        });
      });
    }
  };

  // Like functionality
  card->onLikeToggled = [this](const FeedPost &post, bool liked) {
    if (networkClient == nullptr)
      return;

    if (liked) {
      networkClient->likePost(post.id, "", [](Outcome<juce::var> result) {
        if (result.isError())
          Log::error("SavedPosts: Like failed: " + result.getError());
      });
    } else {
      networkClient->unlikePost(post.id, [](Outcome<juce::var> result) {
        if (result.isError())
          Log::error("SavedPosts: Unlike failed: " + result.getError());
      });
    }
  };
}
