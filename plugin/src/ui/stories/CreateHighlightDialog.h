#pragma once

#include <JuceHeader.h>

class NetworkClient;

//==============================================================================
/**
 * CreateHighlightDialog - Modal dialog for creating a new story highlight
 *
 * Features:
 * - Text input for highlight name
 * - Optional description input
 * - Create and Cancel buttons
 * - Animated appearance
 */
class CreateHighlightDialog : public juce::Component,
                               public juce::Button::Listener
{
public:
    CreateHighlightDialog();
    ~CreateHighlightDialog() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void buttonClicked(juce::Button* button) override;

    // Setup
    void setNetworkClient(NetworkClient* client) { networkClient = client; }

    // Callbacks
    std::function<void(const juce::String& highlightId)> onHighlightCreated;
    std::function<void()> onCancelled;

    // Show as modal overlay
    void showModal(juce::Component* parentComponent);
    void closeDialog();

private:
    NetworkClient* networkClient = nullptr;

    // UI Components
    std::unique_ptr<juce::TextEditor> nameInput;
    std::unique_ptr<juce::TextEditor> descriptionInput;
    std::unique_ptr<juce::TextButton> createButton;
    std::unique_ptr<juce::TextButton> cancelButton;

    // State
    bool isCreating = false;
    juce::String errorMessage;

    // Layout constants
    static constexpr int DIALOG_WIDTH = 400;
    static constexpr int DIALOG_HEIGHT = 300;
    static constexpr int PADDING = 20;
    static constexpr int INPUT_HEIGHT = 44;
    static constexpr int BUTTON_HEIGHT = 44;
    static constexpr int LABEL_HEIGHT = 20;

    // Drawing
    void drawHeader(juce::Graphics& g);
    void drawError(juce::Graphics& g);

    // Actions
    void createHighlight();
    void updateCreateButtonState();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CreateHighlightDialog)
};
