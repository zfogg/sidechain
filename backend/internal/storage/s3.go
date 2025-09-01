package storage

import (
	"bytes"
	"context"
	"fmt"
	"path/filepath"
	"strings"
	"time"

	"github.com/aws/aws-sdk-go-v2/aws"
	"github.com/aws/aws-sdk-go-v2/config"
	"github.com/aws/aws-sdk-go-v2/service/s3"
	"github.com/google/uuid"
)

// S3Uploader handles audio file uploads to AWS S3
type S3Uploader struct {
	client  *s3.Client
	bucket  string
	region  string
	baseURL string
}

// UploadResult contains the result of an S3 upload
type UploadResult struct {
	Key    string `json:"key"`
	URL    string `json:"url"`
	Bucket string `json:"bucket"`
	Region string `json:"region"`
	Size   int64  `json:"size"`
}

// NewS3Uploader creates a new S3 uploader
func NewS3Uploader(region, bucket, baseURL string) (*S3Uploader, error) {
	cfg, err := config.LoadDefaultConfig(context.TODO(),
		config.WithRegion(region),
	)
	if err != nil {
		return nil, fmt.Errorf("failed to load AWS config: %w", err)
	}

	client := s3.NewFromConfig(cfg)

	return &S3Uploader{
		client:  client,
		bucket:  bucket,
		region:  region,
		baseURL: baseURL,
	}, nil
}

// UploadAudio uploads audio file to S3 with proper naming and metadata
func (u *S3Uploader) UploadAudio(ctx context.Context, audioData []byte, userID, originalFilename string) (*UploadResult, error) {
	// Generate unique key for the audio file
	fileID := uuid.New().String()
	extension := filepath.Ext(originalFilename)
	if extension == "" {
		extension = ".mp3"
	}

	// Use organized folder structure: audio/{year}/{month}/{userID}/{fileID}.mp3
	now := time.Now()
	key := fmt.Sprintf("audio/%d/%02d/%s/%s%s",
		now.Year(), now.Month(), userID, fileID, extension)

	// Set up upload parameters
	putObjectInput := &s3.PutObjectInput{
		Bucket:      aws.String(u.bucket),
		Key:         aws.String(key),
		Body:        bytes.NewReader(audioData),
		ContentType: aws.String(getContentType(extension)),

		// Cache for 1 hour (audio files don't change)
		CacheControl: aws.String("max-age=3600"),

		// Set metadata
		Metadata: map[string]string{
			"user-id":           userID,
			"original-filename": originalFilename,
			"upload-timestamp":  now.Format(time.RFC3339),
			"file-type":         "audio",
		},

		// Note: Removed ACL - bucket policy should handle public access
	}

	// Upload to S3
	_, err := u.client.PutObject(ctx, putObjectInput)
	if err != nil {
		return nil, fmt.Errorf("failed to upload to S3: %w", err)
	}

	// Generate public URL
	publicURL := fmt.Sprintf("%s/%s", strings.TrimSuffix(u.baseURL, "/"), key)

	return &UploadResult{
		Key:    key,
		URL:    publicURL,
		Bucket: u.bucket,
		Region: u.region,
		Size:   int64(len(audioData)),
	}, nil
}

// UploadWaveform uploads waveform SVG visualization
func (u *S3Uploader) UploadWaveform(ctx context.Context, svgData []byte, audioKey string) (*UploadResult, error) {
	// Generate waveform key based on audio key
	waveformKey := strings.Replace(audioKey, filepath.Ext(audioKey), "_waveform.svg", 1)

	putObjectInput := &s3.PutObjectInput{
		Bucket:       aws.String(u.bucket),
		Key:          aws.String(waveformKey),
		Body:         bytes.NewReader(svgData),
		ContentType:  aws.String("image/svg+xml"),
		CacheControl: aws.String("max-age=86400"), // Cache waveforms for 24 hours
		// Note: Removed ACL - bucket policy should handle public access

		Metadata: map[string]string{
			"file-type":     "waveform",
			"related-audio": audioKey,
		},
	}

	_, err := u.client.PutObject(ctx, putObjectInput)
	if err != nil {
		return nil, fmt.Errorf("failed to upload waveform: %w", err)
	}

	publicURL := fmt.Sprintf("%s/%s", strings.TrimSuffix(u.baseURL, "/"), waveformKey)

	return &UploadResult{
		Key:    waveformKey,
		URL:    publicURL,
		Bucket: u.bucket,
		Region: u.region,
		Size:   int64(len(svgData)),
	}, nil
}

// DeleteFile deletes a file from S3
func (u *S3Uploader) DeleteFile(ctx context.Context, key string) error {
	_, err := u.client.DeleteObject(ctx, &s3.DeleteObjectInput{
		Bucket: aws.String(u.bucket),
		Key:    aws.String(key),
	})
	if err != nil {
		return fmt.Errorf("failed to delete from S3: %w", err)
	}

	return nil
}

// CheckBucketAccess verifies that we can access the S3 bucket
func (u *S3Uploader) CheckBucketAccess(ctx context.Context) error {
	_, err := u.client.HeadBucket(ctx, &s3.HeadBucketInput{
		Bucket: aws.String(u.bucket),
	})
	if err != nil {
		return fmt.Errorf("cannot access S3 bucket %s: %w", u.bucket, err)
	}

	return nil
}

// getContentType returns the appropriate MIME type for file extensions
func getContentType(extension string) string {
	switch strings.ToLower(extension) {
	case ".mp3":
		return "audio/mpeg"
	case ".wav":
		return "audio/wav"
	case ".ogg":
		return "audio/ogg"
	case ".m4a":
		return "audio/mp4"
	case ".svg":
		return "image/svg+xml"
	default:
		return "application/octet-stream"
	}
}
