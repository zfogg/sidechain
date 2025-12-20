#pragma once

#include "../../models/Comment.h" // Use model from models folder
#include "../../util/HoverState.h"
#include "../../util/Result.h"
#include "../../stores/AppStore.h"
#include "../common/AppStoreComponent.h"
#include <JuceHeader.h>

namespace Sidechain {
namespace Stores {
class AppStore;
}
} // namespace Sidechain

// Type alias for backwards compatibility - UI code uses Comment model from models/
using UIComment = Sidechain::Comment;

// ==============================================================================
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

  // ==============================================================================
  // Data binding

  // Set comment from shared_ptr (preferred - keeps strong reference)
  void setComment(const std::shared_ptr<Sidechain::Comment> &comment);

  // Set comment from model reference (creates internal copy)
  void setComment(const Sidechain::Comment &comment);

  const Sidechain::Comment *getComment() const {
    if (commentPtr) {
      return commentPtr.get();
    }
    return nullptr;
  }

  juce::String getCommentId() const {
    if (commentPtr) {
      return commentPtr->id;
    }
    return "";
  }

  // Set whether this is a reply (indented)
  void setIsReply(bool reply) {
    isReply = reply;
    repaint();
  }

  // Update like state
  void updateLikeCount(int count, bool liked);

  // Set the app store for image caching
  void setAppStore(Sidechain::Stores::AppStore *store) {
    appStore = store;
  }

  // ==============================================================================
  // Callbacks
  std::function<void(const Sidechain::Comment &)> onUserClicked;
  std::function<void(const Sidechain::Comment &, bool liked)> onLikeToggled;
  std::function<void(const Sidechain::Comment &)> onReplyClicked;
  std::function<void(const Sidechain::Comment &)> onEditClicked;
  std::function<void(const Sidechain::Comment &)> onDeleteClicked;
  std::function<void(const Sidechain::Comment &)> onReportClicked;

  // ==============================================================================
  // Component overrides
  void paint(juce::Graphics &g) override;
  void resized() override;
  void mouseUp(const juce::MouseEvent &event) override;
  void mouseEnter(const juce::MouseEvent &event) override;
  void mouseExit(const juce::MouseEvent &event) override;

  // ==============================================================================
  // Layout constants
  static constexpr int ROW_HEIGHT = 80;
  static constexpr int REPLY_ROW_HEIGHT = 70;
  static constexpr int AVATAR_SIZE = 36;
  static constexpr int REPLY_INDENT = 40;

private:
  std::shared_ptr<Sidechain::Comment> commentPtr; // Strong reference to comment model
  HoverState hoverState;
  bool isReply = false;
  Sidechain::Stores::AppStore *appStore = nullptr;

  // ==============================================================================
  // Drawing methods
  void drawAvatar(juce::Graphics &g, juce::Rectangle<int> bounds);
  void drawUserInfo(juce::Graphics &g, juce::Rectangle<int> bounds);
  void drawContent(juce::Graphics &g, juce::Rectangle<int> bounds);
  void drawActions(juce::Graphics &g, juce::Rectangle<int> bounds);

  // ==============================================================================
  // Hit testing helpers
  juce::Rectangle<int> getAvatarBounds() const;
  juce::Rectangle<int> getUserInfoBounds() const;
  juce::Rectangle<int> getContentBounds() const;
  juce::Rectangle<int> getLikeButtonBounds() const;
  juce::Rectangle<int> getReplyButtonBounds() const;
  juce::Rectangle<int> getMoreButtonBounds() const;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CommentRow)
};

// ==============================================================================
// Forward declarations
class NetworkClient;

// ==============================================================================
/**
 * CommentsPanel displays a full comments section for a post
 *
 * Features:
 * - List of comments with infinite scroll (models from EntityStore)
 * - Text input field for new comments
 * - Reply threading (1 level deep)
 * - Like/unlike on comments
 * - Edit/delete own comments
 * - Real-time updates via AppStore subscriptions
 *
 * Architecture:
 * - Extends AppStoreComponent<CommentsState>
 * - Subscribes to comments for current post
 * - Comments delivered as shared_ptr<Comment> models
 * - No manual JSON parsing - all done in AppStore
 */
class CommentsPanel : public Sidechain::UI::AppStoreComponent<Sidechain::Stores::CommentsState>, private juce::Timer {
public:
  explicit CommentsPanel(Sidechain::Stores::AppStore *store = nullptr);
  ~CommentsPanel() override;

  // ==============================================================================
  // Setup

  void setCurrentUserId(const juce::String &userId) {
    currentUserId = userId;
  }

  // Load comments for a post (delegates to AppStore)
  void loadCommentsForPost(const juce::String &postId);

  // Refresh comments from cache/network
  void refreshComments();

  // Get current post ID
  juce::String getCurrentPostId() const {
    return currentPostId;
  }

  // ==============================================================================
  // Callbacks
  std::function<void()> onClose;
  std::function<void(const juce::String &userId)> onUserClicked;

  // ==============================================================================
  // Component overrides
  void paint(juce::Graphics &g) override;
  void resized() override;
  void mouseUp(const juce::MouseEvent &event) override;
  bool keyPressed(const juce::KeyPress &key) override;

  // ==============================================================================
  // Layout constants
  static constexpr int HEADER_HEIGHT = 50;
  static constexpr int INPUT_HEIGHT = 60;

private:
  // ==============================================================================
  // Timer callback for auto-refresh
  void timerCallback() override;

  // ==============================================================================
  // Store subscription callback
  void onCommentStoreChanged();

  // ==============================================================================
  // Data
  // Note: appStore is inherited from AppStoreComponent base class
  juce::String currentPostId;
  juce::String currentUserId;
  std::vector<std::shared_ptr<Sidechain::Comment>> comments;
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

  // ==============================================================================
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

  // ==============================================================================
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

  // Auto-test comment submission (for debugging)
  void autoSubmitTestComment();
  bool autoTestCommentSubmitted = false;

  // Emoji picker
  void showEmojiPicker();
  void insertEmoji(const juce::String &emoji);

  // API callbacks
  void handleCommentsLoaded(Outcome<std::pair<juce::var, int>> commentsResult);
  void handleCommentCreated(Outcome<juce::var> commentResult);
  void handleCommentDeleted(bool success, const juce::String &commentId);
  void handleCommentLikeToggled(const Sidechain::Comment &comment, bool liked);

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

  // ==============================================================================
  // AppStoreComponent<CommentsState> implementation
protected:
  void subscribeToAppStore() override;
  void onAppStateChanged(const Sidechain::Stores::CommentsState &state) override;

private:
  // Unsubscriber function from AppStore subscription
  std::function<void()> commentsUnsubscriber;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CommentsPanel)
};
