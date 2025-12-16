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

var challengeListCmd = &cobra.Command{
	Use:   "challenge list",
	Short: "List MIDI challenges",
	RunE: func(cmd *cobra.Command, args []string) error {
		challengeService := service.NewChallengeService()
		return challengeService.ListChallenges("")
	},
}

var challengeListActiveCmd = &cobra.Command{
	Use:   "challenge active",
	Short: "List active MIDI challenges",
	RunE: func(cmd *cobra.Command, args []string) error {
		challengeService := service.NewChallengeService()
		return challengeService.ListChallenges("active")
	},
}

var challengeListVotingCmd = &cobra.Command{
	Use:   "challenge voting",
	Short: "List challenges in voting phase",
	RunE: func(cmd *cobra.Command, args []string) error {
		challengeService := service.NewChallengeService()
		return challengeService.ListChallenges("voting")
	},
}

var challengeListUpcomingCmd = &cobra.Command{
	Use:   "challenge upcoming",
	Short: "List upcoming MIDI challenges",
	RunE: func(cmd *cobra.Command, args []string) error {
		challengeService := service.NewChallengeService()
		return challengeService.ListChallenges("upcoming")
	},
}

var challengeViewCmd = &cobra.Command{
	Use:   "challenge view <challenge-id>",
	Short: "View a MIDI challenge",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		challengeService := service.NewChallengeService()
		return challengeService.ViewChallenge(args[0])
	},
}

var challengeEntriesCmd = &cobra.Command{
	Use:   "challenge entries <challenge-id>",
	Short: "View entries for a challenge",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		challengeService := service.NewChallengeService()
		return challengeService.ViewChallengeEntries(args[0])
	},
}

var challengeSubmitCmd = &cobra.Command{
	Use:   "challenge submit <challenge-id> <audio-url>",
	Short: "Submit an entry to a challenge",
	Args:  cobra.ExactArgs(2),
	RunE: func(cmd *cobra.Command, args []string) error {
		challengeService := service.NewChallengeService()
		return challengeService.SubmitEntry(args[0], args[1], nil, nil)
	},
}

var challengeVoteCmd = &cobra.Command{
	Use:   "challenge vote <challenge-id> <entry-id>",
	Short: "Vote for a challenge entry",
	Args:  cobra.ExactArgs(2),
	RunE: func(cmd *cobra.Command, args []string) error {
		challengeService := service.NewChallengeService()
		return challengeService.VoteEntry(args[0], args[1])
	},
}

func init() {
	// MIDI list flags
	midiListCmd.Flags().IntVar(&midiLimit, "limit", 20, "Results per page")
	midiListCmd.Flags().IntVar(&midiOffset, "offset", 0, "Pagination offset")

	// Add MIDI pattern subcommands
	midiCmd.AddCommand(midiListCmd)
	midiCmd.AddCommand(midiViewCmd)
	midiCmd.AddCommand(midiUpdateCmd)
	midiCmd.AddCommand(midiDeleteCmd)

	// Add challenge subcommands
	midiCmd.AddCommand(challengeListCmd)
	midiCmd.AddCommand(challengeListActiveCmd)
	midiCmd.AddCommand(challengeListVotingCmd)
	midiCmd.AddCommand(challengeListUpcomingCmd)
	midiCmd.AddCommand(challengeViewCmd)
	midiCmd.AddCommand(challengeEntriesCmd)
	midiCmd.AddCommand(challengeSubmitCmd)
	midiCmd.AddCommand(challengeVoteCmd)

	// Add midi command to root
	rootCmd.AddCommand(midiCmd)
}
