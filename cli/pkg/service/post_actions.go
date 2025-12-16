package service

import (
	"bufio"
	"fmt"
	"os"
	"strings"

	"github.com/zfogg/sidechain/cli/pkg/api"
	"github.com/zfogg/sidechain/cli/pkg/formatter"
)

// PostActionsService provides advanced post action operations
type PostActionsService struct{}

// NewPostActionsService creates a new post actions service
func NewPostActionsService() *PostActionsService {
	return &PostActionsService{}
}

// PinPost pins a post to the user's profile
func (pas *PostActionsService) PinPost() error {
	reader := bufio.NewReader(os.Stdin)

	fmt.Print("Post ID: ")
	postID, _ := reader.ReadString('\n')
	postID = strings.TrimSpace(postID)

	if postID == "" {
		return fmt.Errorf("post ID cannot be empty")
	}

	if err := api.PinPost(postID); err != nil {
		return fmt.Errorf("failed to pin post: %w", err)
	}

	formatter.PrintSuccess("✓ Post pinned successfully!")
	fmt.Printf("\n")
	keyValues := map[string]interface{}{
		"Post ID": postID,
		"Status":  "Pinned",
	}
	formatter.PrintKeyValue(keyValues)

	return nil
}

// UnpinPost unpins a post from the user's profile
func (pas *PostActionsService) UnpinPost() error {
	reader := bufio.NewReader(os.Stdin)

	fmt.Print("Post ID: ")
	postID, _ := reader.ReadString('\n')
	postID = strings.TrimSpace(postID)

	if postID == "" {
		return fmt.Errorf("post ID cannot be empty")
	}

	if err := api.UnpinPost(postID); err != nil {
		return fmt.Errorf("failed to unpin post: %w", err)
	}

	formatter.PrintSuccess("✓ Post unpinned successfully!")
	fmt.Printf("\n")
	keyValues := map[string]interface{}{
		"Post ID": postID,
		"Status":  "Unpinned",
	}
	formatter.PrintKeyValue(keyValues)

	return nil
}

// CreateRemix creates a remix based on an existing post
func (pas *PostActionsService) CreateRemix() error {
	reader := bufio.NewReader(os.Stdin)

	fmt.Print("Original Post ID: ")
	postID, _ := reader.ReadString('\n')
	postID = strings.TrimSpace(postID)

	if postID == "" {
		return fmt.Errorf("post ID cannot be empty")
	}

	fmt.Print("Remix Title (optional): ")
	title, _ := reader.ReadString('\n')
	title = strings.TrimSpace(title)

	fmt.Print("Remix Description (optional): ")
	description, _ := reader.ReadString('\n')
	description = strings.TrimSpace(description)

	remixReq := api.RemixRequest{
		OriginalPostID: postID,
		Title:          title,
		Description:    description,
	}

	remix, err := api.CreateRemix(remixReq)
	if err != nil {
		return fmt.Errorf("failed to create remix: %w", err)
	}

	formatter.PrintSuccess("✓ Remix created successfully!")
	fmt.Printf("\n")
	keyValues := map[string]interface{}{
		"Remix ID":         remix.RemixID,
		"Post ID":          remix.PostID,
		"Original Post":    postID,
	}
	formatter.PrintKeyValue(keyValues)

	return nil
}

// ListRemixes displays remixes of a post
func (pas *PostActionsService) ListRemixes() error {
	reader := bufio.NewReader(os.Stdin)

	fmt.Print("Post ID: ")
	postID, _ := reader.ReadString('\n')
	postID = strings.TrimSpace(postID)

	if postID == "" {
		return fmt.Errorf("post ID cannot be empty")
	}

	remixes, err := api.GetRemixes(postID, 1, 20)
	if err != nil {
		return fmt.Errorf("failed to fetch remixes: %w", err)
	}

	if len(remixes.Posts) == 0 {
		fmt.Println("No remixes found for this post")
		return nil
	}

	formatter.PrintInfo("🎵 Remixes (%d total)", remixes.TotalCount)
	fmt.Printf("\n")

	for i, post := range remixes.Posts {
		fmt.Printf("[%d] %s\n", i+1, post.ID)
		if post.Title != "" {
			fmt.Printf("    Title: %s\n", post.Title)
		}
		fmt.Printf("    ❤️  %d | 💬 %d | 🔄 %d\n", post.LikeCount, post.CommentCount, post.RepostCount)
		fmt.Printf("    Created: %s\n\n", post.CreatedAt.Format("2006-01-02 15:04:05"))
	}

	return nil
}

// DownloadPost downloads a post's audio file
func (pas *PostActionsService) DownloadPost() error {
	reader := bufio.NewReader(os.Stdin)

	fmt.Print("Post ID: ")
	postID, _ := reader.ReadString('\n')
	postID = strings.TrimSpace(postID)

	if postID == "" {
		return fmt.Errorf("post ID cannot be empty")
	}

	fmt.Print("Output filename (default: post.mp3): ")
	filename, _ := reader.ReadString('\n')
	filename = strings.TrimSpace(filename)

	if filename == "" {
		filename = "post.mp3"
	}

	downloadURL, err := api.GetPostDownloadURL(postID)
	if err != nil {
		return fmt.Errorf("failed to get download URL: %w", err)
	}

	formatter.PrintSuccess("✓ Download ready!")
	fmt.Printf("\n")
	keyValues := map[string]interface{}{
		"Post ID":    postID,
		"Filename":   filename,
		"Download URL": downloadURL,
	}
	formatter.PrintKeyValue(keyValues)
	fmt.Printf("\nDownload the file using: curl -o %s %s\n", filename, downloadURL)

	return nil
}

// ReportPost reports a post for moderation
func (pas *PostActionsService) ReportPost() error {
	reader := bufio.NewReader(os.Stdin)

	fmt.Print("Post ID: ")
	postID, _ := reader.ReadString('\n')
	postID = strings.TrimSpace(postID)

	if postID == "" {
		return fmt.Errorf("post ID cannot be empty")
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

	if err := api.ReportPost(postID, reason, description); err != nil {
		return fmt.Errorf("failed to report post: %w", err)
	}

	formatter.PrintSuccess("✓ Post reported successfully!")
	fmt.Printf("\n")
	keyValues := map[string]interface{}{
		"Post ID": postID,
		"Reason":  reason,
		"Status":  "Reported",
	}
	formatter.PrintKeyValue(keyValues)

	return nil
}

// GetPostStats displays engagement statistics for a post
func (pas *PostActionsService) GetPostStats() error {
	reader := bufio.NewReader(os.Stdin)

	fmt.Print("Post ID: ")
	postID, _ := reader.ReadString('\n')
	postID = strings.TrimSpace(postID)

	if postID == "" {
		return fmt.Errorf("post ID cannot be empty")
	}

	stats, err := api.GetPostStats(postID)
	if err != nil {
		return fmt.Errorf("failed to fetch post stats: %w", err)
	}

	formatter.PrintInfo("📊 Post Statistics")
	fmt.Printf("\n")

	// Convert int map to interface{} map for formatter
	interfaceStats := make(map[string]interface{})
	for k, v := range stats {
		interfaceStats[k] = v
	}
	formatter.PrintKeyValue(interfaceStats)

	return nil
}
