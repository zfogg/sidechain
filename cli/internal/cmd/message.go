package cmd

import (
	"fmt"
	"os"

	"github.com/spf13/cobra"
	"github.com/zfogg/sidechain/cli/pkg/prompter"
	"github.com/zfogg/sidechain/cli/pkg/service"
)

var (
	messagePage     int
	messagePageSize int
)

var messageCmd = &cobra.Command{
	Use:   "message",
	Short: "Direct messaging commands",
	Long:  "Send and manage direct messages with other users",
}

var messageSendCmd = &cobra.Command{
	Use:   "send <username>",
	Short: "Send a direct message",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		messageService := service.NewMessageService()

		// Prompt user for message content
		content, err := prompter.PromptString("Message: ")
		if err != nil {
			return err
		}

		if content == "" {
			fmt.Fprintf(os.Stderr, "Message cannot be empty.\n")
			os.Exit(1)
		}

		return messageService.SendMessage(args[0], content)
	},
}

var messageInboxCmd = &cobra.Command{
	Use:   "inbox",
	Short: "View your message inbox",
	RunE: func(cmd *cobra.Command, args []string) error {
		messageService := service.NewMessageService()
		return messageService.ListConversations(messagePage, messagePageSize)
	},
}

var messageThreadCmd = &cobra.Command{
	Use:   "thread <username>",
	Short: "View message thread with a user",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		messageService := service.NewMessageService()
		return messageService.ViewUserThread(args[0], messagePage, messagePageSize)
	},
}

var messageMarkReadCmd = &cobra.Command{
	Use:   "mark-read <username>",
	Short: "Mark messages as read",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		messageService := service.NewMessageService()
		return messageService.MarkAsRead(args[0])
	},
}

var messageUnreadCmd = &cobra.Command{
	Use:   "unread",
	Short: "Show unread message count",
	RunE: func(cmd *cobra.Command, args []string) error {
		messageService := service.NewMessageService()
		return messageService.ShowUnreadCount()
	},
}

var messageDeleteCmd = &cobra.Command{
	Use:   "delete <message-id>",
	Short: "Delete a message",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		messageService := service.NewMessageService()
		return messageService.DeleteMessage(args[0])
	},
}

func init() {
	// Pagination flags
	messageInboxCmd.Flags().IntVar(&messagePage, "page", 1, "Page number")
	messageInboxCmd.Flags().IntVar(&messagePageSize, "page-size", 10, "Results per page")

	messageThreadCmd.Flags().IntVar(&messagePage, "page", 1, "Page number")
	messageThreadCmd.Flags().IntVar(&messagePageSize, "page-size", 20, "Results per page")

	// Add subcommands
	messageCmd.AddCommand(messageSendCmd)
	messageCmd.AddCommand(messageInboxCmd)
	messageCmd.AddCommand(messageThreadCmd)
	messageCmd.AddCommand(messageMarkReadCmd)
	messageCmd.AddCommand(messageUnreadCmd)
	messageCmd.AddCommand(messageDeleteCmd)

	// Add message command to root
	rootCmd.AddCommand(messageCmd)
}
