#include "HiddenSynth.h"

//==============================================================================
HiddenSynth::HiddenSynth(SynthEngine& engine)
    : synthEngine(engine)
{
    // Setup waveform buttons
    addAndMakeVisible(sineButton);
    addAndMakeVisible(sawButton);
    addAndMakeVisible(squareButton);
    addAndMakeVisible(triangleButton);

    sineButton.onClick = [this]() {
        synthEngine.setWaveform(SynthEngine::Waveform::Sine);
        updateWaveformButtons();
    };
    sawButton.onClick = [this]() {
        synthEngine.setWaveform(SynthEngine::Waveform::Saw);
        updateWaveformButtons();
    };
    squareButton.onClick = [this]() {
        synthEngine.setWaveform(SynthEngine::Waveform::Square);
        updateWaveformButtons();
    };
    triangleButton.onClick = [this]() {
        synthEngine.setWaveform(SynthEngine::Waveform::Triangle);
        updateWaveformButtons();
    };

    // Setup ADSR sliders
    setupSlider(attackSlider, 0.001, 2.0, 0.01, 0.001);
    setupSlider(decaySlider, 0.001, 2.0, 0.1, 0.001);
    setupSlider(sustainSlider, 0.0, 1.0, 0.7, 0.01);
    setupSlider(releaseSlider, 0.001, 3.0, 0.3, 0.001);

    setupLabel(attackLabel);
    setupLabel(decayLabel);
    setupLabel(sustainLabel);
    setupLabel(releaseLabel);

    // Setup filter sliders
    setupSlider(cutoffSlider, 100.0, 10000.0, 2000.0, 10.0);
    cutoffSlider.setSkewFactorFromMidPoint(1000.0);
    setupSlider(resonanceSlider, 0.0, 1.0, 0.3, 0.01);

    setupLabel(cutoffLabel);
    setupLabel(resonanceLabel);

    // Setup volume
    setupSlider(volumeSlider, 0.0, 1.0, 0.7, 0.01);
    setupLabel(volumeLabel);

    // Setup preset selector
    addAndMakeVisible(presetSelector);
    loadPresetList();
    presetSelector.onChange = [this]() {
        int idx = presetSelector.getSelectedItemIndex();
        if (idx >= 0 && idx < static_cast<int>(presets.size()))
        {
            synthEngine.loadPreset(presets[idx]);
            updateWaveformButtons();

            // Update UI controls to match preset
            attackSlider.setValue(presets[idx].attack, juce::dontSendNotification);
            decaySlider.setValue(presets[idx].decay, juce::dontSendNotification);
            sustainSlider.setValue(presets[idx].sustain, juce::dontSendNotification);
            releaseSlider.setValue(presets[idx].release, juce::dontSendNotification);
            cutoffSlider.setValue(presets[idx].filterCutoff, juce::dontSendNotification);
            resonanceSlider.setValue(presets[idx].filterResonance, juce::dontSendNotification);
            volumeSlider.setValue(presets[idx].volume, juce::dontSendNotification);
        }
    };

    // Setup back button
    addAndMakeVisible(backButton);
    backButton.onClick = [this]() {
        if (onBackPressed)
            onBackPressed();
    };

    // Setup title
    addAndMakeVisible(titleLabel);
    titleLabel.setFont(juce::Font(24.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::cyan);

    // Initial state
    updateWaveformButtons();

    // Start timer for UI updates
    startTimerHz(30);
}

HiddenSynth::~HiddenSynth()
{
    stopTimer();
}

//==============================================================================
void HiddenSynth::setupSlider(juce::Slider& slider, double min, double max,
                              double default_, double step)
{
    slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 15);
    slider.setRange(min, max, step);
    slider.setValue(default_);
    slider.addListener(this);
    addAndMakeVisible(slider);
}

void HiddenSynth::setupLabel(juce::Label& label)
{
    label.setFont(juce::Font(11.0f));
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(label);
}

void HiddenSynth::loadPresetList()
{
    presets = SynthEngine::getDefaultPresets();
    presetSelector.clear();

    for (size_t i = 0; i < presets.size(); ++i)
    {
        presetSelector.addItem(presets[i].name, static_cast<int>(i + 1));
    }

    presetSelector.setSelectedItemIndex(0);
}

void HiddenSynth::updateWaveformButtons()
{
    auto waveform = synthEngine.getWaveform();

    auto normalColour = juce::Colours::darkgrey;
    auto selectedColour = juce::Colours::cyan;

    sineButton.setColour(juce::TextButton::buttonColourId,
                         waveform == SynthEngine::Waveform::Sine ? selectedColour : normalColour);
    sawButton.setColour(juce::TextButton::buttonColourId,
                        waveform == SynthEngine::Waveform::Saw ? selectedColour : normalColour);
    squareButton.setColour(juce::TextButton::buttonColourId,
                           waveform == SynthEngine::Waveform::Square ? selectedColour : normalColour);
    triangleButton.setColour(juce::TextButton::buttonColourId,
                             waveform == SynthEngine::Waveform::Triangle ? selectedColour : normalColour);
}

//==============================================================================
void HiddenSynth::paint(juce::Graphics& g)
{
    // Dark background
    g.fillAll(juce::Colour(0xff1a1a2e));

    if (showingUnlockAnimation)
    {
        drawUnlockAnimation(g);
    }
    else
    {
        drawSynthUI(g);
    }
}

void HiddenSynth::drawUnlockAnimation(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Background gradient
    g.setGradientFill(juce::ColourGradient(
        juce::Colour(0xff0f0f1a), bounds.getCentre(),
        juce::Colour(0xff2a1a4a), bounds.getTopLeft(),
        true));
    g.fillAll();

    // Animated glow
    float glowRadius = bounds.getWidth() * 0.3f * unlockAnimationProgress;
    g.setColour(juce::Colours::cyan.withAlpha(0.3f * (1.0f - unlockAnimationProgress)));
    g.fillEllipse(bounds.getCentreX() - glowRadius,
                  bounds.getCentreY() - glowRadius,
                  glowRadius * 2, glowRadius * 2);

    // Sparkles
    juce::Random random(42);
    int numSparkles = static_cast<int>(20 * unlockAnimationProgress);
    for (int i = 0; i < numSparkles; ++i)
    {
        float angle = random.nextFloat() * juce::MathConstants<float>::twoPi;
        float distance = random.nextFloat() * bounds.getWidth() * 0.4f * unlockAnimationProgress;
        float x = bounds.getCentreX() + std::cos(angle) * distance;
        float y = bounds.getCentreY() + std::sin(angle) * distance;
        float size = random.nextFloat() * 4.0f + 2.0f;

        g.setColour(juce::Colours::cyan.withAlpha(random.nextFloat() * 0.8f));
        g.fillEllipse(x - size/2, y - size/2, size, size);
    }

    // Text
    g.setColour(juce::Colours::cyan);
    g.setFont(juce::Font(32.0f, juce::Font::bold));

    float textAlpha = unlockAnimationProgress < 0.5f
                      ? unlockAnimationProgress * 2.0f
                      : 1.0f;
    g.setColour(juce::Colours::cyan.withAlpha(textAlpha));
    g.drawText("SYNTH UNLOCKED!", bounds.reduced(20), juce::Justification::centred);

    // Emoji
    if (unlockAnimationProgress > 0.3f)
    {
        g.setFont(juce::Font(48.0f));
        g.drawText("🎹", bounds.withY(bounds.getY() + 60), juce::Justification::centred);
    }
}

void HiddenSynth::drawSynthUI(juce::Graphics& g)
{
    // Section backgrounds
    auto bounds = getLocalBounds();

    // Oscillator section
    g.setColour(juce::Colour(0xff252540));
    g.fillRoundedRectangle(10, 50, 200, 60, 5);
    g.setColour(juce::Colours::lightgrey);
    g.setFont(juce::Font(10.0f));
    g.drawText("OSCILLATOR", 10, 52, 200, 15, juce::Justification::centred);

    // ADSR section
    g.setColour(juce::Colour(0xff252540));
    g.fillRoundedRectangle(10, 120, 280, 110, 5);
    g.setColour(juce::Colours::lightgrey);
    g.drawText("ENVELOPE", 10, 122, 280, 15, juce::Justification::centred);

    // Filter section
    g.setColour(juce::Colour(0xff252540));
    g.fillRoundedRectangle(300, 120, 150, 110, 5);
    g.setColour(juce::Colours::lightgrey);
    g.drawText("FILTER", 300, 122, 150, 15, juce::Justification::centred);

    // Draw active voice indicator
    drawVoiceIndicator(g);

    // Keyboard
    drawKeyboard(g);
}

void HiddenSynth::drawVoiceIndicator(juce::Graphics& g)
{
    int activeVoices = synthEngine.getActiveVoiceCount();

    g.setColour(juce::Colours::lightgrey);
    g.setFont(juce::Font(10.0f));
    g.drawText("Voices: " + juce::String(activeVoices) + "/8",
               getWidth() - 80, 55, 70, 20, juce::Justification::right);

    // Voice indicator dots
    for (int i = 0; i < 8; ++i)
    {
        g.setColour(i < activeVoices ? juce::Colours::cyan : juce::Colours::darkgrey);
        g.fillEllipse(static_cast<float>(getWidth() - 80 + i * 9), 75.0f, 6.0f, 6.0f);
    }
}

void HiddenSynth::drawKeyboard(juce::Graphics& g)
{
    const int whiteKeyWidth = keyboardArea.getWidth() / 15; // 15 white keys in 2 octaves
    const int blackKeyWidth = whiteKeyWidth * 2 / 3;
    const int blackKeyHeight = keyboardArea.getHeight() * 2 / 3;

    int whiteKeyIndex = 0;

    // Draw white keys first
    for (int i = 0; i < numKeys; ++i)
    {
        if (!isBlackKey(i))
        {
            int x = keyboardArea.getX() + whiteKeyIndex * whiteKeyWidth;

            // Key background
            if (keyStates[i])
                g.setColour(juce::Colours::cyan.darker(0.3f));
            else
                g.setColour(juce::Colours::white);

            g.fillRect(x, keyboardArea.getY(), whiteKeyWidth - 1, keyboardArea.getHeight());

            // Key border
            g.setColour(juce::Colours::darkgrey);
            g.drawRect(x, keyboardArea.getY(), whiteKeyWidth - 1, keyboardArea.getHeight());

            whiteKeyIndex++;
        }
    }

    // Draw black keys on top
    whiteKeyIndex = 0;
    for (int i = 0; i < numKeys; ++i)
    {
        if (isBlackKey(i))
        {
            // Black key is positioned between previous white key and next
            int x = keyboardArea.getX() + whiteKeyIndex * whiteKeyWidth - blackKeyWidth / 2;

            if (keyStates[i])
                g.setColour(juce::Colours::cyan.darker(0.5f));
            else
                g.setColour(juce::Colour(0xff1a1a1a));

            g.fillRect(x, keyboardArea.getY(), blackKeyWidth, blackKeyHeight);

            g.setColour(juce::Colours::black);
            g.drawRect(x, keyboardArea.getY(), blackKeyWidth, blackKeyHeight);
        }
        else
        {
            whiteKeyIndex++;
        }
    }
}

int HiddenSynth::getKeyAtPosition(juce::Point<int> pos)
{
    if (!keyboardArea.contains(pos))
        return -1;

    const int whiteKeyWidth = keyboardArea.getWidth() / 15;
    const int blackKeyWidth = whiteKeyWidth * 2 / 3;
    const int blackKeyHeight = keyboardArea.getHeight() * 2 / 3;

    int relX = pos.x - keyboardArea.getX();
    int relY = pos.y - keyboardArea.getY();

    // Check black keys first (they're on top)
    if (relY < blackKeyHeight)
    {
        int whiteKeyIndex = 0;
        for (int i = 0; i < numKeys; ++i)
        {
            if (isBlackKey(i))
            {
                int blackKeyX = whiteKeyIndex * whiteKeyWidth - blackKeyWidth / 2;
                if (relX >= blackKeyX && relX < blackKeyX + blackKeyWidth)
                    return i;
            }
            else
            {
                whiteKeyIndex++;
            }
        }
    }

    // Check white keys
    int whiteKeyIndex = relX / whiteKeyWidth;
    int currentWhite = 0;
    for (int i = 0; i < numKeys; ++i)
    {
        if (!isBlackKey(i))
        {
            if (currentWhite == whiteKeyIndex)
                return i;
            currentWhite++;
        }
    }

    return -1;
}

bool HiddenSynth::isBlackKey(int keyIndex)
{
    // Pattern: W B W B W W B W B W B W (C C# D D# E F F# G G# A A# B)
    int noteInOctave = keyIndex % 12;
    return noteInOctave == 1 || noteInOctave == 3 ||
           noteInOctave == 6 || noteInOctave == 8 || noteInOctave == 10;
}

//==============================================================================
void HiddenSynth::resized()
{
    auto bounds = getLocalBounds();

    // Header area
    backButton.setBounds(10, 10, 80, 30);
    titleLabel.setBounds(bounds.getWidth() / 2 - 100, 10, 200, 30);
    presetSelector.setBounds(bounds.getWidth() - 160, 10, 150, 30);

    // Waveform buttons
    int btnWidth = 45;
    int btnY = 70;
    sineButton.setBounds(20, btnY, btnWidth, 25);
    sawButton.setBounds(70, btnY, btnWidth, 25);
    squareButton.setBounds(120, btnY, btnWidth, 25);
    triangleButton.setBounds(170, btnY, btnWidth, 25);

    // ADSR knobs
    int knobSize = 60;
    int knobY = 145;
    attackSlider.setBounds(20, knobY, knobSize, knobSize + 20);
    decaySlider.setBounds(85, knobY, knobSize, knobSize + 20);
    sustainSlider.setBounds(150, knobY, knobSize, knobSize + 20);
    releaseSlider.setBounds(215, knobY, knobSize, knobSize + 20);

    attackLabel.setBounds(20, knobY + knobSize + 15, knobSize, 15);
    decayLabel.setBounds(85, knobY + knobSize + 15, knobSize, 15);
    sustainLabel.setBounds(150, knobY + knobSize + 15, knobSize, 15);
    releaseLabel.setBounds(215, knobY + knobSize + 15, knobSize, 15);

    // Filter knobs
    cutoffSlider.setBounds(310, knobY, knobSize, knobSize + 20);
    resonanceSlider.setBounds(375, knobY, knobSize, knobSize + 20);

    cutoffLabel.setBounds(310, knobY + knobSize + 15, knobSize, 15);
    resonanceLabel.setBounds(375, knobY + knobSize + 15, knobSize, 15);

    // Volume knob
    volumeSlider.setBounds(bounds.getWidth() - 80, knobY, knobSize, knobSize + 20);
    volumeLabel.setBounds(bounds.getWidth() - 80, knobY + knobSize + 15, knobSize, 15);

    // Keyboard area
    keyboardArea = juce::Rectangle<int>(10, 250, bounds.getWidth() - 20, 80);
}

//==============================================================================
void HiddenSynth::timerCallback()
{
    if (showingUnlockAnimation)
    {
        unlockAnimationProgress += 1.0f / (30.0f * unlockAnimationDuration);
        if (unlockAnimationProgress >= 1.0f)
        {
            showingUnlockAnimation = false;
            unlockAnimationProgress = 0.0f;
        }
        repaint();
    }
    else
    {
        // Just repaint voice indicator
        repaint(getWidth() - 90, 50, 90, 40);
    }
}

void HiddenSynth::sliderValueChanged(juce::Slider* slider)
{
    if (slider == &attackSlider || slider == &decaySlider ||
        slider == &sustainSlider || slider == &releaseSlider)
    {
        synthEngine.setADSR(
            static_cast<float>(attackSlider.getValue()),
            static_cast<float>(decaySlider.getValue()),
            static_cast<float>(sustainSlider.getValue()),
            static_cast<float>(releaseSlider.getValue())
        );
    }
    else if (slider == &cutoffSlider)
    {
        synthEngine.setFilterCutoff(static_cast<float>(cutoffSlider.getValue()));
    }
    else if (slider == &resonanceSlider)
    {
        synthEngine.setFilterResonance(static_cast<float>(resonanceSlider.getValue()));
    }
    else if (slider == &volumeSlider)
    {
        synthEngine.setVolume(static_cast<float>(volumeSlider.getValue()));
    }
}

//==============================================================================
void HiddenSynth::playUnlockAnimation()
{
    showingUnlockAnimation = true;
    unlockAnimationProgress = 0.0f;
    repaint();
}

//==============================================================================
void HiddenSynth::mouseDown(const juce::MouseEvent& event)
{
    int key = getKeyAtPosition(event.getPosition());
    if (key >= 0)
    {
        keyStates[key] = true;
        lastKeyPressed = key;
        synthEngine.noteOn(startNote + key, 100);
        repaint(keyboardArea);
    }
}

void HiddenSynth::mouseUp(const juce::MouseEvent& event)
{
    if (lastKeyPressed >= 0)
    {
        keyStates[lastKeyPressed] = false;
        synthEngine.noteOff(startNote + lastKeyPressed);
        lastKeyPressed = -1;
        repaint(keyboardArea);
    }
}

void HiddenSynth::mouseDrag(const juce::MouseEvent& event)
{
    int key = getKeyAtPosition(event.getPosition());

    if (key != lastKeyPressed)
    {
        // Release old key
        if (lastKeyPressed >= 0)
        {
            keyStates[lastKeyPressed] = false;
            synthEngine.noteOff(startNote + lastKeyPressed);
        }

        // Press new key
        if (key >= 0)
        {
            keyStates[key] = true;
            synthEngine.noteOn(startNote + key, 100);
        }

        lastKeyPressed = key;
        repaint(keyboardArea);
    }
}
