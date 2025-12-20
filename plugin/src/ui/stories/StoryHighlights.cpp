#include "StoryHighlights.h"
#include "../../network/NetworkClient.h"
// For ImageLoader namespace
#include "../../util/Json.h"
#include "../../util/Log.h"

// ==============================================================================
StoryHighlights::StoryHighlights(Sidechain::Stores::AppStore *store) : AppStoreComponent(store) {}

StoryHighlights::~StoryHighlights() {}

// ==============================================================================
void StoryHighlights::onAppStateChanged(const Sidechain::Stores::StoriesState &state) {
  // Update highlights from state
  auto stateHighlights = state.highlights;
  highlights.clear();
  for (const auto &highlightPtr : stateHighlights) {
    if (highlightPtr) {
      // Convert Story to StoryHighlight (treating story as a highlight)
      Sidechain::StoryHighlight hl;
      hl.id = highlightPtr->id;
      hl.userId = highlightPtr->userId;
      hl.name = "Stories";                       // Default name
      hl.coverImageUrl = highlightPtr->audioUrl; // Use audio as cover reference
      hl.storyCount = 1;
      highlights.add(hl);
    }
  }

  // Load cover images for new highlights via AppStore
  for (const auto &highlight : highlights) {
    loadCoverImage(highlight);
  }

  repaint();
}

void StoryHighlights::subscribeToAppStore() {
  if (!appStore)
    return;

  juce::Component::SafePointer<StoryHighlights> safeThis(this);
  storeUnsubscriber = appStore->subscribeToStories([safeThis](const Sidechain::Stores::StoriesState &state) {
    if (!safeThis)
      return;
    juce::MessageManager::callAsync([safeThis, state]() {
      if (safeThis)
        safeThis->onAppStateChanged(state);
    });
  });
}

// ==============================================================================
void StoryHighlights::setUserId(const juce::String &id) {
  if (userId != id) {
    userId = id;
    highlights.clear();
    scrollOffset = 0;
    repaint();
  }
}

void StoryHighlights::loadHighlights() {
  if (networkClient == nullptr || userId.isEmpty())
    return;

  isLoading = true;
  repaint();

  juce::Component::SafePointer<StoryHighlights> safeThis(this);
  networkClient->getHighlights(userId, [safeThis](Outcome<juce::var> result) {
    if (safeThis == nullptr)
      return;

    safeThis->isLoading = false;

    if (result.isOk()) {
      auto response = result.getValue();
      if (Json::isObject(response)) {
        auto highlightsArray = Json::getArray(response, "highlights");
        if (Json::isArray(highlightsArray)) {
          safeThis->highlights.clear();
          for (int i = 0; i < highlightsArray.size(); ++i) {
            safeThis->highlights.add(Sidechain::StoryHighlight::fromJSON(highlightsArray[i]));
          }
          Log::info("StoryHighlights: Loaded " + juce::String(safeThis->highlights.size()) + " highlights");

          // Load cover images
          for (const auto &highlight : safeThis->highlights) {
            safeThis->loadCoverImage(highlight);
          }
        }
      }
    } else {
      Log::error("StoryHighlights: Failed to load - " + result.getError());
    }

    safeThis->repaint();
  });
}

void StoryHighlights::setHighlights(const juce::Array<Sidechain::StoryHighlight> &newHighlights) {
  highlights = newHighlights;

  for (const auto &highlight : highlights) {
    loadCoverImage(highlight);
  }

  repaint();
}

// ==============================================================================
void StoryHighlights::paint(juce::Graphics &g) {
  if (isLoading) {
    drawLoadingState(g);
    return;
  }

  auto bounds = getLocalBounds().reduced(PADDING, 0);
  int x = bounds.getX() - scrollOffset;

  // Draw "New" button for own profile
  if (isOwnProfile) {
    auto addBounds = juce::Rectangle<int>(x, bounds.getY() + PADDING, ADD_BUTTON_SIZE, HIGHLIGHT_SIZE + NAME_HEIGHT);
    drawAddButton(g, addBounds);
    x += ADD_BUTTON_SIZE + SPACING;
  }

  // Draw highlights
  for (int i = 0; i < highlights.size(); ++i) {
    auto highlightBounds =
        juce::Rectangle<int>(x, bounds.getY() + PADDING, HIGHLIGHT_SIZE, HIGHLIGHT_SIZE + NAME_HEIGHT);
    drawHighlight(g, highlights[i], highlightBounds);
    x += HIGHLIGHT_SIZE + SPACING;
  }
}

void StoryHighlights::drawHighlight(juce::Graphics &g, const Sidechain::StoryHighlight &highlight,
                                    juce::Rectangle<int> bounds) {
  auto circleBounds = bounds.removeFromTop(HIGHLIGHT_SIZE).toFloat();
  auto nameBounds = bounds;

  // Draw ring around circle
  g.setColour(Colors::highlightRing);
  g.drawEllipse(circleBounds.reduced(2), 2.5f);

  // Draw cover image or placeholder
  auto imageBounds = circleBounds.reduced(4);

  // Check if we have a cached image
  auto it = coverImages.find(highlight.id);
  if (it != coverImages.end() && it->second.isValid()) {
    // Draw cached image
    juce::Path clipPath;
    clipPath.addEllipse(imageBounds);
    g.saveState();
    g.reduceClipRegion(clipPath);
    g.drawImage(it->second, imageBounds, juce::RectanglePlacement::centred | juce::RectanglePlacement::fillDestination);
    g.restoreState();
  } else {
    // Draw placeholder while image loads
    g.setColour(Colors::addButtonBg);
    g.fillEllipse(imageBounds);
    g.setColour(Colors::textPrimary);
    g.setFont(juce::Font(juce::FontOptions().withHeight(20.0f)).boldened());
    juce::String initial = highlight.name.isNotEmpty() ? highlight.name.substring(0, 1).toUpperCase() : "?";
    g.drawText(initial, imageBounds.toNearestInt(), juce::Justification::centred);

    // Load image if not already loading
    if (appStore && !highlight.getCoverUrl().isEmpty() && coverImages.find(highlight.id) == coverImages.end()) {
      loadCoverImage(highlight);
    }
  }

  // Draw name
  g.setColour(Colors::textSecondary);
  g.setFont(juce::Font(juce::FontOptions().withHeight(11.0f)));
  juce::String displayName = highlight.name;
  if (displayName.length() > 10)
    displayName = displayName.substring(0, 9) + "...";
  g.drawText(displayName, nameBounds.reduced(0, 2), juce::Justification::centredTop);
}

void StoryHighlights::drawAddButton(juce::Graphics &g, juce::Rectangle<int> bounds) {
  auto circleBounds = bounds.removeFromTop(ADD_BUTTON_SIZE).toFloat();
  auto nameBounds = bounds;

  // Draw dashed circle
  g.setColour(Colors::addButtonBg);
  g.fillEllipse(circleBounds.reduced(2));

  // Draw plus icon
  g.setColour(Colors::addButtonIcon);
  auto center = circleBounds.getCentre();
  float iconSize = 20.0f;
  g.drawLine(center.x - iconSize / 2, center.y, center.x + iconSize / 2, center.y, 2.5f);
  g.drawLine(center.x, center.y - iconSize / 2, center.x, center.y + iconSize / 2, 2.5f);

  // Draw "New" label
  g.setColour(Colors::textSecondary);
  g.setFont(juce::Font(juce::FontOptions().withHeight(11.0f)));
  g.drawText("New", nameBounds.reduced(0, 2), juce::Justification::centredTop);
}

void StoryHighlights::drawLoadingState(juce::Graphics &g) {
  g.setColour(Colors::textSecondary);
  g.setFont(juce::Font(juce::FontOptions().withHeight(12.0f)));
  g.drawText("Loading highlights...", getLocalBounds(), juce::Justification::centred);
}

// ==============================================================================
void StoryHighlights::resized() {
  // No child components to layout
}

void StoryHighlights::mouseUp(const juce::MouseEvent &event) {
  auto pos = event.getPosition();

  // Check add button
  if (isOwnProfile && isAddButtonAt(pos)) {
    if (onCreateHighlightClicked)
      onCreateHighlightClicked();
    return;
  }

  // Check highlights
  int index = getHighlightIndexAt(pos);
  if (index >= 0 && index < highlights.size()) {
    if (onHighlightClicked)
      onHighlightClicked(highlights[index]);
  }
}

// ==============================================================================
int StoryHighlights::getHighlightIndexAt(juce::Point<int> pos) const {
  for (int i = 0; i < highlights.size(); ++i) {
    if (getHighlightBounds(i).contains(pos))
      return i;
  }
  return -1;
}

bool StoryHighlights::isAddButtonAt(juce::Point<int> pos) const {
  return getAddButtonBounds().contains(pos);
}

juce::Rectangle<int> StoryHighlights::getHighlightBounds(int index) const {
  int x = PADDING - scrollOffset;

  if (isOwnProfile)
    x += ADD_BUTTON_SIZE + SPACING;

  x += index * (HIGHLIGHT_SIZE + SPACING);

  return juce::Rectangle<int>(x, PADDING, HIGHLIGHT_SIZE, HIGHLIGHT_SIZE + NAME_HEIGHT);
}

juce::Rectangle<int> StoryHighlights::getAddButtonBounds() const {
  if (!isOwnProfile)
    return {};

  return juce::Rectangle<int>(PADDING - scrollOffset, PADDING, ADD_BUTTON_SIZE, HIGHLIGHT_SIZE + NAME_HEIGHT);
}

// ==============================================================================
void StoryHighlights::loadCoverImage(const Sidechain::StoryHighlight &highlight) {
  if (!appStore || highlight.getCoverUrl().isEmpty())
    return;

  // Skip if already loading or cached
  if (coverImages.find(highlight.id) != coverImages.end())
    return;

  // Mark as loading (empty image in map)
  coverImages[highlight.id] = juce::Image();

  juce::Component::SafePointer<StoryHighlights> safeThis(this);
  juce::String highlightId = highlight.id;
  juce::String coverUrl = highlight.getCoverUrl();

  appStore->getImage(coverUrl, [safeThis, highlightId](const juce::Image &image) {
    if (safeThis == nullptr)
      return;

    juce::MessageManager::callAsync([safeThis, highlightId, image]() {
      if (safeThis == nullptr)
        return;

      if (image.isValid()) {
        safeThis->coverImages[highlightId] = image;
        safeThis->repaint();
      } else {
        // Remove failed load marker
        safeThis->coverImages.erase(highlightId);
      }
    });
  });
}
