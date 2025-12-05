#include "CommentComponent.h"

//==============================================================================
CommentRowComponent::CommentRowComponent()
{
    setSize(400, ROW_HEIGHT);
}

//==============================================================================
void CommentRowComponent::setComment(const Comment& newComment)
{
    comment = newComment;
    avatarLoadRequested = false;
    avatarImage = juce::Image();
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
    g.setColour(isHovered ? Colors::backgroundHover : Colors::background);
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
    // Create circular clipping
    juce::Path circlePath;
    circlePath.addEllipse(bounds.toFloat());

    g.saveState();
    g.reduceClipRegion(circlePath);

    if (avatarImage.isValid())
    {
        auto scaledImage = avatarImage.rescaled(bounds.getWidth(), bounds.getHeight(),
                                                 juce::Graphics::highResamplingQuality);
        g.drawImageAt(scaledImage, bounds.getX(), bounds.getY());
    }
    else
    {
        // Placeholder with initial
        g.setColour(juce::Colour::fromRGB(70, 70, 70));
        g.fillEllipse(bounds.toFloat());

        g.setColour(juce::Colours::white);
        g.setFont(14.0f);
        juce::String initial = comment.username.isEmpty() ? "?" : comment.username.substring(0, 1).toUpperCase();
        g.drawText(initial, bounds, juce::Justification::centred);
    }

    g.restoreState();

    // Avatar border
    g.setColour(juce::Colour::fromRGB(80, 80, 80));
    g.drawEllipse(bounds.toFloat(), 1.0f);
}

void CommentRowComponent::drawUserInfo(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    // Username
    g.setColour(Colors::textPrimary);
    g.setFont(13.0f);
    auto usernameWidth = juce::GlyphArrangement::getStringWidth(g.getCurrentFont(), comment.username);
    g.drawText(comment.username.isEmpty() ? "Unknown" : comment.username,
               bounds.getX(), bounds.getY(), static_cast<int>(usernameWidth) + 5, 18,
               juce::Justification::centredLeft);

    // Timestamp (after username)
    g.setColour(Colors::textMuted);
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
    g.setColour(Colors::textPrimary);
    g.setFont(13.0f);

    // Draw comment text, wrapping if needed
    g.drawFittedText(comment.content, bounds, juce::Justification::topLeft, 3, 1.0f);
}

void CommentRowComponent::drawActions(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    int buttonX = bounds.getX();

    // Like button
    auto likeBounds = getLikeButtonBounds();
    juce::Colour likeColor = comment.isLiked ? Colors::liked : Colors::textMuted;
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
    g.setColour(Colors::textMuted);
    g.setFont(11.0f);
    g.drawText("Reply", replyBounds, juce::Justification::centredLeft);

    // More button (for own comments or to report)
    if (isHovered)
    {
        auto moreBounds = getMoreButtonBounds();
        g.setColour(Colors::textMuted);
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
