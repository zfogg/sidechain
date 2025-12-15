package main

import (
	"fmt"
	"io"
	"log"
	"net/http"
	"os"

	"github.com/joho/godotenv"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/models"
)

func debugMain() {
	// Load environment
	godotenv.Load()

	// Connect to database
	if err := database.Initialize(); err != nil {
		log.Fatalf("Failed to connect to database: %v", err)
	}
	defer database.Close()

	// Get one post without waveform
	var post models.AudioPost
	if err := database.DB.Where("waveform_url IS NULL OR waveform_url = ''").
		Where("audio_url IS NOT NULL AND audio_url != ''").
		First(&post).Error; err != nil {
		log.Fatalf("Failed to find post: %v", err)
	}

	fmt.Printf("Post ID: %s\n", post.ID)
	fmt.Printf("Audio URL: %s\n", post.AudioURL)
	fmt.Println()

	// Download the audio file
	resp, err := http.Get(post.AudioURL)
	if err != nil {
		log.Fatalf("Failed to download: %v", err)
	}
	defer resp.Body.Close()

	fmt.Printf("HTTP Status: %d\n", resp.StatusCode)
	fmt.Printf("Content-Type: %s\n", resp.Header.Get("Content-Type"))
	fmt.Printf("Content-Length: %s\n", resp.Header.Get("Content-Length"))
	fmt.Println()

	// Read first 100 bytes to see what we get
	buf := make([]byte, 100)
	n, err := resp.Body.Read(buf)
	if err != nil && err != io.EOF {
		log.Fatalf("Failed to read: %v", err)
	}

	fmt.Printf("Read %d bytes\n", n)
	fmt.Printf("First bytes (hex): % x\n", buf[:n])
	fmt.Printf("First bytes (string): %q\n", string(buf[:n]))
}
