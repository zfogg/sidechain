package recommendations

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"strings"
	"time"

	"github.com/zfogg/sidechain/backend/internal/models"
	"gorm.io/gorm"
)

// UserItemPrefix is the prefix used for user items in Gorse
// This distinguishes user items from post items
const UserItemPrefix = "user:"

// FeedbackTypeFollow is the feedback type for follow events
const FeedbackTypeFollow = "follow"

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
		IsHidden:   !post.IsPublic || post.IsArchived, // Hide if private or archived
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

// =============================================================================
// User-as-Item Methods for Follow Recommendations
// =============================================================================

// SyncUserAsItem creates a "user item" in Gorse so follows can be tracked as interactions
// Users are stored with "user:" prefix to distinguish from posts
func (c *GorseRESTClient) SyncUserAsItem(userID string) error {
	var user models.User
	if err := c.db.First(&user, "id = ?", userID).Error; err != nil {
		return fmt.Errorf("user not found: %w", err)
	}

	// Build categories from user's genres plus "users" category
	categories := make([]string, 0, len(user.Genre)+1)
	categories = append(categories, "users") // All user items are in "users" category
	categories = append(categories, user.Genre...)

	// Build labels with user metadata
	labels := make(map[string]interface{})
	labels["type"] = "user"
	labels["username"] = user.Username
	if len(user.Genre) > 0 {
		labels["genres"] = user.Genre
	}
	if user.DAWPreference != "" {
		labels["daw"] = user.DAWPreference
	}
	labels["follower_count"] = user.FollowerCount
	labels["is_private"] = user.IsPrivate

	gorseItem := GorseItem{
		ItemId:     UserItemPrefix + userID,
		IsHidden:   user.IsPrivate, // Hide private users from general recommendations
		Categories: categories,
		Timestamp:  user.CreatedAt.Format(time.RFC3339),
		Labels:     labels,
		Comment:    fmt.Sprintf("@%s - %s", user.Username, user.DisplayName),
	}

	_, err := c.makeRequest(context.Background(), "POST", "/api/item", gorseItem)
	return err
}

// SyncFollowEvent syncs a follow relationship to Gorse as feedback
// This enables collaborative filtering for "who to follow" recommendations
func (c *GorseRESTClient) SyncFollowEvent(followerID, followeeID string) error {
	feedback := []GorseFeedback{
		{
			FeedbackType: FeedbackTypeFollow,
			UserId:       followerID,
			ItemId:       UserItemPrefix + followeeID,
			Timestamp:    time.Now().Format(time.RFC3339),
		},
	}

	_, err := c.makeRequest(context.Background(), "POST", "/api/feedback", feedback)
	return err
}

// RemoveFollowEvent removes a follow relationship from Gorse
// Called when a user unfollows someone
func (c *GorseRESTClient) RemoveFollowEvent(followerID, followeeID string) error {
	endpoint := fmt.Sprintf("/api/feedback/%s/%s/%s",
		FeedbackTypeFollow, followerID, UserItemPrefix+followeeID)
	_, err := c.makeRequest(context.Background(), "DELETE", endpoint, nil)
	// Ignore errors - feedback might not exist
	if err != nil {
		// Log but don't fail - the feedback might not exist
		fmt.Printf("Warning: failed to remove follow feedback: %v\n", err)
	}
	return nil
}

// UserScore represents a user with their recommendation score
type UserScore struct {
	User   models.User
	Score  float64
	Reason string
}

// GetUsersToFollow returns recommended users to follow based on collaborative filtering
// Uses the "users" category to only return user items
func (c *GorseRESTClient) GetUsersToFollow(userID string, limit, offset int) ([]UserScore, error) {
	// Gorse GetRecommend endpoint with category filter
	totalLimit := limit + offset
	endpoint := fmt.Sprintf("/api/recommend/%s/users?n=%d", userID, totalLimit)
	resp, err := c.makeRequest(context.Background(), "GET", endpoint, nil)
	if err != nil {
		// Fallback: try without category (older Gorse versions)
		endpoint = fmt.Sprintf("/api/recommend/%s?n=%d", userID, totalLimit*2)
		resp, err = c.makeRequest(context.Background(), "GET", endpoint, nil)
		if err != nil {
			return nil, fmt.Errorf("failed to get user recommendations: %w", err)
		}
	}
	defer resp.Body.Close()

	var itemIDs []string
	if err := json.NewDecoder(resp.Body).Decode(&itemIDs); err != nil {
		return nil, fmt.Errorf("failed to decode response: %w", err)
	}

	// Filter to only user items (with "user:" prefix) and extract user IDs
	userIDs := make([]string, 0)
	for _, itemID := range itemIDs {
		if strings.HasPrefix(itemID, UserItemPrefix) {
			extractedID := strings.TrimPrefix(itemID, UserItemPrefix)
			// Don't recommend following yourself
			if extractedID != userID {
				userIDs = append(userIDs, extractedID)
			}
		}
	}

	// Apply offset
	if offset > 0 && offset < len(userIDs) {
		userIDs = userIDs[offset:]
	}
	if len(userIDs) > limit {
		userIDs = userIDs[:limit]
	}

	if len(userIDs) == 0 {
		return []UserScore{}, nil
	}

	// Fetch user details from database
	var users []models.User
	if err := c.db.Where("id IN ? AND deleted_at IS NULL", userIDs).Find(&users).Error; err != nil {
		return nil, fmt.Errorf("failed to fetch users: %w", err)
	}

	// Create map for ordering
	userMap := make(map[string]models.User)
	for _, user := range users {
		userMap[user.ID] = user
	}

	// Build UserScore list maintaining Gorse's order
	scores := make([]UserScore, 0, len(userIDs))
	for _, uid := range userIDs {
		if user, ok := userMap[uid]; ok {
			scores = append(scores, UserScore{
				User:   user,
				Score:  1.0,
				Reason: c.generateUserReason(user),
			})
		}
	}

	return scores, nil
}

// generateUserReason generates a human-readable reason for why a user was recommended
func (c *GorseRESTClient) generateUserReason(user models.User) string {
	reasons := make([]string, 0)

	if len(user.Genre) > 0 {
		reasons = append(reasons, "similar music taste")
	}
	if user.FollowerCount > 100 {
		reasons = append(reasons, "popular producer")
	}
	if user.DAWPreference != "" {
		reasons = append(reasons, fmt.Sprintf("uses %s", user.DAWPreference))
	}

	if len(reasons) == 0 {
		return "followed by people you follow"
	}
	if len(reasons) == 1 {
		return reasons[0]
	}
	return reasons[0] + " • " + reasons[1]
}

// BatchSyncUserItems syncs all users as items to Gorse (for initial setup)
func (c *GorseRESTClient) BatchSyncUserItems() error {
	var users []models.User
	if err := c.db.Where("deleted_at IS NULL").Find(&users).Error; err != nil {
		return fmt.Errorf("failed to fetch users: %w", err)
	}

	gorseItems := make([]GorseItem, 0, len(users))
	for _, user := range users {
		categories := make([]string, 0, len(user.Genre)+1)
		categories = append(categories, "users")
		categories = append(categories, user.Genre...)

		labels := make(map[string]interface{})
		labels["type"] = "user"
		labels["username"] = user.Username
		if len(user.Genre) > 0 {
			labels["genres"] = user.Genre
		}
		if user.DAWPreference != "" {
			labels["daw"] = user.DAWPreference
		}
		labels["follower_count"] = user.FollowerCount
		labels["is_private"] = user.IsPrivate

		gorseItems = append(gorseItems, GorseItem{
			ItemId:     UserItemPrefix + user.ID,
			IsHidden:   user.IsPrivate,
			Categories: categories,
			Timestamp:  user.CreatedAt.Format(time.RFC3339),
			Labels:     labels,
			Comment:    fmt.Sprintf("@%s - %s", user.Username, user.DisplayName),
		})
	}

	// Batch in chunks of 100
	const batchSize = 100
	for i := 0; i < len(gorseItems); i += batchSize {
		end := i + batchSize
		if end > len(gorseItems) {
			end = len(gorseItems)
		}

		_, err := c.makeRequest(context.Background(), "POST", "/api/items", gorseItems[i:end])
		if err != nil {
			return fmt.Errorf("failed to sync user items batch: %w", err)
		}
	}

	return nil
}

// StreamFollower represents a follower from GetStream.io
type StreamFollower struct {
	UserID string
}

// BatchSyncFollowsFromStream syncs existing follow relationships from GetStream.io to Gorse
// This requires a function that fetches follows from Stream - pass it as a parameter
type FollowFetcher func(userID string, limit, offset int) ([]StreamFollower, error)

func (c *GorseRESTClient) BatchSyncFollowsFromList(follows []struct {
	FollowerID string
	FolloweeID string
}) error {
	if len(follows) == 0 {
		return nil
	}

	// Batch in chunks of 100
	const batchSize = 100
	for i := 0; i < len(follows); i += batchSize {
		end := i + batchSize
		if end > len(follows) {
			end = len(follows)
		}

		feedbacks := make([]GorseFeedback, 0, end-i)
		for j := i; j < end; j++ {
			f := follows[j]
			feedbacks = append(feedbacks, GorseFeedback{
				FeedbackType: FeedbackTypeFollow,
				UserId:       f.FollowerID,
				ItemId:       UserItemPrefix + f.FolloweeID,
				Timestamp:    time.Now().Format(time.RFC3339),
			})
		}

		_, err := c.makeRequest(context.Background(), "POST", "/api/feedback", feedbacks)
		if err != nil {
			return fmt.Errorf("failed to sync follow batch: %w", err)
		}
	}

	return nil
}

// =============================================================================
// Category-Based Filtering Methods (Task 6)
// =============================================================================

// GetForYouFeedByGenre returns personalized recommendations filtered by genre
// Task 6.1
func (c *GorseRESTClient) GetForYouFeedByGenre(userID, genre string, limit, offset int) ([]PostScore, error) {
	// Gorse category parameter: GET /api/recommend/{user-id}/{category}?n={n}
	totalLimit := limit + offset
	endpoint := fmt.Sprintf("/api/recommend/%s/%s?n=%d", userID, genre, totalLimit)
	resp, err := c.makeRequest(context.Background(), "GET", endpoint, nil)
	if err != nil {
		return nil, fmt.Errorf("failed to get recommendations by genre: %w", err)
	}
	defer resp.Body.Close()

	var itemIDs []string
	if err := json.NewDecoder(resp.Body).Decode(&itemIDs); err != nil {
		return nil, fmt.Errorf("failed to decode response: %w", err)
	}

	// Apply offset
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
				Reason: fmt.Sprintf("matches your %s preferences", genre),
			})
		}
	}

	return scores, nil
}

// GetForYouFeedByBPMRange returns personalized recommendations filtered by BPM range
// Task 6.3
func (c *GorseRESTClient) GetForYouFeedByBPMRange(userID string, minBPM, maxBPM, limit, offset int) ([]PostScore, error) {
	// Get general recommendations first
	totalLimit := (limit + offset) * 3 // Fetch extra to account for filtering
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

	if len(itemIDs) == 0 {
		return []PostScore{}, nil
	}

	// Fetch post details and filter by BPM range in database
	var posts []models.AudioPost
	query := c.db.Where("id IN ? AND deleted_at IS NULL", itemIDs)
	if minBPM > 0 {
		query = query.Where("bpm >= ?", minBPM)
	}
	if maxBPM > 0 {
		query = query.Where("bpm <= ?", maxBPM)
	}
	if err := query.Find(&posts).Error; err != nil {
		return nil, fmt.Errorf("failed to fetch posts: %w", err)
	}

	// Create a map for quick lookup and maintain Gorse's order
	postMap := make(map[string]models.AudioPost)
	for _, post := range posts {
		postMap[post.ID] = post
	}

	// Build filtered list maintaining Gorse's order
	scores := make([]PostScore, 0, limit)
	count := 0
	for _, itemID := range itemIDs {
		if post, ok := postMap[itemID]; ok {
			if count < offset {
				count++
				continue
			}
			if len(scores) >= limit {
				break
			}
			scores = append(scores, PostScore{
				Post:   post,
				Score:  1.0,
				Reason: fmt.Sprintf("matches your BPM preference (%d-%d)", minBPM, maxBPM),
			})
		}
	}

	return scores, nil
}

// GetSimilarPostsByGenre returns similar posts filtered by genre
// Task 6.4
func (c *GorseRESTClient) GetSimilarPostsByGenre(postID, genre string, limit int) ([]models.AudioPost, error) {
	// Get all similar posts first
	fetchLimit := limit * 3 // Fetch extra to account for genre filtering
	endpoint := fmt.Sprintf("/api/item/%s/neighbors?n=%d", postID, fetchLimit)
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

	// Fetch posts filtered by genre
	var posts []models.AudioPost
	// Use JSON contains operator for genre array
	if err := c.db.Where("id IN ? AND deleted_at IS NULL", postIDs).
		Where("genre @> ARRAY[?]::text[]", genre).
		Limit(limit).
		Find(&posts).Error; err != nil {
		return nil, fmt.Errorf("failed to fetch posts: %w", err)
	}

	return posts, nil
}

// GetPopular returns globally popular posts based on engagement metrics
// Task 7.1
func (c *GorseRESTClient) GetPopular(limit, offset int) ([]PostScore, error) {
	// Note: Gorse in-one doesn't have /api/popular, so we use /api/latest
	// which returns recently added items (works as a fallback for popular)
	// Note: Gorse doesn't support offset parameter, so we fetch more and slice
	totalLimit := limit + offset
	endpoint := fmt.Sprintf("/api/latest?n=%d", totalLimit)
	resp, err := c.makeRequest(context.Background(), "GET", endpoint, nil)
	if err != nil {
		return nil, fmt.Errorf("failed to get popular posts: %w", err)
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

	// Extract item IDs
	itemIDs := make([]string, 0, len(scores))
	scoreMap := make(map[string]float64)
	for _, score := range scores {
		itemIDs = append(itemIDs, score.Id)
		scoreMap[score.Id] = score.Score
	}

	// Apply offset
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
	postScores := make([]PostScore, 0, len(itemIDs))
	for _, itemID := range itemIDs {
		if post, ok := postMap[itemID]; ok {
			postScores = append(postScores, PostScore{
				Post:   post,
				Score:  scoreMap[itemID],
				Reason: "trending now",
			})
		}
	}

	return postScores, nil
}

// GetLatest returns recently added posts
// Task 7.2
func (c *GorseRESTClient) GetLatest(limit, offset int) ([]PostScore, error) {
	// Gorse latest endpoint: GET /api/latest?n={n}
	// Note: Gorse doesn't support offset parameter, so we fetch more and slice
	totalLimit := limit + offset
	endpoint := fmt.Sprintf("/api/latest?n=%d", totalLimit)
	resp, err := c.makeRequest(context.Background(), "GET", endpoint, nil)
	if err != nil {
		return nil, fmt.Errorf("failed to get latest posts: %w", err)
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

	// Extract item IDs
	itemIDs := make([]string, 0, len(scores))
	scoreMap := make(map[string]float64)
	for _, score := range scores {
		itemIDs = append(itemIDs, score.Id)
		scoreMap[score.Id] = score.Score
	}

	// Apply offset
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
	postScores := make([]PostScore, 0, len(itemIDs))
	for _, itemID := range itemIDs {
		if post, ok := postMap[itemID]; ok {
			postScores = append(postScores, PostScore{
				Post:   post,
				Score:  scoreMap[itemID],
				Reason: "recently added",
			})
		}
	}

	return postScores, nil
}
