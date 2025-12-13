package stream

import (
	"fmt"
	"sync"
	"time"
)

// MockCall records a method call for assertion
type MockCall struct {
	Method string
	Args   []interface{}
}

// MockStreamClient is a mock implementation of StreamClientInterface for testing.
// It allows configuring responses per method and tracks all calls for assertions.
type MockStreamClient struct {
	mu sync.Mutex

	// Call tracking
	Calls []MockCall

	// Configurable function overrides - set these to customize behavior
	CreateUserFunc                      func(userID, username string) error
	CreateTokenFunc                     func(userID string, expiration time.Time) (string, error)
	CreateLoopActivityFunc              func(userID string, activity *Activity) error
	DeleteLoopActivityFunc              func(userID, activityID string) error
	GetUserTimelineFunc                 func(userID string, limit, offset int) ([]*Activity, error)
	GetGlobalFeedFunc                   func(limit, offset int) ([]*Activity, error)
	GetEnrichedTimelineFunc             func(userID string, limit, offset int) ([]*EnrichedActivity, error)
	GetEnrichedGlobalFeedFunc           func(limit, offset int) ([]*EnrichedActivity, error)
	GetEnrichedUserFeedFunc             func(userID string, limit, offset int) ([]*EnrichedActivity, error)
	GetAggregatedTimelineFunc           func(userID string, limit, offset int) (*AggregatedFeedResponse, error)
	GetTrendingFeedFunc                 func(limit, offset int) (*AggregatedFeedResponse, error)
	GetUserActivitySummaryFunc          func(userID string, limit int) (*AggregatedFeedResponse, error)
	FollowUserFunc                      func(userID, targetUserID string) error
	UnfollowUserFunc                    func(userID, targetUserID string) error
	GetFollowStatsFunc                  func(userID string) (*FollowStats, error)
	GetFollowersFunc                    func(userID string, limit, offset int) ([]*FollowRelation, error)
	GetFollowingFunc                    func(userID string, limit, offset int) ([]*FollowRelation, error)
	CheckIsFollowingFunc                func(userID, targetUserID string) (bool, error)
	AddReactionFunc                     func(kind, userID, activityID string) error
	AddReactionWithEmojiFunc            func(kind, userID, activityID, emoji string) error
	AddReactionWithNotificationFunc     func(kind, actorUserID, activityID, targetUserID, loopID, emoji string) error
	RemoveReactionFunc                  func(reactionID string) error
	RemoveReactionByActivityAndUserFunc func(activityID, userID, kind string) error
	GetUserReactionsFunc                func(userID, kind string, limit int) ([]*Reaction, error)
	RepostActivityFunc                  func(userID, activityID string) (*RepostResponse, error)
	UnrepostActivityFunc                func(userID, activityID string) error
	CheckUserRepostedFunc               func(userID, activityID string) (bool, string, error)
	NotifyRepostFunc                    func(actorUserID, targetUserID, loopID string) error
	GetNotificationsFunc                func(userID string, limit, offset int) (*NotificationResponse, error)
	GetNotificationCountsFunc           func(userID string) (unseen, unread int, err error)
	MarkNotificationsReadFunc           func(userID string) error
	MarkNotificationsSeenFunc           func(userID string) error
	AddToNotificationFeedFunc           func(targetUserID string, activity *Activity) error
	NotifyLikeFunc                      func(actorUserID, targetUserID, loopID string) error
	NotifyFollowFunc                    func(actorUserID, targetUserID string) error
	NotifyCommentFunc                   func(actorUserID, targetUserID, loopID, commentText string) error
	NotifyMentionFunc                   func(actorUserID, targetUserID, loopID, commentID string) error
	NotifyChallengeCreatedFunc          func(targetUserID, challengeID, challengeTitle string) error
	NotifyChallengeDeadlineFunc         func(targetUserID, challengeID, challengeTitle string, hoursRemaining int) error
	NotifyChallengeVotingOpenFunc       func(targetUserID, challengeID, challengeTitle string) error
	NotifyChallengeEndedFunc            func(targetUserID, challengeID, challengeTitle string, winnerUserID, winnerUsername string, userEntryRank int) error
	FollowAggregatedFeedFunc            func(userID, targetUserID string) error
	UnfollowAggregatedFeedFunc          func(userID, targetUserID string) error
	AddToAggregatedFeedFunc             func(feedGroup, feedID string, activity *Activity) error

	// Default responses for simple cases
	DefaultError error
}

// NewMockStreamClient creates a new mock client with sensible defaults
func NewMockStreamClient() *MockStreamClient {
	return &MockStreamClient{
		Calls: make([]MockCall, 0),
	}
}

// recordCall records a method call for later assertion
func (m *MockStreamClient) recordCall(method string, args ...interface{}) {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.Calls = append(m.Calls, MockCall{Method: method, Args: args})
}

// GetCalls returns all recorded calls (thread-safe)
func (m *MockStreamClient) GetCalls() []MockCall {
	m.mu.Lock()
	defer m.mu.Unlock()
	result := make([]MockCall, len(m.Calls))
	copy(result, m.Calls)
	return result
}

// GetCallsForMethod returns calls for a specific method
func (m *MockStreamClient) GetCallsForMethod(method string) []MockCall {
	m.mu.Lock()
	defer m.mu.Unlock()
	var result []MockCall
	for _, call := range m.Calls {
		if call.Method == method {
			result = append(result, call)
		}
	}
	return result
}

// Reset clears all recorded calls
func (m *MockStreamClient) Reset() {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.Calls = make([]MockCall, 0)
}

// AssertCalled checks if a method was called at least once
func (m *MockStreamClient) AssertCalled(method string) bool {
	return len(m.GetCallsForMethod(method)) > 0
}

// AssertNotCalled checks if a method was never called
func (m *MockStreamClient) AssertNotCalled(method string) bool {
	return len(m.GetCallsForMethod(method)) == 0
}

// AssertCallCount checks if a method was called exactly n times
func (m *MockStreamClient) AssertCallCount(method string, count int) bool {
	return len(m.GetCallsForMethod(method)) == count
}

// ============================================================================
// User operations
// ============================================================================

func (m *MockStreamClient) CreateUser(userID, username string) error {
	m.recordCall("CreateUser", userID, username)
	if m.CreateUserFunc != nil {
		return m.CreateUserFunc(userID, username)
	}
	return m.DefaultError
}

func (m *MockStreamClient) CreateToken(userID string, expiration time.Time) (string, error) {
	m.recordCall("CreateToken", userID, expiration)
	if m.CreateTokenFunc != nil {
		return m.CreateTokenFunc(userID, expiration)
	}
	if m.DefaultError != nil {
		return "", m.DefaultError
	}
	return fmt.Sprintf("mock_token_%s_%d", userID, expiration.Unix()), nil
}

// ============================================================================
// Feed operations
// ============================================================================

func (m *MockStreamClient) CreateLoopActivity(userID string, activity *Activity) error {
	m.recordCall("CreateLoopActivity", userID, activity)
	if m.CreateLoopActivityFunc != nil {
		return m.CreateLoopActivityFunc(userID, activity)
	}
	// Set a mock ID if activity is non-nil
	if activity != nil && activity.ID == "" {
		activity.ID = fmt.Sprintf("mock_activity_%s_%d", userID, time.Now().UnixNano())
	}
	return m.DefaultError
}

func (m *MockStreamClient) DeleteLoopActivity(userID, activityID string) error {
	m.recordCall("DeleteLoopActivity", userID, activityID)
	if m.DeleteLoopActivityFunc != nil {
		return m.DeleteLoopActivityFunc(userID, activityID)
	}
	return m.DefaultError
}

func (m *MockStreamClient) GetUserTimeline(userID string, limit, offset int) ([]*Activity, error) {
	m.recordCall("GetUserTimeline", userID, limit, offset)
	if m.GetUserTimelineFunc != nil {
		return m.GetUserTimelineFunc(userID, limit, offset)
	}
	if m.DefaultError != nil {
		return nil, m.DefaultError
	}
	return []*Activity{}, nil
}

func (m *MockStreamClient) GetGlobalFeed(limit, offset int) ([]*Activity, error) {
	m.recordCall("GetGlobalFeed", limit, offset)
	if m.GetGlobalFeedFunc != nil {
		return m.GetGlobalFeedFunc(limit, offset)
	}
	if m.DefaultError != nil {
		return nil, m.DefaultError
	}
	return []*Activity{}, nil
}

// ============================================================================
// Enriched feed operations
// ============================================================================

func (m *MockStreamClient) GetEnrichedTimeline(userID string, limit, offset int) ([]*EnrichedActivity, error) {
	m.recordCall("GetEnrichedTimeline", userID, limit, offset)
	if m.GetEnrichedTimelineFunc != nil {
		return m.GetEnrichedTimelineFunc(userID, limit, offset)
	}
	if m.DefaultError != nil {
		return nil, m.DefaultError
	}
	return []*EnrichedActivity{}, nil
}

func (m *MockStreamClient) GetEnrichedGlobalFeed(limit, offset int) ([]*EnrichedActivity, error) {
	m.recordCall("GetEnrichedGlobalFeed", limit, offset)
	if m.GetEnrichedGlobalFeedFunc != nil {
		return m.GetEnrichedGlobalFeedFunc(limit, offset)
	}
	if m.DefaultError != nil {
		return nil, m.DefaultError
	}
	return []*EnrichedActivity{}, nil
}

func (m *MockStreamClient) GetEnrichedUserFeed(userID string, limit, offset int) ([]*EnrichedActivity, error) {
	m.recordCall("GetEnrichedUserFeed", userID, limit, offset)
	if m.GetEnrichedUserFeedFunc != nil {
		return m.GetEnrichedUserFeedFunc(userID, limit, offset)
	}
	if m.DefaultError != nil {
		return nil, m.DefaultError
	}
	return []*EnrichedActivity{}, nil
}

// ============================================================================
// Aggregated feed operations
// ============================================================================

func (m *MockStreamClient) GetAggregatedTimeline(userID string, limit, offset int) (*AggregatedFeedResponse, error) {
	m.recordCall("GetAggregatedTimeline", userID, limit, offset)
	if m.GetAggregatedTimelineFunc != nil {
		return m.GetAggregatedTimelineFunc(userID, limit, offset)
	}
	if m.DefaultError != nil {
		return nil, m.DefaultError
	}
	return &AggregatedFeedResponse{Groups: []*AggregatedGroup{}}, nil
}

func (m *MockStreamClient) GetTrendingFeed(limit, offset int) (*AggregatedFeedResponse, error) {
	m.recordCall("GetTrendingFeed", limit, offset)
	if m.GetTrendingFeedFunc != nil {
		return m.GetTrendingFeedFunc(limit, offset)
	}
	if m.DefaultError != nil {
		return nil, m.DefaultError
	}
	return &AggregatedFeedResponse{Groups: []*AggregatedGroup{}}, nil
}

func (m *MockStreamClient) GetUserActivitySummary(userID string, limit int) (*AggregatedFeedResponse, error) {
	m.recordCall("GetUserActivitySummary", userID, limit)
	if m.GetUserActivitySummaryFunc != nil {
		return m.GetUserActivitySummaryFunc(userID, limit)
	}
	if m.DefaultError != nil {
		return nil, m.DefaultError
	}
	return &AggregatedFeedResponse{Groups: []*AggregatedGroup{}}, nil
}

// ============================================================================
// Follow operations
// ============================================================================

func (m *MockStreamClient) FollowUser(userID, targetUserID string) error {
	m.recordCall("FollowUser", userID, targetUserID)
	if m.FollowUserFunc != nil {
		return m.FollowUserFunc(userID, targetUserID)
	}
	return m.DefaultError
}

func (m *MockStreamClient) UnfollowUser(userID, targetUserID string) error {
	m.recordCall("UnfollowUser", userID, targetUserID)
	if m.UnfollowUserFunc != nil {
		return m.UnfollowUserFunc(userID, targetUserID)
	}
	return m.DefaultError
}

func (m *MockStreamClient) GetFollowStats(userID string) (*FollowStats, error) {
	m.recordCall("GetFollowStats", userID)
	if m.GetFollowStatsFunc != nil {
		return m.GetFollowStatsFunc(userID)
	}
	if m.DefaultError != nil {
		return nil, m.DefaultError
	}
	return &FollowStats{FollowerCount: 0, FollowingCount: 0}, nil
}

func (m *MockStreamClient) GetFollowers(userID string, limit, offset int) ([]*FollowRelation, error) {
	m.recordCall("GetFollowers", userID, limit, offset)
	if m.GetFollowersFunc != nil {
		return m.GetFollowersFunc(userID, limit, offset)
	}
	if m.DefaultError != nil {
		return nil, m.DefaultError
	}
	return []*FollowRelation{}, nil
}

func (m *MockStreamClient) GetFollowing(userID string, limit, offset int) ([]*FollowRelation, error) {
	m.recordCall("GetFollowing", userID, limit, offset)
	if m.GetFollowingFunc != nil {
		return m.GetFollowingFunc(userID, limit, offset)
	}
	if m.DefaultError != nil {
		return nil, m.DefaultError
	}
	return []*FollowRelation{}, nil
}

func (m *MockStreamClient) CheckIsFollowing(userID, targetUserID string) (bool, error) {
	m.recordCall("CheckIsFollowing", userID, targetUserID)
	if m.CheckIsFollowingFunc != nil {
		return m.CheckIsFollowingFunc(userID, targetUserID)
	}
	if m.DefaultError != nil {
		return false, m.DefaultError
	}
	return false, nil
}

// ============================================================================
// Reaction operations
// ============================================================================

func (m *MockStreamClient) AddReaction(kind, userID, activityID string) error {
	m.recordCall("AddReaction", kind, userID, activityID)
	if m.AddReactionFunc != nil {
		return m.AddReactionFunc(kind, userID, activityID)
	}
	return m.DefaultError
}

func (m *MockStreamClient) AddReactionWithEmoji(kind, userID, activityID, emoji string) error {
	m.recordCall("AddReactionWithEmoji", kind, userID, activityID, emoji)
	if m.AddReactionWithEmojiFunc != nil {
		return m.AddReactionWithEmojiFunc(kind, userID, activityID, emoji)
	}
	return m.DefaultError
}

func (m *MockStreamClient) AddReactionWithNotification(kind, actorUserID, activityID, targetUserID, loopID, emoji string) error {
	m.recordCall("AddReactionWithNotification", kind, actorUserID, activityID, targetUserID, loopID, emoji)
	if m.AddReactionWithNotificationFunc != nil {
		return m.AddReactionWithNotificationFunc(kind, actorUserID, activityID, targetUserID, loopID, emoji)
	}
	return m.DefaultError
}

func (m *MockStreamClient) RemoveReaction(reactionID string) error {
	m.recordCall("RemoveReaction", reactionID)
	if m.RemoveReactionFunc != nil {
		return m.RemoveReactionFunc(reactionID)
	}
	return m.DefaultError
}

func (m *MockStreamClient) RemoveReactionByActivityAndUser(activityID, userID, kind string) error {
	m.recordCall("RemoveReactionByActivityAndUser", activityID, userID, kind)
	if m.RemoveReactionByActivityAndUserFunc != nil {
		return m.RemoveReactionByActivityAndUserFunc(activityID, userID, kind)
	}
	return m.DefaultError
}

func (m *MockStreamClient) GetUserReactions(userID, kind string, limit int) ([]*Reaction, error) {
	m.recordCall("GetUserReactions", userID, kind, limit)
	if m.GetUserReactionsFunc != nil {
		return m.GetUserReactionsFunc(userID, kind, limit)
	}
	if m.DefaultError != nil {
		return nil, m.DefaultError
	}
	return []*Reaction{}, nil
}

// ============================================================================
// Repost operations
// ============================================================================

func (m *MockStreamClient) RepostActivity(userID, activityID string) (*RepostResponse, error) {
	m.recordCall("RepostActivity", userID, activityID)
	if m.RepostActivityFunc != nil {
		return m.RepostActivityFunc(userID, activityID)
	}
	if m.DefaultError != nil {
		return nil, m.DefaultError
	}
	return &RepostResponse{
		ReactionID: "mock-repost-reaction-id",
		ActivityID: activityID,
		UserID:     userID,
		RepostedAt: time.Now().UTC(),
	}, nil
}

func (m *MockStreamClient) UnrepostActivity(userID, activityID string) error {
	m.recordCall("UnrepostActivity", userID, activityID)
	if m.UnrepostActivityFunc != nil {
		return m.UnrepostActivityFunc(userID, activityID)
	}
	return m.DefaultError
}

func (m *MockStreamClient) CheckUserReposted(userID, activityID string) (bool, string, error) {
	m.recordCall("CheckUserReposted", userID, activityID)
	if m.CheckUserRepostedFunc != nil {
		return m.CheckUserRepostedFunc(userID, activityID)
	}
	if m.DefaultError != nil {
		return false, "", m.DefaultError
	}
	return false, "", nil
}

func (m *MockStreamClient) NotifyRepost(actorUserID, targetUserID, loopID string) error {
	m.recordCall("NotifyRepost", actorUserID, targetUserID, loopID)
	if m.NotifyRepostFunc != nil {
		return m.NotifyRepostFunc(actorUserID, targetUserID, loopID)
	}
	return m.DefaultError
}

// ============================================================================
// Notification operations
// ============================================================================

func (m *MockStreamClient) GetNotifications(userID string, limit, offset int) (*NotificationResponse, error) {
	m.recordCall("GetNotifications", userID, limit, offset)
	if m.GetNotificationsFunc != nil {
		return m.GetNotificationsFunc(userID, limit, offset)
	}
	if m.DefaultError != nil {
		return nil, m.DefaultError
	}
	return &NotificationResponse{Groups: []*NotificationGroup{}, Unseen: 0, Unread: 0}, nil
}

func (m *MockStreamClient) GetNotificationCounts(userID string) (unseen, unread int, err error) {
	m.recordCall("GetNotificationCounts", userID)
	if m.GetNotificationCountsFunc != nil {
		return m.GetNotificationCountsFunc(userID)
	}
	return 0, 0, m.DefaultError
}

func (m *MockStreamClient) MarkNotificationsRead(userID string) error {
	m.recordCall("MarkNotificationsRead", userID)
	if m.MarkNotificationsReadFunc != nil {
		return m.MarkNotificationsReadFunc(userID)
	}
	return m.DefaultError
}

func (m *MockStreamClient) MarkNotificationsSeen(userID string) error {
	m.recordCall("MarkNotificationsSeen", userID)
	if m.MarkNotificationsSeenFunc != nil {
		return m.MarkNotificationsSeenFunc(userID)
	}
	return m.DefaultError
}

func (m *MockStreamClient) AddToNotificationFeed(targetUserID string, activity *Activity) error {
	m.recordCall("AddToNotificationFeed", targetUserID, activity)
	if m.AddToNotificationFeedFunc != nil {
		return m.AddToNotificationFeedFunc(targetUserID, activity)
	}
	return m.DefaultError
}

func (m *MockStreamClient) NotifyLike(actorUserID, targetUserID, loopID string) error {
	m.recordCall("NotifyLike", actorUserID, targetUserID, loopID)
	if m.NotifyLikeFunc != nil {
		return m.NotifyLikeFunc(actorUserID, targetUserID, loopID)
	}
	return m.DefaultError
}

func (m *MockStreamClient) NotifyFollow(actorUserID, targetUserID string) error {
	m.recordCall("NotifyFollow", actorUserID, targetUserID)
	if m.NotifyFollowFunc != nil {
		return m.NotifyFollowFunc(actorUserID, targetUserID)
	}
	return m.DefaultError
}

func (m *MockStreamClient) NotifyComment(actorUserID, targetUserID, loopID, commentText string) error {
	m.recordCall("NotifyComment", actorUserID, targetUserID, loopID, commentText)
	if m.NotifyCommentFunc != nil {
		return m.NotifyCommentFunc(actorUserID, targetUserID, loopID, commentText)
	}
	return m.DefaultError
}

func (m *MockStreamClient) NotifyMention(actorUserID, targetUserID, loopID, commentID string) error {
	m.recordCall("NotifyMention", actorUserID, targetUserID, loopID, commentID)
	if m.NotifyMentionFunc != nil {
		return m.NotifyMentionFunc(actorUserID, targetUserID, loopID, commentID)
	}
	return m.DefaultError
}

func (m *MockStreamClient) NotifyChallengeCreated(targetUserID, challengeID, challengeTitle string) error {
	m.recordCall("NotifyChallengeCreated", targetUserID, challengeID, challengeTitle)
	if m.NotifyChallengeCreatedFunc != nil {
		return m.NotifyChallengeCreatedFunc(targetUserID, challengeID, challengeTitle)
	}
	return m.DefaultError
}

func (m *MockStreamClient) NotifyChallengeDeadline(targetUserID, challengeID, challengeTitle string, hoursRemaining int) error {
	m.recordCall("NotifyChallengeDeadline", targetUserID, challengeID, challengeTitle, hoursRemaining)
	if m.NotifyChallengeDeadlineFunc != nil {
		return m.NotifyChallengeDeadlineFunc(targetUserID, challengeID, challengeTitle, hoursRemaining)
	}
	return m.DefaultError
}

func (m *MockStreamClient) NotifyChallengeVotingOpen(targetUserID, challengeID, challengeTitle string) error {
	m.recordCall("NotifyChallengeVotingOpen", targetUserID, challengeID, challengeTitle)
	if m.NotifyChallengeVotingOpenFunc != nil {
		return m.NotifyChallengeVotingOpenFunc(targetUserID, challengeID, challengeTitle)
	}
	return m.DefaultError
}

func (m *MockStreamClient) NotifyChallengeEnded(targetUserID, challengeID, challengeTitle string, winnerUserID, winnerUsername string, userEntryRank int) error {
	m.recordCall("NotifyChallengeEnded", targetUserID, challengeID, challengeTitle, winnerUserID, winnerUsername, userEntryRank)
	if m.NotifyChallengeEndedFunc != nil {
		return m.NotifyChallengeEndedFunc(targetUserID, challengeID, challengeTitle, winnerUserID, winnerUsername, userEntryRank)
	}
	return m.DefaultError
}

// ============================================================================
// Aggregated feed management
// ============================================================================

func (m *MockStreamClient) FollowAggregatedFeed(userID, targetUserID string) error {
	m.recordCall("FollowAggregatedFeed", userID, targetUserID)
	if m.FollowAggregatedFeedFunc != nil {
		return m.FollowAggregatedFeedFunc(userID, targetUserID)
	}
	return m.DefaultError
}

func (m *MockStreamClient) UnfollowAggregatedFeed(userID, targetUserID string) error {
	m.recordCall("UnfollowAggregatedFeed", userID, targetUserID)
	if m.UnfollowAggregatedFeedFunc != nil {
		return m.UnfollowAggregatedFeedFunc(userID, targetUserID)
	}
	return m.DefaultError
}

func (m *MockStreamClient) AddToAggregatedFeed(feedGroup, feedID string, activity *Activity) error {
	m.recordCall("AddToAggregatedFeed", feedGroup, feedID, activity)
	if m.AddToAggregatedFeedFunc != nil {
		return m.AddToAggregatedFeedFunc(feedGroup, feedID, activity)
	}
	return m.DefaultError
}

// Ensure MockStreamClient implements StreamClientInterface
var _ StreamClientInterface = (*MockStreamClient)(nil)
