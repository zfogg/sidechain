package main

import (
	"fmt"
	"log"

	"github.com/joho/godotenv"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/models"
)

func main() {
	// Load environment
	godotenv.Load()

	// Connect to database
	if err := database.Initialize(); err != nil {
		log.Fatalf("Failed to connect to database: %v", err)
	}
	defer database.Close()

	// Get all posts
	var posts []models.AudioPost
	if err := database.DB.Order("created_at DESC").Limit(10).Find(&posts).Error; err != nil {
		log.Fatalf("Failed to query posts: %v", err)
	}

	fmt.Printf("Recent posts (total: %d):\n\n", len(posts))

	for i, post := range posts {
		hasWaveform := "❌ NO"
		if post.WaveformURL != "" {
			hasWaveform = "✅ YES"
		}

		audioPath := "NO AUDIO"
		if post.AudioURL != "" {
			audioPath = post.AudioURL
			if len(audioPath) > 50 {
				audioPath = audioPath[:50] + "..."
			}
		}

		waveformPath := "NO WAVEFORM"
		if post.WaveformURL != "" {
			waveformPath = post.WaveformURL
			if len(waveformPath) > 50 {
				waveformPath = waveformPath[:50] + "..."
			}
		}

		fmt.Printf("[%d] Post ID: %s\n", i+1, post.ID)
		fmt.Printf("    User: %s\n", post.UserID)
		fmt.Printf("    Has Waveform: %s\n", hasWaveform)
		fmt.Printf("    Audio URL: %s\n", audioPath)
		fmt.Printf("    Waveform URL: %s\n", waveformPath)
		fmt.Println()
	}
}
