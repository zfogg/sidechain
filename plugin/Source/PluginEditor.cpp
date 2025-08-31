#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
SidechainAudioProcessorEditor::SidechainAudioProcessorEditor (SidechainAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setSize (PLUGIN_WIDTH, PLUGIN_HEIGHT);
    
    DBG("Creating Sidechain Editor with size: " + juce::String(PLUGIN_WIDTH) + "x" + juce::String(PLUGIN_HEIGHT));
    
    // Status label
    statusLabel = std::make_unique<juce::Label>("status", "🎵 Sidechain - Social VST for Producers");
    statusLabel->setJustificationType(juce::Justification::centred);
    statusLabel->setFont(juce::Font(18.0f));
    statusLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    statusLabel->setColour(juce::Label::backgroundColourId, juce::Colours::darkgrey); // Make background visible
    addAndMakeVisible(statusLabel.get());
    
    DBG("Status label created and added");
    
    // Connect button  
    connectButton = std::make_unique<juce::TextButton>("Connect Account");
    connectButton->addListener(this);
    connectButton->setColour(juce::TextButton::buttonColourId, juce::Colours::blue);
    connectButton->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    addAndMakeVisible(connectButton.get());
    
    DBG("Connect button created and added");
}

SidechainAudioProcessorEditor::~SidechainAudioProcessorEditor()
{
}

//==============================================================================
void SidechainAudioProcessorEditor::paint (juce::Graphics& g)
{
    DBG("Paint called with bounds: " + juce::String(getWidth()) + "x" + juce::String(getHeight()));
    
    // Simple dark background for testing
    g.fillAll (juce::Colour::fromRGB(40, 40, 44));
    
    // Add a visible border
    g.setColour (juce::Colours::white);
    g.drawRect(getLocalBounds(), 2);
    
    // Add debug text directly in paint to ensure something is visible
    g.setColour(juce::Colours::white);
    g.setFont(20.0f);
    g.drawText("SIDECHAIN DEBUG", getLocalBounds(), juce::Justification::centred);
}

void SidechainAudioProcessorEditor::resized()
{
    DBG("Editor resized to: " + juce::String(getWidth()) + "x" + juce::String(getHeight()));
    
    auto bounds = getLocalBounds();
    DBG("Local bounds: " + juce::String(bounds.getWidth()) + "x" + juce::String(bounds.getHeight()));
    
    bounds.reduce(20, 20);
    
    // Status label at top (with null check)
    if (statusLabel != nullptr)
    {
        auto labelBounds = bounds.removeFromTop(40);
        statusLabel->setBounds(labelBounds);
        DBG("Status label bounds: " + juce::String(labelBounds.getX()) + "," + juce::String(labelBounds.getY()) + " " + 
            juce::String(labelBounds.getWidth()) + "x" + juce::String(labelBounds.getHeight()));
    }
    
    bounds.removeFromTop(20); // spacing
    
    // Center the connect button (with null check)
    if (connectButton != nullptr)
    {
        auto buttonArea = bounds.removeFromTop(40);
        auto buttonWidth = 200;
        auto buttonX = (buttonArea.getWidth() - buttonWidth) / 2;
        auto buttonBounds = juce::Rectangle<int>(buttonX, buttonArea.getY(), buttonWidth, buttonArea.getHeight());
        connectButton->setBounds(buttonBounds);
        DBG("Connect button bounds: " + juce::String(buttonBounds.getX()) + "," + juce::String(buttonBounds.getY()) + " " + 
            juce::String(buttonBounds.getWidth()) + "x" + juce::String(buttonBounds.getHeight()));
    }
}

void SidechainAudioProcessorEditor::buttonClicked (juce::Button* button)
{
    if (button == connectButton.get() && connectButton != nullptr)
    {
        if (connectButton->getButtonText() == "Connect Account")
        {
            showAuthenticationDialog();
        }
        else
        {
            // Disconnect
            connectButton->setButtonText("Connect Account");
            if (statusLabel != nullptr)
                statusLabel->setText("🎵 Sidechain - Social VST for Producers", juce::dontSendNotification);
        }
    }
}

void SidechainAudioProcessorEditor::showAuthenticationDialog()
{
    // Simple demo authentication with null checks
    if (connectButton != nullptr)
    {
        connectButton->setButtonText("Connecting...");
        connectButton->setEnabled(false);
        
        // Simulate authentication with immediate callback
        connectButton->setButtonText("✅ Connected as DemoUser");
        connectButton->setEnabled(true);
    }
    
    if (statusLabel != nullptr)
    {
        statusLabel->setText("🎵 Sidechain - Ready to share loops!", juce::dontSendNotification);
    }
    
    DBG("Demo authentication completed");
}

