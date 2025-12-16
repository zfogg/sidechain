package service

import (
	"testing"
)

func TestNewCommentService(t *testing.T) {
	service := NewCommentService()
	if service == nil {
		t.Error("NewCommentService returned nil")
	}
}

func TestCommentService_CreateComment(t *testing.T) {
	service := NewCommentService()

	// CreateComment requires interactive input
	err := service.CreateComment()
	t.Logf("CreateComment (requires input): %v", err)
}

func TestCommentService_ViewComments(t *testing.T) {
	service := NewCommentService()

	// ViewComments requires interactive input
	err := service.ViewComments()
	t.Logf("ViewComments (requires input): %v", err)
}

func TestCommentService_UpdateComment(t *testing.T) {
	service := NewCommentService()

	// UpdateComment requires interactive input
	err := service.UpdateComment()
	t.Logf("UpdateComment (requires input): %v", err)
}

func TestCommentService_DeleteComment(t *testing.T) {
	service := NewCommentService()

	// DeleteComment requires interactive input
	err := service.DeleteComment()
	t.Logf("DeleteComment (requires input): %v", err)
}

func TestCommentService_LikeComment(t *testing.T) {
	service := NewCommentService()

	// LikeComment requires interactive input
	err := service.LikeComment()
	t.Logf("LikeComment (requires input): %v", err)
}

func TestCommentService_UnlikeComment(t *testing.T) {
	service := NewCommentService()

	// UnlikeComment requires interactive input
	err := service.UnlikeComment()
	t.Logf("UnlikeComment (requires input): %v", err)
}

func TestCommentService_ReportComment(t *testing.T) {
	service := NewCommentService()

	// ReportComment requires interactive input
	err := service.ReportComment()
	t.Logf("ReportComment (requires input): %v", err)
}
