package recommendations

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"time"

	"github.com/zfogg/sidechain/backend/internal/models"
	"gorm.io/gorm"
)

// GorseRESTClient provides recommendation algorithms using Gorse REST API
type GorseRESTClient struct {
	baseURL string
	apiKey  string
	client  *http.Client
	db      *gorm.DB
}

// NewGorseRESTClient creates a new Gorse REST client
func NewGorseRESTClient(baseURL, apiKey string, db *gorm.DB) *GorseRESTClient {
	return &GorseRESTClient{
		baseURL: baseURL,
		apiKey:  apiKey,
		client: &http.Client{
			Timeout: 10 * time.Second,
		},
		db: db,
	}
}

// PostScore represents a post with its recommendation score
type PostScore struct {
	Post   models.AudioPost
	Score  float64
	Reason string
}

// GorseUser represents a user in Gorse
// Labels should be map[string]interface{} per Gorse documentation
type GorseUser struct {
	UserId  string                 `json:"UserId"`
	Labels  map[string]interface{} `json:"Labels,omitempty"`
	Comment string                 `json:"Comment,omitempty"`
}

// GorseItem represents an item (post) in Gorse
// Labels should be map[string]interface{} per Gorse documentation
type GorseItem struct {
	ItemId     string                 `json:"ItemId"`
	IsHidden   bool                   `json:"IsHidden,omitempty"`
	Categories []string               `json:"Categories,omitempty"`
	Timestamp  string                 `json:"Timestamp,omitempty"`
	Labels     map[string]interface{} `json:"Labels,omitempty"`
	Comment    string                 `json:"Comment,omitempty"`
}

// GorseFeedback represents user feedback in Gorse
type GorseFeedback struct {
	FeedbackType string `json:"FeedbackType"`
	UserId       string `json:"UserId"`
	ItemId       string `json:"ItemId"`
	Timestamp    string `json:"Timestamp,omitempty"`
}

// makeRequest makes an HTTP request to Gorse API
func (c *GorseRESTClient) makeRequest(ctx context.Context, method, endpoint string, body interface{}) (*http.Response, error) {
	url := c.baseURL + endpoint

	var reqBody io.Reader
	if body != nil {
		jsonData, err := json.Marshal(body)
		if err != nil {
			return nil, fmt.Errorf("failed to marshal request: %w", err)
		}
		reqBody = bytes.NewBuffer(jsonData)
	}

	req, err := http.NewRequestWithContext(ctx, method, url, reqBody)
	if err != nil {
		return nil, fmt.Errorf("failed to create request: %w", err)
	}

	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-API-Key", c.apiKey)

	resp, err := c.client.Do(req)
	if err != nil {
		return nil, fmt.Errorf("request failed: %w", err)
	}

	if resp.StatusCode >= 400 {
		resp.Body.Close()
		return nil, fmt.Errorf("gorse API error: status %d", resp.StatusCode)
	}

	return resp, nil
}

// SyncUser syncs a user to Gorse
func (c *GorseRESTClient) SyncUser(userID string) error {
	var user models.User
	if err := c.db.First(&user, "id = ?", userID).Error; err != nil {
		return fmt.Errorf("user not found: %w", err)
	}

	// Convert genre slice to map format expected by Gorse
	labels := make(map[string]interface{})
	if len(user.Genre) > 0 {
		labels["genres"] = user.Genre
	}

	gorseUser := GorseUser{
		UserId:  userID,
		Labels:  labels,
		Comment: fmt.Sprintf("%s - %s", user.Username, user.DisplayName),
	}

	_, err := c.makeRequest(context.Background(), "POST", "/api/user", gorseUser)
	return err
}

// SyncItem syncs a post (item) to Gorse
func (c *GorseRESTClient) SyncItem(postID string) error {
	var post models.AudioPost
	if err := c.db.Preload("User").First(&post, "id = ?", postID).Error; err != nil {
		return fmt.Errorf("post not found: %w", err)
	}

	categories := make([]string, 0)
	categories = append(categories, post.Genre...)
	if post.BPM > 0 {
		categories = append(categories, fmt.Sprintf("bpm_%d", post.BPM))
	}
	if post.Key != "" {
		categories = append(categories, fmt.Sprintf("key_%s", post.Key))
	}

	// Convert genre slice to map format expected by Gorse
	labels := make(map[string]interface{})
	if len(post.Genre) > 0 {
		labels["genres"] = post.Genre
	}
	if post.BPM > 0 {
		labels["bpm"] = post.BPM
	}
	if post.Key != "" {
		labels["key"] = post.Key
	}

	gorseItem := GorseItem{
		ItemId:     postID,
		IsHidden:   !post.IsPublic,
		Categories: categories,
		Timestamp:  post.CreatedAt.Format(time.RFC3339),
		Labels:     labels,
		Comment:    fmt.Sprintf("%s - %s", post.User.Username, post.DAW),
	}

	_, err := c.makeRequest(context.Background(), "POST", "/api/item", gorseItem)
	return err
}

// SyncFeedback syncs user interactions to Gorse
func (c *GorseRESTClient) SyncFeedback(userID, postID, feedbackType string) error {
	feedback := []GorseFeedback{
		{
			FeedbackType: feedbackType,
			UserId:       userID,
			ItemId:       postID,
			Timestamp:    time.Now().Format(time.RFC3339),
		},
	}

	_, err := c.makeRequest(context.Background(), "POST", "/api/feedback", feedback)
	return err
}

// GetForYouFeed returns personalized recommendations for a user
func (c *GorseRESTClient) GetForYouFeed(userID string, limit, offset int) ([]PostScore, error) {
	// Gorse GetRecommend endpoint: GET /api/recommend/{user-id}?n={n}
	// Note: Gorse doesn't support offset parameter, so we fetch more and slice
	totalLimit := limit + offset
	endpoint := fmt.Sprintf("/api/recommend/%s?n=%d", userID, totalLimit)
	resp, err := c.makeRequest(context.Background(), "GET", endpoint, nil)
	if err != nil {
		return nil, fmt.Errorf("failed to get recommendations: %w", err)
	}
	defer resp.Body.Close()

	var itemIDs []string
	if err := json.NewDecoder(resp.Body).Decode(&itemIDs); err != nil {
		return nil, fmt.Errorf("failed to decode response: %w", err)
	}

	// Apply offset (since Gorse doesn't support offset parameter)
	if offset > 0 && offset < len(itemIDs) {
		itemIDs = itemIDs[offset:]
	}
	if len(itemIDs) > limit {
		itemIDs = itemIDs[:limit]
	}

	if len(itemIDs) == 0 {
		return []PostScore{}, nil
	}

	// Fetch post details from database
	var posts []models.AudioPost
	if err := c.db.Where("id IN ? AND deleted_at IS NULL", itemIDs).Find(&posts).Error; err != nil {
		return nil, fmt.Errorf("failed to fetch posts: %w", err)
	}

	// Create a map for quick lookup
	postMap := make(map[string]models.AudioPost)
	for _, post := range posts {
		postMap[post.ID] = post
	}

	// Build PostScore list maintaining Gorse's order
	scores := make([]PostScore, 0, len(itemIDs))
	for _, itemID := range itemIDs {
		if post, ok := postMap[itemID]; ok {
			scores = append(scores, PostScore{
				Post:   post,
				Score:  1.0,
				Reason: c.generateReason(post),
			})
		}
	}

	return scores, nil
}

// GetSimilarPosts returns posts similar to a given post
func (c *GorseRESTClient) GetSimilarPosts(postID string, limit int) ([]models.AudioPost, error) {
	// Gorse GetNeighbors endpoint: GET /api/item/{item-id}/neighbors?n={n}
	endpoint := fmt.Sprintf("/api/item/%s/neighbors?n=%d", postID, limit)
	resp, err := c.makeRequest(context.Background(), "GET", endpoint, nil)
	if err != nil {
		return nil, fmt.Errorf("failed to get similar posts: %w", err)
	}
	defer resp.Body.Close()

	type Score struct {
		Id    string  `json:"Id"`
		Score float64 `json:"Score"`
	}

	var scores []Score
	if err := json.NewDecoder(resp.Body).Decode(&scores); err != nil {
		return nil, fmt.Errorf("failed to decode response: %w", err)
	}

	postIDs := make([]string, 0, len(scores))
	for _, score := range scores {
		postIDs = append(postIDs, score.Id)
	}

	if len(postIDs) == 0 {
		return []models.AudioPost{}, nil
	}

	var posts []models.AudioPost
	if err := c.db.Where("id IN ? AND deleted_at IS NULL", postIDs).Find(&posts).Error; err != nil {
		return nil, fmt.Errorf("failed to fetch posts: %w", err)
	}

	return posts, nil
}

// GetSimilarUsers returns users with similar taste
func (c *GorseRESTClient) GetSimilarUsers(userID string, limit int) ([]models.User, error) {
	// Use Gorse's user neighbors endpoint: GET /api/user/{user-id}/neighbors?n={n}
	endpoint := fmt.Sprintf("/api/user/%s/neighbors?n=%d", userID, limit)
	resp, err := c.makeRequest(context.Background(), "GET", endpoint, nil)
	if err != nil {
		// If user has no neighbors, return empty
		return []models.User{}, nil
	}
	defer resp.Body.Close()

	type Score struct {
		Id    string  `json:"Id"`
		Score float64 `json:"Score"`
	}

	var scores []Score
	if err := json.NewDecoder(resp.Body).Decode(&scores); err != nil {
		return []models.User{}, nil
	}

	if len(scores) == 0 {
		return []models.User{}, nil
	}

	userIDs := make([]string, 0, len(scores))
	for _, score := range scores {
		userIDs = append(userIDs, score.Id)
	}

	var users []models.User
	if err := c.db.Where("id IN ? AND deleted_at IS NULL", userIDs).Find(&users).Error; err != nil {
		return nil, fmt.Errorf("failed to fetch users: %w", err)
	}

	return users, nil
}

// generateReason generates a human-readable reason for why a post was recommended
func (c *GorseRESTClient) generateReason(post models.AudioPost) string {
	reasons := make([]string, 0)

	if len(post.Genre) > 0 {
		reasons = append(reasons, "matches your genre preferences")
	}
	if post.BPM > 0 {
		reasons = append(reasons, "matches your BPM range")
	}
	if post.Key != "" {
		reasons = append(reasons, "matches your key preferences")
	}
	if post.LikeCount > 10 {
		reasons = append(reasons, "popular with other producers")
	}
	if time.Since(post.CreatedAt).Hours() < 24 {
		reasons = append(reasons, "recently posted")
	}

	if len(reasons) == 0 {
		return "trending"
	}
	if len(reasons) == 1 {
		return reasons[0]
	}
	return reasons[0] + " and " + reasons[1]
}

// BatchSyncUsers syncs all users to Gorse (for initial setup)
// Uses batch endpoint POST /api/users for efficiency
func (c *GorseRESTClient) BatchSyncUsers() error {
	var users []models.User
	if err := c.db.Find(&users).Error; err != nil {
		return fmt.Errorf("failed to fetch users: %w", err)
	}

	gorseUsers := make([]GorseUser, 0, len(users))
	for _, user := range users {
		labels := make(map[string]interface{})
		if len(user.Genre) > 0 {
			labels["genres"] = user.Genre
		}
		gorseUsers = append(gorseUsers, GorseUser{
			UserId:  user.ID,
			Labels:  labels,
			Comment: fmt.Sprintf("%s - %s", user.Username, user.DisplayName),
		})
	}

	_, err := c.makeRequest(context.Background(), "POST", "/api/users", gorseUsers)
	return err
}

// BatchSyncItems syncs all public posts to Gorse (for initial setup)
// Uses batch endpoint POST /api/items for efficiency
func (c *GorseRESTClient) BatchSyncItems() error {
	var posts []models.AudioPost
	if err := c.db.Preload("User").Where("is_public = ? AND deleted_at IS NULL", true).Find(&posts).Error; err != nil {
		return fmt.Errorf("failed to fetch posts: %w", err)
	}

	gorseItems := make([]GorseItem, 0, len(posts))
	for _, post := range posts {
		categories := make([]string, 0)
		categories = append(categories, post.Genre...)
		if post.BPM > 0 {
			categories = append(categories, fmt.Sprintf("bpm_%d", post.BPM))
		}
		if post.Key != "" {
			categories = append(categories, fmt.Sprintf("key_%s", post.Key))
		}

		labels := make(map[string]interface{})
		if len(post.Genre) > 0 {
			labels["genres"] = post.Genre
		}
		if post.BPM > 0 {
			labels["bpm"] = post.BPM
		}
		if post.Key != "" {
			labels["key"] = post.Key
		}

		gorseItems = append(gorseItems, GorseItem{
			ItemId:     post.ID,
			IsHidden:   !post.IsPublic,
			Categories: categories,
			Timestamp:  post.CreatedAt.Format(time.RFC3339),
			Labels:     labels,
			Comment:    fmt.Sprintf("%s - %s", post.User.Username, post.DAW),
		})
	}

	_, err := c.makeRequest(context.Background(), "POST", "/api/items", gorseItems)
	return err
}

// BatchSyncFeedback syncs all play history to Gorse (for initial setup)
func (c *GorseRESTClient) BatchSyncFeedback() error {
	var playHistory []models.PlayHistory
	if err := c.db.Find(&playHistory).Error; err != nil {
		return fmt.Errorf("failed to fetch play history: %w", err)
	}

	// Batch feedbacks in chunks of 100
	const batchSize = 100
	for i := 0; i < len(playHistory); i += batchSize {
		end := i + batchSize
		if end > len(playHistory) {
			end = len(playHistory)
		}

		feedbacks := make([]GorseFeedback, 0, end-i)
		for j := i; j < end; j++ {
			play := playHistory[j]
			feedbackType := "view"
			if play.Completed {
				feedbackType = "like" // Completed plays are stronger signals
			}
			feedbacks = append(feedbacks, GorseFeedback{
				FeedbackType: feedbackType,
				UserId:       play.UserID,
				ItemId:       play.PostID,
				Timestamp:    play.CreatedAt.Format(time.RFC3339),
			})
		}

		_, err := c.makeRequest(context.Background(), "POST", "/api/feedback", feedbacks)
		if err != nil {
			return fmt.Errorf("failed to sync feedback batch: %w", err)
		}
	}

	return nil
}
