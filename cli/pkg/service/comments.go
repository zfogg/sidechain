package service

import (
	"bufio"
	"fmt"
	"os"
	"strings"

	"github.com/zfogg/sidechain/cli/pkg/api"
	"github.com/zfogg/sidechain/cli/pkg/formatter"
)

// CommentService provides operations for managing comments
type CommentService struct{}

// NewCommentService creates a new comment service
func NewCommentService() *CommentService {
	return &CommentService{}
}

// CreateComment creates a new comment on a post
func (cs *CommentService) CreateComment() error {
	reader := bufio.NewReader(os.Stdin)

	fmt.Print("Post ID: ")
	postID, _ := reader.ReadString('\n')
	postID = strings.TrimSpace(postID)

	if postID == "" {
		return fmt.Errorf("post ID cannot be empty")
	}

	fmt.Print("Comment text (max 2000 chars): ")
	content, _ := reader.ReadString('\n')
	content = strings.TrimSpace(content)

	if content == "" {
		return fmt.Errorf("comment text cannot be empty")
	}

	if len(content) > 2000 {
		return fmt.Errorf("comment text exceeds 2000 character limit")
	}

	fmt.Print("Is this a reply to another comment? (y/n) [n]: ")
	isReply, _ := reader.ReadString('\n')
	isReply = strings.TrimSpace(strings.ToLower(isReply))

	var parentID *string
	if isReply == "y" || isReply == "yes" {
		fmt.Print("Parent comment ID: ")
		parentIDStr, _ := reader.ReadString('\n')
		parentIDStr = strings.TrimSpace(parentIDStr)
		if parentIDStr != "" {
			parentID = &parentIDStr
		}
	}

	// Create comment
	req := api.CreateCommentRequest{
		Content:  content,
		ParentID: parentID,
	}

	comment, err := api.CreateComment(postID, req)
	if err != nil {
		return fmt.Errorf("failed to create comment: %w", err)
	}

	formatter.PrintSuccess("✓ Comment created successfully!")
	fmt.Printf("\n")
	keyValues := map[string]interface{}{
		"Comment ID":    comment.ID,
		"Post ID":       comment.PostID,
		"Author":        comment.User.Username,
		"Content":       truncate(comment.Content, 50),
		"Likes":         comment.LikeCount,
		"Created":       comment.CreatedAt,
	}
	formatter.PrintKeyValue(keyValues)

	return nil
}

// ViewComments displays comments on a post
func (cs *CommentService) ViewComments() error {
	reader := bufio.NewReader(os.Stdin)

	fmt.Print("Post ID: ")
	postID, _ := reader.ReadString('\n')
	postID = strings.TrimSpace(postID)

	if postID == "" {
		return fmt.Errorf("post ID cannot be empty")
	}

	// Get comments
	resp, err := api.GetComments(postID, 20, 0)
	if err != nil {
		return fmt.Errorf("failed to fetch comments: %w", err)
	}

	if len(resp.Comments) == 0 {
		fmt.Println("No comments yet")
		return nil
	}

	formatter.PrintInfo("💬 Comments (%d total)", resp.Meta.Total)
	fmt.Printf("\n")

	// Display comments
	for i, comment := range resp.Comments {
		fmt.Printf("[%d] %s (@%s) - ❤️ %d likes\n", i+1, comment.User.DisplayName, comment.User.Username, comment.LikeCount)
		fmt.Printf("    %s\n", truncate(comment.Content, 70))
		fmt.Printf("    %s\n\n", comment.CreatedAt)

		// Display replies if any
		if len(comment.Replies) > 0 {
			for j, reply := range comment.Replies {
				fmt.Printf("      └─ [%d.%d] %s (@%s) - ❤️ %d likes\n", i+1, j+1, reply.User.DisplayName, reply.User.Username, reply.LikeCount)
				fmt.Printf("         %s\n", truncate(reply.Content, 65))
				fmt.Printf("         %s\n\n", reply.CreatedAt)
			}
		}
	}

	return nil
}

// UpdateComment updates a comment
func (cs *CommentService) UpdateComment() error {
	reader := bufio.NewReader(os.Stdin)

	fmt.Print("Comment ID: ")
	commentID, _ := reader.ReadString('\n')
	commentID = strings.TrimSpace(commentID)

	if commentID == "" {
		return fmt.Errorf("comment ID cannot be empty")
	}

	fmt.Print("New comment text (max 2000 chars): ")
	content, _ := reader.ReadString('\n')
	content = strings.TrimSpace(content)

	if content == "" {
		return fmt.Errorf("comment text cannot be empty")
	}

	if len(content) > 2000 {
		return fmt.Errorf("comment text exceeds 2000 character limit")
	}

	// Update comment
	req := api.UpdateCommentRequest{Content: content}
	comment, err := api.UpdateComment(commentID, req)
	if err != nil {
		return fmt.Errorf("failed to update comment: %w", err)
	}

	formatter.PrintSuccess("✓ Comment updated successfully!")
	fmt.Printf("\n")
	keyValues := map[string]interface{}{
		"Comment ID": comment.ID,
		"Content":    truncate(comment.Content, 50),
		"Edited":     fmt.Sprintf("%v", comment.IsEdited),
		"Updated":    comment.UpdatedAt,
	}
	formatter.PrintKeyValue(keyValues)

	return nil
}

// DeleteComment deletes a comment
func (cs *CommentService) DeleteComment() error {
	reader := bufio.NewReader(os.Stdin)

	fmt.Print("Comment ID: ")
	commentID, _ := reader.ReadString('\n')
	commentID = strings.TrimSpace(commentID)

	if commentID == "" {
		return fmt.Errorf("comment ID cannot be empty")
	}

	// Confirm deletion
	fmt.Print("Are you sure you want to delete this comment? (yes/no): ")
	confirm, _ := reader.ReadString('\n')
	confirm = strings.TrimSpace(strings.ToLower(confirm))

	if confirm != "yes" && confirm != "y" {
		fmt.Println("Deletion cancelled")
		return nil
	}

	// Delete comment
	err := api.DeleteComment(commentID)
	if err != nil {
		return fmt.Errorf("failed to delete comment: %w", err)
	}

	formatter.PrintSuccess("✓ Comment deleted successfully!")
	fmt.Printf("\n")
	keyValues := map[string]interface{}{
		"Comment ID": commentID,
		"Status":     "Deleted",
	}
	formatter.PrintKeyValue(keyValues)

	return nil
}

// LikeComment likes a comment
func (cs *CommentService) LikeComment() error {
	reader := bufio.NewReader(os.Stdin)

	fmt.Print("Comment ID: ")
	commentID, _ := reader.ReadString('\n')
	commentID = strings.TrimSpace(commentID)

	if commentID == "" {
		return fmt.Errorf("comment ID cannot be empty")
	}

	// Like comment
	likeCount, err := api.LikeComment(commentID)
	if err != nil {
		return fmt.Errorf("failed to like comment: %w", err)
	}

	formatter.PrintSuccess("✓ Comment liked!")
	fmt.Printf("\n")
	keyValues := map[string]interface{}{
		"Comment ID": commentID,
		"Total Likes": likeCount,
	}
	formatter.PrintKeyValue(keyValues)

	return nil
}

// UnlikeComment unlikes a comment
func (cs *CommentService) UnlikeComment() error {
	reader := bufio.NewReader(os.Stdin)

	fmt.Print("Comment ID: ")
	commentID, _ := reader.ReadString('\n')
	commentID = strings.TrimSpace(commentID)

	if commentID == "" {
		return fmt.Errorf("comment ID cannot be empty")
	}

	// Unlike comment
	likeCount, err := api.UnlikeComment(commentID)
	if err != nil {
		return fmt.Errorf("failed to unlike comment: %w", err)
	}

	formatter.PrintSuccess("✓ Comment unliked!")
	fmt.Printf("\n")
	keyValues := map[string]interface{}{
		"Comment ID": commentID,
		"Total Likes": likeCount,
	}
	formatter.PrintKeyValue(keyValues)

	return nil
}

// ReportComment reports a comment
func (cs *CommentService) ReportComment() error {
	reader := bufio.NewReader(os.Stdin)

	fmt.Print("Comment ID: ")
	commentID, _ := reader.ReadString('\n')
	commentID = strings.TrimSpace(commentID)

	if commentID == "" {
		return fmt.Errorf("comment ID cannot be empty")
	}

	fmt.Print("Report reason (spam/harassment/inappropriate/copyright/violence/other): ")
	reason, _ := reader.ReadString('\n')
	reason = strings.TrimSpace(reason)

	if reason == "" {
		return fmt.Errorf("report reason cannot be empty")
	}

	fmt.Print("Additional description (optional): ")
	description, _ := reader.ReadString('\n')
	description = strings.TrimSpace(description)

	// Report comment
	req := api.ReportCommentRequest{
		Reason:      reason,
		Description: description,
	}

	reportID, err := api.ReportComment(commentID, req)
	if err != nil {
		return fmt.Errorf("failed to report comment: %w", err)
	}

	formatter.PrintSuccess("✓ Comment reported successfully!")
	fmt.Printf("\n")
	keyValues := map[string]interface{}{
		"Report ID":   reportID,
		"Comment ID":  commentID,
		"Reason":      reason,
	}
	formatter.PrintKeyValue(keyValues)

	return nil
}
