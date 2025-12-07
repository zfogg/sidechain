package stream

import "time"

// StreamClientInterface defines the interface for Stream.io client operations.
// This enables mocking for unit tests without requiring a real getstream.io connection.
//
// The interface covers all methods used by handlers for:
// - Feed operations (create, read, timeline, global)
// - Social operations (follow, unfollow, reactions)
// - Notification operations (get, mark read/seen)
// - User operations (create, tokens)
type StreamClientInterface interface {
	// User operations
	CreateUser(userID, username string) error
	CreateToken(userID string, expiration time.Time) (string, error)

	// Feed operations - create, read, delete
	CreateLoopActivity(userID string, activity *Activity) error
	DeleteLoopActivity(userID, activityID string) error
	GetUserTimeline(userID string, limit int, offset int) ([]*Activity, error)
	GetGlobalFeed(limit int, offset int) ([]*Activity, error)

	// Enriched feed operations (includes user data, reactions)
	GetEnrichedTimeline(userID string, limit, offset int) ([]*EnrichedActivity, error)
	GetEnrichedGlobalFeed(limit, offset int) ([]*EnrichedActivity, error)
	GetEnrichedUserFeed(userID string, limit, offset int) ([]*EnrichedActivity, error)

	// Aggregated feed operations
	GetAggregatedTimeline(userID string, limit, offset int) (*AggregatedFeedResponse, error)
	GetTrendingFeed(limit, offset int) (*AggregatedFeedResponse, error)
	GetUserActivitySummary(userID string, limit int) (*AggregatedFeedResponse, error)

	// Follow operations
	FollowUser(userID, targetUserID string) error
	UnfollowUser(userID, targetUserID string) error
	GetFollowStats(userID string) (*FollowStats, error)
	GetFollowers(userID string, limit, offset int) ([]*FollowRelation, error)
	GetFollowing(userID string, limit, offset int) ([]*FollowRelation, error)
	CheckIsFollowing(userID, targetUserID string) (bool, error)

	// Reaction operations
	AddReaction(kind, userID, activityID string) error
	AddReactionWithEmoji(kind, userID, activityID, emoji string) error
	AddReactionWithNotification(kind, actorUserID, activityID, targetUserID, loopID, emoji string) error
	RemoveReaction(reactionID string) error
	RemoveReactionByActivityAndUser(activityID, userID, kind string) error
	GetUserReactions(userID, kind string, limit int) ([]*Reaction, error)

	// Notification operations
	GetNotifications(userID string, limit, offset int) (*NotificationResponse, error)
	GetNotificationCounts(userID string) (unseen, unread int, err error)
	MarkNotificationsRead(userID string) error
	MarkNotificationsSeen(userID string) error
	AddToNotificationFeed(targetUserID string, activity *Activity) error
	NotifyLike(actorUserID, targetUserID, loopID string) error
	NotifyFollow(actorUserID, targetUserID string) error
	NotifyComment(actorUserID, targetUserID, loopID, commentText string) error
	NotifyMention(actorUserID, targetUserID, loopID, commentID string) error

	// MIDI Challenge notifications (R.2.2.4.4)
	NotifyChallengeCreated(targetUserID, challengeID, challengeTitle string) error
	NotifyChallengeDeadline(targetUserID, challengeID, challengeTitle string, hoursRemaining int) error
	NotifyChallengeVotingOpen(targetUserID, challengeID, challengeTitle string) error
	NotifyChallengeEnded(targetUserID, challengeID, challengeTitle string, winnerUserID, winnerUsername string, userEntryRank int) error

	// Aggregated feed management
	FollowAggregatedFeed(userID, targetUserID string) error
	UnfollowAggregatedFeed(userID, targetUserID string) error
	AddToAggregatedFeed(feedGroup, feedID string, activity *Activity) error
}

// Ensure Client implements StreamClientInterface
var _ StreamClientInterface = (*Client)(nil)
