#pragma once

#include "../../stores/AppStore.h"

#include "../../util/reactive/ReactiveBoundComponent.h"
#include "Profile.h"
#include <JuceHeader.h>

class NetworkClient;

//==============================================================================
/**
 * EditProfile provides a modal for editing user profile
 *
 * Features:
 * - Profile picture upload
 * - Display name editing
 * - Bio editing
 * - Location editing
 * - Genre selection
 * - DAW preference
 * - Social links editing
 */
class EditProfile : public Sidechain::Util::ReactiveBoundComponent,
                    public juce::Button::Listener,
                    public juce::TextEditor::Listener {
public:
  EditProfile();
  ~EditProfile() override;

  //==============================================================================
  // Data binding
  void setNetworkClient(NetworkClient *client) {
    networkClient = client;
  }
  void setUserStore(Sidechain::Stores::AppStore *store); // Task 2.4: Use UserStore for profile management

  // Task 2.4: Show modal with current profile from UserStore
  void showWithCurrentProfile(juce::Component *parentComponent);

  // Get pending local path for upload
  const juce::String &getPendingAvatarPath() const {
    return pendingAvatarPath;
  }

  //==============================================================================
  // Modal dialog methods
  void showModal(juce::Component *parentComponent);
  void closeDialog();

  //==============================================================================
  // Navigation callbacks (Task 2.4 - settings section navigation)
  // Note: Profile save is now handled via UserStore subscription
  std::function<void()> onActivityStatusClicked;
  std::function<void()> onMutedUsersClicked;
  std::function<void()> onTwoFactorClicked;
  std::function<void()> onProfileSetupClicked;
  std::function<void()> onLogoutClicked; // Logout button in settings header

  //==============================================================================
  // Component overrides
  void paint(juce::Graphics &g) override;
  void resized() override;
  void buttonClicked(juce::Button *button) override;
  void textEditorTextChanged(juce::TextEditor &editor) override;

private:
  //==============================================================================
  NetworkClient *networkClient = nullptr;
  Sidechain::Stores::AppStore *userStore = nullptr;
  std::function<void()> userStoreUnsubscribe; // Unsubscribe function for UserStore (Task 2.4)

  // Task 2.4: Local form state (tracks what user is editing, not saved state)
  // Saved state comes from UserStore; editors hold unsaved changes
  juce::String originalUsername;  // Username when form opened (to detect changes)
  bool hasUnsavedChanges = false; // Computed from comparing editors to UserStore

  //==============================================================================
  // UI Components
  std::unique_ptr<juce::TextEditor> usernameEditor;
  std::unique_ptr<juce::TextEditor> displayNameEditor;
  std::unique_ptr<juce::TextEditor> bioEditor;
  std::unique_ptr<juce::TextEditor> locationEditor;
  std::unique_ptr<juce::TextEditor> genreEditor;
  std::unique_ptr<juce::TextEditor> dawEditor;

  // Username validation state
  bool isUsernameValid = true;
  bool isCheckingUsername = false;
  juce::String usernameError;

  // Social link editors
  std::unique_ptr<juce::TextEditor> instagramEditor;
  std::unique_ptr<juce::TextEditor> soundcloudEditor;
  std::unique_ptr<juce::TextEditor> spotifyEditor;
  std::unique_ptr<juce::TextEditor> twitterEditor;

  // Buttons
  std::unique_ptr<juce::TextButton> cancelButton;
  std::unique_ptr<juce::TextButton> saveButton;
  std::unique_ptr<juce::TextButton> logoutButton;
  std::unique_ptr<juce::TextButton> changePhotoButton;

  // Privacy toggle
  std::unique_ptr<juce::ToggleButton> privateAccountToggle;

  // Settings section buttons
  std::unique_ptr<juce::TextButton> activityStatusButton;
  std::unique_ptr<juce::TextButton> mutedUsersButton;
  std::unique_ptr<juce::TextButton> twoFactorButton;
  std::unique_ptr<juce::TextButton> profileSetupButton;

  // Avatar
  juce::Image avatarImage;
  juce::String pendingAvatarPath;

  //==============================================================================
  // Drawing methods
  void drawHeader(juce::Graphics &g, juce::Rectangle<int> bounds);
  void drawAvatar(juce::Graphics &g, juce::Rectangle<int> bounds);
  void drawFormSection(juce::Graphics &g, const juce::String &title, juce::Rectangle<int> bounds);

  //==============================================================================
  // Helpers
  void setupEditors();
  void populateFromUserStore(); // Task 2.4: Populate form from UserStore
  void updateHasChanges();      // Task 2.4: Compare editors to UserStore
  void handleSave();            // Task 2.4: Save editors to UserStore
  void handlePhotoSelect();
  void validateUsername(const juce::String &username);

  // Task 2.4: Helper to build social links JSON from editors
  juce::var getSocialLinksFromEditors() const;

  //==============================================================================
  // Hit testing
  juce::Rectangle<int> getAvatarBounds() const;

  //==============================================================================
  // Layout constants
  static constexpr int HEADER_HEIGHT = 60;
  static constexpr int AVATAR_SIZE = 80;
  static constexpr int FIELD_HEIGHT = 40;
  static constexpr int FIELD_SPACING = 15;
  static constexpr int SECTION_SPACING = 25;
  static constexpr int PADDING = 25;

  //==============================================================================
  // Colors
  struct Colors {
    static inline juce::Colour background{0xff1a1a1e};
    static inline juce::Colour headerBg{0xff252529};
    static inline juce::Colour inputBg{0xff2d2d32};
    static inline juce::Colour inputBorder{0xff4a4a4e};
    static inline juce::Colour inputBorderFocused{0xff00d4ff};
    static inline juce::Colour textPrimary{0xffffffff};
    static inline juce::Colour textSecondary{0xffa0a0a0};
    static inline juce::Colour textPlaceholder{0xff707070};
    static inline juce::Colour accent{0xff00d4ff};
    static inline juce::Colour cancelButton{0xff3a3a3e};
    static inline juce::Colour saveButton{0xff00d4ff};
    static inline juce::Colour saveButtonDisabled{0xff3a3a3e};
    static inline juce::Colour errorRed{0xffff4757};
  };

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EditProfile)
};
