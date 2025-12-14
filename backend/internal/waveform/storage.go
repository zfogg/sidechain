package waveform

import (
	"bytes"
	"fmt"
	"path/filepath"
	"time"

	"github.com/aws/aws-sdk-go/aws"
	"github.com/aws/aws-sdk-go/aws/session"
	"github.com/aws/aws-sdk-go/service/s3"
)

// Storage handles uploading waveform images to S3
type Storage struct {
	s3Client   *s3.S3
	bucketName string
	region     string
	cdnURL     string
}

// NewStorage creates a new waveform storage client
func NewStorage(region, bucketName, cdnURL string) (*Storage, error) {
	sess, err := session.NewSession(&aws.Config{
		Region: aws.String(region),
	})
	if err != nil {
		return nil, fmt.Errorf("failed to create AWS session: %w", err)
	}

	return &Storage{
		s3Client:   s3.New(sess),
		bucketName: bucketName,
		region:     region,
		cdnURL:     cdnURL,
	}, nil
}

// UploadWaveform uploads a waveform PNG to S3 and returns the CDN URL
func (s *Storage) UploadWaveform(waveformData []byte, userID, postID string) (string, error) {
	// Generate unique filename
	timestamp := time.Now().Unix()
	filename := fmt.Sprintf("waveforms/%s/%s_%d.png", userID, postID, timestamp)

	// Upload to S3
	// Note: No ACL set - bucket policy should handle public access
	_, err := s.s3Client.PutObject(&s3.PutObjectInput{
		Bucket:       aws.String(s.bucketName),
		Key:          aws.String(filename),
		Body:         bytes.NewReader(waveformData),
		ContentType:  aws.String("image/png"),
		CacheControl: aws.String("public, max-age=31536000"), // Cache for 1 year
	})
	if err != nil {
		return "", fmt.Errorf("failed to upload to S3: %w", err)
	}

	// Return CDN URL
	cdnURL := s.getCDNURL(filename)
	return cdnURL, nil
}

// getCDNURL constructs the CDN URL for a given S3 key
func (s *Storage) getCDNURL(key string) string {
	if s.cdnURL != "" {
		return fmt.Sprintf("%s/%s", s.cdnURL, key)
	}
	// Fallback to S3 URL if no CDN configured
	return fmt.Sprintf("https://%s.s3.%s.amazonaws.com/%s", s.bucketName, s.region, key)
}

// DeleteWaveform deletes a waveform from S3
func (s *Storage) DeleteWaveform(waveformURL string) error {
	// Extract key from URL
	key := s.extractKeyFromURL(waveformURL)
	if key == "" {
		return fmt.Errorf("invalid waveform URL: %s", waveformURL)
	}

	_, err := s.s3Client.DeleteObject(&s3.DeleteObjectInput{
		Bucket: aws.String(s.bucketName),
		Key:    aws.String(key),
	})
	if err != nil {
		return fmt.Errorf("failed to delete from S3: %w", err)
	}

	return nil
}

// extractKeyFromURL extracts the S3 key from a CDN or S3 URL
func (s *Storage) extractKeyFromURL(url string) string {
	// Extract filename from URL
	// Handle both CDN URLs and direct S3 URLs
	return filepath.Base(url)
}
