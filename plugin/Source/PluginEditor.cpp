#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
SidechainAudioProcessorEditor::SidechainAudioProcessorEditor (SidechainAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setSize (PLUGIN_WIDTH, PLUGIN_HEIGHT);
    
    // Status label
    statusLabel = std::make_unique<juce::Label>("status", "🎵 Sidechain - Social VST for Producers");
    statusLabel->setJustificationType(juce::Justification::centred);
    statusLabel->setFont(juce::Font(18.0f, juce::Font::bold));
    statusLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(statusLabel.get());
    
    // Connect button
    connectButton = std::make_unique<juce::TextButton>("Connect Account");
    connectButton->addListener(this);
    addAndMakeVisible(connectButton.get());
}

SidechainAudioProcessorEditor::~SidechainAudioProcessorEditor()
{
}

//==============================================================================
void SidechainAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Dark theme background
    g.fillAll (juce::Colour::fromRGB(24, 24, 28));
    
    // Gradient background
    juce::ColourGradient gradient(juce::Colour::fromRGB(32, 32, 36), 0, 0,
                                 juce::Colour::fromRGB(24, 24, 28), 0, getHeight(), false);
    g.setGradientFill(gradient);
    g.fillAll();
    
    // Border
    g.setColour (juce::Colour::fromRGB(64, 64, 68));
    g.drawRoundedRectangle(getLocalBounds().toFloat(), 4.0f, 1.0f);
}

void SidechainAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    bounds.reduce(20, 20);
    
    // Status label at top
    statusLabel->setBounds(bounds.removeFromTop(40));
    
    bounds.removeFromTop(20); // spacing
    
    // Center the connect button
    auto buttonArea = bounds.removeFromTop(40);
    auto buttonWidth = 200;
    auto buttonX = (buttonArea.getWidth() - buttonWidth) / 2;
    connectButton->setBounds(buttonX, buttonArea.getY(), buttonWidth, buttonArea.getHeight());
}

void SidechainAudioProcessorEditor::buttonClicked (juce::Button* button)
{
    if (button == connectButton.get())
    {
        // For now, just toggle the button text
        if (connectButton->getButtonText() == "Connect Account")
        {
            connectButton->setButtonText("✅ Connected");
            statusLabel->setText("🎵 Sidechain - Ready to share loops!", juce::dontSendNotification);
        }
        else
        {
            connectButton->setButtonText("Connect Account");
            statusLabel->setText("🎵 Sidechain - Social VST for Producers", juce::dontSendNotification);
        }
    }
}