#pragma once

#include "../../models/AggregatedFeedGroup.h"
#include "../../util/Colors.h"
#include "PostCard.h"
#include <JuceHeader.h>

/**
 * AggregatedFeedCard - Displays a group of aggregated activities
 *
 * Shows summary like "User X and 3 others posted today" with
 * an expandable list showing individual activities when clicked.
 *
 * Used for:
 * - Timeline aggregated by user+day
 * - Trending aggregated by genre+day
 * - Notifications aggregated by action+day
 * - User activity aggregated by action+day
 */
class AggregatedFeedCard : public juce::Component {
public:
  AggregatedFeedCard();
  ~AggregatedFeedCard() override;

  /** Set the aggregated group data */
  void setGroup(const Sidechain::AggregatedFeedGroup &group);

  /** Get the group ID */
  juce::String getGroupId() const {
    return groupData.id;
  }

  /** Check if this card is expanded */
  bool isExpanded() const {
    return expanded;
  }

  /** Expand or collapse the activity list */
  void setExpanded(bool shouldExpand);

  void paint(juce::Graphics &g) override;
  void resized() override;
  void mouseEnter(const juce::MouseEvent &event) override;
  void mouseExit(const juce::MouseEvent &event) override;
  void mouseDown(const juce::MouseEvent &event) override;

  // Callbacks for user interactions
  std::function<void(const juce::String &userId)> onUserClicked;
  std::function<void(const juce::String &postId)> onPostClicked;
  std::function<void(const juce::String &postId)> onPlayClicked;

private:
  Sidechain::AggregatedFeedGroup groupData;
  bool expanded = false;
  bool hovering = false;

  // UI components
  juce::Label summaryLabel;
  juce::Label timestampLabel;
  juce::Label activityCountLabel;
  juce::TextButton expandButton;

  // Child post cards (shown when expanded)
  juce::OwnedArray<PostCard> activityCards;
  juce::Viewport activityViewport;
  juce::Component activityContainer;

  void updateSummary();
  void updateActivityCards();
  juce::String formatTimestamp(const juce::Time &time) const;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AggregatedFeedCard)
};
