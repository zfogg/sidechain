#include "CommentComponent.h"
#include "../../util/Colors.h"
#include "../../util/ImageCache.h"
#include "../../network/NetworkClient.h"

//==============================================================================
CommentRowComponent::CommentRowComponent()
{
    setSize(400, ROW_HEIGHT);
}

//==============================================================================
void CommentRowComponent::setComment(const Comment& newComment)
{
    comment = newComment;
    avatarImage = juce::Image();

    // Load avatar via ImageCache
    if (comment.userAvatarUrl.isNotEmpty())
    {
        ImageLoader::load(comment.userAvatarUrl, [this](const juce::Image& img) {
            avatarImage = img;
            repaint();
        });
    }

    repaint();
}

void CommentRowComponent::updateLikeCount(int count, bool liked)
{
    comment.likeCount = count;
    comment.isLiked = liked;
    repaint();
}

//==============================================================================
void CommentRowComponent::paint(juce::Graphics& g)
{
    // Background
    g.setColour(isHovered ? SidechainColors::backgroundLighter() : SidechainColors::backgroundLight());
    g.fillRect(getLocalBounds());

    // Calculate indent for replies
    int indent = isReply ? REPLY_INDENT : 0;
    auto contentArea = getLocalBounds().withTrimmedLeft(indent);

    // Draw avatar
    drawAvatar(g, getAvatarBounds());

    // Draw user info (name + timestamp)
    drawUserInfo(g, getUserInfoBounds());

    // Draw comment content
    drawContent(g, getContentBounds());

    // Draw action buttons (like, reply, more)
    drawActions(g, juce::Rectangle<int>(
        getAvatarBounds().getRight() + 10,
        getHeight() - 24,
        getWidth() - getAvatarBounds().getRight() - 20,
        20
    ));
}

void CommentRowComponent::drawAvatar(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    ImageLoader::drawCircularAvatar(g, bounds, avatarImage,
        ImageLoader::getInitials(comment.username),
        SidechainColors::surface(),
        SidechainColors::textPrimary());

    // Avatar border
    g.setColour(SidechainColors::border());
    g.drawEllipse(bounds.toFloat(), 1.0f);
}

void CommentRowComponent::drawUserInfo(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    // Username
    g.setColour(SidechainColors::textPrimary());
    g.setFont(13.0f);
    auto usernameWidth = juce::GlyphArrangement::getStringWidth(g.getCurrentFont(), comment.username);
    g.drawText(comment.username.isEmpty() ? "Unknown" : comment.username,
               bounds.getX(), bounds.getY(), static_cast<int>(usernameWidth) + 5, 18,
               juce::Justification::centredLeft);

    // Timestamp (after username)
    g.setColour(SidechainColors::textMuted());
    g.setFont(11.0f);
    g.drawText(comment.timeAgo,
               bounds.getX() + static_cast<int>(usernameWidth) + 8, bounds.getY(), 60, 18,
               juce::Justification::centredLeft);

    // "Edited" indicator if applicable
    if (comment.canEdit && comment.isOwnComment)
    {
        // Show within edit window indicator (subtle)
    }
}

void CommentRowComponent::drawContent(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    g.setColour(SidechainColors::textPrimary());
    g.setFont(13.0f);

    // Draw comment text, wrapping if needed
    g.drawFittedText(comment.content, bounds, juce::Justification::topLeft, 3, 1.0f);
}

void CommentRowComponent::drawActions(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    int buttonX = bounds.getX();

    // Like button
    auto likeBounds = getLikeButtonBounds();
    juce::Colour likeColor = comment.isLiked ? SidechainColors::like() : SidechainColors::textMuted();
    g.setColour(likeColor);
    g.setFont(12.0f);

    juce::String heartIcon = comment.isLiked ?
        juce::String(juce::CharPointer_UTF8("\xE2\x99\xA5")) :  // Filled heart
        juce::String(juce::CharPointer_UTF8("\xE2\x99\xA1"));   // Empty heart
    g.drawText(heartIcon, likeBounds.withWidth(16), juce::Justification::centredLeft);

    // Like count
    if (comment.likeCount > 0)
    {
        g.drawText(juce::String(comment.likeCount),
                   likeBounds.withX(likeBounds.getX() + 18).withWidth(25),
                   juce::Justification::centredLeft);
    }

    // Reply button
    auto replyBounds = getReplyButtonBounds();
    g.setColour(SidechainColors::textMuted());
    g.setFont(11.0f);
    g.drawText("Reply", replyBounds, juce::Justification::centredLeft);

    // More button (for own comments or to report)
    if (isHovered)
    {
        auto moreBounds = getMoreButtonBounds();
        g.setColour(SidechainColors::textMuted());
        g.setFont(14.0f);
        g.drawText("...", moreBounds, juce::Justification::centred);
    }
}

//==============================================================================
void CommentRowComponent::resized()
{
    // Layout is handled in paint() using bounds calculations
}

void CommentRowComponent::mouseUp(const juce::MouseEvent& event)
{
    auto pos = event.getPosition();

    // Check avatar/username for user click
    if (getAvatarBounds().contains(pos) || getUserInfoBounds().contains(pos))
    {
        if (onUserClicked)
            onUserClicked(comment);
        return;
    }

    // Check like button
    if (getLikeButtonBounds().contains(pos))
    {
        bool willBeLiked = !comment.isLiked;
        if (onLikeToggled)
            onLikeToggled(comment, willBeLiked);
        return;
    }

    // Check reply button
    if (getReplyButtonBounds().contains(pos))
    {
        if (onReplyClicked)
            onReplyClicked(comment);
        return;
    }

    // Check more button
    if (getMoreButtonBounds().contains(pos))
    {
        // Show context menu
        juce::PopupMenu menu;

        if (comment.isOwnComment)
        {
            if (comment.canEdit)
                menu.addItem(1, "Edit");
            menu.addItem(2, "Delete");
        }
        else
        {
            menu.addItem(3, "Report");
        }

        menu.showMenuAsync(juce::PopupMenu::Options(),
            [this](int result) {
                if (result == 1 && onEditClicked)
                    onEditClicked(comment);
                else if (result == 2 && onDeleteClicked)
                    onDeleteClicked(comment);
                else if (result == 3 && onReportClicked)
                    onReportClicked(comment);
            });
        return;
    }
}

void CommentRowComponent::mouseEnter(const juce::MouseEvent& /*event*/)
{
    isHovered = true;
    repaint();
}

void CommentRowComponent::mouseExit(const juce::MouseEvent& /*event*/)
{
    isHovered = false;
    repaint();
}

//==============================================================================
juce::Rectangle<int> CommentRowComponent::getAvatarBounds() const
{
    int indent = isReply ? REPLY_INDENT : 0;
    int avatarSize = isReply ? AVATAR_SIZE - 4 : AVATAR_SIZE;
    return juce::Rectangle<int>(indent + 12, 10, avatarSize, avatarSize);
}

juce::Rectangle<int> CommentRowComponent::getUserInfoBounds() const
{
    auto avatar = getAvatarBounds();
    return juce::Rectangle<int>(avatar.getRight() + 10, 10, getWidth() - avatar.getRight() - 50, 18);
}

juce::Rectangle<int> CommentRowComponent::getContentBounds() const
{
    auto avatar = getAvatarBounds();
    int rowHeight = isReply ? REPLY_ROW_HEIGHT : ROW_HEIGHT;
    return juce::Rectangle<int>(avatar.getRight() + 10, 30, getWidth() - avatar.getRight() - 25, rowHeight - 55);
}

juce::Rectangle<int> CommentRowComponent::getLikeButtonBounds() const
{
    auto avatar = getAvatarBounds();
    int rowHeight = isReply ? REPLY_ROW_HEIGHT : ROW_HEIGHT;
    return juce::Rectangle<int>(avatar.getRight() + 10, rowHeight - 22, 45, 18);
}

juce::Rectangle<int> CommentRowComponent::getReplyButtonBounds() const
{
    auto likeBounds = getLikeButtonBounds();
    return juce::Rectangle<int>(likeBounds.getRight() + 15, likeBounds.getY(), 40, 18);
}

juce::Rectangle<int> CommentRowComponent::getMoreButtonBounds() const
{
    int rowHeight = isReply ? REPLY_ROW_HEIGHT : ROW_HEIGHT;
    return juce::Rectangle<int>(getWidth() - 30, rowHeight - 22, 20, 18);
}

//==============================================================================
//==============================================================================
// CommentsPanelComponent Implementation
//==============================================================================
//==============================================================================

CommentsPanelComponent::CommentsPanelComponent()
{
    setupUI();
}

CommentsPanelComponent::~CommentsPanelComponent()
{
    stopTimer();
}

void CommentsPanelComponent::setupUI()
{
    // Close button
    closeButton = std::make_unique<juce::TextButton>("X");
    closeButton->onClick = [this]() {
        if (onClose)
            onClose();
    };
    addAndMakeVisible(closeButton.get());

    // Viewport for scrollable comments
    viewport = std::make_unique<juce::Viewport>();
    contentContainer = std::make_unique<juce::Component>();
    viewport->setViewedComponent(contentContainer.get(), false);
    viewport->setScrollBarsShown(true, false);
    addAndMakeVisible(viewport.get());

    // Input field
    inputField = std::make_unique<juce::TextEditor>();
    inputField->setMultiLine(false);
    inputField->setReturnKeyStartsNewLine(false);
    inputField->setTextToShowWhenEmpty("Add a comment...", SidechainColors::textMuted());
    inputField->setColour(juce::TextEditor::backgroundColourId, SidechainColors::surface());
    inputField->setColour(juce::TextEditor::textColourId, SidechainColors::textPrimary());
    inputField->setColour(juce::TextEditor::outlineColourId, SidechainColors::border());
    inputField->onReturnKey = [this]() { submitComment(); };
    addAndMakeVisible(inputField.get());

    // Send button
    sendButton = std::make_unique<juce::TextButton>("Send");
    sendButton->onClick = [this]() { submitComment(); };
    addAndMakeVisible(sendButton.get());
}

void CommentsPanelComponent::loadCommentsForPost(const juce::String& postId)
{
    if (postId.isEmpty() || networkClient == nullptr)
        return;

    currentPostId = postId;
    currentOffset = 0;
    comments.clear();
    commentRows.clear();
    errorMessage = "";
    isLoading = true;
    repaint();

    networkClient->getComments(postId, 20, 0,
        [this](bool success, const juce::var& commentsData, int total) {
            handleCommentsLoaded(success, commentsData, total);
        });
}

void CommentsPanelComponent::refreshComments()
{
    if (currentPostId.isEmpty())
        return;

    loadCommentsForPost(currentPostId);
}

void CommentsPanelComponent::handleCommentsLoaded(bool success, const juce::var& commentsData, int total)
{
    isLoading = false;

    if (success && commentsData.isArray())
    {
        auto* arr = commentsData.getArray();
        if (arr != nullptr)
        {
            for (const auto& item : *arr)
            {
                Comment comment = Comment::fromJson(item);
                if (comment.isValid())
                    comments.add(comment);
            }
        }

        totalCommentCount = total;
        hasMoreComments = comments.size() < total;
        currentOffset = comments.size();
        updateCommentsList();
    }
    else
    {
        errorMessage = "Failed to load comments";
    }

    repaint();
}

void CommentsPanelComponent::loadMoreComments()
{
    if (isLoading || !hasMoreComments || networkClient == nullptr)
        return;

    isLoading = true;
    repaint();

    networkClient->getComments(currentPostId, 20, currentOffset,
        [this](bool success, const juce::var& commentsData, int total) {
            isLoading = false;

            if (success && commentsData.isArray())
            {
                auto* arr = commentsData.getArray();
                if (arr != nullptr)
                {
                    for (const auto& item : *arr)
                    {
                        Comment comment = Comment::fromJson(item);
                        if (comment.isValid())
                            comments.add(comment);
                    }
                }

                totalCommentCount = total;
                hasMoreComments = comments.size() < total;
                currentOffset = comments.size();
                updateCommentsList();
            }

            repaint();
        });
}

void CommentsPanelComponent::updateCommentsList()
{
    commentRows.clear();

    int yPos = 0;
    for (const auto& comment : comments)
    {
        auto* row = new CommentRowComponent();
        row->setComment(comment);
        row->setIsReply(comment.parentId.isNotEmpty());
        setupRowCallbacks(row);

        int rowHeight = comment.parentId.isNotEmpty() ?
            CommentRowComponent::REPLY_ROW_HEIGHT : CommentRowComponent::ROW_HEIGHT;
        row->setBounds(0, yPos, contentContainer->getWidth(), rowHeight);
        contentContainer->addAndMakeVisible(row);
        commentRows.add(row);

        yPos += rowHeight;
    }

    contentContainer->setSize(viewport->getWidth() - 10, yPos);
}

void CommentsPanelComponent::setupRowCallbacks(CommentRowComponent* row)
{
    row->onUserClicked = [this](const Comment& comment) {
        if (onUserClicked)
            onUserClicked(comment.userId);
    };

    row->onLikeToggled = [this](const Comment& comment, bool liked) {
        handleCommentLikeToggled(comment, liked);
    };

    row->onReplyClicked = [this](const Comment& comment) {
        replyingToCommentId = comment.id;
        replyingToUsername = comment.username;
        inputField->setText("@" + comment.username + " ");
        inputField->grabKeyboardFocus();
        repaint();
    };

    row->onEditClicked = [this](const Comment& comment) {
        // Simple edit: populate input with existing content
        inputField->setText(comment.content);
        inputField->grabKeyboardFocus();
        // TODO: Track that we're editing, not creating new
    };

    row->onDeleteClicked = [this](const Comment& comment) {
        if (networkClient == nullptr)
            return;

        // Confirm delete
        auto options = juce::MessageBoxOptions()
            .withTitle("Delete Comment")
            .withMessage("Are you sure you want to delete this comment?")
            .withButton("Delete")
            .withButton("Cancel");

        juce::AlertWindow::showAsync(options, [this, commentId = comment.id](int result) {
            if (result == 1 && networkClient != nullptr)
            {
                networkClient->deleteComment(commentId,
                    [this, commentId](bool success, const juce::var& /*response*/) {
                        handleCommentDeleted(success, commentId);
                    });
            }
        });
    };

    row->onReportClicked = [this](const Comment& /*comment*/) {
        // TODO: Implement report flow
        DBG("Report comment clicked");
    };
}

void CommentsPanelComponent::handleCommentLikeToggled(const Comment& comment, bool liked)
{
    if (networkClient == nullptr)
        return;

    // Optimistic update
    for (auto* row : commentRows)
    {
        if (row->getCommentId() == comment.id)
        {
            int newCount = liked ? comment.likeCount + 1 : juce::jmax(0, comment.likeCount - 1);
            row->updateLikeCount(newCount, liked);
            break;
        }
    }

    // Send to server
    if (liked)
    {
        networkClient->likeComment(comment.id, [this, commentId = comment.id](bool success, const juce::var& /*response*/) {
            if (!success)
            {
                // Revert on failure
                for (auto* row : commentRows)
                {
                    if (row->getCommentId() == commentId)
                    {
                        auto c = row->getComment();
                        row->updateLikeCount(c.likeCount, c.isLiked);
                        break;
                    }
                }
            }
        });
    }
    else
    {
        networkClient->unlikeComment(comment.id, [this, commentId = comment.id](bool success, const juce::var& /*response*/) {
            if (!success)
            {
                // Revert on failure
                for (auto* row : commentRows)
                {
                    if (row->getCommentId() == commentId)
                    {
                        auto c = row->getComment();
                        row->updateLikeCount(c.likeCount, c.isLiked);
                        break;
                    }
                }
            }
        });
    }
}

void CommentsPanelComponent::handleCommentCreated(bool success, const juce::var& commentData)
{
    if (success)
    {
        Comment newComment = Comment::fromJson(commentData);
        if (newComment.isValid())
        {
            comments.insert(0, newComment); // Add at top
            totalCommentCount++;
            updateCommentsList();
        }

        inputField->clear();
        cancelReply();
    }
    else
    {
        errorMessage = "Failed to post comment";
    }

    repaint();
}

void CommentsPanelComponent::handleCommentDeleted(bool success, const juce::String& commentId)
{
    if (success)
    {
        // Remove from list
        for (int i = comments.size() - 1; i >= 0; --i)
        {
            if (comments[i].id == commentId)
            {
                comments.remove(i);
                totalCommentCount--;
                break;
            }
        }
        updateCommentsList();
    }

    repaint();
}

void CommentsPanelComponent::submitComment()
{
    if (networkClient == nullptr || currentPostId.isEmpty())
        return;

    juce::String content = inputField->getText().trim();
    if (content.isEmpty())
        return;

    // Determine if this is a reply
    juce::String parentId = replyingToCommentId;

    networkClient->createComment(currentPostId, content, parentId,
        [this](bool success, const juce::var& comment) {
            handleCommentCreated(success, comment);
        });
}

void CommentsPanelComponent::cancelReply()
{
    replyingToCommentId = "";
    replyingToUsername = "";
}

void CommentsPanelComponent::timerCallback()
{
    // Auto-refresh every 30 seconds
    refreshComments();
}

void CommentsPanelComponent::paint(juce::Graphics& g)
{
    // Background
    g.fillAll(SidechainColors::background());

    // Header
    auto headerBounds = getLocalBounds().removeFromTop(HEADER_HEIGHT);
    g.setColour(SidechainColors::backgroundLight());
    g.fillRect(headerBounds);

    // Header title
    g.setColour(SidechainColors::textPrimary());
    g.setFont(16.0f);
    juce::String title = "Comments";
    if (totalCommentCount > 0)
        title += " (" + juce::String(totalCommentCount) + ")";
    g.drawText(title, headerBounds.withTrimmedLeft(15), juce::Justification::centredLeft);

    // Reply indicator
    if (replyingToUsername.isNotEmpty())
    {
        auto inputBounds = getLocalBounds().removeFromBottom(INPUT_HEIGHT);
        auto replyBounds = inputBounds.removeFromTop(20);

        g.setColour(SidechainColors::accent().withAlpha(0.2f));
        g.fillRect(replyBounds);

        g.setColour(SidechainColors::textSecondary());
        g.setFont(11.0f);
        g.drawText("Replying to @" + replyingToUsername + "  [Cancel]",
                   replyBounds.withTrimmedLeft(10), juce::Justification::centredLeft);
    }

    // Loading indicator
    if (isLoading)
    {
        g.setColour(SidechainColors::textMuted());
        g.setFont(12.0f);
        g.drawText("Loading...", getLocalBounds(), juce::Justification::centred);
    }

    // Error message
    if (errorMessage.isNotEmpty())
    {
        g.setColour(SidechainColors::buttonDanger());
        g.setFont(12.0f);
        g.drawText(errorMessage, getLocalBounds(), juce::Justification::centred);
    }

    // Empty state
    if (!isLoading && comments.isEmpty() && errorMessage.isEmpty())
    {
        g.setColour(SidechainColors::textMuted());
        g.setFont(14.0f);
        g.drawText("No comments yet. Be the first!", getLocalBounds(), juce::Justification::centred);
    }
}

void CommentsPanelComponent::resized()
{
    auto bounds = getLocalBounds();

    // Close button
    closeButton->setBounds(bounds.getWidth() - 45, 10, 30, 30);

    // Input area at bottom
    auto inputBounds = bounds.removeFromBottom(INPUT_HEIGHT);

    // Account for reply indicator
    if (replyingToUsername.isNotEmpty())
        inputBounds.removeFromTop(20);

    sendButton->setBounds(inputBounds.removeFromRight(70).reduced(5));
    inputField->setBounds(inputBounds.reduced(10, 15));

    // Header at top
    bounds.removeFromTop(HEADER_HEIGHT);

    // Viewport fills the rest
    viewport->setBounds(bounds);
    contentContainer->setSize(viewport->getWidth() - 10, contentContainer->getHeight());
    updateCommentsList();
}
