package cmd

import (
	"github.com/spf13/cobra"
	"github.com/zfogg/sidechain/cli/pkg/service"
)

var searchCmd = &cobra.Command{
	Use:   "search",
	Short: "Search and discover producers",
	Long:  "Search for producers, discover trending users, and explore by genre",
}

var searchUsersCmd = &cobra.Command{
	Use:   "users",
	Short: "Search for producers by username or name",
	Long:  "Search for producers using username or display name",
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewSearchService()
		return svc.SearchUsers()
	},
}

var trendingCmd = &cobra.Command{
	Use:   "trending",
	Short: "View trending producers",
	Long:  "View trending producers (day/week/month)",
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewSearchService()
		return svc.GetTrendingUsers()
	},
}

var featuredCmd = &cobra.Command{
	Use:   "featured",
	Short: "View featured producers",
	Long:  "View curated featured producers",
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewSearchService()
		return svc.GetFeaturedProducers()
	},
}

var suggestedCmd = &cobra.Command{
	Use:   "suggested",
	Short: "Get personalized suggestions",
	Long:  "Get personalized producer suggestions based on your interests",
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewSearchService()
		return svc.GetSuggestedUsers()
	},
}

var genresCmd = &cobra.Command{
	Use:   "genres",
	Short: "List available genres",
	Long:  "Display all available music genres",
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewSearchService()
		return svc.ListGenres()
	},
}

var genreCmd = &cobra.Command{
	Use:   "genre",
	Short: "Find producers by genre",
	Long:  "Search for producers in a specific genre",
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewSearchService()
		return svc.GetUsersByGenre()
	},
}

var similarCmd = &cobra.Command{
	Use:   "similar",
	Short: "Find similar producers",
	Long:  "Find producers with similar music taste",
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewSearchService()
		return svc.GetSimilarUsers()
	},
}

func init() {
	searchCmd.AddCommand(searchUsersCmd)
	searchCmd.AddCommand(trendingCmd)
	searchCmd.AddCommand(featuredCmd)
	searchCmd.AddCommand(suggestedCmd)
	searchCmd.AddCommand(genresCmd)
	searchCmd.AddCommand(genreCmd)
	searchCmd.AddCommand(similarCmd)
}
