#include "SoundPage.h"
#include "../../network/NetworkClient.h"
#include "../../util/Result.h"
#include "../../util/Log.h"

//==============================================================================
SoundPage::SoundPage(Sidechain::Stores::AppStore *store) : AppStoreComponent(store) {
  Log::info("SoundPage: Initializing");

  addAndMakeVisible(scrollBar);
  scrollBar.addListener(this);
  scrollBar.setRangeLimits(0.0, 1.0);
  initialize();
}

SoundPage::~SoundPage() {
  Log::debug("SoundPage: Destroying");
  scrollBar.removeListener(this);
  // AppStoreComponent destructor will handle unsubscribe
}

//==============================================================================
// AppStoreComponent virtual methods

void SoundPage::onAppStateChanged(const Sidechain::Stores::SoundState &state) {
  isLoading = state.isLoading || state.isRefreshing;
  errorMessage = state.soundError;

  // Update sound data from state if available
  if (state.soundData.isObject() && !soundId.isEmpty()) {
    // Check if this is the sound we're displaying
    auto id = state.soundData.getProperty("id", "").toString();
    if (id == soundId) {
      sound = Sound::fromJson(state.soundData);
      loadCreatorAvatar();
    }
  }

  Log::debug("SoundPage: Store state changed - isLoading: " + juce::String(static_cast<int>(isLoading)));
  repaint();
}

void SoundPage::subscribeToAppStore() {
  if (!appStore) {
    Log::warn("SoundPage: Cannot subscribe - AppStore is null");
    return;
  }

  Log::debug("SoundPage: Subscribing to AppStore sounds state");

  // Subscribe to sounds state changes
  juce::Component::SafePointer<SoundPage> safeThis(this);
  storeUnsubscriber = appStore->subscribeToSounds([safeThis](const Sidechain::Stores::SoundState &state) {
    if (!safeThis)
      return;

    juce::MessageManager::callAsync([safeThis, state]() {
      if (safeThis)
        safeThis->onAppStateChanged(state);
    });
  });
}

//==============================================================================
void SoundPage::paint(juce::Graphics &g) {
  g.fillAll(Colors::background);

  drawHeader(g);

  auto contentBounds = getContentBounds();

  if (isLoading) {
    drawLoadingState(g, contentBounds);
    return;
  }

  if (errorMessage.isNotEmpty()) {
    drawErrorState(g, contentBounds);
    return;
  }

  if (sound.id.isEmpty()) {
    drawEmptyState(g, contentBounds);
    return;
  }

  // Draw sound info section
  auto infoBounds = contentBounds.removeFromTop(SOUND_INFO_HEIGHT);
  drawSoundInfo(g, infoBounds);

  // Draw separator
  g.setColour(juce::Colour(0xff3a3a3e));
  g.fillRect(contentBounds.removeFromTop(1));

  contentBounds.removeFromTop(PADDING);

  // Draw section header
  g.setColour(Colors::textPrimary);
  g.setFont(juce::FontOptions().withHeight(16.0f).withStyle("Bold"));
  g.drawText("Posts with this sound", contentBounds.removeFromTop(24).reduced(PADDING, 0),
             juce::Justification::centredLeft);

  contentBounds.removeFromTop(8);

  // Draw posts
  if (posts.isEmpty()) {
    drawEmptyState(g, contentBounds);
  } else {
    // Apply scroll offset
    g.saveState();
    g.reduceClipRegion(contentBounds);
    g.addTransform(juce::AffineTransform::translation(0.0f, static_cast<float>(-scrollOffset)));

    for (int i = 0; i < posts.size(); ++i) {
      auto cardBounds = getPostCardBounds(i);
      if (cardBounds.getBottom() - scrollOffset >= contentBounds.getY() &&
          cardBounds.getY() - scrollOffset < contentBounds.getBottom()) {
        drawPostCard(g, cardBounds, posts[i], i);
      }
    }

    g.restoreState();
  }
}

void SoundPage::resized() {
  auto bounds = getLocalBounds();

  // Scroll bar on the right
  scrollBar.setBounds(bounds.removeFromRight(8));

  updateScrollBounds();
}

void SoundPage::mouseUp(const juce::MouseEvent &event) {
  auto pos = event.getPosition();

  // Back button
  if (getBackButtonBounds().contains(pos)) {
    if (onBackPressed)
      onBackPressed();
    return;
  }

  // Creator area
  if (getCreatorBounds().contains(pos) && sound.creatorId.isNotEmpty()) {
    if (onUserSelected)
      onUserSelected(sound.creatorId);
    return;
  }

  // Post cards
  for (int i = 0; i < posts.size(); ++i) {
    auto cardBounds = getPostCardBounds(i);
    cardBounds.translate(0, -scrollOffset);

    if (cardBounds.contains(pos)) {
      // Check play button
      auto playBounds = getPostPlayButtonBounds(i);
      playBounds.translate(0, -scrollOffset);

      if (playBounds.contains(pos)) {
        // Convert SoundPost to FeedPost for playback
        FeedPost feedPost;
        feedPost.id = posts[i].id;
        feedPost.audioUrl = posts[i].audioUrl;
        feedPost.durationSeconds = posts[i].duration;

        if (currentlyPlayingPostId == posts[i].id) {
          if (onPausePost)
            onPausePost(feedPost);
        } else {
          if (onPlayPost)
            onPlayPost(feedPost);
        }
        return;
      }

      // Check user area
      auto userBounds = getPostUserBounds(i);
      userBounds.translate(0, -scrollOffset);

      if (userBounds.contains(pos)) {
        if (onUserSelected)
          onUserSelected(posts[i].userId);
        return;
      }

      // Clicked on post card - navigate to post
      if (onPostSelected)
        onPostSelected(posts[i].id);
      return;
    }
  }
}

void SoundPage::mouseWheelMove([[maybe_unused]] const juce::MouseEvent &event, const juce::MouseWheelDetails &wheel) {
  int delta = static_cast<int>(wheel.deltaY * 200.0f);
  int newOffset =
      juce::jlimit(0, juce::jmax(0, calculateContentHeight() - getContentBounds().getHeight()), scrollOffset - delta);

  if (newOffset != scrollOffset) {
    scrollOffset = newOffset;
    scrollBar.setCurrentRange(scrollOffset, getContentBounds().getHeight());
    repaint();
  }
}

void SoundPage::scrollBarMoved(juce::ScrollBar *bar, double newRangeStart) {
  if (bar == &scrollBar) {
    scrollOffset = static_cast<int>(newRangeStart);
    repaint();
  }
}

//==============================================================================
void SoundPage::setNetworkClient(NetworkClient *client) {
  networkClient = client;
}

void SoundPage::loadSound(const juce::String &id) {
  soundId = id;
  sound = Sound();
  posts.clear();
  errorMessage.clear();
  scrollOffset = 0;

  fetchSound();
}

void SoundPage::loadSoundForPost(const juce::String &postId) {
  if (networkClient == nullptr) {
    errorMessage = "Network client not available";
    repaint();
    return;
  }

  isLoading = true;
  repaint();

  // First get the sound for this post
  networkClient->getSoundForPost(postId, [this](Outcome<juce::var> result) {
    juce::MessageManager::callAsync([this, result]() {
      if (result.isOk()) {
        auto response = result.getValue();
        sound = Sound::fromJson(response);
        soundId = sound.id;

        // Now fetch the posts using this sound
        fetchSoundPosts();
      } else {
        isLoading = false;
        errorMessage = result.getError();
        repaint();
      }
    });
  });
}

void SoundPage::refresh() {
  if (soundId.isNotEmpty()) {
    fetchSound();
  }
}

//==============================================================================
void SoundPage::setCurrentlyPlayingPost(const juce::String &postId) {
  currentlyPlayingPostId = postId;
  repaint();
}

void SoundPage::setPlaybackProgress(float progress) {
  playbackProgress = progress;
  repaint();
}

void SoundPage::clearPlayingState() {
  currentlyPlayingPostId.clear();
  playbackProgress = 0.0f;
  repaint();
}

//==============================================================================
void SoundPage::fetchSound() {
  if (networkClient == nullptr || soundId.isEmpty()) {
    errorMessage = "Cannot fetch sound";
    repaint();
    return;
  }

  isLoading = true;
  repaint();

  networkClient->getSound(soundId, [this](Outcome<juce::var> result) {
    juce::MessageManager::callAsync([this, result]() {
      if (result.isOk()) {
        auto response = result.getValue();
        sound = Sound::fromJson(response);
        loadCreatorAvatar();

        // Now fetch the posts
        fetchSoundPosts();
      } else {
        isLoading = false;
        errorMessage = result.getError();
        repaint();
      }
    });
  });
}

void SoundPage::fetchSoundPosts() {
  if (networkClient == nullptr || soundId.isEmpty())
    return;

  networkClient->getSoundPosts(soundId, 50, 0, [this](Outcome<juce::var> result) {
    juce::MessageManager::callAsync([this, result]() {
      isLoading = false;

      if (result.isOk()) {
        auto response = result.getValue();
        auto postsArray = response.getProperty("posts", juce::var());

        posts.clear();
        if (postsArray.isArray()) {
          for (int i = 0; i < postsArray.size(); ++i) {
            posts.add(SoundPost::fromJson(postsArray[i]));
          }
        }

        updateScrollBounds();
      } else {
        errorMessage = result.getError();
      }

      repaint();
    });
  });
}

void SoundPage::loadCreatorAvatar() {
  if (sound.creatorAvatarUrl.isEmpty() || networkClient == nullptr)
    return;

  // Load avatar image asynchronously
  // For now, we'll skip this - would need image loading infrastructure
}

//==============================================================================
void SoundPage::drawHeader(juce::Graphics &g) {
  auto bounds = getLocalBounds().removeFromTop(HEADER_HEIGHT);

  // Background
  g.setColour(Colors::cardBg);
  g.fillRect(bounds);

  // Bottom border
  g.setColour(juce::Colour(0xff3a3a3e));
  g.fillRect(bounds.removeFromBottom(1));

  // Back button
  auto backBounds = getBackButtonBounds();
  g.setColour(Colors::accent);
  g.setFont(juce::FontOptions().withHeight(20.0f));
  g.drawText(juce::CharPointer_UTF8("\xe2\x86\x90"), backBounds,
             juce::Justification::centred); // Left arrow

  // Title
  g.setColour(Colors::textPrimary);
  g.setFont(juce::FontOptions().withHeight(18.0f).withStyle("Bold"));
  g.drawText("Sound", bounds.reduced(60, 0), juce::Justification::centred);
}

void SoundPage::drawSoundInfo(juce::Graphics &g, juce::Rectangle<int> &bounds) {
  bounds = bounds.reduced(PADDING);

  // Sound icon and name row
  auto nameRow = bounds.removeFromTop(40);

  // Sound icon (musical note)
  auto iconBounds = nameRow.removeFromLeft(40);
  g.setColour(Colors::soundIcon);
  g.setFont(juce::FontOptions().withHeight(28.0f));
  g.drawText(juce::CharPointer_UTF8("\xe2\x99\xab"), iconBounds,
             juce::Justification::centred); // Musical note

  nameRow.removeFromLeft(12);

  // Sound name
  g.setColour(Colors::textPrimary);
  g.setFont(juce::FontOptions().withHeight(20.0f).withStyle("Bold"));
  g.drawText(sound.name.isNotEmpty() ? sound.name : "Untitled Sound", nameRow, juce::Justification::centredLeft);

  bounds.removeFromTop(8);

  // Creator info
  auto creatorRow = bounds.removeFromTop(24);

  g.setColour(Colors::textSecondary);
  g.setFont(juce::FontOptions().withHeight(14.0f));
  g.drawText("by ", creatorRow.removeFromLeft(20), juce::Justification::centredLeft);

  g.setColour(Colors::accent);
  g.drawText(sound.getCreatorName().isNotEmpty() ? sound.getCreatorName() : "Unknown", creatorRow,
             juce::Justification::centredLeft);

  bounds.removeFromTop(16);

  // Stats row
  auto statsRow = bounds.removeFromTop(32);

  // Usage count badge
  auto usageBounds = statsRow.removeFromLeft(120);
  g.setColour(Colors::cardBg.brighter(0.1f));
  g.fillRoundedRectangle(usageBounds.toFloat(), 6.0f);

  g.setColour(Colors::textPrimary);
  g.setFont(juce::FontOptions().withHeight(14.0f).withStyle("Bold"));
  g.drawText(sound.getUsageCountString(), usageBounds, juce::Justification::centred);

  statsRow.removeFromLeft(12);

  // Duration badge
  if (sound.duration > 0) {
    auto durationBounds = statsRow.removeFromLeft(70);
    g.setColour(Colors::cardBg.brighter(0.1f));
    g.fillRoundedRectangle(durationBounds.toFloat(), 6.0f);

    g.setColour(Colors::textSecondary);
    g.setFont(juce::FontOptions().withHeight(14.0f));
    g.drawText(sound.getDurationString(), durationBounds, juce::Justification::centred);
  }

  // Trending badge
  if (sound.isTrending) {
    statsRow.removeFromLeft(12);
    auto trendingBounds = statsRow.removeFromLeft(90);
    g.setColour(Colors::trendingBadge.withAlpha(0.2f));
    g.fillRoundedRectangle(trendingBounds.toFloat(), 6.0f);

    g.setColour(Colors::trendingBadge);
    g.setFont(juce::FontOptions().withHeight(13.0f).withStyle("Bold"));
    g.drawText(juce::CharPointer_UTF8("\xf0\x9f\x94\xa5 Trending"), trendingBounds, juce::Justification::centred);
  }
}

void SoundPage::drawPostCard(juce::Graphics &g, juce::Rectangle<int> bounds, const SoundPost &post,
                             [[maybe_unused]] int index) {
  bool isPlaying = (post.id == currentlyPlayingPostId);

  // Card background
  g.setColour(isPlaying ? Colors::cardBgHover : Colors::cardBg);
  g.fillRoundedRectangle(bounds.toFloat(), 8.0f);

  // Playback progress bar (if playing)
  if (isPlaying && playbackProgress > 0) {
    auto progressBounds = bounds.withHeight(3);
    g.setColour(Colors::accent.withAlpha(0.3f));
    g.fillRect(progressBounds);
    g.setColour(Colors::accent);
    g.fillRect(
        progressBounds.withWidth(static_cast<int>(static_cast<float>(progressBounds.getWidth()) * playbackProgress)));
  }

  auto cardContent = bounds.reduced(12);

  // Play button
  auto playBounds = cardContent.removeFromLeft(50);
  playBounds = playBounds.withSizeKeepingCentre(44, 44);

  g.setColour(Colors::playButton);
  g.fillEllipse(playBounds.toFloat());

  g.setColour(Colors::cardBg);
  g.setFont(juce::FontOptions().withHeight(18.0f));
  g.drawText(isPlaying ? juce::CharPointer_UTF8("\xe2\x8f\xb8") : juce::CharPointer_UTF8("\xe2\x96\xb6"), playBounds,
             juce::Justification::centred);

  cardContent.removeFromLeft(12);

  // User info
  auto userRow = cardContent.removeFromTop(cardContent.getHeight() / 2);

  g.setColour(Colors::textPrimary);
  g.setFont(juce::FontOptions().withHeight(14.0f).withStyle("Bold"));
  g.drawText(post.getUserDisplayName(), userRow, juce::Justification::centredLeft);

  // Post metadata
  juce::String metadata;
  if (post.bpm > 0)
    metadata += juce::String(post.bpm) + " BPM";
  if (post.key.isNotEmpty()) {
    if (metadata.isNotEmpty())
      metadata += " | ";
    metadata += post.key;
  }

  g.setColour(Colors::textSecondary);
  g.setFont(juce::FontOptions().withHeight(12.0f));
  g.drawText(metadata, cardContent, juce::Justification::centredLeft);

  // Stats on right side
  auto statsBounds = bounds.reduced(12).removeFromRight(80);

  g.setColour(Colors::textSecondary);
  g.setFont(juce::FontOptions().withHeight(12.0f));

  juce::String likesText =
      juce::String(juce::CharPointer_UTF8("\xe2\x9d\xa4\xef\xb8\x8f ")) + juce::String(post.likeCount);
  g.drawText(likesText, statsBounds.removeFromTop(statsBounds.getHeight() / 2), juce::Justification::centredRight);

  juce::String playsText = juce::String(juce::CharPointer_UTF8("\xe2\x96\xb6 ")) + juce::String(post.playCount);
  g.drawText(playsText, statsBounds, juce::Justification::centredRight);
}

void SoundPage::drawLoadingState(juce::Graphics &g, juce::Rectangle<int> bounds) {
  g.setColour(Colors::textSecondary);
  g.setFont(juce::FontOptions().withHeight(16.0f));
  g.drawText("Loading sound...", bounds, juce::Justification::centred);
}

void SoundPage::drawErrorState(juce::Graphics &g, juce::Rectangle<int> bounds) {
  g.setColour(juce::Colour(0xffff4757));
  g.setFont(juce::FontOptions().withHeight(16.0f));
  g.drawText(errorMessage, bounds, juce::Justification::centred);
}

void SoundPage::drawEmptyState(juce::Graphics &g, juce::Rectangle<int> bounds) {
  g.setColour(Colors::textSecondary);
  g.setFont(juce::FontOptions().withHeight(16.0f));
  g.drawText("No posts found with this sound", bounds, juce::Justification::centred);
}

//==============================================================================
juce::Rectangle<int> SoundPage::getBackButtonBounds() const {
  return juce::Rectangle<int>(8, 8, 44, 44);
}

juce::Rectangle<int> SoundPage::getCreatorBounds() const {
  return juce::Rectangle<int>(PADDING, HEADER_HEIGHT + 48, 200, 24);
}

juce::Rectangle<int> SoundPage::getContentBounds() const {
  auto bounds = getLocalBounds();
  bounds.removeFromTop(HEADER_HEIGHT);
  bounds.removeFromRight(scrollBar.getWidth());
  return bounds;
}

juce::Rectangle<int> SoundPage::getPostCardBounds(int index) const {
  auto contentBounds = getContentBounds();
  contentBounds.removeFromTop(SOUND_INFO_HEIGHT + 1 + PADDING + 24 + 8); // Info + separator + header

  int y = contentBounds.getY() + index * (POST_CARD_HEIGHT + 8);
  return juce::Rectangle<int>(PADDING, y, contentBounds.getWidth() - PADDING * 2, POST_CARD_HEIGHT);
}

juce::Rectangle<int> SoundPage::getPostPlayButtonBounds(int index) const {
  auto cardBounds = getPostCardBounds(index);
  return juce::Rectangle<int>(cardBounds.getX() + 12, cardBounds.getCentreY() - 22, 44, 44);
}

juce::Rectangle<int> SoundPage::getPostUserBounds(int index) const {
  auto cardBounds = getPostCardBounds(index);
  return juce::Rectangle<int>(cardBounds.getX() + 74, cardBounds.getY(), 200, cardBounds.getHeight() / 2);
}

//==============================================================================
int SoundPage::calculateContentHeight() const {
  int height = SOUND_INFO_HEIGHT + 1 + PADDING + 24 + 8; // Info + separator + header
  height += posts.size() * (POST_CARD_HEIGHT + 8);
  height += PADDING; // Bottom padding
  return height;
}

void SoundPage::updateScrollBounds() {
  int totalHeight = calculateContentHeight();
  int visibleHeight = getContentBounds().getHeight();

  scrollBar.setRangeLimits(0, juce::jmax(0, totalHeight));
  scrollBar.setCurrentRange(scrollOffset, visibleHeight);
}
