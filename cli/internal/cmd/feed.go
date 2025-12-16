package cmd

import (
	"github.com/spf13/cobra"
	"github.com/zfogg/sidechain/cli/pkg/service"
)

var (
	feedPage     int
	feedPageSize int
)

var feedCmd = &cobra.Command{
	Use:   "feed",
	Short: "Feed commands",
	Long:  "View and interact with feeds",
}

var feedTimelineCmd = &cobra.Command{
	Use:   "timeline",
	Short: "View your timeline",
	RunE: func(cmd *cobra.Command, args []string) error {
		feedService := service.NewFeedService()
		return feedService.ViewTimeline(feedPage, feedPageSize)
	},
}

var feedGlobalCmd = &cobra.Command{
	Use:   "global",
	Short: "View global feed",
	RunE: func(cmd *cobra.Command, args []string) error {
		feedService := service.NewFeedService()
		return feedService.ViewGlobalFeed(feedPage, feedPageSize)
	},
}

var feedTrendingCmd = &cobra.Command{
	Use:   "trending",
	Short: "View trending posts",
	RunE: func(cmd *cobra.Command, args []string) error {
		feedService := service.NewFeedService()
		return feedService.ViewTrendingFeed(feedPage, feedPageSize)
	},
}

var feedForYouCmd = &cobra.Command{
	Use:   "for-you",
	Short: "View personalized recommendations",
	RunE: func(cmd *cobra.Command, args []string) error {
		feedService := service.NewFeedService()
		return feedService.ViewForYouFeed(feedPage, feedPageSize)
	},
}

var feedSearchCmd = &cobra.Command{
	Use:   "search <query>",
	Short: "Search for posts",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		feedService := service.NewFeedService()
		return feedService.SearchPosts(args[0], feedPage, feedPageSize)
	},
}

var soundCmd = &cobra.Command{
	Use:   "sound",
	Short: "Sound discovery commands",
}

var soundSearchCmd = &cobra.Command{
	Use:   "search <query>",
	Short: "Search for sounds",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		feedService := service.NewFeedService()
		return feedService.SearchSounds(args[0], feedPage, feedPageSize)
	},
}

var soundTrendingCmd = &cobra.Command{
	Use:   "trending",
	Short: "View trending sounds",
	RunE: func(cmd *cobra.Command, args []string) error {
		feedService := service.NewFeedService()
		return feedService.ViewTrendingSounds(feedPage, feedPageSize)
	},
}

var soundInfoCmd = &cobra.Command{
	Use:   "info <sound-id>",
	Short: "View sound details",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		feedService := service.NewFeedService()
		return feedService.ViewSoundInfo(args[0])
	},
}

var soundPostsCmd = &cobra.Command{
	Use:   "posts <sound-id>",
	Short: "View posts using a sound",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		feedService := service.NewFeedService()
		return feedService.ViewSoundPosts(args[0], feedPage, feedPageSize)
	},
}

var soundFeaturedCmd = &cobra.Command{
	Use:   "featured",
	Short: "Browse featured sounds",
	Long:  "View hand-picked featured sounds from the community",
	RunE: func(cmd *cobra.Command, args []string) error {
		soundService := service.NewSoundPagesService()
		return soundService.BrowseFeaturedSounds(feedPage, feedPageSize)
	},
}

var soundRecentCmd = &cobra.Command{
	Use:   "recent",
	Short: "Browse recent sounds",
	Long:  "View recently uploaded sounds",
	RunE: func(cmd *cobra.Command, args []string) error {
		soundService := service.NewSoundPagesService()
		return soundService.BrowseRecentSounds(feedPage, feedPageSize)
	},
}

var soundUpdateCmd = &cobra.Command{
	Use:   "update",
	Short: "Update sound metadata",
	Long:  "Edit sound name and description",
	RunE: func(cmd *cobra.Command, args []string) error {
		soundService := service.NewSoundPagesService()
		return soundService.UpdateSoundMetadata()
	},
}

var discoverCmd = &cobra.Command{
	Use:   "discover",
	Short: "Discover users and content",
}

var discoverTrendingUsersCmd = &cobra.Command{
	Use:   "trending-users",
	Short: "View trending producers",
	RunE: func(cmd *cobra.Command, args []string) error {
		feedService := service.NewFeedService()
		return feedService.ViewTrendingUsers(feedPage, feedPageSize)
	},
}

var discoverFeaturedCmd = &cobra.Command{
	Use:   "featured",
	Short: "View featured producers",
	RunE: func(cmd *cobra.Command, args []string) error {
		feedService := service.NewFeedService()
		return feedService.ViewFeaturedUsers(feedPage, feedPageSize)
	},
}

var discoverSuggestedCmd = &cobra.Command{
	Use:   "suggested",
	Short: "View suggested users",
	RunE: func(cmd *cobra.Command, args []string) error {
		feedService := service.NewFeedService()
		return feedService.ViewSuggestedUsers(feedPage, feedPageSize)
	},
}

var discoverByGenreCmd = &cobra.Command{
	Use:   "by-genre <genre>",
	Short: "Browse users by genre",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		feedService := service.NewFeedService()
		return feedService.ViewUsersByGenre(args[0], feedPage, feedPageSize)
	},
}

var discoverRecommendationsCmd = &cobra.Command{
	Use:   "recommendations <username>",
	Short: "Get recommended users similar to a user",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		feedService := service.NewFeedService()
		return feedService.ViewRecommendations(args[0], feedPage, feedPageSize)
	},
}

var (
	feedGenreFilter string
	feedMinBPM      int
	feedMaxBPM      int
)

var feedEnrichedCmd = &cobra.Command{
	Use:   "enriched",
	Short: "View enriched timeline with reaction counts",
	Long:  "View your timeline with reaction counts and advanced enrichment",
	RunE: func(cmd *cobra.Command, args []string) error {
		feedService := service.NewFeedService()
		return feedService.ViewEnrichedTimeline(feedPage, feedPageSize)
	},
}

var feedLatestCmd = &cobra.Command{
	Use:   "latest",
	Short: "View recent posts chronologically",
	Long:  "View recently uploaded posts in chronological order",
	RunE: func(cmd *cobra.Command, args []string) error {
		feedService := service.NewFeedService()
		return feedService.ViewLatestFeed(feedPage, feedPageSize)
	},
}

var feedForYouAdvancedCmd = &cobra.Command{
	Use:   "for-you-advanced",
	Short: "View personalized feed with filters",
	Long:  "View personalized recommendations with optional genre and BPM filtering",
	RunE: func(cmd *cobra.Command, args []string) error {
		feedService := service.NewFeedService()
		return feedService.ViewForYouFeedAdvanced(feedPage, feedPageSize, feedGenreFilter, feedMinBPM, feedMaxBPM)
	},
}

func init() {
	// Pagination flags for feed commands
	feedTimelineCmd.Flags().IntVar(&feedPage, "page", 1, "Page number")
	feedTimelineCmd.Flags().IntVar(&feedPageSize, "page-size", 10, "Results per page")

	feedGlobalCmd.Flags().IntVar(&feedPage, "page", 1, "Page number")
	feedGlobalCmd.Flags().IntVar(&feedPageSize, "page-size", 10, "Results per page")

	feedTrendingCmd.Flags().IntVar(&feedPage, "page", 1, "Page number")
	feedTrendingCmd.Flags().IntVar(&feedPageSize, "page-size", 10, "Results per page")

	feedForYouCmd.Flags().IntVar(&feedPage, "page", 1, "Page number")
	feedForYouCmd.Flags().IntVar(&feedPageSize, "page-size", 10, "Results per page")

	feedSearchCmd.Flags().IntVar(&feedPage, "page", 1, "Page number")
	feedSearchCmd.Flags().IntVar(&feedPageSize, "page-size", 10, "Results per page")

	soundSearchCmd.Flags().IntVar(&feedPage, "page", 1, "Page number")
	soundSearchCmd.Flags().IntVar(&feedPageSize, "page-size", 10, "Results per page")

	soundTrendingCmd.Flags().IntVar(&feedPage, "page", 1, "Page number")
	soundTrendingCmd.Flags().IntVar(&feedPageSize, "page-size", 10, "Results per page")

	soundPostsCmd.Flags().IntVar(&feedPage, "page", 1, "Page number")
	soundPostsCmd.Flags().IntVar(&feedPageSize, "page-size", 10, "Results per page")

	soundFeaturedCmd.Flags().IntVar(&feedPage, "page", 1, "Page number")
	soundFeaturedCmd.Flags().IntVar(&feedPageSize, "page-size", 20, "Results per page")

	soundRecentCmd.Flags().IntVar(&feedPage, "page", 1, "Page number")
	soundRecentCmd.Flags().IntVar(&feedPageSize, "page-size", 20, "Results per page")

	discoverTrendingUsersCmd.Flags().IntVar(&feedPage, "page", 1, "Page number")
	discoverTrendingUsersCmd.Flags().IntVar(&feedPageSize, "page-size", 10, "Results per page")

	discoverFeaturedCmd.Flags().IntVar(&feedPage, "page", 1, "Page number")
	discoverFeaturedCmd.Flags().IntVar(&feedPageSize, "page-size", 10, "Results per page")

	discoverSuggestedCmd.Flags().IntVar(&feedPage, "page", 1, "Page number")
	discoverSuggestedCmd.Flags().IntVar(&feedPageSize, "page-size", 10, "Results per page")

	discoverByGenreCmd.Flags().IntVar(&feedPage, "page", 1, "Page number")
	discoverByGenreCmd.Flags().IntVar(&feedPageSize, "page-size", 10, "Results per page")

	discoverRecommendationsCmd.Flags().IntVar(&feedPage, "page", 1, "Page number")
	discoverRecommendationsCmd.Flags().IntVar(&feedPageSize, "page-size", 10, "Results per page")

	// Enriched feed flags
	feedEnrichedCmd.Flags().IntVar(&feedPage, "page", 1, "Page number")
	feedEnrichedCmd.Flags().IntVar(&feedPageSize, "page-size", 10, "Results per page")

	feedLatestCmd.Flags().IntVar(&feedPage, "page", 1, "Page number")
	feedLatestCmd.Flags().IntVar(&feedPageSize, "page-size", 10, "Results per page")

	feedForYouAdvancedCmd.Flags().IntVar(&feedPage, "page", 1, "Page number")
	feedForYouAdvancedCmd.Flags().IntVar(&feedPageSize, "page-size", 10, "Results per page")
	feedForYouAdvancedCmd.Flags().StringVar(&feedGenreFilter, "genre", "", "Filter by genre (e.g., 'electronic', 'hip-hop')")
	feedForYouAdvancedCmd.Flags().IntVar(&feedMinBPM, "min-bpm", 0, "Minimum BPM filter")
	feedForYouAdvancedCmd.Flags().IntVar(&feedMaxBPM, "max-bpm", 0, "Maximum BPM filter")

	// Add subcommands
	feedCmd.AddCommand(feedTimelineCmd)
	feedCmd.AddCommand(feedGlobalCmd)
	feedCmd.AddCommand(feedTrendingCmd)
	feedCmd.AddCommand(feedForYouCmd)
	feedCmd.AddCommand(feedEnrichedCmd)
	feedCmd.AddCommand(feedLatestCmd)
	feedCmd.AddCommand(feedForYouAdvancedCmd)
	feedCmd.AddCommand(feedSearchCmd)
	feedCmd.AddCommand(soundCmd)
	feedCmd.AddCommand(discoverCmd)

	soundCmd.AddCommand(soundSearchCmd)
	soundCmd.AddCommand(soundTrendingCmd)
	soundCmd.AddCommand(soundInfoCmd)
	soundCmd.AddCommand(soundPostsCmd)
	soundCmd.AddCommand(soundFeaturedCmd)
	soundCmd.AddCommand(soundRecentCmd)
	soundCmd.AddCommand(soundUpdateCmd)

	discoverCmd.AddCommand(discoverTrendingUsersCmd)
	discoverCmd.AddCommand(discoverFeaturedCmd)
	discoverCmd.AddCommand(discoverSuggestedCmd)
	discoverCmd.AddCommand(discoverByGenreCmd)
	discoverCmd.AddCommand(discoverRecommendationsCmd)
}
