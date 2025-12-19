#include "MidiChallengeDetail.h"
#include "../../audio/HttpAudioPlayer.h"
#include "../../network/NetworkClient.h"
#include "../../util/Async.h"
#include "../../util/Json.h"
#include "../../util/Log.h"
#include "../../util/Result.h"

// ==============================================================================
MidiChallengeDetail::MidiChallengeDetail(Sidechain::Stores::AppStore *store) : AppStoreComponent(store) {
  Log::info("MidiChallengeDetail: Initializing");

  // Set up scroll bar
  scrollBar.setRangeLimits(0.0, 100.0);
  scrollBar.addListener(this);
  addAndMakeVisible(scrollBar);
  initialize();
}

MidiChallengeDetail::~MidiChallengeDetail() {
  Log::debug("MidiChallengeDetail: Destroying");
  scrollBar.removeListener(this);
}

// ==============================================================================
void MidiChallengeDetail::setNetworkClient(NetworkClient *client) {
  networkClient = client;
  Log::debug("MidiChallengeDetail: NetworkClient set " + juce::String(client != nullptr ? "(valid)" : "(null)"));
}

void MidiChallengeDetail::setAudioPlayer(HttpAudioPlayer *player) {
  audioPlayer = player;
}

// ==============================================================================
// AppStoreComponent virtual methods

void MidiChallengeDetail::subscribeToAppStore() {
  Log::debug("MidiChallengeDetail: Subscribing to AppStore");

  if (!appStore) {
    Log::warn("MidiChallengeDetail: Cannot subscribe to null AppStore");
    return;
  }

  // Subscribe to challenge state changes
  juce::Component::SafePointer<MidiChallengeDetail> safeThis(this);
  storeUnsubscriber =
      appStore->subscribeToChallenges([safeThis](const Sidechain::Stores::ChallengeState &challengeState) {
        // Check if component still exists
        if (!safeThis)
          return;

        // Schedule UI update on message thread for thread safety
        juce::MessageManager::callAsync([safeThis, challengeState]() {
          // Double-check component still exists after async dispatch
          if (!safeThis)
            return;

          safeThis->onAppStateChanged(challengeState);
        });
      });

  Log::debug("MidiChallengeDetail: Successfully subscribed to AppStore");
}

void MidiChallengeDetail::onAppStateChanged(const Sidechain::Stores::ChallengeState &state) {
  // Update UI when challenge state changes in the store
  // Could refresh the current challenge if it's been updated
  Log::debug("MidiChallengeDetail: Handling challenge state change");

  // If the current challenge ID matches one in the state, refresh its data
  if (!challengeId.isEmpty() && state.challenges.size() > 0) {
    // Check if our challenge was updated
    for (const auto &ch : state.challenges) {
      juce::String chId = ch.getProperty("id", "").toString();
      if (chId == challengeId) {
        repaint();
        break;
      }
    }
  }
}

// ==============================================================================
void MidiChallengeDetail::paint(juce::Graphics &g) {
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

  // Challenge info
  drawChallengeInfo(g, contentBounds);

  // Action buttons
  drawActionButtons(g, contentBounds);

  // Entries list (leaderboard)
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

void MidiChallengeDetail::resized() {
  updateScrollBounds();

  // Position scroll bar
  scrollBar.setBounds(getWidth() - 12, HEADER_HEIGHT, 12, getHeight() - HEADER_HEIGHT);
}

void MidiChallengeDetail::mouseUp(const juce::MouseEvent &event) {
  auto pos = event.getPosition();

  // Back button
  if (getBackButtonBounds().contains(pos)) {
    if (onBackPressed)
      onBackPressed();
    return;
  }

  // Submit button
  if (!hasUserSubmitted() && challenge.isAcceptingSubmissions() && getSubmitButtonBounds().contains(pos)) {
    if (onSubmitEntry)
      onSubmitEntry();
    return;
  }

  // Entry cards
  for (int i = 0; i < entries.size(); ++i) {
    auto cardBounds = getEntryCardBounds(i);
    cardBounds.translate(0, -scrollOffset);
    if (cardBounds.contains(pos)) {
      // Check if vote button was clicked
      auto voteBounds = getVoteButtonBounds(i);
      voteBounds.translate(0, -scrollOffset);
      if (challenge.isVoting() && !entries[i].hasVoted && voteBounds.contains(pos)) {
        voteForEntry(entries[i].id);
        return;
      }

      // Check if play button was clicked
      auto playBounds = getPlayButtonBounds(i);
      playBounds.translate(0, -scrollOffset);
      if (playBounds.contains(pos)) {
        if (audioPlayer && entries[i].audioUrl.isNotEmpty()) {
          audioPlayer->loadAndPlay(entries[i].id, entries[i].audioUrl);
        }
        return;
      }

      // Otherwise, navigate to entry/post
      if (onEntrySelected)
        onEntrySelected(entries[i].id);
      return;
    }
  }
}

void MidiChallengeDetail::mouseWheelMove(const juce::MouseEvent & /*event*/, const juce::MouseWheelDetails &wheel) {
  float delta = wheel.deltaY;
  scrollOffset = juce::jmax(0, scrollOffset - static_cast<int>(delta * 30.0f));
  updateScrollBounds();
  repaint();
}

void MidiChallengeDetail::scrollBarMoved(juce::ScrollBar * /*scrollBar*/, double newRangeStart) {
  scrollOffset = static_cast<int>(newRangeStart);
  repaint();
}

// ==============================================================================
void MidiChallengeDetail::loadChallenge(const juce::String &id) {
  challengeId = id;
  fetchChallenge();
}

void MidiChallengeDetail::refresh() {
  fetchChallenge();
}

// ==============================================================================
void MidiChallengeDetail::fetchChallenge() {
  if (!networkClient || challengeId.isEmpty()) {
    Log::warn("MidiChallengeDetail: No network client or challenge ID");
    return;
  }

  isLoading = true;
  errorMessage = "";
  repaint();

  networkClient->getMIDIChallenge(challengeId, [this](Outcome<juce::var> result) {
    juce::MessageManager::callAsync([this, result]() {
      isLoading = false;

      if (!result.isOk()) {
        errorMessage = "Failed to load challenge: " + result.getError();
        Log::warn("MidiChallengeDetail: " + errorMessage);
        repaint();
        return;
      }

      auto response = result.getValue();

      // Parse challenge
      if (response.hasProperty("challenge")) {
        challenge = MIDIChallenge::fromJSON(response["challenge"]);
      } else {
        challenge = MIDIChallenge::fromJSON(response);
      }

      // Parse entries
      entries.clear();
      userEntryId = "";

      juce::var entriesVar;
      if (response.hasProperty("challenge") && response["challenge"].hasProperty("entries")) {
        entriesVar = response["challenge"]["entries"];
      } else if (response.hasProperty("entries")) {
        entriesVar = response["entries"];
      }

      if (entriesVar.isArray()) {
        for (int i = 0; i < entriesVar.size(); ++i) {
          auto entry = MIDIChallengeEntry::fromJSON(entriesVar[i]);
          entries.add(entry);

          // Check if this is the user's entry
          if (entry.userId == currentUserId)
            userEntryId = entry.id;
        }
      }

      Log::info("MidiChallengeDetail: Loaded challenge with " + juce::String(entries.size()) + " entries");
      updateScrollBounds();
      repaint();
    });
  });
}

void MidiChallengeDetail::voteForEntry(const juce::String &entryId) {
  if (!networkClient || challengeId.isEmpty())
    return;

  networkClient->voteMIDIChallengeEntry(challengeId, entryId, [this, entryId](Outcome<juce::var> result) {
    juce::MessageManager::callAsync([this, entryId, result]() {
      if (result.isOk()) {
        // Refresh to get updated vote counts
        refresh();
      } else {
        Log::warn("MidiChallengeDetail: Failed to vote: " + result.getError());
      }
    });
  });
}

// ==============================================================================
void MidiChallengeDetail::drawHeader(juce::Graphics &g) {
  auto bounds = juce::Rectangle<int>(0, 0, getWidth(), HEADER_HEIGHT);

  // Background
  g.setColour(SidechainColors::surface());
  g.fillRect(bounds);

  // Title
  g.setColour(SidechainColors::textPrimary());
  g.setFont(juce::Font(juce::FontOptions().withHeight(20.0f)).boldened());
  g.drawText("Challenge Details", bounds.removeFromLeft(getWidth() - 100), juce::Justification::centredLeft);

  // Back button
  auto backBounds = getBackButtonBounds();
  g.setColour(SidechainColors::textPrimary());
  g.setFont(16.0f);
  g.drawText("←", backBounds, juce::Justification::centred);
}

void MidiChallengeDetail::drawChallengeInfo(juce::Graphics &g, juce::Rectangle<int> &bounds) {
  auto infoBounds = bounds.removeFromTop(INFO_HEIGHT).reduced(PADDING, 0);

  // Background
  g.setColour(SidechainColors::surface());
  g.fillRoundedRectangle(infoBounds.toFloat(), 8.0f);

  // Border
  g.setColour(SidechainColors::border());
  g.drawRoundedRectangle(infoBounds.toFloat(), 8.0f, 1.0f);

  auto contentBounds = infoBounds.reduced(12, 12);

  // Title
  g.setColour(SidechainColors::textPrimary());
  g.setFont(juce::Font(juce::FontOptions().withHeight(22.0f)).boldened());
  auto titleBounds = contentBounds.removeFromTop(28);
  g.drawText(challenge.title, titleBounds, juce::Justification::centredLeft);

  // Description
  if (challenge.description.isNotEmpty()) {
    g.setColour(SidechainColors::textSecondary());
    g.setFont(13.0f);
    auto descBounds = contentBounds.removeFromTop(50);
    g.drawText(challenge.description, descBounds, juce::Justification::topLeft, true);
  }

  // Constraints
  g.setColour(SidechainColors::textSecondary());
  g.setFont(11.0f);
  auto constraintsText = formatConstraints(challenge.constraints);
  if (constraintsText.isNotEmpty()) {
    auto constraintsBounds = contentBounds.removeFromTop(20);
    g.drawText("Constraints: " + constraintsText, constraintsBounds, juce::Justification::centredLeft);
  }

  // Status and entry count
  auto metaBounds = contentBounds;
  juce::String meta = juce::String(challenge.entryCount) + " entr" + (challenge.entryCount != 1 ? "ies" : "y");
  if (challenge.status == "active")
    meta = "🎯 " + meta;
  else if (challenge.status == "voting")
    meta = "🗳️ " + meta;
  g.drawText(meta, metaBounds, juce::Justification::centredLeft);
}

void MidiChallengeDetail::drawActionButtons(juce::Graphics &g, juce::Rectangle<int> &bounds) {
  bounds.removeFromTop(BUTTON_HEIGHT + 8); // Reserve space for buttons

  // Submit button (only if user hasn't submitted and challenge is accepting)
  if (!hasUserSubmitted() && challenge.isAcceptingSubmissions()) {
    auto submitBounds = getSubmitButtonBounds();
    submitBounds.translate(0, -scrollOffset);
    bool isHovered = submitBounds.contains(getMouseXYRelative());
    g.setColour(isHovered ? SidechainColors::coralPink().brighter(0.2f) : SidechainColors::coralPink());
    g.fillRoundedRectangle(submitBounds.toFloat(), 8.0f);

    g.setColour(SidechainColors::textPrimary());
    g.setFont(14.0f);
    g.drawText("Submit Entry", submitBounds, juce::Justification::centred);
  }
}

void MidiChallengeDetail::drawEntryCard(juce::Graphics &g, juce::Rectangle<int> bounds, const MIDIChallengeEntry &entry,
                                        int index) {
  bounds = bounds.reduced(PADDING, 4);

  bool isHovered = bounds.contains(getMouseXYRelative());
  g.setColour(isHovered ? SidechainColors::surface().brighter(0.1f) : SidechainColors::surface());
  g.fillRoundedRectangle(bounds.toFloat(), 8.0f);

  // Border
  g.setColour(SidechainColors::border());
  g.drawRoundedRectangle(bounds.toFloat(), 8.0f, 1.0f);

  // Rank badge (top 3)
  if (index < 3) {
    auto badgeBounds = bounds.removeFromLeft(40).reduced(8, 8);
    g.setColour(SidechainColors::getMedalColor(index));
    g.fillRoundedRectangle(badgeBounds.toFloat(), 4.0f);
    g.setColour(SidechainColors::textPrimary());
    g.setFont(16.0f);
    g.drawText("# " + juce::String(index + 1), badgeBounds, juce::Justification::centred);
  } else {
    bounds.removeFromLeft(8);
  }

  // Username
  g.setColour(SidechainColors::textPrimary());
  g.setFont(juce::Font(juce::FontOptions().withHeight(14.0f)).boldened());
  auto nameBounds = bounds.removeFromTop(20).reduced(8, 0);
  g.drawText(entry.username, nameBounds, juce::Justification::centredLeft);

  // Vote count and buttons
  auto bottomBounds = bounds.removeFromBottom(30).reduced(8, 0);

  // Vote count
  g.setColour(SidechainColors::textSecondary());
  g.setFont(12.0f);
  juce::String voteText = juce::String(entry.voteCount) + " vote" + (entry.voteCount != 1 ? "s" : "");
  g.drawText(voteText, bottomBounds.removeFromLeft(100), juce::Justification::centredLeft);

  // Vote button (if voting phase and user hasn't voted)
  if (challenge.isVoting() && !entry.hasVoted) {
    auto voteBounds = getVoteButtonBounds(index);
    voteBounds.translate(0, -scrollOffset);
    bool voteHovered = voteBounds.contains(getMouseXYRelative());
    g.setColour(voteHovered ? SidechainColors::coralPink().brighter(0.2f) : SidechainColors::coralPink());
    g.fillRoundedRectangle(voteBounds.toFloat(), 6.0f);
    g.setColour(SidechainColors::textPrimary());
    g.setFont(11.0f);
    g.drawText("Vote", voteBounds, juce::Justification::centred);
  }

  // Play button
  if (entry.audioUrl.isNotEmpty()) {
    auto playBounds = getPlayButtonBounds(index);
    playBounds.translate(0, -scrollOffset);
    bool playHovered = playBounds.contains(getMouseXYRelative());
    g.setColour(playHovered ? SidechainColors::surface().brighter(0.2f) : SidechainColors::surface());
    g.fillRoundedRectangle(playBounds.toFloat(), 6.0f);
    g.setColour(SidechainColors::textPrimary());
    g.setFont(11.0f);
    g.drawText("▶", playBounds, juce::Justification::centred);
  }
}

void MidiChallengeDetail::drawLoadingState(juce::Graphics &g, juce::Rectangle<int> bounds) {
  g.setColour(SidechainColors::textSecondary());
  g.setFont(14.0f);
  g.drawText("Loading challenge...", bounds, juce::Justification::centred);
}

void MidiChallengeDetail::drawErrorState(juce::Graphics &g, juce::Rectangle<int> bounds) {
  g.setColour(SidechainColors::error());
  g.setFont(14.0f);
  g.drawText(errorMessage, bounds, juce::Justification::centred);
}

void MidiChallengeDetail::drawEmptyState(juce::Graphics &g, juce::Rectangle<int> bounds) {
  g.setColour(SidechainColors::textSecondary());
  g.setFont(14.0f);
  juce::String text = "No entries yet.";
  if (challenge.isAcceptingSubmissions())
    text += "\nBe the first to submit!";
  g.drawText(text, bounds, juce::Justification::centred);
}

// ==============================================================================
juce::Rectangle<int> MidiChallengeDetail::getBackButtonBounds() const {
  return juce::Rectangle<int>(PADDING, 0, 50, HEADER_HEIGHT);
}

juce::Rectangle<int> MidiChallengeDetail::getSubmitButtonBounds() const {
  auto contentBounds = getContentBounds();
  contentBounds.translate(0, -scrollOffset);
  auto buttonsBounds = contentBounds.removeFromTop(BUTTON_HEIGHT + 8).reduced(PADDING, 0);
  return buttonsBounds.removeFromLeft(150);
}

juce::Rectangle<int> MidiChallengeDetail::getContentBounds() const {
  return juce::Rectangle<int>(0, HEADER_HEIGHT, getWidth(), getHeight() - HEADER_HEIGHT);
}

juce::Rectangle<int> MidiChallengeDetail::getEntryCardBounds(int index) const {
  auto contentBounds = getContentBounds();
  contentBounds.removeFromTop(INFO_HEIGHT + BUTTON_HEIGHT + 16); // Skip info and buttons
  return contentBounds.removeFromTop(ENTRY_CARD_HEIGHT).translated(0, index * ENTRY_CARD_HEIGHT);
}

juce::Rectangle<int> MidiChallengeDetail::getVoteButtonBounds(int index) const {
  auto cardBounds = getEntryCardBounds(index);
  auto bottomBounds = cardBounds.removeFromBottom(30).reduced(8, 4);
  return bottomBounds.removeFromRight(60);
}

juce::Rectangle<int> MidiChallengeDetail::getPlayButtonBounds(int index) const {
  auto cardBounds = getEntryCardBounds(index);
  auto bottomBounds = cardBounds.removeFromBottom(30).reduced(8, 4);
  bottomBounds.removeFromRight(60); // Skip vote button
  return bottomBounds.removeFromRight(40);
}

// ==============================================================================
int MidiChallengeDetail::calculateContentHeight() const {
  int height = INFO_HEIGHT + BUTTON_HEIGHT + 16; // Info and buttons
  height += entries.size() * ENTRY_CARD_HEIGHT;
  return height;
}

void MidiChallengeDetail::updateScrollBounds() {
  int contentHeight = calculateContentHeight();
  int viewportHeight = getContentBounds().getHeight();
  int maxScroll = juce::jmax(0, contentHeight - viewportHeight);

  scrollBar.setRangeLimits(0.0, static_cast<double>(maxScroll));
  scrollBar.setCurrentRange(scrollOffset, static_cast<double>(viewportHeight), juce::dontSendNotification);
  scrollBar.setVisible(maxScroll > 0);
}

bool MidiChallengeDetail::hasUserSubmitted() const {
  return userEntryId.isNotEmpty();
}

juce::String MidiChallengeDetail::formatConstraints(const MIDIChallengeConstraints &constraints) const {
  juce::StringArray parts;

  if (constraints.bpmMin > 0 || constraints.bpmMax > 0) {
    if (constraints.bpmMin > 0 && constraints.bpmMax > 0)
      parts.add("BPM: " + juce::String(constraints.bpmMin) + "-" + juce::String(constraints.bpmMax));
    else if (constraints.bpmMin > 0)
      parts.add("BPM: ≥" + juce::String(constraints.bpmMin));
    else
      parts.add("BPM: ≤" + juce::String(constraints.bpmMax));
  }

  if (constraints.key.isNotEmpty())
    parts.add("Key: " + constraints.key);

  if (constraints.scale.isNotEmpty())
    parts.add("Scale: " + constraints.scale);

  if (constraints.noteCountMin > 0 || constraints.noteCountMax > 0) {
    if (constraints.noteCountMin > 0 && constraints.noteCountMax > 0)
      parts.add("Notes: " + juce::String(constraints.noteCountMin) + "-" + juce::String(constraints.noteCountMax));
    else if (constraints.noteCountMin > 0)
      parts.add("Notes: ≥" + juce::String(constraints.noteCountMin));
    else
      parts.add("Notes: ≤" + juce::String(constraints.noteCountMax));
  }

  if (constraints.durationMin > 0 || constraints.durationMax > 0) {
    if (constraints.durationMin > 0 && constraints.durationMax > 0)
      parts.add("Duration: " + juce::String(constraints.durationMin, 1) + "-" +
                juce::String(constraints.durationMax, 1) + "s");
    else if (constraints.durationMin > 0)
      parts.add("Duration: ≥" + juce::String(constraints.durationMin, 1) + "s");
    else
      parts.add("Duration: ≤" + juce::String(constraints.durationMax, 1) + "s");
  }

  return parts.joinIntoString(", ");
}
