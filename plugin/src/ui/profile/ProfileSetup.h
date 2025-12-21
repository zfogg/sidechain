#pragma once

#include "../../stores/AppStore.h"
#include "../common/AppStoreComponent.h"
#include <JuceHeader.h>

// ==============================================================================
/**
 * ProfileSetup - First-time user profile setup wizard
 *
 * Features:
 * - Profile picture selection/upload
 * - Username and display name input
 * - Bio input (optional)
 * - Skip option for quick start
 * - Image preview while uploading
 * - Validation and error handling
 *
 * This component is shown after initial authentication to help users
 * complete their profile before accessing the main feed.
 */
class ProfileSetup : public Sidechain::UI::AppStoreComponent<Sidechain::Stores::UserState> {
public:
  ProfileSetup(Sidechain::Stores::AppStore *store = nullptr);
  ~ProfileSetup() override;

  void paint(juce::Graphics &) override;
  void resized() override;
  void mouseUp(const juce::MouseEvent &event) override;

  // Callback for when user wants to skip profile setup
  std::function<void()> onSkipSetup;

  // Callback for when user completes profile setup
  std::function<void()> onCompleteSetup;

  // Callback for when profile picture is selected
  std::function<void(const juce::String &picUrl)> onProfilePicSelected;

  // Callback for logout
  std::function<void()> onLogout;

  // Set user info
  void setUserInfo(const juce::String &username, const juce::String &email, const juce::String &picUrl = "");

  // Set local preview path while uploading
  void setLocalPreviewPath(const juce::String &localPath);

  // Set the profile picture URL (S3 URL) after successful upload
  void setProfilePictureUrl(const juce::String &s3Url);

  // Set the profile image directly (from UserDataStore's cached image)
  void setProfileImage(const juce::Image &image);

  // Upload progress control
  /** Set the upload progress (0.0 to 1.0) and show progress indicator */
  void setUploadProgress(float progress);

  /** Mark upload as complete with success/failure */
  void setUploadComplete(bool success);

  /** Reset upload state to allow new uploads */
  void resetUploadState();

protected:
  // ==============================================================================
  // AppStoreComponent overrides
  void onAppStateChanged(const Sidechain::Stores::UserState &state) override;
  void subscribeToAppStore();

private:
  juce::String username;
  juce::String email;
  juce::String profilePicUrl;    // The S3 URL for the profile picture
  juce::String localPreviewPath; // Local file path for preview while uploading
  juce::Image previewImage;      // Cached image for display

  // Upload progress state
  bool isUploading = false;
  float uploadProgress = 0.0f;
  bool uploadSuccess = false;

  // Cached button bounds for responsive hit testing
  juce::Rectangle<int> cachedUploadBtn;
  juce::Rectangle<int> cachedSkipBtn;
  juce::Rectangle<int> cachedContinueBtn;
  juce::Rectangle<int> cachedPicBounds;
  juce::Rectangle<int> cachedLogoutBtn;

  // UI helper methods
  void drawCircularProfilePic(juce::Graphics &g, juce::Rectangle<int> bounds);
  juce::Rectangle<int> getButtonArea(int index, int totalButtons);

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProfileSetup)
};