#include "Recording.h"
#include "../../PluginProcessor.h"
#include "../../util/Colors.h"
#include "../../util/Log.h"
#include "../../util/StringFormatter.h"
#include "../../util/Constants.h"

//==============================================================================
Recording::Recording(SidechainAudioProcessor& processor)
    : audioProcessor(processor)
{
    Log::info("Recording: Initializing recording component");

    // Set up recording dot animation (ping-pong for pulsing effect)
    recordingDotAnimation.setPingPong(true);
    recordingDotAnimation.setRepeatCount(-1);  // Infinite repeat
    recordingDotAnimation.onUpdate = [this](float progress) {
        if (currentState == State::Recording)
            repaint();
    };

    // Start timer for UI updates (~30fps)
    startTimerHz(30);
    Log::debug("Recording: Timer started at 30Hz for UI updates");

    // TODO: Phase 2.1.10 - Test with mono/stereo/surround bus configurations
    // Upload cancellation is already implemented - Upload component has onCancel callback wired up in PluginEditor.cpp (line 163-166)

    Log::info("Recording: Initialization complete");
}

Recording::~Recording()
{
    Log::debug("Recording: Destroying recording component");
    stopTimer();
}

//==============================================================================
void Recording::timerCallback()
{
    // Check if recording stopped externally (e.g., max length reached)
    if (currentState == State::Recording && !audioProcessor.isRecording())
    {
        // Recording stopped (likely hit max length)
        Log::info("Recording::timerCallback: Recording stopped externally (likely max length reached)");
        stopRecording();
    }

    // Update progressive key detection periodically during recording
    // Only update if detector is active and has processed some audio
    // Calling getCurrentKey() before any audio is processed can cause crashes
    if (currentState == State::Recording && ProgressiveKeyDetector::isAvailable() && 
        progressiveKeyDetector.isActive() && progressiveKeyDetector.getSamplesProcessed() > 0)
    {
        updateKeyDetection();
    }

    // Repaint for smooth animations
    if (currentState == State::Recording)
    {
        repaint();
    }
}

//==============================================================================
void Recording::paint(juce::Graphics& g)
{
    // Dark background
    g.fillAll(SidechainColors::background());

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

void Recording::resized()
{
    Log::debug("Recording::resized: Component resized to " + juce::String(getWidth()) + "x" + juce::String(getHeight()));
    // Account for header - component is positioned below header in PluginEditor, but add extra top padding
    // to ensure content doesn't appear too close to header visually
    auto bounds = getLocalBounds();
    bounds.removeFromTop(Constants::Ui::HEADER_HEIGHT / 2); // Add extra spacing from header
    bounds = bounds.reduced(20); // Standard padding on all sides

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

    // Import MIDI button (below action buttons in idle state)
    bounds.removeFromTop(10);
    importMidiButtonArea = bounds.removeFromTop(buttonHeight);
}

void Recording::mouseUp(const juce::MouseEvent& event)
{
    auto pos = event.getPosition();
    Log::debug("Recording::mouseUp: Mouse clicked at (" + juce::String(pos.x) + ", " + juce::String(pos.y) + "), state: " +
               juce::String(currentState == State::Idle ? "Idle" : currentState == State::Recording ? "Recording" : "Preview"));

    switch (currentState)
    {
        case State::Idle:
            if (recordButtonArea.contains(pos))
            {
                Log::info("Recording::mouseUp: Record button clicked in Idle state");
                startRecording();
            }
            else if (importMidiButtonArea.contains(pos))
            {
                Log::info("Recording::mouseUp: Import MIDI button clicked");
                showMidiImportDialog();
            }
            break;

        case State::Recording:
            if (recordButtonArea.contains(pos))
            {
                Log::info("Recording::mouseUp: Stop button clicked in Recording state");
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
                Log::info("Recording::mouseUp: Discard button clicked");
                discardRecording();
            }
            else if (uploadButton.contains(pos))
            {
                Log::info("Recording::mouseUp: Upload/Share button clicked");
                confirmRecording();
            }
            break;
        }
    }
}

//==============================================================================
void Recording::drawIdleState(juce::Graphics& g)
{
    // Draw record button (red circle)
    drawRecordButton(g);

    // Instructions text
    g.setColour(SidechainColors::textSecondary());
    g.setFont(16.0f);
    g.drawText("Press to record audio from your DAW",
               timeDisplayArea, juce::Justification::centredLeft);

    // Show max recording time
    g.setFont(12.0f);
    g.setColour(SidechainColors::textMuted());
    g.drawText("Maximum recording length: 60 seconds",
               progressBarArea, juce::Justification::centred);

    // Draw import MIDI button (R.3.3.6.3)
    drawImportMidiButton(g);
}

void Recording::drawRecordingState(juce::Graphics& g)
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

void Recording::drawPreviewState(juce::Graphics& g)
{
    // Draw smaller record button (to re-record)
    g.setColour(SidechainColors::surface());
    g.fillEllipse(recordButtonArea.toFloat());

    g.setColour(SidechainColors::recording());
    g.fillEllipse(recordButtonArea.reduced(10).toFloat());

    // Show recording duration
    double duration = static_cast<double>(recordedAudio.getNumSamples()) / recordedSampleRate;
    g.setColour(SidechainColors::textPrimary());
    g.setFont(20.0f);
    g.drawText("Recorded: " + formatTime(duration),
               timeDisplayArea, juce::Justification::centredLeft);

    // Draw waveform preview
    drawWaveformPreview(g);

    // Draw action buttons
    drawActionButtons(g);
}

//==============================================================================
void Recording::drawRecordButton(juce::Graphics& g)
{
    bool isRecording = (currentState == State::Recording);

    // Outer ring (darker background)
    g.setColour(SidechainColors::surface());
    g.fillEllipse(recordButtonArea.toFloat());

    // Inner recording indicator
    if (isRecording)
    {
        // Pulsing red with square shape (stop indicator)
        // Use animation progress for smooth pulsing (0.5 to 1.0 opacity)
        float opacity = 0.5f + 0.5f * recordingDotAnimation.getProgress();
        g.setColour(SidechainColors::recording().withAlpha(opacity));
        auto innerRect = recordButtonArea.reduced(recordButtonArea.getWidth() / 4);
        g.fillRoundedRectangle(innerRect.toFloat(), 4.0f);
    }
    else
    {
        // Red circle (record indicator)
        g.setColour(SidechainColors::recording());
        g.fillEllipse(recordButtonArea.reduced(10).toFloat());
    }
}

void Recording::drawTimeDisplay(juce::Graphics& g)
{
    double seconds = audioProcessor.getRecordingLengthSeconds();

    // Log periodically to avoid spam (every 5 seconds)
    static double lastLoggedTime = -1.0;
    if (seconds - lastLoggedTime >= 5.0)
    {
        Log::debug("Recording::drawTimeDisplay: Recording time: " + formatTime(seconds));
        lastLoggedTime = seconds;
    }

    // Large time display
    g.setColour(SidechainColors::textPrimary());
    g.setFont(juce::Font(32.0f, juce::Font::bold));
    g.drawText(formatTime(seconds), timeDisplayArea.removeFromTop(40),
               juce::Justification::centredLeft);

    // Recording indicator text
    float opacity = 0.5f + 0.5f * recordingDotAnimation.getProgress();
    g.setColour(SidechainColors::recording().withAlpha(opacity));
    g.setFont(14.0f);
    g.drawText("RECORDING", timeDisplayArea.removeFromTop(20),
               juce::Justification::centredLeft);

    // Draw key detection result if available
    if (currentState == State::Recording)
    {
        drawKeyDisplay(g);
    }
}

void Recording::drawKeyDisplay(juce::Graphics& g)
{
    if (!ProgressiveKeyDetector::isAvailable())
        return;

    if (detectedKey.isValid())
    {
        g.setColour(SidechainColors::textSecondary());
        g.setFont(12.0f);
        juce::String keyText = "Key: " + detectedKey.name;
        if (detectedKey.camelot.isNotEmpty())
        {
            keyText += " (" + detectedKey.camelot + ")";
        }
        g.drawText(keyText, timeDisplayArea, juce::Justification::centredLeft);
    }
    else if (progressiveKeyDetector.isActive() && !isDetectingKey)
    {
        g.setColour(SidechainColors::textMuted());
        g.setFont(12.0f);
        g.drawText("Analyzing key...", timeDisplayArea, juce::Justification::centredLeft);
    }
}

void Recording::drawLevelMeters(juce::Graphics& g)
{
    // Get levels from processor
    float peakL = audioProcessor.getPeakLevel(0);
    float peakR = audioProcessor.getPeakLevel(1);
    float rmsL = audioProcessor.getRMSLevel(0);
    float rmsR = audioProcessor.getRMSLevel(1);

    // Log peak levels periodically to avoid spam (when levels are significant)
    static float lastLoggedPeakL = -1.0f;
    static float lastLoggedPeakR = -1.0f;
    if (std::abs(peakL - lastLoggedPeakL) > 0.1f || std::abs(peakR - lastLoggedPeakR) > 0.1f)
    {
        Log::debug("Recording::drawLevelMeters: Peak levels - L: " + juce::String(peakL, 2) +
                   ", R: " + juce::String(peakR, 2) + ", RMS - L: " + juce::String(rmsL, 2) +
                   ", R: " + juce::String(rmsR, 2));
        lastLoggedPeakL = peakL;
        lastLoggedPeakR = peakR;
    }

    // Draw background
    g.setColour(SidechainColors::backgroundLight());
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

void Recording::drawSingleMeter(juce::Graphics& g, juce::Rectangle<int> bounds,
                                         float peak, float rms, const juce::String& label)
{
    // Label
    g.setColour(SidechainColors::textMuted());
    g.setFont(12.0f);
    auto labelArea = bounds.removeFromLeft(20);
    g.drawText(label, labelArea, juce::Justification::centred);

    // Meter background
    g.setColour(SidechainColors::background());
    g.fillRoundedRectangle(bounds.toFloat(), 2.0f);

    // RMS level (darker green)
    float rmsWidth = bounds.getWidth() * juce::jlimit(0.0f, 1.0f, rms);
    if (rmsWidth > 0)
    {
        g.setColour(SidechainColors::success().darker(0.3f));
        g.fillRoundedRectangle(bounds.withWidth(static_cast<int>(rmsWidth)).toFloat(), 2.0f);
    }

    // Peak level (bright green, yellow, red gradient)
    float peakWidth = bounds.getWidth() * juce::jlimit(0.0f, 1.0f, peak);
    if (peakWidth > 0)
    {
        juce::Colour peakColor;
        if (peak < 0.7f)
            peakColor = SidechainColors::success();
        else if (peak < 0.9f)
            peakColor = SidechainColors::warning();
        else
            peakColor = SidechainColors::recording();

        g.setColour(peakColor);
        auto peakBar = bounds.withWidth(static_cast<int>(peakWidth));
        peakBar = peakBar.withHeight(bounds.getHeight() / 2).withY(bounds.getY() + bounds.getHeight() / 4);
        g.fillRoundedRectangle(peakBar.toFloat(), 1.0f);
    }
}

void Recording::drawProgressBar(juce::Graphics& g)
{
    float progress = audioProcessor.getRecordingProgress();
    double maxSeconds = audioProcessor.getMaxRecordingLengthSeconds();

    // Background
    g.setColour(SidechainColors::backgroundLight());
    g.fillRoundedRectangle(progressBarArea.toFloat(), 4.0f);

    // Progress fill
    int fillWidth = static_cast<int>(progressBarArea.getWidth() * progress);
    if (fillWidth > 0)
    {
        auto fillRect = progressBarArea.withWidth(fillWidth);

        // Gradient from green to yellow to red
        juce::Colour fillColor;
        if (progress < 0.7f)
            fillColor = SidechainColors::success();
        else if (progress < 0.9f)
            fillColor = SidechainColors::warning();
        else
            fillColor = SidechainColors::recording();

        g.setColour(fillColor);
        g.fillRoundedRectangle(fillRect.toFloat(), 4.0f);
    }

    // Time labels
    g.setColour(SidechainColors::textMuted());
    g.setFont(10.0f);
    g.drawText("0:00", progressBarArea.withY(progressBarArea.getBottom() + 2),
               juce::Justification::left);
    g.drawText(formatTime(maxSeconds), progressBarArea.withY(progressBarArea.getBottom() + 2),
               juce::Justification::right);
}

void Recording::drawWaveformPreview(juce::Graphics& g)
{
    if (recordedAudio.getNumSamples() == 0)
        return;

    // Background
    g.setColour(SidechainColors::waveformBackground());
    g.fillRoundedRectangle(waveformArea.toFloat(), 4.0f);

    // Generate and draw waveform path
    auto path = generateWaveformPath(recordedAudio, waveformArea.reduced(4));

    g.setColour(SidechainColors::waveform());
    g.strokePath(path, juce::PathStrokeType(1.5f));
}

void Recording::drawActionButtons(juce::Graphics& g)
{
    int buttonWidth = actionButtonsArea.getWidth() / 2 - 10;

    // Discard button (left)
    auto discardButton = actionButtonsArea.withWidth(buttonWidth);
    g.setColour(SidechainColors::buttonSecondary());
    g.fillRoundedRectangle(discardButton.toFloat(), 8.0f);
    g.setColour(SidechainColors::textPrimary());
    g.setFont(16.0f);
    g.drawText("Discard", discardButton, juce::Justification::centred);

    // Upload button (right)
    auto uploadButton = actionButtonsArea.withX(actionButtonsArea.getRight() - buttonWidth).withWidth(buttonWidth);
    g.setColour(SidechainColors::primary());
    g.fillRoundedRectangle(uploadButton.toFloat(), 8.0f);
    g.setColour(SidechainColors::textPrimary());
    g.drawText("Share Loop", uploadButton, juce::Justification::centred);
}

//==============================================================================
juce::String Recording::formatTime(double seconds)
{
    return StringFormatter::formatDurationMMSS(seconds);
}

juce::Path Recording::generateWaveformPath(const juce::AudioBuffer<float>& buffer,
                                                    juce::Rectangle<int> bounds)
{
    juce::Path path;

    if (buffer.getNumSamples() == 0)
    {
        Log::warn("Recording::generateWaveformPath: Empty buffer, returning empty path");
        return path;
    }

    int numSamples = buffer.getNumSamples();
    int width = bounds.getWidth();
    float height = bounds.getHeight();
    float centerY = bounds.getCentreY();

    Log::debug("Recording::generateWaveformPath: Generating waveform - samples: " + juce::String(numSamples) +
               ", width: " + juce::String(width) + ", channels: " + juce::String(buffer.getNumChannels()));

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

    Log::debug("Recording::generateWaveformPath: Waveform path generated with " + juce::String(width) + " points");
    return path;
}

//==============================================================================
void Recording::startRecording()
{
    Log::info("Recording::startRecording: Starting recording");
    
    // Safety check: ensure processor is prepared before starting
    double sampleRate = audioProcessor.getCurrentSampleRate();
    if (sampleRate <= 0.0)
    {
        Log::error("Recording::startRecording: Invalid sample rate (" + juce::String(sampleRate) + 
                   "Hz). Processor may not be prepared. Using default.");
        sampleRate = Constants::Audio::DEFAULT_SAMPLE_RATE;
    }
    
    audioProcessor.startRecording();
    currentState = State::Recording;

    // Start pulsing animation
    recordingDotAnimation.start();

    // Start progressive key detection if available
    if (ProgressiveKeyDetector::isAvailable())
    {
        if (progressiveKeyDetector.start(sampleRate))
        {
            Log::info("Recording::startRecording: Progressive key detection started at " + juce::String(sampleRate, 1) + "Hz");
            detectedKey = KeyDetector::Key();  // Reset
            keyDetectionBuffer.setSize(2, 0);  // Clear buffer
            keyDetectionSamplesAccumulated = 0;
            isDetectingKey = false;
        }
        else
        {
            Log::warn("Recording::startRecording: Failed to start progressive key detection");
        }
    }

    Log::debug("Recording::startRecording: State changed to Recording, animation started");
    repaint();
}

void Recording::stopRecording()
{
    Log::info("Recording::stopRecording: Stopping recording");

    // Stop pulsing animation
    recordingDotAnimation.stop();

    // Finalize progressive key detection
    if (progressiveKeyDetector.isActive())
    {
        // Process any remaining accumulated audio
        if (keyDetectionBuffer.getNumSamples() > 0)
        {
            processKeyDetectionChunk(keyDetectionBuffer);
            keyDetectionBuffer.setSize(2, 0);
        }

        // Finalize and get final key
        if (progressiveKeyDetector.finalize())
        {
            detectedKey = progressiveKeyDetector.getFinalKey();
            if (detectedKey.isValid())
            {
                Log::info("Recording::stopRecording: Final key detected: " + detectedKey.name +
                         " (Camelot: " + detectedKey.camelot + ")");
            }
        }
        progressiveKeyDetector.reset();
    }

    audioProcessor.stopRecording();
    recordedAudio = audioProcessor.getRecordedAudio();
    recordedSampleRate = audioProcessor.getCurrentSampleRate();

    int numSamples = recordedAudio.getNumSamples();
    int numChannels = recordedAudio.getNumChannels();
    double duration = static_cast<double>(numSamples) / recordedSampleRate;

    Log::debug("Recording::stopRecording: Recording stopped - samples: " + juce::String(numSamples) +
               ", channels: " + juce::String(numChannels) + ", sampleRate: " + juce::String(recordedSampleRate, 1) +
               "Hz, duration: " + juce::String(duration, 2) + "s");

    if (recordedAudio.getNumSamples() > 0)
    {
        currentState = State::Preview;
        Log::info("Recording::stopRecording: Recording complete, showing preview - " +
            juce::String(recordedAudio.getNumSamples()) + " samples captured, duration: " + formatTime(duration));
    }
    else
    {
        currentState = State::Idle;
        Log::warn("Recording::stopRecording: Recording stopped but no audio captured");
    }

    repaint();
}

void Recording::discardRecording()
{
    Log::info("Recording::discardRecording: Discarding recording");
    int discardedSamples = recordedAudio.getNumSamples();
    recordedAudio.setSize(0, 0);
    currentState = State::Idle;
    Log::debug("Recording::discardRecording: State reset to Idle, discarded " + juce::String(discardedSamples) + " samples");

    if (onRecordingDiscarded)
    {
        Log::debug("Recording::discardRecording: Calling onRecordingDiscarded callback");
        onRecordingDiscarded();
    }
    else
    {
        Log::warn("Recording::discardRecording: onRecordingDiscarded callback not set");
    }

    repaint();
}

void Recording::confirmRecording()
{
    int numSamples = recordedAudio.getNumSamples();
    int numChannels = recordedAudio.getNumChannels();
    double duration = static_cast<double>(numSamples) / recordedSampleRate;

    Log::info("Recording::confirmRecording: Confirming recording for upload - samples: " + juce::String(numSamples) +
              ", channels: " + juce::String(numChannels) + ", duration: " + formatTime(duration));

    if (onRecordingComplete && recordedAudio.getNumSamples() > 0)
    {
        // Make a copy of the audio data before any state changes
        // This prevents potential race conditions with async repaints
        juce::AudioBuffer<float> audioCopy(recordedAudio);
        Log::debug("Recording::confirmRecording: Created audio copy for callback");

        // Get MIDI data - either from capture or from import (R.3.3.6.3)
        juce::var midiData;
        if (hasImportedMidi && !importedMidiData.isVoid())
        {
            // Use imported MIDI data
            midiData = importedMidiData;
            Log::info("Recording::confirmRecording: Using imported MIDI data");
        }
        else
        {
            // Get captured MIDI from processor
            midiData = audioProcessor.getCapturedMIDIData();
            Log::info("Recording::confirmRecording: Using captured MIDI data");
        }

        // Reset state BEFORE calling callback to prevent any repaint
        // from accessing the original buffer while it's being modified
        recordedAudio.setSize(0, 0);
        currentState = State::Idle;
        importedMidiData = juce::var();  // Clear imported MIDI
        hasImportedMidi = false;
        Log::debug("Recording::confirmRecording: State reset to Idle before callback");

        // Now call the callback with the copy - safe even if callback
        // triggers view changes that cause repaints
        Log::info("Recording::confirmRecording: Calling onRecordingComplete callback");
        onRecordingComplete(audioCopy, midiData);
    }
    else
    {
        if (!onRecordingComplete)
        {
            Log::warn("Recording::confirmRecording: onRecordingComplete callback not set");
        }
        if (recordedAudio.getNumSamples() == 0)
        {
            Log::warn("Recording::confirmRecording: No audio to share");
        }
        // No audio to share, just reset state
        recordedAudio.setSize(0, 0);
        currentState = State::Idle;
    }

    repaint();
}

//==============================================================================
void Recording::updateKeyDetection()
{
    // Update key detection display from progressive detector
    // Only call getCurrentKey() if detector is active and has processed samples
    // Calling it before any audio chunks are added can cause crashes
    if (progressiveKeyDetector.isActive() && progressiveKeyDetector.getSamplesProcessed() > 0)
    {
        auto result = progressiveKeyDetector.getCurrentKey();
        if (result.isValid())
        {
            detectedKey = result;
        }
    }
}

void Recording::processKeyDetectionChunk(const juce::AudioBuffer<float>& buffer)
{
    // Process a chunk of audio for key detection
    if (progressiveKeyDetector.isActive() && buffer.getNumSamples() > 0)
    {
        progressiveKeyDetector.addAudioChunk(buffer, buffer.getNumChannels());
    }
}

//==============================================================================
// MIDI Import (R.3.3.6.3)

void Recording::drawImportMidiButton(juce::Graphics& g)
{
    bool isHovered = importMidiButtonArea.contains(getMouseXYRelative());
    auto bgColor = isHovered ? SidechainColors::surfaceHover() : SidechainColors::surface();

    g.setColour(bgColor);
    g.fillRoundedRectangle(importMidiButtonArea.toFloat(), 8.0f);

    // Border
    g.setColour(SidechainColors::border());
    g.drawRoundedRectangle(importMidiButtonArea.toFloat(), 8.0f, 1.0f);

    // Text
    g.setColour(SidechainColors::textPrimary());
    g.setFont(14.0f);

    juce::String buttonText = hasImportedMidi ? "MIDI Imported ✓" : "Import MIDI File...";
    g.drawText(buttonText, importMidiButtonArea, juce::Justification::centred);
}

void Recording::showMidiImportDialog()
{
    Log::info("Recording::showMidiImportDialog: Opening MIDI file chooser");

    // Create file chooser for MIDI files
    auto chooser = std::make_shared<juce::FileChooser>(
        "Select a MIDI file to import",
        juce::File::getSpecialLocation(juce::File::userMusicDirectory),
        "*.mid;*.midi"
    );

    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this, chooser](const juce::FileChooser& fc)
        {
            auto results = fc.getResults();
            if (results.size() > 0)
            {
                auto file = results[0];
                Log::info("Recording::showMidiImportDialog: User selected file: " + file.getFullPathName());
                importMidiFile(file);
            }
            else
            {
                Log::debug("Recording::showMidiImportDialog: User cancelled file selection");
            }
        });
}

void Recording::importMidiFile(const juce::File& file)
{
    Log::info("Recording::importMidiFile: Importing MIDI file: " + file.getFileName());

    // Read and parse MIDI file using JUCE
    juce::MidiFile midiFile;
    juce::FileInputStream inputStream(file);

    if (!inputStream.openedOk())
    {
        Log::error("Recording::importMidiFile: Failed to open MIDI file");
        return;
    }

    if (!midiFile.readFrom(inputStream))
    {
        Log::error("Recording::importMidiFile: Failed to parse MIDI file");
        return;
    }

    // Convert MIDI file to our JSON format
    juce::Array<juce::var> eventsArray;
    double tempo = 120.0;  // Default tempo
    int timeSignatureNum = 4;
    int timeSignatureDen = 4;

    // Get time format - convert to seconds
    short timeFormat = midiFile.getTimeFormat();
    double ticksPerSecond = 500.0;  // Default for PPQN at 120 BPM

    if (timeFormat > 0)
    {
        // PPQN format - ticksPerQuarterNote
        double ticksPerQuarterNote = static_cast<double>(timeFormat);
        ticksPerSecond = ticksPerQuarterNote * (tempo / 60.0);
    }

    // Process all tracks
    for (int track = 0; track < midiFile.getNumTracks(); ++track)
    {
        const juce::MidiMessageSequence* sequence = midiFile.getTrack(track);
        if (sequence == nullptr)
            continue;

        for (int i = 0; i < sequence->getNumEvents(); ++i)
        {
            const juce::MidiMessageSequence::MidiEventHolder* eventHolder = sequence->getEventPointer(i);
            const juce::MidiMessage& message = eventHolder->message;
            double timeInSeconds = message.getTimeStamp() / ticksPerSecond;

            // Look for tempo changes
            if (message.isTempoMetaEvent())
            {
                tempo = message.getTempoSecondsPerQuarterNote() > 0
                    ? 60.0 / message.getTempoSecondsPerQuarterNote()
                    : 120.0;
                // Update ticks conversion
                if (timeFormat > 0)
                {
                    ticksPerSecond = static_cast<double>(timeFormat) * (tempo / 60.0);
                }
            }
            // Look for time signature
            else if (message.isTimeSignatureMetaEvent())
            {
                int numerator, denominator;
                message.getTimeSignatureInfo(numerator, denominator);
                timeSignatureNum = numerator;
                timeSignatureDen = denominator;
            }
            // Note on
            else if (message.isNoteOn())
            {
                juce::DynamicObject::Ptr event = new juce::DynamicObject();
                event->setProperty("time", timeInSeconds);
                event->setProperty("type", "note_on");
                event->setProperty("note", message.getNoteNumber());
                event->setProperty("velocity", message.getVelocity());
                event->setProperty("channel", message.getChannel() - 1);  // JUCE uses 1-based channels
                eventsArray.add(juce::var(event.get()));
            }
            // Note off
            else if (message.isNoteOff())
            {
                juce::DynamicObject::Ptr event = new juce::DynamicObject();
                event->setProperty("time", timeInSeconds);
                event->setProperty("type", "note_off");
                event->setProperty("note", message.getNoteNumber());
                event->setProperty("velocity", 0);
                event->setProperty("channel", message.getChannel() - 1);
                eventsArray.add(juce::var(event.get()));
            }
        }
    }

    // Build the MIDI data JSON structure
    juce::DynamicObject::Ptr midiData = new juce::DynamicObject();
    midiData->setProperty("events", eventsArray);
    midiData->setProperty("tempo", tempo);
    midiData->setProperty("time_signature", juce::String(timeSignatureNum) + "/" + juce::String(timeSignatureDen));

    importedMidiData = juce::var(midiData.get());
    hasImportedMidi = true;

    Log::info("Recording::importMidiFile: Successfully imported " + juce::String(eventsArray.size()) +
              " MIDI events, tempo: " + juce::String(tempo, 1) + " BPM");

    repaint();
}
