#include "ActivityStatusSettings.h"
#include "../../network/NetworkClient.h"
#include "../../util/Log.h"
#include "../../util/Result.h"

//==============================================================================
ActivityStatusSettings::ActivityStatusSettings()
{
    Log::info("ActivityStatusSettings: Initializing");
    setSize(400, 320);
    setupToggles();
}

ActivityStatusSettings::~ActivityStatusSettings()
{
    Log::debug("ActivityStatusSettings: Destroying");
}

//==============================================================================
void ActivityStatusSettings::setupToggles()
{
    auto styleToggle = [this](juce::ToggleButton& toggle, const juce::String& label) {
        toggle.setButtonText(label);
        toggle.setColour(juce::ToggleButton::textColourId, Colors::textPrimary);
        toggle.setColour(juce::ToggleButton::tickColourId, Colors::accent);
        toggle.setColour(juce::ToggleButton::tickDisabledColourId, Colors::textSecondary);
        toggle.setToggleState(true, juce::dontSendNotification);
        toggle.onClick = [this, &toggle]() { handleToggleChange(&toggle); };
        addAndMakeVisible(toggle);
    };

    // Create toggle buttons
    showActivityStatusToggle = std::make_unique<juce::ToggleButton>();
    styleToggle(*showActivityStatusToggle, "Show Activity Status");

    showLastActiveToggle = std::make_unique<juce::ToggleButton>();
    styleToggle(*showLastActiveToggle, "Show Last Active Time");

    // Close button
    closeButton = std::make_unique<juce::TextButton>("Close");
    closeButton->setColour(juce::TextButton::buttonColourId, Colors::closeButton);
    closeButton->setColour(juce::TextButton::textColourOffId, Colors::textSecondary);
    closeButton->addListener(this);
    addAndMakeVisible(closeButton.get());
}

void ActivityStatusSettings::loadSettings()
{
    if (networkClient == nullptr)
    {
        Log::error("ActivityStatusSettings: No network client set");
        return;
    }

    isLoading = true;
    errorMessage = "";
    repaint();

    networkClient->get("/settings/activity-status", [this](Outcome<juce::var> result) {
        juce::MessageManager::callAsync([this, result]() {
            isLoading = false;

            if (result.isOk())
            {
                auto response = result.getValue();

                showActivityStatus = response.getProperty("show_activity_status", true);
                showLastActive = response.getProperty("show_last_active", true);

                populateFromSettings();
                Log::info("ActivityStatusSettings: Settings loaded successfully");
            }
            else
            {
                errorMessage = "Failed to load settings: " + result.getError();
                Log::error("ActivityStatusSettings: " + errorMessage);
            }

            repaint();
        });
    });
}

void ActivityStatusSettings::populateFromSettings()
{
    showActivityStatusToggle->setToggleState(showActivityStatus, juce::dontSendNotification);
    showLastActiveToggle->setToggleState(showLastActive, juce::dontSendNotification);
}

void ActivityStatusSettings::handleToggleChange(juce::ToggleButton* toggle)
{
    // Update local state based on which toggle changed
    if (toggle == showActivityStatusToggle.get())
        showActivityStatus = toggle->getToggleState();
    else if (toggle == showLastActiveToggle.get())
        showLastActive = toggle->getToggleState();

    // Save immediately when changed
    saveSettings();
}

void ActivityStatusSettings::saveSettings()
{
    if (networkClient == nullptr || isSaving)
        return;

    isSaving = true;
    errorMessage = "";

    // Build update payload
    auto* updateData = new juce::DynamicObject();
    updateData->setProperty("show_activity_status", showActivityStatus);
    updateData->setProperty("show_last_active", showLastActive);

    juce::var payload(updateData);

    networkClient->put("/settings/activity-status", payload, [this](Outcome<juce::var> result) {
        juce::MessageManager::callAsync([this, result]() {
            isSaving = false;

            if (result.isOk())
            {
                Log::info("ActivityStatusSettings: Settings saved successfully");
            }
            else
            {
                errorMessage = "Failed to save: " + result.getError();
                Log::error("ActivityStatusSettings: " + errorMessage);
            }

            repaint();
        });
    });
}

//==============================================================================
void ActivityStatusSettings::paint(juce::Graphics& g)
{
    // Background
    g.fillAll(Colors::background);

    // Header
    auto headerBounds = getLocalBounds().removeFromTop(HEADER_HEIGHT);
    drawHeader(g, headerBounds);

    // Description text below toggles
    int y = HEADER_HEIGHT + PADDING + TOGGLE_HEIGHT;
    drawDescription(g, juce::Rectangle<int>(PADDING, y, getWidth() - PADDING * 2, DESCRIPTION_HEIGHT),
                    "When off, others won't see if you're online.");

    y += TOGGLE_HEIGHT + DESCRIPTION_HEIGHT;
    drawDescription(g, juce::Rectangle<int>(PADDING, y, getWidth() - PADDING * 2, DESCRIPTION_HEIGHT),
                    "When off, others won't see your last active time.");

    // Loading indicator
    if (isLoading)
    {
        g.setColour(Colors::textSecondary);
        g.setFont(14.0f);
        g.drawText("Loading...", getLocalBounds(), juce::Justification::centred);
    }

    // Error message
    if (errorMessage.isNotEmpty())
    {
        g.setColour(Colors::errorRed);
        g.setFont(12.0f);
        g.drawText(errorMessage, PADDING, getHeight() - 50, getWidth() - PADDING * 2, 20,
                   juce::Justification::centred);
    }
}

void ActivityStatusSettings::drawHeader(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    g.setColour(Colors::headerBg);
    g.fillRect(bounds);

    // Title
    g.setColour(Colors::textPrimary);
    g.setFont(juce::Font(18.0f, juce::Font::bold));
    g.drawText("Activity Status", bounds, juce::Justification::centred);

    // Bottom border
    g.setColour(Colors::toggleBorder);
    g.drawLine(0, static_cast<float>(bounds.getBottom()), static_cast<float>(getWidth()),
               static_cast<float>(bounds.getBottom()), 1.0f);
}

void ActivityStatusSettings::drawDescription(juce::Graphics& g, juce::Rectangle<int> bounds, const juce::String& text)
{
    g.setColour(Colors::textSecondary);
    g.setFont(12.0f);
    g.drawText(text, bounds, juce::Justification::centredLeft);
}

//==============================================================================
void ActivityStatusSettings::resized()
{
    // Close button in header
    closeButton->setBounds(getWidth() - PADDING - 60, 15, 60, 30);

    int y = HEADER_HEIGHT + PADDING;
    int toggleWidth = getWidth() - PADDING * 2;

    // Activity status toggle
    showActivityStatusToggle->setBounds(PADDING, y, toggleWidth, TOGGLE_HEIGHT);
    y += TOGGLE_HEIGHT + DESCRIPTION_HEIGHT;

    // Last active toggle
    showLastActiveToggle->setBounds(PADDING, y, toggleWidth, TOGGLE_HEIGHT);
}

//==============================================================================
void ActivityStatusSettings::buttonClicked(juce::Button* button)
{
    if (button == closeButton.get())
    {
        if (onClose)
            onClose();
    }
}
