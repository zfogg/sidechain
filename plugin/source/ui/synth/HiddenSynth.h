#pragma once

#include <JuceHeader.h>
#include "../../audio/SynthEngine.h"

//==============================================================================
/**
 * HiddenSynth provides the UI for the hidden synthesizer easter egg
 *
 * Features:
 * - Oscillator waveform selector
 * - ADSR envelope controls
 * - Filter cutoff and resonance knobs
 * - Preset selector
 * - Visual keyboard for mouse input
 * - Unlock animation when first revealed
 */
class HiddenSynth : public juce::Component,
                    public juce::Timer,
                    public juce::Slider::Listener
{
public:
    HiddenSynth(SynthEngine& engine);
    ~HiddenSynth() override;

    //==============================================================================
    void paint(juce::Graphics&) override;
    void resized() override;

    //==============================================================================
    void timerCallback() override;
    void sliderValueChanged(juce::Slider* slider) override;

    //==============================================================================
    /** Play unlock animation */
    void playUnlockAnimation();

    /** Check if currently showing unlock animation */
    bool isShowingUnlockAnimation() const { return showingUnlockAnimation; }

    /** Callback when back button is pressed */
    std::function<void()> onBackPressed;

private:
    //==============================================================================
    SynthEngine& synthEngine;

    // Animation state
    bool showingUnlockAnimation = false;
    float unlockAnimationProgress = 0.0f;
    static constexpr float unlockAnimationDuration = 2.0f; // seconds

    //==============================================================================
    // Waveform buttons
    juce::TextButton sineButton { "Sine" };
    juce::TextButton sawButton { "Saw" };
    juce::TextButton squareButton { "Square" };
    juce::TextButton triangleButton { "Tri" };

    // ADSR sliders
    juce::Slider attackSlider;
    juce::Slider decaySlider;
    juce::Slider sustainSlider;
    juce::Slider releaseSlider;

    juce::Label attackLabel { {}, "A" };
    juce::Label decayLabel { {}, "D" };
    juce::Label sustainLabel { {}, "S" };
    juce::Label releaseLabel { {}, "R" };

    // Filter controls
    juce::Slider cutoffSlider;
    juce::Slider resonanceSlider;

    juce::Label cutoffLabel { {}, "Cutoff" };
    juce::Label resonanceLabel { {}, "Res" };

    // Volume
    juce::Slider volumeSlider;
    juce::Label volumeLabel { {}, "Vol" };

    // Preset selector
    juce::ComboBox presetSelector;
    std::vector<SynthEngine::Preset> presets;

    // Back button
    juce::TextButton backButton { "<< Back" };

    // Title
    juce::Label titleLabel { {}, "SECRET SYNTH" };

    //==============================================================================
    // Virtual keyboard
    static constexpr int numKeys = 25;  // 2 octaves + 1
    static constexpr int startNote = 48; // C3
    std::array<bool, numKeys> keyStates {};
    juce::Rectangle<int> keyboardArea;

    void drawKeyboard(juce::Graphics& g);
    int getKeyAtPosition(juce::Point<int> pos);
    bool isBlackKey(int keyIndex);

    void mouseDown(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;

    int lastKeyPressed = -1;

    //==============================================================================
    // Drawing helpers
    void drawUnlockAnimation(juce::Graphics& g);
    void drawSynthUI(juce::Graphics& g);
    void drawHeader(juce::Graphics& g);
    void drawVoiceIndicator(juce::Graphics& g);

    //==============================================================================
    // Setup helpers
    void setupSlider(juce::Slider& slider, double min, double max, double default_,
                     double step = 0.01);
    void setupLabel(juce::Label& label);
    void updateWaveformButtons();
    void loadPresetList();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HiddenSynth)
};
