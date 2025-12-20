package search

import (
	"context"
	"sync"
	"time"

	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/logger"
	"github.com/zfogg/sidechain/backend/internal/models"
	"go.uber.org/zap"
)

// ReconciliationService periodically verifies and resynchronizes data between
// PostgreSQL and Elasticsearch to catch any missed syncs and ensure consistency
type ReconciliationService struct {
	client    *Client
	interval  time.Duration
	stopChan  chan struct{}
	wg        sync.WaitGroup
	isRunning bool
	mu        sync.Mutex
}

// NewReconciliationService creates a new reconciliation service
func NewReconciliationService(client *Client, interval time.Duration) *ReconciliationService {
	return &ReconciliationService{
		client:   client,
		interval: interval,
		stopChan: make(chan struct{}),
	}
}

// Start begins the periodic reconciliation loop
func (rs *ReconciliationService) Start() {
	rs.mu.Lock()
	if rs.isRunning {
		rs.mu.Unlock()
		return
	}
	rs.isRunning = true
	rs.mu.Unlock()

	logger.Log.Info("Starting Elasticsearch reconciliation service",
		zap.Duration("interval", rs.interval),
	)

	rs.wg.Add(1)
	go rs.reconciliationLoop()
}

// Stop gracefully stops the reconciliation service
func (rs *ReconciliationService) Stop() {
	rs.mu.Lock()
	if !rs.isRunning {
		rs.mu.Unlock()
		return
	}
	rs.isRunning = false
	rs.mu.Unlock()

	close(rs.stopChan)
	rs.wg.Wait()
	logger.Log.Info("Elasticsearch reconciliation service stopped")
}

// reconciliationLoop runs the periodic reconciliation checks
func (rs *ReconciliationService) reconciliationLoop() {
	defer rs.wg.Done()

	// Run once immediately on startup
	rs.performReconciliation()

	ticker := time.NewTicker(rs.interval)
	defer ticker.Stop()

	for {
		select {
		case <-rs.stopChan:
			return
		case <-ticker.C:
			rs.performReconciliation()
		}
	}
}

// performReconciliation checks for data drift and resynchronizes
func (rs *ReconciliationService) performReconciliation() {
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Minute)
	defer cancel()

	startTime := time.Now()
	logger.Log.Info("Starting Elasticsearch reconciliation check")

	postsDrifted := rs.reconcilePostEngagement(ctx)
	usersDrifted := rs.reconcileUserFollowers(ctx)

	duration := time.Since(startTime)
	logger.Log.Info("Elasticsearch reconciliation completed",
		zap.Int("posts_resync", postsDrifted),
		zap.Int("users_resync", usersDrifted),
		zap.Duration("duration", duration),
	)
}

// reconcilePostEngagement reindexes a sample of posts to ensure engagement metrics are synced
func (rs *ReconciliationService) reconcilePostEngagement(ctx context.Context) int {
	if rs.client == nil {
		return 0
	}

	var posts []models.AudioPost
	// Get a sample of public posts (limit to 100 per reconciliation to avoid overload)
	if err := database.DB.
		Where("is_public = true AND is_archived = false AND deleted_at IS NULL").
		Order("RANDOM()"). // Random sample to ensure even coverage over time
		Limit(100).
		Find(&posts).Error; err != nil {
		logger.Log.Warn("Failed to query posts for reconciliation",
			zap.Error(err),
		)
		return 0
	}

	if len(posts) == 0 {
		return 0
	}

	logger.Log.Debug("Reconciling engagement metrics for posts",
		zap.Int("sample_size", len(posts)),
	)

	resyncedCount := 0
	for _, post := range posts {
		// Fetch user for username
		var user models.User
		if err := database.DB.Select("username").First(&user, "id = ?", post.UserID).Error; err != nil {
			continue
		}

		// Re-index the post with current metrics
		postDoc := AudioPostToSearchDoc(post, user.Username)
		if err := rs.client.IndexPost(ctx, post.ID, postDoc); err != nil {
			logger.Log.Warn("Failed to reconcile post",
				zap.String("post_id", post.ID),
				zap.Error(err),
			)
		} else {
			resyncedCount++
		}
	}

	logger.Log.Debug("Post reconciliation complete",
		zap.Int("resynced", resyncedCount),
	)

	return resyncedCount
}

// reconcileUserFollowers reindexes a sample of users to ensure follower counts are synced
func (rs *ReconciliationService) reconcileUserFollowers(ctx context.Context) int {
	if rs.client == nil {
		return 0
	}

	var users []models.User
	// Get a sample of users (limit to 50 per reconciliation to avoid overload)
	if err := database.DB.
		Where("is_active = true AND deleted_at IS NULL").
		Order("RANDOM()"). // Random sample to ensure even coverage over time
		Limit(50).
		Find(&users).Error; err != nil {
		logger.Log.Warn("Failed to query users for reconciliation",
			zap.Error(err),
		)
		return 0
	}

	if len(users) == 0 {
		return 0
	}

	logger.Log.Debug("Reconciling follower counts for users",
		zap.Int("sample_size", len(users)),
	)

	resyncedCount := 0
	for _, user := range users {
		// Create user document with current profile data
		userDoc := map[string]interface{}{
			"id":             user.ID,
			"username":       user.Username,
			"display_name":   user.DisplayName,
			"bio":            user.Bio,
			"genre":          user.Genre,
			"follower_count": user.FollowerCount,
			"created_at":     user.CreatedAt,
		}

		// Re-index the user with current data
		if err := rs.client.IndexUser(ctx, user.ID, userDoc); err != nil {
			logger.Log.Warn("Failed to reconcile user",
				zap.String("user_id", user.ID),
				zap.Error(err),
			)
		} else {
			resyncedCount++
		}
	}

	logger.Log.Debug("User reconciliation complete",
		zap.Int("resynced", resyncedCount),
	)

	return resyncedCount
}
