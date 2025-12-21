#include "ProfileSetup.h"
#include "../../util/Colors.h"
#include "../../util/Log.h"
#include <thread>

ProfileSetup::ProfileSetup(Sidechain::Stores::AppStore *store)
    : AppStoreComponent(
          store, [store](auto cb) { return store ? store->subscribeToUser(cb) : std::function<void()>([]() {}); }) {
  Log::info("ProfileSetup: Initializing profile setup component");
  setSize(1000, 800);
  Log::info("ProfileSetup: Initialization complete");
}

// Layout constants for responsive design
namespace ProfileSetupLayout {
constexpr int HEADER_HEIGHT = 60;
constexpr int TITLE_AREA_HEIGHT = 100;
constexpr int PROFILE_PIC_SIZE = 150;
constexpr int BUTTON_HEIGHT = 36;
constexpr int SMALL_BUTTON_HEIGHT = 32;
constexpr int BUTTON_SPACING = 10;
constexpr int CONTENT_WIDTH = 500; // Maximum content width
constexpr int LOGOUT_BUTTON_WIDTH = 140;
constexpr int UPLOAD_BUTTON_WIDTH = 150;
constexpr int SKIP_BUTTON_WIDTH = 70;
constexpr int CONTINUE_BUTTON_WIDTH = 90;
} // namespace ProfileSetupLayout

ProfileSetup::~ProfileSetup() {
  Log::debug("ProfileSetup: Destroying profile setup component");
}

void ProfileSetup::setUserInfo(const juce::String &user, const juce::String &userEmail, const juce::String &picUrl) {
  Log::info("ProfileSetup::setUserInfo: Setting user info - username: " + user + ", email: " + userEmail +
            ", profilePicUrl: " + (picUrl.isNotEmpty() ? picUrl : "empty"));
  username = user;
  email = userEmail;
  profilePicUrl = picUrl;

  // Note: We no longer download images directly here due to JUCE SSL issues on
  // Linux. Instead, UserDataStore downloads via the HTTP proxy and passes the
  // cached image via setProfileImage. This method just stores the URL for
  // reference.

  repaint();
}

void ProfileSetup::setProfileImage(const juce::Image &image) {
  if (image.isValid()) {
    previewImage = image;
    Log::info("ProfileSetup::setProfileImage: Received profile image from cache - " + juce::String(image.getWidth()) +
              "x" + juce::String(image.getHeight()) + " pixels");
    repaint();
  } else {
    Log::warn("ProfileSetup::setProfileImage: Invalid image provided");
  }
}

void ProfileSetup::setLocalPreviewPath(const juce::String &localPath) {
  Log::info("ProfileSetup::setLocalPreviewPath: Setting local preview path: " + localPath);
  localPreviewPath = localPath;

  // Load the image immediately for preview
  juce::File imageFile(localPath);
  if (imageFile.existsAsFile()) {
    previewImage = juce::ImageFileFormat::loadFrom(imageFile);
    if (previewImage.isValid()) {
      Log::info("ProfileSetup::setLocalPreviewPath: Loaded local preview image - " +
                juce::String(previewImage.getWidth()) + "x" + juce::String(previewImage.getHeight()) + " pixels");
    } else {
      Log::warn("ProfileSetup::setLocalPreviewPath: Failed to load image from: " + localPath);
    }
  } else {
    Log::warn("ProfileSetup::setLocalPreviewPath: File does not exist: " + localPath);
  }

  repaint();
}

void ProfileSetup::setProfilePictureUrl(const juce::String &s3Url) {
  Log::info("ProfileSetup::setProfilePictureUrl: Setting S3 URL: " + s3Url);
  profilePicUrl = s3Url;
  localPreviewPath = ""; // Clear local path since we now have the S3 URL
  Log::debug("ProfileSetup::setProfilePictureUrl: Local preview path cleared");

  // The previewImage should already be set from setLocalPreviewPath,
  // so we don't need to re-download immediately
  repaint();
}

void ProfileSetup::setUploadProgress(float progress) {
  isUploading = true;
  uploadProgress = juce::jlimit(0.0f, 1.0f, progress);
  uploadSuccess = false;
  Log::debug("ProfileSetup::setUploadProgress: Progress = " + juce::String(static_cast<int>(progress * 100)) + "%");
  repaint();
}

void ProfileSetup::setUploadComplete(bool success) {
  isUploading = false;
  uploadProgress = success ? 1.0f : 0.0f;
  uploadSuccess = success;
  Log::info("ProfileSetup::setUploadComplete: Upload " + juce::String(success ? "succeeded" : "failed"));
  repaint();

  // If success, auto-hide the success message after 3 seconds
  if (success) {
    juce::Timer::callAfterDelay(3000, [this]() {
      if (uploadSuccess) {
        // Keep showing success, but could reset here if desired
      }
    });
  }
}

void ProfileSetup::resetUploadState() {
  isUploading = false;
  uploadProgress = 0.0f;
  uploadSuccess = false;
  Log::debug("ProfileSetup::resetUploadState: Upload state reset");
  repaint();
}

void ProfileSetup::paint(juce::Graphics &g) {
  using namespace ProfileSetupLayout;

  // Background
  g.fillAll(SidechainColors::background());

  // Calculate responsive positions
  [[maybe_unused]] const int contentWidth = juce::jmin(CONTENT_WIDTH, getWidth() - 40); // Cap at max width with padding

  // Logout button at top-right (responsive)
  auto logoutBtnBounds = juce::Rectangle<int>(getWidth() - LOGOUT_BUTTON_WIDTH - 10, 10, LOGOUT_BUTTON_WIDTH, 40);
  g.setColour(SidechainColors::buttonDanger());
  g.fillRoundedRectangle(logoutBtnBounds.toFloat(), 6.0f);
  g.setColour(SidechainColors::textPrimary());
  g.setFont(16.0f);
  g.drawText("Logout", logoutBtnBounds, juce::Justification::centred);

  // Header - centered with responsive width
  auto headerArea = getLocalBounds().withY(HEADER_HEIGHT).withHeight(40);
  g.setColour(SidechainColors::textPrimary());
  g.setFont(24.0f);
  g.drawText("Complete Your Profile", headerArea, juce::Justification::centred);

  g.setColour(SidechainColors::textSecondary());
  g.setFont(16.0f);
  auto subtitleArea = getLocalBounds().withY(HEADER_HEIGHT + 50).withHeight(30);
  g.drawText("Welcome " + username + "! Let's set up your profile.", subtitleArea, juce::Justification::centred);

  // Calculate content area - profile pic on left, buttons on right
  int contentY = HEADER_HEIGHT + TITLE_AREA_HEIGHT;
  int totalContentWidth = PROFILE_PIC_SIZE + 30 + UPLOAD_BUTTON_WIDTH; // pic + gap + buttons
  int contentAreaStartX = (getWidth() - totalContentWidth) / 2;

  // Profile picture area (circular placeholder) - responsive centering
  auto picBounds = juce::Rectangle<int>(contentAreaStartX, contentY, PROFILE_PIC_SIZE, PROFILE_PIC_SIZE);
  drawCircularProfilePic(g, picBounds);

  // Buttons positioned to the right of the profile picture
  int buttonX = picBounds.getRight() + 30;
  int buttonY = contentY + 10; // Align with top of profile pic + small offset

  // Upload button
  auto uploadBtn = juce::Rectangle<int>(buttonX, buttonY, UPLOAD_BUTTON_WIDTH, BUTTON_HEIGHT);
  g.setColour(SidechainColors::primary());
  g.fillRoundedRectangle(uploadBtn.toFloat(), 6.0f);
  g.setColour(SidechainColors::textPrimary());
  g.setFont(14.0f);

  // Show upload progress or button text
  if (isUploading) {
    // Draw progress bar background
    g.setColour(SidechainColors::primary().darker(0.3f));
    g.fillRoundedRectangle(uploadBtn.toFloat(), 6.0f);

    // Draw progress bar fill
    auto progressWidth = static_cast<int>(static_cast<float>(uploadBtn.getWidth()) * uploadProgress);
    auto progressBounds = uploadBtn.withWidth(progressWidth);
    g.setColour(SidechainColors::primary());
    g.fillRoundedRectangle(progressBounds.toFloat(), 6.0f);

    // Draw progress text
    g.setColour(SidechainColors::textPrimary());
    juce::String progressText = "Uploading " + juce::String(static_cast<int>(uploadProgress * 100)) + "%";
    g.drawText(progressText, uploadBtn, juce::Justification::centred);
  } else if (uploadSuccess) {
    // Show success state
    g.setColour(SidechainColors::success());
    g.fillRoundedRectangle(uploadBtn.toFloat(), 6.0f);
    g.setColour(SidechainColors::background());
    g.drawText("[OK] Uploaded!", uploadBtn, juce::Justification::centred);
  } else {
    g.drawText("Upload Photo", uploadBtn, juce::Justification::centred);
  }

  // Skip and Continue buttons below upload button
  int actionButtonY = buttonY + BUTTON_HEIGHT + BUTTON_SPACING;

  // Skip button
  auto skipBtn = juce::Rectangle<int>(buttonX, actionButtonY, SKIP_BUTTON_WIDTH, SMALL_BUTTON_HEIGHT);
  g.setColour(SidechainColors::buttonSecondary());
  g.fillRoundedRectangle(skipBtn.toFloat(), 4.0f);
  g.setColour(SidechainColors::textPrimary());
  g.drawText("Skip", skipBtn, juce::Justification::centred);

  // Continue button
  auto continueBtn = juce::Rectangle<int>(buttonX + SKIP_BUTTON_WIDTH + BUTTON_SPACING, actionButtonY,
                                          CONTINUE_BUTTON_WIDTH, SMALL_BUTTON_HEIGHT);
  g.setColour(SidechainColors::success());
  g.fillRoundedRectangle(continueBtn.toFloat(), 4.0f);
  g.setColour(SidechainColors::background()); // Dark text on mint green
  g.drawText("Continue", continueBtn, juce::Justification::centred);

  // Store button bounds for hit testing
  cachedUploadBtn = uploadBtn;
  cachedSkipBtn = skipBtn;
  cachedContinueBtn = continueBtn;
  cachedPicBounds = picBounds;
  cachedLogoutBtn = logoutBtnBounds;
}

void ProfileSetup::drawCircularProfilePic(juce::Graphics &g, juce::Rectangle<int> bounds) {
  // Save graphics state before clipping
  juce::Graphics::ScopedSaveState saveState(g);

  // Create circular clipping path
  juce::Path circlePath;
  circlePath.addEllipse(bounds.toFloat());
  g.reduceClipRegion(circlePath);

  // Use cached previewImage if available (from local upload or S3 download)
  if (previewImage.isValid()) {
    // Scale image to fit circle
    auto scaledImage =
        previewImage.rescaled(bounds.getWidth(), bounds.getHeight(), juce::Graphics::highResamplingQuality);
    g.drawImageAt(scaledImage, bounds.getX(), bounds.getY());
  } else {
    // Default profile picture placeholder with initials
    g.setColour(SidechainColors::surface());
    g.fillEllipse(bounds.toFloat());

    g.setColour(SidechainColors::textMuted());
    g.setFont(36.0f);
    juce::String initials = username.substring(0, 2).toUpperCase();
    g.drawText(initials, bounds, juce::Justification::centred);
  }

  // Border
  g.setColour(SidechainColors::textSecondary());
  g.drawEllipse(bounds.toFloat(), 2.0f);
}

void ProfileSetup::resized() {
  // Component layout will be handled in paint method
}

void ProfileSetup::mouseUp(const juce::MouseEvent &event) {
  auto pos = event.getPosition();
  Log::debug("ProfileSetup::mouseUp: Mouse clicked at (" + juce::String(pos.x) + ", " + juce::String(pos.y) + ")");

  // Use cached bounds from last paint call for responsive hit testing
  if (cachedUploadBtn.contains(pos) || cachedPicBounds.contains(pos)) {
    // Don't allow clicking during upload
    if (isUploading) {
      Log::debug("ProfileSetup::mouseUp: Upload in progress, ignoring click");
      return;
    }

    Log::info("ProfileSetup::mouseUp: Upload button or profile picture area "
              "clicked, opening file picker");
    // Open file picker for profile picture
    auto chooser =
        std::make_shared<juce::FileChooser>("Select Profile Picture", juce::File(), "*.jpg;*.jpeg;*.png;*.gif");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                         [this, chooser](const juce::FileChooser &) {
                           auto selectedFile = chooser->getResult();
                           if (selectedFile.existsAsFile()) {
                             // Update local profilePicUrl and notify parent
                             profilePicUrl = selectedFile.getFullPathName(); // Temporary - will be S3 URL
                             Log::info("ProfileSetup::mouseUp: Profile picture selected: " + profilePicUrl);

                             // Reset upload states
                             uploadSuccess = false;

                             // Notify parent component (which will call setUploadProgress during
                             // upload)
                             if (onProfilePicSelected) {
                               Log::debug("ProfileSetup::mouseUp: Calling onProfilePicSelected "
                                          "callback");
                               onProfilePicSelected(profilePicUrl);
                             } else {
                               Log::warn("ProfileSetup::mouseUp: onProfilePicSelected callback "
                                         "not set");
                             }

                             repaint();
                           } else {
                             Log::debug("ProfileSetup::mouseUp: File picker cancelled or no "
                                        "file selected");
                           }
                         });
  } else if (cachedSkipBtn.contains(pos)) {
    Log::info("ProfileSetup::mouseUp: Skip button clicked");
    if (onSkipSetup) {
      onSkipSetup();
    } else {
      Log::warn("ProfileSetup::mouseUp: onSkipSetup callback not set");
    }
  } else if (cachedContinueBtn.contains(pos)) {
    Log::info("ProfileSetup::mouseUp: Continue button clicked");
    if (onCompleteSetup) {
      onCompleteSetup();
    } else {
      Log::warn("ProfileSetup::mouseUp: onCompleteSetup callback not set");
    }
  } else if (cachedLogoutBtn.contains(pos)) {
    Log::info("ProfileSetup::mouseUp: Logout button clicked");
    if (onLogout) {
      Log::debug("ProfileSetup::mouseUp: Calling onLogout callback");
      onLogout();
    } else {
      Log::warn("ProfileSetup::mouseUp: onLogout callback not set");
    }
  }
}

juce::Rectangle<int> ProfileSetup::getButtonArea(int index, int totalButtons) {
  const int buttonWidth = 200;
  const int buttonHeight = 40;
  const int spacing = 10;

  int totalWidth = totalButtons * buttonWidth + (totalButtons - 1) * spacing;
  int startX = (getWidth() - totalWidth) / 2;

  return juce::Rectangle<int>(startX + index * (buttonWidth + spacing), 0, buttonWidth, buttonHeight);
}

// ==============================================================================
// AppStoreComponent overrides
// ==============================================================================
void ProfileSetup::onAppStateChanged(const Sidechain::Stores::UserState &state) {
  // Update UI from UserState
  username = state.username;
  email = state.email;
  profilePicUrl = state.profilePictureUrl;

  if (state.profileImage.isValid()) {
    previewImage = state.profileImage;
  }

  repaint();
}
