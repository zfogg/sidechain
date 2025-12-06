package stream

import (
	"context"
	"encoding/json"
	"fmt"
	"os"
	"time"

	chat "github.com/GetStream/stream-chat-go/v5"
	stream "github.com/GetStream/stream-go2/v8"
)

// Feed group names configured in getstream.io dashboard
const (
	// Flat feeds (chronological)
	FeedGroupUser     = "user"     // Personal feed - user's own posts
	FeedGroupTimeline = "timeline" // Feed from followed users
	FeedGroupGlobal   = "global"   // All posts from all users

	// Notification feed (with read/seen tracking)
	FeedGroupNotification = "notification" // Likes, follows, comments notifications

	// Aggregated feeds (grouped activities)
	FeedGroupTimelineAggregated = "timeline_aggregated" // Grouped timeline ("X posted 3 loops today")
	FeedGroupTrending           = "trending"            // Grouped by genre/time
	FeedGroupUserActivity       = "user_activity"       // User's activity summary for profile
)

// Notification verbs - these become the "verb" field in notification activities
const (
	NotifVerbLike    = "like"    // Someone liked your loop
	NotifVerbFollow  = "follow"  // Someone followed you
	NotifVerbComment = "comment" // Someone commented on your loop
	NotifVerbMention = "mention" // Someone mentioned you in a comment
	NotifVerbRepost  = "repost"  // Someone reposted your loop
)

// Client wraps the getstream.io clients with Sidechain-specific functionality
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

// NewClient creates a new getstream.io client for Sidechain
func NewClient() (*Client, error) {
	apiKey := os.Getenv("STREAM_API_KEY")
	apiSecret := os.Getenv("STREAM_API_SECRET")

	if apiKey == "" || apiSecret == "" {
		return nil, fmt.Errorf("STREAM_API_KEY and STREAM_API_SECRET must be set")
	}

	// Initialize getstream.io Feeds V2 client
	feedsClient, err := stream.New(apiKey, apiSecret)
	if err != nil {
		return nil, fmt.Errorf("failed to create getstream.io Feeds client: %w", err)
	}

	// Initialize Chat client (separate SDK for chat features)
	chatClient, err := chat.NewClient(apiKey, apiSecret)
	if err != nil {
		return nil, fmt.Errorf("failed to create getstream.io Chat client: %w", err)
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

// CreateUser creates a getstream.io user for both feeds and chat
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

	// Build list of feeds to fan-out to using the "to" field
	toFeeds := []string{}

	// Also post to global feed (flat feed for chronological view)
	globalFeed, err := c.feedsClient.FlatFeed(FeedGroupGlobal, "main")
	if err != nil {
		return fmt.Errorf("failed to get global feed: %w", err)
	}
	toFeeds = append(toFeeds, globalFeed.ID())

	// Also post to trending feed (aggregated by genre + time for discovery)
	// This feed groups activities for "trending in house music today" style views
	trendingFeed, err := c.feedsClient.AggregatedFeed(FeedGroupTrending, "main")
	if err != nil {
		// Log but don't fail - trending feed might not exist yet
		fmt.Printf("⚠️ Could not get trending feed: %v\n", err)
	} else {
		toFeeds = append(toFeeds, trendingFeed.ID())
	}

	// Also post to user's activity feed (aggregated for profile "posted 5 loops this week")
	userActivityFeed, err := c.feedsClient.AggregatedFeed(FeedGroupUserActivity, userID)
	if err != nil {
		// Log but don't fail - user activity feed might not exist yet
		fmt.Printf("⚠️ Could not get user activity feed: %v\n", err)
	} else {
		toFeeds = append(toFeeds, userActivityFeed.ID())
	}

	// Set the "to" field to fan-out to all destination feeds
	streamActivity.To = toFeeds

	// Add activity to user's feed (will also propagate to all "to" feeds)
	resp, err := userFeed.AddActivity(ctx, streamActivity)
	if err != nil {
		return fmt.Errorf("failed to create getstream.io activity: %w", err)
	}

	// Update activity with returned ID and time
	activity.ID = resp.ID
	if !resp.Time.IsZero() {
		activity.Time = resp.Time.Format(time.RFC3339)
	}

	fmt.Printf("📝 getstream.io Activity Created: user:%s posted loop (ID: %s) with BPM:%d, Key:%s → fanned out to %d feeds\n",
		userID, activity.ID, activity.BPM, activity.Key, len(toFeeds)+1)

	return nil
}

// DeleteLoopActivity removes an activity from a user's feed
// This cascades to all feeds the activity was fanned out to
func (c *Client) DeleteLoopActivity(userID, activityID string) error {
	ctx := context.Background()

	userFeed, err := c.feedsClient.FlatFeed(FeedGroupUser, userID)
	if err != nil {
		return fmt.Errorf("failed to get user feed: %w", err)
	}

	_, err = userFeed.RemoveActivityByID(ctx, activityID)
	if err != nil {
		return fmt.Errorf("failed to delete activity: %w", err)
	}

	fmt.Printf("🗑️ Deleted activity %s from user:%s feed\n", activityID, userID)
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

	// Convert getstream.io activities to our Activity type
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

	// Convert getstream.io activities to our Activity type
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
// Also sets up the aggregated timeline follow and sends a notification
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

	// Also set up aggregated timeline follow (for grouped timeline view)
	err = c.FollowAggregatedFeed(userID, targetUserID)
	if err != nil {
		// Log but don't fail - aggregated feed might not exist yet
		fmt.Printf("⚠️ Could not set up aggregated timeline follow: %v\n", err)
	}

	// Send follow notification to the target user
	err = c.NotifyFollow(userID, targetUserID)
	if err != nil {
		// Log but don't fail - notification is best-effort
		fmt.Printf("⚠️ Failed to send follow notification: %v\n", err)
	}

	fmt.Printf("👥 %s followed %s\n", userID, targetUserID)
	return nil
}

// UnfollowUser makes userID unfollow targetUserID
// Also removes the aggregated timeline follow
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

	// Also remove aggregated timeline follow
	err = c.UnfollowAggregatedFeed(userID, targetUserID)
	if err != nil {
		// Log but don't fail - aggregated feed might not exist
		fmt.Printf("⚠️ Could not remove aggregated timeline follow: %v\n", err)
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

// AddReactionWithNotification adds a reaction and triggers a notification to the activity owner
// Use this when you know the activity owner's ID (e.g., from the loop record in your database)
// For "like" reactions, this will send a notification to the loop owner
func (c *Client) AddReactionWithNotification(kind, actorUserID, activityID, targetUserID, loopID, emoji string) error {
	// First add the reaction
	err := c.AddReactionWithEmoji(kind, actorUserID, activityID, emoji)
	if err != nil {
		return err
	}

	// Don't notify yourself
	if actorUserID == targetUserID {
		return nil
	}

	// Trigger notification for "like" reactions
	if kind == "like" {
		err = c.NotifyLike(actorUserID, targetUserID, loopID)
		if err != nil {
			// Log but don't fail the reaction - notification is best-effort
			fmt.Printf("⚠️ Failed to send like notification: %v\n", err)
		}
	}

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

// RemoveReactionByActivityAndUser removes a reaction from an activity for a specific user
// It finds the reaction ID first, then deletes it
func (c *Client) RemoveReactionByActivityAndUser(activityID, userID, kind string) error {
	ctx := context.Background()

	// Filter reactions by activity ID
	resp, err := c.feedsClient.Reactions().Filter(ctx,
		stream.ByActivityID(activityID),
	)
	if err != nil {
		return fmt.Errorf("failed to get reactions for activity: %w", err)
	}

	// Find the reaction by this user with the specified kind
	var reactionID string
	for _, r := range resp.Results {
		if r.UserID == userID && r.Kind == kind {
			reactionID = r.ID
			break
		}
	}

	if reactionID == "" {
		return fmt.Errorf("reaction not found for user %s on activity %s", userID, activityID)
	}

	// Delete the reaction
	_, err = c.feedsClient.Reactions().Delete(ctx, reactionID)
	if err != nil {
		return fmt.Errorf("failed to remove reaction: %w", err)
	}

	fmt.Printf("😀 Removed %s reaction %s by user %s on activity %s\n", kind, reactionID, userID, activityID)
	return nil
}

// FollowStats contains follower and following counts
type FollowStats struct {
	FollowerCount  int `json:"follower_count"`
	FollowingCount int `json:"following_count"`
}

// followStatsJSON is used to extract counts from getstream.io's FollowStatResponse via JSON
type followStatsJSON struct {
	Results struct {
		Followers struct {
			Count int `json:"count"`
		} `json:"followers"`
		Following struct {
			Count int `json:"count"`
		} `json:"following"`
	} `json:"results"`
}

// GetFollowStats returns the follower and following counts for a user
func (c *Client) GetFollowStats(userID string) (*FollowStats, error) {
	ctx := context.Background()

	// Get the user's feed to query follow stats
	// We check the user feed for followers (who follows this user's posts)
	userFeed, err := c.feedsClient.FlatFeed(FeedGroupUser, userID)
	if err != nil {
		return nil, fmt.Errorf("failed to get user feed: %w", err)
	}

	// Get follow stats - followers are timeline feeds following this user feed
	stats, err := userFeed.FollowStats(ctx,
		stream.WithFollowerSlugs(FeedGroupTimeline), // Timelines that follow this user feed
	)
	if err != nil {
		return nil, fmt.Errorf("failed to get follow stats: %w", err)
	}

	// Get following count from the timeline feed
	timelineFeed, err := c.feedsClient.FlatFeed(FeedGroupTimeline, userID)
	if err != nil {
		return nil, fmt.Errorf("failed to get timeline feed: %w", err)
	}

	followingStats, err := timelineFeed.FollowStats(ctx,
		stream.WithFollowingSlugs(FeedGroupUser), // User feeds that this timeline follows
	)
	if err != nil {
		return nil, fmt.Errorf("failed to get following stats: %w", err)
	}

	// Extract counts via JSON marshaling since SDK fields are unexported
	var followerData, followingData followStatsJSON

	followerJSON, _ := json.Marshal(stats)
	json.Unmarshal(followerJSON, &followerData)

	followingJSON, _ := json.Marshal(followingStats)
	json.Unmarshal(followingJSON, &followingData)

	result := &FollowStats{
		FollowerCount:  followerData.Results.Followers.Count,
		FollowingCount: followingData.Results.Following.Count,
	}

	fmt.Printf("📊 Follow stats for %s: %d followers, %d following\n",
		userID, result.FollowerCount, result.FollowingCount)
	return result, nil
}

// FollowRelation represents a follow relationship
type FollowRelation struct {
	FeedID   string `json:"feed_id"`
	TargetID string `json:"target_id"`
	UserID   string `json:"user_id"` // Extracted user ID
}

// GetFollowers returns a paginated list of users who follow this user
func (c *Client) GetFollowers(userID string, limit, offset int) ([]*FollowRelation, error) {
	ctx := context.Background()

	userFeed, err := c.feedsClient.FlatFeed(FeedGroupUser, userID)
	if err != nil {
		return nil, fmt.Errorf("failed to get user feed: %w", err)
	}

	resp, err := userFeed.GetFollowers(ctx,
		stream.WithFollowersLimit(limit),
		stream.WithFollowersOffset(offset),
	)
	if err != nil {
		return nil, fmt.Errorf("failed to get followers: %w", err)
	}

	followers := make([]*FollowRelation, 0, len(resp.Results))
	for _, f := range resp.Results {
		// Extract user ID from feed ID (format: "timeline:user123")
		userIDFromFeed := extractUserIDFromFeed(f.FeedID)
		followers = append(followers, &FollowRelation{
			FeedID:   f.FeedID,
			TargetID: f.TargetID,
			UserID:   userIDFromFeed,
		})
	}

	fmt.Printf("👥 Got %d followers for user %s\n", len(followers), userID)
	return followers, nil
}

// GetFollowing returns a paginated list of users this user follows
func (c *Client) GetFollowing(userID string, limit, offset int) ([]*FollowRelation, error) {
	ctx := context.Background()

	timelineFeed, err := c.feedsClient.FlatFeed(FeedGroupTimeline, userID)
	if err != nil {
		return nil, fmt.Errorf("failed to get timeline feed: %w", err)
	}

	resp, err := timelineFeed.GetFollowing(ctx,
		stream.WithFollowingLimit(limit),
		stream.WithFollowingOffset(offset),
	)
	if err != nil {
		return nil, fmt.Errorf("failed to get following: %w", err)
	}

	following := make([]*FollowRelation, 0, len(resp.Results))
	for _, f := range resp.Results {
		// Extract user ID from target ID (format: "user:user123")
		userIDFromFeed := extractUserIDFromFeed(f.TargetID)
		following = append(following, &FollowRelation{
			FeedID:   f.FeedID,
			TargetID: f.TargetID,
			UserID:   userIDFromFeed,
		})
	}

	fmt.Printf("👥 Got %d following for user %s\n", len(following), userID)
	return following, nil
}

// CheckIsFollowing checks if userID is following targetUserID
func (c *Client) CheckIsFollowing(userID, targetUserID string) (bool, error) {
	ctx := context.Background()

	timelineFeed, err := c.feedsClient.FlatFeed(FeedGroupTimeline, userID)
	if err != nil {
		return false, fmt.Errorf("failed to get timeline feed: %w", err)
	}

	targetFeedID := fmt.Sprintf("%s:%s", FeedGroupUser, targetUserID)
	resp, err := timelineFeed.GetFollowing(ctx,
		stream.WithFollowingFilter(targetFeedID),
	)
	if err != nil {
		return false, fmt.Errorf("failed to check following: %w", err)
	}

	return len(resp.Results) > 0, nil
}

// EnrichedActivity extends Activity with reaction data
type EnrichedActivity struct {
	*Activity
	ReactionCounts  map[string]int        `json:"reaction_counts"`
	OwnReactions    map[string][]string   `json:"own_reactions"` // kind -> reaction IDs
	LatestReactions map[string][]Reaction `json:"latest_reactions"`
}

// Reaction represents a reaction to an activity
type Reaction struct {
	ID        string         `json:"id"`
	Kind      string         `json:"kind"`
	UserID    string         `json:"user_id"`
	CreatedAt string         `json:"created_at"`
	Data      map[string]any `json:"data,omitempty"`
}

// GetEnrichedTimeline gets the timeline with reaction counts and own reactions
func (c *Client) GetEnrichedTimeline(userID string, limit, offset int) ([]*EnrichedActivity, error) {
	ctx := context.Background()

	timelineFeed, err := c.feedsClient.FlatFeed(FeedGroupTimeline, userID)
	if err != nil {
		return nil, fmt.Errorf("failed to get timeline feed: %w", err)
	}

	resp, err := timelineFeed.GetActivities(ctx,
		stream.WithActivitiesLimit(limit),
		stream.WithActivitiesOffset(offset),
		stream.WithEnrichReactionCounts(),
		stream.WithEnrichOwnReactions(),
		stream.WithEnrichRecentReactionsLimit(3), // Get 3 most recent reactions per activity
	)
	if err != nil {
		return nil, fmt.Errorf("failed to get enriched timeline: %w", err)
	}

	return convertEnrichedActivities(resp.Results), nil
}

// GetEnrichedGlobalFeed gets the global feed with reaction counts
func (c *Client) GetEnrichedGlobalFeed(limit, offset int) ([]*EnrichedActivity, error) {
	ctx := context.Background()

	globalFeed, err := c.feedsClient.FlatFeed(FeedGroupGlobal, "main")
	if err != nil {
		return nil, fmt.Errorf("failed to get global feed: %w", err)
	}

	resp, err := globalFeed.GetActivities(ctx,
		stream.WithActivitiesLimit(limit),
		stream.WithActivitiesOffset(offset),
		stream.WithEnrichReactionCounts(),
		stream.WithEnrichRecentReactionsLimit(3),
	)
	if err != nil {
		return nil, fmt.Errorf("failed to get enriched global feed: %w", err)
	}

	return convertEnrichedActivities(resp.Results), nil
}

// GetEnrichedUserFeed gets a user's own posts with reaction counts
func (c *Client) GetEnrichedUserFeed(userID string, limit, offset int) ([]*EnrichedActivity, error) {
	ctx := context.Background()

	userFeed, err := c.feedsClient.FlatFeed(FeedGroupUser, userID)
	if err != nil {
		return nil, fmt.Errorf("failed to get user feed: %w", err)
	}

	resp, err := userFeed.GetActivities(ctx,
		stream.WithActivitiesLimit(limit),
		stream.WithActivitiesOffset(offset),
		stream.WithEnrichReactionCounts(),
		stream.WithEnrichRecentReactionsLimit(3),
	)
	if err != nil {
		return nil, fmt.Errorf("failed to get enriched user feed: %w", err)
	}

	return convertEnrichedActivities(resp.Results), nil
}

// GetUserReactions gets reactions by a user (for "liked posts" feature)
func (c *Client) GetUserReactions(userID, kind string, limit int) ([]*Reaction, error) {
	ctx := context.Background()

	resp, err := c.feedsClient.Reactions().Filter(ctx,
		stream.ByUserID(userID),
		stream.WithLimit(limit),
		stream.WithActivityData(),
	)
	if err != nil {
		return nil, fmt.Errorf("failed to get user reactions: %w", err)
	}

	reactions := make([]*Reaction, 0, len(resp.Results))
	for _, r := range resp.Results {
		if kind != "" && r.Kind != kind {
			continue // Filter by kind if specified
		}
		reactions = append(reactions, &Reaction{
			ID:        r.ID,
			Kind:      r.Kind,
			UserID:    r.UserID,
			CreatedAt: r.CreatedAt.Format(time.RFC3339),
			Data:      r.Data,
		})
	}

	return reactions, nil
}

// Helper to extract user ID from feed ID (e.g., "timeline:user123" -> "user123")
func extractUserIDFromFeed(feedID string) string {
	for i := len(feedID) - 1; i >= 0; i-- {
		if feedID[i] == ':' {
			return feedID[i+1:]
		}
	}
	return feedID
}

// Helper to convert Stream activities to enriched activities
func convertEnrichedActivities(acts []stream.Activity) []*EnrichedActivity {
	enriched := make([]*EnrichedActivity, 0, len(acts))
	for _, act := range acts {
		activity := convertStreamActivity(&act)

		ea := &EnrichedActivity{
			Activity:        activity,
			ReactionCounts:  make(map[string]int),
			OwnReactions:    make(map[string][]string),
			LatestReactions: make(map[string][]Reaction),
		}

		// Extract reaction counts from Extra
		if act.Extra != nil {
			if counts, ok := act.Extra["reaction_counts"].(map[string]interface{}); ok {
				for kind, count := range counts {
					if c, ok := count.(float64); ok {
						ea.ReactionCounts[kind] = int(c)
					}
				}
			}
			if own, ok := act.Extra["own_reactions"].(map[string]interface{}); ok {
				for kind, reactions := range own {
					if rList, ok := reactions.([]interface{}); ok {
						for _, r := range rList {
							if rMap, ok := r.(map[string]interface{}); ok {
								if id, ok := rMap["id"].(string); ok {
									ea.OwnReactions[kind] = append(ea.OwnReactions[kind], id)
								}
							}
						}
					}
				}
			}
			if latest, ok := act.Extra["latest_reactions"].(map[string]interface{}); ok {
				for kind, reactions := range latest {
					if rList, ok := reactions.([]interface{}); ok {
						for _, r := range rList {
							if rMap, ok := r.(map[string]interface{}); ok {
								reaction := Reaction{
									Kind: kind,
								}
								if id, ok := rMap["id"].(string); ok {
									reaction.ID = id
								}
								if uid, ok := rMap["user_id"].(string); ok {
									reaction.UserID = uid
								}
								ea.LatestReactions[kind] = append(ea.LatestReactions[kind], reaction)
							}
						}
					}
				}
			}
		}

		enriched = append(enriched, ea)
	}
	return enriched
}

// CreateToken creates a JWT token for user authentication
// If expiration is zero time, token never expires
func (c *Client) CreateToken(userID string, expiration time.Time) (string, error) {
	token, err := c.ChatClient.CreateToken(userID, expiration)
	if err != nil {
		return "", fmt.Errorf("failed to create token: %w", err)
	}
	return token, nil
}

// =============================================================================
// NOTIFICATION FEED TYPES AND METHODS
// =============================================================================

// NotificationGroup represents a grouped notification from getstream.io
//
//	getstream.io notification feeds automatically group activities based on the
//
// aggregation_format configured in the dashboard (e.g., "{{ verb }}_{{ time.strftime('%Y-%m-%d') }}")
type NotificationGroup struct {
	ID            string      `json:"id"`
	Group         string      `json:"group"`          // The aggregation key (e.g., "like_2024-01-15")
	Verb          string      `json:"verb"`           // The verb for this group
	ActivityCount int         `json:"activity_count"` // Number of activities in group
	ActorCount    int         `json:"actor_count"`    // Number of unique actors
	Activities    []*Activity `json:"activities"`     // The actual activities
	IsRead        bool        `json:"is_read"`        // Has user read this group?
	IsSeen        bool        `json:"is_seen"`        // Has user seen this group?
	CreatedAt     string      `json:"created_at"`
	UpdatedAt     string      `json:"updated_at"`
}

// NotificationResponse wraps the notification feed response with counts
type NotificationResponse struct {
	Groups []*NotificationGroup `json:"groups"`
	Unseen int                  `json:"unseen"` // Total unseen notification groups
	Unread int                  `json:"unread"` // Total unread notification groups
}

// GetNotifications retrieves the notification feed for a user
// Returns grouped notifications with unseen/unread counts for badge display
func (c *Client) GetNotifications(userID string, limit, offset int) (*NotificationResponse, error) {
	ctx := context.Background()

	// Get the user's notification feed (Notification type feed)
	notifFeed, err := c.feedsClient.NotificationFeed(FeedGroupNotification, userID)
	if err != nil {
		return nil, fmt.Errorf("failed to get notification feed: %w", err)
	}

	// Get activities with pagination
	resp, err := notifFeed.GetActivities(ctx,
		stream.WithActivitiesLimit(limit),
		stream.WithActivitiesOffset(offset),
	)
	if err != nil {
		return nil, fmt.Errorf("failed to query notifications: %w", err)
	}

	// Convert response to our types
	groups := make([]*NotificationGroup, 0, len(resp.Results))
	for _, group := range resp.Results {
		activities := make([]*Activity, 0, len(group.Activities))
		for _, act := range group.Activities {
			activities = append(activities, convertStreamActivity(&act))
		}

		ng := &NotificationGroup{
			ID:            group.ID,
			Group:         group.Group,
			Verb:          group.Verb,
			ActivityCount: group.ActivityCount,
			ActorCount:    group.ActorCount,
			Activities:    activities,
			IsRead:        group.IsRead,
			IsSeen:        group.IsSeen,
		}

		// Extract timestamps
		if !group.CreatedAt.IsZero() {
			ng.CreatedAt = group.CreatedAt.Format(time.RFC3339)
		}
		if !group.UpdatedAt.IsZero() {
			ng.UpdatedAt = group.UpdatedAt.Format(time.RFC3339)
		}

		groups = append(groups, ng)
	}

	result := &NotificationResponse{
		Groups: groups,
		Unseen: resp.Unseen,
		Unread: resp.Unread,
	}

	fmt.Printf("🔔 Fetched %d notification groups for user:%s (unseen: %d, unread: %d)\n",
		len(groups), userID, resp.Unseen, resp.Unread)
	return result, nil
}

// MarkNotificationsRead marks all notifications as read for a user
// Call this when the user opens the notifications panel
//
//	getstream.io marks notifications via query options when fetching
func (c *Client) MarkNotificationsRead(userID string) error {
	ctx := context.Background()

	notifFeed, err := c.feedsClient.NotificationFeed(FeedGroupNotification, userID)
	if err != nil {
		return fmt.Errorf("failed to get notification feed: %w", err)
	}

	// Mark all as read by fetching with the mark_read option
	// Passing true marks ALL notifications as read
	_, err = notifFeed.GetActivities(ctx,
		stream.WithActivitiesLimit(1), // Minimal fetch
		stream.WithNotificationsMarkRead(true),
	)
	if err != nil {
		return fmt.Errorf("failed to mark notifications read: %w", err)
	}

	fmt.Printf("✓ Marked all notifications read for user:%s\n", userID)
	return nil
}

// MarkNotificationsSeen marks all notifications as seen for a user
// Call this when the user views the notification icon (clears badge count)
//
//	getstream.io marks notifications via query options when fetching
func (c *Client) MarkNotificationsSeen(userID string) error {
	ctx := context.Background()

	notifFeed, err := c.feedsClient.NotificationFeed(FeedGroupNotification, userID)
	if err != nil {
		return fmt.Errorf("failed to get notification feed: %w", err)
	}

	// Mark all as seen by fetching with the mark_seen option
	// Passing true marks ALL notifications as seen
	_, err = notifFeed.GetActivities(ctx,
		stream.WithActivitiesLimit(1), // Minimal fetch
		stream.WithNotificationsMarkSeen(true),
	)
	if err != nil {
		return fmt.Errorf("failed to mark notifications seen: %w", err)
	}

	fmt.Printf("👁 Marked all notifications seen for user:%s\n", userID)
	return nil
}

// GetNotificationCounts returns just the unseen/unread counts for badge display
// This is more efficient than fetching full notifications when you only need counts
func (c *Client) GetNotificationCounts(userID string) (unseen, unread int, err error) {
	ctx := context.Background()

	notifFeed, err := c.feedsClient.NotificationFeed(FeedGroupNotification, userID)
	if err != nil {
		return 0, 0, fmt.Errorf("failed to get notification feed: %w", err)
	}

	// Request minimal data - just need the counts
	resp, err := notifFeed.GetActivities(ctx,
		stream.WithActivitiesLimit(1),
	)
	if err != nil {
		return 0, 0, fmt.Errorf("failed to query notification counts: %w", err)
	}

	return resp.Unseen, resp.Unread, nil
}

// AddToNotificationFeed adds a notification activity to a user's notification feed
// This is used internally to trigger notifications for likes, follows, comments, etc.
func (c *Client) AddToNotificationFeed(targetUserID string, activity *Activity) error {
	ctx := context.Background()

	notifFeed, err := c.feedsClient.NotificationFeed(FeedGroupNotification, targetUserID)
	if err != nil {
		return fmt.Errorf("failed to get notification feed: %w", err)
	}

	// Build the notification activity
	streamActivity := stream.Activity{
		Actor:  activity.Actor,
		Verb:   activity.Verb,
		Object: activity.Object,
		Extra:  make(map[string]any),
	}

	// Copy extra fields
	if activity.Extra != nil {
		for k, v := range activity.Extra {
			streamActivity.Extra[k] = v
		}
	}

	// Add activity to notification feed
	_, err = notifFeed.AddActivity(ctx, streamActivity)
	if err != nil {
		return fmt.Errorf("failed to add notification activity: %w", err)
	}

	fmt.Printf("🔔 Added %s notification to user:%s\n", activity.Verb, targetUserID)
	return nil
}

// NotifyLike sends a like notification to the loop owner
func (c *Client) NotifyLike(actorUserID, targetUserID, loopID string) error {
	activity := &Activity{
		Actor:  fmt.Sprintf("user:%s", actorUserID),
		Verb:   NotifVerbLike,
		Object: fmt.Sprintf("loop:%s", loopID),
		Extra: map[string]interface{}{
			"actor_id": actorUserID,
			"loop_id":  loopID,
		},
	}
	return c.AddToNotificationFeed(targetUserID, activity)
}

// NotifyFollow sends a follow notification to the followed user
func (c *Client) NotifyFollow(actorUserID, targetUserID string) error {
	activity := &Activity{
		Actor:  fmt.Sprintf("user:%s", actorUserID),
		Verb:   NotifVerbFollow,
		Object: fmt.Sprintf("user:%s", targetUserID),
		Extra: map[string]interface{}{
			"actor_id":  actorUserID,
			"target_id": targetUserID,
		},
	}
	return c.AddToNotificationFeed(targetUserID, activity)
}

// NotifyComment sends a comment notification to the loop owner
func (c *Client) NotifyComment(actorUserID, targetUserID, loopID, commentText string) error {
	activity := &Activity{
		Actor:  fmt.Sprintf("user:%s", actorUserID),
		Verb:   NotifVerbComment,
		Object: fmt.Sprintf("loop:%s", loopID),
		Extra: map[string]interface{}{
			"actor_id": actorUserID,
			"loop_id":  loopID,
			"preview":  truncateString(commentText, 100), // Preview of comment
		},
	}
	return c.AddToNotificationFeed(targetUserID, activity)
}

// NotifyMention sends a mention notification when a user is @mentioned in a comment
func (c *Client) NotifyMention(actorUserID, targetUserID, loopID, commentID string) error {
	activity := &Activity{
		Actor:  fmt.Sprintf("user:%s", actorUserID),
		Verb:   NotifVerbMention,
		Object: fmt.Sprintf("comment:%s", commentID),
		Extra: map[string]interface{}{
			"actor_id":   actorUserID,
			"loop_id":    loopID,
			"comment_id": commentID,
		},
	}
	return c.AddToNotificationFeed(targetUserID, activity)
}

// =============================================================================
// AGGREGATED FEED TYPES AND METHODS
// =============================================================================

// AggregatedGroup represents a grouped set of activities from an aggregated feed
// These are grouped by the aggregation_format in getstream.io dashboard
type AggregatedGroup struct {
	ID            string      `json:"id"`
	Group         string      `json:"group"`          // The aggregation key
	Verb          string      `json:"verb"`           // Common verb for the group
	ActivityCount int         `json:"activity_count"` // Number of activities
	ActorCount    int         `json:"actor_count"`    // Number of unique actors
	Activities    []*Activity `json:"activities"`     // The grouped activities
	CreatedAt     string      `json:"created_at"`
	UpdatedAt     string      `json:"updated_at"`
}

// AggregatedFeedResponse wraps an aggregated feed response
type AggregatedFeedResponse struct {
	Groups []*AggregatedGroup `json:"groups"`
}

// GetAggregatedTimeline gets the aggregated timeline feed for a user
// Activities are grouped by the aggregation_format configured in dashboard
// Example: "{{ actor }}_{{ verb }}_{{ time.strftime('%Y-%m-%d') }}" groups by user+verb+day
func (c *Client) GetAggregatedTimeline(userID string, limit, offset int) (*AggregatedFeedResponse, error) {
	ctx := context.Background()

	// Get the user's aggregated timeline feed
	aggFeed, err := c.feedsClient.AggregatedFeed(FeedGroupTimelineAggregated, userID)
	if err != nil {
		return nil, fmt.Errorf("failed to get aggregated timeline feed: %w", err)
	}

	resp, err := aggFeed.GetActivities(ctx,
		stream.WithActivitiesLimit(limit),
		stream.WithActivitiesOffset(offset),
	)
	if err != nil {
		return nil, fmt.Errorf("failed to query aggregated timeline: %w", err)
	}

	groups := convertAggregatedGroups(resp.Results)
	fmt.Printf("📊 Fetched %d aggregated groups from timeline for user:%s\n", len(groups), userID)

	return &AggregatedFeedResponse{Groups: groups}, nil
}

// GetTrendingFeed gets trending loops grouped by genre/time
// Requires "trending" aggregated feed configured in getstream.io dashboard
func (c *Client) GetTrendingFeed(limit, offset int) (*AggregatedFeedResponse, error) {
	ctx := context.Background()

	trendingFeed, err := c.feedsClient.AggregatedFeed(FeedGroupTrending, "main")
	if err != nil {
		return nil, fmt.Errorf("failed to get trending feed: %w", err)
	}

	resp, err := trendingFeed.GetActivities(ctx,
		stream.WithActivitiesLimit(limit),
		stream.WithActivitiesOffset(offset),
	)
	if err != nil {
		return nil, fmt.Errorf("failed to query trending feed: %w", err)
	}

	groups := convertAggregatedGroups(resp.Results)
	fmt.Printf("🔥 Fetched %d trending groups\n", len(groups))

	return &AggregatedFeedResponse{Groups: groups}, nil
}

// GetUserActivitySummary gets aggregated activity for a user's profile
// Groups the user's activities for display like "Posted 5 loops this week"
func (c *Client) GetUserActivitySummary(userID string, limit int) (*AggregatedFeedResponse, error) {
	ctx := context.Background()

	activityFeed, err := c.feedsClient.AggregatedFeed(FeedGroupUserActivity, userID)
	if err != nil {
		return nil, fmt.Errorf("failed to get user activity feed: %w", err)
	}

	resp, err := activityFeed.GetActivities(ctx,
		stream.WithActivitiesLimit(limit),
	)
	if err != nil {
		return nil, fmt.Errorf("failed to query user activity: %w", err)
	}

	groups := convertAggregatedGroups(resp.Results)
	fmt.Printf("📈 Fetched %d activity groups for user:%s profile\n", len(groups), userID)

	return &AggregatedFeedResponse{Groups: groups}, nil
}

// AddToAggregatedFeed adds an activity to an aggregated feed
// The activity will be grouped based on the feed's aggregation_format
func (c *Client) AddToAggregatedFeed(feedGroup, feedID string, activity *Activity) error {
	ctx := context.Background()

	aggFeed, err := c.feedsClient.AggregatedFeed(feedGroup, feedID)
	if err != nil {
		return fmt.Errorf("failed to get aggregated feed: %w", err)
	}

	streamActivity := stream.Activity{
		Actor:  activity.Actor,
		Verb:   activity.Verb,
		Object: activity.Object,
		Extra:  make(map[string]any),
	}

	// Copy extra fields for loop metadata
	if activity.AudioURL != "" {
		streamActivity.Extra["audio_url"] = activity.AudioURL
	}
	if activity.BPM > 0 {
		streamActivity.Extra["bpm"] = activity.BPM
	}
	if activity.Key != "" {
		streamActivity.Extra["key"] = activity.Key
	}
	if len(activity.Genre) > 0 {
		streamActivity.Extra["genre"] = activity.Genre
	}
	if activity.Extra != nil {
		for k, v := range activity.Extra {
			streamActivity.Extra[k] = v
		}
	}

	_, err = aggFeed.AddActivity(ctx, streamActivity)
	if err != nil {
		return fmt.Errorf("failed to add to aggregated feed: %w", err)
	}

	fmt.Printf("📊 Added activity to aggregated feed %s:%s\n", feedGroup, feedID)
	return nil
}

// FollowAggregatedFeed makes an aggregated feed follow a source feed
// Used to set up the aggregated timeline to receive activities from followed users
func (c *Client) FollowAggregatedFeed(userID, targetUserID string) error {
	ctx := context.Background()

	// Get user's aggregated timeline
	aggTimeline, err := c.feedsClient.AggregatedFeed(FeedGroupTimelineAggregated, userID)
	if err != nil {
		return fmt.Errorf("failed to get aggregated timeline: %w", err)
	}

	// Get target's user feed
	targetFeed, err := c.feedsClient.FlatFeed(FeedGroupUser, targetUserID)
	if err != nil {
		return fmt.Errorf("failed to get target user feed: %w", err)
	}

	// Follow the target's user feed
	_, err = aggTimeline.Follow(ctx, targetFeed)
	if err != nil {
		return fmt.Errorf("failed to follow for aggregated timeline: %w", err)
	}

	fmt.Printf("📊 %s aggregated timeline now follows %s\n", userID, targetUserID)
	return nil
}

// UnfollowAggregatedFeed removes the follow relationship for aggregated timeline
func (c *Client) UnfollowAggregatedFeed(userID, targetUserID string) error {
	ctx := context.Background()

	aggTimeline, err := c.feedsClient.AggregatedFeed(FeedGroupTimelineAggregated, userID)
	if err != nil {
		return fmt.Errorf("failed to get aggregated timeline: %w", err)
	}

	targetFeed, err := c.feedsClient.FlatFeed(FeedGroupUser, targetUserID)
	if err != nil {
		return fmt.Errorf("failed to get target user feed: %w", err)
	}

	_, err = aggTimeline.Unfollow(ctx, targetFeed)
	if err != nil {
		return fmt.Errorf("failed to unfollow for aggregated timeline: %w", err)
	}

	fmt.Printf("📊 %s aggregated timeline unfollowed %s\n", userID, targetUserID)
	return nil
}

// Helper to convert aggregated activity groups
func convertAggregatedGroups(groups []stream.ActivityGroup) []*AggregatedGroup {
	result := make([]*AggregatedGroup, 0, len(groups))
	for _, group := range groups {
		activities := make([]*Activity, 0, len(group.Activities))
		for _, act := range group.Activities {
			activities = append(activities, convertStreamActivity(&act))
		}

		ag := &AggregatedGroup{
			ID:            group.ID,
			Group:         group.Group,
			Verb:          group.Verb,
			ActivityCount: group.ActivityCount,
			ActorCount:    group.ActorCount,
			Activities:    activities,
		}

		if !group.CreatedAt.IsZero() {
			ag.CreatedAt = group.CreatedAt.Format(time.RFC3339)
		}
		if !group.UpdatedAt.IsZero() {
			ag.UpdatedAt = group.UpdatedAt.Format(time.RFC3339)
		}

		result = append(result, ag)
	}
	return result
}

// Helper to truncate strings for notification previews
func truncateString(s string, maxLen int) string {
	if len(s) <= maxLen {
		return s
	}
	return s[:maxLen-3] + "..."
}

// =============================================================================
// EXISTING HELPER FUNCTIONS
// =============================================================================

// convertStreamActivity converts getstream.io Activity to our Activity type
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
		// Extract comment_count if present (added by enrichment function)
		if commentCount, ok := act.Extra["comment_count"].(float64); ok {
			if activity.Extra == nil {
				activity.Extra = make(map[string]interface{})
			}
			activity.Extra["comment_count"] = int(commentCount)
		}
	}

	// Extract Object field
	activity.Object = act.Object

	return activity
}
