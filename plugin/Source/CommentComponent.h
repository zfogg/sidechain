#pragma once

#include <JuceHeader.h>
#include "TimeUtils.h"

//==============================================================================
/**
 * Comment represents a single comment on a post
 *
 * Maps to the Comment model from the backend
 */
struct Comment
{
    juce::String id;
    juce::String postId;          // The post this comment belongs to
    juce::String userId;
    juce::String username;
    juce::String userAvatarUrl;
    juce::String content;
    juce::String parentId;        // For threaded replies (empty for top-level)
    juce::Time createdAt;
    juce::String timeAgo;         // Human-readable time (e.g., "2h ago")
    int likeCount = 0;
    bool isLiked = false;
    bool isOwnComment = false;    // Whether current user authored this comment
    bool canEdit = false;         // Within 5-minute edit window

    // Parse from JSON response
    static Comment fromJson(const juce::var& json)
    {
        Comment comment;
        if (json.isObject())
        {
            comment.id = json.getProperty("id", "").toString();
            comment.postId = json.getProperty("post_id", "").toString();
            comment.userId = json.getProperty("user_id", "").toString();
            comment.username = json.getProperty("username", "").toString();
            comment.userAvatarUrl = json.getProperty("avatar_url", "").toString();
            if (comment.userAvatarUrl.isEmpty())
                comment.userAvatarUrl = json.getProperty("profile_picture_url", "").toString();
            comment.content = json.getProperty("content", "").toString();
            comment.parentId = json.getProperty("parent_id", "").toString();
            comment.likeCount = static_cast<int>(json.getProperty("like_count", 0));
            comment.isLiked = static_cast<bool>(json.getProperty("is_liked", false));
            comment.isOwnComment = static_cast<bool>(json.getProperty("is_own_comment", false));
            comment.canEdit = static_cast<bool>(json.getProperty("can_edit", false));

            // Parse timestamp
            auto createdAtStr = json.getProperty("created_at", "").toString();
            if (createdAtStr.isNotEmpty())
            {
                comment.createdAt = juce::Time::fromISO8601(createdAtStr);
                comment.timeAgo = TimeUtils::formatTimeAgoShort(comment.createdAt);
            }
        }
        return comment;
    }

    bool isValid() const
    {
        return id.isNotEmpty() && content.isNotEmpty();
    }
};

//==============================================================================
/**
 * CommentRowComponent displays a single comment
 *
 * Features:
 * - User avatar with circular clip and fallback to initials
 * - Username and relative timestamp
 * - Comment text content
 * - Like button with count
 * - Reply button
 * - Edit/delete menu for own comments
 * - Indentation for replies (1 level deep)
 */
class CommentRowComponent : public juce::Component
{
public:
    CommentRowComponent();
    ~CommentRowComponent() override = default;

    //==============================================================================
    // Data binding
    void setComment(const Comment& comment);
    const Comment& getComment() const { return comment; }
    juce::String getCommentId() const { return comment.id; }

    // Set whether this is a reply (indented)
    void setIsReply(bool reply) { isReply = reply; repaint(); }

    // Update like state
    void updateLikeCount(int count, bool liked);

    //==============================================================================
    // Callbacks
    std::function<void(const Comment&)> onUserClicked;
    std::function<void(const Comment&, bool liked)> onLikeToggled;
    std::function<void(const Comment&)> onReplyClicked;
    std::function<void(const Comment&)> onEditClicked;
    std::function<void(const Comment&)> onDeleteClicked;
    std::function<void(const Comment&)> onReportClicked;

    //==============================================================================
    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseEnter(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;

    //==============================================================================
    // Layout constants
    static constexpr int ROW_HEIGHT = 80;
    static constexpr int REPLY_ROW_HEIGHT = 70;
    static constexpr int AVATAR_SIZE = 36;
    static constexpr int REPLY_INDENT = 40;

private:
    Comment comment;
    bool isHovered = false;
    bool isReply = false;

    // Cached avatar image
    juce::Image avatarImage;
    bool avatarLoadRequested = false;

    //==============================================================================
    // Drawing methods
    void drawAvatar(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawUserInfo(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawContent(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawActions(juce::Graphics& g, juce::Rectangle<int> bounds);

    //==============================================================================
    // Hit testing helpers
    juce::Rectangle<int> getAvatarBounds() const;
    juce::Rectangle<int> getUserInfoBounds() const;
    juce::Rectangle<int> getContentBounds() const;
    juce::Rectangle<int> getLikeButtonBounds() const;
    juce::Rectangle<int> getReplyButtonBounds() const;
    juce::Rectangle<int> getMoreButtonBounds() const;

    //==============================================================================
    // Colors (matching app theme)
    struct Colors
    {
        static inline juce::Colour background { 0xff2d2d32 };
        static inline juce::Colour backgroundHover { 0xff3a3a3e };
        static inline juce::Colour textPrimary { 0xffffffff };
        static inline juce::Colour textSecondary { 0xffa0a0a0 };
        static inline juce::Colour textMuted { 0xff707070 };
        static inline juce::Colour accent { 0xff00d4ff };
        static inline juce::Colour liked { 0xffff5050 };
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CommentRowComponent)
};
