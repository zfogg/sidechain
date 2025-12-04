package stream

import (
	"context"
	"fmt"
	"os"
	"time"

	stream "github.com/GetStream/stream-go2/v8"
	chat "github.com/GetStream/stream-chat-go/v5"
)

// Feed group names configured in Stream.io dashboard
const (
	FeedGroupUser     = "user"     // Personal feed - user's own posts
	FeedGroupTimeline = "timeline" // Aggregated feed from followed users
	FeedGroupGlobal   = "global"   // All posts from all users
)

// Client wraps the Stream.io clients with Sidechain-specific functionality
type Client struct {
	feedsClient *stream.Client
	ChatClient  *chat.Client
}

// Activity represents a Sidechain activity (loop post)
type Activity struct {
	ID           string                 `json:"id,omitempty"`
	Actor        string                 `json:"actor"`
	Verb         string                 `json:"verb"`
	Object       string                 `json:"object"`
	ForeignID    string                 `json:"foreign_id,omitempty"`
	Time         string                 `json:"time,omitempty"`
	AudioURL     string                 `json:"audio_url"`
	BPM          int                    `json:"bpm"`
	Key          string                 `json:"key,omitempty"`
	DAW          string                 `json:"daw,omitempty"`
	DurationBars int                    `json:"duration_bars"`
	Genre        []string               `json:"genre,omitempty"`
	Waveform     string                 `json:"waveform,omitempty"`
	Extra        map[string]interface{} `json:"extra,omitempty"`
}

// NewClient creates a new Stream.io client for Sidechain
func NewClient() (*Client, error) {
	apiKey := os.Getenv("STREAM_API_KEY")
	apiSecret := os.Getenv("STREAM_API_SECRET")

	if apiKey == "" || apiSecret == "" {
		return nil, fmt.Errorf("STREAM_API_KEY and STREAM_API_SECRET must be set")
	}

	// Initialize Stream.io Feeds V2 client
	feedsClient, err := stream.New(apiKey, apiSecret)
	if err != nil {
		return nil, fmt.Errorf("failed to create Stream.io Feeds client: %w", err)
	}

	// Initialize Chat client (separate SDK for chat features)
	chatClient, err := chat.NewClient(apiKey, apiSecret)
	if err != nil {
		return nil, fmt.Errorf("failed to create Stream.io Chat client: %w", err)
	}

	return &Client{
		feedsClient: feedsClient,
		ChatClient:  chatClient,
	}, nil
}

// FeedsClient returns the underlying feeds client for direct access if needed
func (c *Client) FeedsClient() *stream.Client {
	return c.feedsClient
}

// CreateUser creates a Stream.io user for both feeds and chat
func (c *Client) CreateUser(userID, username string) error {
	ctx := context.Background()

	// Create user in Chat system
	user := &chat.User{
		ID:   userID,
		Name: username,
	}
	_, err := c.ChatClient.UpsertUser(ctx, user)
	if err != nil {
		return fmt.Errorf("failed to create chat user: %w", err)
	}

	// Note: Feeds V2 users are created automatically when they perform actions
	// No explicit user creation needed for feeds
	return nil
}

// CreateLoopActivity creates an activity for a new loop post
func (c *Client) CreateLoopActivity(userID string, activity *Activity) error {
	ctx := context.Background()

	// Get the user's feed
	userFeed, err := c.feedsClient.FlatFeed(FeedGroupUser, userID)
	if err != nil {
		return fmt.Errorf("failed to get user feed: %w", err)
	}

	// Build the activity with extra fields for loop metadata
	streamActivity := stream.Activity{
		Actor:  fmt.Sprintf("user:%s", userID),
		Verb:   "posted",
		Object: activity.Object,
		Extra: map[string]any{
			"audio_url":     activity.AudioURL,
			"bpm":           activity.BPM,
			"duration_bars": activity.DurationBars,
		},
	}

	// Add optional fields
	if activity.Key != "" {
		streamActivity.Extra["key"] = activity.Key
	}
	if activity.DAW != "" {
		streamActivity.Extra["daw"] = activity.DAW
	}
	if len(activity.Genre) > 0 {
		streamActivity.Extra["genre"] = activity.Genre
	}
	if activity.Waveform != "" {
		streamActivity.Extra["waveform"] = activity.Waveform
	}
	if activity.Extra != nil {
		for k, v := range activity.Extra {
			streamActivity.Extra[k] = v
		}
	}

	// Set foreign ID if provided
	if activity.ForeignID != "" {
		streamActivity.ForeignID = activity.ForeignID
	}

	// Also post to global feed
	globalFeed, err := c.feedsClient.FlatFeed(FeedGroupGlobal, "main")
	if err != nil {
		return fmt.Errorf("failed to get global feed: %w", err)
	}

	// Set the "to" field to also add to global feed
	streamActivity.To = []string{globalFeed.ID()}

	// Add activity to user's feed (will also propagate to global via "to")
	resp, err := userFeed.AddActivity(ctx, streamActivity)
	if err != nil {
		return fmt.Errorf("failed to create Stream.io activity: %w", err)
	}

	// Update activity with returned ID and time
	activity.ID = resp.ID
	if !resp.Time.IsZero() {
		activity.Time = resp.Time.Format(time.RFC3339)
	}

	fmt.Printf("📝 Stream.io Activity Created: user:%s posted loop (ID: %s) with BPM:%d, Key:%s\n",
		userID, activity.ID, activity.BPM, activity.Key)

	return nil
}

// GetUserTimeline gets the timeline feed for a user (posts from people they follow)
func (c *Client) GetUserTimeline(userID string, limit int, offset int) ([]*Activity, error) {
	ctx := context.Background()

	// Get the user's timeline feed
	timelineFeed, err := c.feedsClient.FlatFeed(FeedGroupTimeline, userID)
	if err != nil {
		return nil, fmt.Errorf("failed to get timeline feed: %w", err)
	}

	// Get activities with pagination
	resp, err := timelineFeed.GetActivities(ctx,
		stream.WithActivitiesLimit(limit),
		stream.WithActivitiesOffset(offset),
	)
	if err != nil {
		return nil, fmt.Errorf("failed to query timeline: %w", err)
	}

	// Convert Stream.io activities to our Activity type
	activities := make([]*Activity, 0, len(resp.Results))
	for _, act := range resp.Results {
		activity := convertStreamActivity(&act)
		activities = append(activities, activity)
	}

	fmt.Printf("📱 Fetched %d activities from timeline for user:%s\n", len(activities), userID)
	return activities, nil
}

// GetGlobalFeed gets the global feed of recent activities
func (c *Client) GetGlobalFeed(limit int, offset int) ([]*Activity, error) {
	ctx := context.Background()

	// Get the global feed
	globalFeed, err := c.feedsClient.FlatFeed(FeedGroupGlobal, "main")
	if err != nil {
		return nil, fmt.Errorf("failed to get global feed: %w", err)
	}

	// Get activities with pagination
	resp, err := globalFeed.GetActivities(ctx,
		stream.WithActivitiesLimit(limit),
		stream.WithActivitiesOffset(offset),
	)
	if err != nil {
		return nil, fmt.Errorf("failed to query global feed: %w", err)
	}

	// Convert Stream.io activities to our Activity type
	activities := make([]*Activity, 0, len(resp.Results))
	for _, act := range resp.Results {
		activity := convertStreamActivity(&act)
		activities = append(activities, activity)
	}

	fmt.Printf("🌍 Fetched %d activities from global feed\n", len(activities))
	return activities, nil
}

// FollowUser makes userID follow targetUserID
// This connects the user's timeline feed to the target's user feed
func (c *Client) FollowUser(userID, targetUserID string) error {
	ctx := context.Background()

	// Get the follower's timeline feed
	timelineFeed, err := c.feedsClient.FlatFeed(FeedGroupTimeline, userID)
	if err != nil {
		return fmt.Errorf("failed to get timeline feed: %w", err)
	}

	// Get the target's user feed
	targetFeed, err := c.feedsClient.FlatFeed(FeedGroupUser, targetUserID)
	if err != nil {
		return fmt.Errorf("failed to get target user feed: %w", err)
	}

	// Follow: user's timeline follows target's user feed
	// When target posts to their user feed, it appears in follower's timeline
	_, err = timelineFeed.Follow(ctx, targetFeed)
	if err != nil {
		return fmt.Errorf("failed to follow user: %w", err)
	}

	fmt.Printf("👥 %s followed %s\n", userID, targetUserID)
	return nil
}

// UnfollowUser makes userID unfollow targetUserID
func (c *Client) UnfollowUser(userID, targetUserID string) error {
	ctx := context.Background()

	// Get the follower's timeline feed
	timelineFeed, err := c.feedsClient.FlatFeed(FeedGroupTimeline, userID)
	if err != nil {
		return fmt.Errorf("failed to get timeline feed: %w", err)
	}

	// Get the target's user feed
	targetFeed, err := c.feedsClient.FlatFeed(FeedGroupUser, targetUserID)
	if err != nil {
		return fmt.Errorf("failed to get target user feed: %w", err)
	}

	// Unfollow: remove the follow relationship
	_, err = timelineFeed.Unfollow(ctx, targetFeed)
	if err != nil {
		return fmt.Errorf("failed to unfollow user: %w", err)
	}

	fmt.Printf("👥 %s unfollowed %s\n", userID, targetUserID)
	return nil
}

// AddReaction adds a reaction (like, play, etc.) to an activity with emoji support
func (c *Client) AddReaction(kind, userID, activityID string) error {
	return c.AddReactionWithEmoji(kind, userID, activityID, "")
}

// AddReactionWithEmoji adds a reaction with emoji support
func (c *Client) AddReactionWithEmoji(kind, userID, activityID, emoji string) error {
	ctx := context.Background()

	// Build reaction data
	var data map[string]any
	if emoji != "" {
		data = map[string]any{
			"emoji": emoji,
		}
	}

	// Create the reaction
	req := stream.AddReactionRequestObject{
		Kind:       kind,
		ActivityID: activityID,
		UserID:     userID,
		Data:       data,
	}

	_, err := c.feedsClient.Reactions().Add(ctx, req)
	if err != nil {
		return fmt.Errorf("failed to add reaction: %w", err)
	}

	emojiSuffix := ""
	if emoji != "" {
		emojiSuffix = fmt.Sprintf(" %s", emoji)
	}
	fmt.Printf("😀 %s reacted %s to %s%s\n", userID, kind, activityID, emojiSuffix)
	return nil
}

// RemoveReaction removes a reaction from an activity
func (c *Client) RemoveReaction(reactionID string) error {
	ctx := context.Background()

	_, err := c.feedsClient.Reactions().Delete(ctx, reactionID)
	if err != nil {
		return fmt.Errorf("failed to remove reaction: %w", err)
	}

	fmt.Printf("😀 Removed reaction %s\n", reactionID)
	return nil
}

// CreateToken creates a JWT token for user authentication
func (c *Client) CreateToken(userID string) (string, error) {
	token, err := c.ChatClient.CreateToken(userID, time.Time{})
	if err != nil {
		return "", fmt.Errorf("failed to create token: %w", err)
	}
	return token, nil
}

// convertStreamActivity converts Stream.io Activity to our Activity type
func convertStreamActivity(act *stream.Activity) *Activity {
	activity := &Activity{
		ID:    act.ID,
		Actor: act.Actor,
		Verb:  act.Verb,
	}

	// Extract timestamp
	if !act.Time.IsZero() {
		activity.Time = act.Time.Format(time.RFC3339)
	}

	// Extract custom data fields for loop metadata from Extra
	if act.Extra != nil {
		if audioURL, ok := act.Extra["audio_url"].(string); ok {
			activity.AudioURL = audioURL
		}
		if bpm, ok := act.Extra["bpm"].(float64); ok {
			activity.BPM = int(bpm)
		}
		if key, ok := act.Extra["key"].(string); ok {
			activity.Key = key
		}
		if daw, ok := act.Extra["daw"].(string); ok {
			activity.DAW = daw
		}
		if durationBars, ok := act.Extra["duration_bars"].(float64); ok {
			activity.DurationBars = int(durationBars)
		}
		if waveform, ok := act.Extra["waveform"].(string); ok {
			activity.Waveform = waveform
		}
		if genre, ok := act.Extra["genre"].([]interface{}); ok {
			for _, g := range genre {
				if gs, ok := g.(string); ok {
					activity.Genre = append(activity.Genre, gs)
				}
			}
		}
	}

	// Extract Object field
	activity.Object = act.Object

	return activity
}
