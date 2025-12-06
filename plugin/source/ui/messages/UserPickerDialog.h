#pragma once

#include <JuceHeader.h>
#include "../../network/NetworkClient.h"
#include "../social/UserCard.h"

class NetworkClient;

//==============================================================================
/**
 * UserPickerDialog provides a modal dialog for selecting users
 *
 * Features:
 * - Search input for finding users
 * - Scrollable list of user results
 * - Multi-select support
 * - "Add" button to confirm selection
 */
class UserPickerDialog : public juce::Component,
                         public juce::TextEditor::Listener,
                         public juce::Button::Listener,
                         public juce::Timer
{
public:
    UserPickerDialog();
    ~UserPickerDialog() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseUp(const juce::MouseEvent& event) override;
    void textEditorTextChanged(juce::TextEditor& editor) override;
    void textEditorReturnKeyPressed(juce::TextEditor& editor) override;
    void buttonClicked(juce::Button* button) override;
    void timerCallback() override;

    // Setup
    void setNetworkClient(NetworkClient* client);
    void setCurrentUserId(const juce::String& userId) { currentUserId = userId; }
    void setExcludedUserIds(const juce::Array<juce::String>& excluded) { excludedUserIds = excluded; }

    // Callback when users are selected
    std::function<void(const juce::Array<juce::String>& selectedUserIds)> onUsersSelected;

    // Show as modal dialog
    void showModal(juce::Component* parentComponent);

private:
    NetworkClient* networkClient = nullptr;
    juce::String currentUserId;
    juce::Array<juce::String> excludedUserIds;  // User IDs to exclude from results

    // UI Components
    std::unique_ptr<juce::TextEditor> searchInput;
    std::unique_ptr<juce::TextButton> addButton;
    std::unique_ptr<juce::TextButton> cancelButton;
    std::unique_ptr<juce::ScrollBar> scrollBar;

    // Data
    juce::Array<DiscoveredUser> searchResults;
    juce::Array<juce::String> selectedUserIds;
    bool isSearching = false;
    juce::String currentQuery;

    // Layout
    static constexpr int DIALOG_WIDTH = 500;
    static constexpr int DIALOG_HEIGHT = 600;
    static constexpr int HEADER_HEIGHT = 60;
    static constexpr int INPUT_HEIGHT = 50;
    static constexpr int BUTTON_HEIGHT = 40;
    static constexpr int ITEM_HEIGHT = 60;
    double scrollPosition = 0.0;

    // Drawing
    void drawHeader(juce::Graphics& g);
    void drawResults(juce::Graphics& g);
    void drawUserItem(juce::Graphics& g, const DiscoveredUser& user, int y, int width, bool isSelected);

    // Helpers
    void performSearch();
    void toggleUserSelection(const juce::String& userId);
    bool isUserSelected(const juce::String& userId) const;
    juce::Rectangle<int> getUserItemBounds(int index) const;
    int getItemIndexAtY(int y) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UserPickerDialog)
};
