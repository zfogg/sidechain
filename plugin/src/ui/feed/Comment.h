#pragma once

#include "../../util/Colors.h"
#include "../../util/HoverState.h"
#include "../../util/Result.h"
#include "../../util/Time.h"
#include "../../stores/AppStore.h"
#include <JuceHeader.h>

//==============================================================================
/**
 * Comment represents a single comment on a post
 *
 * Maps to the Comment model from the backend
 */
struct Comment {
  juce::String id;
  juce::String postId; // The post this comment belongs to
  juce::String userId;
  juce::String username;
  juce::String userAvatarUrl;
  juce::String content;
  juce::String parentId; // For threaded replies (empty for top-level)
  juce::Time createdAt;
  juce::String timeAgo; // Human-readable time (e.g., "2h ago")
  int likeCount = 0;
  bool isLiked = false;
  bool isOwnComment = false; // Whether current user authored this comment
  bool canEdit = false;      // Within 5-minute edit window

  // Parse from JSON response
  static Comment fromJson(const juce::var &json) {
    Comment comment;
    if (json.isObject()) {
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
      if (createdAtStr.isNotEmpty()) {
        comment.createdAt = juce::Time::fromISO8601(createdAtStr);
        comment.timeAgo = TimeUtils::formatTimeAgoShort(comment.createdAt);
      }
    }
    return comment;
  }

  bool isValid() const {
    return id.isNotEmpty() && content.isNotEmpty();
  }
};

//==============================================================================
/**
 * CommentRow displays a single comment
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
class CommentRow : public juce::Component {
public:
  CommentRow();
  ~CommentRow() override = default;

  //==============================================================================
  // Data binding
  void setComment(const Comment &comment);
  const Comment &getComment() const {
    return comment;
  }
  juce::String getCommentId() const {
    return comment.id;
  }

  // Set whether this is a reply (indented)
  void setIsReply(bool reply) {
    isReply = reply;
    repaint();
  }

  // Update like state
  void updateLikeCount(int count, bool liked);

  //==============================================================================
  // Callbacks
  std::function<void(const Comment &)> onUserClicked;
  std::function<void(const Comment &, bool liked)> onLikeToggled;
  std::function<void(const Comment &)> onReplyClicked;
  std::function<void(const Comment &)> onEditClicked;
  std::function<void(const Comment &)> onDeleteClicked;
  std::function<void(const Comment &)> onReportClicked;

  //==============================================================================
  // Component overrides
  void paint(juce::Graphics &g) override;
  void resized() override;
  void mouseUp(const juce::MouseEvent &event) override;
  void mouseEnter(const juce::MouseEvent &event) override;
  void mouseExit(const juce::MouseEvent &event) override;

  //==============================================================================
  // Layout constants
  static constexpr int ROW_HEIGHT = 80;
  static constexpr int REPLY_ROW_HEIGHT = 70;
  static constexpr int AVATAR_SIZE = 36;
  static constexpr int REPLY_INDENT = 40;

private:
  Comment comment;
  HoverState hoverState;
  bool isReply = false;

  // Cached avatar image
  juce::Image avatarImage;

  //==============================================================================
  // Drawing methods
  void drawAvatar(juce::Graphics &g, juce::Rectangle<int> bounds);
  void drawUserInfo(juce::Graphics &g, juce::Rectangle<int> bounds);
  void drawContent(juce::Graphics &g, juce::Rectangle<int> bounds);
  void drawActions(juce::Graphics &g, juce::Rectangle<int> bounds);

  //==============================================================================
  // Hit testing helpers
  juce::Rectangle<int> getAvatarBounds() const;
  juce::Rectangle<int> getUserInfoBounds() const;
  juce::Rectangle<int> getContentBounds() const;
  juce::Rectangle<int> getLikeButtonBounds() const;
  juce::Rectangle<int> getReplyButtonBounds() const;
  juce::Rectangle<int> getMoreButtonBounds() const;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CommentRow)
};

//==============================================================================
// Forward declarations
class NetworkClient;

//==============================================================================
/**
 * CommentsPanel displays a full comments section for a post
 *
 * Features:
 * - List of comments with infinite scroll
 * - Text input field for new comments
 * - Reply threading (1 level deep)
 * - Like/unlike on comments
 * - Edit/delete own comments
 * - Real-time updates via CommentStore (reactive pattern)
 */
class CommentsPanel : public juce::Component, private juce::Timer {
public:
  CommentsPanel();
  ~CommentsPanel() override;

  //==============================================================================
  // Setup
  void setNetworkClient(NetworkClient *client) {
    networkClient = client;
  }
  void setAppStore(Sidechain::Stores::AppStore *store) {
    appStore = store;
  }
  void setCurrentUserId(const juce::String &userId) {
    currentUserId = userId;
  }

  // Load comments for a post
  void loadCommentsForPost(const juce::String &postId);
  void refreshComments();

  // Get current post ID
  juce::String getCurrentPostId() const {
    return currentPostId;
  }

  //==============================================================================
  // Callbacks
  std::function<void()> onClose;
  std::function<void(const juce::String &userId)> onUserClicked;

  //==============================================================================
  // Component overrides
  void paint(juce::Graphics &g) override;
  void resized() override;
  void mouseUp(const juce::MouseEvent &event) override;
  bool keyPressed(const juce::KeyPress &key) override;

  //==============================================================================
  // Layout constants
  static constexpr int HEADER_HEIGHT = 50;
  static constexpr int INPUT_HEIGHT = 60;

private:
  //==============================================================================
  // Timer callback for auto-refresh
  void timerCallback() override;

  //==============================================================================
  // Store subscription callback
  void onCommentStoreChanged();

  //==============================================================================
  // Data
  NetworkClient *networkClient = nullptr;
  Sidechain::Stores::AppStore *appStore = nullptr;
  juce::String currentPostId;
  juce::String currentUserId;
  juce::Array<Comment> comments;
  int totalCommentCount = 0;
  bool isLoading = false;
  bool hasMoreComments = false;
  int currentOffset = 0;
  juce::String errorMessage;

  // Reply state
  juce::String replyingToCommentId;
  juce::String replyingToUsername;

  // Edit state
  juce::String editCommentId; // ID of comment being edited (empty if creating new)

  //==============================================================================
  // UI Components
  std::unique_ptr<juce::Viewport> viewport;
  std::unique_ptr<juce::Component> contentContainer;
  juce::OwnedArray<CommentRow> commentRows;
  std::unique_ptr<juce::TextEditor> inputField;
  std::unique_ptr<juce::TextButton> sendButton;
  std::unique_ptr<juce::TextButton> emojiButton;
  std::unique_ptr<juce::TextButton> closeButton;

  // Mention autocomplete
  std::unique_ptr<juce::Component> mentionAutocompletePanel;
  juce::Array<juce::String> mentionSuggestions; // Array of usernames
  juce::Array<juce::String> mentionUserIds;     // Corresponding user IDs
  int selectedMentionIndex = -1;
  bool isShowingMentions = false;
  int mentionQueryStart = -1; // Position in text where @mention starts

  //==============================================================================
  // Methods
  void setupUI();
  void updateCommentsList();
  void loadMoreComments();
  void submitComment();
  void cancelReply();

  // Mention autocomplete
  void checkForMention();
  void showMentionAutocomplete(const juce::String &query);
  void hideMentionAutocomplete();
  void selectMention(int index);
  void insertMention(const juce::String &username);

  // Emoji picker
  void showEmojiPicker();
  void insertEmoji(const juce::String &emoji);

  // API callbacks
  void handleCommentsLoaded(Outcome<std::pair<juce::var, int>> commentsResult);
  void handleCommentCreated(Outcome<juce::var> commentResult);
  void handleCommentDeleted(bool success, const juce::String &commentId);
  void handleCommentLikeToggled(const Comment &comment, bool liked);

  // Row callbacks
  void setupRowCallbacks(CommentRow *row);

  // TextEditor listener for mention detection
  class MentionListener : public juce::TextEditor::Listener {
  public:
    MentionListener(CommentsPanel *parentPanel) : parent(parentPanel) {}
    void textEditorTextChanged(juce::TextEditor &editor) override;
    void textEditorReturnKeyPressed(juce::TextEditor &editor) override;

  private:
    CommentsPanel *parent;
  };
  std::unique_ptr<MentionListener> mentionListener;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CommentsPanel)
};
