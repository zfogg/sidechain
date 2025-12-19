package handlers

import (
	"context"
	"encoding/json"
	"fmt"
	"net/http"
	"os"
	"path/filepath"
	"strings"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/google/uuid"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/logger"
	"github.com/zfogg/sidechain/backend/internal/metrics"
	"github.com/zfogg/sidechain/backend/internal/models"
	"github.com/zfogg/sidechain/backend/internal/recommendations"
	"github.com/zfogg/sidechain/backend/internal/stream"
	"github.com/zfogg/sidechain/backend/internal/timeline"
	"github.com/zfogg/sidechain/backend/internal/util"
	"github.com/zfogg/sidechain/backend/internal/websocket"
	"gorm.io/gorm"
)

// TODO: - Test GetTimeline - authenticated user, pagination, empty feed
// TODO: - Test GetGlobalFeed - pagination, ordering
// TODO: - Test CreatePost - valid post, missing fields, audio URL validation
// TODO: - Test GetAggregatedTimeline - grouping
// TODO: - Test GetTrendingFeed - trending algorithm

// ============================================================================
// POSTS & FEEDS - PROFESSIONAL ENHANCEMENTS
// ============================================================================

// NOTE: Common enhancements (caching, analytics, rate limiting, moderation,
// search, webhooks, export, performance, anti-abuse) are documented in
// common_todos.go. See that file for shared TODO items.

// TODO: PROFESSIONAL-1.1 - Implement post-specific analytics tracking
// - Track view counts (increment on feed load)
// - Track play counts (increment on audio playback start)
// - Track play duration (total time listened)
// - Track share counts (when post URL is copied/shared)
// - Store in audio_posts table or separate analytics table
// - Return analytics in enriched feed responses

// TODO: PROFESSIONAL-1.3 - Add post quality scoring algorithm
// - Score based on engagement (likes, comments, plays, shares)
// - Score based on recency (newer posts weighted higher)
// - Score based on user reputation (verified/featured users)
// - Use for trending feed ranking and discovery
// - Cache scores and recalculate periodically (every hour)

// TODO: PROFESSIONAL-1.4 - Implement post deduplication
// - Detect duplicate audio uploads using audio fingerprinting
// - Warn users before posting duplicate content
// - Option to link to original post instead of creating duplicate
// - Prevent spam and improve feed quality

// TODO: PROFESSIONAL-1.5 - Add post-specific content moderation
// - Validate audio content (check for explicit content markers)
// - Validate metadata (prevent spam in titles/descriptions)
// - Audio fingerprinting for duplicate detection

// TODO: PROFESSIONAL-1.6 - Enhance feed filtering and search
// - Filter by BPM range (e.g., 120-130 BPM)
// - Filter by key (e.g., all posts in C major)
// - Filter by genre (multiple genres supported)
// - Filter by DAW (e.g., only Ableton Live posts)
// - Filter by date range (e.g., last 7 days)
// - Combine multiple filters (BPM + genre + key)

// TODO: PROFESSIONAL-1.7 - Implement feed personalization
// - Learn user preferences from listening history
// - Boost posts from genres user interacts with most
// - Boost posts from users user follows or interacts with
// - Downrank posts user has already seen
// - A/B testing framework for feed algorithms

// TODO: PROFESSIONAL-1.8 - Add feed pagination improvements
// - Cursor-based pagination (more efficient than offset)
// - Prevent duplicate posts when new posts are created during scrolling
// - Support "refresh" to get new posts since last view
// - Track last viewed timestamp per user per feed

// TODO: PROFESSIONAL-1.9 - Implement post drafts and scheduling
// - Allow users to save posts as drafts
// - Schedule posts for future publication
// - Edit drafts before posting
// - Auto-publish scheduled posts at specified time

// TODO: PROFESSIONAL-1.10 - Add post collaboration features
// - Tag other users in posts (collaborators)
// - Show "featuring" badge on posts with collaborators
// - Notify collaborators when post is published
// - Shared ownership for collaborative posts

// TODO: PROFESSIONAL-1.11 - Enhance post metadata
// - Support custom tags (beyond genre)
// - Support post descriptions/notes
// - Support BPM detection accuracy confidence
// - Support key detection confidence
// - Support audio analysis (energy, danceability, valence)

// TODO: PROFESSIONAL-1.12 - Implement post collections
// - Allow users to organize posts into collections/albums
// - Collections can be public or private
// - Support multiple collections per user
// - Collection covers and descriptions
// - Share collections with other users

// TODO: PROFESSIONAL-1.13 - Add post-specific export functionality
// - Export post metadata as JSON
// - Export post audio and MIDI together as ZIP
// - Export collection as playlist

// TODO: PROFESSIONAL-1.14 - Implement post-specific rate limiting
// - Limit posts per day (e.g., 10 posts/day for free users, unlimited for pro)
// - Limit feed requests per minute (prevent scraping)

// TODO: PROFESSIONAL-1.15 - Add post versioning
// - Track post edits/revisions
// - Show edit history (if enabled)
// - Allow reverting to previous versions
// - Timestamp when post was last modified

// TODO: PROFESSIONAL-1.16 - Enhance trending algorithm
// - Weight by engagement velocity (likes in first hour)
// - Weight by user engagement (active users vs bots)
// - Time decay (older engagement counts less)
// - Genre-specific trending (trending in electronic music)
// - Location-based trending (if location data available)

// TODO: PROFESSIONAL-1.17 - Implement feed aggregation improvements
// - Group posts by user (show "User posted 3 loops today")
// - Group posts by genre (show "5 new hip-hop loops")
// - Group posts by time period (show "Trending this week")
// - Customizable aggregation preferences

// TODO: PROFESSIONAL-1.18 - Add post embedding support
// - Generate embed code for posts (for sharing on websites)
// - OEmbed endpoint for rich previews
// - Support for social media preview cards (Open Graph, Twitter Cards)
// - Thumbnail generation for embeds

// TODO: PROFESSIONAL-1.19 - Implement feed-specific webhooks
// - Webhook for engagement milestones (100 likes, etc.)
// - Webhook for post updates (edits, deletions)
// - See common_todos.go for general webhook infrastructure

// TODO: PROFESSIONAL-1.20 - Add post-specific archival
// - Auto-archive old posts (older than 1 year, low engagement)
// - Manual archive option for users
// - Archived posts still accessible but not in main feed
// - See common_todos.go for general export/backup infrastructure

// GetTimeline gets the user's timeline feed
func (h *Handlers) GetTimeline(c *gin.Context) {
	userID, ok := util.GetUserIDFromContext(c)
	if !ok {
		return
	}
	limit := util.ParseInt(c.DefaultQuery("limit", "20"), 20)
	offset := util.ParseInt(c.DefaultQuery("offset", "0"), 0)

	activities, err := h.stream.GetUserTimeline(userID, limit, offset)
	if err != nil {
		util.RespondInternalError(c, "failed_to_get_timeline", "Failed to get timeline")
		return
	}

	// Enrich activities with waveform URLs from database (for old posts that don't have them)
	for _, activity := range activities {
		if activity.WaveformURL == "" && activity.Object != "" {
			// Extract post ID from object (e.g., "loop:post-id" -> "post-id")
			postID := activity.Object
			if len(postID) > 5 && postID[:5] == "loop:" {
				postID = postID[5:]
			}

			// Fetch waveform URL from database
			var post models.AudioPost
			if err := database.DB.Select("waveform_url").Where("id = ?", postID).First(&post).Error; err == nil {
				if post.WaveformURL != "" {
					activity.WaveformURL = post.WaveformURL
				}
			}
		}
	}

	c.JSON(http.StatusOK, gin.H{
		"activities": activities,
		"meta": gin.H{
			"limit":  limit,
			"offset": offset,
			"total":  len(activities),
		},
	})
}

// GetGlobalFeed gets the global feed
func (h *Handlers) GetGlobalFeed(c *gin.Context) {
	// Record feed generation timing metric
	startTime := time.Now()
	defer func() {
		duration := time.Since(startTime).Seconds()
		metrics.Get().FeedGenerationTime.WithLabelValues("global").Observe(duration)
	}()

	limit := util.ParseInt(c.DefaultQuery("limit", "20"), 20)
	offset := util.ParseInt(c.DefaultQuery("offset", "0"), 0)

	activities, err := h.stream.GetGlobalFeed(limit, offset)
	if err != nil {
		util.RespondInternalError(c, "failed_to_get_global_feed", "Failed to get global feed")
		return
	}

	// Enrich activities with waveform URLs from database (for old posts that don't have them)
	for _, activity := range activities {
		if activity.WaveformURL == "" && activity.Object != "" {
			// Extract post ID from object (e.g., "loop:post-id" -> "post-id")
			postID := activity.Object
			if len(postID) > 5 && postID[:5] == "loop:" {
				postID = postID[5:]
			}

			// Fetch waveform URL from database
			var post models.AudioPost
			if err := database.DB.Select("waveform_url").Where("id = ?", postID).First(&post).Error; err == nil {
				if post.WaveformURL != "" {
					activity.WaveformURL = post.WaveformURL
				}
			}
		}
	}

	c.JSON(http.StatusOK, gin.H{
		"activities": activities,
		"meta": gin.H{
			"limit":  limit,
			"offset": offset,
			"total":  len(activities),
		},
	})
}

// CreatePost creates a new post (alternative to upload)
func (h *Handlers) CreatePost(c *gin.Context) {
	userID, ok := util.GetUserIDFromContext(c)
	if !ok {
		return
	}

	var req struct {
		AudioURL     string   `json:"audio_url" binding:"required"`
		BPM          int      `json:"bpm"`
		Key          string   `json:"key"`
		DAW          string   `json:"daw"`
		DurationBars int      `json:"duration_bars"`
		Genre        []string `json:"genre"`
		// MIDI data
		MIDIData *models.MIDIData `json:"midi_data,omitempty"` // Optional inline MIDI data
		MIDIID   string           `json:"midi_id,omitempty"`   // Optional existing MIDI pattern ID
		// Remix support - alternative to /posts/:id/remix endpoint
		RemixOfPostID  string `json:"remix_of_post_id,omitempty"`  // Post being remixed
		RemixOfStoryID string `json:"remix_of_story_id,omitempty"` // Story being remixed
		RemixType      string `json:"remix_type,omitempty"`        // "audio", "midi", or "both"
	}

	if err := c.ShouldBindJSON(&req); err != nil {
		util.RespondBadRequest(c, "invalid_request", err.Error())
		return
	}

	postID := uuid.New().String()

	// Handle MIDI data - either use existing pattern or create new one
	var midiPatternID *string
	if req.MIDIID != "" {
		// Verify the MIDI pattern exists and user has access
		var pattern models.MIDIPattern
		if err := database.DB.First(&pattern, "id = ?", req.MIDIID).Error; err != nil {
			if util.HandleDBError(c, err, "midi_pattern") {
				return
			}
			util.RespondBadRequest(c, "invalid_midi_id")
			return
		}
		// Check access - must be public or owned by user
		if !pattern.IsPublic && pattern.UserID != userID {
			util.RespondForbidden(c, "midi_access_denied")
			return
		}
		midiPatternID = &req.MIDIID
	} else if req.MIDIData != nil && len(req.MIDIData.Events) > 0 {
		// Create new MIDI pattern from inline data
		pattern := &models.MIDIPattern{
			UserID:        userID,
			Name:          "MIDI from post",
			Events:        req.MIDIData.Events,
			Tempo:         req.MIDIData.Tempo,
			TimeSignature: req.MIDIData.TimeSignature,
			IsPublic:      true, // Public by default when attached to post
		}
		if err := database.DB.Create(pattern).Error; err != nil {
			util.RespondInternalError(c, "failed_to_create_midi")
			return
		}
		midiPatternID = &pattern.ID
	}

	activity := &stream.Activity{
		Actor:        "user:" + userID,
		Verb:         "posted",
		Object:       "loop:" + postID,
		ForeignID:    "loop:" + postID,
		AudioURL:     req.AudioURL,
		BPM:          req.BPM,
		Key:          req.Key,
		DAW:          req.DAW,
		DurationBars: req.DurationBars,
		Genre:        req.Genre,
	}

	// Add MIDI info to activity extra data
	if midiPatternID != nil {
		activity.Extra = map[string]interface{}{
			"has_midi":        true,
			"midi_pattern_id": *midiPatternID,
		}
	}

	if err := h.stream.CreateLoopActivity(userID, activity); err != nil {
		util.RespondInternalError(c, "failed_to_create_post", "Failed to create post")
		return
	}

	// Save AudioPost to database with Stream activity ID for future lookups
	post := &models.AudioPost{
		ID:               postID,
		UserID:           userID,
		AudioURL:         req.AudioURL,
		Filename:         req.AudioURL, // Use audio URL as fallback filename
		BPM:              req.BPM,
		Key:              req.Key,
		DAW:              req.DAW,
		DurationBars:     req.DurationBars,
		Genre:            models.StringArray(req.Genre),
		MIDIPatternID:    midiPatternID,
		StreamActivityID: activity.ID, // Store the Stream activity ID for likes/reactions
	}
	if err := database.DB.Create(post).Error; err != nil {
		fmt.Printf("Warning: Failed to save AudioPost %s to database: %v\n", postID, err)
		// Don't fail the request - the Stream activity was created successfully
	}

	// Broadcast feed invalidation for real-time updates + Activity broadcast
	if h.wsHandler != nil {
		go func() {
			// Fetch user details for activity broadcast
			var user models.User
			if err := database.DB.Select("id", "username", "display_name", "profile_picture_url").
				First(&user, "id = ?", userID).Error; err == nil {
				// Broadcast the actual activity so clients don't need to refetch
				activityPayload := &websocket.ActivityUpdatePayload{
					ID:          activity.ForeignID,
					Actor:       userID,
					ActorName:   user.Username,
					ActorAvatar: user.ProfilePictureURL,
					Verb:        "posted",
					Object:      postID,
					ObjectType:  "loop_post",
					AudioURL:    req.AudioURL,
					BPM:         req.BPM,
					Key:         req.Key,
					DAW:         req.DAW,
					Genre:       req.Genre,
					FeedTypes:   []string{"global", "trending", "timeline"},
				}
				h.wsHandler.BroadcastActivity(activityPayload)
			}

			// Also broadcast feed invalidation for clients that prefer to refetch
			h.wsHandler.BroadcastFeedInvalidation("global", "new_post")
			h.wsHandler.BroadcastFeedInvalidation("trending", "new_post")
			h.wsHandler.BroadcastFeedInvalidation("timeline", "new_post")

			// Notify followers that user posted something
			h.wsHandler.BroadcastTimelineUpdate(userID, "user_activity", 1)
		}()
	}

	// Index post to Elasticsearch
	if h.search != nil {
		go func() {
			// Fetch user for indexing
			var user models.User
			if err := database.DB.First(&user, "id = ?", userID).Error; err == nil {
				postDoc := map[string]interface{}{
					"id":            postID,
					"user_id":       userID,
					"username":      user.Username,
					"genre":         req.Genre,
					"bpm":           req.BPM,
					"key":           req.Key,
					"daw":           req.DAW,
					"like_count":    0,
					"play_count":    0,
					"comment_count": 0,
					"created_at":    time.Now().UTC(),
				}
				if err := h.search.IndexPost(c.Request.Context(), postID, postDoc); err != nil {
					fmt.Printf("Warning: Failed to index post %s in Elasticsearch: %v\n", postID, err)
				}
			}
		}()
	}

	response := gin.H{
		"post_id":   postID,
		"activity":  activity,
		"timestamp": time.Now().UTC(),
	}
	if midiPatternID != nil {
		response["midi_pattern_id"] = *midiPatternID
		response["has_midi"] = true
	}

	c.JSON(http.StatusCreated, response)
}

// GetEnrichedTimeline gets the user's timeline with reaction counts
// GET /api/feed/timeline/enriched
// Falls back to Gorse recommendations or recent posts if timeline is empty
func (h *Handlers) GetEnrichedTimeline(c *gin.Context) {
	currentUser, ok := util.GetUserFromContext(c)
	if !ok {
		return
	}

	limit := util.ParseInt(c.DefaultQuery("limit", "20"), 20)
	offset := util.ParseInt(c.DefaultQuery("offset", "0"), 0)

	activities, err := h.stream.GetEnrichedTimeline(currentUser.StreamUserID, limit, offset)
	if err != nil {
		util.RespondInternalError(c, "failed_to_get_timeline", err.Error())
		return
	}

	// If timeline is empty and we're on the first page, fall back to recommendations
	if len(activities) == 0 && offset == 0 {
		fallbackActivities := h.getFallbackFeed(currentUser.ID, limit)
		if len(fallbackActivities) > 0 {
			c.JSON(http.StatusOK, gin.H{
				"activities": fallbackActivities,
				"meta": gin.H{
					"limit":    limit,
					"offset":   offset,
					"count":    len(fallbackActivities),
					"fallback": true,
				},
			})
			return
		}
	}

	// Enrich activities with is_following state for each poster
	enrichedActivities := make([]interface{}, len(activities))

	for i, activity := range activities {
		// Convert EnrichedActivity to map using JSON marshal/unmarshal
		var activityMap map[string]interface{}
		activityBytes, _ := json.Marshal(activity)
		json.Unmarshal(activityBytes, &activityMap)

		// Extract poster's database user ID from actor field (format: "user:USER_ID")
		// Note: This should now be database ID after our fixes
		actorField := activity.Actor
		posterUserID := strings.TrimPrefix(actorField, "user:")

		// Query follow state using database IDs - no caching for fresh data
		isFollowing := false
		if posterUserID != currentUser.ID && posterUserID != "" {
			var err error
			isFollowing, err = h.stream.CheckIsFollowing(currentUser.ID, posterUserID)

			if err != nil {

				logger.WarnWithFields("Failed to check follow status", err)

				isFollowing = false

			}
		}

		// Add follow state to activity
		activityMap["is_following"] = isFollowing
		activityMap["is_own_post"] = (posterUserID == currentUser.ID)
		enrichedActivities[i] = activityMap
	}

	c.JSON(http.StatusOK, gin.H{
		"activities": enrichedActivities,
		"meta": gin.H{
			"limit":  limit,
			"offset": offset,
			"count":  len(activities),
		},
	})
}

// getFallbackFeed returns recommended posts when the user's timeline is empty
// First tries Gorse recommendations, then falls back to recent posts from database
func (h *Handlers) getFallbackFeed(userID string, limit int) []map[string]interface{} {
	// Get muted user IDs for filtering
	mutedUserIDs, err := GetMutedUserIDs(userID)
	if err != nil {
		logger.WarnWithFields("Failed to fetch muted users for "+userID+", feed may show muted content", err)
		mutedUserIDs = []string{} // Explicit empty list on error
	}
	mutedUserSet := make(map[string]bool, len(mutedUserIDs))
	for _, id := range mutedUserIDs {
		mutedUserSet[id] = true
	}

	// Try Gorse recommendations first
	gorseURL := os.Getenv("GORSE_URL")
	if gorseURL == "" {
		gorseURL = "http://localhost:8087"
	}
	gorseAPIKey := os.Getenv("GORSE_API_KEY")
	if gorseAPIKey == "" {
		gorseAPIKey = "sidechain_gorse_api_key"
	}

	recService := recommendations.NewGorseRESTClient(gorseURL, gorseAPIKey, database.DB)
	scores, err := recService.GetForYouFeed(userID, limit, 0)
	if err == nil && len(scores) > 0 {
		activities := make([]map[string]interface{}, 0, len(scores))
		for _, score := range scores {
			// Skip posts from muted users
			if mutedUserSet[score.Post.UserID] {
				continue
			}
			activity := map[string]interface{}{
				"id":                    score.Post.ID,
				"actor":                 "user:" + score.Post.UserID,
				"verb":                  "posted",
				"object":                "loop:" + score.Post.ID,
				"audio_url":             score.Post.AudioURL,
				"waveform_url":          score.Post.WaveformURL,
				"bpm":                   score.Post.BPM,
				"key":                   score.Post.Key,
				"daw":                   score.Post.DAW,
				"duration":              score.Post.Duration,
				"genre":                 score.Post.Genre,
				"time":                  score.Post.CreatedAt,
				"like_count":            score.Post.LikeCount,
				"play_count":            score.Post.PlayCount,
				"recommendation_reason": score.Reason,
				"is_recommended":        true,
			}
			activities = append(activities, activity)
		}
		return activities
	}

	// Fall back to recent posts from database
	// Include all public posts (including user's own) since this is a discovery feed
	// Exclude archived posts and posts from muted users
	query := database.DB.
		Where("is_public = ? AND is_archived = ? AND deleted_at IS NULL", true, false)

	// Filter out muted users
	if len(mutedUserIDs) > 0 {
		query = query.Where("user_id NOT IN ?", mutedUserIDs)
	}

	var posts []models.AudioPost
	if err := query.
		Order("created_at DESC").
		Limit(limit).
		Find(&posts).Error; err != nil {
		return nil
	}

	activities := make([]map[string]interface{}, 0, len(posts))
	for _, post := range posts {
		activity := map[string]interface{}{
			"id":             post.ID,
			"actor":          "user:" + post.UserID,
			"verb":           "posted",
			"object":         "loop:" + post.ID,
			"audio_url":      post.AudioURL,
			"waveform_url":   post.WaveformURL,
			"bpm":            post.BPM,
			"key":            post.Key,
			"daw":            post.DAW,
			"duration":       post.Duration,
			"genre":          post.Genre,
			"time":           post.CreatedAt,
			"like_count":     post.LikeCount,
			"play_count":     post.PlayCount,
			"is_recommended": true,
		}
		activities = append(activities, activity)
	}

	return activities
}

// GetUnifiedTimeline gets the user's unified timeline with Gorse recommendations
// This is the main timeline endpoint that combines:
// 1. Posts from followed users
// 2. Personalized Gorse recommendations
// 3. Trending posts
// 4. Recent posts as fallback
// GET /api/feed/unified
func (h *Handlers) GetUnifiedTimeline(c *gin.Context) {
	currentUser, ok := util.GetUserFromContext(c)
	if !ok {
		return
	}

	limit := util.ParseInt(c.DefaultQuery("limit", "20"), 20)
	offset := util.ParseInt(c.DefaultQuery("offset", "0"), 0)

	// Limit max request size
	if limit > 100 {
		limit = 100
	}

	// Get the stream client as concrete type for timeline service
	streamClient, ok := h.stream.(*stream.Client)
	if !ok {
		util.RespondInternalError(c, "stream_client_error", "Failed to get stream client")
		return
	}

	// Create timeline service and get unified timeline
	timelineSvc := timeline.NewService(streamClient)
	resp, err := timelineSvc.GetTimeline(context.Background(), currentUser.ID, limit, offset)
	if err != nil {
		util.RespondInternalError(c, "failed_to_get_timeline", err.Error())
		return
	}

	// Convert timeline items to activity format for plugin compatibility
	activities := make([]map[string]interface{}, 0, len(resp.Items))

	for _, item := range resp.Items {
		if item.Post == nil {
			continue
		}

		activity := map[string]interface{}{
			"id":           item.Post.ID,
			"actor":        "user:" + item.Post.UserID,
			"verb":         "posted",
			"object":       "loop:" + item.Post.ID,
			"audio_url":    item.Post.AudioURL,
			"waveform_url": item.Post.WaveformURL,
			"bpm":          item.Post.BPM,
			"key":          item.Post.Key,
			"daw":          item.Post.DAW,
			"duration":     item.Post.Duration,
			"genre":        item.Post.Genre,
			"time":         item.CreatedAt,
			"like_count":   item.Post.LikeCount,
			"play_count":   item.Post.PlayCount,
			"score":        item.Score,
			"source":       item.Source,
		}

		// Add recommendation reason if present
		if item.Reason != "" {
			activity["recommendation_reason"] = item.Reason
		}

		// Mark if this is from recommendations
		if item.Source == "gorse" || item.Source == "trending" {
			activity["is_recommended"] = true
		}

		// Add user info if available
		if item.User != nil {
			activity["user"] = map[string]interface{}{
				"id":           item.User.ID,
				"username":     item.User.Username,
				"display_name": item.User.DisplayName,
				"avatar_url":   item.User.GetAvatarURL(),
			}
		}

		// Enrich with is_following state
		// Use database IDs (NOT Stream IDs) to match FollowUser behavior
		posterUserID := item.Post.UserID
		isFollowing := false

		// Check if this post is the current user's own post
		if posterUserID != currentUser.ID && posterUserID != "" && item.User != nil && item.User.ID != "" {
			// Query follow state using DATABASE IDs - no caching to ensure fresh data
			var err error
			isFollowing, err = h.stream.CheckIsFollowing(currentUser.ID, item.User.ID)

			if err != nil {

				logger.WarnWithFields("Failed to check follow status", err)

				isFollowing = false

			}
		}

		activity["is_following"] = isFollowing

		// Mark if this is the current user's own post
		activity["is_own_post"] = (posterUserID == currentUser.ID)

		activities = append(activities, activity)
	}

	// Debug: Log first activity to see what we're returning
	if len(activities) > 0 {
		firstActivity := activities[0]
		if posterID, ok := firstActivity["actor"].(string); ok {
			fmt.Printf("📤 RESPONSE SAMPLE: actor=%s is_following=%v is_own_post=%v\n",
				posterID, firstActivity["is_following"], firstActivity["is_own_post"])
		}
	}

	c.JSON(http.StatusOK, gin.H{
		"activities": activities,
		"meta": gin.H{
			"limit":           resp.Meta.Limit,
			"offset":          resp.Meta.Offset,
			"count":           resp.Meta.Count,
			"has_more":        resp.Meta.HasMore,
			"following_count": resp.Meta.FollowingCount,
			"gorse_count":     resp.Meta.GorseCount,
			"trending_count":  resp.Meta.TrendingCount,
			"recent_count":    resp.Meta.RecentCount,
		},
	})
}

// GetEnrichedGlobalFeed gets the global feed with reaction counts
// GET /api/feed/global/enriched
func (h *Handlers) GetEnrichedGlobalFeed(c *gin.Context) {
	// Record feed generation timing metric
	startTime := time.Now()
	defer func() {
		duration := time.Since(startTime).Seconds()
		metrics.Get().FeedGenerationTime.WithLabelValues("enriched_global").Observe(duration)
	}()

	limit := util.ParseInt(c.DefaultQuery("limit", "20"), 20)
	offset := util.ParseInt(c.DefaultQuery("offset", "0"), 0)

	activities, err := h.stream.GetEnrichedGlobalFeed(limit, offset)
	if err != nil {
		util.RespondInternalError(c, "failed_to_get_global_feed", err.Error())
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"activities": activities,
		"meta": gin.H{
			"limit":  limit,
			"offset": offset,
			"count":  len(activities),
		},
	})
}

// GetAggregatedTimeline gets the user's aggregated timeline (grouped by user+day)
// GET /api/feed/timeline/aggregated
func (h *Handlers) GetAggregatedTimeline(c *gin.Context) {
	fmt.Printf("🎯 DEBUG: GetAggregatedTimeline called (NEW CODE)\n")
	currentUser, ok := util.GetUserFromContext(c)
	if !ok {
		return
	}

	limit := util.ParseInt(c.DefaultQuery("limit", "20"), 20)
	offset := util.ParseInt(c.DefaultQuery("offset", "0"), 0)

	// Use database-backed timeline service instead of Stream.io
	streamClient, ok := h.stream.(*stream.Client)
	if !ok {
		util.RespondInternalError(c, "stream_client_error", "Failed to get stream client")
		return
	}

	timelineSvc := timeline.NewService(streamClient)
	timelineResp, err := timelineSvc.GetTimeline(context.Background(), currentUser.ID, limit, offset)
	if err != nil {
		util.RespondInternalError(c, "failed_to_get_aggregated_timeline", err.Error())
		return
	}

	// Convert timeline items to activity format for plugin compatibility
	// The plugin's PostsFeed expects flat activities, not aggregated groups
	activities := make([]map[string]interface{}, 0, len(timelineResp.Items))

	for _, item := range timelineResp.Items {
		if item.Post == nil {
			continue
		}

		activity := map[string]interface{}{
			"id":           item.Post.ID,
			"actor":        "user:" + item.Post.UserID,
			"verb":         "posted",
			"object":       "loop:" + item.Post.ID,
			"audio_url":    item.Post.AudioURL,
			"waveform_url": item.Post.WaveformURL,
			"bpm":          item.Post.BPM,
			"key":          item.Post.Key,
			"daw":          item.Post.DAW,
			"duration":     item.Post.Duration,
			"genre":        item.Post.Genre,
			"time":         item.CreatedAt,
			"like_count":   item.Post.LikeCount,
			"play_count":   item.Post.PlayCount,
			"score":        item.Score,
			"source":       item.Source,
		}

		// Add recommendation reason if present
		if item.Reason != "" {
			activity["recommendation_reason"] = item.Reason
		}

		// Mark if this is from recommendations
		if item.Source == "gorse" || item.Source == "trending" {
			activity["is_recommended"] = true
		}

		// Add user info if available
		if item.User != nil {
			activity["user"] = map[string]interface{}{
				"id":           item.User.ID,
				"username":     item.User.Username,
				"display_name": item.User.DisplayName,
				"avatar_url":   item.User.GetAvatarURL(),
			}
		}

		activities = append(activities, activity)
	}

	c.JSON(http.StatusOK, gin.H{
		"posts": activities,
		"meta": gin.H{
			"limit":  limit,
			"offset": offset,
			"total":  len(activities),
			// Indicate if more posts are available
			"has_more": timelineResp.Meta.HasMore,
		},
	})
}

// GetTrendingFeed gets trending loops - returns flat activities for the plugin feed
// Activities are extracted from aggregated groups and sorted by engagement score
// GET /api/feed/trending
func (h *Handlers) GetTrendingFeed(c *gin.Context) {
	// Record feed generation timing metric
	startTime := time.Now()
	defer func() {
		duration := time.Since(startTime).Seconds()
		metrics.Get().FeedGenerationTime.WithLabelValues("trending").Observe(duration)
	}()

	limit := util.ParseInt(c.DefaultQuery("limit", "20"), 20)
	offset := util.ParseInt(c.DefaultQuery("offset", "0"), 0)

	resp, err := h.stream.GetTrendingFeed(limit, offset)
	if err != nil {
		util.RespondInternalError(c, "failed_to_get_trending_feed", err.Error())
		return
	}

	// Flatten groups into activities for the plugin's feed display
	// Each group contains activities - we extract them for flat display
	var activities []*stream.Activity
	for _, group := range resp.Groups {
		activities = append(activities, group.Activities...)
	}

	// Limit to requested amount after flattening
	if len(activities) > limit {
		activities = activities[:limit]
	}

	// Enrich activities with user data
	enrichedActivities := make([]map[string]interface{}, len(activities))
	for i, activity := range activities {
		// Convert to map
		activityMap := make(map[string]interface{})
		activityBytes, _ := json.Marshal(activity)
		json.Unmarshal(activityBytes, &activityMap)

		// Extract user ID from actor ("user:uuid")
		if actor, ok := activityMap["actor"].(string); ok && len(actor) > 5 {
			userID := actor[5:] // Strip "user:" prefix

			// Fetch user from database
			var user models.User
			if err := database.DB.Where("id = ?", userID).First(&user).Error; err == nil {
				activityMap["user"] = gin.H{
					"id":         user.ID,
					"username":   user.Username,
					"avatar_url": user.ProfilePictureURL,
				}
			} else {
				// User not found in database (stale Stream.io data)
				// Provide placeholder to prevent "Unknown" display
				activityMap["user"] = gin.H{
					"id":         userID,
					"username":   "DeletedUser",
					"avatar_url": "https://api.dicebear.com/7.x/avataaars/svg?seed=deleted",
				}
			}
		}

		// Enrich with waveform URL if missing (for old posts)
		if waveformURL, ok := activityMap["waveform_url"].(string); !ok || waveformURL == "" {
			if object, ok := activityMap["object"].(string); ok && len(object) > 5 && object[:5] == "loop:" {
				postID := object[5:]
				var post models.AudioPost
				if err := database.DB.Select("waveform_url").Where("id = ?", postID).First(&post).Error; err == nil {
					if post.WaveformURL != "" {
						activityMap["waveform_url"] = post.WaveformURL
					}
				}
			}
		}

		enrichedActivities[i] = activityMap
	}

	c.JSON(http.StatusOK, gin.H{
		"activities": enrichedActivities,
		"meta": gin.H{
			"limit":  limit,
			"offset": offset,
			"count":  len(enrichedActivities),
		},
	})
}

// GetTrendingFeedGrouped gets trending loops grouped by genre/time
// Returns aggregated groups for advanced display ("5 new electronic loops this week")
// GET /api/feed/trending/grouped
func (h *Handlers) GetTrendingFeedGrouped(c *gin.Context) {
	limit := util.ParseInt(c.DefaultQuery("limit", "20"), 20)
	offset := util.ParseInt(c.DefaultQuery("offset", "0"), 0)

	resp, err := h.stream.GetTrendingFeed(limit, offset)
	if err != nil {
		util.RespondInternalError(c, "failed_to_get_trending_feed", err.Error())
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"groups": resp.Groups,
		"meta": gin.H{
			"limit":  limit,
			"offset": offset,
			"count":  len(resp.Groups),
		},
	})
}

// GetUserActivitySummary gets aggregated activity for a user's profile
// GET /api/users/:id/activity
func (h *Handlers) GetUserActivitySummary(c *gin.Context) {
	targetUserID := c.Param("id")
	limit := util.ParseInt(c.DefaultQuery("limit", "10"), 10)

	// Validate user exists
	var user models.User
	if err := database.DB.First(&user, "id = ? OR stream_user_id = ?", targetUserID, targetUserID).Error; err != nil {
		if util.HandleDBError(c, err, "user") {
			return
		}
	}

	resp, err := h.stream.GetUserActivitySummary(user.StreamUserID, limit)
	if err != nil {
		util.RespondInternalError(c, "failed_to_get_activity_summary", err.Error())
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"user_id": user.ID,
		"groups":  resp.Groups,
		"meta": gin.H{
			"limit": limit,
			"count": len(resp.Groups),
		},
	})
}

// DeletePost deletes a post (soft delete)
// DELETE /api/v1/posts/:id
func (h *Handlers) DeletePost(c *gin.Context) {
	postID := c.Param("id")
	userID, ok := util.GetUserIDFromContext(c)
	if !ok {
		return
	}

	// Find the post
	var post models.AudioPost
	if err := database.DB.First(&post, "id = ?", postID).Error; err != nil {
		if util.HandleDBError(c, err, "post") {
			return
		}
	}

	// Check ownership
	if post.UserID != userID {
		util.RespondForbidden(c, "not_post_owner")
		return
	}

	// Soft delete the post
	if err := database.DB.Delete(&post).Error; err != nil {
		util.RespondInternalError(c, "failed_to_delete_post", err.Error())
		return
	}

	// Delete from Stream.io if it has a stream activity ID
	// Remove from origin feed (user feed) - this will cascade to all other feeds
	if h.stream != nil && post.StreamActivityID != "" {
		if err := h.stream.DeleteLoopActivity(post.UserID, post.StreamActivityID); err != nil {
			// Log but don't fail - post is already deleted in database
			fmt.Printf("Failed to delete Stream.io activity: %v\n", err)
		}
	}

	// Delete post from Elasticsearch index
	if h.search != nil {
		if err := h.search.DeletePost(c.Request.Context(), post.ID); err != nil {
			// Log but don't fail - post is already deleted in database
			fmt.Printf("Warning: Failed to delete post %s from Elasticsearch: %v\n", post.ID, err)
		}
	}

	c.JSON(http.StatusOK, gin.H{
		"message": "post_deleted",
	})
}

// ReportPost reports a post for moderation
// POST /api/v1/posts/:id/report
func (h *Handlers) ReportPost(c *gin.Context) {
	postID := c.Param("id")
	userID, ok := util.GetUserIDFromContext(c)
	if !ok {
		return
	}

	var req struct {
		Reason      string `json:"reason" binding:"required"`
		Description string `json:"description,omitempty"`
	}
	if err := c.ShouldBindJSON(&req); err != nil {
		util.RespondBadRequest(c, "invalid_request", err.Error())
		return
	}

	// Validate reason
	validReasons := []string{"spam", "harassment", "inappropriate", "copyright", "violence", "other"}
	valid := false
	for _, r := range validReasons {
		if req.Reason == r {
			valid = true
			break
		}
	}
	if !valid {
		util.RespondBadRequest(c, "invalid_reason", "Reason must be one of: spam, harassment, inappropriate, copyright, violence, other")
		return
	}

	// Find the post
	var post models.AudioPost
	if err := database.DB.First(&post, "id = ?", postID).Error; err != nil {
		if util.HandleDBError(c, err, "post") {
			return
		}
	}

	// Create report
	report := models.Report{
		ReporterID:   userID,
		TargetType:   models.ReportTargetPost,
		TargetID:     postID,
		TargetUserID: &post.UserID,
		Reason:       models.ReportReason(req.Reason),
		Description:  req.Description,
		Status:       models.ReportStatusPending,
	}

	if err := database.DB.Create(&report).Error; err != nil {
		util.RespondInternalError(c, "failed_to_create_report", err.Error())
		return
	}

	c.JSON(http.StatusCreated, gin.H{
		"message":   "report_created",
		"report_id": report.ID,
	})
}

// DownloadPost generates a download URL for a post and tracks the download
// POST /api/v1/posts/:id/download
func (h *Handlers) DownloadPost(c *gin.Context) {
	postID := c.Param("id")
	userID, ok := util.GetUserIDFromContext(c)
	if !ok {
		return
	}

	// Find the post
	var post models.AudioPost
	if err := database.DB.First(&post, "id = ?", postID).Error; err != nil {
		if util.HandleDBError(c, err, "post") {
			return
		}
	}

	// Check if post is public or user owns it
	if !post.IsPublic && post.UserID != userID {
		util.RespondForbidden(c, "post_not_accessible")
		return
	}

	if err := database.DB.Model(&post).UpdateColumn("download_count", gorm.Expr("download_count + 1")).Error; err != nil {
		logger.WarnWithFields("Failed to increment download count for post "+postID, err)
	}

	// Reload post to get updated download count
	database.DB.First(&post, "id = ?", postID)

	// Real-time Gorse feedback sync
	if h.gorse != nil {
		go func() {
			if err := h.gorse.SyncFeedback(userID, postID, "download"); err != nil {
				fmt.Printf("Warning: Failed to sync download to Gorse: %v\n", err)
			}
		}()
	}

	// Generate filename: {username}_{title}_{bpm}bpm.mp3
	// For now, use a simple format since we don't have title field
	var user models.User
	database.DB.First(&user, "id = ?", post.UserID)
	username := user.Username
	if username == "" {
		username = "user"
	}

	// Extract extension from audio URL or default to .mp3
	extension := ".mp3"
	if post.OriginalFilename != "" {
		ext := filepath.Ext(post.OriginalFilename)
		if ext != "" {
			extension = ext
		}
	}

	filename := fmt.Sprintf("%s_%s_%dbpm%s", username, postID[:8], post.BPM, extension)

	// Return download URL and metadata
	// Note: For now, we return the public audio URL directly
	// In the future, we can generate signed URLs with expiration
	c.JSON(http.StatusOK, gin.H{
		"download_url": post.AudioURL,
		"filename":     filename,
		"metadata": gin.H{
			"bpm":      post.BPM,
			"key":      post.Key,
			"duration": post.Duration,
			"genre":    post.Genre,
			"daw":      post.DAW,
		},
		"download_count": post.DownloadCount,
	})
}

// UpdateCommentAudience updates the comment audience setting for a post
// PUT /api/v1/posts/:id/comment-audience
func (h *Handlers) UpdateCommentAudience(c *gin.Context) {
	postID := c.Param("id")
	userID, ok := util.GetUserIDFromContext(c)
	if !ok {
		return
	}

	var req struct {
		CommentAudience string `json:"comment_audience" binding:"required"`
	}
	if err := c.ShouldBindJSON(&req); err != nil {
		util.RespondBadRequest(c, "invalid_request", err.Error())
		return
	}

	// Validate comment_audience value
	if req.CommentAudience != models.CommentAudienceEveryone &&
		req.CommentAudience != models.CommentAudienceFollowers &&
		req.CommentAudience != models.CommentAudienceOff {
		util.RespondBadRequest(c, "invalid_comment_audience", "Must be 'everyone', 'followers', or 'off'")
		return
	}

	// Find the post
	var post models.AudioPost
	if err := database.DB.First(&post, "id = ?", postID).Error; err != nil {
		util.RespondNotFound(c, "post_not_found")
		return
	}

	// Check ownership
	if post.UserID != userID {
		util.RespondForbidden(c, "not_post_owner")
		return
	}

	// Update the comment_audience
	if err := database.DB.Model(&post).Update("comment_audience", req.CommentAudience).Error; err != nil {
		util.RespondInternalError(c, "failed_to_update")
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"message":          "comment_audience_updated",
		"comment_audience": req.CommentAudience,
	})
}

// TrackPlay tracks a play event for analytics and recommendations
// POST /api/v1/posts/:id/play
func (h *Handlers) TrackPlay(c *gin.Context) {
	user, exists := c.Get("user")
	if !exists {
		util.RespondUnauthorized(c, "user_not_authenticated")
		return
	}
	currentUser := user.(*models.User)

	postID := c.Param("id")

	// Request body
	var req struct {
		Duration  float64 `json:"duration" binding:"required"` // Seconds listened
		Completed bool    `json:"completed"`                   // Did user listen to the full track?
	}

	if err := c.ShouldBindJSON(&req); err != nil {
		util.RespondBadRequest(c, "invalid_request", "Duration is required")
		return
	}

	// Verify post exists
	var post models.AudioPost
	if err := database.DB.First(&post, "id = ?", postID).Error; err != nil {
		if err == gorm.ErrRecordNotFound {
			util.RespondNotFound(c, "post_not_found")
			return
		}
		util.RespondInternalError(c, "query_failed", "Failed to fetch post")
		return
	}

	// Create play history record
	playHistory := models.PlayHistory{
		UserID:    currentUser.ID,
		PostID:    postID,
		Duration:  req.Duration,
		Completed: req.Completed,
	}

	if err := database.DB.Create(&playHistory).Error; err != nil {
		util.RespondInternalError(c, "failed_to_track_play", "Failed to save play history")
		return
	}

	// Real-time Gorse feedback sync
	// Completed plays are stronger signals (like), partial plays are weaker (view)
	if h.gorse != nil {
		go func() {
			feedbackType := "view"
			if req.Completed {
				feedbackType = "like" // Completed play = stronger signal
			}
			if err := h.gorse.SyncFeedback(currentUser.ID, postID, feedbackType); err != nil {
				fmt.Printf("Warning: Failed to sync play to Gorse: %v\n", err)
			}
		}()
	}

	c.JSON(http.StatusOK, gin.H{
		"status":    "play_tracked",
		"post_id":   postID,
		"duration":  req.Duration,
		"completed": req.Completed,
	})
}

// ViewPost tracks when a user views a post in their feed
// POST /api/v1/posts/:id/view
func (h *Handlers) ViewPost(c *gin.Context) {
	user, exists := c.Get("user")
	if !exists {
		util.RespondUnauthorized(c, "user_not_authenticated")
		return
	}
	currentUser := user.(*models.User)

	postID := c.Param("id")

	// Verify post exists
	var post models.AudioPost
	if err := database.DB.First(&post, "id = ?", postID).Error; err != nil {
		if err == gorm.ErrRecordNotFound {
			util.RespondNotFound(c, "post_not_found")
			return
		}
		util.RespondInternalError(c, "query_failed", "Failed to fetch post")
		return
	}

	// Don't track viewing your own post
	if post.UserID == currentUser.ID {
		c.JSON(http.StatusOK, gin.H{
			"status":  "own_post",
			"post_id": postID,
		})
		return
	}

	// Real-time Gorse feedback sync
	// View events are important for understanding user behavior even if they don't play
	if h.gorse != nil {
		go func() {
			if err := h.gorse.SyncFeedback(currentUser.ID, postID, "view"); err != nil {
				fmt.Printf("Warning: Failed to sync view to Gorse: %v\n", err)
			}
		}()
	}

	c.JSON(http.StatusOK, gin.H{
		"status":  "view_tracked",
		"post_id": postID,
	})
}
