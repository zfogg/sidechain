package main

import (
	"bytes"
	"context"
	"fmt"
	"io"
	"log"
	"net/http"
	"net/url"
	"os"
	"os/exec"
	"strings"
	"time"

	"github.com/aws/aws-sdk-go-v2/aws"
	"github.com/aws/aws-sdk-go-v2/config"
	"github.com/aws/aws-sdk-go-v2/service/s3"
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

	// Initialize AWS S3 client for downloading audio files
	cfg, err := config.LoadDefaultConfig(context.Background(), config.WithRegion(region))
	if err != nil {
		log.Fatalf("❌ Failed to load AWS config: %v", err)
	}
	s3Client := s3.NewFromConfig(cfg)

	log.Println("✅ Waveform tools initialized")
	log.Println()

	// Parse command
	command := "all"
	if len(os.Args) > 1 {
		command = os.Args[1]
	}

	switch command {
	case "all":
		backfillPosts(generator, storage, s3Client, bucket)
		backfillStories(generator, storage, s3Client, bucket)
	case "posts":
		backfillPosts(generator, storage, s3Client, bucket)
	case "stories":
		backfillStories(generator, storage, s3Client, bucket)
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

func backfillPosts(generator *waveform.Generator, storage *waveform.Storage, s3Client *s3.Client, bucket string) {
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

		waveformURL, err := generateAndUploadWaveform(generator, storage, s3Client, bucket, post.AudioURL, post.UserID, post.ID)
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

func backfillStories(generator *waveform.Generator, storage *waveform.Storage, s3Client *s3.Client, bucket string) {
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

		waveformURL, err := generateAndUploadWaveform(generator, storage, s3Client, bucket, story.AudioURL, story.UserID, story.ID)
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

func generateAndUploadWaveform(generator *waveform.Generator, storage *waveform.Storage, s3Client *s3.Client, bucket, audioURL, userID, postID string) (string, error) {
	// Parse URL to determine source
	parsedURL, err := url.Parse(audioURL)
	if err != nil {
		return "", fmt.Errorf("failed to parse audio URL: %w", err)
	}

	var audioData []byte

	// Check if this is an S3 URL or external HTTP URL
	isS3URL := strings.Contains(parsedURL.Host, "amazonaws.com") || strings.Contains(parsedURL.Host, bucket)

	if isS3URL {
		// Extract S3 key from URL path (remove leading slash)
		s3Key := parsedURL.Path
		if len(s3Key) > 0 && s3Key[0] == '/' {
			s3Key = s3Key[1:]
		}

		// Download audio file from S3
		ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
		defer cancel()

		result, err := s3Client.GetObject(ctx, &s3.GetObjectInput{
			Bucket: aws.String(bucket),
			Key:    aws.String(s3Key),
		})
		if err != nil {
			return "", fmt.Errorf("failed to download from S3 (bucket=%s, key=%s): %w", bucket, s3Key, err)
		}
		defer result.Body.Close()

		audioData, err = io.ReadAll(result.Body)
		if err != nil {
			return "", fmt.Errorf("failed to read audio data from S3: %w", err)
		}
	} else {
		// Download from external HTTP URL
		ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
		defer cancel()

		req, err := http.NewRequestWithContext(ctx, "GET", audioURL, nil)
		if err != nil {
			return "", fmt.Errorf("failed to create HTTP request: %w", err)
		}

		resp, err := http.DefaultClient.Do(req)
		if err != nil {
			return "", fmt.Errorf("failed to download from URL %s: %w", audioURL, err)
		}
		defer resp.Body.Close()

		if resp.StatusCode != http.StatusOK {
			return "", fmt.Errorf("HTTP request failed with status %d for URL %s", resp.StatusCode, audioURL)
		}

		audioData, err = io.ReadAll(resp.Body)
		if err != nil {
			return "", fmt.Errorf("failed to read audio data from HTTP: %w", err)
		}
	}

	// Convert MP3 to WAV using ffmpeg (most audio files are MP3)
	wavData, err := convertToWAV(audioData)
	if err != nil {
		return "", fmt.Errorf("failed to convert audio to WAV: %w", err)
	}

	// Create a reader for the WAV data
	wavReader := bytes.NewReader(wavData)

	// Generate waveform PNG
	waveformPNG, err := generator.GenerateFromWAV(wavReader)
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

// convertToWAV converts any audio format (MP3, WAV, etc.) to WAV using ffmpeg
func convertToWAV(audioData []byte) ([]byte, error) {
	// Create temp files for input and output (ffmpeg needs files to write proper WAV headers)
	inputFile, err := os.CreateTemp("", "audio_input_*")
	if err != nil {
		return nil, fmt.Errorf("failed to create temp input file: %w", err)
	}
	defer os.Remove(inputFile.Name())
	defer inputFile.Close()

	outputFile, err := os.CreateTemp("", "audio_output_*.wav")
	if err != nil {
		return nil, fmt.Errorf("failed to create temp output file: %w", err)
	}
	outputPath := outputFile.Name()
	outputFile.Close() // Close so ffmpeg can write to it
	defer os.Remove(outputPath)

	// Write input data to temp file
	if _, err := inputFile.Write(audioData); err != nil {
		return nil, fmt.Errorf("failed to write input data: %w", err)
	}
	inputFile.Close()

	// Create ffmpeg command to convert any audio format to WAV
	// Using temp files allows ffmpeg to write proper WAV headers with correct sizes
	cmd := exec.Command("ffmpeg",
		"-y",                     // Overwrite output
		"-i", inputFile.Name(),   // Input from temp file
		"-f", "wav",              // Output format
		"-acodec", "pcm_s16le",   // 16-bit PCM (little-endian)
		"-ar", "44100",           // Sample rate
		"-ac", "1",               // Mono
		"-loglevel", "error",     // Only show errors
		outputPath,               // Output to temp file
	)

	// Capture stderr
	var stderr bytes.Buffer
	cmd.Stderr = &stderr

	// Run conversion
	if err := cmd.Run(); err != nil {
		return nil, fmt.Errorf("ffmpeg conversion failed (input: %d bytes): %v (stderr: %s)", len(audioData), err, stderr.String())
	}

	// Read output file
	wavData, err := os.ReadFile(outputPath)
	if err != nil {
		return nil, fmt.Errorf("failed to read output file: %w", err)
	}

	if len(wavData) == 0 {
		return nil, fmt.Errorf("ffmpeg produced no output (input: %d bytes, stderr: %s)", len(audioData), stderr.String())
	}

	log.Printf("   Converted %d bytes -> %d bytes WAV", len(audioData), len(wavData))
	return wavData, nil
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
