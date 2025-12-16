#include "StoriesFeed.h"
#include "../../network/NetworkClient.h"
#include "../../util/Async.h"
#include "../../util/Log.h"
#include "../../util/Result.h"

namespace StoryFeedColors {
const juce::Colour background(0xff1a1a2e);
const juce::Colour surface(0xff25253a);
const juce::Colour ringUnviewed(0xff7c4dff); // Purple gradient ring
const juce::Colour ringViewed(0xff4a4a5a);   // Gray ring for viewed
const juce::Colour createBg(0xff2d2d44);
const juce::Colour createPlus(0xff7c4dff);
const juce::Colour textPrimary(0xffffffff);
const juce::Colour textSecondary(0xffb0b0b0);
const juce::Colour badgeRed(0xffe53935);
} // namespace StoryFeedColors

//==============================================================================
StoriesFeed::StoriesFeed() {
  // Start animation timer
  startTimerHz(60);

  Log::info("StoriesFeed created");
}

StoriesFeed::~StoriesFeed() {
  unbindFromStore();
  stopTimer();
  Log::info("StoriesFeed destroyed");
}

//==============================================================================
// Store integration methods
void StoriesFeed::bindToStore(std::shared_ptr<Sidechain::Stores::AppStore> store) {
  Log::debug("StoriesFeed: Binding to StoriesStore");

  storiesStore = store;
  if (!storiesStore)
    return;

  // Subscribe to store changes
  juce::Component::SafePointer<StoriesFeed> safeThis(this);
  storeUnsubscriber = storiesStore->subscribe([safeThis](const Sidechain::Stores::AppState &appState) {
    if (!safeThis)
      return;

    const auto &state = appState.stories;
    auto groups = state.feedUserStories;

    juce::MessageManager::callAsync([safeThis, groups]() {
      if (!safeThis)
        return;

      std::vector<UserStories> newUserStoriesGroups;
      for (int i = 0; i < groups.size(); ++i) {
        const auto &groupVar = groups[i];
        UserStories uiGroup;
        uiGroup.userId = groupVar.getProperty("user_id", "").toString();
        uiGroup.username = groupVar.getProperty("username", "").toString();
        uiGroup.avatarUrl = groupVar.getProperty("avatar_url", "").toString();
        uiGroup.hasUnviewed = static_cast<bool>(groupVar.getProperty("has_unviewed", false));

        auto storiesArray = groupVar.getProperty("stories", juce::var());
        if (storiesArray.isArray()) {
          for (int j = 0; j < storiesArray.size(); ++j) {
            const auto &storyVar = storiesArray[j];
            StoryData uiStory;
            uiStory.id = storyVar.getProperty("id", "").toString();
            uiStory.userId = storyVar.getProperty("user_id", "").toString();
            uiStory.username = storyVar.getProperty("username", "").toString();
            uiStory.userAvatarUrl = storyVar.getProperty("user_avatar_url", "").toString();
            uiStory.audioUrl = storyVar.getProperty("audio_url", "").toString();
            uiStory.filename = storyVar.getProperty("filename", "").toString();
            uiStory.midiFilename = storyVar.getProperty("midi_filename", "").toString();
            uiStory.audioDuration = static_cast<float>(storyVar.getProperty("audio_duration", 0.0));
            uiStory.midiData = storyVar.getProperty("midi_data", juce::var());
            uiStory.midiPatternId = storyVar.getProperty("midi_pattern_id", "").toString();
            uiStory.viewCount = static_cast<int>(storyVar.getProperty("view_count", 0));
            uiStory.viewed = static_cast<bool>(storyVar.getProperty("viewed", false));
            uiStory.expiresAt = juce::Time::fromISO8601(storyVar.getProperty("expires_at", "").toString());
            uiStory.createdAt = juce::Time::fromISO8601(storyVar.getProperty("created_at", "").toString());
            uiGroup.stories.push_back(uiStory);
          }
        }
        newUserStoriesGroups.push_back(uiGroup);
      }

      safeThis->userStoriesGroups = newUserStoriesGroups;
      safeThis->repaint();
    });
  });

  Log::debug("StoriesFeed: Successfully bound to AppStore");
}

void StoriesFeed::unbindFromStore() {
  if (storeUnsubscriber) {
    storeUnsubscriber();
    storeUnsubscriber = nullptr;
  }
  storiesStore = nullptr;
  Log::debug("StoriesFeed: Unbound from AppStore");
}

//==============================================================================
void StoriesFeed::paint(juce::Graphics &g) {
  // Background
  g.fillAll(StoryFeedColors::background);

  // Draw "Create Story" circle (always first)
  auto createBounds = getCircleBounds(0);
  if (createBounds.getX() + createBounds.getWidth() > 0 && createBounds.getX() < getWidth()) {
    drawCreateStoryCircle(g, createBounds);
  }

  // Draw user story circles
  for (int i = 0; i < static_cast<int>(userStoriesGroups.size()); ++i) {
    auto bounds = getCircleBounds(i + 1);
    if (bounds.getX() + bounds.getWidth() > 0 && bounds.getX() < getWidth()) {
      drawStoryCircle(g, bounds, userStoriesGroups[static_cast<size_t>(i)], i);
    }
  }

  // Fade edges for scroll indication
  if (scrollOffset > 0) {
    // Left fade
    juce::ColourGradient leftFade(StoryFeedColors::background, 0, 0, StoryFeedColors::background.withAlpha(0.0f), 20, 0,
                                  false);
    g.setGradientFill(leftFade);
    g.fillRect(0, 0, 20, getHeight());
  }

  if (scrollOffset < maxScrollOffset) {
    // Right fade
    juce::ColourGradient rightFade(StoryFeedColors::background.withAlpha(0.0f), static_cast<float>(getWidth() - 20),
                                   0.0f, StoryFeedColors::background, static_cast<float>(getWidth()), 0.0f, false);
    g.setGradientFill(rightFade);
    g.fillRect(getWidth() - 20, 0, 20, getHeight());
  }
}

void StoriesFeed::resized() {
  // Calculate max scroll offset
  int contentWidth = calculateContentWidth();
  maxScrollOffset = juce::jmax(0.0f, static_cast<float>(contentWidth - getWidth()));
}

void StoriesFeed::mouseUp(const juce::MouseEvent &event) {
  auto pos = event.getPosition();

  // Check "Create Story" circle
  auto createBounds = getCircleBounds(0);
  if (createBounds.contains(pos)) {
    if (onCreateStory)
      onCreateStory();
    return;
  }

  // Check user story circles
  for (int i = 0; i < static_cast<int>(userStoriesGroups.size()); ++i) {
    auto bounds = getCircleBounds(i + 1);
    if (bounds.contains(pos)) {
      if (onStoryTapped)
        onStoryTapped(userStoriesGroups[static_cast<size_t>(i)].userId, 0);
      return;
    }
  }
}

void StoriesFeed::mouseWheelMove(const juce::MouseEvent & /*event*/, const juce::MouseWheelDetails &wheel) {
  // Horizontal scroll
  float delta = (std::abs(wheel.deltaX) > 0.0001f) ? wheel.deltaX : wheel.deltaY;
  targetScrollOffset = juce::jlimit(0.0f, maxScrollOffset, targetScrollOffset - delta * 50.0f);
}

//==============================================================================
void StoriesFeed::timerCallback() {
  // Smooth scroll animation
  if (std::abs(scrollOffset - targetScrollOffset) > 0.5f) {
    scrollOffset += (targetScrollOffset - scrollOffset) * 0.2f;
    repaint();
  } else if (std::abs(scrollOffset - targetScrollOffset) > 0.0001f) {
    scrollOffset = targetScrollOffset;
    repaint();
  }
}

//==============================================================================
void StoriesFeed::loadStories() {
  if (!networkClient) {
    Log::warn("StoriesFeed: No network client set");
    return;
  }

  Log::info("StoriesFeed: Loading stories...");

  networkClient->getStoriesFeed([this](Outcome<juce::var> result) {
    juce::MessageManager::callAsync([this, result]() {
      if (!result.isOk()) {
        Log::warn("StoriesFeed: Failed to load stories: " + result.getError());
        return;
      }

      auto response = result.getValue();
      if (response.hasProperty("stories")) {
        auto *storiesArray = response["stories"].getArray();
        if (storiesArray) {
          std::vector<StoryData> stories;
          for (const auto &storyVar : *storiesArray) {
            StoryData story;
            story.id = storyVar["id"].toString();
            story.userId = storyVar["user_id"].toString();
            story.audioUrl = storyVar["audio_url"].toString();
            story.filename = storyVar["filename"].toString();
            story.midiFilename = storyVar["midi_filename"].toString();
            story.audioDuration = static_cast<float>(storyVar["audio_duration"]);
            story.midiData = storyVar["midi_data"];
            story.midiPatternId = storyVar["midi_pattern_id"].toString(); // R.3.3.5.5 - MIDI download support
            story.viewCount = static_cast<int>(storyVar["view_count"]);
            story.viewed = static_cast<bool>(storyVar["viewed"]);

            // Parse user info
            if (storyVar.hasProperty("user")) {
              story.username = storyVar["user"]["username"].toString();
              story.userAvatarUrl = storyVar["user"]["avatar_url"].toString();
            }

            // Parse timestamps (ISO 8601 format)
            // Simplified - would need proper date parsing
            story.expiresAt = juce::Time::getCurrentTime() + juce::RelativeTime::hours(24);

            stories.push_back(story);
          }

          setStories(stories);
          Log::info("StoriesFeed: Loaded " + juce::String(stories.size()) + " stories");
        }
      }
    });
  });
}

void StoriesFeed::setStories(const std::vector<StoryData> &newStories) {
  // Group stories by user
  userStoriesGroups.clear();

  std::map<juce::String, UserStories> userMap;

  for (const auto &story : newStories) {
    if (story.isExpired())
      continue;

    auto &userStories = userMap[story.userId];
    if (userStories.userId.isEmpty()) {
      userStories.userId = story.userId;
      userStories.username = story.username;
      userStories.avatarUrl = story.userAvatarUrl;
    }

    userStories.stories.push_back(story);
    if (!story.viewed)
      userStories.hasUnviewed = true;
  }

  // Convert map to vector
  for (auto &pair : userMap) {
    userStoriesGroups.push_back(std::move(pair.second));
  }

  // Sort: unviewed first, then by most recent
  std::sort(userStoriesGroups.begin(), userStoriesGroups.end(), [](const UserStories &a, const UserStories &b) {
    if (a.hasUnviewed != b.hasUnviewed)
      return a.hasUnviewed;
    return false; // Keep original order otherwise
  });

  resized();
  repaint();
}

bool StoriesFeed::hasOwnStory() const {
  for (const auto &group : userStoriesGroups) {
    if (group.userId == currentUserId)
      return true;
  }
  return false;
}

//==============================================================================
void StoriesFeed::drawCreateStoryCircle(juce::Graphics &g, juce::Rectangle<int> bounds) {
  auto circleBounds = bounds.removeFromTop(CIRCLE_SIZE + RING_THICKNESS * 2);

  // Center the circle
  auto center = circleBounds.getCentre().toFloat();
  float radius = CIRCLE_SIZE / 2.0f;

  // Background circle
  g.setColour(StoryFeedColors::createBg);
  g.fillEllipse(center.x - radius, center.y - radius, radius * 2, radius * 2);

  // Avatar if user has one
  if (currentUserAvatarUrl.isNotEmpty()) {
    auto it = avatarCache.find(currentUserId);
    if (it != avatarCache.end() && it->second.isValid()) {
      // Draw avatar
      g.saveState();
      juce::Path clipPath;
      clipPath.addEllipse(center.x - radius, center.y - radius, radius * 2, radius * 2);
      g.reduceClipRegion(clipPath);
      g.drawImage(it->second, juce::Rectangle<float>(center.x - radius, center.y - radius, radius * 2, radius * 2),
                  juce::RectanglePlacement::centred | juce::RectanglePlacement::fillDestination);
      g.restoreState();
    } else {
      // Load avatar
      loadAvatarImage(currentUserId, currentUserAvatarUrl);
    }
  }

  // Plus icon overlay
  float plusSize = 20.0f;
  auto plusBounds = juce::Rectangle<float>(center.x + radius - plusSize * 0.6f, center.y + radius - plusSize * 0.6f,
                                           plusSize, plusSize);

  g.setColour(StoryFeedColors::createPlus);
  g.fillEllipse(plusBounds);

  g.setColour(StoryFeedColors::textPrimary);
  g.setFont(juce::FontOptions(16.0f).withStyle("Bold"));
  g.drawText("+", plusBounds.toNearestInt(), juce::Justification::centred);

  // "Your Story" label
  auto labelBounds = bounds.removeFromTop(LABEL_HEIGHT);
  g.setColour(StoryFeedColors::textSecondary);
  g.setFont(10.0f);
  g.drawText("Your Story", labelBounds, juce::Justification::centredTop);
}

void StoriesFeed::drawStoryCircle(juce::Graphics &g, juce::Rectangle<int> bounds, const UserStories &userStories,
                                  int /*index*/) {
  auto circleBounds = bounds.removeFromTop(CIRCLE_SIZE + RING_THICKNESS * 2);
  auto center = circleBounds.getCentre().toFloat();
  float outerRadius = (CIRCLE_SIZE + RING_THICKNESS * 2) / 2.0f;
  float innerRadius = CIRCLE_SIZE / 2.0f;

  // Draw ring (purple gradient if unviewed, gray if viewed)
  if (userStories.hasUnviewed) {
    // Purple gradient ring
    juce::ColourGradient gradient(juce::Colour(0xff9c27b0), center.x, center.y - outerRadius, juce::Colour(0xff2196f3),
                                  center.x, center.y + outerRadius, false);
    g.setGradientFill(gradient);
  } else {
    g.setColour(StoryFeedColors::ringViewed);
  }

  g.fillEllipse(center.x - outerRadius, center.y - outerRadius, outerRadius * 2, outerRadius * 2);

  // Inner circle (gap)
  g.setColour(StoryFeedColors::background);
  float gapRadius = innerRadius + 2.0f;
  g.fillEllipse(center.x - gapRadius, center.y - gapRadius, gapRadius * 2, gapRadius * 2);

  // Avatar
  auto it = avatarCache.find(userStories.userId);
  if (it != avatarCache.end() && it->second.isValid()) {
    g.saveState();
    juce::Path clipPath;
    clipPath.addEllipse(center.x - innerRadius, center.y - innerRadius, innerRadius * 2, innerRadius * 2);
    g.reduceClipRegion(clipPath);
    g.drawImage(
        it->second,
        juce::Rectangle<float>(center.x - innerRadius, center.y - innerRadius, innerRadius * 2, innerRadius * 2),
        juce::RectanglePlacement::centred | juce::RectanglePlacement::fillDestination);
    g.restoreState();
  } else {
    // Placeholder avatar
    g.setColour(StoryFeedColors::surface);
    g.fillEllipse(center.x - innerRadius, center.y - innerRadius, innerRadius * 2, innerRadius * 2);

    // Initials
    g.setColour(StoryFeedColors::textPrimary);
    g.setFont(juce::FontOptions(18.0f).withStyle("Bold"));
    juce::String initial = userStories.username.isNotEmpty() ? userStories.username.substring(0, 1).toUpperCase() : "?";
    g.drawText(initial, circleBounds, juce::Justification::centred);

    // Load avatar async
    if (userStories.avatarUrl.isNotEmpty())
      loadAvatarImage(userStories.userId, userStories.avatarUrl);
  }

  // Story count badge (if multiple stories)
  if (userStories.stories.size() > 1) {
    float badgeSize = 18.0f;
    auto badgeBounds = juce::Rectangle<float>(center.x + innerRadius - badgeSize * 0.5f,
                                              center.y - innerRadius - badgeSize * 0.3f, badgeSize, badgeSize);
    g.setColour(StoryFeedColors::badgeRed);
    g.fillEllipse(badgeBounds);

    g.setColour(StoryFeedColors::textPrimary);
    g.setFont(10.0f);
    g.drawText(juce::String(userStories.stories.size()), badgeBounds.toNearestInt(), juce::Justification::centred);
  }

  // Username label
  auto labelBounds = bounds.removeFromTop(LABEL_HEIGHT);
  g.setColour(StoryFeedColors::textSecondary);
  g.setFont(10.0f);

  juce::String displayName = userStories.username;
  if (displayName.length() > 10)
    displayName = displayName.substring(0, 9) + "...";

  g.drawText(displayName, labelBounds, juce::Justification::centredTop);
}

int StoriesFeed::calculateContentWidth() const {
  int numCircles = 1 + static_cast<int>(userStoriesGroups.size()); // +1 for create story
  return CIRCLE_PADDING + numCircles * (CIRCLE_SIZE + RING_THICKNESS * 2 + CIRCLE_PADDING);
}

juce::Rectangle<int> StoriesFeed::getCircleBounds(int index) const {
  int x = CIRCLE_PADDING + index * (CIRCLE_SIZE + RING_THICKNESS * 2 + CIRCLE_PADDING);
  x -= static_cast<int>(scrollOffset);

  return juce::Rectangle<int>(x, 5, CIRCLE_SIZE + RING_THICKNESS * 2, CIRCLE_SIZE + RING_THICKNESS * 2 + LABEL_HEIGHT);
}

void StoriesFeed::loadAvatarImage(const juce::String &userId, const juce::String &avatarUrl) {
  if (avatarUrl.isEmpty() || avatarCache.count(userId) > 0)
    return;

  // Mark as loading (placeholder)
  avatarCache[userId] = juce::Image();

  Async::runVoid([this, userId, avatarUrl]() {
    juce::URL url(avatarUrl);
    auto inputStream = url.createInputStream(
        juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress).withConnectionTimeoutMs(5000));

    if (inputStream) {
      juce::MemoryBlock data;
      inputStream->readIntoMemoryBlock(data);

      auto image = juce::ImageFileFormat::loadFrom(data.getData(), data.getSize());

      juce::MessageManager::callAsync([this, userId, image]() {
        if (image.isValid()) {
          avatarCache[userId] = image;
          repaint();
        }
      });
    }
  });
}
