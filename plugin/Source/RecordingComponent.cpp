#include "RecordingComponent.h"
#include "PluginProcessor.h"

//==============================================================================
RecordingComponent::RecordingComponent(SidechainAudioProcessor& processor)
    : audioProcessor(processor)
{
    // Start timer for UI updates (~30fps)
    startTimerHz(30);
}

RecordingComponent::~RecordingComponent()
{
    stopTimer();
}

//==============================================================================
void RecordingComponent::timerCallback()
{
    // Update animation frame
    animationFrame++;

    // Pulsing recording dot
    if (currentState == State::Recording)
    {
        recordingDotOpacity = 0.5f + 0.5f * std::sin(animationFrame * 0.15f);
    }

    // Check if recording stopped externally (e.g., max length reached)
    if (currentState == State::Recording && !audioProcessor.isRecording())
    {
        // Recording stopped (likely hit max length)
        stopRecording();
    }

    // Repaint for smooth animations
    if (currentState == State::Recording)
    {
        repaint();
    }
}

//==============================================================================
void RecordingComponent::paint(juce::Graphics& g)
{
    // Dark background
    g.fillAll(juce::Colour::fromRGB(28, 28, 32));

    switch (currentState)
    {
        case State::Idle:
            drawIdleState(g);
            break;
        case State::Recording:
            drawRecordingState(g);
            break;
        case State::Preview:
            drawPreviewState(g);
            break;
    }
}

void RecordingComponent::resized()
{
    auto bounds = getLocalBounds().reduced(20);

    // Calculate areas based on component size
    int topSectionHeight = 80;
    int meterHeight = 60;
    int progressHeight = 20;
    int waveformHeight = 100;
    int buttonHeight = 44;

    // Top section: Record button + time
    auto topSection = bounds.removeFromTop(topSectionHeight);
    recordButtonArea = topSection.removeFromLeft(topSectionHeight).reduced(10);
    timeDisplayArea = topSection.reduced(10);

    bounds.removeFromTop(10); // Spacing

    // Level meters
    levelMeterArea = bounds.removeFromTop(meterHeight);

    bounds.removeFromTop(10); // Spacing

    // Progress bar
    progressBarArea = bounds.removeFromTop(progressHeight);

    bounds.removeFromTop(10); // Spacing

    // Waveform area (takes remaining space minus buttons)
    int remainingHeight = bounds.getHeight() - buttonHeight - 20;
    if (remainingHeight > 0)
    {
        waveformArea = bounds.removeFromTop(juce::jmin(waveformHeight, remainingHeight));
    }

    bounds.removeFromTop(10); // Spacing

    // Action buttons at bottom
    actionButtonsArea = bounds.removeFromTop(buttonHeight);
}

void RecordingComponent::mouseUp(const juce::MouseEvent& event)
{
    auto pos = event.getPosition();

    switch (currentState)
    {
        case State::Idle:
            if (recordButtonArea.contains(pos))
            {
                startRecording();
            }
            break;

        case State::Recording:
            if (recordButtonArea.contains(pos))
            {
                stopRecording();
            }
            break;

        case State::Preview:
        {
            // Check action buttons
            int buttonWidth = actionButtonsArea.getWidth() / 2 - 10;
            auto discardButton = actionButtonsArea.withWidth(buttonWidth);
            auto uploadButton = actionButtonsArea.withX(actionButtonsArea.getRight() - buttonWidth).withWidth(buttonWidth);

            if (discardButton.contains(pos))
            {
                discardRecording();
            }
            else if (uploadButton.contains(pos))
            {
                confirmRecording();
            }
            break;
        }
    }
}

//==============================================================================
void RecordingComponent::drawIdleState(juce::Graphics& g)
{
    // Draw record button (red circle)
    drawRecordButton(g);

    // Instructions text
    g.setColour(juce::Colours::lightgrey);
    g.setFont(16.0f);
    g.drawText("Press to record audio from your DAW",
               timeDisplayArea, juce::Justification::centredLeft);

    // Show max recording time
    g.setFont(12.0f);
    g.setColour(juce::Colours::grey);
    g.drawText("Maximum recording length: 60 seconds",
               progressBarArea, juce::Justification::centred);
}

void RecordingComponent::drawRecordingState(juce::Graphics& g)
{
    // Draw pulsing record button
    drawRecordButton(g);

    // Draw time display
    drawTimeDisplay(g);

    // Draw level meters
    drawLevelMeters(g);

    // Draw progress bar
    drawProgressBar(g);
}

void RecordingComponent::drawPreviewState(juce::Graphics& g)
{
    // Draw smaller record button (to re-record)
    g.setColour(juce::Colour::fromRGB(60, 60, 64));
    g.fillEllipse(recordButtonArea.toFloat());

    g.setColour(juce::Colour::fromRGB(255, 82, 82));
    g.fillEllipse(recordButtonArea.reduced(10).toFloat());

    // Show recording duration
    double duration = static_cast<double>(recordedAudio.getNumSamples()) / recordedSampleRate;
    g.setColour(juce::Colours::white);
    g.setFont(20.0f);
    g.drawText("Recorded: " + formatTime(duration),
               timeDisplayArea, juce::Justification::centredLeft);

    // Draw waveform preview
    drawWaveformPreview(g);

    // Draw action buttons
    drawActionButtons(g);
}

//==============================================================================
void RecordingComponent::drawRecordButton(juce::Graphics& g)
{
    bool isRecording = (currentState == State::Recording);

    // Outer ring (darker background)
    g.setColour(juce::Colour::fromRGB(60, 60, 64));
    g.fillEllipse(recordButtonArea.toFloat());

    // Inner recording indicator
    if (isRecording)
    {
        // Pulsing red with square shape (stop indicator)
        g.setColour(juce::Colour::fromRGB(255, 82, 82).withAlpha(recordingDotOpacity));
        auto innerRect = recordButtonArea.reduced(recordButtonArea.getWidth() / 4);
        g.fillRoundedRectangle(innerRect.toFloat(), 4.0f);
    }
    else
    {
        // Red circle (record indicator)
        g.setColour(juce::Colour::fromRGB(255, 82, 82));
        g.fillEllipse(recordButtonArea.reduced(10).toFloat());
    }
}

void RecordingComponent::drawTimeDisplay(juce::Graphics& g)
{
    double seconds = audioProcessor.getRecordingLengthSeconds();

    // Large time display
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(32.0f, juce::Font::bold));
    g.drawText(formatTime(seconds), timeDisplayArea.removeFromTop(40),
               juce::Justification::centredLeft);

    // Recording indicator text
    g.setColour(juce::Colour::fromRGB(255, 82, 82).withAlpha(recordingDotOpacity));
    g.setFont(14.0f);
    g.drawText("RECORDING", timeDisplayArea, juce::Justification::centredLeft);
}

void RecordingComponent::drawLevelMeters(juce::Graphics& g)
{
    // Get levels from processor
    float peakL = audioProcessor.getPeakLevel(0);
    float peakR = audioProcessor.getPeakLevel(1);
    float rmsL = audioProcessor.getRMSLevel(0);
    float rmsR = audioProcessor.getRMSLevel(1);

    // Draw background
    g.setColour(juce::Colour::fromRGB(40, 40, 44));
    g.fillRoundedRectangle(levelMeterArea.toFloat(), 4.0f);

    auto innerArea = levelMeterArea.reduced(8);
    int meterHeight = (innerArea.getHeight() - 4) / 2;

    // Left channel
    auto leftMeter = innerArea.removeFromTop(meterHeight);
    drawSingleMeter(g, leftMeter, peakL, rmsL, "L");

    innerArea.removeFromTop(4); // Spacing

    // Right channel
    auto rightMeter = innerArea.removeFromTop(meterHeight);
    drawSingleMeter(g, rightMeter, peakR, rmsR, "R");
}

void RecordingComponent::drawSingleMeter(juce::Graphics& g, juce::Rectangle<int> bounds,
                                         float peak, float rms, const juce::String& label)
{
    // Label
    g.setColour(juce::Colours::grey);
    g.setFont(12.0f);
    auto labelArea = bounds.removeFromLeft(20);
    g.drawText(label, labelArea, juce::Justification::centred);

    // Meter background
    g.setColour(juce::Colour::fromRGB(30, 30, 34));
    g.fillRoundedRectangle(bounds.toFloat(), 2.0f);

    // RMS level (darker green)
    float rmsWidth = bounds.getWidth() * juce::jlimit(0.0f, 1.0f, rms);
    if (rmsWidth > 0)
    {
        g.setColour(juce::Colour::fromRGB(0, 150, 100));
        g.fillRoundedRectangle(bounds.withWidth(static_cast<int>(rmsWidth)).toFloat(), 2.0f);
    }

    // Peak level (bright green, yellow, red gradient)
    float peakWidth = bounds.getWidth() * juce::jlimit(0.0f, 1.0f, peak);
    if (peakWidth > 0)
    {
        juce::Colour peakColor;
        if (peak < 0.7f)
            peakColor = juce::Colour::fromRGB(0, 212, 100);
        else if (peak < 0.9f)
            peakColor = juce::Colour::fromRGB(255, 200, 0);
        else
            peakColor = juce::Colour::fromRGB(255, 82, 82);

        g.setColour(peakColor);
        auto peakBar = bounds.withWidth(static_cast<int>(peakWidth));
        peakBar = peakBar.withHeight(bounds.getHeight() / 2).withY(bounds.getY() + bounds.getHeight() / 4);
        g.fillRoundedRectangle(peakBar.toFloat(), 1.0f);
    }
}

void RecordingComponent::drawProgressBar(juce::Graphics& g)
{
    float progress = audioProcessor.getRecordingProgress();
    double maxSeconds = audioProcessor.getMaxRecordingLengthSeconds();

    // Background
    g.setColour(juce::Colour::fromRGB(40, 40, 44));
    g.fillRoundedRectangle(progressBarArea.toFloat(), 4.0f);

    // Progress fill
    int fillWidth = static_cast<int>(progressBarArea.getWidth() * progress);
    if (fillWidth > 0)
    {
        auto fillRect = progressBarArea.withWidth(fillWidth);

        // Gradient from green to yellow to red
        juce::Colour fillColor;
        if (progress < 0.7f)
            fillColor = juce::Colour::fromRGB(0, 212, 100);
        else if (progress < 0.9f)
            fillColor = juce::Colour::fromRGB(255, 200, 0);
        else
            fillColor = juce::Colour::fromRGB(255, 82, 82);

        g.setColour(fillColor);
        g.fillRoundedRectangle(fillRect.toFloat(), 4.0f);
    }

    // Time labels
    g.setColour(juce::Colours::grey);
    g.setFont(10.0f);
    g.drawText("0:00", progressBarArea.withY(progressBarArea.getBottom() + 2),
               juce::Justification::left);
    g.drawText(formatTime(maxSeconds), progressBarArea.withY(progressBarArea.getBottom() + 2),
               juce::Justification::right);
}

void RecordingComponent::drawWaveformPreview(juce::Graphics& g)
{
    if (recordedAudio.getNumSamples() == 0)
        return;

    // Background
    g.setColour(juce::Colour::fromRGB(40, 40, 44));
    g.fillRoundedRectangle(waveformArea.toFloat(), 4.0f);

    // Generate and draw waveform path
    auto path = generateWaveformPath(recordedAudio, waveformArea.reduced(4));

    g.setColour(juce::Colour::fromRGB(0, 212, 255)); // Sidechain blue
    g.strokePath(path, juce::PathStrokeType(1.5f));
}

void RecordingComponent::drawActionButtons(juce::Graphics& g)
{
    int buttonWidth = actionButtonsArea.getWidth() / 2 - 10;

    // Discard button (left)
    auto discardButton = actionButtonsArea.withWidth(buttonWidth);
    g.setColour(juce::Colour::fromRGB(108, 117, 125));
    g.fillRoundedRectangle(discardButton.toFloat(), 8.0f);
    g.setColour(juce::Colours::white);
    g.setFont(16.0f);
    g.drawText("Discard", discardButton, juce::Justification::centred);

    // Upload button (right)
    auto uploadButton = actionButtonsArea.withX(actionButtonsArea.getRight() - buttonWidth).withWidth(buttonWidth);
    g.setColour(juce::Colour::fromRGB(0, 212, 255)); // Sidechain blue
    g.fillRoundedRectangle(uploadButton.toFloat(), 8.0f);
    g.setColour(juce::Colours::white);
    g.drawText("Share Loop", uploadButton, juce::Justification::centred);
}

//==============================================================================
juce::String RecordingComponent::formatTime(double seconds)
{
    int mins = static_cast<int>(seconds) / 60;
    int secs = static_cast<int>(seconds) % 60;
    return juce::String::formatted("%d:%02d", mins, secs);
}

juce::Path RecordingComponent::generateWaveformPath(const juce::AudioBuffer<float>& buffer,
                                                    juce::Rectangle<int> bounds)
{
    juce::Path path;

    if (buffer.getNumSamples() == 0)
        return path;

    int numSamples = buffer.getNumSamples();
    int width = bounds.getWidth();
    float height = bounds.getHeight();
    float centerY = bounds.getCentreY();

    // Samples per pixel
    int samplesPerPixel = juce::jmax(1, numSamples / width);

    path.startNewSubPath(bounds.getX(), centerY);

    for (int x = 0; x < width; ++x)
    {
        int startSample = x * numSamples / width;
        int endSample = juce::jmin((x + 1) * numSamples / width, numSamples);

        // Find peak in this range
        float peak = 0.0f;
        for (int i = startSample; i < endSample; ++i)
        {
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                peak = juce::jmax(peak, std::abs(buffer.getSample(ch, i)));
            }
        }

        float y = centerY - (peak * height * 0.45f);
        path.lineTo(bounds.getX() + x, y);
    }

    return path;
}

//==============================================================================
void RecordingComponent::startRecording()
{
    audioProcessor.startRecording();
    currentState = State::Recording;
    animationFrame = 0;
    repaint();

    DBG("RecordingComponent: Started recording");
}

void RecordingComponent::stopRecording()
{
    audioProcessor.stopRecording();
    recordedAudio = audioProcessor.getRecordedAudio();
    recordedSampleRate = audioProcessor.getCurrentSampleRate();

    if (recordedAudio.getNumSamples() > 0)
    {
        currentState = State::Preview;
        DBG("RecordingComponent: Stopped recording, " +
            juce::String(recordedAudio.getNumSamples()) + " samples captured");
    }
    else
    {
        currentState = State::Idle;
        DBG("RecordingComponent: Recording stopped but no audio captured");
    }

    repaint();
}

void RecordingComponent::discardRecording()
{
    recordedAudio.setSize(0, 0);
    currentState = State::Idle;

    if (onRecordingDiscarded)
        onRecordingDiscarded();

    repaint();

    DBG("RecordingComponent: Recording discarded");
}

void RecordingComponent::confirmRecording()
{
    if (onRecordingComplete && recordedAudio.getNumSamples() > 0)
    {
        onRecordingComplete(recordedAudio);
    }

    // Reset state after sharing
    recordedAudio.setSize(0, 0);
    currentState = State::Idle;
    repaint();

    DBG("RecordingComponent: Recording confirmed for upload");
}
