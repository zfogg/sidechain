package storage

import (
	"context"
	"mime/multipart"
)

// ProfilePictureUploader defines the interface for uploading profile pictures
// This interface allows for easy mocking in tests
type ProfilePictureUploader interface {
	UploadProfilePicture(ctx context.Context, file multipart.File, header *multipart.FileHeader, userID string) (*UploadResult, error)
}

// Ensure S3Uploader implements ProfilePictureUploader
var _ ProfilePictureUploader = (*S3Uploader)(nil)
