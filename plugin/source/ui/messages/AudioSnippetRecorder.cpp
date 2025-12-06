#include "AudioSnippetRecorder.h"
#include "../../PluginProcessor.h"
#include "../../util/Colors.h"
#include "../../util/Log.h"

//==============================================================================
AudioSnippetRecorder::AudioSnippetRecorder(SidechainAudioProcessor& processor)
    : audioProcessor(processor)
{
    Log::info("AudioSnippetRecorder: Initializing");

    // Start timer for UI updates (~30fps)
    startTimerHz(30);
    Log::debug("AudioSnippetRecorder: Timer started at 30Hz for UI updates");
}

AudioSnippetRecorder::~AudioSnippetRecorder()
{
    Log::debug("AudioSnippetRecorder: Destroying");
    stopTimer();

    // Stop recording if active
    if (currentState == State::Recording)
    {
        audioProcessor.stopRecording();
    }
}

//==============================================================================
void AudioSnippetRecorder::timerCallback()
{
    // Check if recording stopped externally (e.g., max length reached)
    if (currentState == State::Recording && !audioProcessor.isRecording())
    {
        Log::info("AudioSnippetRecorder::timerCallback: Recording stopped externally (likely max length reached)");
        stopRecording();
    }

    // Auto-stop if max duration reached
    if (currentState == State::Recording && hasReachedMaxDuration())
    {
        Log::info("AudioSnippetRecorder::timerCallback: Max duration reached, auto-stopping");
        stopRecording();
    }

    // Repaint for smooth animations and timer updates
    if (currentState == State::Recording)
    {
        repaint();
    }
}

//==============================================================================
void AudioSnippetRecorder::paint(juce::Graphics& g)
{
    // Dark background
    g.fillAll(juce::Colour(0xff1a1a1a));

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

void AudioSnippetRecorder::resized()
{
    auto bounds = getLocalBounds().reduced(10);

    // Compact layout for message input area
    int buttonSize = 40;
    int timerWidth = 60;
    int waveformHeight = 50;

    // Record button on left
    recordButtonArea = bounds.removeFromLeft(buttonSize).reduced(5);

    // Timer next to button
    timerArea = bounds.removeFromLeft(timerWidth).reduced(5);

    // Waveform takes remaining space (if recording/preview)
    if (currentState == State::Recording || currentState == State::Preview)
    {
        waveformArea = bounds.removeFromLeft(bounds.getWidth() - buttonSize * 2 - 10).withHeight(waveformHeight).reduced(5);

        // Cancel and send buttons on right
        if (currentState == State::Preview)
        {
            cancelButtonArea = bounds.removeFromRight(buttonSize).reduced(5);
            sendButtonArea = bounds.removeFromRight(buttonSize).reduced(5);
        }
    }
}

//==============================================================================
void AudioSnippetRecorder::mouseDown(const juce::MouseEvent& event)
{
    if (recordButtonArea.contains(event.getPosition()))
    {
        if (currentState == State::Idle)
        {
            startRecording();
        }
        else if (currentState == State::Recording)
        {
            stopRecording();
        }
    }
    else if (cancelButtonArea.contains(event.getPosition()) && currentState == State::Preview)
    {
        cancelRecording();
    }
    else if (sendButtonArea.contains(event.getPosition()) && currentState == State::Preview)
    {
        sendRecording();
    }
}

void AudioSnippetRecorder::mouseUp(const juce::MouseEvent& event)
{
    // For hold-to-record, we could implement here, but for now using toggle
}

//==============================================================================
void AudioSnippetRecorder::drawIdleState(juce::Graphics& g)
{
    drawRecordButton(g, false);
}

void AudioSnippetRecorder::drawRecordingState(juce::Graphics& g)
{
    drawRecordButton(g, true);
    drawTimer(g);
    drawWaveform(g);
}

void AudioSnippetRecorder::drawPreviewState(juce::Graphics& g)
{
    drawRecordButton(g, false);
    drawTimer(g);
    drawWaveform(g);
    drawCancelButton(g);
    drawSendButton(g);
}

void AudioSnippetRecorder::drawRecordButton(juce::Graphics& g, bool isRecording)
{
    juce::Colour buttonColor = isRecording ? juce::Colour(0xffff0000) : SidechainColors::primary();

    g.setColour(buttonColor);
    g.fillEllipse(recordButtonArea.toFloat());

    // White circle in center
    g.setColour(juce::Colours::white);
    auto center = recordButtonArea.getCentre().toFloat();
    float radius = isRecording ? 8.0f : 12.0f;
    g.fillEllipse(center.x - radius, center.y - radius, radius * 2, radius * 2);
}

void AudioSnippetRecorder::drawTimer(juce::Graphics& g)
{
    double duration = getRecordingDuration();
    juce::String timeText = formatTime(duration);

    g.setColour(juce::Colours::white);
    g.setFont(14.0f);
    g.drawText(timeText, timerArea, juce::Justification::centred);

    // Show max duration indicator
    if (currentState == State::Recording)
    {
        float progress = static_cast<float>(duration / MAX_DURATION_SECONDS);
        progress = juce::jlimit(0.0f, 1.0f, progress);

        // Progress bar below timer
        auto progressBar = timerArea.withY(timerArea.getBottom() - 3).withHeight(2);
        g.setColour(juce::Colour(0xff3a3a3a));
        g.fillRect(progressBar);

        g.setColour(progress > 0.9f ? juce::Colour(0xffff0000) : SidechainColors::primary());
        g.fillRect(progressBar.withWidth(static_cast<int>(progressBar.getWidth() * progress)));
    }
}

void AudioSnippetRecorder::drawWaveform(juce::Graphics& g)
{
    if (waveformArea.isEmpty())
        return;

    if (currentState == State::Recording)
    {
        // Show live waveform from processor
        // For now, show a simple animated waveform placeholder
        g.setColour(SidechainColors::primary().withAlpha(0.5f));
        g.fillRoundedRectangle(waveformArea.toFloat(), 4.0f);

        // Draw animated bars (simplified - in real implementation, use actual audio levels)
        int numBars = 20;
        float barWidth = static_cast<float>(waveformArea.getWidth()) / numBars;
        float maxHeight = static_cast<float>(waveformArea.getHeight());

        for (int i = 0; i < numBars; ++i)
        {
            float height = maxHeight * (0.3f + 0.7f * std::sin((i * 0.5f + juce::Time::currentTimeMillis() * 0.01f) * 0.1f));
            height = juce::jlimit(2.0f, maxHeight, height);

            auto bar = juce::Rectangle<float>(
                waveformArea.getX() + i * barWidth + 2,
                waveformArea.getY() + (maxHeight - height) * 0.5f,
                barWidth - 4,
                height
            );

            g.setColour(SidechainColors::primary());
            g.fillRoundedRectangle(bar, 2.0f);
        }
    }
    else if (currentState == State::Preview && recordedAudio.getNumSamples() > 0)
    {
        // Show recorded waveform
        juce::Path waveformPath = generateWaveformPath(recordedAudio, waveformArea);
        g.setColour(SidechainColors::primary());
        g.strokePath(waveformPath, juce::PathStrokeType(2.0f));
    }
}

void AudioSnippetRecorder::drawCancelButton(juce::Graphics& g)
{
    g.setColour(juce::Colour(0xff888888));
    g.fillEllipse(cancelButtonArea.toFloat());

    // X icon
    g.setColour(juce::Colours::white);
    g.setFont(20.0f);
    g.drawText("×", cancelButtonArea, juce::Justification::centred);
}

void AudioSnippetRecorder::drawSendButton(juce::Graphics& g)
{
    g.setColour(SidechainColors::primary());
    g.fillEllipse(sendButtonArea.toFloat());

    // Send icon (arrow)
    g.setColour(juce::Colours::white);
    g.setFont(20.0f);
    g.drawText("→", sendButtonArea, juce::Justification::centred);
}

//==============================================================================
juce::Path AudioSnippetRecorder::generateWaveformPath(const juce::AudioBuffer<float>& buffer,
                                                      juce::Rectangle<int> bounds)
{
    juce::Path path;

    if (buffer.getNumSamples() == 0)
        return path;

    int numSamples = buffer.getNumSamples();
    int numChannels = buffer.getNumChannels();
    // Use first channel (mono) for waveform visualization
    if (numChannels == 0)
        return path;  // Safety check: no channels available
    int channelToUse = 0;

    float width = static_cast<float>(bounds.getWidth());
    float height = static_cast<float>(bounds.getHeight());
    float centerY = bounds.getY() + height * 0.5f;

    // Sample the buffer (take every Nth sample for performance)
    int step = juce::jmax(1, numSamples / static_cast<int>(width));

    path.startNewSubPath(static_cast<float>(bounds.getX()), centerY);

    for (int i = 0; i < numSamples; i += step)
    {
        float sample = buffer.getSample(channelToUse, i);
        float x = bounds.getX() + (static_cast<float>(i) / numSamples) * width;
        float y = centerY - sample * height * 0.4f;

        if (i == 0)
            path.startNewSubPath(x, y);
        else
            path.lineTo(x, y);
    }

    return path;
}

juce::String AudioSnippetRecorder::formatTime(double seconds)
{
    int minutes = static_cast<int>(seconds) / 60;
    int secs = static_cast<int>(seconds) % 60;
    return juce::String::formatted("%d:%02d", minutes, secs);
}

//==============================================================================
void AudioSnippetRecorder::startRecording()
{
    // Start recording an audio snippet for message sending.
    // IMPORTANT GOTCHAS:
    // - Cannot start if processor is already recording (e.g., for a full post upload)
    // - Shows error alert if processor is busy (already implemented below)
    // - Maximum duration is 30 seconds (enforced automatically)
    // - Recording state is managed by SidechainAudioProcessor
    Log::info("AudioSnippetRecorder::startRecording: Starting recording");

    // Check if processor is already recording (e.g., for a full post)
    if (audioProcessor.isRecording())
    {
        Log::warn("AudioSnippetRecorder::startRecording: Processor already recording, cannot start snippet");
        juce::MessageManager::callAsync([]() {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::WarningIcon,
                "Recording Busy",
                "Cannot start audio snippet recording. The audio processor is already recording.");
        });
        return;
    }

    audioProcessor.startRecording();
    recordingStartTime = juce::Time::getCurrentTime();
    currentState = State::Recording;

    Log::debug("AudioSnippetRecorder::startRecording: State changed to Recording");
    repaint();
}

void AudioSnippetRecorder::stopRecording()
{
    // Stop recording and transition to preview state.
    // IMPORTANT GOTCHAS:
    // - Sample rate is captured before stopping (may change during recording)
    // - Audio buffer is copied from processor (processor may continue recording for other purposes)
    // - Automatically transitions to Preview state if recording was successful
    // - If recording failed or was empty, returns to Idle state
    Log::info("AudioSnippetRecorder::stopRecording: Stopping recording");

    // Get sample rate before stopping (in case it changes)
    recordedSampleRate = audioProcessor.getCurrentSampleRate();

    audioProcessor.stopRecording();
    recordedAudio = audioProcessor.getRecordedAudio();

    int numSamples = recordedAudio.getNumSamples();
    double duration = static_cast<double>(numSamples) / recordedSampleRate;

    Log::debug("AudioSnippetRecorder::stopRecording: Recording stopped - samples: " + juce::String(numSamples) +
               ", duration: " + formatTime(duration));

    if (recordedAudio.getNumSamples() > 0)
    {
        currentState = State::Preview;
        Log::info("AudioSnippetRecorder::stopRecording: Recording complete, showing preview");
    }
    else
    {
        currentState = State::Idle;
        Log::warn("AudioSnippetRecorder::stopRecording: Recording stopped but no audio captured");
    }

    resized(); // Update layout for preview state
    repaint();
}

void AudioSnippetRecorder::cancelRecording()
{
    Log::info("AudioSnippetRecorder::cancelRecording: Cancelling recording");

    recordedAudio.setSize(0, 0);
    currentState = State::Idle;

    if (onRecordingCancelled)
    {
        onRecordingCancelled();
    }

    resized(); // Update layout for idle state
    repaint();
}

void AudioSnippetRecorder::sendRecording()
{
    Log::info("AudioSnippetRecorder::sendRecording: Sending recording");

    if (onRecordingComplete && recordedAudio.getNumSamples() > 0)
    {
        // Make a copy of the audio data
        juce::AudioBuffer<float> audioCopy(recordedAudio);

        // Reset state
        recordedAudio.setSize(0, 0);
        currentState = State::Idle;

        // Call callback
        onRecordingComplete(audioCopy, recordedSampleRate);

        resized(); // Update layout for idle state
    }
    else
    {
        Log::warn("AudioSnippetRecorder::sendRecording: No audio to send or callback not set");
    }

    repaint();
}

double AudioSnippetRecorder::getRecordingDuration() const
{
    if (currentState == State::Recording)
    {
        juce::int64 elapsed = juce::Time::currentTimeMillis() - recordingStartTime.toMilliseconds();
        return elapsed / 1000.0;
    }
    else if (currentState == State::Preview && recordedAudio.getNumSamples() > 0)
    {
        return static_cast<double>(recordedAudio.getNumSamples()) / recordedSampleRate;
    }
    return 0.0;
}

bool AudioSnippetRecorder::hasReachedMaxDuration() const
{
    return getRecordingDuration() >= MAX_DURATION_SECONDS;
}
