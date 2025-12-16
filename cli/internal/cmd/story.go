package cmd

import (
	"fmt"
	"os"

	"github.com/spf13/cobra"
	"github.com/zfogg/sidechain/cli/pkg/prompter"
	"github.com/zfogg/sidechain/cli/pkg/service"
)

var (
	storyPage     int
	storyPageSize int
)

var storyCmd = &cobra.Command{
	Use:   "story",
	Short: "Story management commands",
	Long:  "Create, view, and manage stories (24-hour expiring audio posts)",
}

var storyListCmd = &cobra.Command{
	Use:   "list",
	Short: "List active stories from followed users",
	RunE: func(cmd *cobra.Command, args []string) error {
		storyService := service.NewStoryService()
		return storyService.ListStories(storyPage, storyPageSize)
	},
}

var storyViewCmd = &cobra.Command{
	Use:   "view <story-id>",
	Short: "View a specific story",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		storyService := service.NewStoryService()
		return storyService.ViewStory(args[0])
	},
}

var storyUserCmd = &cobra.Command{
	Use:   "user <username>",
	Short: "View all stories from a user",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		storyService := service.NewStoryService()
		return storyService.ViewUserStories(args[0])
	},
}

var storyViewersCmd = &cobra.Command{
	Use:   "viewers <story-id>",
	Short: "View who watched a story (owner only)",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		storyService := service.NewStoryService()
		return storyService.ViewStoryViewers(args[0])
	},
}

var storyDeleteCmd = &cobra.Command{
	Use:   "delete <story-id>",
	Short: "Delete a story",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		storyService := service.NewStoryService()
		return storyService.DeleteStory(args[0])
	},
}

var highlightCreateCmd = &cobra.Command{
	Use:   "highlight create",
	Short: "Create a new highlight collection",
	RunE: func(cmd *cobra.Command, args []string) error {
		storyService := service.NewStoryService()

		// Prompt for highlight name
		name, err := prompter.PromptString("Highlight name: ")
		if err != nil {
			return err
		}

		if name == "" {
			fmt.Fprintf(os.Stderr, "Highlight name cannot be empty.\n")
			os.Exit(1)
		}

		// Prompt for description
		description, err := prompter.PromptString("Description (optional): ")
		if err != nil {
			return err
		}

		return storyService.CreateHighlight(name, description)
	},
}

var highlightListCmd = &cobra.Command{
	Use:   "highlight list <username>",
	Short: "List highlights for a user",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		storyService := service.NewStoryService()
		return storyService.ListHighlights(args[0])
	},
}

var highlightViewCmd = &cobra.Command{
	Use:   "highlight view <highlight-id>",
	Short: "View a highlight collection",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		storyService := service.NewStoryService()
		return storyService.ViewHighlight(args[0])
	},
}

var highlightUpdateCmd = &cobra.Command{
	Use:   "highlight update <highlight-id>",
	Short: "Update a highlight",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		storyService := service.NewStoryService()

		// Prompt for highlight name
		name, err := prompter.PromptString("New name: ")
		if err != nil {
			return err
		}

		if name == "" {
			fmt.Fprintf(os.Stderr, "Name cannot be empty.\n")
			os.Exit(1)
		}

		// Prompt for description
		description, err := prompter.PromptString("New description (optional): ")
		if err != nil {
			return err
		}

		return storyService.UpdateHighlight(args[0], name, description)
	},
}

var highlightDeleteCmd = &cobra.Command{
	Use:   "highlight delete <highlight-id>",
	Short: "Delete a highlight",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		storyService := service.NewStoryService()
		return storyService.DeleteHighlight(args[0])
	},
}

var highlightAddCmd = &cobra.Command{
	Use:   "highlight add <highlight-id> <story-id>",
	Short: "Add a story to a highlight",
	Args:  cobra.ExactArgs(2),
	RunE: func(cmd *cobra.Command, args []string) error {
		storyService := service.NewStoryService()
		return storyService.AddStoryToHighlight(args[0], args[1])
	},
}

var highlightRemoveCmd = &cobra.Command{
	Use:   "highlight remove <highlight-id> <story-id>",
	Short: "Remove a story from a highlight",
	Args:  cobra.ExactArgs(2),
	RunE: func(cmd *cobra.Command, args []string) error {
		storyService := service.NewStoryService()
		return storyService.RemoveStoryFromHighlight(args[0], args[1])
	},
}

func init() {
	// Story list/pagination flags
	storyListCmd.Flags().IntVar(&storyPage, "page", 1, "Page number")
	storyListCmd.Flags().IntVar(&storyPageSize, "page-size", 10, "Results per page")

	// Add story subcommands
	storyCmd.AddCommand(storyListCmd)
	storyCmd.AddCommand(storyViewCmd)
	storyCmd.AddCommand(storyUserCmd)
	storyCmd.AddCommand(storyViewersCmd)
	storyCmd.AddCommand(storyDeleteCmd)

	// Add highlight subcommands
	storyCmd.AddCommand(highlightCreateCmd)
	storyCmd.AddCommand(highlightListCmd)
	storyCmd.AddCommand(highlightViewCmd)
	storyCmd.AddCommand(highlightUpdateCmd)
	storyCmd.AddCommand(highlightDeleteCmd)
	storyCmd.AddCommand(highlightAddCmd)
	storyCmd.AddCommand(highlightRemoveCmd)

	// Add story command to root
	rootCmd.AddCommand(storyCmd)
}
