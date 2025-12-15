#include "AggregatedFeedCard.h"
#include "../../util/Time.h"

AggregatedFeedCard::AggregatedFeedCard() {
  // Setup summary label
  addAndMakeVisible(summaryLabel);
  summaryLabel.setFont(
      juce::Font(juce::FontOptions().withHeight(16.0f).withStyle("Bold")));
  summaryLabel.setColour(juce::Label::textColourId,
                         SidechainColors::textPrimary());

  // Setup timestamp
  addAndMakeVisible(timestampLabel);
  timestampLabel.setFont(juce::Font(juce::FontOptions().withHeight(13.0f)));
  timestampLabel.setColour(juce::Label::textColourId,
                           SidechainColors::textSecondary());

  // Setup activity count
  addAndMakeVisible(activityCountLabel);
  activityCountLabel.setFont(juce::Font(juce::FontOptions().withHeight(13.0f)));
  activityCountLabel.setColour(juce::Label::textColourId,
                               SidechainColors::textSecondary());

  // Setup expand button
  addAndMakeVisible(expandButton);
  expandButton.setButtonText("Show all");
  expandButton.onClick = [this]() { setExpanded(!expanded); };

  // Setup viewport for expanded activities
  activityViewport.setViewedComponent(&activityContainer, false);
  activityViewport.setScrollBarsShown(true, false);
  addChildComponent(activityViewport);

  setSize(600, 80); // Collapsed height
}

AggregatedFeedCard::~AggregatedFeedCard() {}

void AggregatedFeedCard::setGroup(const AggregatedFeedGroup &group) {
  groupData = group;
  updateSummary();
  updateActivityCards();
  repaint();
}

void AggregatedFeedCard::setExpanded(bool shouldExpand) {
  if (expanded == shouldExpand)
    return;

  expanded = shouldExpand;
  expandButton.setButtonText(expanded ? "Hide" : "Show all");
  activityViewport.setVisible(expanded);

  // Resize to show/hide activities
  if (expanded) {
    int totalHeight = 80;                      // Header height
    totalHeight += activityCards.size() * 250; // Each activity card ~250px
    setSize(getWidth(), totalHeight);
  } else {
    setSize(getWidth(), 80); // Collapsed height
  }

  if (auto *parent = getParentComponent())
    parent->resized();
}

void AggregatedFeedCard::updateSummary() {
  // Set summary text
  summaryLabel.setText(groupData.getSummary(), juce::dontSendNotification);

  // Set timestamp
  timestampLabel.setText(formatTimestamp(groupData.updatedAt),
                         juce::dontSendNotification);

  // Set activity count
  activityCountLabel.setText(juce::String(groupData.activityCount) +
                                 " activities",
                             juce::dontSendNotification);
}

void AggregatedFeedCard::updateActivityCards() {
  activityCards.clear();

  // Create a PostCard for each activity in the group
  for (const auto &post : groupData.activities) {
    auto *card = new PostCard();
    card->setPost(post);

    // Forward callbacks
    card->onUserClicked = [this](const FeedPost &post) {
      if (onUserClicked)
        onUserClicked(post.userId);
    };

    card->onPlayClicked = [this](const FeedPost &post) {
      if (onPlayClicked)
        onPlayClicked(post.id);
    };

    activityCards.add(card);
    activityContainer.addAndMakeVisible(card);
  }

  // Layout activity cards vertically
  int yPos = 0;
  for (auto *card : activityCards) {
    card->setBounds(0, yPos, getWidth() - 20, 250);
    yPos += 250 + 10; // 10px spacing
  }

  activityContainer.setSize(getWidth() - 20, yPos);
}

juce::String AggregatedFeedCard::formatTimestamp(const juce::Time &time) const {
  return TimeUtils::formatTimeAgo(time);
}

void AggregatedFeedCard::paint(juce::Graphics &g) {
  // Draw background
  auto bgColor =
      hovering ? SidechainColors::surfaceHover() : SidechainColors::surface();
  g.setColour(bgColor);
  g.fillRoundedRectangle(getLocalBounds().toFloat(), 8.0f);

  // Draw border
  g.setColour(SidechainColors::border());
  g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), 8.0f, 1.0f);
}

void AggregatedFeedCard::resized() {
  auto bounds = getLocalBounds().reduced(16);

  // Layout header (always visible)
  auto headerBounds = bounds.removeFromTop(60);

  // Summary on left
  summaryLabel.setBounds(headerBounds.removeFromTop(24));

  // Timestamp and activity count
  auto metaBounds = headerBounds.removeFromTop(20);
  timestampLabel.setBounds(metaBounds.removeFromLeft(200));
  activityCountLabel.setBounds(metaBounds.removeFromLeft(150));

  // Expand button on right
  expandButton.setBounds(bounds.getRight() - 100, 20, 80, 30);

  // Expanded activity list
  if (expanded) {
    bounds.removeFromTop(10); // Spacing
    activityViewport.setBounds(bounds);
  }
}

void AggregatedFeedCard::mouseEnter(const juce::MouseEvent &) {
  hovering = true;
  repaint();
}

void AggregatedFeedCard::mouseExit(const juce::MouseEvent &) {
  hovering = false;
  repaint();
}

void AggregatedFeedCard::mouseDown(const juce::MouseEvent &event) {
  // Click to expand/collapse
  if (!expanded && !expandButton.getBounds().contains(event.getPosition())) {
    setExpanded(true);
  }
}
