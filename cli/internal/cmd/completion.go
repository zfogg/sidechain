package cmd

import (
	"fmt"
	"os"

	"github.com/spf13/cobra"
)

var completionCmd = &cobra.Command{
	Use:   "completion [bash|zsh|fish|powershell]",
	Short: "Generate shell completion scripts",
	Long: `Generate shell completion scripts for bash, zsh, fish, or powershell.

To load completions in your shell session, run:

Bash:
  source <(sidechain-cli completion bash)

Zsh:
  source <(sidechain-cli completion zsh)

Fish:
  sidechain-cli completion fish | source

PowerShell:
  sidechain-cli completion powershell | Out-String | Invoke-Expression

To load completions for every new session, execute once:

Bash:
  sidechain-cli completion bash > /etc/bash_completion.d/sidechain-cli

Zsh:
  sidechain-cli completion zsh > /usr/local/share/zsh/site-functions/_sidechain-cli

Fish:
  sidechain-cli completion fish > ~/.config/fish/completions/sidechain-cli.fish

PowerShell:
  sidechain-cli completion powershell >> $PROFILE
`,
	ValidArgs: []string{"bash", "zsh", "fish", "powershell"},
	Args:      cobra.MatchAll(cobra.ExactArgs(1), cobra.OnlyValidArgs),
	RunE: func(cmd *cobra.Command, args []string) error {
		switch args[0] {
		case "bash":
			return rootCmd.GenBashCompletion(os.Stdout)
		case "zsh":
			return rootCmd.GenZshCompletion(os.Stdout)
		case "fish":
			return rootCmd.GenFishCompletion(os.Stdout, true)
		case "powershell":
			return rootCmd.GenPowerShellCompletion(os.Stdout)
		}
		return fmt.Errorf("unknown shell: %s", args[0])
	},
}

func init() {
	rootCmd.AddCommand(completionCmd)
}
