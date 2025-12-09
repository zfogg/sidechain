// Dummy source file to generate JuceHeader.h
// This file is used by SidechainJuceHeaderHelper to trigger JUCE header generation
#include <JuceHeader.h>

// Minimal JUCE GUI application class for header generation
class JuceHeaderHelperApp : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "JuceHeaderHelper"; }
    const juce::String getApplicationVersion() override { return "1.0.0"; }
    void initialise(const juce::String&) override {}
    void shutdown() override {}
};

// Required macro for JUCE GUI apps - provides WinMain on Windows
START_JUCE_APPLICATION(JuceHeaderHelperApp)

