package service

import (
	"bufio"
	"fmt"
	"os"
	"strings"
	"text/tabwriter"

	"github.com/zfogg/sidechain/cli/pkg/api"
	"github.com/zfogg/sidechain/cli/pkg/formatter"
	"github.com/zfogg/sidechain/cli/pkg/logger"
)

// MessagingService manages direct messaging operations
type MessagingService struct{}

// NewMessagingService creates a new messaging service
func NewMessagingService() *MessagingService {
	return &MessagingService{}
}

// ListConversations displays all active message conversations
func (ms *MessagingService) ListConversations(page, pageSize int) error {
	logger.Debug("Listing conversations", "page", page, "page_size", pageSize)

	response, err := api.GetConversations(page, pageSize)
	if err != nil {
		return err
	}

	if len(response.Threads) == 0 {
		fmt.Println("No conversations found")
		return nil
	}

	ms.displayConversationsList(response.Threads)
	return nil
}

// SendMessage interactively sends a direct message to a user
func (ms *MessagingService) SendMessage() error {
	logger.Debug("Sending message")

	formatter.PrintInfo("💬 Send Direct Message")

	reader := bufio.NewReader(os.Stdin)

	fmt.Print("\nRecipient username: ")
	username, _ := reader.ReadString('\n')
	username = strings.TrimSpace(username)

	if username == "" {
		return fmt.Errorf("recipient username is required")
	}

	fmt.Print("Message: ")
	content, _ := reader.ReadString('\n')
	content = strings.TrimSpace(content)

	if content == "" {
		return fmt.Errorf("message content is required")
	}

	// For now, we'll need the user ID. In production, this would use username lookup
	// For demo, we'll use username as ID
	msg, err := api.SendMessage(username, content)
	if err != nil {
		return err
	}

	formatter.PrintSuccess("Message sent!")
	fmt.Printf("To: %s\n", msg.Sender.Username)
	fmt.Printf("Message: %s\n", msg.Content)
	fmt.Printf("Sent at: %s\n\n", msg.CreatedAt.Format("2006-01-02 15:04:05"))

	return nil
}

// ViewThread displays messages from a specific conversation thread
func (ms *MessagingService) ViewThread(threadID string, page, pageSize int) error {
	logger.Debug("Viewing message thread", "thread_id", threadID)

	response, err := api.GetMessageThread(threadID, page, pageSize)
	if err != nil {
		return err
	}

	if len(response.Messages) == 0 {
		fmt.Println("No messages in this thread")
		return nil
	}

	// Mark as read
	_ = api.MarkMessagesAsRead(threadID)

	ms.displayMessageThread(response.Messages)
	return nil
}

// ViewUserThread displays messages with a specific user
func (ms *MessagingService) ViewUserThread(userID string, page, pageSize int) error {
	logger.Debug("Viewing user thread", "user_id", userID)

	response, err := api.GetUserThread(userID, page, pageSize)
	if err != nil {
		return err
	}

	if len(response.Messages) == 0 {
		fmt.Println("No messages with this user")
		return nil
	}

	// Mark as read
	_ = api.MarkMessagesAsRead(userID)

	ms.displayMessageThread(response.Messages)
	return nil
}

// GetUnreadCount displays the count of unread messages
func (ms *MessagingService) GetUnreadCount() error {
	logger.Debug("Getting unread message count")

	count, err := api.GetUnreadMessageCount()
	if err != nil {
		return err
	}

	if count == 0 {
		fmt.Println("📭 No unread messages")
	} else {
		formatter.PrintInfo(fmt.Sprintf("💬 %d unread message(s)", count))
	}

	return nil
}

// DeleteMessage deletes a specific message
func (ms *MessagingService) DeleteMessage(messageID string) error {
	logger.Debug("Deleting message", "message_id", messageID)

	reader := bufio.NewReader(os.Stdin)
	fmt.Print("Are you sure you want to delete this message? (y/n): ")
	confirm, _ := reader.ReadString('\n')
	confirm = strings.TrimSpace(strings.ToLower(confirm))

	if confirm != "y" && confirm != "yes" {
		fmt.Println("Deletion cancelled")
		return nil
	}

	err := api.DeleteMessage(messageID)
	if err != nil {
		return err
	}

	formatter.PrintSuccess("Message deleted")
	return nil
}

// Helper functions

func (ms *MessagingService) displayConversationsList(threads []api.MessageThread) {
	fmt.Printf("\n💬 Your Conversations\n")
	fmt.Println(strings.Repeat("=", 80))
	fmt.Printf("\n")

	w := tabwriter.NewWriter(os.Stdout, 0, 0, 2, ' ', 0)
	fmt.Fprintln(w, "User\tLast Message\tUnread\tUpdated")
	fmt.Fprintln(w, strings.Repeat("-", 70))

	for _, thread := range threads {
		lastMsg := ""
		if thread.LastMessage != nil {
			lastMsg = ms.truncate(thread.LastMessage.Content, 30)
		}

		unreadBadge := ""
		if thread.UnreadCount > 0 {
			unreadBadge = fmt.Sprintf("🔔 %d", thread.UnreadCount)
		}

		fmt.Fprintf(w, "%s\t%s\t%s\t%s\n",
			thread.Participant.Username,
			lastMsg,
			unreadBadge,
			thread.UpdatedAt.Format("2006-01-02 15:04"),
		)
	}

	w.Flush()
	fmt.Printf("\n")
}

func (ms *MessagingService) displayMessageThread(messages []api.Message) {
	fmt.Printf("\n💬 Message Thread\n")
	fmt.Println(strings.Repeat("=", 80))
	fmt.Printf("\n")

	for _, msg := range messages {
		senderName := msg.Sender.Username
		if senderName == "" {
			senderName = "Unknown"
		}

		fmt.Printf("%s [%s]\n", senderName, msg.CreatedAt.Format("2006-01-02 15:04:05"))
		fmt.Printf("  %s\n", msg.Content)

		if msg.IsDeleted {
			fmt.Printf("  (Message deleted)\n")
		}

		fmt.Printf("\n")
	}
}

func (ms *MessagingService) truncate(s string, maxLen int) string {
	if len(s) > maxLen {
		return s[:maxLen-3] + "..."
	}
	return s
}
