package api

import (
	"fmt"
	"time"

	"github.com/zfogg/sidechain/cli/pkg/client"
	"github.com/zfogg/sidechain/cli/pkg/logger"
)

// Message represents a direct message
type Message struct {
	ID        string    `json:"id"`
	ThreadID  string    `json:"thread_id"`
	SenderID  string    `json:"sender_id"`
	Sender    User      `json:"sender"`
	Content   string    `json:"content"`
	CreatedAt time.Time `json:"created_at"`
	UpdatedAt time.Time `json:"updated_at"`
	IsDeleted bool      `json:"is_deleted"`
}

// MessageThread represents a conversation thread
type MessageThread struct {
	ID            string    `json:"id"`
	ParticipantID string    `json:"participant_id"`
	Participant   User      `json:"participant"`
	LastMessage   *Message  `json:"last_message"`
	UnreadCount   int       `json:"unread_count"`
	UpdatedAt     time.Time `json:"updated_at"`
}

// MessageListResponse wraps a list of messages
type MessageListResponse struct {
	Messages   []Message `json:"messages"`
	TotalCount int       `json:"total_count"`
	Page       int       `json:"page"`
	PageSize   int       `json:"page_size"`
}

// ThreadListResponse wraps a list of message threads
type ThreadListResponse struct {
	Threads    []MessageThread `json:"threads"`
	TotalCount int             `json:"total_count"`
	Page       int             `json:"page"`
	PageSize   int             `json:"page_size"`
}

// SendMessageRequest is the request to send a message
type SendMessageRequest struct {
	RecipientID string `json:"recipient_id"`
	Content     string `json:"content"`
}

// SendMessage sends a direct message to a user
func SendMessage(recipientID string, content string) (*Message, error) {
	logger.Debug("Sending message", "recipient_id", recipientID)

	req := SendMessageRequest{
		RecipientID: recipientID,
		Content:     content,
	}

	var response struct {
		Message Message `json:"message"`
	}

	resp, err := client.GetClient().
		R().
		SetBody(req).
		SetResult(&response).
		Post("/api/v1/messages")

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to send message: %s", resp.Status())
	}

	return &response.Message, nil
}

// GetMessageThread retrieves messages in a conversation thread
func GetMessageThread(threadID string, page int, pageSize int) (*MessageListResponse, error) {
	logger.Debug("Fetching message thread", "thread_id", threadID)

	var response MessageListResponse
	resp, err := client.GetClient().
		R().
		SetQueryParam("page", fmt.Sprintf("%d", page)).
		SetQueryParam("page_size", fmt.Sprintf("%d", pageSize)).
		SetResult(&response).
		Get(fmt.Sprintf("/api/v1/messages/%s", threadID))

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to fetch message thread: %s", resp.Status())
	}

	return &response, nil
}

// GetConversations retrieves all message conversations
func GetConversations(page int, pageSize int) (*ThreadListResponse, error) {
	logger.Debug("Fetching conversations", "page", page)

	var response ThreadListResponse
	resp, err := client.GetClient().
		R().
		SetQueryParam("page", fmt.Sprintf("%d", page)).
		SetQueryParam("page_size", fmt.Sprintf("%d", pageSize)).
		SetResult(&response).
		Get("/api/v1/messages/conversations")

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to fetch conversations: %s", resp.Status())
	}

	return &response, nil
}

// GetUserThread retrieves messages with a specific user
func GetUserThread(userID string, page int, pageSize int) (*MessageListResponse, error) {
	logger.Debug("Fetching messages with user", "user_id", userID)

	var response MessageListResponse
	resp, err := client.GetClient().
		R().
		SetQueryParam("page", fmt.Sprintf("%d", page)).
		SetQueryParam("page_size", fmt.Sprintf("%d", pageSize)).
		SetResult(&response).
		Get(fmt.Sprintf("/api/v1/messages/user/%s", userID))

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to fetch user thread: %s", resp.Status())
	}

	return &response, nil
}

// DeleteMessage deletes a message
func DeleteMessage(messageID string) error {
	logger.Debug("Deleting message", "message_id", messageID)

	resp, err := client.GetClient().
		R().
		Delete(fmt.Sprintf("/api/v1/messages/%s", messageID))

	if err != nil {
		return err
	}

	if !resp.IsSuccess() {
		return fmt.Errorf("failed to delete message: %s", resp.Status())
	}

	return nil
}

// MarkMessagesAsRead marks all messages in a thread as read
func MarkMessagesAsRead(threadID string) error {
	logger.Debug("Marking messages as read", "thread_id", threadID)

	resp, err := client.GetClient().
		R().
		Post(fmt.Sprintf("/api/v1/messages/%s/mark-read", threadID))

	if err != nil {
		return err
	}

	if !resp.IsSuccess() {
		return fmt.Errorf("failed to mark messages as read: %s", resp.Status())
	}

	return nil
}

// GetUnreadMessageCount gets the count of unread messages
func GetUnreadMessageCount() (int, error) {
	logger.Debug("Fetching unread message count")

	var response struct {
		UnreadCount int `json:"unread_count"`
	}

	resp, err := client.GetClient().
		R().
		SetResult(&response).
		Get("/api/v1/messages/unread-count")

	if err != nil {
		return 0, err
	}

	if !resp.IsSuccess() {
		return 0, fmt.Errorf("failed to fetch unread count: %s", resp.Status())
	}

	return response.UnreadCount, nil
}
