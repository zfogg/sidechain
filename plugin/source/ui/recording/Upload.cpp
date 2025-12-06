#include "Upload.h"
#include "../../PluginProcessor.h"
#include "../../util/Colors.h"
#include "../../util/Constants.h"
#include "../../util/StringFormatter.h"
#include "../../util/Async.h"
#include "../../util/Log.h"
#include "../../util/Result.h"

//==============================================================================
// Static data: Musical keys (Camelot wheel order is producer-friendly)
const std::array<Upload::MusicalKey, Upload::NUM_KEYS>& Upload::getMusicalKeys()
{
    static const std::array<MusicalKey, NUM_KEYS> keys = {{
        { "Not set", "-" },
        { "C Major", "C" },
        { "C# / Db Major", "C#" },
        { "D Major", "D" },
        { "D# / Eb Major", "D#" },
        { "E Major", "E" },
        { "F Major", "F" },
        { "F# / Gb Major", "F#" },
        { "G Major", "G" },
        { "G# / Ab Major", "G#" },
        { "A Major", "A" },
        { "A# / Bb Major", "A#" },
        { "B Major", "B" },
        { "C Minor", "Cm" },
        { "C# / Db Minor", "C#m" },
        { "D Minor", "Dm" },
        { "D# / Eb Minor", "D#m" },
        { "E Minor", "Em" },
        { "F Minor", "Fm" },
        { "F# / Gb Minor", "F#m" },
        { "G Minor", "Gm" },
        { "G# / Ab Minor", "G#m" },
        { "A Minor", "Am" },
        { "A# / Bb Minor", "A#m" },
        { "B Minor", "Bm" }
    }};
    return keys;
}

// Static data: Genres
const std::array<juce::String, Upload::NUM_GENRES>& Upload::getGenres()
{
    static const std::array<juce::String, NUM_GENRES> genres = {{
        "Electronic",
        "Hip-Hop / Trap",
        "House",
        "Techno",
        "Drum & Bass",
        "Dubstep",
        "Pop",
        "R&B / Soul",
        "Rock",
        "Lo-Fi",
        "Ambient",
        "Other"
    }};
    return genres;
}

//==============================================================================
Upload::Upload(SidechainAudioProcessor& processor, NetworkClient& network)
    : audioProcessor(processor), networkClient(network)
{
    Log::info("Upload: Initializing upload component");
    setWantsKeyboardFocus(true);
    startTimerHz(30);
    Log::debug("Upload: Timer started at 30Hz, keyboard focus enabled");
    Log::info("Upload: Initialization complete");
}

Upload::~Upload()
{
    Log::debug("Upload: Destroying upload component");
    stopTimer();
}

//==============================================================================
void Upload::setAudioToUpload(const juce::AudioBuffer<float>& audio, double sampleRate)
{
    audioBuffer = audio;
    audioSampleRate = sampleRate;

    // Get BPM from DAW
    if (audioProcessor.isBPMAvailable())
    {
        bpm = audioProcessor.getCurrentBPM();
        bpmFromDAW = true;
    }
    else
    {
        bpm = Constants::Audio::DEFAULT_BPM;
        bpmFromDAW = false;
    }

    // Reset form state
    title = "";
    selectedKeyIndex = 0;
    selectedGenreIndex = 0;
    uploadState = UploadState::Editing;
    uploadProgress = 0.0f;
    errorMessage = "";
    activeField = 0; // Focus title field

    repaint();
}

void Upload::reset()
{
    audioBuffer.setSize(0, 0);
    title = "";
    bpm = 0.0;
    bpmFromDAW = false;
    selectedKeyIndex = 0;
    selectedGenreIndex = 0;
    uploadState = UploadState::Editing;
    uploadProgress = 0.0f;
    errorMessage = "";
    activeField = -1;
    tapTimes.clear();
    Log::debug("Upload::reset: All state cleared");

    repaint();
}

//==============================================================================
void Upload::timerCallback()
{
    // Update BPM from DAW if we're still editing and it changes
    if (uploadState == UploadState::Editing && bpmFromDAW && audioProcessor.isBPMAvailable())
    {
        double newBpm = audioProcessor.getCurrentBPM();
        if (std::abs(newBpm - bpm) > 0.1)
        {
            Log::debug("Upload::timerCallback: BPM updated from DAW: " + juce::String(bpm, 1) + " -> " + juce::String(newBpm, 1));
            bpm = newBpm;
            repaint();
        }
    }

    // Animate upload progress (simulate for now)
    if (uploadState == UploadState::Uploading)
    {
        // In real implementation, this would be updated by network callback
        repaint();
    }
}

//==============================================================================
void Upload::paint(juce::Graphics& g)
{
    // Dark background with subtle gradient
    auto bounds = getLocalBounds();
    g.setGradientFill(SidechainColors::backgroundGradient(
        bounds.getTopLeft().toFloat(),
        bounds.getBottomLeft().toFloat()));
    g.fillRect(bounds);

    // Draw all sections
    drawHeader(g);
    drawWaveform(g);
    drawTitleField(g);
    drawBPMField(g);
    drawTapTempoButton(g);
    drawKeyDropdown(g);
    drawDetectKeyButton(g);
    drawGenreDropdown(g);

    if (uploadState == UploadState::Uploading)
    {
        drawProgressBar(g);
    }

    drawButtons(g);
    drawStatus(g);
}

void Upload::resized()
{
    Log::debug("Upload::resized: Component resized to " + juce::String(getWidth()) + "x" + juce::String(getHeight()));
    auto bounds = getLocalBounds().reduced(24);
    int rowHeight = 48;
    int fieldSpacing = 16;

    // Header
    headerArea = bounds.removeFromTop(40);
    bounds.removeFromTop(fieldSpacing);

    // Waveform preview
    waveformArea = bounds.removeFromTop(100);
    bounds.removeFromTop(fieldSpacing);

    // Title field (full width)
    titleFieldArea = bounds.removeFromTop(rowHeight);
    bounds.removeFromTop(fieldSpacing);

    // BPM field + Tap tempo button (side by side)
    auto bpmRow = bounds.removeFromTop(rowHeight);
    bpmFieldArea = bpmRow.removeFromLeft(bpmRow.getWidth() / 2 - 8);
    bpmRow.removeFromLeft(16);
    tapTempoButtonArea = bpmRow;
    bounds.removeFromTop(fieldSpacing);

    // Key dropdown + Detect button (left side), Genre dropdown (right side)
    auto dropdownRow = bounds.removeFromTop(rowHeight);
    auto leftHalf = dropdownRow.removeFromLeft(dropdownRow.getWidth() / 2 - 8);
    keyDropdownArea = leftHalf.removeFromLeft(leftHalf.getWidth() - 80);  // Leave room for detect button
    leftHalf.removeFromLeft(8);
    detectKeyButtonArea = leftHalf;  // Remaining space for detect button
    dropdownRow.removeFromLeft(16);
    genreDropdownArea = dropdownRow;
    bounds.removeFromTop(fieldSpacing);

    // Progress bar (only shown during upload)
    progressBarArea = bounds.removeFromTop(24);
    bounds.removeFromTop(fieldSpacing);

    // Status area
    statusArea = bounds.removeFromTop(24);
    bounds.removeFromTop(fieldSpacing);

    // Buttons at bottom
    auto buttonRow = bounds.removeFromBottom(52);
    int buttonWidth = (buttonRow.getWidth() - 16) / 2;
    cancelButtonArea = buttonRow.removeFromLeft(buttonWidth);
    buttonRow.removeFromLeft(16);
    shareButtonArea = buttonRow;
}

//==============================================================================
void Upload::mouseUp(const juce::MouseEvent& event)
{
    auto pos = event.getPosition();
    Log::debug("Upload::mouseUp: Mouse clicked at (" + juce::String(pos.x) + ", " + juce::String(pos.y) +
               "), state: " + juce::String(uploadState == UploadState::Editing ? "Editing" :
                                           uploadState == UploadState::Uploading ? "Uploading" :
                                           uploadState == UploadState::Success ? "Success" : "Error"));

    if (uploadState == UploadState::Editing)
    {
        // Title field
        if (titleFieldArea.contains(pos))
        {
            Log::info("Upload::mouseUp: Title field clicked");
            activeField = 0;
            grabKeyboardFocus();
            repaint();
            return;
        }

        // BPM field
        if (bpmFieldArea.contains(pos))
        {
            Log::info("Upload::mouseUp: BPM field clicked");
            activeField = 1;
            bpmFromDAW = false; // Manual override
            Log::debug("Upload::mouseUp: BPM manual override enabled");
            grabKeyboardFocus();
            repaint();
            return;
        }

        // Tap tempo
        if (tapTempoButtonArea.contains(pos))
        {
            Log::info("Upload::mouseUp: Tap tempo button clicked");
            handleTapTempo();
            return;
        }

        // Key dropdown
        if (keyDropdownArea.contains(pos))
        {
            Log::info("Upload::mouseUp: Key dropdown clicked");
            showKeyPicker();
            return;
        }

        // Detect key button
        if (detectKeyButtonArea.contains(pos))
        {
            Log::info("Upload::mouseUp: Detect key button clicked");
            detectKey();
            return;
        }

        // Genre dropdown
        if (genreDropdownArea.contains(pos))
        {
            Log::info("Upload::mouseUp: Genre dropdown clicked");
            showGenrePicker();
            return;
        }

        // Cancel button
        if (cancelButtonArea.contains(pos))
        {
            Log::info("Upload::mouseUp: Cancel button clicked");
            cancelUpload();
            return;
        }

        // Share button
        if (shareButtonArea.contains(pos))
        {
            Log::info("Upload::mouseUp: Share button clicked");
            startUpload();
            return;
        }

        // Clicked elsewhere - clear field focus
        Log::debug("Upload::mouseUp: Clicked outside fields, clearing focus");
        activeField = -1;
        repaint();
    }
    else if (uploadState == UploadState::Success || uploadState == UploadState::Error)
    {
        // Any click dismisses
        if (uploadState == UploadState::Success && onUploadComplete)
        {
            Log::info("Upload::mouseUp: Success state clicked, calling onUploadComplete");
            onUploadComplete();
        }
        else if (uploadState == UploadState::Error)
        {
            Log::info("Upload::mouseUp: Error state clicked, returning to Editing");
            uploadState = UploadState::Editing;
            repaint();
        }
    }
}

//==============================================================================
void Upload::drawHeader(juce::Graphics& g)
{
    g.setColour(SidechainColors::textPrimary());
    g.setFont(juce::Font(24.0f, juce::Font::bold));
    g.drawText("Share Your Loop", headerArea, juce::Justification::centredLeft);

    // Duration badge
    auto durationBadge = headerArea.removeFromRight(80);
    g.setFont(14.0f);
    g.setColour(SidechainColors::textMuted());
    g.drawText(formatDuration(), durationBadge, juce::Justification::centredRight);
}

void Upload::drawWaveform(juce::Graphics& g)
{
    // Background
    g.setColour(SidechainColors::waveformBackground());
    g.fillRoundedRectangle(waveformArea.toFloat(), 8.0f);

    if (audioBuffer.getNumSamples() == 0)
        return;

    // Draw waveform
    auto path = generateWaveformPath(waveformArea.reduced(12, 8));
    g.setColour(SidechainColors::waveform());
    g.strokePath(path, juce::PathStrokeType(2.0f));
}

void Upload::drawTitleField(juce::Graphics& g)
{
    drawTextField(g, titleFieldArea, "Title", title, activeField == 0);
}

void Upload::drawBPMField(juce::Graphics& g)
{
    juce::String bpmText = bpm > 0 ? juce::String(bpm, 1) : "";
    juce::String label = bpmFromDAW ? "BPM (from DAW)" : "BPM";
    drawTextField(g, bpmFieldArea, label, bpmText, activeField == 1);
}

void Upload::drawTapTempoButton(juce::Graphics& g)
{
    bool isHovered = tapTempoButtonArea.contains(getMouseXYRelative());
    auto bgColor = isHovered ? SidechainColors::surfaceHover() : SidechainColors::surface();

    g.setColour(bgColor);
    g.fillRoundedRectangle(tapTempoButtonArea.toFloat(), 8.0f);

    // Border
    g.setColour(SidechainColors::border());
    g.drawRoundedRectangle(tapTempoButtonArea.toFloat(), 8.0f, 1.0f);

    // Text
    g.setColour(SidechainColors::textPrimary());
    g.setFont(14.0f);
    g.drawText("Tap Tempo", tapTempoButtonArea, juce::Justification::centred);
}

void Upload::drawKeyDropdown(juce::Graphics& g)
{
    auto& keys = getMusicalKeys();
    juce::String value = selectedKeyIndex < (int)keys.size() ? keys[selectedKeyIndex].name : "Not set";
    bool isHovered = keyDropdownArea.contains(getMouseXYRelative());
    drawDropdown(g, keyDropdownArea, "Key", value, isHovered);
}

void Upload::drawDetectKeyButton(juce::Graphics& g)
{
    bool isHovered = detectKeyButtonArea.contains(getMouseXYRelative());
    bool isEnabled = KeyDetector::isAvailable() && audioBuffer.getNumSamples() > 0 && !isDetectingKey;

    auto bgColor = isEnabled
        ? (isHovered ? SidechainColors::surfaceHover() : SidechainColors::surface())
        : SidechainColors::backgroundLight();

    g.setColour(bgColor);
    g.fillRoundedRectangle(detectKeyButtonArea.toFloat(), 8.0f);

    // Border
    auto borderColor = isEnabled ? SidechainColors::border() : SidechainColors::borderSubtle();
    g.setColour(borderColor);
    g.drawRoundedRectangle(detectKeyButtonArea.toFloat(), 8.0f, 1.0f);

    // Text
    g.setColour(isEnabled ? SidechainColors::textPrimary() : SidechainColors::textMuted());
    g.setFont(12.0f);

    juce::String buttonText = isDetectingKey ? "..." : "Detect";
    g.drawText(buttonText, detectKeyButtonArea, juce::Justification::centred);
}

void Upload::drawGenreDropdown(juce::Graphics& g)
{
    auto& genres = getGenres();
    juce::String value = selectedGenreIndex < (int)genres.size() ? genres[selectedGenreIndex] : "Electronic";
    bool isHovered = genreDropdownArea.contains(getMouseXYRelative());
    drawDropdown(g, genreDropdownArea, "Genre", value, isHovered);
}

void Upload::drawProgressBar(juce::Graphics& g)
{
    // Background
    g.setColour(SidechainColors::backgroundLight());
    g.fillRoundedRectangle(progressBarArea.toFloat(), 4.0f);

    // Progress fill
    if (uploadProgress > 0.0f)
    {
        auto fillWidth = progressBarArea.getWidth() * uploadProgress;
        auto fillRect = progressBarArea.withWidth(static_cast<int>(fillWidth));
        g.setColour(SidechainColors::primary());
        g.fillRoundedRectangle(fillRect.toFloat(), 4.0f);
    }
}

void Upload::drawButtons(juce::Graphics& g)
{
    bool cancelHovered = cancelButtonArea.contains(getMouseXYRelative());
    bool shareHovered = shareButtonArea.contains(getMouseXYRelative());
    bool canShare = !title.isEmpty() && audioBuffer.getNumSamples() > 0;

    if (uploadState == UploadState::Uploading)
    {
        // Show cancel only during upload
        drawButton(g, cancelButtonArea, "Cancel", SidechainColors::buttonSecondary(), cancelHovered, true);
        // Share button disabled during upload
        drawButton(g, shareButtonArea, "Uploading...", SidechainColors::primary().darker(0.2f), false, false);
    }
    else
    {
        drawButton(g, cancelButtonArea, "Cancel", SidechainColors::buttonSecondary(), cancelHovered, true);
        drawButton(g, shareButtonArea, "Share Loop", SidechainColors::primary(), shareHovered, canShare);
    }
}

void Upload::drawStatus(juce::Graphics& g)
{
    if (uploadState == UploadState::Error && !errorMessage.isEmpty())
    {
        g.setColour(SidechainColors::error());
        g.setFont(14.0f);
        g.drawText(errorMessage, statusArea, juce::Justification::centred);
    }
    else if (uploadState == UploadState::Success)
    {
        // Success icon and title
        g.setColour(SidechainColors::success());
        g.setFont(juce::Font(16.0f, juce::Font::bold));
        g.drawText(juce::CharPointer_UTF8("\xE2\x9C\x93 Loop shared!"), statusArea, juce::Justification::centred);  // checkmark

        // Show post details below
        auto detailsArea = statusArea.translated(0, 24);
        g.setColour(SidechainColors::textSecondary());
        g.setFont(12.0f);

        juce::String details = "\"" + lastUploadedTitle + "\"";
        if (!lastUploadedGenre.isEmpty())
            details += " · " + lastUploadedGenre;
        if (lastUploadedBpm > 0)
            details += " · " + StringFormatter::formatBPM(lastUploadedBpm);

        g.drawText(details, detailsArea, juce::Justification::centred);
    }
    else if (uploadState == UploadState::Uploading)
    {
        g.setColour(SidechainColors::primary());
        g.setFont(14.0f);
        g.drawText("Uploading... " + StringFormatter::formatPercentage(uploadProgress), statusArea, juce::Justification::centred);
    }
    else if (title.isEmpty() && activeField != 0)
    {
        g.setColour(SidechainColors::textMuted());
        g.setFont(12.0f);
        g.drawText("Give your loop a title to share", statusArea, juce::Justification::centred);
    }
}

//==============================================================================
void Upload::drawTextField(juce::Graphics& g, juce::Rectangle<int> bounds,
                                     const juce::String& label, const juce::String& value,
                                     bool isActive, bool isEditable)
{
    // Background
    auto bgColor = isActive ? SidechainColors::surfaceHover() : SidechainColors::surface();
    g.setColour(bgColor);
    g.fillRoundedRectangle(bounds.toFloat(), 8.0f);

    // Border
    auto borderColor = isActive ? SidechainColors::borderActive() : SidechainColors::border();
    g.setColour(borderColor);
    g.drawRoundedRectangle(bounds.toFloat(), 8.0f, isActive ? 2.0f : 1.0f);

    auto innerBounds = bounds.reduced(16, 0);

    // Label (top-left, smaller)
    g.setColour(SidechainColors::textMuted());
    g.setFont(11.0f);
    auto labelBounds = innerBounds.removeFromTop(20).withTrimmedTop(6);
    g.drawText(label, labelBounds, juce::Justification::centredLeft);

    // Value
    g.setColour(SidechainColors::textPrimary());
    g.setFont(16.0f);
    auto valueBounds = innerBounds.withTrimmedBottom(8);

    if (value.isEmpty() && isActive)
    {
        g.setColour(SidechainColors::textMuted());
        g.drawText("Enter " + label.toLowerCase() + "...", valueBounds, juce::Justification::centredLeft);
    }
    else
    {
        g.drawText(value + (isActive ? "|" : ""), valueBounds, juce::Justification::centredLeft);
    }
}

void Upload::drawDropdown(juce::Graphics& g, juce::Rectangle<int> bounds,
                                    const juce::String& label, const juce::String& value,
                                    bool isHovered)
{
    auto bgColor = isHovered ? SidechainColors::surfaceHover() : SidechainColors::surface();
    g.setColour(bgColor);
    g.fillRoundedRectangle(bounds.toFloat(), 8.0f);

    // Border
    g.setColour(SidechainColors::border());
    g.drawRoundedRectangle(bounds.toFloat(), 8.0f, 1.0f);

    auto innerBounds = bounds.reduced(16, 0);

    // Label
    g.setColour(SidechainColors::textMuted());
    g.setFont(11.0f);
    auto labelBounds = innerBounds.removeFromTop(20).withTrimmedTop(6);
    g.drawText(label, labelBounds, juce::Justification::centredLeft);

    // Value
    g.setColour(SidechainColors::textPrimary());
    g.setFont(16.0f);
    auto valueBounds = innerBounds.withTrimmedBottom(8);
    g.drawText(value, valueBounds, juce::Justification::centredLeft);

    // Dropdown arrow
    auto arrowArea = bounds.removeFromRight(40);
    g.setColour(SidechainColors::textMuted());
    juce::Path arrow;
    float cx = arrowArea.getCentreX();
    float cy = arrowArea.getCentreY();
    arrow.addTriangle(cx - 6, cy - 3, cx + 6, cy - 3, cx, cy + 4);
    g.fillPath(arrow);
}

void Upload::drawButton(juce::Graphics& g, juce::Rectangle<int> bounds,
                                  const juce::String& text, juce::Colour bgColor,
                                  bool isHovered, bool isEnabled)
{
    auto color = isEnabled ? (isHovered ? bgColor.brighter(0.15f) : bgColor) : bgColor.withAlpha(0.5f);
    g.setColour(color);
    g.fillRoundedRectangle(bounds.toFloat(), 10.0f);

    g.setColour(isEnabled ? SidechainColors::textPrimary() : SidechainColors::textPrimary().withAlpha(0.5f));
    g.setFont(juce::Font(16.0f, juce::Font::bold));
    g.drawText(text, bounds, juce::Justification::centred);
}

juce::Path Upload::generateWaveformPath(juce::Rectangle<int> bounds)
{
    juce::Path path;

    if (audioBuffer.getNumSamples() == 0)
        return path;

    int numSamples = audioBuffer.getNumSamples();
    int width = bounds.getWidth();
    float height = static_cast<float>(bounds.getHeight());
    float centerY = bounds.getCentreY();

    path.startNewSubPath(bounds.getX(), centerY);

    for (int x = 0; x < width; ++x)
    {
        int startSample = x * numSamples / width;
        int endSample = juce::jmin((x + 1) * numSamples / width, numSamples);

        float peak = 0.0f;
        for (int i = startSample; i < endSample; ++i)
        {
            for (int ch = 0; ch < audioBuffer.getNumChannels(); ++ch)
            {
                peak = juce::jmax(peak, std::abs(audioBuffer.getSample(ch, i)));
            }
        }

        float y = centerY - (peak * height * 0.45f);
        path.lineTo(bounds.getX() + x, y);
    }

    return path;
}

juce::String Upload::formatDuration() const
{
    if (audioBuffer.getNumSamples() == 0 || audioSampleRate <= 0)
        return "0:00";

    double seconds = static_cast<double>(audioBuffer.getNumSamples()) / audioSampleRate;
    return StringFormatter::formatDuration(seconds);
}

//==============================================================================
void Upload::handleTapTempo()
{
    double now = juce::Time::getMillisecondCounterHiRes();

    // Reset if more than 2 seconds since last tap
    if (now - lastTapTime > 2000.0)
    {
        tapTimes.clear();
    }

    tapTimes.push_back(now);
    lastTapTime = now;

    // Need at least 2 taps to calculate BPM
    if (tapTimes.size() >= 2)
    {
        // Calculate average interval
        double totalInterval = 0.0;
        for (size_t i = 1; i < tapTimes.size(); ++i)
        {
            totalInterval += tapTimes[i] - tapTimes[i - 1];
        }
        double avgInterval = totalInterval / (tapTimes.size() - 1);

        // Convert to BPM (ms to beats per minute)
        if (avgInterval > 0)
        {
            bpm = 60000.0 / avgInterval;
            bpmFromDAW = false;
            repaint();
        }
    }

    // Keep only last 8 taps
    if (tapTimes.size() > 8)
    {
        tapTimes.erase(tapTimes.begin());
    }
}

void Upload::detectKey()
{
    Log::info("Upload::detectKey: Starting key detection");

    if (!KeyDetector::isAvailable())
    {
        Log::warn("Upload::detectKey: Key detection not available");
        keyDetectionStatus = "Key detection not available";
        repaint();
        return;
    }

    if (audioBuffer.getNumSamples() == 0)
    {
        Log::warn("Upload::detectKey: No audio to analyze");
        keyDetectionStatus = "No audio to analyze";
        repaint();
        return;
    }

    if (isDetectingKey)
    {
        Log::debug("Upload::detectKey: Key detection already in progress");
        return;
    }

    isDetectingKey = true;
    keyDetectionStatus = "Analyzing...";
    Log::debug("Upload::detectKey: Starting analysis - samples: " + juce::String(audioBuffer.getNumSamples()) +
               ", sampleRate: " + juce::String(audioSampleRate, 1) + "Hz");
    repaint();

    // Run detection on background thread to avoid UI blocking
    Async::runVoid([this]() {
        Log::debug("Upload::detectKey: Running key detection on background thread");
        auto detectedKey = keyDetector.detectKey(
            audioBuffer,
            audioSampleRate,
            audioBuffer.getNumChannels());

        Log::debug("Upload::detectKey: Detection complete - valid: " + juce::String(detectedKey.isValid() ? "yes" : "no") +
                   (detectedKey.isValid() ? (", name: " + detectedKey.name + ", Camelot: " + detectedKey.camelot +
                    ", confidence: " + juce::String(detectedKey.confidence, 2)) : ""));

        // Map detected key to our key index
        int keyIndex = 0;  // "Not set"
        if (detectedKey.isValid())
        {
            // Try to find matching key in our list
            auto& keys = getMusicalKeys();
            for (int i = 1; i < (int)keys.size(); ++i)
            {
                // Match by short name (Am, F#, etc.)
                if (keys[i].shortName.equalsIgnoreCase(detectedKey.shortName))
                {
                    keyIndex = i;
                    Log::debug("Upload::detectKey: Matched key by shortName: " + keys[i].name);
                    break;
                }
                // Also try matching by standard name prefix
                if (detectedKey.name.containsIgnoreCase(keys[i].shortName.replace("m", " Minor").replace("#", "# /")))
                {
                    keyIndex = i;
                    Log::debug("Upload::detectKey: Matched key by name prefix: " + keys[i].name);
                    break;
                }
            }
        }

        // Update UI on message thread
        juce::MessageManager::callAsync([this, keyIndex, detectedKey]() {
            isDetectingKey = false;

            if (detectedKey.isValid())
            {
                selectedKeyIndex = keyIndex;
                keyDetectionStatus = "Detected: " + detectedKey.name;
                if (detectedKey.confidence > 0.0f)
                {
                    keyDetectionStatus += " (" + StringFormatter::formatConfidence(detectedKey.confidence) + " confidence)";
                }
                Log::info("Upload::detectKey: Key detected: " + detectedKey.name + " (Camelot: " + detectedKey.camelot +
                          "), confidence: " + juce::String(detectedKey.confidence, 2) + ", mapped to index: " + juce::String(keyIndex));
            }
            else
            {
                keyDetectionStatus = "Could not detect key";
                Log::warn("Upload::detectKey: Could not detect key");
            }
            repaint();

            // Clear status after 3 seconds
            juce::Timer::callAfterDelay(3000, [this]() {
                keyDetectionStatus = "";
                repaint();
            });
        });
    });
}

void Upload::showKeyPicker()
{
    Log::debug("Upload::showKeyPicker: Showing key picker menu");
    juce::PopupMenu menu;
    auto& keys = getMusicalKeys();

    for (int i = 0; i < (int)keys.size(); ++i)
    {
        menu.addItem(i + 1, keys[i].name, true, i == selectedKeyIndex);
    }

    menu.showMenuAsync(juce::PopupMenu::Options()
        .withTargetComponent(this)
        .withTargetScreenArea(keyDropdownArea.translated(getScreenX(), getScreenY())),
        [this](int result) {
            if (result > 0)
            {
                auto& keys = getMusicalKeys();
                int newIndex = result - 1;
                Log::info("Upload::showKeyPicker: Key selected: " + keys[newIndex].name + " (index: " + juce::String(newIndex) + ")");
                selectedKeyIndex = newIndex;
                repaint();
            }
        });
}

void Upload::showGenrePicker()
{
    Log::debug("Upload::showGenrePicker: Showing genre picker menu");
    juce::PopupMenu menu;
    auto& genres = getGenres();

    for (int i = 0; i < (int)genres.size(); ++i)
    {
        menu.addItem(i + 1, genres[i], true, i == selectedGenreIndex);
    }

    menu.showMenuAsync(juce::PopupMenu::Options()
        .withTargetComponent(this)
        .withTargetScreenArea(genreDropdownArea.translated(getScreenX(), getScreenY())),
        [this](int result) {
            if (result > 0)
            {
                auto& genres = getGenres();
                int newIndex = result - 1;
                Log::info("Upload::showGenrePicker: Genre selected: " + genres[newIndex] + " (index: " + juce::String(newIndex) + ")");
                selectedGenreIndex = newIndex;
                repaint();
            }
        });
}

void Upload::cancelUpload()
{
    Log::info("Upload::cancelUpload: Upload cancelled by user");
    if (onCancel)
    {
        Log::debug("Upload::cancelUpload: Calling onCancel callback");
        onCancel();
    }
    else
    {
        Log::warn("Upload::cancelUpload: onCancel callback not set");
    }
}

void Upload::startUpload()
{
    Log::info("Upload::startUpload: Starting upload process");

    if (title.isEmpty())
    {
        Log::warn("Upload::startUpload: Validation failed - title is empty");
        errorMessage = "Please enter a title";
        uploadState = UploadState::Error;
        repaint();
        return;
    }

    if (audioBuffer.getNumSamples() == 0)
    {
        Log::warn("Upload::startUpload: Validation failed - no audio to upload");
        errorMessage = "No audio to upload";
        uploadState = UploadState::Error;
        repaint();
        return;
    }

    uploadState = UploadState::Uploading;
    uploadProgress = 0.1f;  // Show initial progress
    errorMessage = "";
    Log::debug("Upload::startUpload: State changed to Uploading, progress: 10%");
    repaint();

    // Build metadata struct for uploadAudioWithMetadata
    auto& keys = getMusicalKeys();
    auto& genres = getGenres();

    NetworkClient::AudioUploadMetadata metadata;
    metadata.title = title;
    metadata.bpm = bpm;
    metadata.key = selectedKeyIndex > 0 && selectedKeyIndex < (int)keys.size()
        ? keys[selectedKeyIndex].shortName : "";
    metadata.genre = selectedGenreIndex < (int)genres.size()
        ? genres[selectedGenreIndex] : "";
    metadata.durationSeconds = static_cast<double>(audioBuffer.getNumSamples()) / audioSampleRate;
    metadata.sampleRate = static_cast<int>(audioSampleRate);
    metadata.numChannels = audioBuffer.getNumChannels();

    Log::info("Upload::startUpload: Upload metadata - title: \"" + title + "\", BPM: " + juce::String(bpm, 1) +
              ", key: " + metadata.key + ", genre: " + metadata.genre + ", duration: " +
              juce::String(metadata.durationSeconds, 2) + "s, sampleRate: " + juce::String(metadata.sampleRate) +
              "Hz, channels: " + juce::String(metadata.numChannels));

    // Simulate progress updates while waiting for upload
    // (JUCE's URL class doesn't provide progress callbacks)
    juce::Timer::callAfterDelay(500, [this]() {
        if (uploadState == UploadState::Uploading)
        {
            uploadProgress = 0.3f;
            Log::debug("Upload::startUpload: Progress update: 30%");
            repaint();
        }
    });
    juce::Timer::callAfterDelay(1000, [this]() {
        if (uploadState == UploadState::Uploading)
        {
            uploadProgress = 0.6f;
            Log::debug("Upload::startUpload: Progress update: 60%");
            repaint();
        }
    });

    // Start async upload with full metadata
    Log::info("Upload::startUpload: Calling networkClient.uploadAudioWithMetadata");
    networkClient.uploadAudioWithMetadata(audioBuffer, audioSampleRate, metadata,
        [this, savedTitle = title, savedGenre = metadata.genre, savedBpm = bpm](Outcome<juce::String> uploadResult) {
            juce::MessageManager::callAsync([this, uploadResult, savedTitle, savedGenre, savedBpm]() {
                if (uploadResult.isOk())
                {
                    auto audioUrl = uploadResult.getValue();
                    uploadState = UploadState::Success;
                    uploadProgress = 1.0f;
                    lastUploadedTitle = savedTitle;
                    lastUploadedGenre = savedGenre;
                    lastUploadedBpm = savedBpm;
                    lastUploadedUrl = audioUrl;
                    Log::info("Upload::startUpload: Upload successful - URL: " + audioUrl);
                    Log::info("Upload::startUpload: Upload details - Title: \"" + savedTitle +
                             "\", Genre: " + savedGenre + ", BPM: " + juce::String(savedBpm, 1));

                    // Auto-dismiss after 3 seconds (longer to show success preview)
                    juce::Timer::callAfterDelay(3000, [this]() {
                        if (uploadState == UploadState::Success && onUploadComplete)
                        {
                            Log::debug("Upload::startUpload: Auto-dismissing success state, calling onUploadComplete");
                            onUploadComplete();
                        }
                    });
                }
                else
                {
                    uploadState = UploadState::Error;
                    errorMessage = "Upload failed: " + uploadResult.getError();
                    uploadProgress = 0.0f;
                    Log::error("Upload::startUpload: Upload failed");
                }
                repaint();
            });
        });
}

bool Upload::keyPressed(const juce::KeyPress& key)
{
    if (activeField < 0)
        return false;

    // Handle special keys
    if (key == juce::KeyPress::escapeKey)
    {
        Log::debug("Upload::keyPressed: Escape key pressed, clearing field focus");
        activeField = -1;
        repaint();
        return true;
    }

    if (key == juce::KeyPress::tabKey)
    {
        int newField = (activeField + 1) % 2;
        Log::debug("Upload::keyPressed: Tab key pressed, switching field: " + juce::String(activeField) + " -> " + juce::String(newField));
        activeField = newField;
        repaint();
        return true;
    }

    if (key == juce::KeyPress::returnKey)
    {
        Log::debug("Upload::keyPressed: Return key pressed, clearing field focus");
        activeField = -1;
        repaint();
        return true;
    }

    if (activeField == 0) // Title field
    {
        if (key == juce::KeyPress::backspaceKey)
        {
            if (!title.isEmpty())
            {
                title = title.dropLastCharacters(1);
                Log::debug("Upload::keyPressed: Backspace in title field, new length: " + juce::String(title.length()));
                repaint();
            }
            return true;
        }

        // Handle text input - get the character from the key
        juce::juce_wchar character = key.getTextCharacter();
        if (character >= 32 && character < 127) // Printable ASCII
        {
            if (title.length() < 100) // Max title length
            {
                title += juce::String::charToString(character);
                Log::debug("Upload::keyPressed: Character added to title, new length: " + juce::String(title.length()));
                repaint();
            }
            else
            {
                Log::debug("Upload::keyPressed: Title max length (100) reached");
            }
            return true;
        }
    }
    else if (activeField == 1) // BPM field
    {
        if (key == juce::KeyPress::backspaceKey)
        {
            juce::String bpmStr = bpm > 0 ? juce::String(bpm, 1) : "";
            if (!bpmStr.isEmpty())
            {
                bpmStr = bpmStr.dropLastCharacters(1);
                bpm = bpmStr.isEmpty() ? 0.0 : bpmStr.getDoubleValue();
                bpmFromDAW = false;
                Log::debug("Upload::keyPressed: Backspace in BPM field, new BPM: " + juce::String(bpm, 1));
                repaint();
            }
            return true;
        }

        // Handle numeric input
        juce::juce_wchar character = key.getTextCharacter();
        if ((character >= '0' && character <= '9') || character == '.')
        {
            juce::String bpmStr = bpm > 0 ? juce::String(bpm, 1) : "";
            bpmStr += juce::String::charToString(character);
            double newBpm = bpmStr.getDoubleValue();
            if (newBpm <= Constants::Audio::MAX_BPM)
            {
                bpm = newBpm;
                bpmFromDAW = false;
                Log::debug("Upload::keyPressed: BPM updated: " + juce::String(bpm, 1));
                repaint();
            }
            else
            {
                Log::debug("Upload::keyPressed: BPM exceeds max (" + juce::String(Constants::Audio::MAX_BPM) + ")");
            }
            return true;
        }
    }

    return false;
}

void Upload::focusGained(FocusChangeType /*cause*/)
{
    // When component gains focus, activate title field if nothing is active
    if (activeField < 0)
    {
        Log::debug("Upload::focusGained: Component gained focus, activating title field");
        activeField = 0;
        repaint();
    }
}
