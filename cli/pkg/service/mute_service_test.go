package service

import (
	"testing"
)

func TestNewMuteService(t *testing.T) {
	service := NewMuteService()
	if service == nil {
		t.Error("NewMuteService returned nil")
	}
}

func TestMuteService_MuteUser(t *testing.T) {
	service := NewMuteService()

	// MuteUser requires interactive input
	err := service.MuteUser()
	t.Logf("MuteUser (requires input): %v", err)
}

func TestMuteService_UnmuteUser(t *testing.T) {
	service := NewMuteService()

	// UnmuteUser requires interactive input
	err := service.UnmuteUser()
	t.Logf("UnmuteUser (requires input): %v", err)
}

func TestMuteService_ListMutedUsers(t *testing.T) {
	service := NewMuteService()

	tests := []struct {
		page  int
		limit int
	}{
		{1, 10},
		{2, 20},
	}

	for _, tt := range tests {
		err := service.ListMutedUsers(tt.page, tt.limit)
		t.Logf("ListMutedUsers page=%d, limit=%d: %v", tt.page, tt.limit, err)
	}
}
