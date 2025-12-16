package service

import (
	"fmt"
	"strings"

	"github.com/zfogg/sidechain/cli/pkg/api"
	"github.com/zfogg/sidechain/cli/pkg/logger"
)

// MessageService handles direct messaging operations
type MessageService struct{}

// NewMessageService creates a new message service
func NewMessageService() *MessageService {
	return &MessageService{}
}

// SendMessage sends a direct message to a user
func (ms *MessageService) SendMessage(username string, content string) error {
	logger.Debug("Sending message to user", "username", username)

	if content == "" {
		return fmt.Errorf("message content cannot be empty")
	}

	if len(content) > 5000 {
		return fmt.Errorf("message exceeds maximum length (5000 characters)")
	}

	// Get recipient user ID from username
	userProfile, err := api.GetUserProfile(username)
	if err != nil {
		logger.Error("Failed to find user", "error", err)
		return fmt.Errorf("user '%s' not found", username)
	}

	// Send the message
	message, err := api.SendMessage(userProfile.ID, content)
	if err != nil {
		logger.Error("Failed to send message", "error", err)
		return err
	}

	logger.Debug("Message sent successfully", "message_id", message.ID)
	fmt.Printf("✓ Message sent to %s\n", username)
	return nil
}

// ListConversations lists all message conversations
func (ms *MessageService) ListConversations(page, pageSize int) error {
	logger.Debug("Listing conversations", "page", page)

	if page < 1 {
		page = 1
	}
	if pageSize < 1 || pageSize > 100 {
		pageSize = 10
	}

	threads, err := api.GetConversations(page, pageSize)
	if err != nil {
		logger.Error("Failed to fetch conversations", "error", err)
		return err
	}

	if len(threads.Threads) == 0 {
		fmt.Println("No conversations yet. Start a conversation with `message send <username>`")
		return nil
	}

	ms.displayConversationList(threads)
	return nil
}

// ViewUserThread views messages with a specific user
func (ms *MessageService) ViewUserThread(username string, page, pageSize int) error {
	logger.Debug("Viewing thread with user", "username", username)

	if page < 1 {
		page = 1
	}
	if pageSize < 1 || pageSize > 100 {
		pageSize = 20
	}

	// Get user ID from username
	userProfile, err := api.GetUserProfile(username)
	if err != nil {
		logger.Error("Failed to find user", "error", err)
		return fmt.Errorf("user '%s' not found", username)
	}

	// Fetch messages
	messages, err := api.GetUserThread(userProfile.ID, page, pageSize)
	if err != nil {
		logger.Error("Failed to fetch messages", "error", err)
		return err
	}

	if len(messages.Messages) == 0 {
		fmt.Printf("No messages with %s yet.\n", username)
		return nil
	}

	ms.displayMessageThread(username, messages)
	return nil
}

// MarkAsRead marks messages with a user as read
func (ms *MessageService) MarkAsRead(username string) error {
	logger.Debug("Marking messages as read", "username", username)

	// Get user ID from username
	userProfile, err := api.GetUserProfile(username)
	if err != nil {
		logger.Error("Failed to find user", "error", err)
		return fmt.Errorf("user '%s' not found", username)
	}

	err = api.MarkMessagesAsRead(userProfile.ID)
	if err != nil {
		logger.Error("Failed to mark messages as read", "error", err)
		return err
	}

	logger.Debug("Messages marked as read", "user_id", userProfile.ID)
	fmt.Printf("✓ Marked messages with %s as read\n", username)
	return nil
}

// ShowUnreadCount displays the count of unread messages
func (ms *MessageService) ShowUnreadCount() error {
	logger.Debug("Fetching unread count")

	count, err := api.GetUnreadMessageCount()
	if err != nil {
		logger.Error("Failed to fetch unread count", "error", err)
		return err
	}

	if count == 0 {
		fmt.Println("No unread messages.")
	} else {
		fmt.Printf("You have %d unread message%s.\n", count, pluralize(count))
	}

	return nil
}

// DeleteMessage deletes a message
func (ms *MessageService) DeleteMessage(messageID string) error {
	logger.Debug("Deleting message", "message_id", messageID)

	err := api.DeleteMessage(messageID)
	if err != nil {
		logger.Error("Failed to delete message", "error", err)
		return err
	}

	logger.Debug("Message deleted successfully")
	fmt.Println("✓ Message deleted")
	return nil
}

// displayConversationList displays a list of conversations
func (ms *MessageService) displayConversationList(threads *api.ThreadListResponse) {
	fmt.Printf("\n📱 Conversations (Page %d)\n", threads.Page)
	fmt.Println(strings.Repeat("─", 60))

	for i, thread := range threads.Threads {
		unreadBadge := ""
		if thread.UnreadCount > 0 {
			unreadBadge = fmt.Sprintf(" [%d unread]", thread.UnreadCount)
		}

		lastMessagePreview := "(No messages)"
		if thread.LastMessage != nil {
			preview := thread.LastMessage.Content
			if len(preview) > 40 {
				preview = preview[:37] + "..."
			}
			lastMessagePreview = fmt.Sprintf(`"%s"`, preview)
		}

		fmt.Printf("%2d. @%s%s\n", i+1, thread.Participant.Username, unreadBadge)
		fmt.Printf("    Last: %s\n", lastMessagePreview)
		if i < len(threads.Threads)-1 {
			fmt.Println(strings.Repeat("─", 60))
		}
	}

	fmt.Printf("\nShowing %d of %d conversations\n\n", len(threads.Threads), threads.TotalCount)
}

// SearchMessages searches for messages containing a query string
func (ms *MessageService) SearchMessages(query string, page, pageSize int) error {
	logger.Debug("Searching messages", "query", query)

	if query == "" {
		return fmt.Errorf("search query is required")
	}

	// Get all conversations and search through them
	threads, err := api.GetConversations(1, 100)
	if err != nil {
		logger.Error("Failed to fetch conversations", "error", err)
		return err
	}

	fmt.Printf("\n🔍 Searching for: \"%s\"\n", query)
	fmt.Println(strings.Repeat("─", 60))

	matchCount := 0
	for _, thread := range threads.Threads {
		messages, err := api.GetUserThread(thread.Participant.ID, 1, 100)
		if err != nil {
			continue
		}

		for _, msg := range messages.Messages {
			if strings.Contains(strings.ToLower(msg.Content), strings.ToLower(query)) {
				matchCount++
				preview := msg.Content
				if len(preview) > 50 {
					preview = preview[:47] + "..."
				}
				fmt.Printf("@%s: %s\n", msg.Sender.Username, preview)
				fmt.Printf("  Date: %s\n\n", msg.CreatedAt.Format("2006-01-02 15:04:05"))
			}
		}
	}

	if matchCount == 0 {
		fmt.Printf("No messages found containing \"%s\"\n\n", query)
	} else {
		fmt.Printf("Found %d matching message(s)\n\n", matchCount)
	}

	return nil
}

// ClearConversation clears the message history with a specific user
func (ms *MessageService) ClearConversation(username string) error {
	logger.Debug("Clearing conversation", "username", username)

	// Get user ID from username
	userProfile, err := api.GetUserProfile(username)
	if err != nil {
		logger.Error("Failed to find user", "error", err)
		return fmt.Errorf("user '%s' not found", username)
	}

	// Get all messages in thread
	messages, err := api.GetUserThread(userProfile.ID, 1, 1000)
	if err != nil {
		logger.Error("Failed to fetch messages", "error", err)
		return err
	}

	if len(messages.Messages) == 0 {
		fmt.Printf("No messages to clear with %s\n", username)
		return nil
	}

	fmt.Printf("This will delete %d message(s) with @%s.\n", len(messages.Messages), username)
	fmt.Print("Are you sure? (y/n): ")

	var confirm string
	fmt.Scanln(&confirm)

	if confirm != "y" && confirm != "yes" {
		fmt.Println("Cancelled.")
		return nil
	}

	deletedCount := 0
	for _, msg := range messages.Messages {
		err := api.DeleteMessage(msg.ID)
		if err == nil {
			deletedCount++
		}
	}

	fmt.Printf("✓ Deleted %d message(s)\n", deletedCount)
	return nil
}

// displayMessageThread displays a message thread
func (ms *MessageService) displayMessageThread(username string, messages *api.MessageListResponse) {
	fmt.Printf("\n💬 Messages with @%s (Page %d)\n", username, messages.Page)
	fmt.Println(strings.Repeat("─", 60))

	// Get current user for comparison
	currentUser, err := api.GetCurrentUser()
	currentUserID := ""
	if err == nil && currentUser != nil {
		currentUserID = currentUser.ID
	}

	for _, msg := range messages.Messages {
		senderLabel := "You"
		if msg.SenderID != currentUserID {
			senderLabel = "@" + msg.Sender.Username
		}

		fmt.Printf("%s (%s):\n", senderLabel, msg.CreatedAt.Format("15:04 Jan 02"))
		fmt.Printf("  %s\n\n", msg.Content)
	}

	fmt.Printf("Showing %d of %d messages\n\n", len(messages.Messages), messages.TotalCount)
}
