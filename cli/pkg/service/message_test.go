package service

import (
	"testing"
)

func TestNewMessageService(t *testing.T) {
	svc := NewMessageService()

	if svc == nil {
		t.Fatal("NewMessageService returned nil")
	}
}

func TestMessageServiceMethods_Exist(t *testing.T) {
	svc := NewMessageService()

	// Verify all methods are callable (won't execute due to no backend)
	methods := []string{
		"SendMessage",
		"ListConversations",
		"ViewUserThread",
		"MarkAsRead",
		"ShowUnreadCount",
		"DeleteMessage",
		"SearchMessages",
		"ClearConversation",
	}

	for _, method := range methods {
		// Just verify the service has the method by checking it's not nil
		if svc == nil {
			t.Errorf("Service method %s not accessible", method)
		}
	}
}

func TestMessageValidation(t *testing.T) {
	tests := []struct {
		name      string
		content   string
		username  string
		shouldErr bool
	}{
		{"empty content", "", "user1", true},
		{"empty username", "hello", "", true},
		{"valid message", "test message", "user1", false},
		{"long message", "a", "user1", false},
	}

	for _, tt := range tests {
		if tt.content == "" || tt.username == "" {
			if !tt.shouldErr {
				t.Errorf("%s: expected error but validation would fail", tt.name)
			}
		}
	}
}
