#include "StoryViewer.h"
#include "../../network/NetworkClient.h"
#include "../../util/Log.h"
#include "../../util/Result.h"
#include "../../util/DAWProjectFolder.h"
#include "../../util/WaveformGenerator.h"
#include <JuceHeader.h>

namespace StoryViewerColors
{
    const juce::Colour background(0xff0a0a14);
    const juce::Colour surface(0xff1a1a2e);
    const juce::Colour headerBg(0x80000000);
    const juce::Colour progressBg(0x40ffffff);
    const juce::Colour progressFg(0xffffffff);
    const juce::Colour textPrimary(0xffffffff);
    const juce::Colour textSecondary(0xffb0b0b0);
    const juce::Colour waveformColor(0xff7c4dff);
    const juce::Colour playOverlay(0x80000000);
}

//==============================================================================
StoryViewer::StoryViewer()
{
    // Create piano roll component
    pianoRoll = std::make_unique<PianoRoll>();
    addChildComponent(pianoRoll.get());

    // Create audio player
    audioPlayer = std::make_unique<HttpAudioPlayer>();

    // Start timer for progress updates
    startTimerHz(30);

    // TODO: Phase 7.5.5.2.1 - Create note waterfall visualization (alternative to piano roll)
    // TODO: Phase 7.5.5.2.2 - Create circular visualization (alternative to piano roll)
    // TODO: Phase 7.5.5.2 - Toggle between piano roll and note list view (add note list view alternative)

    Log::info("StoryViewer created");
}

StoryViewer::~StoryViewer()
{
    stopTimer();
    if (audioPlayer)
        audioPlayer->stop();

    Log::info("StoryViewer destroyed");
}

//==============================================================================
void StoryViewer::paint(juce::Graphics& g)
{
    // Background
    g.fillAll(StoryViewerColors::background);

    // Check if current story is expired
    const auto* story = getCurrentStory();
    if (story && story->isExpired())
    {
        drawExpiredMessage(g);
        return;
    }

    // Draw components
    drawProgressBar(g);
    drawHeader(g);
    drawStoryContent(g);
    drawDeleteButton(g);  // Delete button for own stories
    drawMIDIButton(g);
    drawAudioDownloadButton(g);  // 19.1 Audio download
    drawRemixButton(g);  // R.3.2 Remix Chains

    // Play/pause overlay if paused
    if (!playing)
    {
        drawPlayPauseOverlay(g);
    }
}

void StoryViewer::resized()
{
    auto bounds = getLocalBounds();

    // Progress bar at top
    progressBarArea = bounds.removeFromTop(4);

    // Header area
    headerArea = bounds.removeFromTop(60);

    // Close button in header
    closeButtonArea = headerArea.removeFromRight(50).reduced(10);

    // Content area (rest of the space)
    contentArea = bounds;

    // Tap areas (left third = previous, right third = next)
    int tapWidth = contentArea.getWidth() / 3;
    leftTapArea = contentArea.withWidth(tapWidth);
    rightTapArea = contentArea.withX(contentArea.getRight() - tapWidth).withWidth(tapWidth);

    // Viewers and share buttons (bottom, only shown for story owner)
    auto bottomArea = contentArea.removeFromBottom(50);
    viewersButtonArea = bottomArea.removeFromRight(120).reduced(10, 5);
    shareButtonArea = bottomArea.removeFromRight(100).reduced(10, 5);
    deleteButtonArea = bottomArea.removeFromRight(80).reduced(10, 5);  // Delete button for own stories
    midiButtonArea = bottomArea.removeFromRight(80).reduced(10, 5);
    audioDownloadButtonArea = bottomArea.removeFromRight(80).reduced(10, 5);  // 19.1 Audio download
    remixButtonArea = bottomArea.removeFromRight(80).reduced(10, 5);  // R.3.2 Remix Chains

    // Piano roll positioning
    if (pianoRoll)
    {
        auto pianoRollBounds = contentArea.reduced(20);
        pianoRollBounds = pianoRollBounds.removeFromBottom(pianoRollBounds.getHeight() / 2);
        pianoRoll->setBounds(pianoRollBounds);
    }
}

void StoryViewer::mouseUp(const juce::MouseEvent& event)
{
    auto pos = event.getPosition();

    // Check for swipe completion
    if (isDragging)
    {
        int deltaX = pos.x - dragStartPoint.x;
        int deltaY = pos.y - dragStartPoint.y;

        if (std::abs(deltaX) > SWIPE_THRESHOLD && std::abs(deltaX) > std::abs(deltaY))
        {
            if (deltaX > 0)
                showPreviousStory();
            else
                showNextStory();
        }
        else if (deltaY > SWIPE_THRESHOLD)
        {
            closeViewer();
        }

        isDragging = false;
        return;
    }

    // Close button
    if (closeButtonArea.contains(pos))
    {
        closeViewer();
        return;
    }

    // Tap navigation
    if (leftTapArea.contains(pos))
    {
        showPreviousStory();
        return;
    }

    if (rightTapArea.contains(pos))
    {
        showNextStory();
        return;
    }

    // Viewers button (if story owner)
    const auto* story = getCurrentStory();
    if (story && story->userId == currentUserId)
    {
        if (viewersButtonArea.contains(pos))
        {
            if (onViewersClicked)
                onViewersClicked(story->id);
            return;
        }

        // Share button
        if (shareButtonArea.contains(pos))
        {
            handleShareStory(story->id);
            if (onShareClicked)
                onShareClicked(story->id);
            return;
        }

        // Delete button
        if (deleteButtonArea.contains(pos))
        {
            handleDeleteStory(story->id);
            return;
        }
    }

    // MIDI download button (available to all viewers when story has MIDI)
    if (story && story->hasDownloadableMIDI() && midiButtonArea.contains(pos))
    {
        handleDownloadMIDI(*story);
        if (onDownloadMIDIClicked)
            onDownloadMIDIClicked(*story);
        return;
    }

    // Audio download button (19.1 - available to all viewers when story has audio)
    if (story && story->audioUrl.isNotEmpty() && audioDownloadButtonArea.contains(pos))
    {
        handleDownloadAudio(*story);
        if (onDownloadAudioClicked)
            onDownloadAudioClicked(*story);
        return;
    }

    // Remix button (R.3.2 Remix Chains) - available for non-expired stories
    if (story && !story->isExpired() && remixButtonArea.contains(pos))
    {
        if (onRemixClicked)
        {
            // Determine remix type based on what's available
            juce::String remixType = "audio";
            if (story->hasDownloadableMIDI() && story->audioUrl.isNotEmpty())
                remixType = "both";
            else if (story->hasDownloadableMIDI())
                remixType = "midi";

            // If both are available, show a menu
            if (remixType == "both")
            {
                juce::PopupMenu menu;
                menu.addItem(1, "Remix Audio Only");
                menu.addItem(2, "Remix MIDI Only");
                menu.addItem(3, "Remix Both Audio & MIDI");

                menu.showMenuAsync(juce::PopupMenu::Options(), [this, story](int result) {
                    if (result == 0) return;  // Cancelled

                    juce::String selectedType = (result == 1) ? "audio" : (result == 2) ? "midi" : "both";
                    if (onRemixClicked)
                        onRemixClicked(story->id, selectedType);
                });
            }
            else
            {
                onRemixClicked(story->id, remixType);
            }
        }
        return;
    }

    // Center tap = play/pause
    togglePlayPause();
}

void StoryViewer::mouseDown(const juce::MouseEvent& event)
{
    dragStartPoint = event.getPosition();
    isDragging = true;
}

void StoryViewer::mouseDrag(const juce::MouseEvent& /*event*/)
{
    // Visual feedback during drag could be added here
}

//==============================================================================
void StoryViewer::timerCallback()
{
    if (playing && audioPlayer)
    {
        playbackPosition = audioPlayer->getPositionSeconds();
        updateProgress();

        // Check if story finished
        if (playbackPosition >= storyDuration)
        {
            onStoryComplete();
        }

        repaint();
    }
}

//==============================================================================
void StoryViewer::setStories(const std::vector<StoryData>& newStories, int startIndex)
{
    stories = newStories;
    currentStoryIndex = juce::jlimit(0, static_cast<int>(stories.size()) - 1, startIndex);

    // Initialize progress segments
    progressSegments.clear();
    for (size_t i = 0; i < stories.size(); ++i)
    {
        progressSegments.push_back({0.0f, false});
    }

    // Mark previous stories as complete
    for (int i = 0; i < currentStoryIndex; ++i)
    {
        progressSegments[i].progress = 1.0f;
        progressSegments[i].completed = true;
    }

    loadCurrentStory();
}

void StoryViewer::showNextStory()
{
    if (currentStoryIndex < static_cast<int>(stories.size()) - 1)
    {
        // Mark current as complete
        if (currentStoryIndex >= 0 && currentStoryIndex < static_cast<int>(progressSegments.size()))
        {
            progressSegments[currentStoryIndex].progress = 1.0f;
            progressSegments[currentStoryIndex].completed = true;
        }

        currentStoryIndex++;
        loadCurrentStory();
    }
    else
    {
        // No more stories, close or go to next user
        if (onNextUser)
        {
            // Could notify to load next user's stories
        }
        closeViewer();
    }
}

void StoryViewer::showPreviousStory()
{
    if (currentStoryIndex > 0)
    {
        currentStoryIndex--;
        // Reset progress for stories after current
        for (int i = currentStoryIndex; i < static_cast<int>(progressSegments.size()); ++i)
        {
            progressSegments[i].progress = 0.0f;
            progressSegments[i].completed = false;
        }
        loadCurrentStory();
    }
}

void StoryViewer::closeViewer()
{
    // Stop playback
    if (audioPlayer)
        audioPlayer->stop();

    playing = false;

    if (onClose)
        onClose();
}

void StoryViewer::togglePlayPause()
{
    if (playing)
    {
        if (audioPlayer)
            audioPlayer->pause();
        playing = false;
    }
    else
    {
        if (audioPlayer)
            audioPlayer->play();
        playing = true;
    }

    repaint();
}

//==============================================================================
void StoryViewer::drawHeader(juce::Graphics& g)
{
    // Semi-transparent header background
    g.setColour(StoryViewerColors::headerBg);
    g.fillRect(headerArea.withHeight(headerArea.getHeight() + 20));

    const auto* story = getCurrentStory();
    if (!story)
        return;

    auto bounds = headerArea.reduced(15, 10);

    // Avatar placeholder
    int avatarSize = 40;
    auto avatarBounds = bounds.removeFromLeft(avatarSize);
    g.setColour(StoryViewerColors::surface);
    g.fillEllipse(avatarBounds.toFloat());

    // Initial
    g.setColour(StoryViewerColors::textPrimary);
    g.setFont(juce::Font(16.0f, juce::Font::bold));
    juce::String initial = story->username.isNotEmpty() ?
                           story->username.substring(0, 1).toUpperCase() : "?";
    g.drawText(initial, avatarBounds, juce::Justification::centred);

    bounds.removeFromLeft(10);

    // Username
    g.setColour(StoryViewerColors::textPrimary);
    g.setFont(juce::Font(14.0f, juce::Font::bold));
    g.drawText(story->username, bounds.removeFromTop(20), juce::Justification::centredLeft);

    // Expiration time
    g.setColour(StoryViewerColors::textSecondary);
    g.setFont(12.0f);
    g.drawText(story->getExpirationText(), bounds.removeFromTop(18), juce::Justification::centredLeft);

    // Close button (X)
    g.setColour(StoryViewerColors::textPrimary);
    g.setFont(juce::Font(20.0f, juce::Font::bold));
    g.drawText(juce::CharPointer_UTF8("\xc3\x97"), closeButtonArea, juce::Justification::centred); // � character
}

void StoryViewer::drawProgressBar(juce::Graphics& g)
{
    if (progressSegments.empty())
        return;

    auto bounds = progressBarArea.reduced(10, 0);
    int numSegments = static_cast<int>(progressSegments.size());
    int segmentGap = 4;
    int totalGaps = (numSegments - 1) * segmentGap;
    int segmentWidth = (bounds.getWidth() - totalGaps) / numSegments;

    for (int i = 0; i < numSegments; ++i)
    {
        int x = bounds.getX() + i * (segmentWidth + segmentGap);
        auto segmentBounds = juce::Rectangle<int>(x, bounds.getY(), segmentWidth, bounds.getHeight());

        // Background
        g.setColour(StoryViewerColors::progressBg);
        g.fillRoundedRectangle(segmentBounds.toFloat(), 2.0f);

        // Progress
        float progress = progressSegments[i].progress;
        if (progress > 0)
        {
            auto progressBounds = segmentBounds.withWidth(static_cast<int>(segmentWidth * progress));
            g.setColour(StoryViewerColors::progressFg);
            g.fillRoundedRectangle(progressBounds.toFloat(), 2.0f);
        }
    }
}

void StoryViewer::drawStoryContent(juce::Graphics& g)
{
    const auto* story = getCurrentStory();
    if (!story)
    {
        g.setColour(StoryViewerColors::textSecondary);
        g.setFont(16.0f);
        g.drawText("No story to display", contentArea, juce::Justification::centred);
        return;
    }

    // Draw waveform in upper portion
    auto waveformBounds = contentArea.reduced(20).removeFromTop(100);
    drawWaveform(g, waveformBounds);

    // Piano roll shows MIDI if available
    if (story->midiData.isObject() && pianoRoll)
    {
        pianoRoll->setVisible(true);
        pianoRoll->setMIDIData(story->midiData);
        pianoRoll->setPlaybackPosition(playbackPosition);
    }
    else if (pianoRoll)
    {
        pianoRoll->setVisible(false);

        // Show "Audio only" message
        g.setColour(StoryViewerColors::textSecondary);
        g.setFont(14.0f);
        g.drawText("Audio Only - No MIDI Data", contentArea.reduced(20).removeFromBottom(100),
                   juce::Justification::centred);
    }

    // View count (for story owner)
    if (story->userId == currentUserId)
    {
        drawViewCount(g);
    }
}

void StoryViewer::drawWaveform(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    // Background
    g.setColour(StoryViewerColors::surface);
    g.fillRoundedRectangle(bounds.toFloat(), 8.0f);

    // Draw waveform if loaded, otherwise show placeholder
    if (waveformLoaded && !currentWaveformPath.isEmpty())
    {
        // Draw actual waveform
        g.setColour(StoryViewerColors::waveformColor);
        g.strokePath(currentWaveformPath, juce::PathStrokeType(1.5f));
    }
    else
    {
        // Placeholder waveform visualization while loading
        float centerY = bounds.getCentreY();
        float amplitude = bounds.getHeight() * 0.4f;

        juce::Path wavePath;
        wavePath.startNewSubPath(bounds.getX(), centerY);

        for (int x = bounds.getX(); x < bounds.getRight(); x += 2)
        {
            float progress = static_cast<float>(x - bounds.getX()) / bounds.getWidth();
            float wave = std::sin(progress * 30.0f) * amplitude * (0.3f + progress * 0.7f);

            wavePath.lineTo(x, centerY + wave);
        }

        g.setColour(StoryViewerColors::waveformColor.withAlpha(0.3f));
        g.strokePath(wavePath, juce::PathStrokeType(2.0f));
    }

    // Playback position indicator
    if (storyDuration > 0)
    {
        float positionX = bounds.getX() + (playbackPosition / storyDuration) * bounds.getWidth();
        g.setColour(StoryViewerColors::progressFg);
        g.drawVerticalLine(static_cast<int>(positionX), bounds.getY(), bounds.getBottom());
    }
}

void StoryViewer::drawViewCount(juce::Graphics& g)
{
    const auto* story = getCurrentStory();
    if (!story)
        return;

    auto viewCountBounds = contentArea.removeFromBottom(40).reduced(20, 0);

    g.setColour(StoryViewerColors::textSecondary);
    g.setFont(14.0f);

    juce::String viewText = juce::String(story->viewCount) + " view" +
                            (story->viewCount != 1 ? "s" : "");
    g.drawText(viewText, viewCountBounds, juce::Justification::centredLeft);
}

void StoryViewer::drawPlayPauseOverlay(juce::Graphics& g)
{
    // Semi-transparent overlay
    g.setColour(StoryViewerColors::playOverlay);
    g.fillRect(contentArea);

    // Play icon
    auto center = contentArea.getCentre().toFloat();
    float iconSize = 60.0f;

    juce::Path playIcon;
    playIcon.addTriangle(center.x - iconSize * 0.3f, center.y - iconSize * 0.4f,
                         center.x - iconSize * 0.3f, center.y + iconSize * 0.4f,
                         center.x + iconSize * 0.4f, center.y);

    g.setColour(StoryViewerColors::textPrimary.withAlpha(0.8f));
    g.fillPath(playIcon);
}

void StoryViewer::drawExpiredMessage(juce::Graphics& g)
{
    // Dark overlay
    g.setColour(StoryViewerColors::background);
    g.fillRect(getLocalBounds());

    // Expired message
    auto center = getLocalBounds().getCentre().toFloat();

    g.setColour(StoryViewerColors::textSecondary);
    g.setFont(juce::Font(24.0f, juce::Font::bold));
    g.drawText("Story Expired",
               juce::Rectangle<int>(0, static_cast<int>(center.y - 40), getWidth(), 30),
               juce::Justification::centred);

    g.setColour(StoryViewerColors::textSecondary.withAlpha(0.7f));
    g.setFont(14.0f);
    g.drawText("This story has expired and is no longer available.",
               juce::Rectangle<int>(0, static_cast<int>(center.y), getWidth(), 20),
               juce::Justification::centred);
}

void StoryViewer::drawViewersButton(juce::Graphics& g)
{
    if (viewersButtonArea.isEmpty())
        return;

    const auto* story = getCurrentStory();
    if (!story)
        return;

    // Button background
    g.setColour(StoryViewerColors::surface.withAlpha(0.8f));
    g.fillRoundedRectangle(viewersButtonArea.toFloat(), 8.0f);

    // Button border
    g.setColour(StoryViewerColors::textSecondary.withAlpha(0.5f));
    g.drawRoundedRectangle(viewersButtonArea.toFloat(), 8.0f, 1.0f);

    // Icon and text
    g.setColour(StoryViewerColors::textPrimary);
    g.setFont(12.0f);

    juce::String buttonText = juce::String(story->viewCount) + " view" + (story->viewCount != 1 ? "s" : "");
    g.drawText(buttonText, viewersButtonArea, juce::Justification::centred);
}

void StoryViewer::drawShareButton(juce::Graphics& g)
{
    if (shareButtonArea.isEmpty())
        return;

    // Button background
    g.setColour(StoryViewerColors::surface.withAlpha(0.8f));
    g.fillRoundedRectangle(shareButtonArea.toFloat(), 8.0f);

    // Button border
    g.setColour(StoryViewerColors::textSecondary.withAlpha(0.5f));
    g.drawRoundedRectangle(shareButtonArea.toFloat(), 8.0f, 1.0f);

    // Share icon (simple arrow pointing right)
    g.setColour(StoryViewerColors::textPrimary);
    g.setFont(12.0f);
    g.drawText("Share", shareButtonArea, juce::Justification::centred);
}

void StoryViewer::drawMIDIButton(juce::Graphics& g)
{
    const auto* story = getCurrentStory();
    if (!story || !story->hasDownloadableMIDI())
        return;

    if (midiButtonArea.isEmpty())
        return;

    // Button background - use a slightly different color to indicate it's special
    g.setColour(StoryViewerColors::waveformColor.withAlpha(0.3f));
    g.fillRoundedRectangle(midiButtonArea.toFloat(), 8.0f);

    // Button border
    g.setColour(StoryViewerColors::waveformColor.withAlpha(0.7f));
    g.drawRoundedRectangle(midiButtonArea.toFloat(), 8.0f, 1.0f);

    // MIDI text (avoid emoji for Linux font compatibility)
    g.setColour(StoryViewerColors::textPrimary);
    g.setFont(11.0f);
    g.drawText("[MIDI]", midiButtonArea, juce::Justification::centred);
}

void StoryViewer::drawAudioDownloadButton(juce::Graphics& g)
{
    const auto* story = getCurrentStory();
    if (!story || story->audioUrl.isEmpty())
        return;

    if (audioDownloadButtonArea.isEmpty())
        return;

    // Button background - blue tint for audio download
    g.setColour(juce::Colour(0xff2196F3).withAlpha(0.3f));
    g.fillRoundedRectangle(audioDownloadButtonArea.toFloat(), 8.0f);

    // Button border
    g.setColour(juce::Colour(0xff2196F3).withAlpha(0.7f));
    g.drawRoundedRectangle(audioDownloadButtonArea.toFloat(), 8.0f, 1.0f);

    // Button text
    g.setColour(StoryViewerColors::textPrimary);
    g.setFont(11.0f);
    g.drawText("[Audio]", audioDownloadButtonArea, juce::Justification::centred);
}

void StoryViewer::drawDeleteButton(juce::Graphics& g)
{
    const auto* story = getCurrentStory();
    if (!story || story->userId != currentUserId)
        return;

    if (deleteButtonArea.isEmpty())
        return;

    // Button background - red tint for delete action
    g.setColour(juce::Colour(0xFFFF4757).withAlpha(0.3f));  // Red tint for delete
    g.fillRoundedRectangle(deleteButtonArea.toFloat(), 8.0f);

    // Button border
    g.setColour(juce::Colour(0xFFFF4757).withAlpha(0.7f));
    g.drawRoundedRectangle(deleteButtonArea.toFloat(), 8.0f, 1.0f);

    // Button text
    g.setColour(StoryViewerColors::textPrimary);
    g.setFont(11.0f);
    g.drawText("Delete", deleteButtonArea, juce::Justification::centred);
}

void StoryViewer::drawRemixButton(juce::Graphics& g)
{
    const auto* story = getCurrentStory();
    if (!story || story->isExpired())
        return;

    if (remixButtonArea.isEmpty())
        return;

    // Button background - accent color for remix action
    g.setColour(juce::Colour(0xff4CAF50).withAlpha(0.3f));  // Green tint for remix
    g.fillRoundedRectangle(remixButtonArea.toFloat(), 8.0f);

    // Button border
    g.setColour(juce::Colour(0xff4CAF50).withAlpha(0.7f));
    g.drawRoundedRectangle(remixButtonArea.toFloat(), 8.0f, 1.0f);

    // Button text - show what can be remixed
    g.setColour(StoryViewerColors::textPrimary);
    g.setFont(11.0f);

    juce::String buttonText = "Remix";
    if (story->hasDownloadableMIDI() && story->audioUrl.isNotEmpty())
        buttonText = "Remix";  // Both available
    else if (story->hasDownloadableMIDI())
        buttonText = "Remix MIDI";
    else
        buttonText = "Remix Audio";

    g.drawText(buttonText, remixButtonArea, juce::Justification::centred);
}

//==============================================================================
// Share functionality - copy story link to clipboard
void StoryViewer::handleShareStory(const juce::String& storyId)
{
    // Generate shareable link (expires in 24h)
    juce::String shareUrl = "https://sidechain.live/story/" + storyId;
    juce::SystemClipboard::copyTextToClipboard(shareUrl);
    Log::info("StoryViewer: Copied story link to clipboard: " + shareUrl);
}

//==============================================================================
// MIDI download functionality (R.3.3.5.5)
void StoryViewer::handleDeleteStory(const juce::String& storyId)
{
    Log::debug("StoryViewer: Delete story clicked for story: " + storyId);

    if (networkClient == nullptr)
    {
        Log::warn("StoryViewer: Cannot delete story - networkClient is null");
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::WarningIcon,
            "Error",
            "Unable to delete story. Please try again later.");
        return;
    }

    // Show confirmation dialog
    auto options = juce::MessageBoxOptions()
        .withIconType(juce::MessageBoxIconType::QuestionIcon)
        .withTitle("Delete Story")
        .withMessage("Are you sure you want to delete this story? This action cannot be undone.")
        .withButton("Delete")
        .withButton("Cancel");

    juce::AlertWindow::showAsync(options, [this, storyId](int result) {
        if (result == 1)  // Delete clicked
        {
            networkClient->deleteStory(storyId, [this, storyId](Outcome<juce::var> deleteResult) {
                juce::MessageManager::callAsync([this, deleteResult, storyId]() {
                    if (deleteResult.isOk())
                    {
                        Log::info("StoryViewer: Story deleted successfully: " + storyId);
                        
                        // Remove story from local list
                        stories.erase(
                            std::remove_if(stories.begin(), stories.end(),
                                [storyId](const StoryData& s) { return s.id == storyId; }),
                            stories.end());

                        // Adjust current index if needed
                        if (currentStoryIndex >= static_cast<int>(stories.size()))
                        {
                            currentStoryIndex = static_cast<int>(stories.size()) - 1;
                        }

                        // If no stories left, close viewer
                        if (stories.empty())
                        {
                            closeViewer();
                        }
                        else
                        {
                            // Load the current story (or previous if we deleted the current one)
                            loadCurrentStory();
                            repaint();
                        }

                        // Notify callback
                        if (onDeleteClicked)
                            onDeleteClicked(storyId);
                    }
                    else
                    {
                        Log::error("StoryViewer: Failed to delete story: " + deleteResult.getError());
                        juce::AlertWindow::showMessageBoxAsync(
                            juce::MessageBoxIconType::WarningIcon,
                            "Delete Failed",
                            "Failed to delete story:\n" + deleteResult.getError());
                    }
                });
            });
        }
    });
}

void StoryViewer::handleDownloadMIDI(const StoryData& story)
{
    Log::debug("StoryViewer: Download MIDI clicked for story: " + story.id + ", midiId: " + story.midiPatternId);

    if (!story.hasDownloadableMIDI() || story.midiPatternId.isEmpty())
    {
        Log::warn("StoryViewer: Cannot download MIDI - no MIDI pattern ID available");
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::WarningIcon,
            "Error",
            "No MIDI data available for this story.");
        return;
    }

    if (networkClient == nullptr)
    {
        Log::warn("StoryViewer: Cannot download MIDI - networkClient is null");
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::WarningIcon,
            "Error",
            "Unable to download MIDI. Please try again later.");
        return;
    }

    // Determine target location for MIDI files (R.3.3.7.2)
    // Try DAW project folder first, fallback to default location
    juce::File targetDir = DAWProjectFolder::getMIDIFileLocation();
    
    // Ensure target directory exists
    if (!targetDir.exists())
    {
        auto result = targetDir.createDirectory();
        if (result.failed())
        {
            Log::warn("Failed to create MIDI directory: " + result.getErrorMessage());
            // Fallback to default location
            targetDir = DAWProjectFolder::getDefaultMIDIFolder();
        }
    }

    // Create filename from story username and ID
    juce::String safeName = story.username.isNotEmpty() ? story.username : "unknown";
    safeName = safeName.replaceCharacters(" /\\:*?\"<>|", "__________");
    juce::String filename = safeName + "_" + story.midiPatternId.substring(0, 8) + ".mid";
    juce::File targetFile = targetDir.getChildFile(filename);

    // Detect DAW for notification message
    auto dawInfo = DAWProjectFolder::detectDAWProjectFolder();
    juce::String notificationMessage = "MIDI saved to:\n" + targetFile.getFullPathName();
    if (dawInfo.isAccessible && dawInfo.dawName != "Unknown")
    {
        notificationMessage += "\n\nFile is in your " + dawInfo.dawName + " project folder and ready to import.";
    }

    // Download the MIDI file
    networkClient->downloadMIDI(
        story.midiPatternId,
        targetFile,
        [targetFile, notificationMessage](Outcome<juce::var> result) {
            if (result.isOk())
            {
                Log::info("Story MIDI downloaded: " + targetFile.getFullPathName());
                juce::AlertWindow::showMessageBoxAsync(
                    juce::MessageBoxIconType::InfoIcon,
                    "MIDI Downloaded",
                    notificationMessage);
            }
            else
            {
                Log::error("Failed to download story MIDI: " + result.getError());
                juce::AlertWindow::showMessageBoxAsync(
                    juce::MessageBoxIconType::WarningIcon,
                    "Download Failed",
                    "Failed to download MIDI:\n" + result.getError());
            }
        });
}

void StoryViewer::handleDownloadAudio(const StoryData& story)
{
    Log::debug("StoryViewer: Download audio clicked for story: " + story.id);

    if (story.audioUrl.isEmpty())
    {
        Log::warn("StoryViewer: Cannot download audio - no audio URL available");
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::WarningIcon,
            "Error",
            "No audio data available for this story.");
        return;
    }

    if (networkClient == nullptr)
    {
        Log::warn("StoryViewer: Cannot download audio - networkClient is null");
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::WarningIcon,
            "Error",
            "Unable to download audio. Please try again later.");
        return;
    }

    // Get download info from backend (19.1)
    networkClient->getStoryDownloadInfo(story.id, [this, story](Outcome<NetworkClient::DownloadInfo> downloadInfoOutcome) {
        if (!downloadInfoOutcome.isOk())
        {
            Log::error("Failed to get story download info: " + downloadInfoOutcome.getError());
            juce::AlertWindow::showMessageBoxAsync(
                juce::MessageBoxIconType::WarningIcon,
                "Error",
                "Failed to get download information. Please try again.");
            return;
        }

        auto info = downloadInfoOutcome.getValue();

        // Determine target location for audio files
        juce::File targetDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
            .getChildFile("Sidechain")
            .getChildFile("Downloads");

        if (!targetDir.exists())
        {
            targetDir.createDirectory();
        }

        juce::File targetFile = targetDir.getChildFile(info.filename);

        // Detect DAW for notification message
        auto dawInfo = DAWProjectFolder::detectDAWProjectFolder();
        juce::String notificationMessage = "Audio saved to:\n" + targetFile.getFullPathName();
        if (dawInfo.isAccessible && dawInfo.dawName != "Unknown")
        {
            notificationMessage += "\n\nYou can drag this file into your " + dawInfo.dawName + " project.";
        }

        // Download the audio file
        networkClient->downloadFile(
            info.downloadUrl,
            targetFile,
            nullptr,  // No progress callback for now
            [targetFile, notificationMessage](Outcome<juce::var> result) {
                if (result.isOk())
                {
                    Log::info("Story audio downloaded: " + targetFile.getFullPathName());
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::MessageBoxIconType::InfoIcon,
                        "Audio Downloaded",
                        notificationMessage);
                }
                else
                {
                    Log::error("Failed to download story audio: " + result.getError());
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::MessageBoxIconType::WarningIcon,
                        "Download Failed",
                        "Failed to download audio:\n" + result.getError());
                }
            });
    });
}

//==============================================================================
void StoryViewer::loadCurrentStory()
{
    const auto* story = getCurrentStory();
    if (!story)
        return;

    Log::info("StoryViewer: Loading story " + story->id);

    // Check if story is expired
    if (story->isExpired())
    {
        Log::warn("StoryViewer: Story " + story->id + " is expired");
        // Stop playback and show expired message
        if (audioPlayer)
            audioPlayer->stop();
        playing = false;
        waveformLoaded = false;
        currentWaveformPath.clear();
        repaint();
        return;
    }

    // Stop current playback
    if (audioPlayer)
        audioPlayer->stop();

    // Reset waveform
    waveformLoaded = false;
    currentWaveformPath.clear();

    // Set up audio player
    storyDuration = story->audioDuration;
    playbackPosition = 0.0;

    if (story->audioUrl.isNotEmpty() && audioPlayer)
    {
        audioPlayer->loadAndPlay(story->id, story->audioUrl);
        playing = true;

        // Load waveform from audio URL
        auto waveformBounds = contentArea.reduced(20).removeFromTop(100);
        WaveformGenerator::generateWaveformPathFromUrl(
            story->audioUrl,
            waveformBounds.reduced(10),
            [this](juce::Path path) {
                currentWaveformPath = path;
                waveformLoaded = !path.isEmpty();
                repaint();
            });
    }

    // Set up MIDI visualization
    if (pianoRoll && story->midiData.isObject())
    {
        pianoRoll->setMIDIData(story->midiData);
        pianoRoll->setVisible(true);

        // Wire up seek callback
        pianoRoll->onSeekToTime = [this](double timeSeconds) {
            if (audioPlayer)
            {
                audioPlayer->seekToPosition(timeSeconds);
                playbackPosition = timeSeconds;
                repaint();
            }
        };
    }
    else if (pianoRoll)
    {
        pianoRoll->setVisible(false);
    }

    // Mark as viewed
    markStoryAsViewed();

    resized();
    repaint();
}

void StoryViewer::markStoryAsViewed()
{
    const auto* story = getCurrentStory();
    if (!story || !networkClient)
        return;

    // Don't mark own stories
    if (story->userId == currentUserId)
        return;

    networkClient->viewStory(story->id, [](Outcome<juce::var> result) {
        if (result.isOk())
            Log::debug("Story marked as viewed");
        else
            Log::warn("Failed to mark story as viewed: " + result.getError());
    });
}

void StoryViewer::updateProgress()
{
    if (currentStoryIndex < 0 || currentStoryIndex >= static_cast<int>(progressSegments.size()))
        return;

    if (storyDuration > 0)
    {
        progressSegments[currentStoryIndex].progress =
            static_cast<float>(playbackPosition / storyDuration);
    }
}

void StoryViewer::onStoryComplete()
{
    Log::debug("Story playback complete");

    // Mark as complete
    if (currentStoryIndex >= 0 && currentStoryIndex < static_cast<int>(progressSegments.size()))
    {
        progressSegments[currentStoryIndex].progress = 1.0f;
        progressSegments[currentStoryIndex].completed = true;
    }

    // Auto-advance to next story
    showNextStory();
}

const StoryData* StoryViewer::getCurrentStory() const
{
    if (currentStoryIndex >= 0 && currentStoryIndex < static_cast<int>(stories.size()))
    {
        return &stories[currentStoryIndex];
    }
    return nullptr;
}
