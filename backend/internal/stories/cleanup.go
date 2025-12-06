package stories

import (
	"context"
	"fmt"
	"log"
	"strings"
	"time"

	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/models"
	"github.com/zfogg/sidechain/backend/internal/storage"
)

// FileDeleter interface for deleting files from storage
type FileDeleter interface {
	DeleteFile(ctx context.Context, key string) error
}

// CleanupService handles periodic cleanup of expired stories
type CleanupService struct {
	fileDeleter FileDeleter // For deleting audio files from S3
	ctx         context.Context
	cancel      context.CancelFunc
	interval    time.Duration
}

// NewCleanupService creates a new story cleanup service
func NewCleanupService(fileDeleter FileDeleter, interval time.Duration) *CleanupService {
	ctx, cancel := context.WithCancel(context.Background())
	return &CleanupService{
		fileDeleter: fileDeleter,
		ctx:         ctx,
		cancel:      cancel,
		interval:    interval,
	}
}

// Start begins the periodic cleanup process
func (s *CleanupService) Start() {
	log.Println("🧹 Starting story cleanup service")
	go s.run()
}

// Stop stops the cleanup service
func (s *CleanupService) Stop() {
	log.Println("🧹 Stopping story cleanup service")
	s.cancel()
}

// run executes cleanup on the configured interval
func (s *CleanupService) run() {
	// Run immediately on startup
	s.cleanupExpiredStories()

	// Then run on interval
	ticker := time.NewTicker(s.interval)
	defer ticker.Stop()

	for {
		select {
		case <-ticker.C:
			s.cleanupExpiredStories()
		case <-s.ctx.Done():
			return
		}
	}
}

// cleanupExpiredStories deletes expired stories and their associated data (7.5.1.5.1)
func (s *CleanupService) cleanupExpiredStories() {
	startTime := time.Now()
	log.Println("🧹 Starting story cleanup...")

	// Find all expired stories
	var expiredStories []models.Story
	if err := database.DB.Where("expires_at < ?", time.Now().UTC()).Find(&expiredStories).Error; err != nil {
		log.Printf("❌ Failed to query expired stories: %v", err)
		return
	}

	if len(expiredStories) == 0 {
		log.Println("✅ No expired stories to clean up")
		return
	}

	log.Printf("🗑️ Found %d expired stories to delete", len(expiredStories))

	deletedCount := 0
	audioDeletedCount := 0
	viewDeletedCount := 0
	errors := 0

	for _, story := range expiredStories {
		// Delete associated view records first (foreign key constraint)
		viewsDeleted := database.DB.Where("story_id = ?", story.ID).Delete(&models.StoryView{}).RowsAffected
		viewDeletedCount += int(viewsDeleted)

		// Delete audio file from S3 if URL is present
		if story.AudioURL != "" && s.fileDeleter != nil {
			// Extract S3 key from URL (assuming CDN base URL format)
			// URL format: https://cdn.example.com/audio/2024/12/user123/file.mp3
			// We need to extract the path after the base URL
			if err := s.deleteAudioFromS3(story.AudioURL); err != nil {
				log.Printf("⚠️ Warning: Failed to delete audio from S3 for story %s: %v", story.ID, err)
				// Continue anyway - database cleanup is more important
			} else {
				audioDeletedCount++
			}
		}

		// Delete the story record
		if err := database.DB.Delete(&story).Error; err != nil {
			log.Printf("❌ Failed to delete story %s: %v", story.ID, err)
			errors++
			continue
		}

		deletedCount++
	}

	duration := time.Since(startTime)
	log.Printf("✅ Story cleanup completed: %d stories deleted, %d audio files deleted, %d views deleted, %d errors (took %v)",
		deletedCount, audioDeletedCount, viewDeletedCount, errors, duration)
}

// deleteAudioFromS3 deletes an audio file from S3 storage
func (s *CleanupService) deleteAudioFromS3(audioURL string) error {
	if s.fileDeleter == nil {
		return fmt.Errorf("file deleter not configured")
	}

	// Extract key from URL
	// Assuming URL format: https://cdn.example.com/audio/2024/12/user123/file.mp3
	// Key would be: audio/2024/12/user123/file.mp3
	key := extractS3KeyFromURL(audioURL)
	if key == "" {
		return fmt.Errorf("could not extract S3 key from URL: %s", audioURL)
	}

	ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
	defer cancel()

	return s.fileDeleter.DeleteFile(ctx, key)
}

// extractS3KeyFromURL extracts the S3 key from a CDN URL
// Example: https://cdn.example.com/audio/2024/12/user123/file.mp3 -> audio/2024/12/user123/file.mp3
func extractS3KeyFromURL(url string) string {
	// Find the path after the domain
	// This is a simple implementation - may need adjustment based on actual CDN URL format
	parts := strings.Split(url, "/")
	if len(parts) < 4 {
		return ""
	}

	// Find where the path starts (after https://cdn.example.com)
	// Look for common patterns like "audio/" or "stories/"
	for i, part := range parts {
		if part == "audio" || part == "stories" {
			// Reconstruct the key from this point
			return strings.Join(parts[i:], "/")
		}
	}

	return ""
}

