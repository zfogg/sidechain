package cmd

import (
	"fmt"
	"os"

	"github.com/spf13/cobra"
	"github.com/zfogg/sidechain/cli/pkg/prompter"
	"github.com/zfogg/sidechain/cli/pkg/service"
)

var (
	midiLimit  int
	midiOffset int
)

var midiCmd = &cobra.Command{
	Use:   "midi",
	Short: "MIDI pattern and challenge management",
	Long:  "Create, manage, and participate in MIDI patterns and challenges",
}

// MIDI Pattern Commands

var midiListCmd = &cobra.Command{
	Use:   "list",
	Short: "List your MIDI patterns",
	RunE: func(cmd *cobra.Command, args []string) error {
		midiService := service.NewMIDIService()
		return midiService.ListMIDIPatterns(midiLimit, midiOffset)
	},
}

var midiViewCmd = &cobra.Command{
	Use:   "view <pattern-id>",
	Short: "View a MIDI pattern",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		midiService := service.NewMIDIService()
		return midiService.ViewMIDIPattern(args[0])
	},
}

var midiUpdateCmd = &cobra.Command{
	Use:   "update <pattern-id>",
	Short: "Update a MIDI pattern",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		midiService := service.NewMIDIService()

		// Prompt for new name
		name, err := prompter.PromptString("New pattern name: ")
		if err != nil {
			return err
		}

		if name == "" {
			fmt.Fprintf(os.Stderr, "Pattern name cannot be empty.\n")
			os.Exit(1)
		}

		// Prompt for description
		description, err := prompter.PromptString("New description (optional): ")
		if err != nil {
			return err
		}

		// Prompt for public flag
		publicStr, err := prompter.PromptString("Make public? (y/n): ")
		if err != nil {
			return err
		}
		isPublic := publicStr == "y" || publicStr == "yes"

		return midiService.UpdateMIDIPattern(args[0], name, description, isPublic)
	},
}

var midiDeleteCmd = &cobra.Command{
	Use:   "delete <pattern-id>",
	Short: "Delete a MIDI pattern",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		midiService := service.NewMIDIService()
		return midiService.DeleteMIDIPattern(args[0])
	},
}

// MIDI Challenge Commands

var (
	challengeStatusFilter string
)

var challengeCmd = &cobra.Command{
	Use:   "challenge",
	Short: "MIDI challenge commands",
	Long:  "Create, manage, and participate in MIDI challenges",
}

var challengeListCmd = &cobra.Command{
	Use:   "list",
	Short: "List MIDI challenges",
	Long:  "List all MIDI challenges with optional status filtering",
	RunE: func(cmd *cobra.Command, args []string) error {
		midiChallengesService := service.NewMIDIChallengesService()
		return midiChallengesService.ListChallenges(challengeStatusFilter)
	},
}

var challengeListActiveCmd = &cobra.Command{
	Use:   "active",
	Short: "List active MIDI challenges",
	RunE: func(cmd *cobra.Command, args []string) error {
		midiChallengesService := service.NewMIDIChallengesService()
		return midiChallengesService.ListChallenges("active")
	},
}

var challengeListVotingCmd = &cobra.Command{
	Use:   "voting",
	Short: "List challenges in voting phase",
	RunE: func(cmd *cobra.Command, args []string) error {
		midiChallengesService := service.NewMIDIChallengesService()
		return midiChallengesService.ListChallenges("voting")
	},
}

var challengeListUpcomingCmd = &cobra.Command{
	Use:   "upcoming",
	Short: "List upcoming MIDI challenges",
	RunE: func(cmd *cobra.Command, args []string) error {
		midiChallengesService := service.NewMIDIChallengesService()
		return midiChallengesService.ListChallenges("upcoming")
	},
}

var challengeViewCmd = &cobra.Command{
	Use:   "view <challenge-id>",
	Short: "View a MIDI challenge",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		midiChallengesService := service.NewMIDIChallengesService()
		return midiChallengesService.ViewChallenge(args[0])
	},
}

var challengeEntriesCmd = &cobra.Command{
	Use:   "entries <challenge-id>",
	Short: "View leaderboard for a challenge",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		midiChallengesService := service.NewMIDIChallengesService()
		return midiChallengesService.ViewLeaderboard(args[0])
	},
}

var challengeSubmitCmd = &cobra.Command{
	Use:   "submit <challenge-id>",
	Short: "Submit an entry to a challenge",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		midiChallengesService := service.NewMIDIChallengesService()
		return midiChallengesService.SubmitEntry(args[0])
	},
}

var challengeVoteCmd = &cobra.Command{
	Use:   "vote <challenge-id>",
	Short: "Vote for a challenge entry",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		midiChallengesService := service.NewMIDIChallengesService()
		return midiChallengesService.VoteForEntry(args[0])
	},
}

func init() {
	// MIDI list flags
	midiListCmd.Flags().IntVar(&midiLimit, "limit", 20, "Results per page")
	midiListCmd.Flags().IntVar(&midiOffset, "offset", 0, "Pagination offset")

	// Challenge list status filter
	challengeListCmd.Flags().StringVar(&challengeStatusFilter, "status", "", "Filter by status (active, voting, upcoming, ended)")

	// Add MIDI pattern subcommands
	midiCmd.AddCommand(midiListCmd)
	midiCmd.AddCommand(midiViewCmd)
	midiCmd.AddCommand(midiUpdateCmd)
	midiCmd.AddCommand(midiDeleteCmd)

	// Add challenge subcommands to challenge parent command
	challengeCmd.AddCommand(challengeListCmd)
	challengeCmd.AddCommand(challengeListActiveCmd)
	challengeCmd.AddCommand(challengeListVotingCmd)
	challengeCmd.AddCommand(challengeListUpcomingCmd)
	challengeCmd.AddCommand(challengeViewCmd)
	challengeCmd.AddCommand(challengeEntriesCmd)
	challengeCmd.AddCommand(challengeSubmitCmd)
	challengeCmd.AddCommand(challengeVoteCmd)

	// Add challenge parent to midi command
	midiCmd.AddCommand(challengeCmd)

	// Add midi command to root
	rootCmd.AddCommand(midiCmd)
}
