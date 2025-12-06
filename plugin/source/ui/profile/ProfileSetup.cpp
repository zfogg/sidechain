#include "ProfileSetup.h"
#include "../../util/Colors.h"
#include "../../util/Log.h"
#include <thread>

ProfileSetup::ProfileSetup()
{
    Log::info("ProfileSetup: Initializing profile setup component");
    setSize(1000, 800);
    Log::info("ProfileSetup: Initialization complete");
}

ProfileSetup::~ProfileSetup()
{
    Log::debug("ProfileSetup: Destroying profile setup component");
}

void ProfileSetup::setUserInfo(const juce::String& user, const juce::String& userEmail, const juce::String& picUrl)
{
    Log::info("ProfileSetup::setUserInfo: Setting user info - username: " + user +
              ", email: " + userEmail + ", profilePicUrl: " + (picUrl.isNotEmpty() ? picUrl : "empty"));
    username = user;
    email = userEmail;
    profilePicUrl = picUrl;

    // Note: We no longer download images directly here due to JUCE SSL issues on Linux.
    // Instead, UserDataStore downloads via the HTTP proxy and passes the cached image
    // via setProfileImage(). This method just stores the URL for reference.

    repaint();
}

void ProfileSetup::setProfileImage(const juce::Image& image)
{
    if (image.isValid())
    {
        previewImage = image;
        Log::info("ProfileSetup::setProfileImage: Received profile image from cache - " +
            juce::String(image.getWidth()) + "x" + juce::String(image.getHeight()) + " pixels");
        repaint();
    }
    else
    {
        Log::warn("ProfileSetup::setProfileImage: Invalid image provided");
    }
}

void ProfileSetup::setLocalPreviewPath(const juce::String& localPath)
{
    Log::info("ProfileSetup::setLocalPreviewPath: Setting local preview path: " + localPath);
    localPreviewPath = localPath;

    // Load the image immediately for preview
    juce::File imageFile(localPath);
    if (imageFile.existsAsFile())
    {
        previewImage = juce::ImageFileFormat::loadFrom(imageFile);
        if (previewImage.isValid())
        {
            Log::info("ProfileSetup::setLocalPreviewPath: Loaded local preview image - " +
                juce::String(previewImage.getWidth()) + "x" + juce::String(previewImage.getHeight()) + " pixels");
        }
        else
        {
            Log::warn("ProfileSetup::setLocalPreviewPath: Failed to load image from: " + localPath);
        }
    }
    else
    {
        Log::warn("ProfileSetup::setLocalPreviewPath: File does not exist: " + localPath);
    }

    repaint();
}

void ProfileSetup::setProfilePictureUrl(const juce::String& s3Url)
{
    Log::info("ProfileSetup::setProfilePictureUrl: Setting S3 URL: " + s3Url);
    profilePicUrl = s3Url;
    localPreviewPath = ""; // Clear local path since we now have the S3 URL
    Log::debug("ProfileSetup::setProfilePictureUrl: Local preview path cleared");

    // The previewImage should already be set from setLocalPreviewPath,
    // so we don't need to re-download immediately
    repaint();
}

void ProfileSetup::paint(juce::Graphics& g)
{
    // Background
    g.fillAll(SidechainColors::background());

    // Logout button at top-right
    auto logoutBtnBounds = juce::Rectangle<int>(getWidth() - 150, 10, 140, 40);
    g.setColour(SidechainColors::buttonDanger());
    g.fillRoundedRectangle(logoutBtnBounds.toFloat(), 6.0f);
    g.setColour(SidechainColors::textPrimary());
    g.setFont(16.0f);
    g.drawText("Logout", logoutBtnBounds, juce::Justification::centred);

    // Header
    g.setColour(SidechainColors::textPrimary());
    g.setFont(24.0f);
    g.drawText("Complete Your Profile", getLocalBounds().withY(60).withHeight(40), juce::Justification::centred);

    g.setColour(SidechainColors::textSecondary());
    g.setFont(16.0f);
    g.drawText("Welcome " + username + "! Let's set up your profile.", getLocalBounds().withY(110).withHeight(30), juce::Justification::centred);

    // Profile picture area (circular placeholder)
    auto picBounds = juce::Rectangle<int>(200, 140, 150, 150);
    drawCircularProfilePic(g, picBounds);

    // Buttons positioned to the right of the profile picture
    // Upload button
    auto uploadBtn = juce::Rectangle<int>(400, 150, 150, 36);
    g.setColour(SidechainColors::primary());
    g.fillRoundedRectangle(uploadBtn.toFloat(), 6.0f);
    g.setColour(SidechainColors::textPrimary());
    g.setFont(14.0f);
    g.drawText("📸 Upload Photo", uploadBtn, juce::Justification::centred);

    // Skip button
    auto skipBtn = juce::Rectangle<int>(400, 196, 70, 32);
    g.setColour(SidechainColors::buttonSecondary());
    g.fillRoundedRectangle(skipBtn.toFloat(), 4.0f);
    g.setColour(SidechainColors::textPrimary());
    g.drawText("Skip", skipBtn, juce::Justification::centred);

    // Continue button
    auto continueBtn = juce::Rectangle<int>(480, 196, 70, 32);
    g.setColour(SidechainColors::success());
    g.fillRoundedRectangle(continueBtn.toFloat(), 4.0f);
    g.setColour(SidechainColors::background());  // Dark text on mint green
    g.drawText("Continue", continueBtn, juce::Justification::centred);
}

void ProfileSetup::drawCircularProfilePic(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    // Save graphics state before clipping
    juce::Graphics::ScopedSaveState saveState(g);

    // Create circular clipping path
    juce::Path circlePath;
    circlePath.addEllipse(bounds.toFloat());
    g.reduceClipRegion(circlePath);

    // Use cached previewImage if available (from local upload or S3 download)
    if (previewImage.isValid())
    {
        // Scale image to fit circle
        auto scaledImage = previewImage.rescaled(bounds.getWidth(), bounds.getHeight(), juce::Graphics::highResamplingQuality);
        g.drawImageAt(scaledImage, bounds.getX(), bounds.getY());
    }
    else
    {
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

void ProfileSetup::resized()
{
    // Component layout will be handled in paint method
}

void ProfileSetup::mouseUp(const juce::MouseEvent& event)
{
    auto pos = event.getPosition();
    Log::debug("ProfileSetup::mouseUp: Mouse clicked at (" + juce::String(pos.x) + ", " + juce::String(pos.y) + ")");

    // Use same coordinates as paint method
    auto uploadBtn = juce::Rectangle<int>(400, 150, 150, 36);
    auto skipBtn = juce::Rectangle<int>(400, 196, 70, 32);
    auto continueBtn = juce::Rectangle<int>(480, 196, 70, 32);
    auto picBounds = juce::Rectangle<int>(200, 140, 150, 150);
    auto logoutBtn = juce::Rectangle<int>(getWidth() - 150, 10, 140, 40);

    if (uploadBtn.contains(event.getPosition()) || picBounds.contains(event.getPosition()))
    {
        Log::info("ProfileSetup::mouseUp: Upload/Skip button or profile picture area clicked, opening file picker");
        // Open file picker for profile picture
        auto chooser = std::make_shared<juce::FileChooser>("Select Profile Picture", juce::File(), "*.jpg;*.jpeg;*.png;*.gif");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                           [this, chooser](const juce::FileChooser&)
        {
            auto selectedFile = chooser->getResult();
            if (selectedFile.existsAsFile())
            {
                // Update local profilePicUrl and notify parent
                profilePicUrl = selectedFile.getFullPathName(); // Temporary - will be S3 URL
                Log::info("ProfileSetup::mouseUp: Profile picture selected: " + profilePicUrl);

                // Notify parent component
                if (onProfilePicSelected)
                {
                    Log::debug("ProfileSetup::mouseUp: Calling onProfilePicSelected callback");
                    onProfilePicSelected(profilePicUrl);
                }
                else
                {
                    Log::warn("ProfileSetup::mouseUp: onProfilePicSelected callback not set");
                }

                repaint();
            }
            else
            {
                Log::debug("ProfileSetup::mouseUp: File picker cancelled or no file selected");
            }
        });
    }
    else if (skipBtn.contains(event.getPosition()) || continueBtn.contains(event.getPosition()))
    {
        // Both skip and continue go to the feed
        if (skipBtn.contains(event.getPosition()) && onSkipSetup)
        {
            Log::info("ProfileSetup::mouseUp: Skip button clicked");
            onSkipSetup();
        }
        else if (continueBtn.contains(event.getPosition()) && onCompleteSetup)
        {
            Log::info("ProfileSetup::mouseUp: Continue button clicked");
            onCompleteSetup();
        }
        else
        {
            Log::warn("ProfileSetup::mouseUp: Skip/Continue button clicked but callback not set");
        }
    }
    else if (logoutBtn.contains(event.getPosition()))
    {
        Log::info("ProfileSetup::mouseUp: Logout button clicked");
        if (onLogout)
        {
            Log::debug("ProfileSetup::mouseUp: Calling onLogout callback");
            onLogout();
        }
        else
        {
            Log::warn("ProfileSetup::mouseUp: onLogout callback not set");
        }
    }
}

juce::Rectangle<int> ProfileSetup::getButtonArea(int index, int totalButtons)
{
    const int buttonWidth = 200;
    const int buttonHeight = 40;
    const int spacing = 10;

    int totalWidth = totalButtons * buttonWidth + (totalButtons - 1) * spacing;
    int startX = (getWidth() - totalWidth) / 2;

    return juce::Rectangle<int>(startX + index * (buttonWidth + spacing), 0, buttonWidth, buttonHeight);
}