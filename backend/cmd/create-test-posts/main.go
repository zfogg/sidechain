package main

import (
	"fmt"
	"log"

	"github.com/joho/godotenv"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/models"
	"github.com/zfogg/sidechain/backend/internal/stream"
)

func main() {
	// Load environment variables
	if err := godotenv.Load(); err != nil {
		log.Println("Warning: .env file not found, using system environment variables")
	}

	// Initialize database
	if err := database.Initialize(); err != nil {
		log.Fatalf("Failed to initialize database: %v", err)
	}
	defer database.Close()

	// Initialize Stream.io client
	streamClient, err := stream.NewClient()
	if err != nil {
		log.Fatalf("Failed to initialize Stream.io client: %v", err)
	}

	// Get test users
	var users []models.User
	if err := database.DB.Where("username IN (?)", []string{"alice", "bob", "charlie", "diana", "eve"}).Find(&users).Error; err != nil {
		log.Fatalf("Failed to fetch test users: %v", err)
	}

	userMap := make(map[string]*models.User)
	for i := range users {
		userMap[users[i].Username] = &users[i]
	}

	// Create a test post for bob
	bob, ok := userMap["bob"]
	if !ok {
		log.Fatalf("Bob user not found")
	}

	// Check if bob already has posts
	var existingPosts int64
	database.DB.Model(&models.AudioPost{}).Where("user_id = ? AND deleted_at IS NULL", bob.ID).Count(&existingPosts)
	if existingPosts > 0 {
		fmt.Printf("✓ Bob already has %d posts\n", existingPosts)
		return
	}

	// Create a test audio post for bob
	post := models.AudioPost{
		UserID:           bob.ID,
		Filename:         "test_loop.wav",
		AudioURL:         "https://example.com/test_loop.wav",
		WaveformURL:      "https://example.com/waveform.svg",
		Duration:         8.0,
		DurationBars:     2,
		BPM:              128,
		Key:              "C",
		DAW:              "Ableton",
		Genre:            models.StringArray{"House", "Electronic"},
		IsPublic:         true,
		IsArchived:       false,
		ProcessingStatus: "complete",
		LikeCount:        0,
		PlayCount:        0,
		CommentCount:     0,
	}

	if err := database.DB.Create(&post).Error; err != nil {
		log.Fatalf("Failed to create post: %v", err)
	}

	// Create Stream.io activity for the post
	if err := streamClient.CreatePost(bob.ID, post.ID, post.Filename, post.AudioURL, post.WaveformURL, post.Duration, post.BPM, post.Key, post.DAW, post.Genre); err != nil {
		log.Printf("⚠️  Failed to create Stream.io activity: %v\n", err)
	} else {
		fmt.Printf("✓ Created Stream.io activity for post\n")
	}

	fmt.Printf("✓ Created test post for bob: %s\n", post.ID)
}
