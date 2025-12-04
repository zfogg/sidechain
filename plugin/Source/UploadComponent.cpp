#include "UploadComponent.h"
#include "PluginProcessor.h"

//==============================================================================
// Static data: Musical keys (Camelot wheel order is producer-friendly)
const std::array<UploadComponent::MusicalKey, UploadComponent::NUM_KEYS>& UploadComponent::getMusicalKeys()
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
const std::array<juce::String, UploadComponent::NUM_GENRES>& UploadComponent::getGenres()
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
UploadComponent::UploadComponent(SidechainAudioProcessor& processor, NetworkClient& network)
    : audioProcessor(processor), networkClient(network)
{
    setWantsKeyboardFocus(true);
    startTimerHz(30);
}

UploadComponent::~UploadComponent()
{
    stopTimer();
}

//==============================================================================
void UploadComponent::setAudioToUpload(const juce::AudioBuffer<float>& audio, double sampleRate)
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
        bpm = 120.0; // Default
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

void UploadComponent::reset()
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

    repaint();
}

//==============================================================================
void UploadComponent::timerCallback()
{
    // Update BPM from DAW if we're still editing and it changes
    if (uploadState == UploadState::Editing && bpmFromDAW && audioProcessor.isBPMAvailable())
    {
        double newBpm = audioProcessor.getCurrentBPM();
        if (std::abs(newBpm - bpm) > 0.1)
        {
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
void UploadComponent::paint(juce::Graphics& g)
{
    // Dark background with subtle gradient
    auto bounds = getLocalBounds();
    juce::ColourGradient gradient(
        juce::Colour::fromRGB(28, 28, 32), bounds.getTopLeft().toFloat(),
        juce::Colour::fromRGB(22, 22, 26), bounds.getBottomLeft().toFloat(),
        false);
    g.setGradientFill(gradient);
    g.fillRect(bounds);

    // Draw all sections
    drawHeader(g);
    drawWaveform(g);
    drawTitleField(g);
    drawBPMField(g);
    drawTapTempoButton(g);
    drawKeyDropdown(g);
    drawGenreDropdown(g);

    if (uploadState == UploadState::Uploading)
    {
        drawProgressBar(g);
    }

    drawButtons(g);
    drawStatus(g);
}

void UploadComponent::resized()
{
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

    // Key + Genre dropdowns (side by side)
    auto dropdownRow = bounds.removeFromTop(rowHeight);
    keyDropdownArea = dropdownRow.removeFromLeft(dropdownRow.getWidth() / 2 - 8);
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
void UploadComponent::mouseUp(const juce::MouseEvent& event)
{
    auto pos = event.getPosition();

    if (uploadState == UploadState::Editing)
    {
        // Title field
        if (titleFieldArea.contains(pos))
        {
            activeField = 0;
            repaint();
            return;
        }

        // BPM field
        if (bpmFieldArea.contains(pos))
        {
            activeField = 1;
            bpmFromDAW = false; // Manual override
            repaint();
            return;
        }

        // Tap tempo
        if (tapTempoButtonArea.contains(pos))
        {
            handleTapTempo();
            return;
        }

        // Key dropdown
        if (keyDropdownArea.contains(pos))
        {
            showKeyPicker();
            return;
        }

        // Genre dropdown
        if (genreDropdownArea.contains(pos))
        {
            showGenrePicker();
            return;
        }

        // Cancel button
        if (cancelButtonArea.contains(pos))
        {
            cancelUpload();
            return;
        }

        // Share button
        if (shareButtonArea.contains(pos))
        {
            startUpload();
            return;
        }

        // Clicked elsewhere - clear field focus
        activeField = -1;
        repaint();
    }
    else if (uploadState == UploadState::Success || uploadState == UploadState::Error)
    {
        // Any click dismisses
        if (uploadState == UploadState::Success && onUploadComplete)
        {
            onUploadComplete();
        }
        else if (uploadState == UploadState::Error)
        {
            uploadState = UploadState::Editing;
            repaint();
        }
    }
}

//==============================================================================
void UploadComponent::drawHeader(juce::Graphics& g)
{
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(24.0f, juce::Font::bold));
    g.drawText("Share Your Loop", headerArea, juce::Justification::centredLeft);

    // Duration badge
    auto durationBadge = headerArea.removeFromRight(80);
    g.setFont(14.0f);
    g.setColour(juce::Colour::fromRGB(100, 100, 110));
    g.drawText(formatDuration(), durationBadge, juce::Justification::centredRight);
}

void UploadComponent::drawWaveform(juce::Graphics& g)
{
    // Background
    g.setColour(juce::Colour::fromRGB(38, 38, 44));
    g.fillRoundedRectangle(waveformArea.toFloat(), 8.0f);

    if (audioBuffer.getNumSamples() == 0)
        return;

    // Draw waveform
    auto path = generateWaveformPath(waveformArea.reduced(12, 8));
    g.setColour(juce::Colour::fromRGB(0, 212, 255)); // Sidechain cyan
    g.strokePath(path, juce::PathStrokeType(2.0f));
}

void UploadComponent::drawTitleField(juce::Graphics& g)
{
    drawTextField(g, titleFieldArea, "Title", title, activeField == 0);
}

void UploadComponent::drawBPMField(juce::Graphics& g)
{
    juce::String bpmText = bpm > 0 ? juce::String(bpm, 1) : "";
    juce::String label = bpmFromDAW ? "BPM (from DAW)" : "BPM";
    drawTextField(g, bpmFieldArea, label, bpmText, activeField == 1);
}

void UploadComponent::drawTapTempoButton(juce::Graphics& g)
{
    bool isHovered = tapTempoButtonArea.contains(getMouseXYRelative());
    auto bgColor = isHovered ? juce::Colour::fromRGB(60, 60, 68) : juce::Colour::fromRGB(48, 48, 54);

    g.setColour(bgColor);
    g.fillRoundedRectangle(tapTempoButtonArea.toFloat(), 8.0f);

    // Border
    g.setColour(juce::Colour::fromRGB(70, 70, 80));
    g.drawRoundedRectangle(tapTempoButtonArea.toFloat(), 8.0f, 1.0f);

    // Text
    g.setColour(juce::Colours::white);
    g.setFont(14.0f);
    g.drawText("Tap Tempo", tapTempoButtonArea, juce::Justification::centred);
}

void UploadComponent::drawKeyDropdown(juce::Graphics& g)
{
    auto& keys = getMusicalKeys();
    juce::String value = selectedKeyIndex < (int)keys.size() ? keys[selectedKeyIndex].name : "Not set";
    bool isHovered = keyDropdownArea.contains(getMouseXYRelative());
    drawDropdown(g, keyDropdownArea, "Key", value, isHovered);
}

void UploadComponent::drawGenreDropdown(juce::Graphics& g)
{
    auto& genres = getGenres();
    juce::String value = selectedGenreIndex < (int)genres.size() ? genres[selectedGenreIndex] : "Electronic";
    bool isHovered = genreDropdownArea.contains(getMouseXYRelative());
    drawDropdown(g, genreDropdownArea, "Genre", value, isHovered);
}

void UploadComponent::drawProgressBar(juce::Graphics& g)
{
    // Background
    g.setColour(juce::Colour::fromRGB(38, 38, 44));
    g.fillRoundedRectangle(progressBarArea.toFloat(), 4.0f);

    // Progress fill
    if (uploadProgress > 0.0f)
    {
        auto fillWidth = progressBarArea.getWidth() * uploadProgress;
        auto fillRect = progressBarArea.withWidth(static_cast<int>(fillWidth));
        g.setColour(juce::Colour::fromRGB(0, 212, 255));
        g.fillRoundedRectangle(fillRect.toFloat(), 4.0f);
    }
}

void UploadComponent::drawButtons(juce::Graphics& g)
{
    bool cancelHovered = cancelButtonArea.contains(getMouseXYRelative());
    bool shareHovered = shareButtonArea.contains(getMouseXYRelative());
    bool canShare = !title.isEmpty() && audioBuffer.getNumSamples() > 0;

    if (uploadState == UploadState::Uploading)
    {
        // Show cancel only during upload
        drawButton(g, cancelButtonArea, "Cancel", juce::Colour::fromRGB(108, 117, 125), cancelHovered, true);
        // Share button disabled during upload
        drawButton(g, shareButtonArea, "Uploading...", juce::Colour::fromRGB(0, 150, 180), false, false);
    }
    else
    {
        drawButton(g, cancelButtonArea, "Cancel", juce::Colour::fromRGB(108, 117, 125), cancelHovered, true);
        drawButton(g, shareButtonArea, "Share Loop", juce::Colour::fromRGB(0, 212, 255), shareHovered, canShare);
    }
}

void UploadComponent::drawStatus(juce::Graphics& g)
{
    if (uploadState == UploadState::Error && !errorMessage.isEmpty())
    {
        g.setColour(juce::Colour::fromRGB(255, 82, 82));
        g.setFont(14.0f);
        g.drawText(errorMessage, statusArea, juce::Justification::centred);
    }
    else if (uploadState == UploadState::Success)
    {
        // Success icon and title
        g.setColour(juce::Colour::fromRGB(0, 212, 100));
        g.setFont(juce::Font(16.0f, juce::Font::bold));
        g.drawText(juce::CharPointer_UTF8("\xE2\x9C\x93 Loop shared!"), statusArea, juce::Justification::centred);  // checkmark

        // Show post details below
        auto detailsArea = statusArea.translated(0, 24);
        g.setColour(juce::Colour::fromRGB(160, 160, 170));
        g.setFont(12.0f);

        juce::String details = "\"" + lastUploadedTitle + "\"";
        if (!lastUploadedGenre.isEmpty())
            details += " · " + lastUploadedGenre;
        if (lastUploadedBpm > 0)
            details += " · " + juce::String(lastUploadedBpm, 0) + " BPM";

        g.drawText(details, detailsArea, juce::Justification::centred);
    }
    else if (uploadState == UploadState::Uploading)
    {
        g.setColour(juce::Colour::fromRGB(0, 212, 255));
        g.setFont(14.0f);
        int percent = static_cast<int>(uploadProgress * 100);
        g.drawText("Uploading... " + juce::String(percent) + "%", statusArea, juce::Justification::centred);
    }
    else if (title.isEmpty() && activeField != 0)
    {
        g.setColour(juce::Colour::fromRGB(150, 150, 160));
        g.setFont(12.0f);
        g.drawText("Give your loop a title to share", statusArea, juce::Justification::centred);
    }
}

//==============================================================================
void UploadComponent::drawTextField(juce::Graphics& g, juce::Rectangle<int> bounds,
                                     const juce::String& label, const juce::String& value,
                                     bool isActive, bool isEditable)
{
    // Background
    auto bgColor = isActive ? juce::Colour::fromRGB(48, 48, 56) : juce::Colour::fromRGB(38, 38, 44);
    g.setColour(bgColor);
    g.fillRoundedRectangle(bounds.toFloat(), 8.0f);

    // Border
    auto borderColor = isActive ? juce::Colour::fromRGB(0, 212, 255) : juce::Colour::fromRGB(60, 60, 70);
    g.setColour(borderColor);
    g.drawRoundedRectangle(bounds.toFloat(), 8.0f, isActive ? 2.0f : 1.0f);

    auto innerBounds = bounds.reduced(16, 0);

    // Label (top-left, smaller)
    g.setColour(juce::Colour::fromRGB(130, 130, 140));
    g.setFont(11.0f);
    auto labelBounds = innerBounds.removeFromTop(20).withTrimmedTop(6);
    g.drawText(label, labelBounds, juce::Justification::centredLeft);

    // Value
    g.setColour(juce::Colours::white);
    g.setFont(16.0f);
    auto valueBounds = innerBounds.withTrimmedBottom(8);

    if (value.isEmpty() && isActive)
    {
        g.setColour(juce::Colour::fromRGB(100, 100, 110));
        g.drawText("Enter " + label.toLowerCase() + "...", valueBounds, juce::Justification::centredLeft);
    }
    else
    {
        g.drawText(value + (isActive ? "|" : ""), valueBounds, juce::Justification::centredLeft);
    }
}

void UploadComponent::drawDropdown(juce::Graphics& g, juce::Rectangle<int> bounds,
                                    const juce::String& label, const juce::String& value,
                                    bool isHovered)
{
    auto bgColor = isHovered ? juce::Colour::fromRGB(48, 48, 56) : juce::Colour::fromRGB(38, 38, 44);
    g.setColour(bgColor);
    g.fillRoundedRectangle(bounds.toFloat(), 8.0f);

    // Border
    g.setColour(juce::Colour::fromRGB(60, 60, 70));
    g.drawRoundedRectangle(bounds.toFloat(), 8.0f, 1.0f);

    auto innerBounds = bounds.reduced(16, 0);

    // Label
    g.setColour(juce::Colour::fromRGB(130, 130, 140));
    g.setFont(11.0f);
    auto labelBounds = innerBounds.removeFromTop(20).withTrimmedTop(6);
    g.drawText(label, labelBounds, juce::Justification::centredLeft);

    // Value
    g.setColour(juce::Colours::white);
    g.setFont(16.0f);
    auto valueBounds = innerBounds.withTrimmedBottom(8);
    g.drawText(value, valueBounds, juce::Justification::centredLeft);

    // Dropdown arrow
    auto arrowArea = bounds.removeFromRight(40);
    g.setColour(juce::Colour::fromRGB(130, 130, 140));
    juce::Path arrow;
    float cx = arrowArea.getCentreX();
    float cy = arrowArea.getCentreY();
    arrow.addTriangle(cx - 6, cy - 3, cx + 6, cy - 3, cx, cy + 4);
    g.fillPath(arrow);
}

void UploadComponent::drawButton(juce::Graphics& g, juce::Rectangle<int> bounds,
                                  const juce::String& text, juce::Colour bgColor,
                                  bool isHovered, bool isEnabled)
{
    auto color = isEnabled ? (isHovered ? bgColor.brighter(0.15f) : bgColor) : bgColor.withAlpha(0.5f);
    g.setColour(color);
    g.fillRoundedRectangle(bounds.toFloat(), 10.0f);

    g.setColour(isEnabled ? juce::Colours::white : juce::Colours::white.withAlpha(0.5f));
    g.setFont(juce::Font(16.0f, juce::Font::bold));
    g.drawText(text, bounds, juce::Justification::centred);
}

juce::Path UploadComponent::generateWaveformPath(juce::Rectangle<int> bounds)
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

juce::String UploadComponent::formatDuration() const
{
    if (audioBuffer.getNumSamples() == 0 || audioSampleRate <= 0)
        return "0:00";

    double seconds = static_cast<double>(audioBuffer.getNumSamples()) / audioSampleRate;
    int mins = static_cast<int>(seconds) / 60;
    int secs = static_cast<int>(seconds) % 60;
    return juce::String::formatted("%d:%02d", mins, secs);
}

//==============================================================================
void UploadComponent::handleTapTempo()
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

void UploadComponent::showKeyPicker()
{
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
                selectedKeyIndex = result - 1;
                repaint();
            }
        });
}

void UploadComponent::showGenrePicker()
{
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
                selectedGenreIndex = result - 1;
                repaint();
            }
        });
}

void UploadComponent::cancelUpload()
{
    if (onCancel)
        onCancel();
}

void UploadComponent::startUpload()
{
    if (title.isEmpty())
    {
        errorMessage = "Please enter a title";
        uploadState = UploadState::Error;
        repaint();
        return;
    }

    if (audioBuffer.getNumSamples() == 0)
    {
        errorMessage = "No audio to upload";
        uploadState = UploadState::Error;
        repaint();
        return;
    }

    uploadState = UploadState::Uploading;
    uploadProgress = 0.1f;  // Show initial progress
    errorMessage = "";
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

    // Simulate progress updates while waiting for upload
    // (JUCE's URL class doesn't provide progress callbacks)
    juce::Timer::callAfterDelay(500, [this]() {
        if (uploadState == UploadState::Uploading)
        {
            uploadProgress = 0.3f;
            repaint();
        }
    });
    juce::Timer::callAfterDelay(1000, [this]() {
        if (uploadState == UploadState::Uploading)
        {
            uploadProgress = 0.6f;
            repaint();
        }
    });

    // Start async upload with full metadata
    networkClient.uploadAudioWithMetadata(audioBuffer, audioSampleRate, metadata,
        [this, savedTitle = title, savedGenre = metadata.genre, savedBpm = bpm](bool success, const juce::String& audioUrl) {
            juce::MessageManager::callAsync([this, success, audioUrl, savedTitle, savedGenre, savedBpm]() {
                if (success)
                {
                    uploadState = UploadState::Success;
                    uploadProgress = 1.0f;
                    lastUploadedTitle = savedTitle;
                    lastUploadedGenre = savedGenre;
                    lastUploadedBpm = savedBpm;
                    lastUploadedUrl = audioUrl;
                    DBG("Upload successful: " + audioUrl);
                    DBG("  Title: " + savedTitle + ", Genre: " + savedGenre + ", BPM: " + juce::String(savedBpm));

                    // Auto-dismiss after 3 seconds (longer to show success preview)
                    juce::Timer::callAfterDelay(3000, [this]() {
                        if (uploadState == UploadState::Success && onUploadComplete)
                            onUploadComplete();
                    });
                }
                else
                {
                    uploadState = UploadState::Error;
                    errorMessage = "Upload failed. Tap to try again.";
                    uploadProgress = 0.0f;
                }
                repaint();
            });
        });
}

void UploadComponent::handleKeyPress(const juce::KeyPress& key)
{
    if (activeField == 0) // Title
    {
        if (key == juce::KeyPress::backspaceKey && !title.isEmpty())
        {
            title = title.dropLastCharacters(1);
            repaint();
        }
        else if (key == juce::KeyPress::returnKey)
        {
            activeField = -1;
            repaint();
        }
    }
    else if (activeField == 1) // BPM
    {
        if (key == juce::KeyPress::backspaceKey)
        {
            juce::String bpmStr = juce::String(bpm, 1);
            if (!bpmStr.isEmpty())
            {
                bpmStr = bpmStr.dropLastCharacters(1);
                bpm = bpmStr.isEmpty() ? 0.0 : bpmStr.getDoubleValue();
                repaint();
            }
        }
        else if (key == juce::KeyPress::returnKey)
        {
            activeField = -1;
            repaint();
        }
    }

    if (key == juce::KeyPress::escapeKey)
    {
        activeField = -1;
        repaint();
    }
    else if (key == juce::KeyPress::tabKey)
    {
        activeField = (activeField + 1) % 2;
        repaint();
    }
}

void UploadComponent::handleTextInput(juce::juce_wchar character)
{
    if (activeField == 0 && character >= 32 && character < 127) // Title - printable ASCII
    {
        if (title.length() < 100) // Max title length
        {
            title += juce::String::charToString(character);
            repaint();
        }
    }
    else if (activeField == 1) // BPM - numeric only
    {
        if ((character >= '0' && character <= '9') || character == '.')
        {
            juce::String bpmStr = bpm > 0 ? juce::String(bpm, 1) : "";
            bpmStr += juce::String::charToString(character);
            double newBpm = bpmStr.getDoubleValue();
            if (newBpm <= 300.0) // Reasonable BPM limit
            {
                bpm = newBpm;
                bpmFromDAW = false;
                repaint();
            }
        }
    }
}
