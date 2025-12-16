#include "Comment.h"
#include "../../network/NetworkClient.h"
#include "../../stores/AppStore.h"

#include "../../ui/common/ToastNotification.h"
#include "../../ui/feed/EmojiReactionsPanel.h"
#include "../../util/Async.h"
#include "../../util/Emoji.h"
#include "../../util/HoverState.h"
#include "../../util/Log.h"
#include "../../util/Result.h"
#include "../../util/StringFormatter.h"
#include "../../util/TextEditorStyler.h"
#include "../../util/UIHelpers.h"

using namespace Sidechain::Stores;

//==============================================================================
CommentRow::CommentRow() {
  Log::debug("CommentRow: Initializing comment row");
  setSize(400, ROW_HEIGHT);

  // Set up hover state
  hoverState.onHoverChanged = [this]([[maybe_unused]] bool hovered) { repaint(); };
}

//==============================================================================
void CommentRow::setComment(const Comment &newComment) {
  comment = newComment;

  // Fetch avatar image via AppStore (with caching)
  if (comment.userAvatarUrl.isNotEmpty() && appStore) {
    appStore->getImage(comment.userAvatarUrl, [this](const juce::Image &) { repaint(); });
  }

  repaint();
}

void CommentRow::updateLikeCount(int count, bool liked) {
  Log::debug("CommentRow::updateLikeCount: Updating like count - id: " + comment.id +
             ", count: " + juce::String(count) + ", liked: " + juce::String(liked ? "yes" : "no"));
  comment.likeCount = count;
  comment.isLiked = liked;
  repaint();
}

//==============================================================================
void CommentRow::paint(juce::Graphics &g) {
  // Background - use UIHelpers::drawCardWithHover for consistent styling
  UIHelpers::drawCardWithHover(g, getLocalBounds(), SidechainColors::backgroundLight(),
                               SidechainColors::backgroundLighter(), juce::Colours::transparentBlack,
                               hoverState.isHovered());

  // Draw avatar (indentation handled in getAvatarBounds)
  drawAvatar(g, getAvatarBounds());

  // Draw user info (name + timestamp)
  drawUserInfo(g, getUserInfoBounds());

  // Draw comment content
  drawContent(g, getContentBounds());

  // Draw action buttons (like, reply, more)
  drawActions(g, juce::Rectangle<int>(getAvatarBounds().getRight() + 10, getHeight() - 24,
                                      getWidth() - getAvatarBounds().getRight() - 20, 20));
}

void CommentRow::drawAvatar(juce::Graphics &g, juce::Rectangle<int> bounds) {
  // Use unified getImage() - handles all three cache levels automatically (memory -> file -> HTTP)
  if (appStore && comment.userAvatarUrl.isNotEmpty()) {
    appStore->getImage(comment.userAvatarUrl, [this](const juce::Image &) { repaint(); });
  }

  // Draw placeholder circle (will be replaced with actual image when loaded)
  g.setColour(SidechainColors::surface());
  g.fillEllipse(bounds.toFloat());

  // Avatar border
  g.setColour(SidechainColors::border());
  g.drawEllipse(bounds.toFloat(), 1.0f);
}

void CommentRow::drawUserInfo(juce::Graphics &g, juce::Rectangle<int> bounds) {
  // Username
  g.setColour(SidechainColors::textPrimary());
  g.setFont(13.0f);
  auto usernameWidth = juce::GlyphArrangement::getStringWidth(g.getCurrentFont(), comment.username);
  g.drawText(comment.username.isEmpty() ? "Unknown" : comment.username, bounds.getX(), bounds.getY(),
             static_cast<int>(usernameWidth) + 5, 18, juce::Justification::centredLeft);

  // Timestamp (after username)
  g.setColour(SidechainColors::textMuted());
  g.setFont(11.0f);
  g.drawText(comment.timeAgo, bounds.getX() + static_cast<int>(usernameWidth) + 8, bounds.getY(), 60, 18,
             juce::Justification::centredLeft);

  // "Edited" indicator if applicable
  if (comment.canEdit && comment.isOwnComment) {
    // Show within edit window indicator (subtle)
  }
}

void CommentRow::drawContent(juce::Graphics &g, juce::Rectangle<int> bounds) {
  g.setColour(SidechainColors::textPrimary());
  g.setFont(13.0f);

  // Draw comment text, wrapping if needed
  g.drawFittedText(comment.content, bounds, juce::Justification::topLeft, 3, 1.0f);
}

void CommentRow::drawActions(juce::Graphics &g, [[maybe_unused]] juce::Rectangle<int> bounds) {
  // Like button
  auto likeBounds = getLikeButtonBounds();
  juce::Colour likeColor = comment.isLiked ? SidechainColors::like() : SidechainColors::textMuted();
  g.setColour(likeColor);
  g.setFont(12.0f);

  juce::String heartIcon = comment.isLiked ? juce::String(juce::CharPointer_UTF8("\xE2\x99\xA5")) : // Filled heart
                               juce::String(juce::CharPointer_UTF8("\xE2\x99\xA1"));                // Empty heart
  g.drawText(heartIcon, likeBounds.withWidth(16), juce::Justification::centredLeft);

  // Like count
  if (comment.likeCount > 0) {
    g.drawText(StringFormatter::formatCount(comment.likeCount), likeBounds.withX(likeBounds.getX() + 18).withWidth(25),
               juce::Justification::centredLeft);
  }

  // Reply button
  auto replyBounds = getReplyButtonBounds();
  g.setColour(SidechainColors::textMuted());
  g.setFont(11.0f);
  g.drawText("Reply", replyBounds, juce::Justification::centredLeft);

  // More button (for own comments or to report)
  if (hoverState.isHovered()) {
    auto moreBounds = getMoreButtonBounds();
    g.setColour(SidechainColors::textMuted());
    g.setFont(14.0f);
    g.drawText("...", moreBounds, juce::Justification::centred);
  }
}

//==============================================================================
void CommentRow::resized() {
  // Layout is handled in paint() using bounds calculations
}

void CommentRow::mouseUp(const juce::MouseEvent &event) {
  auto pos = event.getPosition();
  Log::debug("CommentRow::mouseUp: Mouse clicked at (" + juce::String(pos.x) + ", " + juce::String(pos.y) +
             ") on comment: " + comment.id);

  // Check avatar/username for user click
  if (getAvatarBounds().contains(pos) || getUserInfoBounds().contains(pos)) {
    Log::info("CommentRow::mouseUp: User clicked on avatar/username");
    if (onUserClicked)
      onUserClicked(comment);
    return;
  }

  // Check like button
  if (getLikeButtonBounds().contains(pos)) {
    bool willBeLiked = !comment.isLiked;
    Log::info("CommentRow::mouseUp: Like button clicked - will be liked: " + juce::String(willBeLiked ? "yes" : "no"));
    if (onLikeToggled)
      onLikeToggled(comment, willBeLiked);
    return;
  }

  // Check reply button
  if (getReplyButtonBounds().contains(pos)) {
    Log::info("CommentRow::mouseUp: Reply button clicked");
    if (onReplyClicked)
      onReplyClicked(comment);
    return;
  }

  // Check more button
  if (getMoreButtonBounds().contains(pos)) {
    Log::info("CommentRow::mouseUp: More button clicked, showing context menu");
    // Show context menu
    juce::PopupMenu menu;

    if (comment.isOwnComment) {
      if (comment.canEdit)
        menu.addItem(1, "Edit");
      menu.addItem(2, "Delete");
    } else {
      menu.addItem(3, "Report");
    }

    menu.showMenuAsync(juce::PopupMenu::Options(), [this](int result) {
      if (result == 1 && onEditClicked) {
        Log::info("CommentRow::mouseUp: Edit menu item selected");
        onEditClicked(comment);
      } else if (result == 2 && onDeleteClicked) {
        Log::info("CommentRow::mouseUp: Delete menu item selected");
        onDeleteClicked(comment);
      } else if (result == 3 && onReportClicked) {
        Log::info("CommentRow::mouseUp: Report menu item selected");
        onReportClicked(comment);
      }
    });
    return;
  }
}

void CommentRow::mouseEnter(const juce::MouseEvent & /*event*/) {
  hoverState.setHovered(true);
}

void CommentRow::mouseExit(const juce::MouseEvent & /*event*/) {
  hoverState.setHovered(false);
}

//==============================================================================
juce::Rectangle<int> CommentRow::getAvatarBounds() const {
  int indent = isReply ? REPLY_INDENT : 0;
  int avatarSize = isReply ? AVATAR_SIZE - 4 : AVATAR_SIZE;
  return juce::Rectangle<int>(indent + 12, 10, avatarSize, avatarSize);
}

juce::Rectangle<int> CommentRow::getUserInfoBounds() const {
  auto avatar = getAvatarBounds();
  return juce::Rectangle<int>(avatar.getRight() + 10, 10, getWidth() - avatar.getRight() - 50, 18);
}

juce::Rectangle<int> CommentRow::getContentBounds() const {
  auto avatar = getAvatarBounds();
  int rowHeight = isReply ? REPLY_ROW_HEIGHT : ROW_HEIGHT;
  return juce::Rectangle<int>(avatar.getRight() + 10, 30, getWidth() - avatar.getRight() - 25, rowHeight - 55);
}

juce::Rectangle<int> CommentRow::getLikeButtonBounds() const {
  auto avatar = getAvatarBounds();
  int rowHeight = isReply ? REPLY_ROW_HEIGHT : ROW_HEIGHT;
  return juce::Rectangle<int>(avatar.getRight() + 10, rowHeight - 22, 45, 18);
}

juce::Rectangle<int> CommentRow::getReplyButtonBounds() const {
  auto likeBounds = getLikeButtonBounds();
  return juce::Rectangle<int>(likeBounds.getRight() + 15, likeBounds.getY(), 40, 18);
}

juce::Rectangle<int> CommentRow::getMoreButtonBounds() const {
  int rowHeight = isReply ? REPLY_ROW_HEIGHT : ROW_HEIGHT;
  return juce::Rectangle<int>(getWidth() - 30, rowHeight - 22, 20, 18);
}

//==============================================================================
//==============================================================================
// CommentsPanel Implementation
//==============================================================================
//==============================================================================

CommentsPanel::CommentsPanel() {
  setupUI();
}

CommentsPanel::~CommentsPanel() {
  stopTimer();
}

void CommentsPanel::setupUI() {
  // Close button - styled as a prominent X button
  closeButton = std::make_unique<juce::TextButton>("X");
  closeButton->setColour(juce::TextButton::buttonColourId, SidechainColors::backgroundLighter());
  closeButton->setColour(juce::TextButton::buttonOnColourId, SidechainColors::surface());
  closeButton->setColour(juce::TextButton::textColourOffId, SidechainColors::textPrimary());
  closeButton->setColour(juce::TextButton::textColourOnId, SidechainColors::textPrimary());
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

  // Input field with TextEditorStyler
  inputField = std::make_unique<juce::TextEditor>();
  TextEditorStyler::style(*inputField, "Add a comment...");
  inputField->setMultiLine(true); // Allow multiline for longer comments
  inputField->setReturnKeyStartsNewLine(true);
  inputField->setInputRestrictions(1000); // Max comment length
  inputField->onReturnKey = [this]() {
    // If showing mentions, select first mention; otherwise submit
    if (isShowingMentions && mentionSuggestions.size() > 0) {
      selectMention(0);
    } else {
      submitComment();
    }
  };

  // Add mention listener
  mentionListener = std::make_unique<MentionListener>(this);
  inputField->addListener(mentionListener.get());
  addAndMakeVisible(inputField.get());

  // Emoji button
  emojiButton = std::make_unique<juce::TextButton>(Emoji::SMILING_FACE_WITH_SMILING_EYES);
  emojiButton->onClick = [this]() { showEmojiPicker(); };
  addAndMakeVisible(emojiButton.get());

  // Send button
  sendButton = std::make_unique<juce::TextButton>("Send");
  sendButton->onClick = [this]() { submitComment(); };
  addAndMakeVisible(sendButton.get());

  // Mention autocomplete panel (initially hidden)
  mentionAutocompletePanel = std::make_unique<juce::Component>();
  mentionAutocompletePanel->setVisible(false);
  addChildComponent(mentionAutocompletePanel.get());
}

void CommentsPanel::loadCommentsForPost(const juce::String &postId) {
  if (postId.isEmpty()) {
    Log::warn("CommentsPanel::loadCommentsForPost: Cannot load - postId empty");
    return;
  }

  Log::info("CommentsPanel::loadCommentsForPost: Loading comments for post: " + postId);
  currentPostId = postId;

  if (networkClient != nullptr) {
    currentOffset = 0;
    comments.clear();
    commentRows.clear();
    errorMessage = "";
    isLoading = true;
    repaint();

    networkClient->getComments(postId, 20, 0, [this](Outcome<std::pair<juce::var, int>> commentsResult) {
      handleCommentsLoaded(commentsResult);
    });
  } else {
    Log::warn("CommentsPanel::loadCommentsForPost: No NetworkClient available");
  }
}

void CommentsPanel::refreshComments() {
  if (currentPostId.isEmpty())
    return;

  loadCommentsForPost(currentPostId);
}

void CommentsPanel::onCommentStoreChanged() {
  // No-op: State updates happen via NetworkClient callbacks
}

void CommentsPanel::handleCommentsLoaded(Outcome<std::pair<juce::var, int>> commentsResult) {
  isLoading = false;

  if (commentsResult.isOk()) {
    auto [commentsData, total] = commentsResult.getValue();
    if (commentsData.isArray()) {
      auto *arr = commentsData.getArray();
      if (arr != nullptr) {
        for (const auto &item : *arr) {
          Comment comment = Comment::fromJson(item);
          if (comment.isValid())
            comments.add(comment);
        }
      }

      totalCommentCount = total;
      hasMoreComments = comments.size() < total;
      currentOffset = comments.size();
      updateCommentsList();
    } else {
      errorMessage = "Invalid comments response";
    }
  } else {
    errorMessage = "Failed to load comments: " + commentsResult.getError();
  }

  repaint();
}

void CommentsPanel::loadMoreComments() {
  if (isLoading || !hasMoreComments || networkClient == nullptr)
    return;

  isLoading = true;
  repaint();

  networkClient->getComments(currentPostId, 20, currentOffset,
                             [this](Outcome<std::pair<juce::var, int>> commentsResult) {
                               isLoading = false;

                               if (commentsResult.isOk()) {
                                 auto [commentsData, total] = commentsResult.getValue();
                                 if (commentsData.isArray()) {
                                   auto *arr = commentsData.getArray();
                                   if (arr != nullptr) {
                                     for (const auto &item : *arr) {
                                       Comment comment = Comment::fromJson(item);
                                       if (comment.isValid())
                                         comments.add(comment);
                                     }
                                   }

                                   totalCommentCount = total;
                                   hasMoreComments = comments.size() < total;
                                   currentOffset = comments.size();
                                   updateCommentsList();
                                 } else {
                                   errorMessage = "Invalid comments response";
                                 }
                               } else {
                                 errorMessage = "Failed to load more comments: " + commentsResult.getError();
                               }

                               repaint();
                             });
}

void CommentsPanel::updateCommentsList() {
  commentRows.clear();

  int yPos = 0;
  for (const auto &comment : comments) {
    auto *row = new CommentRow();
    row->setComment(comment);
    row->setIsReply(comment.parentId.isNotEmpty());
    setupRowCallbacks(row);

    int rowHeight = comment.parentId.isNotEmpty() ? CommentRow::REPLY_ROW_HEIGHT : CommentRow::ROW_HEIGHT;
    row->setBounds(0, yPos, contentContainer->getWidth(), rowHeight);
    contentContainer->addAndMakeVisible(row);
    commentRows.add(row);

    yPos += rowHeight;
  }

  contentContainer->setSize(viewport->getWidth() - 10, yPos);
}

void CommentsPanel::setupRowCallbacks(CommentRow *row) {
  row->onUserClicked = [this](const Comment &comment) {
    if (onUserClicked)
      onUserClicked(comment.userId);
  };

  row->onLikeToggled = [this](const Comment &comment, bool liked) { handleCommentLikeToggled(comment, liked); };

  row->onReplyClicked = [this](const Comment &comment) {
    replyingToCommentId = comment.id;
    replyingToUsername = comment.username;
    inputField->setText("@" + comment.username + " ");
    inputField->grabKeyboardFocus();
    repaint();
  };

  row->onEditClicked = [this](const Comment &comment) {
    // Handle edit comment action.
    // Set edit state and populate input with existing content
    editCommentId = comment.id;
    inputField->setText(comment.content);
    inputField->grabKeyboardFocus();
    // Clear reply state when editing
    cancelReply();
    repaint();
  };

  // Comment operations are fully implemented:
  // - Comment editing: wired up in submitComment() (line 775-806)
  // - Comment deletion: wired up in onDeleteClicked (line 552)
  // - Comment likes: wired up in handleCommentLikeToggled (line 640-683)
  // - Comment reporting: wired up in onReportClicked (line 561-604) - uses
  // NetworkClient::reportComment()

  row->onDeleteClicked = [this](const Comment &comment) {
    if (networkClient == nullptr)
      return;

    // Confirm delete
    auto options = juce::MessageBoxOptions()
                       .withTitle("Delete Comment")
                       .withMessage("Are you sure you want to delete this comment?")
                       .withButton("Delete")
                       .withButton("Cancel");

    juce::AlertWindow::showAsync(options, [this, commentId = comment.id](int result) {
      if (result == 1 && networkClient != nullptr) {
        networkClient->deleteComment(commentId, [this, commentId](Outcome<juce::var> responseOutcome) {
          handleCommentDeleted(responseOutcome.isOk(), commentId);
        });
      }
    });
  };

  row->onReportClicked = [this](const Comment &comment) {
    // Handle report comment action.
    Log::info("CommentsPanel::setupRowCallbacks: Report comment clicked - "
              "commentId: " +
              comment.id);

    auto options = juce::MessageBoxOptions()
                       .withTitle("Report Comment")
                       .withMessage("Why are you reporting this comment?")
                       .withButton("Spam")
                       .withButton("Harassment")
                       .withButton("Inappropriate")
                       .withButton("Other")
                       .withButton("Cancel");

    juce::AlertWindow::showAsync(options, [this, comment](int reportResult) {
      if (reportResult >= 1 && reportResult <= 4 && networkClient != nullptr) {
        juce::String reasons[] = {"spam", "harassment", "inappropriate", "other"};
        juce::String reason = reasons[reportResult - 1];
        juce::String description = "Reported comment: " + comment.content.substring(0, 100);

        networkClient->reportComment(comment.id, reason, description, [](Outcome<juce::var> result) {
          if (result.isOk()) {
            juce::MessageManager::callAsync([]() {
              juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, "Report Submitted",
                                                     "Thank you for reporting this comment. We will review it "
                                                     "shortly.");
            });
          } else {
            Log::error("CommentsPanel: Failed to report comment - " + result.getError());
            juce::MessageManager::callAsync([result]() {
              juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Error",
                                                     "Failed to report comment: " + result.getError());
            });
          }
        });
      }
    });
  };
}

void CommentsPanel::handleCommentLikeToggled(const Comment &comment, bool liked) {
  Log::info("CommentsPanel::handleCommentLikeToggled: Toggling like - commentId: " + comment.id +
            ", liked: " + juce::String(liked ? "yes" : "no"));

  // Use NetworkClient directly
  if (networkClient == nullptr) {
    Log::warn("CommentsPanel::handleCommentLikeToggled: No networkClient available");
    return;
  }

  Log::debug("CommentsPanel::handleCommentLikeToggled: Using NetworkClient fallback");

  // Optimistic update
  for (auto *row : commentRows) {
    if (row->getCommentId() == comment.id) {
      int newCount = liked ? comment.likeCount + 1 : juce::jmax(0, comment.likeCount - 1);
      row->updateLikeCount(newCount, liked);
      Log::debug("CommentsPanel::handleCommentLikeToggled: Optimistic update - "
                 "new count: " +
                 juce::String(newCount));
      break;
    }
  }

  // Send to server
  if (liked) {
    Log::debug("CommentsPanel::handleCommentLikeToggled: Calling likeComment API");
    networkClient->likeComment(comment.id, [this, commentId = comment.id](Outcome<juce::var> responseOutcome) {
      if (responseOutcome.isError()) {
        Log::warn("CommentsPanel::handleCommentLikeToggled: Like failed, "
                  "reverting optimistic update");
        // Revert on failure
        for (auto *row : commentRows) {
          if (row->getCommentId() == commentId) {
            auto c = row->getComment();
            row->updateLikeCount(c.likeCount, c.isLiked);
            break;
          }
        }
        // Show toast notification for transient error
        ToastManager::getInstance().showError("Couldn't update like. Please try again.");
      } else {
        Log::debug("CommentsPanel::handleCommentLikeToggled: Like successful");
      }
    });
  } else {
    Log::debug("CommentsPanel::handleCommentLikeToggled: Calling unlikeComment API");
    networkClient->unlikeComment(comment.id, [this, commentId = comment.id](Outcome<juce::var> responseOutcome) {
      if (responseOutcome.isError()) {
        Log::warn("CommentsPanel::handleCommentLikeToggled: Unlike failed, "
                  "reverting optimistic update");
        // Revert on failure
        for (auto *row : commentRows) {
          if (row->getCommentId() == commentId) {
            auto c = row->getComment();
            row->updateLikeCount(c.likeCount, c.isLiked);
            break;
          }
        }
        // Show toast notification for transient error
        ToastManager::getInstance().showError("Couldn't update like. Please try again.");
      } else {
        Log::debug("CommentsPanel::handleCommentLikeToggled: Unlike successful");
      }
    });
  }
}

void CommentsPanel::handleCommentCreated(Outcome<juce::var> commentResult) {
  if (commentResult.isOk()) {
    auto commentData = commentResult.getValue();
    Log::info("CommentsPanel::handleCommentCreated: Comment creation successful");

    Comment newComment = Comment::fromJson(commentData);
    if (newComment.isValid()) {
      Log::info("CommentsPanel::handleCommentCreated: Adding new comment - id: " + newComment.id +
                ", username: " + newComment.username);
      comments.insert(0, newComment); // Add at top
      totalCommentCount++;
      updateCommentsList();
    } else {
      Log::warn("CommentsPanel::handleCommentCreated: Comment data invalid");
    }

    inputField->clear();
    cancelReply();
    Log::debug("CommentsPanel::handleCommentCreated: Input cleared, reply cancelled");
  } else {
    errorMessage = "Failed to post comment: " + commentResult.getError();
    Log::error("CommentsPanel::handleCommentCreated: Failed to post comment - " + commentResult.getError());
    // Show toast notification for transient error
    ToastManager::getInstance().showError("Couldn't post comment. Please try again.");
  }

  repaint();
}

void CommentsPanel::handleCommentDeleted(bool success, const juce::String &commentId) {
  Log::info("CommentsPanel::handleCommentDeleted: Comment deletion result - "
            "success: " +
            juce::String(success ? "yes" : "no") + ", commentId: " + commentId);

  if (success) {
    // Remove from list
    for (int i = comments.size() - 1; i >= 0; --i) {
      if (comments[i].id == commentId) {
        Log::debug("CommentsPanel::handleCommentDeleted: Removing comment from list");
        comments.remove(i);
        totalCommentCount--;
        break;
      }
    }
    updateCommentsList();
  } else {
    Log::error("CommentsPanel::handleCommentDeleted: Failed to delete comment");
  }

  repaint();
}

void CommentsPanel::submitComment() {
  if (networkClient == nullptr || currentPostId.isEmpty()) {
    Log::warn("CommentsPanel::submitComment: Cannot submit - networkClient "
              "null or postId empty");
    return;
  }

  juce::String content = inputField->getText().trim();
  if (content.isEmpty()) {
    Log::debug("CommentsPanel::submitComment: Content is empty, not submitting");
    return;
  }

  // Check if we're editing an existing comment
  if (editCommentId.isNotEmpty()) {
    Log::info("CommentsPanel::submitComment: Updating comment - commentId: " + editCommentId +
              ", content length: " + juce::String(content.length()));

    networkClient->updateComment(editCommentId, content, [this](Outcome<juce::var> commentResult) {
      if (commentResult.isOk()) {
        auto commentData = commentResult.getValue();
        Comment updatedComment = Comment::fromJson(commentData);
        if (updatedComment.isValid()) {
          // Update the comment in the list
          for (int i = 0; i < comments.size(); ++i) {
            if (comments[i].id == updatedComment.id) {
              comments.set(i, updatedComment);
              updateCommentsList();
              break;
            }
          }
        }
        inputField->clear();
        editCommentId = ""; // Clear edit state
        cancelReply();
        Log::info("CommentsPanel::submitComment: Comment updated successfully");
      } else {
        Log::error("CommentsPanel::submitComment: Failed to update comment - " + commentResult.getError());
        // Show toast notification for transient error
        ToastManager::getInstance().showError("Couldn't update comment. Please try again.");
      }
      repaint();
    });
    return;
  }

  // Determine if this is a reply
  juce::String parentId = replyingToCommentId;

  Log::info("CommentsPanel::submitComment: Submitting comment - postId: " + currentPostId +
            ", content length: " + juce::String(content.length()) +
            (parentId.isNotEmpty() ? (", replying to: " + parentId) : ", top-level comment"));

  networkClient->createComment(currentPostId, content, parentId,
                               [this](Outcome<juce::var> commentResult) { handleCommentCreated(commentResult); });
}

void CommentsPanel::cancelReply() {
  replyingToCommentId = "";
  replyingToUsername = "";
  // Also clear edit state when canceling reply
  editCommentId = "";
}

void CommentsPanel::timerCallback() {
  // Auto-refresh every 30 seconds
  Log::debug("CommentsPanel::timerCallback: Auto-refreshing comments");
  refreshComments();
}

void CommentsPanel::paint(juce::Graphics &g) {
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
    title += " (" + StringFormatter::formatCount(totalCommentCount) + ")";
  g.drawText(title, headerBounds.withTrimmedLeft(15), juce::Justification::centredLeft);

  // Reply indicator
  if (replyingToUsername.isNotEmpty()) {
    auto inputBounds = getLocalBounds().removeFromBottom(INPUT_HEIGHT);
    auto replyBounds = inputBounds.removeFromTop(20);

    g.setColour(SidechainColors::accent().withAlpha(0.2f));
    g.fillRect(replyBounds);

    g.setColour(SidechainColors::textSecondary());
    g.setFont(11.0f);
    g.drawText("Replying to @" + replyingToUsername + "  [Cancel]", replyBounds.withTrimmedLeft(10),
               juce::Justification::centredLeft);
  }

  // Loading indicator
  if (isLoading) {
    g.setColour(SidechainColors::textMuted());
    g.setFont(12.0f);
    g.drawText("Loading...", getLocalBounds(), juce::Justification::centred);
  }

  // Error message
  if (errorMessage.isNotEmpty()) {
    g.setColour(SidechainColors::buttonDanger());
    g.setFont(12.0f);
    g.drawText(errorMessage, getLocalBounds(), juce::Justification::centred);
  }

  // Empty state
  if (!isLoading && comments.isEmpty() && errorMessage.isEmpty()) {
    g.setColour(SidechainColors::textMuted());
    g.setFont(14.0f);
    g.drawText("No comments yet. Be the first!", getLocalBounds(), juce::Justification::centred);
  }

  // Draw mention autocomplete panel
  if (isShowingMentions && mentionAutocompletePanel != nullptr && mentionAutocompletePanel->isVisible()) {
    auto panelBounds = mentionAutocompletePanel->getBounds();

    // Background
    g.setColour(SidechainColors::backgroundLight());
    g.fillRoundedRectangle(panelBounds.toFloat(), 8.0f);

    // Border
    g.setColour(SidechainColors::border());
    g.drawRoundedRectangle(panelBounds.toFloat(), 8.0f, 1.0f);

    // Draw suggestions
    int yPos = 5;
    for (int i = 0; i < mentionSuggestions.size(); ++i) {
      auto itemBounds = juce::Rectangle<int>(5, yPos, panelBounds.getWidth() - 10, 35);

      // Highlight selected item
      if (i == selectedMentionIndex) {
        g.setColour(SidechainColors::surface());
        g.fillRoundedRectangle(itemBounds.toFloat(), 4.0f);
      }

      // Username
      g.setColour(SidechainColors::textPrimary());
      g.setFont(13.0f);
      g.drawText("@" + mentionSuggestions[i], itemBounds.reduced(10, 0), juce::Justification::centredLeft);

      yPos += 35;
    }
  }
}

void CommentsPanel::resized() {
  auto bounds = getLocalBounds();

  // Close button
  closeButton->setBounds(bounds.getWidth() - 45, 10, 30, 30);

  // Input area at bottom
  auto inputBounds = bounds.removeFromBottom(INPUT_HEIGHT);

  // Account for reply indicator
  if (replyingToUsername.isNotEmpty())
    inputBounds.removeFromTop(20);

  // Button area (emoji + send)
  auto buttonArea = inputBounds.removeFromRight(90);
  sendButton->setBounds(buttonArea.removeFromRight(70).reduced(5));
  emojiButton->setBounds(buttonArea.removeFromRight(30).reduced(5));

  inputField->setBounds(inputBounds.reduced(10, 15));

  // Position mention autocomplete panel above input field
  if (isShowingMentions && mentionAutocompletePanel != nullptr) {
    int panelHeight = juce::jmin(200, mentionSuggestions.size() * 40 + 10);
    mentionAutocompletePanel->setBounds(inputBounds.getX(), inputBounds.getY() - panelHeight - 5,
                                        inputBounds.getWidth(), panelHeight);
  }

  // Header at top
  bounds.removeFromTop(HEADER_HEIGHT);

  // Viewport fills the rest
  viewport->setBounds(bounds);
  contentContainer->setSize(viewport->getWidth() - 10, contentContainer->getHeight());
  updateCommentsList();
}

//==============================================================================
// Mouse and Keyboard Handling

void CommentsPanel::mouseUp(const juce::MouseEvent &event) {
  auto pos = event.getPosition();

  // Check if clicking on mention autocomplete
  if (isShowingMentions && mentionAutocompletePanel != nullptr && mentionAutocompletePanel->isVisible()) {
    auto panelBounds = mentionAutocompletePanel->getBounds();
    if (panelBounds.contains(pos)) {
      int relativeY = pos.y - panelBounds.getY();
      int index = (relativeY - 5) / 35;
      if (index >= 0 && index < mentionSuggestions.size()) {
        selectMention(index);
        return;
      }
    } else {
      // Clicked outside, hide autocomplete
      hideMentionAutocomplete();
    }
  }
}

bool CommentsPanel::keyPressed(const juce::KeyPress &key) {
  if (!isShowingMentions || mentionSuggestions.isEmpty())
    return false;

  // Arrow keys for navigation
  if (key == juce::KeyPress::upKey) {
    selectedMentionIndex = selectedMentionIndex > 0 ? selectedMentionIndex - 1 : mentionSuggestions.size() - 1;
    repaint();
    return true;
  } else if (key == juce::KeyPress::downKey) {
    selectedMentionIndex = (selectedMentionIndex + 1) % mentionSuggestions.size();
    repaint();
    return true;
  } else if (key == juce::KeyPress::escapeKey) {
    hideMentionAutocomplete();
    return true;
  } else if (key == juce::KeyPress::returnKey || key == juce::KeyPress::tabKey) {
    if (selectedMentionIndex >= 0 && selectedMentionIndex < mentionSuggestions.size()) {
      selectMention(selectedMentionIndex);
      return true;
    }
  }

  return false;
}

//==============================================================================
// Mention Autocomplete Implementation

void CommentsPanel::MentionListener::textEditorTextChanged([[maybe_unused]] juce::TextEditor &editor) {
  if (parent != nullptr)
    parent->checkForMention();
}

void CommentsPanel::MentionListener::textEditorReturnKeyPressed([[maybe_unused]] juce::TextEditor &editor) {
  // Handled in onReturnKey callback
}

void CommentsPanel::checkForMention() {
  if (inputField == nullptr || networkClient == nullptr)
    return;

  juce::String text = inputField->getText();
  int caretPos = inputField->getCaretPosition();

  // Find the @ symbol before the caret
  int atPos = -1;
  for (int i = caretPos - 1; i >= 0; --i) {
    if (text[i] == '@') {
      atPos = i;
      break;
    }
    // Stop if we hit a space or newline (not part of mention)
    if (text[i] == ' ' || text[i] == '\n')
      break;
  }

  if (atPos == -1) {
    // No @ found, hide autocomplete
    hideMentionAutocomplete();
    return;
  }

  // Check if there's a space after @ (not a mention)
  if (atPos < text.length() - 1 && text[atPos + 1] == ' ') {
    hideMentionAutocomplete();
    return;
  }

  // Extract the query after @
  int queryStart = atPos + 1;
  int queryEnd = caretPos;

  // Find end of query (space or end of text)
  for (int i = caretPos; i < text.length(); ++i) {
    if (text[i] == ' ' || text[i] == '\n') {
      queryEnd = i;
      break;
    }
  }

  juce::String query = text.substring(queryStart, queryEnd);

  // Only show autocomplete if query is at least 1 character or empty (show
  // suggestions)
  if (query.length() >= 0) {
    mentionQueryStart = atPos;
    showMentionAutocomplete(query);
  } else {
    hideMentionAutocomplete();
  }
}

void CommentsPanel::showMentionAutocomplete(const juce::String &query) {
  if (networkClient == nullptr)
    return;

  isShowingMentions = true;
  mentionSuggestions.clear();
  mentionUserIds.clear();
  selectedMentionIndex = -1;

  // Search for users via AppStore (with caching)
  juce::String searchQuery = query.isEmpty() ? "" : query;
  auto &storeRef = AppStore::getInstance();

  storeRef.searchUsersObservable(searchQuery)
      .subscribe(
          [this](const juce::Array<juce::var> &users) {
            // On next - users found
            mentionSuggestions.clear();
            mentionUserIds.clear();

            for (const auto &user : users) {
              if (user.isObject()) {
                juce::String username = user.getProperty("username", "").toString();
                juce::String userId = user.getProperty("id", "").toString();

                if (username.isNotEmpty() && userId.isNotEmpty()) {
                  mentionSuggestions.add(username);
                  mentionUserIds.add(userId);
                }
              }
            }

            if (mentionSuggestions.size() > 0) {
              selectedMentionIndex = 0;
              mentionAutocompletePanel->setVisible(true);
              resized(); // Update panel position
              repaint();
            } else {
              hideMentionAutocomplete();
            }
          },
          [this](std::exception_ptr) {
            // On error - hide mention panel
            Log::error("CommentsPanel: Search users failed");
            hideMentionAutocomplete();
          });
}

void CommentsPanel::hideMentionAutocomplete() {
  isShowingMentions = false;
  mentionSuggestions.clear();
  mentionUserIds.clear();
  selectedMentionIndex = -1;
  mentionQueryStart = -1;

  if (mentionAutocompletePanel != nullptr)
    mentionAutocompletePanel->setVisible(false);

  repaint();
}

void CommentsPanel::selectMention(int index) {
  if (index < 0 || index >= mentionSuggestions.size() || inputField == nullptr)
    return;

  juce::String username = mentionSuggestions[index];
  juce::String text = inputField->getText();

  // Replace @query with @username
  if (mentionQueryStart >= 0 && mentionQueryStart < text.length()) {
    int queryEnd = mentionQueryStart + 1;
    while (queryEnd < text.length() && text[queryEnd] != ' ' && text[queryEnd] != '\n')
      queryEnd++;

    text = text.substring(0, mentionQueryStart + 1) + username + " " + text.substring(queryEnd);
    inputField->setText(text);
    inputField->setCaretPosition(mentionQueryStart + 1 + username.length() + 1);
  }

  hideMentionAutocomplete();
}

void CommentsPanel::insertMention(const juce::String &username) {
  if (inputField == nullptr)
    return;

  juce::String text = inputField->getText();
  int caretPos = inputField->getCaretPosition();

  text = text.substring(0, caretPos) + "@" + username + " " + text.substring(caretPos);
  inputField->setText(text);
  inputField->setCaretPosition(caretPos + username.length() + 2);
}

//==============================================================================
// Emoji Picker Implementation

void CommentsPanel::showEmojiPicker() {
  if (inputField == nullptr)
    return;

  // Create emoji picker bubble similar to EmojiReactionsBubble
  auto *bubble = new EmojiReactionsBubble(this);

  // Handle emoji selection
  bubble->onEmojiSelected = [this](const juce::String &emoji) { insertEmoji(emoji); };

  // Position near the emoji button
  bubble->show();
}

void CommentsPanel::insertEmoji(const juce::String &emoji) {
  if (inputField == nullptr)
    return;

  juce::String text = inputField->getText();
  int caretPos = inputField->getCaretPosition();

  text = text.substring(0, caretPos) + emoji + text.substring(caretPos);
  inputField->setText(text);
  inputField->setCaretPosition(caretPos + emoji.length());
  inputField->grabKeyboardFocus();
}

//==============================================================================
// Helper functions for image loading and avatar rendering

static juce::Image loadImageFromURL(const juce::String &urlStr) {
  if (urlStr.isEmpty()) {
    return juce::Image();
  }

  try {
    juce::URL url(urlStr);
    auto inputStream = std::unique_ptr<juce::InputStream>(url.createInputStream(false));

    if (inputStream == nullptr) {
      Log::error("loadImageFromURL: Failed to create input stream from URL: " + urlStr);
      return juce::Image();
    }

    auto image = juce::ImageFileFormat::loadFrom(*inputStream);
    if (!image.isValid()) {
      Log::error("loadImageFromURL: Failed to parse image from URL: " + urlStr);
      return juce::Image();
    }

    Log::debug("loadImageFromURL: Successfully loaded image from: " + urlStr);
    return image;
  } catch (const std::exception &e) {
    Log::error("loadImageFromURL: Exception loading image from URL: " + juce::String(e.what()));
    return juce::Image();
  }
}

static juce::String getInitialsFromName(const juce::String &name) {
  if (name.isEmpty()) {
    return "?";
  }

  juce::StringArray parts;
  parts.addTokens(name, " ", "");

  if (parts.size() >= 2) {
    // Get first letter of first and last name
    return (parts[0].substring(0, 1) + parts[parts.size() - 1].substring(0, 1)).toUpperCase();
  } else if (parts.size() == 1) {
    // Get first two letters of single word name
    juce::String initials = parts[0].substring(0, 2).toUpperCase();
    return initials.length() > 0 ? initials : "?";
  }

  return "?";
}
