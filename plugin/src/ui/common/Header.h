#pragma once

#include "../../util/Constants.h"
#include <JuceHeader.h>
#include <functional>

class NetworkClient;

/**
 * Header - Central header bar shown on all post-login pages
 *
 * Features:
 * - App logo/title
 * - Search button (navigates to discovery)
 * - User profile section with avatar and username
 * - Consistent styling across all views
 */
class Header : public juce::Component, public juce::TooltipClient {
public:
  /** Height of the header component in pixels */
  static constexpr int HEADER_HEIGHT = Constants::Ui::HEADER_HEIGHT;

  /** Constructor */
  Header();

  /** Destructor */
  ~Header() override = default;

  /** Paint the header component
   *  @param g Graphics context for drawing
   */
  void paint(juce::Graphics &) override;

  /** Handle component resize
   *  Updates layout of all header elements
   */
  void resized() override;

  /** Handle mouse click events
   *  @param event Mouse event information
   */
  void mouseUp(const juce::MouseEvent &event) override;

  /** Get tooltip text based on current mouse position
   *  @return Tooltip text for the element under the mouse
   */
  juce::String getTooltip() override;

  /** Set user information for display
   *  @param username User's username to display
   *  @param profilePicUrl URL to user's profile picture
   */
  void setUserInfo(const juce::String &username, const juce::String &profilePicUrl);

  /** Set profile image directly (from UserDataStore) - avoids redundant
   * downloads
   *  @param image The profile image to display
   */
  void setProfileImage(const juce::Image &image);

  /** Set NetworkClient for HTTP requests
   *  @param client Pointer to NetworkClient instance
   */
  void setNetworkClient(NetworkClient *client);

  /** Set unread message count for badge display
   *  @param count Number of unread messages
   */
  void setUnreadMessageCount(int count);

  /** Set whether the current user has active stories
   *  @param hasStories True if user has active stories (will show highlighted
   * circle)
   */
  void setHasStories(bool hasStories);

  // Callbacks for header interactions
  std::function<void()> onSearchClicked;
  std::function<void()> onProfileClicked;
  std::function<void()> onLogoClicked;
  std::function<void()> onRecordClicked;
  std::function<void()> onStoryClicked;
  std::function<void()> onMessagesClicked;
  std::function<void()> onProfileStoryClicked; // Called when profile picture is
                                               // clicked to view own story

private:
  juce::String username;
  juce::String profilePicUrl;
  juce::Image cachedProfileImage;
  NetworkClient *networkClient = nullptr;
  int unreadMessageCount = 0;
  bool hasStories = false;

  // Load profile image from URL
  void loadProfileImage(const juce::String &url);

  // Draw helper methods
  void drawLogo(juce::Graphics &g, juce::Rectangle<int> bounds);
  void drawSearchButton(juce::Graphics &g, juce::Rectangle<int> bounds);
  void drawRecordButton(juce::Graphics &g, juce::Rectangle<int> bounds);
  void drawMessagesButton(juce::Graphics &g, juce::Rectangle<int> bounds);
  void drawStoryButton(juce::Graphics &g, juce::Rectangle<int> bounds);
  void drawProfileSection(juce::Graphics &g, juce::Rectangle<int> bounds);
  void drawCircularProfilePic(juce::Graphics &g, juce::Rectangle<int> bounds);

  // Hit test bounds
  juce::Rectangle<int> getLogoBounds() const;
  juce::Rectangle<int> getSearchButtonBounds() const;
  juce::Rectangle<int> getRecordButtonBounds() const;
  juce::Rectangle<int> getMessagesButtonBounds() const;
  juce::Rectangle<int> getStoryButtonBounds() const;
  juce::Rectangle<int> getProfileBounds() const;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Header)
};
