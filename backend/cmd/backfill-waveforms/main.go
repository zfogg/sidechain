package main

import (
	"bytes"
	"context"
	"fmt"
	"io"
	"log"
	"net/http"
	"os"
	"time"

	"github.com/joho/godotenv"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/models"
	"github.com/zfogg/sidechain/backend/internal/waveform"
)

func main() {
	log.Println("🔄 Waveform Backfill Tool")
	log.Println("==========================")
	log.Println()

	// Load environment variables
	if err := godotenv.Load(); err != nil {
		log.Println("Warning: .env file not found, using system environment variables")
	}

	// Check required environment variables
	region := os.Getenv("AWS_REGION")
	bucket := os.Getenv("AWS_BUCKET") // Use AWS_BUCKET to match existing audio upload config
	cdnURL := os.Getenv("CDN_URL")

	if region == "" || bucket == "" {
		log.Fatal("❌ Error: AWS_REGION and AWS_BUCKET environment variables are required")
	}

	log.Printf("📦 S3 Bucket: %s (region: %s)\n", bucket, region)
	if cdnURL != "" {
		log.Printf("🌐 CDN URL: %s\n", cdnURL)
	}
	log.Println()

	// Initialize database
	log.Println("🔄 Connecting to database...")
	if err := database.Initialize(); err != nil {
		log.Fatalf("❌ Failed to connect to database: %v", err)
	}
	defer database.Close()
	log.Println("✅ Database connected")
	log.Println()

	// Initialize waveform generator and storage
	log.Println("🔄 Initializing waveform tools...")
	generator := waveform.NewGenerator()
	storage, err := waveform.NewStorage(region, bucket, cdnURL)
	if err != nil {
		log.Fatalf("❌ Failed to initialize waveform storage: %v", err)
	}
	log.Println("✅ Waveform tools initialized")
	log.Println()

	// Parse command
	command := "all"
	if len(os.Args) > 1 {
		command = os.Args[1]
	}

	switch command {
	case "all":
		backfillPosts(generator, storage)
		backfillStories(generator, storage)
	case "posts":
		backfillPosts(generator, storage)
	case "stories":
		backfillStories(generator, storage)
	case "dry-run":
		dryRun()
	default:
		fmt.Println("Usage: backfill-waveforms [all|posts|stories|dry-run]")
		fmt.Println("  all      - Backfill both posts and stories (default)")
		fmt.Println("  posts    - Backfill audio posts only")
		fmt.Println("  stories  - Backfill stories only")
		fmt.Println("  dry-run  - Show what would be backfilled without doing it")
		os.Exit(1)
	}
}

func backfillPosts(generator *waveform.Generator, storage *waveform.Storage) {
	log.Println("🎵 Backfilling Audio Posts")
	log.Println("==========================")

	// Find all audio posts without waveform URLs
	var posts []models.AudioPost
	if err := database.DB.Where("waveform_url IS NULL OR waveform_url = ''").
		Where("audio_url IS NOT NULL AND audio_url != ''").
		Find(&posts).Error; err != nil {
		log.Printf("❌ Failed to query posts: %v\n", err)
		return
	}

	if len(posts) == 0 {
		log.Println("✅ No posts need waveform backfill")
		log.Println()
		return
	}

	log.Printf("📊 Found %d posts without waveforms\n", len(posts))
	log.Println()

	successCount := 0
	failCount := 0

	for i, post := range posts {
		log.Printf("[%d/%d] Processing post %s (user: %s)...", i+1, len(posts), post.ID, post.UserID)

		waveformURL, err := generateAndUploadWaveform(generator, storage, post.AudioURL, post.UserID, post.ID)
		if err != nil {
			log.Printf(" ❌ Failed: %v\n", err)
			failCount++
			continue
		}

		// Update post with waveform URL
		if err := database.DB.Model(&post).Update("waveform_url", waveformURL).Error; err != nil {
			log.Printf(" ❌ Failed to update database: %v\n", err)
			failCount++
			continue
		}

		log.Printf(" ✅ Success\n")
		successCount++

		// Small delay to avoid rate limiting
		time.Sleep(100 * time.Millisecond)
	}

	log.Println()
	log.Printf("📊 Posts Backfill Complete: %d succeeded, %d failed\n", successCount, failCount)
	log.Println()
}

func backfillStories(generator *waveform.Generator, storage *waveform.Storage) {
	log.Println("📖 Backfilling Stories")
	log.Println("==========================")

	// Find all stories without waveform URLs (including expired ones)
	var stories []models.Story
	if err := database.DB.Where("waveform_url IS NULL OR waveform_url = ''").
		Where("audio_url IS NOT NULL AND audio_url != ''").
		Find(&stories).Error; err != nil {
		log.Printf("❌ Failed to query stories: %v\n", err)
		return
	}

	if len(stories) == 0 {
		log.Println("✅ No stories need waveform backfill")
		log.Println()
		return
	}

	log.Printf("📊 Found %d stories without waveforms\n", len(stories))
	log.Println()

	successCount := 0
	failCount := 0

	for i, story := range stories {
		log.Printf("[%d/%d] Processing story %s (user: %s)...", i+1, len(stories), story.ID, story.UserID)

		waveformURL, err := generateAndUploadWaveform(generator, storage, story.AudioURL, story.UserID, story.ID)
		if err != nil {
			log.Printf(" ❌ Failed: %v\n", err)
			failCount++
			continue
		}

		// Update story with waveform URL
		if err := database.DB.Model(&story).Update("waveform_url", waveformURL).Error; err != nil {
			log.Printf(" ❌ Failed to update database: %v\n", err)
			failCount++
			continue
		}

		log.Printf(" ✅ Success\n")
		successCount++

		// Small delay to avoid rate limiting
		time.Sleep(100 * time.Millisecond)
	}

	log.Println()
	log.Printf("📊 Stories Backfill Complete: %d succeeded, %d failed\n", successCount, failCount)
	log.Println()
}

func generateAndUploadWaveform(generator *waveform.Generator, storage *waveform.Storage, audioURL, userID, postID string) (string, error) {
	// Download audio file from CDN
	ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
	defer cancel()

	req, err := http.NewRequestWithContext(ctx, "GET", audioURL, nil)
	if err != nil {
		return "", fmt.Errorf("failed to create request: %w", err)
	}

	resp, err := http.DefaultClient.Do(req)
	if err != nil {
		return "", fmt.Errorf("failed to download audio: %w", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		return "", fmt.Errorf("audio download returned status %d", resp.StatusCode)
	}

	// Read audio data
	audioData, err := io.ReadAll(resp.Body)
	if err != nil {
		return "", fmt.Errorf("failed to read audio data: %w", err)
	}

	// Generate waveform PNG
	waveformPNG, err := generator.GenerateFromWAV(bytes.NewReader(audioData))
	if err != nil {
		return "", fmt.Errorf("failed to generate waveform: %w", err)
	}

	// Upload to S3
	waveformURL, err := storage.UploadWaveform(waveformPNG, userID, postID)
	if err != nil {
		return "", fmt.Errorf("failed to upload waveform: %w", err)
	}

	return waveformURL, nil
}

func dryRun() {
	log.Println("🔍 Dry Run - Checking what needs backfilling")
	log.Println("============================================")
	log.Println()

	// Count posts without waveforms
	var postCount int64
	if err := database.DB.Model(&models.AudioPost{}).
		Where("waveform_url IS NULL OR waveform_url = ''").
		Where("audio_url IS NOT NULL AND audio_url != ''").
		Count(&postCount).Error; err != nil {
		log.Printf("❌ Failed to count posts: %v\n", err)
	} else {
		log.Printf("📊 Audio Posts without waveforms: %d\n", postCount)
	}

	// Count stories without waveforms
	var storyCount int64
	if err := database.DB.Model(&models.Story{}).
		Where("waveform_url IS NULL OR waveform_url = ''").
		Where("audio_url IS NOT NULL AND audio_url != ''").
		Count(&storyCount).Error; err != nil {
		log.Printf("❌ Failed to count stories: %v\n", err)
	} else {
		log.Printf("📖 Stories without waveforms: %d\n", storyCount)
	}

	log.Println()
	log.Printf("📊 Total items to backfill: %d\n", postCount+storyCount)
	log.Println()
	log.Println("💡 Run without 'dry-run' to perform backfill")
}
