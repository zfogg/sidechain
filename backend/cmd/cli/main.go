package main

import (
	"fmt"
	"os"

	"github.com/spf13/cobra"
)

var (
	authToken string
	apiURL    string = "http://localhost:8787"
	output    string = "text" // "text" or "json"
)

var rootCmd = &cobra.Command{
	Use:   "sidechain",
	Short: "Sidechain CLI - Manage your Sidechain profile and account",
	Long: `Sidechain CLI provides command-line access to your Sidechain account.
Manage your profile privacy settings, follow requests, and more.`,
	PersistentPreRun: func(cmd *cobra.Command, args []string) {
		if authToken == "" {
			authToken = os.Getenv("SIDECHAIN_TOKEN")
		}
		if authToken == "" && cmd.Name() != "help" && cmd.Parent() != nil {
			fmt.Fprintf(os.Stderr, "Error: SIDECHAIN_TOKEN environment variable not set\n")
			fmt.Fprintf(os.Stderr, "Please set your auth token: export SIDECHAIN_TOKEN=<your-token>\n")
			os.Exit(1)
		}
	},
}

func init() {
	rootCmd.PersistentFlags().StringVar(&authToken, "token", "", "Authentication token (defaults to SIDECHAIN_TOKEN env var)")
	rootCmd.PersistentFlags().StringVar(&apiURL, "api", apiURL, "API server URL")
	rootCmd.PersistentFlags().StringVar(&output, "output", output, "Output format: text or json")

	// Add command groups
	rootCmd.AddCommand(profileCmd)
	rootCmd.AddCommand(followRequestsCmd)
}

func main() {
	if err := rootCmd.Execute(); err != nil {
		fmt.Fprintf(os.Stderr, "Error: %v\n", err)
		os.Exit(1)
	}
}
