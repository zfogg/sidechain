package cmd

import (
	"github.com/spf13/cobra"
	"github.com/zfogg/sidechain/cli/pkg/service"
)

var authCmd = &cobra.Command{
	Use:   "auth",
	Short: "Authentication commands",
	Long:  "Manage authentication with Sidechain",
}

var registerCmd = &cobra.Command{
	Use:   "register",
	Short: "Create a new Sidechain account",
	Long:  "Register a new user account with Sidechain",
	RunE: func(cmd *cobra.Command, args []string) error {
		authSvc := service.NewAuthService()
		return authSvc.Register()
	},
}

var loginCmd = &cobra.Command{
	Use:   "login",
	Short: "Login to Sidechain",
	Long:  "Authenticate with Sidechain using email/password or OAuth",
	RunE: func(cmd *cobra.Command, args []string) error {
		return handleLogin()
	},
}

var logoutCmd = &cobra.Command{
	Use:   "logout",
	Short: "Logout from Sidechain",
	RunE: func(cmd *cobra.Command, args []string) error {
		authSvc := service.NewAuthService()
		return authSvc.Logout()
	},
}

var meCmd = &cobra.Command{
	Use:   "me",
	Short: "Display current authenticated user",
	RunE: func(cmd *cobra.Command, args []string) error {
		authSvc := service.NewAuthService()
		return authSvc.GetMe()
	},
}

var resetPasswordCmd = &cobra.Command{
	Use:   "reset-password",
	Short: "Request a password reset",
	RunE: func(cmd *cobra.Command, args []string) error {
		authSvc := service.NewAuthService()
		return authSvc.RequestPasswordReset()
	},
}

var resetPasswordConfirmCmd = &cobra.Command{
	Use:   "confirm-reset",
	Short: "Confirm password reset with token",
	RunE: func(cmd *cobra.Command, args []string) error {
		authSvc := service.NewAuthService()
		return authSvc.ResetPassword()
	},
}

var twoFactorCmd = &cobra.Command{
	Use:   "2fa",
	Short: "Two-factor authentication",
}

var enable2faCmd = &cobra.Command{
	Use:   "enable",
	Short: "Enable two-factor authentication",
	RunE: func(cmd *cobra.Command, args []string) error {
		authSvc := service.NewAuthService()
		return authSvc.Enable2FA()
	},
}

var disable2faCmd = &cobra.Command{
	Use:   "disable",
	Short: "Disable two-factor authentication",
	RunE: func(cmd *cobra.Command, args []string) error {
		authSvc := service.NewAuthService()
		return authSvc.Disable2FA()
	},
}

var status2faCmd = &cobra.Command{
	Use:   "status",
	Short: "Show two-factor authentication status",
	RunE: func(cmd *cobra.Command, args []string) error {
		authSvc := service.NewAuthService()
		return authSvc.Get2FAStatus()
	},
}

var backup2faCmd = &cobra.Command{
	Use:   "backup-codes",
	Short: "Regenerate two-factor backup codes",
	RunE: func(cmd *cobra.Command, args []string) error {
		authSvc := service.NewAuthService()
		return authSvc.GetBackupCodes()
	},
}

var refreshCmd = &cobra.Command{
	Use:   "refresh",
	Short: "Refresh authentication token",
	RunE: func(cmd *cobra.Command, args []string) error {
		authSvc := service.NewAuthService()
		err := authSvc.RefreshToken()
		if err == nil {
			return nil
		}
		return err
	},
}

func init() {
	authCmd.AddCommand(registerCmd)
	authCmd.AddCommand(loginCmd)
	authCmd.AddCommand(logoutCmd)
	authCmd.AddCommand(meCmd)
	authCmd.AddCommand(resetPasswordCmd)
	authCmd.AddCommand(resetPasswordConfirmCmd)
	authCmd.AddCommand(refreshCmd)
	authCmd.AddCommand(twoFactorCmd)

	twoFactorCmd.AddCommand(status2faCmd)
	twoFactorCmd.AddCommand(enable2faCmd)
	twoFactorCmd.AddCommand(disable2faCmd)
	twoFactorCmd.AddCommand(backup2faCmd)
}

// Handler functions
func handleLogin() error {
	authSvc := service.NewAuthService()
	return authSvc.Login()
}
