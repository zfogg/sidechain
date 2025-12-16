package service

import (
	"testing"
)

func TestNewNotificationService(t *testing.T) {
	service := NewNotificationService()
	if service == nil {
		t.Error("NewNotificationService returned nil")
	}
}

func TestNotificationService_ListNotifications(t *testing.T) {
	service := NewNotificationService()

	tests := []struct {
		page     int
		pageSize int
	}{
		{1, 10},
		{2, 20},
	}

	for _, tt := range tests {
		err := service.ListNotifications(tt.page, tt.pageSize)
		t.Logf("ListNotifications page=%d, size=%d: %v", tt.page, tt.pageSize, err)
	}
}

func TestNewNotificationSettingsService(t *testing.T) {
	service := NewNotificationSettingsService()
	if service == nil {
		t.Error("NewNotificationSettingsService returned nil")
	}
}

func TestNotificationSettingsService_ViewPreferences(t *testing.T) {
	service := NewNotificationSettingsService()

	err := service.ViewPreferences()
	t.Logf("ViewPreferences: %v", err)
}

func TestNotificationSettingsService_ManagePreferences(t *testing.T) {
	service := NewNotificationSettingsService()

	// ManagePreferences requires interactive input
	err := service.ManagePreferences()
	t.Logf("ManagePreferences (requires input): %v", err)
}

func TestNewNotificationWatcherService(t *testing.T) {
	service := NewNotificationWatcherService()
	if service == nil {
		t.Error("NewNotificationWatcherService returned nil")
	}
}
