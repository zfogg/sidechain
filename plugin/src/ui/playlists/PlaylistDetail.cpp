#include "PlaylistDetail.h"
#include "../../network/NetworkClient.h"
#include "../../util/Log.h"
#include "../../util/Result.h"

//==============================================================================
PlaylistDetail::PlaylistDetail(Sidechain::Stores::AppStore *store) : AppStoreComponent(store) {
  Log::info("PlaylistDetailComponent: Initializing");

  // Set up scroll bar
  scrollBar.setRangeLimits(0.0, 100.0);
  scrollBar.addListener(this);
  addAndMakeVisible(scrollBar);
  initialize();
}

PlaylistDetail::~PlaylistDetail() {
  Log::debug("PlaylistDetailComponent: Destroying");
  scrollBar.removeListener(this);
}

//==============================================================================
void PlaylistDetail::onAppStateChanged(const Sidechain::Stores::PlaylistState &state) {
  // Find the current playlist in the state
  for (int i = 0; i < state.playlists.size(); ++i) {
    const auto &playlistVar = state.playlists[i];
    if (playlistVar.getProperty("id", "").toString() == playlistId) {
      playlist = Playlist::fromJSON(playlistVar);
      // Note: We still need to fetch full playlist details including entries
      // from NetworkClient, as state may only contain basic info
      break;
    }
  }

  isLoading = state.isLoading;
  errorMessage = state.playlistError;

  updateScrollBounds();
  repaint();
}

void PlaylistDetail::subscribeToAppStore() {
  if (!appStore)
    return;

  juce::Component::SafePointer<PlaylistDetail> safeThis(this);
  storeUnsubscriber = appStore->subscribeToPlaylists([safeThis](const Sidechain::Stores::PlaylistState &state) {
    if (!safeThis)
      return;
    juce::MessageManager::callAsync([safeThis, state]() {
      if (safeThis)
        safeThis->onAppStateChanged(state);
    });
  });
}

//==============================================================================
void PlaylistDetail::setNetworkClient(NetworkClient *client) {
  networkClient = client;
  Log::debug("PlaylistDetailComponent: NetworkClient set " + juce::String(client != nullptr ? "(valid)" : "(null)"));
}

//==============================================================================
void PlaylistDetail::paint(juce::Graphics &g) {
  // Background
  g.fillAll(SidechainColors::background());

  // Header
  drawHeader(g);

  // Content area
  auto contentBounds = getContentBounds();
  contentBounds.translate(0, -scrollOffset);

  if (isLoading) {
    drawLoadingState(g, contentBounds);
    return;
  }

  if (errorMessage.isNotEmpty()) {
    drawErrorState(g, contentBounds);
    return;
  }

  // Playlist info
  drawPlaylistInfo(g, contentBounds);

  // Action buttons
  drawActionButtons(g, contentBounds);

  // Entries list
  if (entries.isEmpty()) {
    drawEmptyState(g, contentBounds);
  } else {
    for (int i = 0; i < entries.size(); ++i) {
      auto cardBounds = getEntryCardBounds(i);
      cardBounds.translate(0, -scrollOffset);
      if (cardBounds.getBottom() >= 0 && cardBounds.getY() < getHeight()) {
        drawEntryCard(g, cardBounds, entries[i], i);
      }
    }
  }
}

void PlaylistDetail::resized() {
  updateScrollBounds();

  // Position scroll bar
  scrollBar.setBounds(getWidth() - 12, HEADER_HEIGHT, 12, getHeight() - HEADER_HEIGHT);
}

void PlaylistDetail::mouseUp(const juce::MouseEvent &event) {
  auto pos = event.getPosition();

  // Back button
  if (getBackButtonBounds().contains(pos)) {
    if (onBackPressed)
      onBackPressed();
    return;
  }

  // Play button
  if (getPlayButtonBounds().contains(pos)) {
    if (onPlayPlaylist)
      onPlayPlaylist();
    return;
  }

  // Add track button
  if (canEdit() && getAddTrackButtonBounds().contains(pos)) {
    if (onAddTrack)
      onAddTrack();
    return;
  }

  // Share button
  if (getShareButtonBounds().contains(pos)) {
    if (onSharePlaylist && playlistId.isNotEmpty())
      onSharePlaylist(playlistId);
    return;
  }

  // Entry cards
  for (int i = 0; i < entries.size(); ++i) {
    auto cardBounds = getEntryCardBounds(i);
    cardBounds.translate(0, -scrollOffset);
    if (cardBounds.contains(pos)) {
      // Check if remove button was clicked
      auto removeBounds = getRemoveEntryButtonBounds(i);
      removeBounds.translate(0, -scrollOffset);
      if (canEdit() && removeBounds.contains(pos)) {
        removeEntry(entries[i].id);
        return;
      }

      // Otherwise, navigate to post
      if (onPostSelected)
        onPostSelected(entries[i].postId);
      return;
    }
  }
}

void PlaylistDetail::mouseWheelMove(const juce::MouseEvent & /*event*/, const juce::MouseWheelDetails &wheel) {
  float delta = wheel.deltaY;
  scrollOffset = juce::jmax(0, scrollOffset - static_cast<int>(delta * 30.0f));
  updateScrollBounds();
  repaint();
}

void PlaylistDetail::scrollBarMoved(juce::ScrollBar * /*scrollBar*/, double newRangeStart) {
  scrollOffset = static_cast<int>(newRangeStart);
  repaint();
}

//==============================================================================
void PlaylistDetail::loadPlaylist(const juce::String &id) {
  playlistId = id;
  fetchPlaylist();
}

void PlaylistDetail::refresh() {
  if (playlistId.isNotEmpty())
    fetchPlaylist();
}

//==============================================================================
void PlaylistDetail::fetchPlaylist() {
  if (!networkClient || playlistId.isEmpty()) {
    Log::warn("PlaylistDetailComponent: No network client or playlist ID");
    return;
  }

  isLoading = true;
  errorMessage = "";
  repaint();

  networkClient->getPlaylist(playlistId, [this](Outcome<juce::var> result) {
    juce::MessageManager::callAsync([this, result]() {
      isLoading = false;

      if (!result.isOk()) {
        errorMessage = "Failed to load playlist: " + result.getError();
        Log::warn("PlaylistDetailComponent: " + errorMessage);
        repaint();
        return;
      }

      auto response = result.getValue();
      playlist = Playlist::fromJSON(response);

      // Parse entries
      entries.clear();
      if (response.hasProperty("entries")) {
        auto entriesArray = response["entries"];
        if (entriesArray.isArray()) {
          for (int i = 0; i < entriesArray.size(); ++i) {
            entries.add(PlaylistEntry::fromJSON(entriesArray[i]));
          }
        }
      }

      // Parse collaborators
      collaborators.clear();
      if (response.hasProperty("collaborators")) {
        auto collabsArray = response["collaborators"];
        if (collabsArray.isArray()) {
          for (int i = 0; i < collabsArray.size(); ++i) {
            collaborators.add(PlaylistCollaborator::fromJSON(collabsArray[i]));
          }
        }
      }

      // Determine user role
      if (playlist.ownerId == currentUserId)
        playlist.userRole = "owner";
      else {
        for (const auto &collab : collaborators) {
          if (collab.userId == currentUserId) {
            playlist.userRole = collab.role;
            break;
          }
        }
      }

      Log::info("PlaylistDetailComponent: Loaded playlist with " + juce::String(entries.size()) + " entries");
      updateScrollBounds();
      repaint();
    });
  });
}

void PlaylistDetail::removeEntry(const juce::String &entryId) {
  if (!networkClient || !canEdit())
    return;

  networkClient->removePlaylistEntry(playlistId, entryId, [this](Outcome<juce::var> result) {
    juce::MessageManager::callAsync([this, result]() {
      if (result.isOk()) {
        Log::info("PlaylistDetailComponent: Entry removed");
        refresh(); // Reload playlist
      } else {
        Log::error("PlaylistDetailComponent: Failed to remove entry: " + result.getError());
      }
    });
  });
}

//==============================================================================
void PlaylistDetail::drawHeader(juce::Graphics &g) {
  auto bounds = juce::Rectangle<int>(0, 0, getWidth(), HEADER_HEIGHT);

  // Background
  g.setColour(SidechainColors::surface());
  g.fillRect(bounds);

  // Title
  g.setColour(SidechainColors::textPrimary());
  g.setFont(juce::Font(juce::FontOptions().withHeight(20.0f)).boldened());
  g.drawText("Playlist", bounds.removeFromLeft(getWidth() - 100), juce::Justification::centredLeft);

  // Back button
  auto backBounds = getBackButtonBounds();
  g.setColour(SidechainColors::textPrimary());
  g.setFont(16.0f);
  g.drawText("←", backBounds, juce::Justification::centred);
}

void PlaylistDetail::drawPlaylistInfo(juce::Graphics &g, juce::Rectangle<int> &bounds) {
  auto infoBounds = bounds.removeFromTop(INFO_HEIGHT).reduced(PADDING, 0);

  // Playlist name
  g.setColour(SidechainColors::textPrimary());
  g.setFont(juce::Font(juce::FontOptions().withHeight(24.0f)).boldened());
  auto nameBounds = infoBounds.removeFromTop(32);
  g.drawText(playlist.name, nameBounds, juce::Justification::centredLeft);

  // Description
  if (playlist.description.isNotEmpty()) {
    g.setColour(SidechainColors::textSecondary());
    g.setFont(14.0f);
    auto descBounds = infoBounds.removeFromTop(40);
    g.drawText(playlist.description, descBounds, juce::Justification::centredLeft, true);
  }

  // Metadata
  g.setColour(SidechainColors::textSecondary());
  g.setFont(12.0f);
  juce::String meta = juce::String(entries.size()) + " track" + (entries.size() != 1 ? "s" : "");
  if (playlist.isCollaborative)
    meta += " • Collaborative";
  if (playlist.isPublic)
    meta += " • Public";
  g.drawText(meta, infoBounds.removeFromTop(20), juce::Justification::centredLeft);
}

void PlaylistDetail::drawActionButtons(juce::Graphics &g, juce::Rectangle<int> &bounds) {
  auto buttonsBounds = bounds.removeFromTop(BUTTON_HEIGHT + 8).reduced(PADDING, 0);

  // Layout buttons dynamically within provided bounds
  // Calculate button sizes proportionally
  const int buttonGap = 8;
  const int numButtons = canEdit() ? 3 : 2; // Play, Add Track (if can edit), Share
  const int totalGapWidth = buttonGap * (numButtons - 1);
  const int totalButtonWidth = buttonsBounds.getWidth() - totalGapWidth;
  const int buttonWidth = totalButtonWidth / numButtons;

  // Play button (always first, highlighted primary action)
  auto playBounds = buttonsBounds.removeFromLeft(buttonWidth);
  bool playHovered = playBounds.contains(getMouseXYRelative());
  g.setColour(playHovered ? SidechainColors::coralPink().brighter(0.2f) : SidechainColors::coralPink());
  g.fillRoundedRectangle(playBounds.toFloat(), 8.0f);
  g.setColour(SidechainColors::textPrimary());
  g.setFont(juce::Font(juce::FontOptions().withHeight(14.0f)).boldened());
  g.drawText("▶ Play All", playBounds, juce::Justification::centred);
  buttonsBounds.removeFromLeft(buttonGap);

  // Add track button (if can edit)
  if (canEdit()) {
    auto addBounds = buttonsBounds.removeFromLeft(buttonWidth);
    bool addHovered = addBounds.contains(getMouseXYRelative());
    g.setColour(addHovered ? SidechainColors::surface().brighter(0.1f) : SidechainColors::surface());
    g.fillRoundedRectangle(addBounds.toFloat(), 8.0f);
    g.setColour(SidechainColors::border());
    g.drawRoundedRectangle(addBounds.toFloat(), 8.0f, 1.0f);
    g.setColour(SidechainColors::textPrimary());
    g.setFont(14.0f);
    g.drawText("+ Add Track", addBounds, juce::Justification::centred);
    buttonsBounds.removeFromLeft(buttonGap);
  }

  // Share button (takes remaining space)
  auto shareBounds = buttonsBounds;
  bool shareHovered = shareBounds.contains(getMouseXYRelative());
  g.setColour(shareHovered ? SidechainColors::surface().brighter(0.1f) : SidechainColors::surface());
  g.fillRoundedRectangle(shareBounds.toFloat(), 8.0f);
  g.setColour(SidechainColors::border());
  g.drawRoundedRectangle(shareBounds.toFloat(), 8.0f, 1.0f);
  g.setColour(SidechainColors::textPrimary());
  g.setFont(14.0f);
  g.drawText("Share", shareBounds, juce::Justification::centred);
}

void PlaylistDetail::drawEntryCard(juce::Graphics &g, juce::Rectangle<int> bounds, const PlaylistEntry &entry,
                                   int index) {
  bounds = bounds.reduced(PADDING, 4);

  bool isHovered = bounds.contains(getMouseXYRelative());
  g.setColour(isHovered ? SidechainColors::surface().brighter(0.1f) : SidechainColors::surface());
  g.fillRoundedRectangle(bounds.toFloat(), 8.0f);

  // Border
  g.setColour(SidechainColors::border());
  g.drawRoundedRectangle(bounds.toFloat(), 8.0f, 1.0f);

  // Position number
  g.setColour(SidechainColors::textSecondary());
  g.setFont(12.0f);
  auto posBounds = bounds.removeFromLeft(30);
  g.drawText(juce::String(index + 1), posBounds, juce::Justification::centred);

  // Post info
  auto contentBounds = bounds.reduced(8, 0);
  g.setColour(SidechainColors::textPrimary());
  g.setFont(juce::Font(juce::FontOptions().withHeight(14.0f)).boldened());
  auto titleBounds = contentBounds.removeFromTop(20);
  juce::String title = entry.postUsername.isNotEmpty() ? entry.postUsername + "'s track" : "Track";
  g.drawText(title, titleBounds, juce::Justification::centredLeft);

  // Metadata
  g.setColour(SidechainColors::textSecondary());
  g.setFont(11.0f);
  juce::String meta;
  if (entry.postBpm > 0)
    meta += juce::String(entry.postBpm) + " BPM";
  if (entry.postKey.isNotEmpty()) {
    if (meta.isNotEmpty())
      meta += " • ";
    meta += entry.postKey;
  }
  if (meta.isNotEmpty())
    g.drawText(meta, contentBounds.removeFromTop(16), juce::Justification::centredLeft);

  // Remove button (if can edit)
  if (canEdit()) {
    auto removeBounds = getRemoveEntryButtonBounds(index);
    removeBounds.translate(-scrollOffset, 0);
    bool removeHovered = removeBounds.contains(getMouseXYRelative());
    g.setColour(removeHovered ? SidechainColors::error() : SidechainColors::textSecondary());
    g.setFont(12.0f);
    g.drawText(juce::CharPointer_UTF8("\xc3\x97"), removeBounds,
               juce::Justification::centred); // ×
  }
}

void PlaylistDetail::drawLoadingState(juce::Graphics &g, juce::Rectangle<int> bounds) {
  g.setColour(SidechainColors::textSecondary());
  g.setFont(14.0f);
  g.drawText("Loading playlist...", bounds, juce::Justification::centred);
}

void PlaylistDetail::drawErrorState(juce::Graphics &g, juce::Rectangle<int> bounds) {
  g.setColour(SidechainColors::error());
  g.setFont(14.0f);
  g.drawText(errorMessage, bounds, juce::Justification::centred);
}

void PlaylistDetail::drawEmptyState(juce::Graphics &g, juce::Rectangle<int> bounds) {
  g.setColour(SidechainColors::textSecondary());
  g.setFont(14.0f);
  g.drawText("This playlist is empty.\nAdd tracks to get started!", bounds, juce::Justification::centred);
}

//==============================================================================
juce::Rectangle<int> PlaylistDetail::getBackButtonBounds() const {
  return juce::Rectangle<int>(PADDING, 0, 50, HEADER_HEIGHT);
}

juce::Rectangle<int> PlaylistDetail::getPlayButtonBounds() const {
  auto contentBounds = getContentBounds();
  contentBounds.removeFromTop(INFO_HEIGHT);
  auto buttonsBounds = contentBounds.removeFromTop(BUTTON_HEIGHT + 8).reduced(PADDING, 0);
  return buttonsBounds.removeFromLeft(buttonsBounds.getWidth() / 2 - 4);
}

juce::Rectangle<int> PlaylistDetail::getAddTrackButtonBounds() const {
  auto contentBounds = getContentBounds();
  contentBounds.removeFromTop(INFO_HEIGHT);
  auto buttonsBounds = contentBounds.removeFromTop(BUTTON_HEIGHT + 8).reduced(PADDING, 0);
  buttonsBounds.removeFromLeft(buttonsBounds.getWidth() / 2 + 4);
  if (canEdit())
    return buttonsBounds.removeFromLeft(buttonsBounds.getWidth() / 2 - 2);
  return juce::Rectangle<int>(); // Not visible if can't edit
}

juce::Rectangle<int> PlaylistDetail::getShareButtonBounds() const {
  auto contentBounds = getContentBounds();
  contentBounds.removeFromTop(INFO_HEIGHT);
  auto buttonsBounds = contentBounds.removeFromTop(BUTTON_HEIGHT + 8).reduced(PADDING, 0);
  buttonsBounds.removeFromLeft(buttonsBounds.getWidth() / 2 + 4);
  if (canEdit())
    buttonsBounds.removeFromLeft(buttonsBounds.getWidth() / 2 + 4);
  return buttonsBounds.removeFromLeft(buttonsBounds.getWidth());
}

juce::Rectangle<int> PlaylistDetail::getContentBounds() const {
  return juce::Rectangle<int>(0, HEADER_HEIGHT, getWidth(), getHeight() - HEADER_HEIGHT);
}

juce::Rectangle<int> PlaylistDetail::getEntryCardBounds(int index) const {
  auto contentBounds = getContentBounds();
  contentBounds.removeFromTop(INFO_HEIGHT + BUTTON_HEIGHT + 16);
  return contentBounds.removeFromTop(ENTRY_CARD_HEIGHT).translated(0, index * ENTRY_CARD_HEIGHT);
}

juce::Rectangle<int> PlaylistDetail::getRemoveEntryButtonBounds(int index) const {
  auto cardBounds = getEntryCardBounds(index);
  return cardBounds.removeFromRight(30).removeFromTop(30).reduced(5);
}

//==============================================================================
int PlaylistDetail::calculateContentHeight() const {
  int height = INFO_HEIGHT + BUTTON_HEIGHT + 16;
  height += entries.size() * ENTRY_CARD_HEIGHT;
  return height;
}

void PlaylistDetail::updateScrollBounds() {
  int contentHeight = calculateContentHeight();
  int viewportHeight = getContentBounds().getHeight();
  int maxScroll = juce::jmax(0, contentHeight - viewportHeight);

  scrollBar.setRangeLimits(0.0, static_cast<double>(maxScroll));
  scrollBar.setCurrentRange(scrollOffset, static_cast<double>(viewportHeight), juce::dontSendNotification);
  scrollBar.setVisible(maxScroll > 0);
}

bool PlaylistDetail::canEdit() const {
  return playlist.canEdit();
}
