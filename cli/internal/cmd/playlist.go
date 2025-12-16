package cmd

import (
	"fmt"
	"os"

	"github.com/spf13/cobra"
	"github.com/zfogg/sidechain/cli/pkg/prompter"
	"github.com/zfogg/sidechain/cli/pkg/service"
)

var (
	playlistPage     int
	playlistPageSize int
	playlistPublic   bool
)

var playlistCmd = &cobra.Command{
	Use:   "playlist",
	Short: "Playlist management commands",
	Long:  "Create, manage, and share playlists",
}

var playlistCreateCmd = &cobra.Command{
	Use:   "create",
	Short: "Create a new playlist",
	RunE: func(cmd *cobra.Command, args []string) error {
		playlistService := service.NewPlaylistService()

		// Prompt for playlist name
		name, err := prompter.PromptString("Playlist name: ")
		if err != nil {
			return err
		}

		if name == "" {
			fmt.Fprintf(os.Stderr, "Playlist name cannot be empty.\n")
			os.Exit(1)
		}

		// Prompt for description
		description, err := prompter.PromptString("Description (optional): ")
		if err != nil {
			return err
		}

		_, err = playlistService.CreatePlaylist(name, description, playlistPublic)
		return err
	},
}

var playlistListCmd = &cobra.Command{
	Use:   "list",
	Short: "List your playlists",
	RunE: func(cmd *cobra.Command, args []string) error {
		playlistService := service.NewPlaylistService()
		return playlistService.ListPlaylists(playlistPage, playlistPageSize)
	},
}

var playlistViewCmd = &cobra.Command{
	Use:   "view <playlist-id>",
	Short: "View playlist details",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		playlistService := service.NewPlaylistService()
		return playlistService.ViewPlaylist(args[0])
	},
}

var playlistDeleteCmd = &cobra.Command{
	Use:   "delete <playlist-id>",
	Short: "Delete a playlist",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		playlistService := service.NewPlaylistService()
		return playlistService.DeletePlaylist(args[0])
	},
}

var playlistAddCmd = &cobra.Command{
	Use:   "add <playlist-id> <post-id>",
	Short: "Add a post to a playlist",
	Args:  cobra.ExactArgs(2),
	RunE: func(cmd *cobra.Command, args []string) error {
		playlistService := service.NewPlaylistService()
		return playlistService.AddPost(args[0], args[1])
	},
}

var playlistRemoveCmd = &cobra.Command{
	Use:   "remove <playlist-id> <entry-id>",
	Short: "Remove a post from a playlist",
	Args:  cobra.ExactArgs(2),
	RunE: func(cmd *cobra.Command, args []string) error {
		playlistService := service.NewPlaylistService()
		return playlistService.RemovePost(args[0], args[1])
	},
}

var playlistContentsCmd = &cobra.Command{
	Use:   "contents <playlist-id>",
	Short: "View playlist contents",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		playlistService := service.NewPlaylistService()
		return playlistService.ListPlaylistEntries(args[0], playlistPage, playlistPageSize)
	},
}

func init() {
	// Create command flags
	playlistCreateCmd.Flags().BoolVar(&playlistPublic, "public", false, "Make playlist public")

	// List/pagination flags
	playlistListCmd.Flags().IntVar(&playlistPage, "page", 1, "Page number")
	playlistListCmd.Flags().IntVar(&playlistPageSize, "page-size", 10, "Results per page")

	playlistContentsCmd.Flags().IntVar(&playlistPage, "page", 1, "Page number")
	playlistContentsCmd.Flags().IntVar(&playlistPageSize, "page-size", 20, "Results per page")

	// Add subcommands
	playlistCmd.AddCommand(playlistCreateCmd)
	playlistCmd.AddCommand(playlistListCmd)
	playlistCmd.AddCommand(playlistViewCmd)
	playlistCmd.AddCommand(playlistDeleteCmd)
	playlistCmd.AddCommand(playlistAddCmd)
	playlistCmd.AddCommand(playlistRemoveCmd)
	playlistCmd.AddCommand(playlistContentsCmd)

	// Add playlist command to root
	rootCmd.AddCommand(playlistCmd)
}
