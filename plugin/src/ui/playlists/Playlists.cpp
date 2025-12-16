#include "Playlists.h"
#include "../../util/Log.h"

//==============================================================================
Playlists::Playlists() {
  Log::info("PlaylistsComponent: Initializing");

  // Set up scroll bar
  scrollBar.setRangeLimits(0.0, 100.0);
  scrollBar.addListener(this);
  addAndMakeVisible(scrollBar);
}

Playlists::~Playlists() {
  Log::debug("PlaylistsComponent: Destroying");
  scrollBar.removeListener(this);
}

//==============================================================================
void Playlists::paint(juce::Graphics &g) {
  // Background
  g.fillAll(SidechainColors::background());

  // Header
  drawHeader(g);

  // Content area
  auto contentBounds = getContentBounds();
  contentBounds.translate(0, -scrollOffset);

  // Filter tabs
  drawFilterTabs(g, contentBounds);

  // Create button
  drawCreateButton(g, contentBounds);

  // Playlists list
  if (isLoading) {
    drawLoadingState(g, contentBounds);
  } else if (errorMessage.isNotEmpty()) {
    drawErrorState(g, contentBounds);
  } else if (playlists.isEmpty()) {
    drawEmptyState(g, contentBounds);
  } else {
    for (int i = 0; i < playlists.size(); ++i) {
      auto cardBounds = getPlaylistCardBounds(i);
      cardBounds.translate(0, -scrollOffset);
      if (cardBounds.getBottom() >= 0 && cardBounds.getY() < getHeight()) {
        drawPlaylistCard(g, cardBounds, playlists[i]);
      }
    }
  }
}

void Playlists::resized() {
  updateScrollBounds();

  // Position scroll bar
  scrollBar.setBounds(getWidth() - 12, HEADER_HEIGHT + FILTER_TAB_HEIGHT, 12,
                      getHeight() - HEADER_HEIGHT - FILTER_TAB_HEIGHT);
}

void Playlists::mouseUp(const juce::MouseEvent &event) {
  auto pos = event.getPosition();

  // Back button
  if (getBackButtonBounds().contains(pos)) {
    if (onBackPressed)
      onBackPressed();
    return;
  }

  // Filter tabs
  for (int i = 0; i < 4; ++i) {
    FilterType filter = static_cast<FilterType>(i);
    if (getFilterTabBounds(filter).contains(pos)) {
      if (currentFilter != filter) {
        currentFilter = filter;
        loadPlaylists();
        repaint();
      }
      return;
    }
  }

  // Create button
  if (getCreateButtonBounds().contains(pos)) {
    if (onCreatePlaylist)
      onCreatePlaylist();
    return;
  }

  // Playlist cards
  for (int i = 0; i < playlists.size(); ++i) {
    auto cardBounds = getPlaylistCardBounds(i);
    cardBounds.translate(0, -scrollOffset);
    if (cardBounds.contains(pos)) {
      if (onPlaylistSelected)
        onPlaylistSelected(playlists[i].id);
      return;
    }
  }
}

void Playlists::mouseWheelMove(const juce::MouseEvent & /*event*/, const juce::MouseWheelDetails &wheel) {
  float delta = wheel.deltaY;
  scrollOffset = juce::jmax(0, scrollOffset - static_cast<int>(delta * 30.0f));
  updateScrollBounds();
  repaint();
}

void Playlists::scrollBarMoved(juce::ScrollBar * /*scrollBar*/, double newRangeStart) {
  scrollOffset = static_cast<int>(newRangeStart);
  repaint();
}

//==============================================================================
void Playlists::loadPlaylists() {
  Log::debug("Playlists: Loading playlists from AppStore");

  // TODO: Apply filter to playlists based on currentFilter
  // For now, just load all playlists
  auto &appStore = Sidechain::Stores::AppStore::getInstance();
  appStore.loadPlaylists();
}

void Playlists::refresh() {
  Log::debug("Playlists: Refreshing playlists");
  auto &appStore = Sidechain::Stores::AppStore::getInstance();
  appStore.loadPlaylists();
}

//==============================================================================
void Playlists::drawHeader(juce::Graphics &g) {
  auto bounds = juce::Rectangle<int>(0, 0, getWidth(), HEADER_HEIGHT);

  // Background
  g.setColour(SidechainColors::surface());
  g.fillRect(bounds);

  // Title
  g.setColour(SidechainColors::textPrimary());
  g.setFont(juce::Font(juce::FontOptions().withHeight(20.0f)).boldened());
  g.drawText("Playlists", bounds.removeFromLeft(getWidth() - 100), juce::Justification::centredLeft);

  // Back button
  auto backBounds = getBackButtonBounds();
  g.setColour(SidechainColors::textPrimary());
  g.setFont(16.0f);
  g.drawText("←", backBounds, juce::Justification::centred);
}

void Playlists::drawFilterTabs(juce::Graphics &g, juce::Rectangle<int> &bounds) {
  auto tabsBounds = bounds.removeFromTop(FILTER_TAB_HEIGHT);

  juce::StringArray tabLabels = {"All", "Owned", "Collaborated", "Public"};
  int tabWidth = tabsBounds.getWidth() / 4;

  for (int i = 0; i < 4; ++i) {
    FilterType filter = static_cast<FilterType>(i);
    auto tabBounds = tabsBounds.removeFromLeft(tabWidth);

    bool isSelected = (currentFilter == filter);
    g.setColour(isSelected ? SidechainColors::coralPink() : SidechainColors::surface());
    g.fillRect(tabBounds);

    g.setColour(isSelected ? SidechainColors::textPrimary() : SidechainColors::textSecondary());
    g.setFont(14.0f);
    g.drawText(tabLabels[i], tabBounds, juce::Justification::centred);
  }
}

void Playlists::drawCreateButton(juce::Graphics &g, juce::Rectangle<int> &bounds) {
  auto buttonBounds = bounds.removeFromTop(CREATE_BUTTON_HEIGHT).reduced(PADDING, 8);

  bool isHovered = buttonBounds.contains(getMouseXYRelative());
  g.setColour(isHovered ? SidechainColors::coralPink().brighter(0.2f) : SidechainColors::coralPink());
  g.fillRoundedRectangle(buttonBounds.toFloat(), 8.0f);

  g.setColour(SidechainColors::textPrimary());
  g.setFont(16.0f);
  g.drawText("+ Create Playlist", buttonBounds, juce::Justification::centred);
}

void Playlists::drawPlaylistCard(juce::Graphics &g, juce::Rectangle<int> bounds, const Playlist &playlist) {
  bounds = bounds.reduced(PADDING, 8);

  bool isHovered = bounds.contains(getMouseXYRelative());
  g.setColour(isHovered ? SidechainColors::surface().brighter(0.1f) : SidechainColors::surface());
  g.fillRoundedRectangle(bounds.toFloat(), 8.0f);

  // Border
  g.setColour(SidechainColors::border());
  g.drawRoundedRectangle(bounds.toFloat(), 8.0f, 1.0f);

  // Playlist name
  g.setColour(SidechainColors::textPrimary());
  g.setFont(juce::Font(juce::FontOptions().withHeight(16.0f)).boldened());
  auto nameBounds = bounds.removeFromTop(24).reduced(12, 0);
  g.drawText(playlist.name, nameBounds, juce::Justification::centredLeft);

  // Description
  if (playlist.description.isNotEmpty()) {
    g.setColour(SidechainColors::textSecondary());
    g.setFont(12.0f);
    auto descBounds = bounds.removeFromTop(18).reduced(12, 0);
    g.drawText(playlist.description, descBounds, juce::Justification::centredLeft, true);
  }

  // Metadata (entry count, collaborative badge)
  auto metaBounds = bounds.reduced(12, 0);
  g.setColour(SidechainColors::textSecondary());
  g.setFont(11.0f);
  juce::String meta = juce::String(playlist.entryCount) + " track" + (playlist.entryCount != 1 ? "s" : "");
  if (playlist.isCollaborative)
    meta += " • Collaborative";
  g.drawText(meta, metaBounds, juce::Justification::centredLeft);
}

void Playlists::drawLoadingState(juce::Graphics &g, juce::Rectangle<int> bounds) {
  g.setColour(SidechainColors::textSecondary());
  g.setFont(14.0f);
  g.drawText("Loading playlists...", bounds, juce::Justification::centred);
}

void Playlists::drawEmptyState(juce::Graphics &g, juce::Rectangle<int> bounds) {
  g.setColour(SidechainColors::textSecondary());
  g.setFont(14.0f);
  g.drawText("No playlists yet.\nCreate your first playlist!", bounds, juce::Justification::centred);
}

void Playlists::drawErrorState(juce::Graphics &g, juce::Rectangle<int> bounds) {
  g.setColour(SidechainColors::error());
  g.setFont(14.0f);
  g.drawText(errorMessage, bounds, juce::Justification::centred);
}

//==============================================================================
juce::Rectangle<int> Playlists::getBackButtonBounds() const {
  return juce::Rectangle<int>(PADDING, 0, 50, HEADER_HEIGHT);
}

juce::Rectangle<int> Playlists::getFilterTabBounds(FilterType filter) const {
  int index = static_cast<int>(filter);
  int tabWidth = getWidth() / 4;
  return juce::Rectangle<int>(index * tabWidth, HEADER_HEIGHT, tabWidth, FILTER_TAB_HEIGHT);
}

juce::Rectangle<int> Playlists::getCreateButtonBounds() const {
  auto contentBounds = getContentBounds();
  return contentBounds.removeFromTop(CREATE_BUTTON_HEIGHT).reduced(PADDING, 8);
}

juce::Rectangle<int> Playlists::getContentBounds() const {
  return juce::Rectangle<int>(0, HEADER_HEIGHT + FILTER_TAB_HEIGHT, getWidth(),
                              getHeight() - HEADER_HEIGHT - FILTER_TAB_HEIGHT);
}

juce::Rectangle<int> Playlists::getPlaylistCardBounds(int index) const {
  auto contentBounds = getContentBounds();
  contentBounds.removeFromTop(CREATE_BUTTON_HEIGHT + 8); // Skip create button
  return contentBounds.removeFromTop(PLAYLIST_CARD_HEIGHT).translated(0, index * PLAYLIST_CARD_HEIGHT);
}

//==============================================================================
void Playlists::fetchPlaylists([[maybe_unused]] FilterType filter) {
  // Already handled in loadPlaylists()
}

int Playlists::calculateContentHeight() const {
  int height = CREATE_BUTTON_HEIGHT + 8; // Create button
  height += playlists.size() * PLAYLIST_CARD_HEIGHT;
  return height;
}

void Playlists::updateScrollBounds() {
  int contentHeight = calculateContentHeight();
  int viewportHeight = getContentBounds().getHeight() - CREATE_BUTTON_HEIGHT - 8;
  int maxScroll = juce::jmax(0, contentHeight - viewportHeight);

  scrollBar.setRangeLimits(0.0, static_cast<double>(maxScroll));
  scrollBar.setCurrentRange(scrollOffset, static_cast<double>(viewportHeight), juce::dontSendNotification);
  scrollBar.setVisible(maxScroll > 0);
}
