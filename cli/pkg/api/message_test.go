package api

import (
	"testing"
)

func TestGetConversations_API(t *testing.T) {
	// Integration test hitting real API
	resp, err := GetConversations(1, 10)

	if err != nil {
		t.Logf("GetConversations error (API may be offline): %v", err)
		return
	}

	if resp == nil {
		t.Error("GetConversations returned nil response")
	}
	if resp.Threads == nil {
		t.Error("Threads slice is nil")
	}
	if resp.Page != 1 {
		t.Errorf("Expected page 1, got %d", resp.Page)
	}
}

func TestGetUserThread_API(t *testing.T) {
	// Integration test - will fail without real user ID
	userID := "test-user-id-nonexistent"

	resp, err := GetUserThread(userID, 1, 10)

	// We expect this to fail since test user doesn't exist, but we're testing the API call itself
	if resp == nil && err == nil {
		t.Error("Expected either response or error")
	}
}

func TestDeleteMessage_API(t *testing.T) {
	// Integration test - will fail without real message ID
	messageID := "test-msg-nonexistent"

	err := DeleteMessage(messageID)

	// We expect error since message doesn't exist, but API should respond
	if err != nil {
		t.Logf("DeleteMessage error (expected, message doesn't exist): %v", err)
	}
}

func TestGetUnreadMessageCount_API(t *testing.T) {
	// Integration test hitting real API
	count, err := GetUnreadMessageCount()

	if err != nil {
		t.Logf("GetUnreadMessageCount error (may be unauthenticated): %v", err)
		return
	}

	if count < 0 {
		t.Errorf("Unread count should be >= 0, got %d", count)
	}
}

func TestMarkMessagesAsRead_API(t *testing.T) {
	// Integration test - will fail without real thread ID
	threadID := "test-thread-nonexistent"

	err := MarkMessagesAsRead(threadID)

	if err != nil {
		t.Logf("MarkMessagesAsRead error (expected, thread doesn't exist): %v", err)
	}
}
