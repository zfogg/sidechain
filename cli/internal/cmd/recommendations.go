package cmd

import (
	"github.com/spf13/cobra"
	"github.com/zfogg/sidechain/cli/pkg/service"
)

var exploreCmd = &cobra.Command{
	Use:   "explore",
	Short: "Explore and browse content",
	Long:  "Browse popular content, latest posts, and personalized recommendations",
}

var explorePopularCmd = &cobra.Command{
	Use:   "popular",
	Short: "Browse popular content",
	Long:  "Explore trending posts, sounds, and producers",
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewRecommendationsService()
		return svc.BrowsePopular()
	},
}

var exploreLatestCmd = &cobra.Command{
	Use:   "latest",
	Short: "Browse latest posts",
	Long:  "View the most recently posted content",
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewRecommendationsService()
		return svc.BrowseLatest()
	},
}

var exploreForYouCmd = &cobra.Command{
	Use:   "for-you",
	Short: "View personalized feed",
	Long:  "Browse content tailored to your interests and follows",
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewRecommendationsService()
		return svc.BrowseForYou()
	},
}

var exploreGenreCmd = &cobra.Command{
	Use:   "genre",
	Short: "Explore by genre",
	Long:  "Discover producers and content in specific music genres",
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewRecommendationsService()
		return svc.DiscoverByGenre()
	},
}

var exploreSimilarCmd = &cobra.Command{
	Use:   "similar",
	Short: "Find similar producers",
	Long:  "Discover producers similar to someone you follow",
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewRecommendationsService()
		return svc.DiscoverSimilar()
	},
}

var exploreFeaturedCmd = &cobra.Command{
	Use:   "featured",
	Short: "View featured producers",
	Long:  "Browse hand-picked featured creators to follow",
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewRecommendationsService()
		return svc.DiscoverFeatured()
	},
}

var exploreSuggestedCmd = &cobra.Command{
	Use:   "suggested",
	Short: "View suggested producers",
	Long:  "See producer suggestions tailored to your interests",
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewRecommendationsService()
		return svc.DiscoverSuggested()
	},
}

func init() {
	exploreCmd.AddCommand(explorePopularCmd)
	exploreCmd.AddCommand(exploreLatestCmd)
	exploreCmd.AddCommand(exploreForYouCmd)
	exploreCmd.AddCommand(exploreGenreCmd)
	exploreCmd.AddCommand(exploreSimilarCmd)
	exploreCmd.AddCommand(exploreFeaturedCmd)
	exploreCmd.AddCommand(exploreSuggestedCmd)
}
