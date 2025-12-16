package service

import (
	"fmt"
	"strconv"

	"github.com/zfogg/sidechain/cli/pkg/api"
	"github.com/zfogg/sidechain/cli/pkg/logger"
)

// PostService provides post-related operations
type PostService struct{}

// NewPostService creates a new post service
func NewPostService() *PostService {
	return &PostService{}
}

// ViewPost displays a post's details
func (ps *PostService) ViewPost(postID string) error {
	logger.Debug("Viewing post", "post_id", postID)

	post, err := api.GetPost(postID)
	if err != nil {
		return fmt.Errorf("failed to fetch post: %w", err)
	}

	// Format and display post
	ps.displayPostDetails(post)
	return nil
}

// ListPosts displays the user's posts
func (ps *PostService) ListPosts(page, pageSize int) error {
	logger.Debug("Listing user posts", "page", page)

	posts, err := api.ListUserPosts(page, pageSize)
	if err != nil {
		return fmt.Errorf("failed to list posts: %w", err)
	}

	if len(posts.Posts) == 0 {
		fmt.Println("No posts found.")
		return nil
	}

	ps.displayPostList(posts)
	return nil
}

// UploadPost uploads an audio file and creates a post
func (ps *PostService) UploadPost(filePath, title, description string, bpm, key string) error {
	logger.Debug("Uploading post", "file", filePath)

	// Validate audio file first
	fmt.Printf("\n🔍 Validating audio file...\n")
	validator := NewAudioValidator()
	audioMetadata, err := validator.ValidateAudioFileWithProgress(filePath, func(msg string) {
		fmt.Printf("  • %s\n", msg)
	})
	if err != nil {
		return fmt.Errorf("audio validation failed: %w", err)
	}

	// Display extracted metadata
	audioMetadata.DisplayMetadata()

	// Build metadata from extracted data and user-provided values
	metadata := make(map[string]interface{})

	// Use user-provided BPM if given, otherwise use extracted BPM
	if bpm != "" {
		bpmInt, err := strconv.Atoi(bpm)
		if err == nil {
			metadata["bpm"] = bpmInt
		}
	} else if audioMetadata.BPM > 0 {
		metadata["bpm"] = audioMetadata.BPM
	}

	// Use user-provided key if given, otherwise use extracted key
	if key != "" {
		metadata["key"] = key
	} else if audioMetadata.Key != "" {
		metadata["key"] = audioMetadata.Key
	}

	post, err := api.UploadPost(filePath, title, description, metadata)
	if err != nil {
		return fmt.Errorf("failed to upload post: %w", err)
	}

	fmt.Printf("✓ Post uploaded successfully!\n")
	ps.displayPostDetails(post)
	return nil
}

// DeletePost deletes a post
func (ps *PostService) DeletePost(postID string) error {
	logger.Debug("Deleting post", "post_id", postID)

	if err := api.DeletePost(postID); err != nil {
		return fmt.Errorf("failed to delete post: %w", err)
	}

	fmt.Printf("✓ Post deleted successfully.\n")
	return nil
}

// ArchivePost archives a post
func (ps *PostService) ArchivePost(postID string) error {
	logger.Debug("Archiving post", "post_id", postID)

	if err := api.ArchivePost(postID); err != nil {
		return fmt.Errorf("failed to archive post: %w", err)
	}

	fmt.Printf("✓ Post archived successfully.\n")
	return nil
}

// UnarchivePost unarchives a post
func (ps *PostService) UnarchivePost(postID string) error {
	logger.Debug("Unarchiving post", "post_id", postID)

	if err := api.UnarchivePost(postID); err != nil {
		return fmt.Errorf("failed to unarchive post: %w", err)
	}

	fmt.Printf("✓ Post unarchived successfully.\n")
	return nil
}

// LikePost likes a post
func (ps *PostService) LikePost(postID string) error {
	logger.Debug("Liking post", "post_id", postID)

	if err := api.LikePost(postID); err != nil {
		return fmt.Errorf("failed to like post: %w", err)
	}

	fmt.Printf("✓ Post liked.\n")
	return nil
}

// UnlikePost unlikes a post
func (ps *PostService) UnlikePost(postID string) error {
	logger.Debug("Unliking post", "post_id", postID)

	if err := api.UnlikePost(postID); err != nil {
		return fmt.Errorf("failed to unlike post: %w", err)
	}

	fmt.Printf("✓ Like removed.\n")
	return nil
}

// ReactToPost adds an emoji reaction to a post
func (ps *PostService) ReactToPost(postID, emoji string) error {
	logger.Debug("Reacting to post", "post_id", postID, "emoji", emoji)

	if err := api.ReactToPost(postID, emoji); err != nil {
		return fmt.Errorf("failed to react to post: %w", err)
	}

	fmt.Printf("✓ Reaction added.\n")
	return nil
}

// CommentOnPost adds a comment to a post
func (ps *PostService) CommentOnPost(postID, content string) error {
	logger.Debug("Commenting on post", "post_id", postID)

	if err := api.CommentOnPost(postID, content); err != nil {
		return fmt.Errorf("failed to comment: %w", err)
	}

	fmt.Printf("✓ Comment posted.\n")
	return nil
}

// ListComments displays comments on a post
func (ps *PostService) ListComments(postID string, page, pageSize int) error {
	logger.Debug("Listing comments", "post_id", postID)

	comments, err := api.ListPostComments(postID, page, pageSize)
	if err != nil {
		return fmt.Errorf("failed to list comments: %w", err)
	}

	if len(comments.Comments) == 0 {
		fmt.Println("No comments yet.")
		return nil
	}

	ps.displayComments(comments)
	return nil
}

// SavePost saves a post
func (ps *PostService) SavePost(postID string) error {
	logger.Debug("Saving post", "post_id", postID)

	if err := api.SavePost(postID); err != nil {
		return fmt.Errorf("failed to save post: %w", err)
	}

	fmt.Printf("✓ Post saved.\n")
	return nil
}

// UnsavePost unsaves a post
func (ps *PostService) UnsavePost(postID string) error {
	logger.Debug("Unsaving post", "post_id", postID)

	if err := api.UnsavePost(postID); err != nil {
		return fmt.Errorf("failed to unsave post: %w", err)
	}

	fmt.Printf("✓ Post removed from saves.\n")
	return nil
}

// ListSavedPosts displays the user's saved posts
func (ps *PostService) ListSavedPosts(page, pageSize int) error {
	logger.Debug("Listing saved posts", "page", page)

	posts, err := api.ListSavedPosts(page, pageSize)
	if err != nil {
		return fmt.Errorf("failed to list saved posts: %w", err)
	}

	if len(posts.Posts) == 0 {
		fmt.Println("No saved posts.")
		return nil
	}

	fmt.Println("Saved Posts:")
	ps.displayPostList(posts)
	return nil
}

// PinPost pins a post to the user's profile
func (ps *PostService) PinPost(postID string) error {
	logger.Debug("Pinning post", "post_id", postID)

	if err := api.PinPost(postID); err != nil {
		return fmt.Errorf("failed to pin post: %w", err)
	}

	fmt.Printf("✓ Post pinned.\n")
	return nil
}

// UnpinPost unpins a post from the user's profile
func (ps *PostService) UnpinPost(postID string) error {
	logger.Debug("Unpinning post", "post_id", postID)

	if err := api.UnpinPost(postID); err != nil {
		return fmt.Errorf("failed to unpin post: %w", err)
	}

	fmt.Printf("✓ Post unpinned.\n")
	return nil
}

// RepostPost reposts a post
func (ps *PostService) RepostPost(postID string) error {
	logger.Debug("Reposting post", "post_id", postID)

	if err := api.RepostPost(postID); err != nil {
		return fmt.Errorf("failed to repost: %w", err)
	}

	fmt.Printf("✓ Post reposted.\n")
	return nil
}

// UnrepostPost undoes a repost
func (ps *PostService) UnrepostPost(postID string) error {
	logger.Debug("Undoing repost", "post_id", postID)

	if err := api.UnrepostPost(postID); err != nil {
		return fmt.Errorf("failed to undo repost: %w", err)
	}

	fmt.Printf("✓ Repost removed.\n")
	return nil
}

// EditComment edits a comment
func (ps *PostService) EditComment(commentID, content string) error {
	logger.Debug("Editing comment", "comment_id", commentID)

	if err := api.EditComment(commentID, content); err != nil {
		return fmt.Errorf("failed to edit comment: %w", err)
	}

	fmt.Printf("✓ Comment edited successfully.\n")
	return nil
}

// DeleteComment deletes a comment
func (ps *PostService) DeleteComment(commentID string) error {
	logger.Debug("Deleting comment", "comment_id", commentID)

	if err := api.DeleteComment(commentID); err != nil {
		return fmt.Errorf("failed to delete comment: %w", err)
	}

	fmt.Printf("✓ Comment deleted successfully.\n")
	return nil
}

// LikeComment likes a comment
func (ps *PostService) LikeComment(commentID string) error {
	logger.Debug("Liking comment", "comment_id", commentID)

	if err := api.LikeComment(commentID); err != nil {
		return fmt.Errorf("failed to like comment: %w", err)
	}

	fmt.Printf("✓ Comment liked.\n")
	return nil
}

// UnlikeComment unlikes a comment
func (ps *PostService) UnlikeComment(commentID string) error {
	logger.Debug("Unliking comment", "comment_id", commentID)

	if err := api.UnlikeComment(commentID); err != nil {
		return fmt.Errorf("failed to unlike comment: %w", err)
	}

	fmt.Printf("✓ Like removed from comment.\n")
	return nil
}

// Helper methods for displaying posts

func (ps *PostService) displayPostDetails(post *api.Post) {
	fmt.Printf("\n📊 Post Details\n")
	fmt.Printf("  ID:         %s\n", post.ID)
	fmt.Printf("  Duration:   %d seconds\n", post.Duration)
	fmt.Printf("  Created:    %s\n", post.CreatedAt.Format("2006-01-02 15:04:05"))
	fmt.Printf("  Engagement:\n")
	fmt.Printf("    ❤️  Likes:   %d\n", post.LikeCount)
	fmt.Printf("    💬 Comments: %d\n", post.CommentCount)
	fmt.Printf("    🔄 Reposts:  %d\n", post.RepostCount)
	fmt.Printf("    💾 Saves:   %d\n", post.SaveCount)
	fmt.Printf("    ▶️  Plays:   %d\n", post.PlayCount)

	if post.BPM > 0 {
		fmt.Printf("  BPM:        %d\n", post.BPM)
	}
	if post.Key != "" {
		fmt.Printf("  Key:        %s\n", post.Key)
	}
	if len(post.Genre) > 0 {
		fmt.Printf("  Genre:      %v\n", post.Genre)
	}

	fmt.Printf("  Status:     ")
	if post.IsArchived {
		fmt.Printf("Archived")
	} else if post.IsPinned {
		fmt.Printf("Pinned")
	} else {
		fmt.Printf("Active")
	}
	fmt.Printf("\n\n")
}

func (ps *PostService) displayPostList(posts *api.PostListResponse) {
	for i, post := range posts.Posts {
		fmt.Printf("%d. [%s]\n", i+1, post.ID)
		fmt.Printf("   Duration: %ds | ❤️ %d | 💬 %d | 🔄 %d\n",
			post.Duration, post.LikeCount, post.CommentCount, post.RepostCount)
		fmt.Printf("   Created: %s\n", post.CreatedAt.Format("2006-01-02 15:04:05"))
		if post.IsPinned {
			fmt.Printf("   📌 Pinned\n")
		}
		if post.IsArchived {
			fmt.Printf("   📂 Archived\n")
		}
		fmt.Printf("\n")
	}

	fmt.Printf("Showing %d of %d posts (Page %d)\n\n", len(posts.Posts), posts.TotalCount, posts.Page)
}

func (ps *PostService) displayComments(comments *api.CommentListResponse) {
	for i, comment := range comments.Comments {
		fmt.Printf("%d. %s\n", i+1, comment.Content)
		fmt.Printf("   ❤️  %d | %s\n\n", comment.LikeCount, comment.CreatedAt.Format("2006-01-02 15:04:05"))
	}

	fmt.Printf("Showing %d of %d comments (Page %d)\n\n", len(comments.Comments), comments.TotalCount, comments.Page)
}
