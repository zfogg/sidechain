package stream

import (
	"context"
	"fmt"
	"os"
	"time"

	"github.com/GetStream/getstream-go/v3"
	chat "github.com/GetStream/stream-chat-go/v5"
)

// Client wraps the Stream.io clients with Sidechain-specific functionality
type Client struct {
	FeedsClient *getstream.Stream
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

	// Initialize Feeds V3 client
	feedsClient, err := getstream.NewClient(apiKey, apiSecret)
	if err != nil {
		return nil, fmt.Errorf("failed to create Stream.io Feeds client: %w", err)
	}

	// Initialize Chat client
	chatClient, err := chat.NewClient(apiKey, apiSecret)
	if err != nil {
		return nil, fmt.Errorf("failed to create Stream.io Chat client: %w", err)
	}

	return &Client{
		FeedsClient: feedsClient, // This is *getstream.Client, not *getstream.Stream
		ChatClient:  chatClient,
	}, nil
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

	// Note: Feeds V3 users are created automatically when they perform actions
	// No explicit user creation needed for feeds
	return nil
}

// CreateLoopActivity creates an activity for a new loop post
func (c *Client) CreateLoopActivity(userID string, activity *Activity) error {
	// TODO: Implement proper V3 API once we get the exact method signatures
	// For now, create a working stub that logs what would be sent
	
	fmt.Printf("📝 Stream.io Activity: user:%s posted loop with BPM:%d, Key:%s\n", 
		userID, activity.BPM, activity.Key)
	
	// Generate mock activity ID for development
	activity.ID = fmt.Sprintf("activity_%s_%d", userID, time.Now().Unix())
	
	return nil
}

// GetUserTimeline gets the timeline feed for a user (posts from people they follow)
func (c *Client) GetUserTimeline(userID string, limit int, offset int) ([]*Activity, error) {
	// TODO: Implement real V3 API
	// For now, return mock data for development
	
	fmt.Printf("📱 Fetching timeline for user:%s (limit:%d, offset:%d)\n", userID, limit, offset)
	
	// Return mock timeline activities
	return []*Activity{
		{
			ID:           "timeline_activity_1",
			Actor:        "user:followed_producer",
			Verb:         "posted",
			Object:       "Amazing house loop",
			AudioURL:     "https://sidechain-media.s3.amazonaws.com/demo1.mp3",
			BPM:          128,
			Key:          "A minor",
			DAW:          "Ableton Live",
			DurationBars: 8,
			Genre:        []string{"House", "Electronic"},
		},
	}, nil
}

// GetGlobalFeed gets the global feed of recent activities
func (c *Client) GetGlobalFeed(limit int, offset int) ([]*Activity, error) {
	// TODO: Implement real V3 API
	// For now, return mock data for development
	
	fmt.Printf("🌍 Fetching global feed (limit:%d, offset:%d)\n", limit, offset)
	
	// Return mock global activities
	return []*Activity{
		{
			ID:           "global_activity_1",
			Actor:        "user:trending_producer",
			Verb:         "posted", 
			Object:       "Fire techno loop 🔥",
			AudioURL:     "https://sidechain-media.s3.amazonaws.com/demo2.mp3",
			BPM:          140,
			Key:          "F# minor",
			DAW:          "FL Studio",
			DurationBars: 16,
			Genre:        []string{"Techno"},
		},
		{
			ID:           "global_activity_2",
			Actor:        "user:chill_producer",
			Verb:         "posted",
			Object:       "Ambient vibes",
			AudioURL:     "https://sidechain-media.s3.amazonaws.com/demo3.mp3",
			BPM:          85,
			Key:          "C major",
			DAW:          "Logic Pro",
			DurationBars: 12,
			Genre:        []string{"Ambient", "Chill"},
		},
	}, nil
}

// FollowUser makes userID follow targetUserID
func (c *Client) FollowUser(userID, targetUserID string) error {
	// TODO: Implement real V3 follow API
	fmt.Printf("👥 %s followed %s\n", userID, targetUserID)
	return nil
}

// UnfollowUser makes userID unfollow targetUserID  
func (c *Client) UnfollowUser(userID, targetUserID string) error {
	// TODO: Implement real V3 unfollow API
	fmt.Printf("👥 %s unfollowed %s\n", userID, targetUserID)
	return nil
}

// AddReaction adds a reaction (like, play, etc.) to an activity with emoji support
func (c *Client) AddReaction(kind, userID, activityID string) error {
	return c.AddReactionWithEmoji(kind, userID, activityID, "")
}

// AddReactionWithEmoji adds a reaction with emoji support
func (c *Client) AddReactionWithEmoji(kind, userID, activityID, emoji string) error {
	// TODO: Implement real V3 reaction API
	emojiSuffix := ""
	if emoji != "" {
		emojiSuffix = fmt.Sprintf(" %s", emoji)
	}
	fmt.Printf("😀 %s reacted %s to %s%s\n", userID, kind, activityID, emojiSuffix)
	return nil
}

// RemoveReaction removes a reaction from an activity
func (c *Client) RemoveReaction(activityID, reactionType string) error {
	// TODO: Implement real V3 reaction removal API
	fmt.Printf("😀 Removed %s reaction from %s\n", reactionType, activityID)
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

// Helper function for development - will implement real V3 conversion later
func mockStreamActivity(id, actor, verb, object string) *Activity {
	return &Activity{
		ID:     id,
		Actor:  actor,
		Verb:   verb,
		Object: object,
	}
}

