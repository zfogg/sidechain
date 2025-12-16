package api

import (
	"fmt"
	"time"

	"github.com/zfogg/sidechain/cli/pkg/client"
	"github.com/zfogg/sidechain/cli/pkg/logger"
)

// Story represents a user's story (24-hour expiring audio post)
type Story struct {
	ID            string    `json:"id"`
	UserID        string    `json:"user_id"`
	User          User      `json:"user"`
	AudioURL      string    `json:"audio_url"`
	AudioDuration float64   `json:"audio_duration"`
	Filename      string    `json:"filename"`
	MIDIFilename  string    `json:"midi_filename"`
	MIDIPatternID *string   `json:"midi_pattern_id"`
	WaveformURL   string    `json:"waveform_url"`
	BPM           *int      `json:"bpm"`
	Key           *string   `json:"key"`
	Genre         []string  `json:"genre"`
	ExpiresAt     time.Time `json:"expires_at"`
	ViewCount     int       `json:"view_count"`
	CreatedAt     time.Time `json:"created_at"`
	UpdatedAt     time.Time `json:"updated_at"`
}

// StoryView represents a user viewing a story
type StoryView struct {
	ID       string    `json:"id"`
	StoryID  string    `json:"story_id"`
	ViewerID string    `json:"viewer_id"`
	Viewer   User      `json:"viewer"`
	ViewedAt time.Time `json:"viewed_at"`
}

// StoryListResponse wraps a list of stories
type StoryListResponse struct {
	Stories    []Story `json:"stories"`
	TotalCount int     `json:"total_count"`
	Page       int     `json:"page"`
	PageSize   int     `json:"page_size"`
}

// StoryHighlight represents a collection of saved stories
type StoryHighlight struct {
	ID          string    `json:"id"`
	UserID      string    `json:"user_id"`
	User        User      `json:"user"`
	Name        string    `json:"name"`
	CoverImage  string    `json:"cover_image"`
	Description string    `json:"description"`
	SortOrder   int       `json:"sort_order"`
	StoryCount  int       `json:"story_count"`
	CreatedAt   time.Time `json:"created_at"`
	UpdatedAt   time.Time `json:"updated_at"`
}

// HighlightListResponse wraps a list of highlights
type HighlightListResponse struct {
	Highlights []StoryHighlight `json:"highlights"`
	TotalCount int              `json:"total_count"`
}

// HighlightDetail includes highlight with its stories
type HighlightDetail struct {
	ID          string    `json:"id"`
	UserID      string    `json:"user_id"`
	User        User      `json:"user"`
	Name        string    `json:"name"`
	CoverImage  string    `json:"cover_image"`
	Description string    `json:"description"`
	SortOrder   int       `json:"sort_order"`
	Stories     []Story   `json:"stories"`
	CreatedAt   time.Time `json:"created_at"`
	UpdatedAt   time.Time `json:"updated_at"`
}

// CreateStoryRequest is the request to create a story
type CreateStoryRequest struct {
	Filename      string   `json:"filename"`
	MIDIFilename  string   `json:"midi_filename"`
	MIDIPatternID *string  `json:"midi_pattern_id"`
	AudioDuration float64  `json:"audio_duration"`
	BPM           *int     `json:"bpm"`
	Key           *string  `json:"key"`
	Genre         []string `json:"genre"`
}

// GetStories retrieves active stories from followed users
func GetStories(page int, pageSize int) (*StoryListResponse, error) {
	logger.Debug("Fetching stories", "page", page)

	var response StoryListResponse
	resp, err := client.GetClient().
		R().
		SetQueryParam("page", fmt.Sprintf("%d", page)).
		SetQueryParam("page_size", fmt.Sprintf("%d", pageSize)).
		SetResult(&response).
		Get("/api/v1/stories")

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to fetch stories: %s", resp.Status())
	}

	return &response, nil
}

// GetStory retrieves a single story by ID
func GetStory(storyID string) (*Story, error) {
	logger.Debug("Fetching story", "story_id", storyID)

	var response struct {
		Story Story `json:"story"`
	}

	resp, err := client.GetClient().
		R().
		SetResult(&response).
		Get(fmt.Sprintf("/api/v1/stories/%s", storyID))

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to fetch story: %s", resp.Status())
	}

	return &response.Story, nil
}

// GetUserStories retrieves all active stories for a specific user
func GetUserStories(userID string, page int, pageSize int) (*StoryListResponse, error) {
	logger.Debug("Fetching user stories", "user_id", userID)

	var response StoryListResponse
	resp, err := client.GetClient().
		R().
		SetQueryParam("page", fmt.Sprintf("%d", page)).
		SetQueryParam("page_size", fmt.Sprintf("%d", pageSize)).
		SetResult(&response).
		Get(fmt.Sprintf("/api/v1/users/%s/stories", userID))

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to fetch user stories: %s", resp.Status())
	}

	return &response, nil
}

// ViewStory marks a story as viewed by the current user
func ViewStory(storyID string) (*Story, error) {
	logger.Debug("Viewing story", "story_id", storyID)

	var response struct {
		Story Story `json:"story"`
	}

	resp, err := client.GetClient().
		R().
		SetResult(&response).
		Post(fmt.Sprintf("/api/v1/stories/%s/view", storyID))

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to view story: %s", resp.Status())
	}

	return &response.Story, nil
}

// GetStoryViews retrieves list of users who viewed a story (owner only)
func GetStoryViews(storyID string, page int, pageSize int) ([]StoryView, error) {
	logger.Debug("Fetching story views", "story_id", storyID)

	var response struct {
		Views []StoryView `json:"views"`
		Count int         `json:"count"`
	}

	resp, err := client.GetClient().
		R().
		SetQueryParam("page", fmt.Sprintf("%d", page)).
		SetQueryParam("page_size", fmt.Sprintf("%d", pageSize)).
		SetResult(&response).
		Get(fmt.Sprintf("/api/v1/stories/%s/views", storyID))

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to fetch story views: %s", resp.Status())
	}

	return response.Views, nil
}

// DownloadStory gets download URLs for story audio/MIDI
func DownloadStory(storyID string) (*Story, error) {
	logger.Debug("Downloading story", "story_id", storyID)

	var response struct {
		Story Story `json:"story"`
	}

	resp, err := client.GetClient().
		R().
		SetResult(&response).
		Post(fmt.Sprintf("/api/v1/stories/%s/download", storyID))

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to download story: %s", resp.Status())
	}

	return &response.Story, nil
}

// DeleteStory deletes a story (owner only)
func DeleteStory(storyID string) error {
	logger.Debug("Deleting story", "story_id", storyID)

	resp, err := client.GetClient().
		R().
		Delete(fmt.Sprintf("/api/v1/stories/%s", storyID))

	if err != nil {
		return err
	}

	if !resp.IsSuccess() {
		return fmt.Errorf("failed to delete story: %s", resp.Status())
	}

	return nil
}

// CreateHighlight creates a new highlight collection
func CreateHighlight(name string, description string) (*StoryHighlight, error) {
	logger.Debug("Creating highlight", "name", name)

	req := struct {
		Name        string `json:"name"`
		Description string `json:"description"`
	}{
		Name:        name,
		Description: description,
	}

	var response struct {
		Highlight StoryHighlight `json:"highlight"`
	}

	resp, err := client.GetClient().
		R().
		SetBody(req).
		SetResult(&response).
		Post("/api/v1/highlights")

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to create highlight: %s", resp.Status())
	}

	return &response.Highlight, nil
}

// GetHighlights retrieves all highlights for a user
func GetHighlights(userID string) (*HighlightListResponse, error) {
	logger.Debug("Fetching highlights", "user_id", userID)

	var response HighlightListResponse

	resp, err := client.GetClient().
		R().
		SetResult(&response).
		Get(fmt.Sprintf("/api/v1/users/%s/highlights", userID))

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to fetch highlights: %s", resp.Status())
	}

	return &response, nil
}

// GetHighlight retrieves a single highlight with its stories
func GetHighlight(highlightID string) (*HighlightDetail, error) {
	logger.Debug("Fetching highlight", "highlight_id", highlightID)

	var response struct {
		Highlight HighlightDetail `json:"highlight"`
	}

	resp, err := client.GetClient().
		R().
		SetResult(&response).
		Get(fmt.Sprintf("/api/v1/highlights/%s", highlightID))

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to fetch highlight: %s", resp.Status())
	}

	return &response.Highlight, nil
}

// UpdateHighlight updates highlight metadata
func UpdateHighlight(highlightID string, name string, description string) (*StoryHighlight, error) {
	logger.Debug("Updating highlight", "highlight_id", highlightID)

	req := struct {
		Name        string `json:"name"`
		Description string `json:"description"`
	}{
		Name:        name,
		Description: description,
	}

	var response struct {
		Highlight StoryHighlight `json:"highlight"`
	}

	resp, err := client.GetClient().
		R().
		SetBody(req).
		SetResult(&response).
		Put(fmt.Sprintf("/api/v1/highlights/%s", highlightID))

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to update highlight: %s", resp.Status())
	}

	return &response.Highlight, nil
}

// DeleteHighlight deletes a highlight collection (owner only)
func DeleteHighlight(highlightID string) error {
	logger.Debug("Deleting highlight", "highlight_id", highlightID)

	resp, err := client.GetClient().
		R().
		Delete(fmt.Sprintf("/api/v1/highlights/%s", highlightID))

	if err != nil {
		return err
	}

	if !resp.IsSuccess() {
		return fmt.Errorf("failed to delete highlight: %s", resp.Status())
	}

	return nil
}

// AddStoryToHighlight adds a story to a highlight collection (owner only)
func AddStoryToHighlight(highlightID string, storyID string) error {
	logger.Debug("Adding story to highlight", "highlight_id", highlightID, "story_id", storyID)

	req := struct {
		StoryID string `json:"story_id"`
	}{
		StoryID: storyID,
	}

	resp, err := client.GetClient().
		R().
		SetBody(req).
		Post(fmt.Sprintf("/api/v1/highlights/%s/stories", highlightID))

	if err != nil {
		return err
	}

	if !resp.IsSuccess() {
		return fmt.Errorf("failed to add story to highlight: %s", resp.Status())
	}

	return nil
}

// RemoveStoryFromHighlight removes a story from a highlight (owner only)
func RemoveStoryFromHighlight(highlightID string, storyID string) error {
	logger.Debug("Removing story from highlight", "highlight_id", highlightID, "story_id", storyID)

	resp, err := client.GetClient().
		R().
		Delete(fmt.Sprintf("/api/v1/highlights/%s/stories/%s", highlightID, storyID))

	if err != nil {
		return err
	}

	if !resp.IsSuccess() {
		return fmt.Errorf("failed to remove story from highlight: %s", resp.Status())
	}

	return nil
}
