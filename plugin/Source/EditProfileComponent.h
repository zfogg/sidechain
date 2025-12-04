#pragma once

#include <JuceHeader.h>
#include "ProfileComponent.h"

class NetworkClient;

//==============================================================================
/**
 * EditProfileComponent provides a modal for editing user profile
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
class EditProfileComponent : public juce::Component,
                             public juce::Button::Listener,
                             public juce::TextEditor::Listener
{
public:
    EditProfileComponent();
    ~EditProfileComponent() override;

    //==============================================================================
    // Data binding
    void setNetworkClient(NetworkClient* client) { networkClient = client; }
    void setProfile(const UserProfile& profile);
    const UserProfile& getProfile() const { return profile; }

    //==============================================================================
    // Callbacks
    std::function<void()> onCancel;
    std::function<void(const UserProfile& updatedProfile)> onSave;
    std::function<void(const juce::String& localPath)> onProfilePicSelected;

    //==============================================================================
    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;
    void buttonClicked(juce::Button* button) override;
    void textEditorTextChanged(juce::TextEditor& editor) override;

private:
    //==============================================================================
    UserProfile profile;
    UserProfile originalProfile;
    NetworkClient* networkClient = nullptr;

    // Form state
    bool isSaving = false;
    bool hasChanges = false;
    juce::String errorMessage;

    //==============================================================================
    // UI Components
    std::unique_ptr<juce::TextEditor> displayNameEditor;
    std::unique_ptr<juce::TextEditor> bioEditor;
    std::unique_ptr<juce::TextEditor> locationEditor;
    std::unique_ptr<juce::TextEditor> genreEditor;
    std::unique_ptr<juce::TextEditor> dawEditor;

    // Social link editors
    std::unique_ptr<juce::TextEditor> instagramEditor;
    std::unique_ptr<juce::TextEditor> soundcloudEditor;
    std::unique_ptr<juce::TextEditor> spotifyEditor;
    std::unique_ptr<juce::TextEditor> twitterEditor;

    // Buttons
    std::unique_ptr<juce::TextButton> cancelButton;
    std::unique_ptr<juce::TextButton> saveButton;
    std::unique_ptr<juce::TextButton> changePhotoButton;

    // Avatar
    juce::Image avatarImage;
    juce::String pendingAvatarPath;

    //==============================================================================
    // Drawing methods
    void drawHeader(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawAvatar(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawFormSection(juce::Graphics& g, const juce::String& title, juce::Rectangle<int> bounds);

    //==============================================================================
    // Helpers
    void setupEditors();
    void populateFromProfile();
    void collectToProfile();
    void updateHasChanges();
    void handleSave();
    void handlePhotoSelect();

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
    struct Colors
    {
        static inline juce::Colour background { 0xff1a1a1e };
        static inline juce::Colour headerBg { 0xff252529 };
        static inline juce::Colour inputBg { 0xff2d2d32 };
        static inline juce::Colour inputBorder { 0xff4a4a4e };
        static inline juce::Colour inputBorderFocused { 0xff00d4ff };
        static inline juce::Colour textPrimary { 0xffffffff };
        static inline juce::Colour textSecondary { 0xffa0a0a0 };
        static inline juce::Colour textPlaceholder { 0xff707070 };
        static inline juce::Colour accent { 0xff00d4ff };
        static inline juce::Colour cancelButton { 0xff3a3a3e };
        static inline juce::Colour saveButton { 0xff00d4ff };
        static inline juce::Colour saveButtonDisabled { 0xff3a3a3e };
        static inline juce::Colour errorRed { 0xffff4757 };
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EditProfileComponent)
};
