#pragma once

#include "../../util/HoverState.h"
#include <JuceHeader.h>

namespace Sidechain {
namespace Stores {
class AppStore;
}
} // namespace Sidechain

// ==============================================================================
/**
 * DiscoveredUser represents a user result from search or discovery endpoints
 */
struct DiscoveredUser {
  juce::String id;
  juce::String username;
  juce::String displayName;
  juce::String bio;
  juce::String avatarUrl;
  juce::String genre;
  int followerCount = 0;
  int postCount = 0;
  bool isFollowing = false;
  bool isOnline = false;        // Online presence indicator
  bool isInStudio = false;      // Currently producing in DAW
  juce::String currentDAW;      // Which DAW they're using (if in studio)
  float similarityScore = 0.0f; // For "similar users" results

  static DiscoveredUser fromJson(const juce::var &json) {
    DiscoveredUser user;
    if (json.isObject()) {
      user.id = json.getProperty("id", "").toString();
      user.username = json.getProperty("username", "").toString();
      user.displayName = json.getProperty("display_name", "").toString();
      user.bio = json.getProperty("bio", "").toString();
      user.avatarUrl = json.getProperty("profile_picture_url", "").toString();
      if (user.avatarUrl.isEmpty())
        user.avatarUrl = json.getProperty("avatar_url", "").toString();
      user.genre = json.getProperty("genre", "").toString();
      user.followerCount = static_cast<int>(json.getProperty("follower_count", 0));
      user.postCount = static_cast<int>(json.getProperty("post_count", 0));
      user.isFollowing = static_cast<bool>(json.getProperty("is_following", false));
      user.isOnline = static_cast<bool>(json.getProperty("is_online", false));
      user.similarityScore = static_cast<float>(json.getProperty("similarity_score", 0.0));

      // Parse presence info if available
      auto presence = json.getProperty("presence", juce::var());
      if (presence.isObject()) {
        auto status = presence.getProperty("status", "").toString();
        user.isOnline = (status == "online" || status == "in_studio");
        user.isInStudio = (status == "in_studio");
        user.currentDAW = presence.getProperty("daw", "").toString();
      }
    }
    return user;
  }

  juce::String getDisplayNameOrUsername() const {
    return displayName.isEmpty() ? username : displayName;
  }
};

// ==============================================================================
/**
 * UserCard displays a user in a compact card format for discovery/search
 *
 * Features:
 * - Avatar with circular clip and fallback to initials
 * - Display name and username
 * - Genre badge
 * - Follower count
 * - Follow/unfollow button
 */
class UserCard : public juce::Component {
public:
  UserCard();
  ~UserCard() override;

  // ==============================================================================
  // Data binding
  void setUser(const DiscoveredUser &user);
  const DiscoveredUser &getUser() const {
    return user;
  }
  juce::String getUserId() const {
    return user.id;
  }

  // Update follow state
  void setIsFollowing(bool following);

  // Set the app store for image caching
  void setAppStore(Sidechain::Stores::AppStore *store) {
    appStore = store;
  }

  // ==============================================================================
  // Callbacks
  std::function<void(const DiscoveredUser &)> onUserClicked;
  std::function<void(const DiscoveredUser &, bool willFollow)> onFollowToggled;

  // ==============================================================================
  // Component overrides
  void paint(juce::Graphics &g) override;
  void resized() override;
  void mouseUp(const juce::MouseEvent &event) override;
  void mouseEnter(const juce::MouseEvent &event) override;
  void mouseExit(const juce::MouseEvent &event) override;

  // ==============================================================================
  // Layout constants
  static constexpr int CARD_HEIGHT = 72;
  static constexpr int AVATAR_SIZE = 48;

private:
  DiscoveredUser user;
  HoverState hoverState;
  Sidechain::Stores::AppStore *appStore = nullptr;

  // ==============================================================================
  // Drawing methods
  void drawBackground(juce::Graphics &g);
  void drawAvatar(juce::Graphics &g, juce::Rectangle<int> bounds);
  void drawUserInfo(juce::Graphics &g, juce::Rectangle<int> bounds);
  void drawFollowButton(juce::Graphics &g, juce::Rectangle<int> bounds);
  void drawGenreBadge(juce::Graphics &g, juce::Rectangle<int> bounds);

  // ==============================================================================
  // Hit testing helpers
  juce::Rectangle<int> getAvatarBounds() const;
  juce::Rectangle<int> getUserInfoBounds() const;
  juce::Rectangle<int> getFollowButtonBounds() const;

  // ==============================================================================
  // Colors (matching app theme)
  struct Colors {
    static inline juce::Colour background{0xff2d2d32};
    static inline juce::Colour backgroundHover{0xff3a3a3e};
    static inline juce::Colour textPrimary{0xffffffff};
    static inline juce::Colour textSecondary{0xffa0a0a0};
    static inline juce::Colour accent{0xff00d4ff};
    static inline juce::Colour followButton{0xffc9d9fb}; // Light blue
    static inline juce::Colour followingButton{0xff3a3a3e};
    static inline juce::Colour badge{0xff3a3a3e};
    static inline juce::Colour onlineIndicator{0xff00d464};   // Green dot for online
    static inline juce::Colour inStudioIndicator{0xff00d4ff}; // Cyan for "in studio"
  };

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UserCard)
};
