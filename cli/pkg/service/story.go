package service

import (
	"fmt"
	"strings"
	"time"

	"github.com/zfogg/sidechain/cli/pkg/api"
	"github.com/zfogg/sidechain/cli/pkg/logger"
)

// StoryService handles story and highlight operations
type StoryService struct{}

// NewStoryService creates a new story service
func NewStoryService() *StoryService {
	return &StoryService{}
}

// ListStories lists active stories from followed users
func (ss *StoryService) ListStories(page, pageSize int) error {
	logger.Debug("Listing stories", "page", page)

	if page < 1 {
		page = 1
	}
	if pageSize < 1 || pageSize > 100 {
		pageSize = 10
	}

	stories, err := api.GetStories(page, pageSize)
	if err != nil {
		logger.Error("Failed to fetch stories", "error", err)
		return err
	}

	if len(stories.Stories) == 0 {
		fmt.Println("No active stories. Follow users to see their stories!")
		return nil
	}

	ss.displayStoryList(stories)
	return nil
}

// ViewStory displays a single story
func (ss *StoryService) ViewStory(storyID string) error {
	logger.Debug("Viewing story", "story_id", storyID)

	story, err := api.GetStory(storyID)
	if err != nil {
		logger.Error("Failed to fetch story", "error", err)
		return err
	}

	// Mark as viewed
	_, _ = api.ViewStory(storyID)

	ss.displayStoryDetail(story)
	return nil
}

// ViewUserStories displays all stories for a user
func (ss *StoryService) ViewUserStories(username string) error {
	logger.Debug("Viewing user stories", "username", username)

	// Get user ID from username
	userProfile, err := api.GetUserProfile(username)
	if err != nil {
		logger.Error("Failed to find user", "error", err)
		return fmt.Errorf("user '%s' not found", username)
	}

	stories, err := api.GetUserStories(userProfile.ID, 1, 100)
	if err != nil {
		logger.Error("Failed to fetch user stories", "error", err)
		return err
	}

	if len(stories.Stories) == 0 {
		fmt.Printf("@%s has no active stories.\n", username)
		return nil
	}

	ss.displayUserStories(username, stories)
	return nil
}

// ViewStoryViewers displays who viewed a story (owner only)
func (ss *StoryService) ViewStoryViewers(storyID string) error {
	logger.Debug("Viewing story viewers", "story_id", storyID)

	viewers, err := api.GetStoryViews(storyID, 1, 100)
	if err != nil {
		logger.Error("Failed to fetch story viewers", "error", err)
		return err
	}

	if len(viewers) == 0 {
		fmt.Println("No one has viewed this story yet.")
		return nil
	}

	ss.displayViewers(viewers)
	return nil
}

// DeleteStory deletes a story
func (ss *StoryService) DeleteStory(storyID string) error {
	logger.Debug("Deleting story", "story_id", storyID)

	err := api.DeleteStory(storyID)
	if err != nil {
		logger.Error("Failed to delete story", "error", err)
		return err
	}

	logger.Debug("Story deleted successfully")
	fmt.Println("✓ Story deleted")
	return nil
}

// CreateHighlight creates a new highlight collection
func (ss *StoryService) CreateHighlight(name string, description string) error {
	logger.Debug("Creating highlight", "name", name)

	if name == "" {
		return fmt.Errorf("highlight name cannot be empty")
	}

	if len(name) > 100 {
		return fmt.Errorf("highlight name exceeds maximum length (100 characters)")
	}

	if len(description) > 500 {
		return fmt.Errorf("description exceeds maximum length (500 characters)")
	}

	highlight, err := api.CreateHighlight(name, description)
	if err != nil {
		logger.Error("Failed to create highlight", "error", err)
		return err
	}

	logger.Debug("Highlight created successfully", "highlight_id", highlight.ID)
	fmt.Printf("✓ Highlight '%s' created\n", name)
	return nil
}

// ListHighlights lists all highlights for the current user
func (ss *StoryService) ListHighlights(username string) error {
	logger.Debug("Listing highlights", "username", username)

	// Get user ID from username
	userProfile, err := api.GetUserProfile(username)
	if err != nil {
		logger.Error("Failed to find user", "error", err)
		return fmt.Errorf("user '%s' not found", username)
	}

	highlights, err := api.GetHighlights(userProfile.ID)
	if err != nil {
		logger.Error("Failed to fetch highlights", "error", err)
		return err
	}

	if len(highlights.Highlights) == 0 {
		fmt.Printf("@%s has no highlights yet.\n", username)
		return nil
	}

	ss.displayHighlightList(username, highlights)
	return nil
}

// ViewHighlight displays a highlight collection with its stories
func (ss *StoryService) ViewHighlight(highlightID string) error {
	logger.Debug("Viewing highlight", "highlight_id", highlightID)

	highlight, err := api.GetHighlight(highlightID)
	if err != nil {
		logger.Error("Failed to fetch highlight", "error", err)
		return err
	}

	ss.displayHighlightDetail(highlight)
	return nil
}

// UpdateHighlight updates highlight metadata
func (ss *StoryService) UpdateHighlight(highlightID string, name string, description string) error {
	logger.Debug("Updating highlight", "highlight_id", highlightID)

	if name == "" {
		return fmt.Errorf("highlight name cannot be empty")
	}

	if len(name) > 100 {
		return fmt.Errorf("highlight name exceeds maximum length (100 characters)")
	}

	if len(description) > 500 {
		return fmt.Errorf("description exceeds maximum length (500 characters)")
	}

	_, err := api.UpdateHighlight(highlightID, name, description)
	if err != nil {
		logger.Error("Failed to update highlight", "error", err)
		return err
	}

	logger.Debug("Highlight updated successfully")
	fmt.Println("✓ Highlight updated")
	return nil
}

// DeleteHighlight deletes a highlight collection
func (ss *StoryService) DeleteHighlight(highlightID string) error {
	logger.Debug("Deleting highlight", "highlight_id", highlightID)

	err := api.DeleteHighlight(highlightID)
	if err != nil {
		logger.Error("Failed to delete highlight", "error", err)
		return err
	}

	logger.Debug("Highlight deleted successfully")
	fmt.Println("✓ Highlight deleted")
	return nil
}

// AddStoryToHighlight adds a story to a highlight
func (ss *StoryService) AddStoryToHighlight(highlightID string, storyID string) error {
	logger.Debug("Adding story to highlight", "story_id", storyID)

	err := api.AddStoryToHighlight(highlightID, storyID)
	if err != nil {
		logger.Error("Failed to add story to highlight", "error", err)
		return err
	}

	logger.Debug("Story added to highlight")
	fmt.Println("✓ Story added to highlight")
	return nil
}

// RemoveStoryFromHighlight removes a story from a highlight
func (ss *StoryService) RemoveStoryFromHighlight(highlightID string, storyID string) error {
	logger.Debug("Removing story from highlight", "story_id", storyID)

	err := api.RemoveStoryFromHighlight(highlightID, storyID)
	if err != nil {
		logger.Error("Failed to remove story from highlight", "error", err)
		return err
	}

	logger.Debug("Story removed from highlight")
	fmt.Println("✓ Story removed from highlight")
	return nil
}

// Display functions

func (ss *StoryService) displayStoryList(stories *api.StoryListResponse) {
	fmt.Printf("\n📖 Active Stories (Page %d)\n", stories.Page)
	fmt.Println(strings.Repeat("─", 60))

	for i, story := range stories.Stories {
		expiresIn := time.Until(story.ExpiresAt)
		hoursLeft := int(expiresIn.Hours())
		minutesLeft := int(expiresIn.Minutes()) % 60

		fmt.Printf("%2d. @%s - %s\n", i+1, story.User.Username, story.Filename)
		fmt.Printf("    Duration: %.1fs | Expires in: %dh %dm | 👁 %d\n",
			story.AudioDuration, hoursLeft, minutesLeft, story.ViewCount)

		if len(story.Genre) > 0 {
			fmt.Printf("    Genre: %s\n", strings.Join(story.Genre, ", "))
		}

		if i < len(stories.Stories)-1 {
			fmt.Println(strings.Repeat("─", 60))
		}
	}

	fmt.Printf("\nShowing %d of %d stories\n\n", len(stories.Stories), stories.TotalCount)
}

func (ss *StoryService) displayStoryDetail(story *api.Story) {
	expiresIn := time.Until(story.ExpiresAt)
	hoursLeft := int(expiresIn.Hours())
	minutesLeft := int(expiresIn.Minutes()) % 60

	fmt.Printf("\n📖 %s\n", story.Filename)
	fmt.Println(strings.Repeat("─", 60))
	fmt.Printf("Creator: @%s\n", story.User.Username)
	fmt.Printf("Duration: %.1fs\n", story.AudioDuration)
	fmt.Printf("Views: %d\n", story.ViewCount)
	fmt.Printf("Expires in: %dh %dm\n", hoursLeft, minutesLeft)

	if len(story.Genre) > 0 {
		fmt.Printf("Genre: %s\n", strings.Join(story.Genre, ", "))
	}

	if story.BPM != nil {
		fmt.Printf("BPM: %d\n", *story.BPM)
	}

	if story.Key != nil {
		fmt.Printf("Key: %s\n", *story.Key)
	}

	fmt.Printf("\nAudio: %s\n", story.AudioURL)

	if story.MIDIFilename != "" {
		fmt.Printf("MIDI: %s\n", story.MIDIFilename)
	}

	fmt.Println()
}

func (ss *StoryService) displayUserStories(username string, stories *api.StoryListResponse) {
	fmt.Printf("\n📖 Stories from @%s (Page %d)\n", username, stories.Page)
	fmt.Println(strings.Repeat("─", 60))

	for i, story := range stories.Stories {
		expiresIn := time.Until(story.ExpiresAt)
		hoursLeft := int(expiresIn.Hours())

		fmt.Printf("%2d. %s\n", i+1, story.Filename)
		fmt.Printf("    %.1fs | Expires in %dh | 👁 %d\n",
			story.AudioDuration, hoursLeft, story.ViewCount)

		if i < len(stories.Stories)-1 {
			fmt.Println(strings.Repeat("─", 60))
		}
	}

	fmt.Printf("\nShowing %d of %d stories\n\n", len(stories.Stories), stories.TotalCount)
}

func (ss *StoryService) displayViewers(viewers []api.StoryView) {
	fmt.Println("\n👁 Story Viewers")
	fmt.Println(strings.Repeat("─", 60))

	for i, view := range viewers {
		fmt.Printf("%2d. @%s\n", i+1, view.Viewer.Username)
		fmt.Printf("    Viewed at: %s\n", view.ViewedAt.Format("Jan 02, 15:04"))

		if i < len(viewers)-1 {
			fmt.Println(strings.Repeat("─", 60))
		}
	}

	fmt.Printf("\nTotal viewers: %d\n\n", len(viewers))
}

func (ss *StoryService) displayHighlightList(username string, highlights *api.HighlightListResponse) {
	fmt.Printf("\n⭐ Highlights from @%s\n", username)
	fmt.Println(strings.Repeat("─", 60))

	for i, h := range highlights.Highlights {
		fmt.Printf("%2d. %s\n", i+1, h.Name)
		fmt.Printf("    Stories: %d\n", h.StoryCount)

		if h.Description != "" {
			desc := h.Description
			if len(desc) > 45 {
				desc = desc[:42] + "..."
			}
			fmt.Printf("    %s\n", desc)
		}

		if i < len(highlights.Highlights)-1 {
			fmt.Println(strings.Repeat("─", 60))
		}
	}

	fmt.Printf("\nShowing %d of %d highlights\n\n", len(highlights.Highlights), highlights.TotalCount)
}

func (ss *StoryService) displayHighlightDetail(highlight *api.HighlightDetail) {
	fmt.Printf("\n⭐ %s\n", highlight.Name)
	fmt.Println(strings.Repeat("─", 60))

	if highlight.Description != "" {
		fmt.Printf("Description: %s\n", highlight.Description)
	}

	fmt.Printf("Stories: %d\n", len(highlight.Stories))
	fmt.Println(strings.Repeat("─", 60))

	for i, story := range highlight.Stories {
		expiresIn := time.Until(story.ExpiresAt)
		hoursLeft := int(expiresIn.Hours())

		fmt.Printf("%2d. %s\n", i+1, story.Filename)
		fmt.Printf("    %.1fs | Expires in %dh | 👁 %d\n",
			story.AudioDuration, hoursLeft, story.ViewCount)

		if i < len(highlight.Stories)-1 {
			fmt.Println(strings.Repeat("─", 60))
		}
	}

	fmt.Println()
}
