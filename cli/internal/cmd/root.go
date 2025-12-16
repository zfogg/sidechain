package cmd

import (
	"fmt"
	"os"

	"github.com/spf13/cobra"
	"github.com/zfogg/sidechain/cli/pkg/client"
	"github.com/zfogg/sidechain/cli/pkg/config"
	"github.com/zfogg/sidechain/cli/pkg/credentials"
	"github.com/zfogg/sidechain/cli/pkg/logger"
)

var (
	verbose    bool
	configPath string
	outputFmt  string
	asUser     string
)

var rootCmd = &cobra.Command{
	Use:   "sidechain-cli",
	Short: "Sidechain CLI - Social music production platform",
	Long: `Sidechain CLI is a command-line interface for the Sidechain
social music production platform. Share loops, collaborate, and
manage your music production community directly from the terminal.`,
	PersistentPreRun: func(cmd *cobra.Command, args []string) {
		// Initialize config and logger
		if err := config.Init(configPath); err != nil {
			fmt.Fprintf(os.Stderr, "Error initializing config: %v\n", err)
			os.Exit(1)
		}

		logger.Init(verbose)

		// Save output format to config
		config.SetString("output.format", outputFmt)

		// Validate impersonation usage
		if asUser != "" {
			// Load credentials to check if user is admin
			creds, err := credentials.Load()
			if err != nil {
				fmt.Fprintf(os.Stderr, "Error loading credentials: %v\n", err)
				os.Exit(1)
			}

			if creds == nil {
				fmt.Fprintf(os.Stderr, "Error: You must be logged in to use --as-user\n")
				os.Exit(1)
			}

			if !creds.IsAdmin {
				fmt.Fprintf(os.Stderr, "Error: Only admin users can impersonate other users\n")
				os.Exit(1)
			}

			// Set the impersonation user in the client
			client.SetImpersonateUser(asUser)
		}
	},
}

func Execute() {
	if err := rootCmd.Execute(); err != nil {
		fmt.Fprintf(os.Stderr, "Error: %v\n", err)
		os.Exit(1)
	}
}

func init() {
	rootCmd.PersistentFlags().BoolVarP(&verbose, "verbose", "v", false, "Enable verbose output")
	rootCmd.PersistentFlags().StringVar(&configPath, "config", "", "Path to config file (default: ~/.sidechain/config.yaml)")
	rootCmd.PersistentFlags().StringVar(&outputFmt, "output", "text", "Output format: text, json, table")
	rootCmd.PersistentFlags().StringVar(&asUser, "as-user", "", "Admin impersonation: run commands as another user (requires admin account)")

	// Add subcommands
	rootCmd.AddCommand(authCmd)
	rootCmd.AddCommand(profileCmd)
	rootCmd.AddCommand(postCmd)
	rootCmd.AddCommand(postActionsCmd)
	rootCmd.AddCommand(commentCmd)
	rootCmd.AddCommand(feedCmd)
	rootCmd.AddCommand(notificationsCmd)
	rootCmd.AddCommand(reportCmd)
	rootCmd.AddCommand(muteCmd)
	rootCmd.AddCommand(errorCmd)
	rootCmd.AddCommand(exploreCmd)
	rootCmd.AddCommand(discoverCmd)
	rootCmd.AddCommand(recommendCmd)
	rootCmd.AddCommand(projectCmd)
	rootCmd.AddCommand(searchCmd)
	rootCmd.AddCommand(followCmd)
	rootCmd.AddCommand(settingsCmd)
	rootCmd.AddCommand(adminCmd)
	rootCmd.AddCommand(versionCmd)
}
