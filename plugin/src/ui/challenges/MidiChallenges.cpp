#include "MidiChallenges.h"
#include "../../util/Log.h"

// ==============================================================================
MidiChallenges::MidiChallenges(Sidechain::Stores::AppStore *store) : AppStoreComponent(store) {
  Log::info("MidiChallenges: Initializing");

  // Set up scroll bar
  scrollBar.setRangeLimits(0.0, 100.0);
  scrollBar.addListener(this);
  addAndMakeVisible(scrollBar);
  subscribeToAppStore();
}

MidiChallenges::~MidiChallenges() {
  Log::debug("MidiChallenges: Destroying");
  scrollBar.removeListener(this);
  // AppStoreComponent destructor will handle unsubscribe
}

// ==============================================================================
// AppStoreComponent virtual methods

void MidiChallenges::onAppStateChanged(const Sidechain::Stores::ChallengeState &state) {
  isLoading = state.isLoading;
  errorMessage = state.challengeError;

  // Rebuild challenges list from state
  challenges.clear();
  for (const auto &challenge : state.challenges) {
    // Convert from shared_ptr<MIDIChallenge> to MIDIChallenge
    if (challenge) {
      challenges.add(*challenge);
    }
  }

  Log::debug("MidiChallenges: Store state changed - " + juce::String(challenges.size()) + " challenges");
  repaint();
}

void MidiChallenges::subscribeToAppStore() {
  if (!appStore) {
    Log::warn("MidiChallenges: Cannot subscribe - AppStore is null");
    return;
  }

  Log::debug("MidiChallenges: Subscribing to AppStore challenges state");

  // Subscribe to challenges state changes
  juce::Component::SafePointer<MidiChallenges> safeThis(this);
  storeUnsubscriber = appStore->subscribeToChallenges([safeThis](const Sidechain::Stores::ChallengeState &state) {
    if (!safeThis)
      return;

    juce::MessageManager::callAsync([safeThis, state]() {
      if (safeThis)
        safeThis->onAppStateChanged(state);
    });
  });

  // Load challenges to populate initial state
  loadChallenges();
}

// ==============================================================================
void MidiChallenges::paint(juce::Graphics &g) {
  // Background
  g.fillAll(SidechainColors::background());

  // Header
  drawHeader(g);

  // Content area
  auto contentBounds = getContentBounds();
  contentBounds.translate(0, -scrollOffset);

  // Filter tabs
  drawFilterTabs(g, contentBounds);

  // Challenges list
  if (isLoading) {
    drawLoadingState(g, contentBounds);
  } else if (errorMessage.isNotEmpty()) {
    drawErrorState(g, contentBounds);
  } else if (challenges.isEmpty()) {
    drawEmptyState(g, contentBounds);
  } else {
    for (int i = 0; i < challenges.size(); ++i) {
      auto cardBounds = getChallengeCardBounds(i);
      cardBounds.translate(0, -scrollOffset);
      if (cardBounds.getBottom() >= 0 && cardBounds.getY() < getHeight()) {
        drawChallengeCard(g, cardBounds, challenges[i]);
      }
    }
  }
}

void MidiChallenges::resized() {
  updateScrollBounds();

  // Position scroll bar
  scrollBar.setBounds(getWidth() - 12, HEADER_HEIGHT + FILTER_TAB_HEIGHT, 12,
                      getHeight() - HEADER_HEIGHT - FILTER_TAB_HEIGHT);
}

void MidiChallenges::mouseUp(const juce::MouseEvent &event) {
  auto pos = event.getPosition();

  // Back button
  if (getBackButtonBounds().contains(pos)) {
    if (onBackPressed)
      onBackPressed();
    return;
  }

  // Filter tabs
  for (int i = 0; i < 5; ++i) {
    FilterType filter = static_cast<FilterType>(i);
    if (getFilterTabBounds(filter).contains(pos)) {
      if (currentFilter != filter) {
        currentFilter = filter;
        loadChallenges();
        repaint();
      }
      return;
    }
  }

  // Challenge cards
  for (int i = 0; i < challenges.size(); ++i) {
    auto cardBounds = getChallengeCardBounds(i);
    cardBounds.translate(0, -scrollOffset);
    if (cardBounds.contains(pos)) {
      if (onChallengeSelected)
        onChallengeSelected(challenges[i].id);
      return;
    }
  }
}

void MidiChallenges::mouseWheelMove(const juce::MouseEvent & /*event*/, const juce::MouseWheelDetails &wheel) {
  float delta = wheel.deltaY;
  scrollOffset = juce::jmax(0, scrollOffset - static_cast<int>(delta * 30.0f));
  updateScrollBounds();
  repaint();
}

void MidiChallenges::scrollBarMoved(juce::ScrollBar * /*scrollBar*/, double newRangeStart) {
  scrollOffset = static_cast<int>(newRangeStart);
  repaint();
}

// ==============================================================================
void MidiChallenges::loadChallenges() {
  Log::debug("MidiChallenges: Loading challenges from AppStore");
  Sidechain::Stores::AppStore::getInstance().loadChallenges();
}

void MidiChallenges::refresh() {
  Log::debug("MidiChallenges: Refreshing challenges");
  Sidechain::Stores::AppStore::getInstance().loadChallenges();
}

// ==============================================================================
void MidiChallenges::drawHeader(juce::Graphics &g) {
  auto bounds = juce::Rectangle<int>(0, 0, getWidth(), HEADER_HEIGHT);

  // Background
  g.setColour(SidechainColors::surface());
  g.fillRect(bounds);

  // Title
  g.setColour(SidechainColors::textPrimary());
  g.setFont(juce::Font(juce::FontOptions().withHeight(20.0f)).boldened());
  g.drawText("MIDI Challenges", bounds.removeFromLeft(getWidth() - 100), juce::Justification::centredLeft);

  // Back button
  auto backBounds = getBackButtonBounds();
  g.setColour(SidechainColors::textPrimary());
  g.setFont(16.0f);
  g.drawText("←", backBounds, juce::Justification::centred);
}

void MidiChallenges::drawFilterTabs(juce::Graphics &g, juce::Rectangle<int> &bounds) {
  auto tabsBounds = bounds.removeFromTop(FILTER_TAB_HEIGHT);

  juce::StringArray tabLabels = {"All", "Active", "Voting", "Past", "Upcoming"};
  int tabWidth = tabsBounds.getWidth() / 5;

  for (int i = 0; i < 5; ++i) {
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

void MidiChallenges::drawChallengeCard(juce::Graphics &g, juce::Rectangle<int> bounds,
                                       const Sidechain::MIDIChallenge &challenge) {
  bounds = bounds.reduced(PADDING, 8);

  bool isHovered = bounds.contains(getMouseXYRelative());
  g.setColour(isHovered ? SidechainColors::surface().brighter(0.1f) : SidechainColors::surface());
  g.fillRoundedRectangle(bounds.toFloat(), 8.0f);

  // Border
  g.setColour(SidechainColors::border());
  g.drawRoundedRectangle(bounds.toFloat(), 8.0f, 1.0f);

  // Title
  g.setColour(SidechainColors::textPrimary());
  g.setFont(juce::Font(juce::FontOptions().withHeight(18.0f)).boldened());
  auto titleBounds = bounds.removeFromTop(24).reduced(12, 0);
  g.drawText(challenge.title, titleBounds, juce::Justification::centredLeft);

  // Description
  if (challenge.description.isNotEmpty()) {
    g.setColour(SidechainColors::textSecondary());
    g.setFont(12.0f);
    auto descBounds = bounds.removeFromTop(36).reduced(12, 0);
    g.drawText(challenge.description, descBounds, juce::Justification::topLeft, true);
  }

  // Status and time remaining
  auto metaBounds = bounds.reduced(12, 0);
  g.setColour(SidechainColors::textSecondary());
  g.setFont(11.0f);
  juce::String meta = getStatusDisplayText(challenge);
  meta += " • " + getTimeRemainingText(challenge);
  meta += " • " + juce::String(challenge.entryCount) + " entr" + (challenge.entryCount != 1 ? "ies" : "y");
  g.drawText(meta, metaBounds, juce::Justification::centredLeft);
}

void MidiChallenges::drawLoadingState(juce::Graphics &g, juce::Rectangle<int> bounds) {
  g.setColour(SidechainColors::textSecondary());
  g.setFont(14.0f);
  g.drawText("Loading challenges...", bounds, juce::Justification::centred);
}

void MidiChallenges::drawEmptyState(juce::Graphics &g, juce::Rectangle<int> bounds) {
  g.setColour(SidechainColors::textSecondary());
  g.setFont(14.0f);
  g.drawText("No challenges available.\nCheck back later for new challenges!", bounds, juce::Justification::centred);
}

void MidiChallenges::drawErrorState(juce::Graphics &g, juce::Rectangle<int> bounds) {
  g.setColour(SidechainColors::error());
  g.setFont(14.0f);
  g.drawText(errorMessage, bounds, juce::Justification::centred);
}

// ==============================================================================
juce::Rectangle<int> MidiChallenges::getBackButtonBounds() const {
  return juce::Rectangle<int>(PADDING, 0, 50, HEADER_HEIGHT);
}

juce::Rectangle<int> MidiChallenges::getFilterTabBounds(FilterType filter) const {
  int index = static_cast<int>(filter);
  int tabWidth = getWidth() / 5;
  return juce::Rectangle<int>(index * tabWidth, HEADER_HEIGHT, tabWidth, FILTER_TAB_HEIGHT);
}

juce::Rectangle<int> MidiChallenges::getContentBounds() const {
  return juce::Rectangle<int>(0, HEADER_HEIGHT + FILTER_TAB_HEIGHT, getWidth(),
                              getHeight() - HEADER_HEIGHT - FILTER_TAB_HEIGHT);
}

juce::Rectangle<int> MidiChallenges::getChallengeCardBounds(int index) const {
  auto contentBounds = getContentBounds();
  return contentBounds.removeFromTop(CHALLENGE_CARD_HEIGHT).translated(0, index * CHALLENGE_CARD_HEIGHT);
}

// ==============================================================================
void MidiChallenges::fetchChallenges(FilterType filter) {
  // First load all challenges from AppStore
  loadChallenges();

  // Then filter by status based on the provided filter type
  // This is client-side filtering - full implementation would request filtered results from API
  juce::Array<Sidechain::MIDIChallenge> filteredChallenges;

  for (const auto &challenge : challenges) {
    bool includeChallenge = false;

    switch (filter) {
    case FilterType::All:
      includeChallenge = true;
      break;
    case FilterType::Active:
      includeChallenge = (challenge.status == "active");
      break;
    case FilterType::Voting:
      includeChallenge = (challenge.status == "voting");
      break;
    case FilterType::Past:
      includeChallenge = (challenge.status == "completed" || challenge.status == "closed");
      break;
    case FilterType::Upcoming:
      includeChallenge = (challenge.status == "upcoming" || challenge.status == "scheduled");
      break;
    }

    if (includeChallenge) {
      filteredChallenges.add(challenge);
    }
  }

  challenges = filteredChallenges;
  updateScrollBounds();
  repaint();
}

int MidiChallenges::calculateContentHeight() const {
  return challenges.size() * CHALLENGE_CARD_HEIGHT;
}

void MidiChallenges::updateScrollBounds() {
  int contentHeight = calculateContentHeight();
  int viewportHeight = getContentBounds().getHeight();
  int maxScroll = juce::jmax(0, contentHeight - viewportHeight);

  scrollBar.setRangeLimits(0.0, static_cast<double>(maxScroll));
  scrollBar.setCurrentRange(scrollOffset, static_cast<double>(viewportHeight), juce::dontSendNotification);
  scrollBar.setVisible(maxScroll > 0);
}

juce::String MidiChallenges::getStatusDisplayText(const Sidechain::MIDIChallenge &challenge) const {
  if (challenge.status == "active")
    return juce::String(juce::CharPointer_UTF8("\xF0\x9F\xA3\xAF")) + " Active"; // 🎯
  else if (challenge.status == "voting")
    return juce::String(juce::CharPointer_UTF8("\xF0\x9F\x97\xB3\xEF\xB8\x8F")) + " Voting"; // 🗳️
  else if (challenge.status == "ended")
    return juce::String(juce::CharPointer_UTF8("\xE2\x9C\x85")) + " Ended"; // ✅
  else if (challenge.status == "upcoming")
    return juce::String(juce::CharPointer_UTF8("\xF0\x9F\x8F\xB0")) + " Upcoming"; // ⏰
  return challenge.status;
}

juce::String MidiChallenges::getTimeRemainingText(const Sidechain::MIDIChallenge &challenge) const {
  auto now = juce::Time::getCurrentTime();

  if (challenge.isAcceptingSubmissions()) {
    auto remaining = challenge.endDate - now;
    auto seconds = remaining.inSeconds();
    if (seconds > 0) {
      if (seconds < 3600)
        return "Submissions close in " + juce::String(seconds / 60) + " min";
      else if (seconds < 86400)
        return "Submissions close in " + juce::String(seconds / 3600) + " hour" + (seconds >= 7200 ? "s" : "");
      else
        return "Submissions close in " + juce::String(seconds / 86400) + " day" + (seconds >= 172800 ? "s" : "");
    }
    return "Submissions closing soon";
  } else if (challenge.isVoting()) {
    auto remaining = challenge.votingEndDate - now;
    auto seconds = remaining.inSeconds();
    if (seconds > 0) {
      if (seconds < 3600)
        return "Voting ends in " + juce::String(seconds / 60) + " min";
      else if (seconds < 86400)
        return "Voting ends in " + juce::String(seconds / 3600) + " hour" + (seconds >= 7200 ? "s" : "");
      else
        return "Voting ends in " + juce::String(seconds / 86400) + " day" + (seconds >= 172800 ? "s" : "");
    }
    return "Voting ending soon";
  } else if (challenge.hasEnded()) {
    return "Challenge ended";
  } else {
    auto remaining = challenge.startDate - now;
    auto seconds = remaining.inSeconds();
    if (seconds > 0) {
      if (seconds < 3600)
        return "Starts in " + juce::String(seconds / 60) + " min";
      else if (seconds < 86400)
        return "Starts in " + juce::String(seconds / 3600) + " hour" + (seconds >= 7200 ? "s" : "");
      else
        return "Starts in " + juce::String(seconds / 86400) + " day" + (seconds >= 172800 ? "s" : "");
    }
    return "Starting soon";
  }
}
