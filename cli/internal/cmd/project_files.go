package cmd

import (
	"github.com/spf13/cobra"
	"github.com/zfogg/sidechain/cli/pkg/service"
)

var projectCmd = &cobra.Command{
	Use:   "project",
	Short: "Manage project files",
	Long:  "Upload, download, and manage your project files (DAW projects)",
}

var listProjectsCmd = &cobra.Command{
	Use:   "list",
	Short: "List your project files",
	Long:  "Display all your project files with optional DAW filtering",
	RunE: func(cmd *cobra.Command, args []string) error {
		limit, _ := cmd.Flags().GetInt("limit")
		offset, _ := cmd.Flags().GetInt("offset")
		daw, _ := cmd.Flags().GetString("daw")
		svc := service.NewProjectFileService()
		return svc.ListProjectFiles(limit, offset, daw)
	},
}

var viewProjectCmd = &cobra.Command{
	Use:   "view",
	Short: "View project file details",
	Long:  "Display detailed information for a specific project file",
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewProjectFileService()
		return svc.ViewProjectFile()
	},
}

var deleteProjectCmd = &cobra.Command{
	Use:   "delete",
	Short: "Delete a project file",
	Long:  "Delete a project file (requires confirmation)",
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewProjectFileService()
		return svc.DeleteProjectFile()
	},
}

func init() {
	projectCmd.AddCommand(listProjectsCmd)
	projectCmd.AddCommand(viewProjectCmd)
	projectCmd.AddCommand(deleteProjectCmd)

	listProjectsCmd.Flags().IntP("limit", "l", 20, "Number of files per page")
	listProjectsCmd.Flags().IntP("offset", "o", 0, "Offset for pagination")
	listProjectsCmd.Flags().StringP("daw", "d", "", "Filter by DAW type (ableton, fl_studio, logic, pro_tools, cubase, studio_one, reaper, bitwig, other)")
}
