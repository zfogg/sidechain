#include "StoryRecording.h"
#include "../../PluginProcessor.h"
#include "../../util/Log.h"
#include "../../util/UIHelpers.h"

// Colors for Stories UI (matching app theme)
namespace StoryColors
{
    const juce::Colour background(0xff1a1a2e);
    const juce::Colour surface(0xff25253a);
    const juce::Colour recordRed(0xffe53935);
    const juce::Colour recordRedDark(0xffb71c1c);
    const juce::Colour midiBlue(0xff00bcd4);
    const juce::Colour midiActive(0xff00e5ff);
    const juce::Colour textPrimary(0xffffffff);
    const juce::Colour textSecondary(0xffb0b0b0);
    const juce::Colour waveformColor(0xff7c4dff);
    const juce::Colour progressBg(0xff2d2d44);
    const juce::Colour progressFg(0xff7c4dff);
    const juce::Colour buttonGreen(0xff4caf50);
    const juce::Colour buttonGray(0xff616161);
}

//==============================================================================
StoryRecording::StoryRecording(SidechainAudioProcessor& processor)
    : audioProcessor(processor)
{
    // Prepare MIDI capture
    midiCapture.prepare(44100.0, 512);

    // Create piano roll for preview
    pianoRollPreview = std::make_unique<PianoRollComponent>();
    addChildComponent(pianoRollPreview.get());

    // Create buffer audio player for preview playback
    bufferAudioPlayer = std::make_unique<BufferAudioPlayer>();

    // Set up buffer audio player callbacks
    bufferAudioPlayer->onPlaybackStarted = [this]() {
        isPreviewPlaying = true;
        repaint();
    };
    bufferAudioPlayer->onPlaybackPaused = [this]() {
        isPreviewPlaying = false;
        repaint();
    };
    bufferAudioPlayer->onPlaybackStopped = [this]() {
        isPreviewPlaying = false;
        previewPlaybackPosition = 0.0;
        repaint();
    };
    bufferAudioPlayer->onProgressUpdate = [this](double progress) {
        previewPlaybackPosition = progress * bufferAudioPlayer->getDurationSeconds();
        repaint();
    };

    // Start timer for UI updates (30 fps)
    startTimerHz(30);

    Log::info("StoryRecording created");
}

StoryRecording::~StoryRecording()
{
    stopTimer();

    // Unregister buffer audio player from processor
    if (bufferAudioPlayer)
    {
        bufferAudioPlayer->stop();
        audioProcessor.setBufferAudioPlayer(nullptr);
    }

    Log::info("StoryRecording destroyed");
}

//==============================================================================
void StoryRecording::paint(juce::Graphics& g)
{
    // Background
    g.fillAll(StoryColors::background);

    // Draw based on state
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

void StoryRecording::resized()
{
    auto bounds = getLocalBounds();

    // Header (back button, title)
    headerArea = bounds.removeFromTop(60);

    // Main content area
    auto contentArea = bounds.reduced(20);

    if (currentState == State::Preview)
    {
        // Preview layout: waveform at top, piano roll in middle, playback controls, metadata, action buttons at bottom
        waveformArea = contentArea.removeFromTop(100);
        contentArea.removeFromTop(10);
        pianoRollArea = contentArea.removeFromTop(180);
        contentArea.removeFromTop(10);
        playbackControlsArea = contentArea.removeFromTop(50);
        contentArea.removeFromTop(10);
        metadataArea = contentArea.removeFromTop(80);
        contentArea.removeFromTop(10);
        actionButtonsArea = contentArea.removeFromTop(50);

        // Position piano roll component
        if (pianoRollPreview)
            pianoRollPreview->setBounds(pianoRollArea);
    }
    else
    {
        // Recording layout: countdown ring + record button centered
        int ringSize = juce::jmin(contentArea.getWidth(), contentArea.getHeight() - 150);
        ringSize = juce::jmin(ringSize, 250);

        int verticalOffset = (contentArea.getHeight() - ringSize - 100) / 3;
        contentArea.removeFromTop(verticalOffset);

        countdownArea = contentArea.removeFromTop(ringSize).withSizeKeepingCentre(ringSize, ringSize);

        // Record button inside countdown ring
        recordButtonArea = countdownArea.reduced(30);

        contentArea.removeFromTop(20);

        // Time display below ring
        timeDisplayArea = contentArea.removeFromTop(40);

        // MIDI indicator
        midiIndicatorArea = contentArea.removeFromTop(30);

        // Waveform preview (during recording)
        contentArea.removeFromTop(20);
        waveformArea = contentArea.removeFromTop(80);
    }

    // Cancel button area at bottom
    cancelButtonArea = getLocalBounds().removeFromBottom(60).reduced(20, 10);
}

void StoryRecording::mouseUp(const juce::MouseEvent& event)
{
    auto pos = event.getPosition();

    if (currentState == State::Preview)
    {
        // Check playback controls
        if (playbackControlsArea.contains(pos))
        {
            togglePreviewPlayback();
            return;
        }

        // Check action buttons
        int buttonWidth = actionButtonsArea.getWidth() / 2 - 10;
        auto discardBounds = actionButtonsArea.withWidth(buttonWidth);
        auto postBounds = actionButtonsArea.withX(actionButtonsArea.getRight() - buttonWidth).withWidth(buttonWidth);

        if (discardBounds.contains(pos))
        {
            discardRecording();
            return;
        }

        if (postBounds.contains(pos))
        {
            confirmRecording();
            return;
        }
    }
    else if (currentState == State::Idle)
    {
        // Start recording on record button click
        if (recordButtonArea.contains(pos))
        {
            startRecording();
        }
        else if (cancelButtonArea.contains(pos))
        {
            if (onCancel)
                onCancel();
        }
    }
    else if (currentState == State::Recording)
    {
        // Stop recording on record button click (if allowed)
        if (recordButtonArea.contains(pos) && canStopRecording())
        {
            stopRecording();
        }
        else if (cancelButtonArea.contains(pos))
        {
            discardRecording();
        }
    }
}


//==============================================================================
void StoryRecording::timerCallback()
{
    // Note: Animation class is timer-based and auto-updates internally,
    // no need to call update() - just query getProgress() when drawing

    if (currentState == State::Recording)
    {
        // Update recording duration
        currentRecordingDuration = audioProcessor.getRecordingLengthSeconds();

        // Check for auto-stop at max duration
        if (currentRecordingDuration >= MAX_DURATION_SECONDS)
        {
            Log::info("StoryRecording: Auto-stopping at max duration");
            stopRecording();
            return;
        }

        // Check for MIDI activity
        // In a real implementation, we'd check midiCapture.getEventCount() or similar
        // For now, we simulate MIDI activity detection
        hasMIDIActivity = midiCapture.isCapturing() && midiCapture.getTotalTime() > 0;

        repaint();
    }
}

//==============================================================================
void StoryRecording::drawIdleState(juce::Graphics& g)
{
    drawHeader(g);
    drawCountdownRing(g);
    drawRecordButton(g);
    drawTimeDisplay(g);
    drawMIDIIndicator(g);

    // Cancel button
    g.setColour(StoryColors::textSecondary);
    g.setFont(14.0f);
    g.drawText("Cancel", cancelButtonArea, juce::Justification::centred);
}

void StoryRecording::drawRecordingState(juce::Graphics& g)
{
    drawHeader(g);
    drawCountdownRing(g);
    drawRecordButton(g);
    drawTimeDisplay(g);
    drawMIDIIndicator(g);

    // Waveform during recording
    if (!waveformArea.isEmpty())
    {
        drawWaveformPreview(g);
    }

    // Stop button label (shows when can stop)
    if (canStopRecording())
    {
        g.setColour(StoryColors::textSecondary);
        g.setFont(12.0f);
        g.drawText("Tap to stop", cancelButtonArea.withY(cancelButtonArea.getY() - 30).withHeight(20),
                   juce::Justification::centred);
    }
    else
    {
        g.setColour(StoryColors::textSecondary.withAlpha(0.5f));
        g.setFont(12.0f);
        g.drawText("Min " + juce::String(static_cast<int>(MIN_DURATION_SECONDS)) + "s required",
                   cancelButtonArea.withY(cancelButtonArea.getY() - 30).withHeight(20),
                   juce::Justification::centred);
    }

    // Cancel button
    g.setColour(StoryColors::recordRed);
    g.setFont(14.0f);
    g.drawText("Cancel Recording", cancelButtonArea, juce::Justification::centred);
}

void StoryRecording::drawPreviewState(juce::Graphics& g)
{
    drawHeader(g);

    // Waveform preview
    drawWaveformPreview(g);

    // Duration display
    g.setColour(StoryColors::textPrimary);
    g.setFont(16.0f);
    g.drawText(formatTime(currentRecordingDuration), timeDisplayArea, juce::Justification::centred);

    // Playback controls
    drawPlaybackControls(g);

    // Metadata input
    drawMetadataInput(g);

    // Action buttons
    drawActionButtons(g);
}

//==============================================================================
void StoryRecording::drawHeader(juce::Graphics& g)
{
    // Title
    g.setColour(StoryColors::textPrimary);
    g.setFont(juce::Font(18.0f, juce::Font::bold));

    juce::String title;
    switch (currentState)
    {
        case State::Idle:
            title = "Create Story";
            break;
        case State::Recording:
            title = "Recording...";
            break;
        case State::Preview:
            title = "Preview Story";
            break;
    }

    g.drawText(title, headerArea.reduced(20, 0), juce::Justification::centred);

    // Subtitle
    if (currentState == State::Idle)
    {
        g.setColour(StoryColors::textSecondary);
        g.setFont(12.0f);
        g.drawText("5-60 seconds • Expires in 24 hours",
                   headerArea.withY(headerArea.getY() + 30).withHeight(20).reduced(20, 0),
                   juce::Justification::centred);
    }
}

void StoryRecording::drawRecordButton(juce::Graphics& g)
{
    auto center = recordButtonArea.getCentre().toFloat();
    float radius = juce::jmin(recordButtonArea.getWidth(), recordButtonArea.getHeight()) / 2.0f - 5.0f;

    if (currentState == State::Recording)
    {
        // Pulsing stop square
        float pulseAmount = recordingDotAnimation.getProgress() * 0.1f;
        float adjustedRadius = radius * (0.9f + pulseAmount);

        g.setColour(StoryColors::recordRed);
        float squareSize = adjustedRadius * 0.6f;
        g.fillRoundedRectangle(center.x - squareSize, center.y - squareSize,
                               squareSize * 2, squareSize * 2, 8.0f);

        // Outer ring
        g.setColour(StoryColors::recordRed.withAlpha(0.3f));
        g.drawEllipse(center.x - adjustedRadius, center.y - adjustedRadius,
                      adjustedRadius * 2, adjustedRadius * 2, 3.0f);
    }
    else
    {
        // Record circle
        g.setColour(StoryColors::recordRed);
        g.fillEllipse(center.x - radius, center.y - radius, radius * 2, radius * 2);

        // Inner circle (slightly darker)
        float innerRadius = radius * 0.85f;
        g.setColour(StoryColors::recordRedDark);
        g.fillEllipse(center.x - innerRadius, center.y - innerRadius,
                      innerRadius * 2, innerRadius * 2);

        // White center
        float centerRadius = radius * 0.3f;
        g.setColour(StoryColors::textPrimary);
        g.fillEllipse(center.x - centerRadius, center.y - centerRadius,
                      centerRadius * 2, centerRadius * 2);
    }
}

void StoryRecording::drawTimeDisplay(juce::Graphics& g)
{
    g.setColour(StoryColors::textPrimary);
    g.setFont(juce::Font(32.0f, juce::Font::bold));

    juce::String timeStr;
    if (currentState == State::Recording)
    {
        timeStr = formatTime(currentRecordingDuration);
    }
    else
    {
        timeStr = formatTime(0);
    }

    g.drawText(timeStr, timeDisplayArea, juce::Justification::centred);

    // Max duration indicator
    g.setColour(StoryColors::textSecondary);
    g.setFont(12.0f);
    g.drawText("/ " + formatTime(MAX_DURATION_SECONDS),
               timeDisplayArea.withY(timeDisplayArea.getBottom()).withHeight(20),
               juce::Justification::centred);
}

void StoryRecording::drawCountdownRing(juce::Graphics& g)
{
    auto center = countdownArea.getCentre().toFloat();
    float radius = juce::jmin(countdownArea.getWidth(), countdownArea.getHeight()) / 2.0f - 5.0f;

    // Background ring
    g.setColour(StoryColors::progressBg);
    g.drawEllipse(center.x - radius, center.y - radius, radius * 2, radius * 2, 6.0f);

    // Progress ring (when recording)
    if (currentState == State::Recording)
    {
        float progress = static_cast<float>(currentRecordingDuration / MAX_DURATION_SECONDS);
        progress = juce::jlimit(0.0f, 1.0f, progress);

        // Draw arc
        juce::Path arc;
        float startAngle = -juce::MathConstants<float>::halfPi; // Start from top
        float endAngle = startAngle + (progress * juce::MathConstants<float>::twoPi);

        arc.addArc(center.x - radius, center.y - radius, radius * 2, radius * 2,
                   startAngle, endAngle, true);

        g.setColour(StoryColors::progressFg);
        g.strokePath(arc, juce::PathStrokeType(6.0f, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));
    }
}

void StoryRecording::drawMIDIIndicator(juce::Graphics& g)
{
    auto bounds = midiIndicatorArea.reduced(20, 0);

    // MIDI icon
    g.setColour(hasMIDIActivity ? StoryColors::midiActive : StoryColors::midiBlue.withAlpha(0.5f));
    g.setFont(14.0f);

    juce::String midiText = hasMIDIActivity ? "MIDI: Active" : "MIDI: Waiting...";
    g.drawText(midiText, bounds, juce::Justification::centred);

    // Activity indicator dot
    if (hasMIDIActivity)
    {
        float dotSize = 8.0f;
        float pulseAmount = midiActivityAnimation.getProgress();
        float adjustedSize = dotSize * (1.0f + pulseAmount * 0.3f);

        g.setColour(StoryColors::midiActive.withAlpha(1.0f - pulseAmount * 0.5f));
        g.fillEllipse(bounds.getX() - 20, bounds.getCentreY() - adjustedSize / 2,
                      adjustedSize, adjustedSize);
    }
}

void StoryRecording::drawWaveformPreview(juce::Graphics& g)
{
    if (waveformArea.isEmpty())
        return;

    // Background
    g.setColour(StoryColors::surface);
    g.fillRoundedRectangle(waveformArea.toFloat(), 8.0f);

    if (currentState == State::Recording)
    {
        // Draw live waveform from processor's current buffer
        // For now, draw a placeholder animated waveform
        g.setColour(StoryColors::waveformColor);

        juce::Path wavePath;
        float centerY = waveformArea.getCentreY();
        float amplitude = waveformArea.getHeight() * 0.4f;

        wavePath.startNewSubPath(waveformArea.getX(), centerY);

        for (int x = waveformArea.getX(); x < waveformArea.getRight(); x += 2)
        {
            float progress = static_cast<float>(x - waveformArea.getX()) / waveformArea.getWidth();
            float wave = std::sin(progress * 20.0f + currentRecordingDuration * 5.0f) * amplitude * 0.5f;
            wave += std::sin(progress * 8.0f + currentRecordingDuration * 3.0f) * amplitude * 0.3f;

            wavePath.lineTo(x, centerY + wave);
        }

        g.strokePath(wavePath, juce::PathStrokeType(2.0f));
    }
    else if (currentState == State::Preview && recordedAudio.getNumSamples() > 0)
    {
        // Draw actual waveform
        auto path = generateWaveformPath(recordedAudio, waveformArea.reduced(10));
        g.setColour(StoryColors::waveformColor);
        g.strokePath(path, juce::PathStrokeType(1.5f));
    }
    else
    {
        // Placeholder text
        g.setColour(StoryColors::textSecondary);
        g.setFont(12.0f);
        g.drawText("Waveform will appear here", waveformArea, juce::Justification::centred);
    }
}

void StoryRecording::drawPlaybackControls(juce::Graphics& g)
{
    if (playbackControlsArea.isEmpty())
        return;

    auto bounds = playbackControlsArea.reduced(20, 0);
    int buttonSize = 40;
    int buttonX = bounds.getCentreX() - buttonSize / 2;

    // Play/Pause button
    auto playButtonBounds = juce::Rectangle<int>(buttonX, bounds.getY(), buttonSize, buttonSize);

    g.setColour(StoryColors::buttonGreen);
    g.fillEllipse(playButtonBounds.toFloat());

    // Play/Pause icon
    g.setColour(StoryColors::textPrimary);
    juce::Path iconPath;
    auto center = playButtonBounds.getCentre().toFloat();

    if (isPreviewPlaying)
    {
        // Pause icon (two rectangles)
        float barWidth = 4.0f;
        float barHeight = 12.0f;
        iconPath.addRectangle(center.x - barWidth - 2, center.y - barHeight / 2, barWidth, barHeight);
        iconPath.addRectangle(center.x + 2, center.y - barHeight / 2, barWidth, barHeight);
    }
    else
    {
        // Play icon (triangle)
        float size = 10.0f;
        iconPath.addTriangle(center.x - size * 0.3f, center.y - size * 0.4f,
                             center.x - size * 0.3f, center.y + size * 0.4f,
                             center.x + size * 0.4f, center.y);
    }

    g.fillPath(iconPath);

    // Progress indicator (simple line)
    if (bufferAudioPlayer && bufferAudioPlayer->hasBuffer())
    {
        double duration = bufferAudioPlayer->getDurationSeconds();
        if (duration > 0.0)
        {
            float progress = static_cast<float>(bufferAudioPlayer->getPlaybackProgress());
            progress = juce::jlimit(0.0f, 1.0f, progress);

            auto progressBounds = bounds.withY(bounds.getBottom() - 4).withHeight(2);
            g.setColour(StoryColors::progressBg);
            g.fillRect(progressBounds);

            g.setColour(StoryColors::progressFg);
            g.fillRect(progressBounds.withWidth(static_cast<int>(progressBounds.getWidth() * progress)));
        }
    }
}

void StoryRecording::drawMetadataInput(juce::Graphics& g)
{
    if (metadataArea.isEmpty())
        return;

    auto bounds = metadataArea.reduced(10, 5);

    // Background
    g.setColour(StoryColors::surface);
    g.fillRoundedRectangle(bounds.toFloat(), 8.0f);

    // Title
    g.setColour(StoryColors::textSecondary);
    g.setFont(12.0f);
    g.drawText("Metadata (optional)", bounds.removeFromTop(18), juce::Justification::centredLeft);

    bounds.removeFromTop(5);

    // Three columns: BPM, Key, Genre
    int columnWidth = bounds.getWidth() / 3 - 5;
    auto bpmBounds = bounds.withWidth(columnWidth);
    auto keyBounds = bounds.withX(bounds.getX() + columnWidth + 5).withWidth(columnWidth);
    auto genreBounds = bounds.withX(bounds.getX() + (columnWidth + 5) * 2).withWidth(columnWidth);

    // BPM
    g.setColour(StoryColors::textSecondary);
    g.setFont(10.0f);
    g.drawText("BPM", bpmBounds.removeFromTop(14), juce::Justification::centredLeft);
    g.setColour(StoryColors::textPrimary);
    g.setFont(14.0f);
    juce::String bpmText = storyBPM > 0 ? juce::String(storyBPM) : "Auto";
    g.drawText(bpmText, bpmBounds, juce::Justification::centredLeft);

    // Key
    g.setColour(StoryColors::textSecondary);
    g.setFont(10.0f);
    g.drawText("Key", keyBounds.removeFromTop(14), juce::Justification::centredLeft);
    g.setColour(StoryColors::textPrimary);
    g.setFont(14.0f);
    juce::String keyText = storyKey.isNotEmpty() ? storyKey : "None";
    g.drawText(keyText, keyBounds, juce::Justification::centredLeft);

    // Genre
    g.setColour(StoryColors::textSecondary);
    g.setFont(10.0f);
    g.drawText("Genre", genreBounds.removeFromTop(14), juce::Justification::centredLeft);
    g.setColour(StoryColors::textPrimary);
    g.setFont(14.0f);
    juce::String genreText = storyGenres.size() > 0 ? storyGenres.joinIntoString(", ") : "None";
    if (genreText.length() > 20)
        genreText = genreText.substring(0, 20) + "...";
    g.drawText(genreText, genreBounds, juce::Justification::centredLeft);
}

void StoryRecording::drawActionButtons(juce::Graphics& g)
{
    int buttonWidth = actionButtonsArea.getWidth() / 2 - 10;

    // Discard button
    auto discardBounds = actionButtonsArea.withWidth(buttonWidth);
    g.setColour(StoryColors::buttonGray);
    g.fillRoundedRectangle(discardBounds.toFloat(), 8.0f);
    g.setColour(StoryColors::textPrimary);
    g.setFont(14.0f);
    g.drawText("Discard", discardBounds, juce::Justification::centred);

    // Post button
    auto postBounds = actionButtonsArea.withX(actionButtonsArea.getRight() - buttonWidth).withWidth(buttonWidth);
    g.setColour(StoryColors::buttonGreen);
    g.fillRoundedRectangle(postBounds.toFloat(), 8.0f);
    g.setColour(StoryColors::textPrimary);
    g.setFont(juce::Font(14.0f, juce::Font::bold));
    g.drawText("Post Story", postBounds, juce::Justification::centred);
}

//==============================================================================
juce::String StoryRecording::formatTime(double seconds)
{
    int mins = static_cast<int>(seconds) / 60;
    int secs = static_cast<int>(seconds) % 60;
    return juce::String::formatted("%02d:%02d", mins, secs);
}

juce::Path StoryRecording::generateWaveformPath(const juce::AudioBuffer<float>& buffer,
                                                          juce::Rectangle<int> bounds)
{
    juce::Path path;

    if (buffer.getNumSamples() == 0)
        return path;

    int numSamples = buffer.getNumSamples();
    int width = bounds.getWidth();
    float height = bounds.getHeight();
    float centerY = bounds.getCentreY();

    path.startNewSubPath(bounds.getX(), centerY);

    for (int x = 0; x < width; ++x)
    {
        int startSample = x * numSamples / width;
        int endSample = juce::jmin((x + 1) * numSamples / width, numSamples);

        float peak = 0.0f;
        for (int i = startSample; i < endSample; ++i)
        {
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                peak = juce::jmax(peak, std::abs(buffer.getSample(ch, i)));
            }
        }

        float y = centerY - (peak * height * 0.5f);
        path.lineTo(bounds.getX() + x, y);
    }

    return path;
}

//==============================================================================
void StoryRecording::startRecording()
{
    Log::info("StoryRecording: Starting recording");

    // Start audio recording
    audioProcessor.startRecording();

    // Start MIDI capture
    midiCapture.startCapture();

    // Update state
    currentState = State::Recording;
    recordingStartTime = juce::Time::getMillisecondCounterHiRes() / 1000.0;
    currentRecordingDuration = 0.0;
    hasMIDIActivity = false;

    // Reset animations
    recordingDotAnimation.start();

    resized();
    repaint();
}

void StoryRecording::stopRecording()
{
    if (currentState != State::Recording)
        return;

    Log::info("StoryRecording: Stopping recording");

    // Stop audio recording
    audioProcessor.stopRecording();

    // Stop MIDI capture
    auto midiEvents = midiCapture.stopCapture();

    // Get recorded audio
    recordedAudio = audioProcessor.getRecordedAudio();
    recordedSampleRate = audioProcessor.getCurrentSampleRate();

    // Auto-detect BPM from DAW if available
    if (audioProcessor.isBPMAvailable())
    {
        storyBPM = static_cast<int>(audioProcessor.getCurrentBPM());
    }

    // Update state
    currentState = State::Preview;

    // Set up MIDI visualization for preview
    if (pianoRollPreview)
    {
        auto midiData = midiCapture.getNormalizedMIDIDataAsJSON();
        pianoRollPreview->setMIDIData(midiData);
        pianoRollPreview->setVisible(true);
    }

    // Load recorded audio into BufferAudioPlayer for preview playback
    if (bufferAudioPlayer && recordedAudio.getNumSamples() > 0)
    {
        // Prepare buffer audio player with current DAW sample rate
        double dawSampleRate = audioProcessor.getCurrentSampleRate();
        bufferAudioPlayer->prepareToPlay(dawSampleRate, 512);

        // Load the recorded buffer
        bufferAudioPlayer->loadBuffer(recordedAudio, recordedSampleRate);

        // Register with processor so it can be mixed in processBlock
        audioProcessor.setBufferAudioPlayer(bufferAudioPlayer.get());

        Log::info("StoryRecording: Loaded " + juce::String(recordedAudio.getNumSamples()) +
                  " samples at " + juce::String(recordedSampleRate) + "Hz for preview");
    }

    resized();
    repaint();
}

void StoryRecording::discardRecording()
{
    Log::info("StoryRecording: Discarding recording");

    // Stop preview playback
    stopPreviewPlayback();

    // Clear recorded data
    recordedAudio.clear();
    midiCapture.reset();

    // Clear buffer audio player and unregister from processor
    if (bufferAudioPlayer)
    {
        bufferAudioPlayer->clear();
        audioProcessor.setBufferAudioPlayer(nullptr);
    }

    // Hide piano roll
    if (pianoRollPreview)
    {
        pianoRollPreview->clearMIDIData();
        pianoRollPreview->setVisible(false);
    }

    // Reset state
    currentState = State::Idle;
    currentRecordingDuration = 0.0;
    hasMIDIActivity = false;
    previewPlaybackPosition = 0.0;

    if (onRecordingDiscarded)
        onRecordingDiscarded();

    resized();
    repaint();
}

void StoryRecording::confirmRecording()
{
    Log::info("StoryRecording: Confirming recording");

    if (onRecordingComplete)
    {
        // Get normalized and validated MIDI data
        auto midiData = midiCapture.getNormalizedMIDIDataAsJSON();

        onRecordingComplete(recordedAudio, midiData, storyBPM, storyKey, storyGenres);
    }

    // Reset for next recording
    stopPreviewPlayback();
    recordedAudio.clear();
    midiCapture.reset();

    // Clear buffer audio player
    if (bufferAudioPlayer)
    {
        bufferAudioPlayer->clear();
        audioProcessor.setBufferAudioPlayer(nullptr);
    }

    currentState = State::Idle;
    currentRecordingDuration = 0.0;
    storyBPM = 0;
    storyKey = "";
    storyGenres.clear();

    resized();
    repaint();
}

bool StoryRecording::canStopRecording() const
{
    return currentRecordingDuration >= MIN_DURATION_SECONDS;
}

void StoryRecording::togglePreviewPlayback()
{
    if (!bufferAudioPlayer || !bufferAudioPlayer->hasBuffer())
        return;

    if (isPreviewPlaying)
    {
        bufferAudioPlayer->pause();
    }
    else
    {
        // Reset position if at end
        double duration = bufferAudioPlayer->getDurationSeconds();
        if (previewPlaybackPosition >= duration)
        {
            previewPlaybackPosition = 0.0;
            bufferAudioPlayer->seekToPosition(0.0);
        }
        bufferAudioPlayer->play();
    }

    repaint();
}

void StoryRecording::stopPreviewPlayback()
{
    if (bufferAudioPlayer)
    {
        bufferAudioPlayer->stop();
    }

    isPreviewPlaying = false;
    previewPlaybackPosition = 0.0;

    if (pianoRollPreview)
        pianoRollPreview->setPlaybackPosition(0.0);

    repaint();
}
