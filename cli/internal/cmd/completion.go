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
  source <(sidechain completion bash)

Zsh:
  source <(sidechain completion zsh)

Fish:
  sidechain completion fish | source

PowerShell:
  sidechain completion powershell | Out-String | Invoke-Expression

To load completions for every new session, execute once:

Bash:
  sidechain completion bash > /etc/bash_completion.d/sidechain

Zsh:
  sidechain completion zsh > /usr/local/share/zsh/site-functions/_sidechain

Fish:
  sidechain completion fish > ~/.config/fish/completions/sidechain.fish

PowerShell:
  sidechain completion powershell >> $PROFILE
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
