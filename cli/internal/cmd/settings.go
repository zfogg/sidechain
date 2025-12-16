package cmd

import (
	"github.com/spf13/cobra"
)

var settingsCmd = &cobra.Command{
	Use:   "settings",
	Short: "Manage user settings",
	Long:  "Configure user preferences and account settings",
}

func init() {
	// Settings subcommands will be added here
}
