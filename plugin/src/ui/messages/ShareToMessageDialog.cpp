#include "ShareToMessageDialog.h"
#include "../../util/Colors.h"
#include "../../util/Log.h"
#include "../../util/StringFormatter.h"

//==============================================================================
ShareToMessageDialog::ShareToMessageDialog()
{
    // Search input
    searchInput = std::make_unique<juce::TextEditor>();
    searchInput->setMultiLine(false);
    searchInput->setReturnKeyStartsNewLine(false);
    searchInput->setScrollbarsShown(false);
    searchInput->setCaretVisible(true);
    searchInput->setTextToShowWhenEmpty("Search for a user...", SidechainColors::textMuted());
    searchInput->setFont(juce::Font(juce::FontOptions().withHeight(15.0f)));
    searchInput->setColour(juce::TextEditor::backgroundColourId, SidechainColors::surface());
    searchInput->setColour(juce::TextEditor::textColourId, SidechainColors::textPrimary());
    searchInput->setColour(juce::TextEditor::outlineColourId, SidechainColors::border());
    searchInput->setColour(juce::TextEditor::focusedOutlineColourId, SidechainColors::primary());
    searchInput->addListener(this);
    addAndMakeVisible(searchInput.get());

    // Scroll bar
    scrollBar = std::make_unique<juce::ScrollBar>(true);
    scrollBar->addListener(this);
    scrollBar->setRangeLimits(0.0, 0.0);
    addAndMakeVisible(scrollBar.get());

    // Send button
    sendButton = std::make_unique<juce::TextButton>("Send");
    sendButton->addListener(this);
    sendButton->setEnabled(false);
    sendButton->setColour(juce::TextButton::buttonColourId, SidechainColors::primary());
    sendButton->setColour(juce::TextButton::textColourOnId, SidechainColors::textPrimary());
    sendButton->setColour(juce::TextButton::textColourOffId, SidechainColors::textPrimary());
    addAndMakeVisible(sendButton.get());

    // Cancel button
    cancelButton = std::make_unique<juce::TextButton>("Cancel");
    cancelButton->addListener(this);
    cancelButton->setColour(juce::TextButton::buttonColourId, SidechainColors::surface());
    cancelButton->setColour(juce::TextButton::textColourOnId, SidechainColors::textPrimary());
    cancelButton->setColour(juce::TextButton::textColourOffId, SidechainColors::textPrimary());
    addAndMakeVisible(cancelButton.get());

    // Set size last to avoid resized() being called before components are created
    setSize(DIALOG_WIDTH, DIALOG_HEIGHT);
}

ShareToMessageDialog::~ShareToMessageDialog()
{
    stopTimer();
    scrollBar->removeListener(this);
}

//==============================================================================
void ShareToMessageDialog::paint(juce::Graphics& g)
{
    // Semi-transparent backdrop
    g.fillAll(juce::Colours::black.withAlpha(0.6f));

    // Dialog background
    auto dialogBounds = getLocalBounds().withSizeKeepingCentre(DIALOG_WIDTH, DIALOG_HEIGHT);

    // Shadow
    g.setColour(juce::Colours::black.withAlpha(0.3f));
    g.fillRoundedRectangle(dialogBounds.toFloat().translated(4, 4), 12.0f);

    // Background
    g.setColour(SidechainColors::backgroundLight());
    g.fillRoundedRectangle(dialogBounds.toFloat(), 12.0f);

    // Border
    g.setColour(SidechainColors::border());
    g.drawRoundedRectangle(dialogBounds.toFloat(), 12.0f, 1.0f);

    drawHeader(g);
    drawContentPreview(g);

    if (isSending)
        drawSendingState(g);
    else if (isLoadingChannels && recentChannels.empty())
        drawLoadingState(g);
    else
        drawRecipientsList(g);
}

void ShareToMessageDialog::drawHeader(juce::Graphics& g)
{
    auto dialogBounds = getLocalBounds().withSizeKeepingCentre(DIALOG_WIDTH, DIALOG_HEIGHT);
    auto headerBounds = dialogBounds.removeFromTop(HEADER_HEIGHT - 20);

    g.setColour(SidechainColors::textPrimary());
    g.setFont(juce::Font(juce::FontOptions().withHeight(20.0f)).boldened());
    g.drawText("Send to...", headerBounds.reduced(PADDING, 0).withTrimmedTop(15),
               juce::Justification::centredLeft);
}

void ShareToMessageDialog::drawContentPreview(juce::Graphics& g)
{
    auto dialogBounds = getLocalBounds().withSizeKeepingCentre(DIALOG_WIDTH, DIALOG_HEIGHT);
    dialogBounds.removeFromTop(HEADER_HEIGHT - 20);
    auto previewBounds = dialogBounds.removeFromTop(PREVIEW_HEIGHT).reduced(PADDING, 5);

    // Background
    g.setColour(SidechainColors::surface());
    g.fillRoundedRectangle(previewBounds.toFloat(), 8.0f);

    // Content preview based on share type
    auto contentBounds = previewBounds.reduced(12, 8);

    if (shareType == ShareType::Post)
    {
        // Post preview
        g.setColour(SidechainColors::textSecondary());
        g.setFont(juce::Font(juce::FontOptions().withHeight(11.0f)));
        g.drawText("Sharing post by @" + postToShare.username,
                   contentBounds.removeFromTop(16), juce::Justification::centredLeft);

        // Audio info
        g.setColour(SidechainColors::textPrimary());
        g.setFont(juce::Font(juce::FontOptions().withHeight(14.0f)));

        juce::String info;
        if (postToShare.bpm > 0)
            info += juce::String(postToShare.bpm) + " BPM";
        if (postToShare.key.isNotEmpty())
        {
            if (info.isNotEmpty()) info += " • ";
            info += postToShare.key;
        }
        if (postToShare.durationSeconds > 0)
        {
            if (info.isNotEmpty()) info += " • ";
            info += StringFormatter::formatDuration(postToShare.durationSeconds);
        }
        g.drawText(info, contentBounds.removeFromTop(20), juce::Justification::centredLeft);

        // Genres
        if (!postToShare.genres.isEmpty())
        {
            g.setColour(SidechainColors::textMuted());
            g.setFont(juce::Font(juce::FontOptions().withHeight(12.0f)));
            g.drawText(postToShare.genres.joinIntoString(", "),
                       contentBounds, juce::Justification::centredLeft);
        }
    }
    else if (shareType == ShareType::Story)
    {
        // Story preview
        g.setColour(SidechainColors::textSecondary());
        g.setFont(juce::Font(juce::FontOptions().withHeight(11.0f)));
        g.drawText("Sharing story by @" + storyToShare.username,
                   contentBounds.removeFromTop(16), juce::Justification::centredLeft);

        g.setColour(SidechainColors::textPrimary());
        g.setFont(juce::Font(juce::FontOptions().withHeight(14.0f)));

        juce::String info = "Audio story";
        if (storyToShare.audioDuration > 0)
            info += " • " + StringFormatter::formatDuration(storyToShare.audioDuration);
        g.drawText(info, contentBounds, juce::Justification::centredLeft);
    }
    else
    {
        g.setColour(SidechainColors::textMuted());
        g.setFont(juce::Font(juce::FontOptions().withHeight(14.0f)));
        g.drawText("No content selected", previewBounds, juce::Justification::centred);
    }
}

void ShareToMessageDialog::drawRecipientsList(juce::Graphics& g)
{
    auto contentBounds = getContentBounds();

    // Clip to content area
    g.saveState();
    g.reduceClipRegion(contentBounds);

    int y = contentBounds.getY() - static_cast<int>(scrollOffset);
    int itemWidth = contentBounds.getWidth() - 12;

    // Section header: Recent Conversations
    if (!recentChannels.empty() && searchResults.empty())
    {
        g.setColour(SidechainColors::textMuted());
        g.setFont(juce::Font(juce::FontOptions().withHeight(11.0f)));
        g.drawText("RECENT CONVERSATIONS", contentBounds.getX(), y, itemWidth, 20,
                   juce::Justification::centredLeft);
        y += 25;
    }

    // Draw recent channels
    if (searchResults.empty())
    {
        for (const auto& channel : recentChannels)
        {
            auto itemBounds = juce::Rectangle<int>(contentBounds.getX(), y, itemWidth, ITEM_HEIGHT - 4);

            if (itemBounds.getBottom() >= contentBounds.getY() &&
                itemBounds.getY() <= contentBounds.getBottom())
            {
                juce::String recipientId = channel.type + ":" + channel.id;
                bool selected = isSelected(recipientId);
                drawChannelItem(g, channel, itemBounds, selected);
            }
            y += ITEM_HEIGHT;
        }
    }

    // Section header: Search Results
    if (!searchResults.empty())
    {
        g.setColour(SidechainColors::textMuted());
        g.setFont(juce::Font(juce::FontOptions().withHeight(11.0f)));
        g.drawText("USERS", contentBounds.getX(), y, itemWidth, 20,
                   juce::Justification::centredLeft);
        y += 25;

        for (const auto& user : searchResults)
        {
            auto itemBounds = juce::Rectangle<int>(contentBounds.getX(), y, itemWidth, ITEM_HEIGHT - 4);

            if (itemBounds.getBottom() >= contentBounds.getY() &&
                itemBounds.getY() <= contentBounds.getBottom())
            {
                juce::String recipientId = "user:" + user.id;
                bool selected = isSelected(recipientId);
                drawUserItem(g, user, itemBounds, selected);
            }
            y += ITEM_HEIGHT;
        }
    }

    g.restoreState();
}

void ShareToMessageDialog::drawChannelItem(juce::Graphics& g, const StreamChatClient::Channel& channel,
                                           juce::Rectangle<int> bounds, bool isSelected)
{
    // Background
    g.setColour(isSelected ? SidechainColors::primary().withAlpha(0.2f) : SidechainColors::surface());
    g.fillRoundedRectangle(bounds.toFloat(), 8.0f);

    if (isSelected)
    {
        g.setColour(SidechainColors::primary());
        g.drawRoundedRectangle(bounds.toFloat(), 8.0f, 2.0f);
    }

    // Avatar placeholder
    auto avatarBounds = bounds.removeFromLeft(bounds.getHeight()).reduced(10);
    g.setColour(SidechainColors::backgroundLighter());
    g.fillEllipse(avatarBounds.toFloat());

    // Initial
    g.setColour(SidechainColors::textPrimary());
    g.setFont(juce::Font(juce::FontOptions().withHeight(16.0f)).boldened());
    juce::String displayName = getChannelDisplayName(channel);
    juce::String initial = displayName.isNotEmpty() ? displayName.substring(0, 1).toUpperCase() : "?";
    g.drawText(initial, avatarBounds, juce::Justification::centred);

    // Channel name
    auto textBounds = bounds.reduced(10, 0);
    g.setColour(SidechainColors::textPrimary());
    g.setFont(juce::Font(juce::FontOptions().withHeight(15.0f)));
    g.drawText(displayName, textBounds.removeFromTop(25), juce::Justification::centredLeft);

    // Last message preview
    if (channel.lastMessage.isObject())
    {
        juce::String lastText = channel.lastMessage.getProperty("text", "").toString();
        if (lastText.length() > 40)
            lastText = lastText.substring(0, 40) + "...";

        g.setColour(SidechainColors::textMuted());
        g.setFont(juce::Font(juce::FontOptions().withHeight(12.0f)));
        g.drawText(lastText, textBounds, juce::Justification::centredLeft);
    }

    // Selection checkmark
    if (isSelected)
    {
        auto checkBounds = bounds.removeFromRight(30).reduced(5);
        g.setColour(SidechainColors::primary());
        g.setFont(juce::Font(juce::FontOptions().withHeight(18.0f)));
        g.drawText(juce::String(juce::CharPointer_UTF8("\xE2\x9C\x93")), checkBounds, juce::Justification::centred);
    }
}

void ShareToMessageDialog::drawUserItem(juce::Graphics& g, const SearchUser& user,
                                        juce::Rectangle<int> bounds, bool isSelected)
{
    // Background
    g.setColour(isSelected ? SidechainColors::primary().withAlpha(0.2f) : SidechainColors::surface());
    g.fillRoundedRectangle(bounds.toFloat(), 8.0f);

    if (isSelected)
    {
        g.setColour(SidechainColors::primary());
        g.drawRoundedRectangle(bounds.toFloat(), 8.0f, 2.0f);
    }

    // Avatar placeholder
    auto avatarBounds = bounds.removeFromLeft(bounds.getHeight()).reduced(10);
    g.setColour(SidechainColors::backgroundLighter());
    g.fillEllipse(avatarBounds.toFloat());

    // Initial
    g.setColour(SidechainColors::textPrimary());
    g.setFont(juce::Font(juce::FontOptions().withHeight(16.0f)).boldened());
    juce::String initial = user.username.isNotEmpty() ? user.username.substring(0, 1).toUpperCase() : "?";
    g.drawText(initial, avatarBounds, juce::Justification::centred);

    // Username
    auto textBounds = bounds.reduced(10, 0);
    g.setColour(SidechainColors::textPrimary());
    g.setFont(juce::Font(juce::FontOptions().withHeight(15.0f)));
    g.drawText("@" + user.username, textBounds.removeFromTop(25), juce::Justification::centredLeft);

    // Display name
    if (user.displayName.isNotEmpty() && user.displayName != user.username)
    {
        g.setColour(SidechainColors::textMuted());
        g.setFont(juce::Font(juce::FontOptions().withHeight(12.0f)));
        g.drawText(user.displayName, textBounds, juce::Justification::centredLeft);
    }

    // Selection checkmark
    if (isSelected)
    {
        auto checkBounds = bounds.removeFromRight(30).reduced(5);
        g.setColour(SidechainColors::primary());
        g.setFont(juce::Font(juce::FontOptions().withHeight(18.0f)));
        g.drawText(juce::String(juce::CharPointer_UTF8("\xE2\x9C\x93")), checkBounds, juce::Justification::centred);
    }
}

void ShareToMessageDialog::drawLoadingState(juce::Graphics& g)
{
    auto contentBounds = getContentBounds();
    g.setColour(SidechainColors::textSecondary());
    g.setFont(juce::Font(juce::FontOptions().withHeight(14.0f)));
    g.drawText("Loading conversations...", contentBounds, juce::Justification::centred);
}

void ShareToMessageDialog::drawSendingState(juce::Graphics& g)
{
    auto contentBounds = getContentBounds();

    g.setColour(SidechainColors::textPrimary());
    g.setFont(juce::Font(juce::FontOptions().withHeight(16.0f)));
    g.drawText("Sending...", contentBounds.withTrimmedBottom(30), juce::Justification::centred);

    if (sendTotal > 0)
    {
        g.setColour(SidechainColors::textSecondary());
        g.setFont(juce::Font(juce::FontOptions().withHeight(13.0f)));
        g.drawText(juce::String(sendProgress) + " of " + juce::String(sendTotal),
                   contentBounds.withTrimmedTop(30), juce::Justification::centred);
    }
}

//==============================================================================
void ShareToMessageDialog::resized()
{
    auto dialogBounds = getLocalBounds().withSizeKeepingCentre(DIALOG_WIDTH, DIALOG_HEIGHT);

    // Header
    dialogBounds.removeFromTop(HEADER_HEIGHT - 20);

    // Preview
    dialogBounds.removeFromTop(PREVIEW_HEIGHT);

    auto contentArea = dialogBounds.reduced(PADDING, 5);

    // Search input at top of content
    searchInput->setBounds(contentArea.removeFromTop(INPUT_HEIGHT - 10));
    contentArea.removeFromTop(10);

    // Buttons at bottom
    auto buttonBounds = contentArea.removeFromBottom(BUTTON_HEIGHT + 10).withTrimmedTop(10);
    cancelButton->setBounds(buttonBounds.removeFromLeft(100));
    buttonBounds.removeFromLeft(10);
    sendButton->setBounds(buttonBounds.removeFromLeft(100));

    // Scrollbar
    scrollBar->setBounds(contentArea.removeFromRight(10));

    // Update scroll range
    int totalItems = searchResults.empty() ? (int)recentChannels.size() : (int)searchResults.size();
    int totalHeight = 25 + totalItems * ITEM_HEIGHT; // Section header + items
    int visibleHeight = contentArea.getHeight();
    scrollBar->setRangeLimits(0.0, static_cast<double>(juce::jmax(0, totalHeight - visibleHeight)));
    scrollBar->setCurrentRangeStart(scrollOffset, juce::dontSendNotification);
}

//==============================================================================
void ShareToMessageDialog::mouseUp(const juce::MouseEvent& event)
{
    if (isSending)
        return;

    auto pos = event.getPosition();
    int index = getItemIndexAt(pos);

    if (index >= 0)
    {
        if (searchResults.empty() && index < (int)recentChannels.size())
        {
            // Clicked on a channel
            const auto& channel = recentChannels[index];
            juce::String recipientId = channel.type + ":" + channel.id;
            toggleSelection(recipientId);
        }
        else if (!searchResults.empty() && index < (int)searchResults.size())
        {
            // Clicked on a user
            const auto& user = searchResults[index];
            juce::String recipientId = "user:" + user.id;
            toggleSelection(recipientId);
        }
        repaint();
    }
}

void ShareToMessageDialog::textEditorTextChanged(juce::TextEditor& editor)
{
    if (&editor == searchInput.get())
    {
        currentQuery = editor.getText().trim();

        // Debounce search
        stopTimer();
        if (currentQuery.length() >= 2)
        {
            startTimer(300); // Search after 300ms of no typing
        }
        else if (currentQuery.isEmpty())
        {
            searchResults.clear();
            repaint();
        }
    }
}

void ShareToMessageDialog::textEditorReturnKeyPressed(juce::TextEditor& editor)
{
    if (&editor == searchInput.get() && currentQuery.length() >= 2)
    {
        stopTimer();
        performSearch();
    }
}

void ShareToMessageDialog::buttonClicked(juce::Button* button)
{
    if (button == sendButton.get())
    {
        sendToSelectedRecipients();
    }
    else if (button == cancelButton.get())
    {
        closeDialog();
        if (onCancelled)
            onCancelled();
    }
}

void ShareToMessageDialog::scrollBarMoved(juce::ScrollBar* /*scrollBar*/, double newRangeStart)
{
    scrollOffset = newRangeStart;
    repaint();
}

void ShareToMessageDialog::timerCallback()
{
    stopTimer();
    performSearch();
}

//==============================================================================
void ShareToMessageDialog::setPostToShare(const FeedPost& post)
{
    shareType = ShareType::Post;
    postToShare = post;
    storyToShare = StoryData();
    repaint();
}

void ShareToMessageDialog::setStoryToShare(const StoryData& story)
{
    shareType = ShareType::Story;
    storyToShare = story;
    postToShare = FeedPost();
    repaint();
}

void ShareToMessageDialog::showModal(juce::Component* parentComponent)
{
    if (parentComponent == nullptr)
        return;

    // Reset state
    selectedRecipients.clear();
    searchResults.clear();
    currentQuery.clear();
    scrollOffset = 0.0;
    isSending = false;
    sendProgress = 0;
    sendTotal = 0;

    searchInput->clear();
    sendButton->setEnabled(false);

    // Size to fill parent
    setBounds(parentComponent->getLocalBounds());
    parentComponent->addAndMakeVisible(this);
    toFront(true);

    loadRecentChannels();
}

void ShareToMessageDialog::closeDialog()
{
    juce::MessageManager::callAsync([this]() {
        setVisible(false);
        if (auto* parent = getParentComponent())
            parent->removeChildComponent(this);
    });
}

//==============================================================================
void ShareToMessageDialog::loadRecentChannels()
{
    if (!streamChatClient || !streamChatClient->isAuthenticated())
    {
        Log::warn("ShareToMessageDialog: Cannot load channels - not authenticated");
        return;
    }

    isLoadingChannels = true;
    repaint();

    juce::Component::SafePointer<ShareToMessageDialog> safeThis(this);
    streamChatClient->queryChannels([safeThis](Outcome<std::vector<StreamChatClient::Channel>> result) {
        if (safeThis == nullptr) return;

        safeThis->isLoadingChannels = false;

        if (result.isOk())
        {
            safeThis->recentChannels = result.getValue();
            Log::info("ShareToMessageDialog: Loaded " + juce::String(safeThis->recentChannels.size()) + " channels");
        }
        else
        {
            Log::error("ShareToMessageDialog: Failed to load channels - " + result.getError());
        }

        safeThis->resized();
        safeThis->repaint();
    }, 20, 0);
}

void ShareToMessageDialog::performSearch()
{
    if (!networkClient || currentQuery.length() < 2)
        return;

    isSearching = true;
    repaint();

    juce::Component::SafePointer<ShareToMessageDialog> safeThis(this);
    networkClient->searchUsers(currentQuery, 20, 0, [safeThis](Outcome<juce::var> result) {
        if (safeThis == nullptr) return;

        safeThis->isSearching = false;

        if (result.isError())
        {
            Log::error("ShareToMessageDialog: Search failed - " + result.getError());
            safeThis->repaint();
            return;
        }

        auto response = result.getValue();
        safeThis->searchResults.clear();

        if (response.isObject())
        {
            auto users = response.getProperty("users", juce::var());
            if (users.isArray())
            {
                for (int i = 0; i < users.size(); ++i)
                {
                    auto userData = users[i];
                    SearchUser user;
                    user.id = userData.getProperty("id", "").toString();
                    user.username = userData.getProperty("username", "").toString();
                    user.displayName = userData.getProperty("display_name", "").toString();
                    user.avatarUrl = userData.getProperty("avatar_url", "").toString();

                    // Don't show current user
                    if (user.id != safeThis->currentUserId && !user.id.isEmpty())
                    {
                        safeThis->searchResults.push_back(user);
                    }
                }
            }
        }

        safeThis->resized();
        safeThis->repaint();
    });
}

void ShareToMessageDialog::toggleSelection(const juce::String& recipientId)
{
    int index = selectedRecipients.indexOf(recipientId);
    if (index >= 0)
    {
        selectedRecipients.remove(index);
    }
    else
    {
        selectedRecipients.add(recipientId);
    }

    sendButton->setEnabled(!selectedRecipients.isEmpty());
}

bool ShareToMessageDialog::isSelected(const juce::String& recipientId) const
{
    return selectedRecipients.contains(recipientId);
}

//==============================================================================
void ShareToMessageDialog::sendToSelectedRecipients()
{
    if (selectedRecipients.isEmpty() || shareType == ShareType::None)
        return;

    isSending = true;
    sendProgress = 0;
    sendTotal = selectedRecipients.size();
    sendButton->setEnabled(false);
    repaint();

    // Process each recipient
    for (const auto& recipientId : selectedRecipients)
    {
        if (recipientId.startsWith("user:"))
        {
            // New DM - need to create channel first
            juce::String userId = recipientId.substring(5);
            createAndSendToUser(userId);
        }
        else
        {
            // Existing channel - format is "type:id"
            int colonIdx = recipientId.indexOf(":");
            if (colonIdx > 0)
            {
                juce::String channelType = recipientId.substring(0, colonIdx);
                juce::String channelId = recipientId.substring(colonIdx + 1);
                sendToChannel(channelType, channelId);
            }
        }
    }
}

void ShareToMessageDialog::sendToChannel(const juce::String& channelType, const juce::String& channelId)
{
    if (!streamChatClient)
        return;

    // Build message with shared content in extraData
    juce::var extraData = juce::var(new juce::DynamicObject());
    auto* obj = extraData.getDynamicObject();

    juce::String messageText;

    if (shareType == ShareType::Post)
    {
        // Create shared_post object
        auto sharedPost = juce::var(new juce::DynamicObject());
        auto* postObj = sharedPost.getDynamicObject();
        postObj->setProperty("id", postToShare.id);
        postObj->setProperty("author_id", postToShare.userId);
        postObj->setProperty("author_username", postToShare.username);
        postObj->setProperty("author_avatar_url", postToShare.userAvatarUrl);
        postObj->setProperty("audio_url", postToShare.audioUrl);
        postObj->setProperty("waveform_svg", postToShare.waveformSvg);
        postObj->setProperty("duration_seconds", postToShare.durationSeconds);
        postObj->setProperty("bpm", postToShare.bpm);
        postObj->setProperty("key", postToShare.key);
        if (!postToShare.genres.isEmpty())
            postObj->setProperty("genres", postToShare.genres.joinIntoString(","));

        obj->setProperty("shared_post", sharedPost);
        messageText = "Check out this post by @" + postToShare.username;
    }
    else if (shareType == ShareType::Story)
    {
        // Create shared_story object
        auto sharedStory = juce::var(new juce::DynamicObject());
        auto* storyObj = sharedStory.getDynamicObject();
        storyObj->setProperty("id", storyToShare.id);
        storyObj->setProperty("user_id", storyToShare.userId);
        storyObj->setProperty("username", storyToShare.username);
        storyObj->setProperty("avatar_url", storyToShare.userAvatarUrl);
        storyObj->setProperty("audio_url", storyToShare.audioUrl);
        storyObj->setProperty("duration", storyToShare.audioDuration);

        obj->setProperty("shared_story", sharedStory);
        messageText = "Check out this story by @" + storyToShare.username;
    }

    juce::Component::SafePointer<ShareToMessageDialog> safeThis(this);
    streamChatClient->sendMessage(channelType, channelId, messageText, extraData,
        [safeThis](Outcome<StreamChatClient::Message> result) {
            if (safeThis == nullptr) return;

            safeThis->sendProgress++;

            if (result.isError())
            {
                Log::error("ShareToMessageDialog: Failed to send to channel - " + result.getError());
            }
            else
            {
                Log::info("ShareToMessageDialog: Sent to channel successfully");
            }

            // Check if all sends are complete
            if (safeThis->sendProgress >= safeThis->sendTotal)
            {
                safeThis->isSending = false;
                safeThis->closeDialog();
                if (safeThis->onShareComplete)
                    safeThis->onShareComplete();
            }
            else
            {
                safeThis->repaint();
            }
        });
}

void ShareToMessageDialog::createAndSendToUser(const juce::String& userId)
{
    if (!streamChatClient)
        return;

    juce::Component::SafePointer<ShareToMessageDialog> safeThis(this);

    // Create direct channel with user, then send
    streamChatClient->createDirectChannel(userId, [safeThis, userId](Outcome<StreamChatClient::Channel> result) {
        if (safeThis == nullptr) return;

        if (result.isError())
        {
            Log::error("ShareToMessageDialog: Failed to create channel with user - " + result.getError());
            safeThis->sendProgress++;

            if (safeThis->sendProgress >= safeThis->sendTotal)
            {
                safeThis->isSending = false;
                safeThis->closeDialog();
                if (safeThis->onShareComplete)
                    safeThis->onShareComplete();
            }
            return;
        }

        auto channel = result.getValue();
        safeThis->sendToChannel(channel.type, channel.id);
    });
}

//==============================================================================
juce::Rectangle<int> ShareToMessageDialog::getContentBounds() const
{
    auto dialogBounds = getLocalBounds().withSizeKeepingCentre(DIALOG_WIDTH, DIALOG_HEIGHT);
    dialogBounds.removeFromTop(HEADER_HEIGHT - 20);
    dialogBounds.removeFromTop(PREVIEW_HEIGHT);
    dialogBounds.removeFromTop(INPUT_HEIGHT); // Search input
    dialogBounds.removeFromBottom(BUTTON_HEIGHT + 20);
    return dialogBounds.reduced(PADDING, 5);
}

juce::Rectangle<int> ShareToMessageDialog::getItemBounds(int index) const
{
    auto contentBounds = getContentBounds();
    int y = contentBounds.getY() - static_cast<int>(scrollOffset) + 25 + index * ITEM_HEIGHT;
    return juce::Rectangle<int>(contentBounds.getX(), y, contentBounds.getWidth() - 12, ITEM_HEIGHT - 4);
}

int ShareToMessageDialog::getItemIndexAt(juce::Point<int> pos) const
{
    auto contentBounds = getContentBounds();
    if (!contentBounds.contains(pos))
        return -1;

    int totalItems = searchResults.empty() ? (int)recentChannels.size() : (int)searchResults.size();
    for (int i = 0; i < totalItems; ++i)
    {
        if (getItemBounds(i).contains(pos))
            return i;
    }
    return -1;
}

juce::String ShareToMessageDialog::getChannelDisplayName(const StreamChatClient::Channel& channel) const
{
    // For direct messages, show the other user's name
    if (channel.type == "messaging" && channel.members.isArray())
    {
        for (int i = 0; i < channel.members.size(); ++i)
        {
            auto member = channel.members[i];
            if (member.isObject())
            {
                auto user = member.getProperty("user", juce::var());
                if (user.isObject())
                {
                    juce::String memberId = user.getProperty("id", "").toString();
                    if (memberId != currentUserId)
                    {
                        juce::String name = user.getProperty("name", "").toString();
                        if (name.isEmpty())
                            name = user.getProperty("username", "").toString();
                        if (!name.isEmpty())
                            return name;
                    }
                }
            }
        }
    }

    // Fall back to channel name
    if (channel.name.isNotEmpty())
        return channel.name;

    return "Conversation";
}
