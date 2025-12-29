#include "Header.h"
#include "../../network/NetworkClient.h"
#include "../../stores/AppStore.h"

#include "../../util/Async.h"
#include "../../util/Colors.h"
#include "../../util/Constants.h"
#include "../../util/Log.h"
#include "../../util/UIHelpers.h"
#include "../../util/Validate.h"

// ==============================================================================
Header::Header() {
  Log::info("Header: Initializing header component");
  // Set initial size - parent will call setBounds to resize to full width
  // Note: Don't set a fixed width here - let parent's setBounds control it
  setSize(100, HEADER_HEIGHT); // Minimal width, will be resized by parent
  Log::info("Header: Initialization complete");
}

// ==============================================================================
void Header::setNetworkClient(NetworkClient *client) {
  networkClient = client;
  Log::info("Header::setNetworkClient: NetworkClient set " + juce::String(client != nullptr ? "(valid)" : "(null)"));
}

// ==============================================================================
void Header::setUserInfo(const juce::String &user, const juce::String &picUrl) {
  Log::info("Header::setUserInfo: Setting user info - username: " + user + ", picUrl: " + picUrl);
  Log::debug("Header::setUserInfo: appStore is " + juce::String(appStore != nullptr ? "SET" : "NULL"));
  username = user;

  // Fetch avatar image via AppStore reactive observable (with caching)
  if (picUrl.isNotEmpty() && appStore) {
    Log::info("Header::setUserInfo: Loading profile image from AppStore - URL: " + picUrl);
    UIHelpers::loadImageAsync<Header>(
        this, appStore, picUrl,
        [](Header *comp, const juce::Image &image) {
          Log::info("Header: Profile image loaded successfully - size: " + juce::String(image.getWidth()) + "x" +
                    juce::String(image.getHeight()));
          comp->cachedProfileImage = image;
          comp->repaint();
        },
        [](Header *) { Log::warn("Header: Profile image is invalid or failed to load"); }, "Header");
  } else {
    Log::warn(
        "Header::setUserInfo: Not loading image - picUrl empty: " + juce::String(picUrl.isEmpty() ? "YES" : "NO") +
        ", appStore null: " + juce::String(appStore == nullptr ? "YES" : "NO"));
  }

  profilePicUrl = picUrl;
  repaint();
}

void Header::setProfileImage(const juce::Image &image) {
  if (image.isValid()) {
    Log::info("Header::setProfileImage: Setting profile image directly - size: " + juce::String(image.getWidth()) +
              "x" + juce::String(image.getHeight()));
    cachedProfileImage = image;
  } else {
    Log::warn("Header::setProfileImage: Invalid image provided");
    cachedProfileImage = juce::Image();
  }
  repaint();
}

// ==============================================================================
void Header::paint(juce::Graphics &g) {
  auto bounds = getLocalBounds();

  // Background
  g.setColour(SidechainColors::backgroundLight());
  g.fillRect(bounds);

  // Draw sections
  drawLogo(g, getLogoBounds());
  drawSearchButton(g, getSearchButtonBounds());
  drawRecordButton(g, getRecordButtonBounds());
  drawMessagesButton(g, getMessagesButtonBounds());
  drawStoryButton(g, getStoryButtonBounds());
  drawProfileSection(g, getProfileBounds());

  // Bottom border - use UIHelpers::drawDivider for consistency
  UIHelpers::drawDivider(g, 0, bounds.getBottom() - 1, bounds.getWidth(), SidechainColors::border(), 1.0f);
}

void Header::resized() {
  Log::debug("Header::resized: Component resized to " + juce::String(getWidth()) + "x" + juce::String(getHeight()));
  // Layout is handled in getBounds methods
}

void Header::mouseUp(const juce::MouseEvent &event) {
  auto pos = event.getPosition();
  Log::debug("Header::mouseUp: Mouse clicked at (" + juce::String(pos.x) + ", " + juce::String(pos.y) + ")");

  if (getLogoBounds().contains(pos)) {
    Log::info("Header::mouseUp: Logo clicked");
    if (onLogoClicked)
      onLogoClicked();
    else
      Log::warn("Header::mouseUp: Logo clicked but callback not set");
  } else if (getSearchButtonBounds().contains(pos)) {
    Log::info("Header::mouseUp: Search button clicked");
    if (onSearchClicked)
      onSearchClicked();
    else
      Log::warn("Header::mouseUp: Search clicked but callback not set");
  } else if (getRecordButtonBounds().contains(pos)) {
    Log::info("Header::mouseUp: Record button clicked");
    if (onRecordClicked)
      onRecordClicked();
    else
      Log::warn("Header::mouseUp: Record clicked but callback not set");
  } else if (getMessagesButtonBounds().contains(pos)) {
    Log::info("Header::mouseUp: Messages button clicked");
    if (onMessagesClicked)
      onMessagesClicked();
    else
      Log::warn("Header::mouseUp: Messages clicked but callback not set");
  } else if (getStoryButtonBounds().contains(pos)) {
    Log::info("Header::mouseUp: Story button clicked");
    if (onStoryClicked)
      onStoryClicked();
    else
      Log::warn("Header::mouseUp: Story clicked but callback not set");
  } else if (getProfileBounds().contains(pos)) {
    // Safely log username (avoid assertion failures with invalid strings)
    juce::String safeUsername = username.isNotEmpty() ? username : "(unknown)";
    Log::info("Header::mouseUp: Profile section clicked - username: " + safeUsername);

    // Check if profile picture area was clicked (for story viewing)
    auto picBounds = juce::Rectangle<int>(getProfileBounds().getX(), getProfileBounds().getCentreY() - 18, 36, 36);

    if (picBounds.contains(pos) && hasStories) {
      // Profile picture clicked and user has stories - view story
      Log::info("Header::mouseUp: Profile picture clicked with stories - "
                "opening story viewer");
      if (onProfileStoryClicked)
        onProfileStoryClicked();
      else
        Log::warn("Header::mouseUp: Profile story clicked but callback not set");
    } else {
      // Regular profile click
      if (onProfileClicked)
        onProfileClicked();
      else
        Log::warn("Header::mouseUp: Profile clicked but callback not set");
    }
  }
}

// ==============================================================================
void Header::drawLogo(juce::Graphics &g, juce::Rectangle<int> bounds) {
  g.setColour(SidechainColors::textPrimary());
  g.setFont(juce::Font(juce::FontOptions().withHeight(30.0f)).boldened());
  g.drawText("Sidechain", bounds, juce::Justification::centredLeft);
}

void Header::drawSearchButton(juce::Graphics &g, juce::Rectangle<int> bounds) {
  // Use UIHelpers::drawOutlineButton for consistent button styling
  g.setFont(juce::Font(juce::FontOptions().withHeight(22.0f)));
  UIHelpers::drawOutlineButton(g, bounds, "Search users...", SidechainColors::border(), SidechainColors::textMuted(),
                               false, 8.0f);
}

void Header::drawRecordButton(juce::Graphics &g, juce::Rectangle<int> bounds) {
  // Record button is wired up in PluginEditor.cpp (line 355-356) - navigates to
  // Recording screen Use UIHelpers::drawButton for base button, then add custom
  // recording dot
  g.setFont(juce::Font(juce::FontOptions().withHeight(24.0f)));
  UIHelpers::drawButton(g, bounds, "Record", SidechainColors::primary(), juce::Colours::white, false, 8.0f);

  // Draw red recording dot indicator
  auto dotBounds = bounds.withWidth(bounds.getHeight()).reduced(bounds.getHeight() / 3);
  dotBounds.setX(bounds.getX() + 12);
  g.setColour(SidechainColors::error()); // Red recording indicator
  g.fillEllipse(dotBounds.toFloat());
}

void Header::drawMessagesButton(juce::Graphics &g, juce::Rectangle<int> bounds) {
  // Draw messages icon as a chat bubble (avoid emoji for Linux font
  // compatibility)
  g.setColour(SidechainColors::textMuted());

  // Draw envelope-style icon
  auto iconBounds = bounds.withWidth(22).withHeight(16).withCentre(bounds.getCentre());
  g.drawRoundedRectangle(iconBounds.toFloat(), 2.0f, 1.5f);

  // Draw flap lines on envelope
  juce::Path flap;
  flap.startNewSubPath(static_cast<float>(iconBounds.getX()), static_cast<float>(iconBounds.getY()));
  flap.lineTo(static_cast<float>(iconBounds.getCentreX()), static_cast<float>(iconBounds.getCentreY()) - 2.0f);
  flap.lineTo(static_cast<float>(iconBounds.getRight()), static_cast<float>(iconBounds.getY()));
  g.strokePath(flap, juce::PathStrokeType(1.5f));

  // Draw unread badge if there are unread messages
  if (unreadMessageCount > 0) {
    int badgeSize = 16;
    auto badgeBounds = juce::Rectangle<int>(bounds.getX() + 20, bounds.getY() + 6, badgeSize, badgeSize);

    // Draw red badge background
    g.setColour(SidechainColors::error());
    g.fillEllipse(badgeBounds.toFloat());

    // Draw badge text
    g.setColour(juce::Colours::white);
    g.setFont(15.0f);
    juce::String countText = unreadMessageCount > 99 ? "99+" : juce::String(unreadMessageCount);
    g.drawText(countText, badgeBounds, juce::Justification::centred);
  }
}

void Header::drawStoryButton(juce::Graphics &g, juce::Rectangle<int> bounds) {
  // Draw story icon as a circle with plus (avoid emoji for Linux font
  // compatibility)
  g.setColour(SidechainColors::textMuted());

  auto iconBounds = bounds.withWidth(24).withHeight(24).withCentre(bounds.getCentre());
  g.drawEllipse(iconBounds.toFloat().reduced(2), 1.5f);

  // Draw plus sign inside
  auto centerF = iconBounds.getCentre().toFloat();
  g.drawLine(centerF.x - 5.0f, centerF.y, centerF.x + 5.0f, centerF.y, 1.5f);
  g.drawLine(centerF.x, centerF.y - 5.0f, centerF.x, centerF.y + 5.0f, 1.5f);
}

void Header::drawProfileSection(juce::Graphics &g, juce::Rectangle<int> bounds) {
  // Profile picture
  auto picBounds = juce::Rectangle<int>(bounds.getX(), bounds.getCentreY() - 18, 36, 36);
  drawCircularProfilePic(g, picBounds);

  // Username
  g.setColour(SidechainColors::textPrimary());
  g.setFont(14.0f);
  auto textBounds = bounds.withX(picBounds.getRight() + 8).withWidth(bounds.getWidth() - 44);
  g.drawText(username, textBounds, juce::Justification::centredLeft);
}

void Header::drawCircularProfilePic(juce::Graphics &g, juce::Rectangle<int> bounds) {
  // Draw highlighted circle if user has stories (Instagram-style)
  if (hasStories) {
    // Outer gradient circle (highlighted)
    juce::ColourGradient gradient(SidechainColors::error(), // Red
                                  0.0f, 0.0f,
                                  SidechainColors::warning(), // Yellow
                                  1.0f, 1.0f, true);
    gradient.addColour(0.5f, SidechainColors::primary()); // Orange/coral

    g.setGradientFill(gradient);
    g.drawEllipse(bounds.toFloat().expanded(2.0f), 2.5f);
  }

  // If we have a cached profile image, draw it using utility
  if (cachedProfileImage.isValid()) {
    Log::info("Header: Drawing profile photo from S3 - size: " + juce::String(cachedProfileImage.getWidth()) + "x" +
              juce::String(cachedProfileImage.getHeight()) + " into bounds: " + juce::String(bounds.getWidth()) + "x" +
              juce::String(bounds.getHeight()) + ", URL: " + profilePicUrl);
    // Draw avatar without border (border is handled separately for story highlight case)
    UIHelpers::drawCircularAvatar(g, bounds, cachedProfileImage, SidechainColors::primary(),
                                  juce::Colours::transparentBlack);
  } else {
    // Use loadImageAsync to load profile image (with caching)
    if (appStore && profilePicUrl.isNotEmpty()) {
      UIHelpers::loadImageAsync<Header>(
          const_cast<Header *>(this), appStore, profilePicUrl,
          [](Header *comp, const juce::Image &image) {
            Log::debug("Header: Image loaded from observable, triggering repaint");
            comp->cachedProfileImage = image;
            comp->repaint();
          },
          [](Header *) { Log::warn("Header: Failed to load profile image in paint"); }, "Header");
    }

    // Draw placeholder using utility
    UIHelpers::drawCircularAvatar(g, bounds, juce::Image(), SidechainColors::primary(),
                                  juce::Colours::transparentBlack);
  }

  // Border (only if no story highlight)
  if (!hasStories) {
    g.setColour(SidechainColors::border());
    g.drawEllipse(bounds.toFloat().reduced(0.5f), 1.0f);
  }
}

// ==============================================================================
juce::Rectangle<int> Header::getLogoBounds() const {
  return juce::Rectangle<int>(20, 0, 240, getHeight());
}

juce::Rectangle<int> Header::getSearchButtonBounds() const {
  int buttonWidth = 220;
  int buttonHeight = 36;
  int x = (getWidth() - buttonWidth) / 2;
  int y = (getHeight() - buttonHeight) / 2;
  return juce::Rectangle<int>(x, y, buttonWidth, buttonHeight);
}

juce::Rectangle<int> Header::getRecordButtonBounds() const {
  // Position the record button between search and profile
  auto searchBounds = getSearchButtonBounds();
  int buttonWidth = 140;
  int buttonHeight = 36;
  int x = searchBounds.getRight() + 16; // 16px gap after search
  int y = (getHeight() - buttonHeight) / 2;
  return juce::Rectangle<int>(x, y, buttonWidth, buttonHeight);
}

juce::Rectangle<int> Header::getProfileBounds() const {
  // Anchor profile section to the right edge
  return juce::Rectangle<int>(getWidth() - 160, 0, 140, getHeight());
}

juce::Rectangle<int> Header::getStoryButtonBounds() const {
  // Position story button to the left of profile
  auto profileBounds = getProfileBounds();
  int buttonWidth = 36;
  int buttonHeight = 36;
  int x = profileBounds.getX() - buttonWidth - 12; // 12px gap before profile
  int y = (getHeight() - buttonHeight) / 2;
  return juce::Rectangle<int>(x, y, buttonWidth, buttonHeight);
}

juce::Rectangle<int> Header::getMessagesButtonBounds() const {
  // Position messages button to the left of story
  auto storyBounds = getStoryButtonBounds();
  int buttonWidth = 36;
  int buttonHeight = 36;
  int x = storyBounds.getX() - buttonWidth - 12; // 12px gap before story
  int y = (getHeight() - buttonHeight) / 2;
  return juce::Rectangle<int>(x, y, buttonWidth, buttonHeight);
}

void Header::setUnreadMessageCount(int count) {
  if (unreadMessageCount != count) {
    unreadMessageCount = count;
    repaint();
  }
}

void Header::setHasStories(bool hasStoriesValue) {
  if (hasStories != hasStoriesValue) {
    hasStories = hasStoriesValue;
    repaint();
  }
}

// ==============================================================================
juce::String Header::getTooltip() {
  auto mousePos = getMouseXYRelative();

  if (getLogoBounds().contains(mousePos))
    return "Return to feed";

  if (getSearchButtonBounds().contains(mousePos))
    return "Find producers and sounds";

  if (getRecordButtonBounds().contains(mousePos))
    return "Record a new loop";

  if (getMessagesButtonBounds().contains(mousePos))
    return "Direct messages";

  if (getStoryButtonBounds().contains(mousePos))
    return "Post a story";

  if (getProfileBounds().contains(mousePos)) {
    // Check if hovering over profile picture specifically
    auto picBounds = juce::Rectangle<int>(getProfileBounds().getX(), getProfileBounds().getCentreY() - 18, 36, 36);

    if (picBounds.contains(mousePos) && hasStories)
      return "View your story";

    return "Your profile";
  }

  return {};
}
