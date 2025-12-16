package service

import (
	"fmt"

	"github.com/zfogg/sidechain/cli/pkg/api"
	"github.com/zfogg/sidechain/cli/pkg/logger"
)

// FeedService provides feed-related operations
type FeedService struct{}

// NewFeedService creates a new feed service
func NewFeedService() *FeedService {
	return &FeedService{}
}

// ViewTimeline displays the user's timeline feed
func (fs *FeedService) ViewTimeline(page, pageSize int) error {
	logger.Debug("Viewing timeline", "page", page)

	feed, err := api.GetFeed("timeline", page, pageSize)
	if err != nil {
		return fmt.Errorf("failed to fetch timeline: %w", err)
	}

	if len(feed.Posts) == 0 {
		fmt.Println("No posts in your timeline.")
		return nil
	}

	fs.displayFeed("Your Timeline", feed)
	return nil
}

// ViewGlobalFeed displays the global feed
func (fs *FeedService) ViewGlobalFeed(page, pageSize int) error {
	logger.Debug("Viewing global feed", "page", page)

	feed, err := api.GetFeed("global", page, pageSize)
	if err != nil {
		return fmt.Errorf("failed to fetch global feed: %w", err)
	}

	if len(feed.Posts) == 0 {
		fmt.Println("No posts available.")
		return nil
	}

	fs.displayFeed("Global Feed", feed)
	return nil
}

// ViewTrendingFeed displays trending posts
func (fs *FeedService) ViewTrendingFeed(page, pageSize int) error {
	logger.Debug("Viewing trending feed", "page", page)

	feed, err := api.GetFeed("trending", page, pageSize)
	if err != nil {
		return fmt.Errorf("failed to fetch trending feed: %w", err)
	}

	if len(feed.Posts) == 0 {
		fmt.Println("No trending posts available.")
		return nil
	}

	fs.displayFeed("Trending Posts", feed)
	return nil
}

// ViewForYouFeed displays personalized recommendations
func (fs *FeedService) ViewForYouFeed(page, pageSize int) error {
	logger.Debug("Viewing for-you feed", "page", page)

	feed, err := api.GetFeed("for-you", page, pageSize)
	if err != nil {
		return fmt.Errorf("failed to fetch for-you feed: %w", err)
	}

	if len(feed.Posts) == 0 {
		fmt.Println("No recommendations available yet.")
		return nil
	}

	fs.displayFeed("Recommended For You", feed)
	return nil
}

// SearchPosts searches for posts
func (fs *FeedService) SearchPosts(query string, page, pageSize int) error {
	logger.Debug("Searching posts", "query", query)

	results, err := api.SearchPosts(query, page, pageSize)
	if err != nil {
		return fmt.Errorf("failed to search posts: %w", err)
	}

	if len(results.Posts) == 0 {
		fmt.Printf("No posts found for \"%s\"\n", query)
		return nil
	}

	fmt.Printf("Search Results for \"%s\":\n\n", query)
	displayPostList(results)
	return nil
}

// SearchSounds searches for sounds
func (fs *FeedService) SearchSounds(query string, page, pageSize int) error {
	logger.Debug("Searching sounds", "query", query)

	results, err := api.SearchSounds(query, page, pageSize)
	if err != nil {
		return fmt.Errorf("failed to search sounds: %w", err)
	}

	if len(results.Sounds) == 0 {
		fmt.Printf("No sounds found for \"%s\"\n", query)
		return nil
	}

	fs.displaySounds("Search Results", results)
	return nil
}

// ViewTrendingSounds displays trending sounds
func (fs *FeedService) ViewTrendingSounds(page, pageSize int) error {
	logger.Debug("Viewing trending sounds")

	sounds, err := api.GetTrendingSounds(page, pageSize)
	if err != nil {
		return fmt.Errorf("failed to fetch trending sounds: %w", err)
	}

	if len(sounds.Sounds) == 0 {
		fmt.Println("No trending sounds available.")
		return nil
	}

	fs.displaySounds("Trending Sounds", sounds)
	return nil
}

// ViewSoundInfo displays details about a sound
func (fs *FeedService) ViewSoundInfo(soundID string) error {
	logger.Debug("Viewing sound info", "sound_id", soundID)

	sound, err := api.GetSoundInfo(soundID)
	if err != nil {
		return fmt.Errorf("failed to fetch sound info: %w", err)
	}

	fs.displaySoundDetail(sound)
	return nil
}

// ViewSoundPosts displays posts using a specific sound
func (fs *FeedService) ViewSoundPosts(soundID string, page, pageSize int) error {
	logger.Debug("Viewing posts for sound", "sound_id", soundID)

	posts, err := api.GetSoundPosts(soundID, page, pageSize)
	if err != nil {
		return fmt.Errorf("failed to fetch sound posts: %w", err)
	}

	if len(posts.Posts) == 0 {
		fmt.Println("No posts using this sound.")
		return nil
	}

	fmt.Printf("Posts Using This Sound:\n\n")
	displayPostList(posts)
	return nil
}

// ViewTrendingUsers displays trending producers
func (fs *FeedService) ViewTrendingUsers(page, pageSize int) error {
	logger.Debug("Viewing trending users")

	users, err := api.GetTrendingUsers(page, pageSize)
	if err != nil {
		return fmt.Errorf("failed to fetch trending users: %w", err)
	}

	if len(users.Users) == 0 {
		fmt.Println("No trending producers available.")
		return nil
	}

	fs.displayUsers("Trending Producers", users)
	return nil
}

// ViewFeaturedUsers displays featured producers
func (fs *FeedService) ViewFeaturedUsers(page, pageSize int) error {
	logger.Debug("Viewing featured users")

	users, err := api.GetFeaturedUsers(page, pageSize)
	if err != nil {
		return fmt.Errorf("failed to fetch featured users: %w", err)
	}

	if len(users.Users) == 0 {
		fmt.Println("No featured producers available.")
		return nil
	}

	fs.displayUsers("Featured Producers", users)
	return nil
}

// ViewSuggestedUsers displays suggested users to follow
func (fs *FeedService) ViewSuggestedUsers(page, pageSize int) error {
	logger.Debug("Viewing suggested users")

	users, err := api.GetSuggestedUsers(page, pageSize)
	if err != nil {
		return fmt.Errorf("failed to fetch suggested users: %w", err)
	}

	if len(users.Users) == 0 {
		fmt.Println("No suggested users available.")
		return nil
	}

	fs.displayUsers("Suggested For You", users)
	return nil
}

// ViewUsersByGenre displays users by genre
func (fs *FeedService) ViewUsersByGenre(genre string, page, pageSize int) error {
	logger.Debug("Viewing users by genre", "genre", genre)

	users, err := api.GetUsersByGenre(genre, page, pageSize)
	if err != nil {
		return fmt.Errorf("failed to fetch users by genre: %w", err)
	}

	if len(users.Users) == 0 {
		fmt.Printf("No producers found in genre \"%s\"\n", genre)
		return nil
	}

	fs.displayUsers(fmt.Sprintf("Producers in %s", genre), users)
	return nil
}

// ViewRecommendations displays recommended users similar to a given user
func (fs *FeedService) ViewRecommendations(username string, page, pageSize int) error {
	logger.Debug("Viewing recommendations", "username", username)

	users, err := api.GetRecommendations(username, page, pageSize)
	if err != nil {
		return fmt.Errorf("failed to fetch recommendations: %w", err)
	}

	if len(users.Users) == 0 {
		fmt.Printf("No recommendations found for @%s\n", username)
		return nil
	}

	fs.displayUsers(fmt.Sprintf("Similar to @%s", username), users)
	return nil
}

// ViewEnrichedTimeline displays timeline with reaction counts and enrichment
func (fs *FeedService) ViewEnrichedTimeline(page, pageSize int) error {
	logger.Debug("Viewing enriched timeline", "page", page)

	feed, err := api.GetEnrichedTimeline(page, pageSize)
	if err != nil {
		return fmt.Errorf("failed to fetch enriched timeline: %w", err)
	}

	if len(feed.Posts) == 0 {
		fmt.Println("No posts in your enriched timeline")
		return nil
	}

	fs.displayFeed("✨ Enriched Timeline", feed)
	return nil
}

// ViewLatestFeed displays recent posts in chronological order
func (fs *FeedService) ViewLatestFeed(page, pageSize int) error {
	logger.Debug("Viewing latest feed", "page", page)

	feed, err := api.GetLatestFeed(page, pageSize)
	if err != nil {
		return fmt.Errorf("failed to fetch latest feed: %w", err)
	}

	if len(feed.Posts) == 0 {
		fmt.Println("No recent posts found")
		return nil
	}

	fs.displayFeed("📅 Latest Posts", feed)
	return nil
}

// ViewForYouFeedAdvanced displays personalized feed with optional filtering
func (fs *FeedService) ViewForYouFeedAdvanced(page, pageSize int, genre string, minBPM, maxBPM int) error {
	logger.Debug("Viewing for-you feed with filters", "page", page, "genre", genre, "min_bpm", minBPM, "max_bpm", maxBPM)

	feed, err := api.GetForYouFeedWithFilters(page, pageSize, genre, minBPM, maxBPM)
	if err != nil {
		return fmt.Errorf("failed to fetch for-you feed: %w", err)
	}

	if len(feed.Posts) == 0 {
		fmt.Println("No posts found matching your criteria")
		return nil
	}

	title := "🎯 For You"
	if genre != "" || minBPM > 0 || maxBPM > 0 {
		title += " (Filtered"
		if genre != "" {
			title += fmt.Sprintf(": %s", genre)
		}
		if minBPM > 0 || maxBPM > 0 {
			if genre != "" {
				title += " |"
			}
			if minBPM > 0 && maxBPM > 0 {
				title += fmt.Sprintf(" BPM: %d-%d", minBPM, maxBPM)
			} else if minBPM > 0 {
				title += fmt.Sprintf(" BPM: %d+", minBPM)
			} else {
				title += fmt.Sprintf(" BPM: up to %d", maxBPM)
			}
		}
		title += ")"
	}

	fs.displayFeed(title, feed)
	return nil
}

// Helper display functions

func (fs *FeedService) displayFeed(title string, feed *api.FeedResponse) {
	fmt.Printf("\n📰 %s\n", title)
	fmt.Println(string(make([]byte, len(title)+4)))
	displayPostList(&api.PostListResponse{
		Posts:      feed.Posts,
		TotalCount: feed.TotalCount,
		Page:       feed.Page,
		PageSize:   feed.PageSize,
	})
}

func (fs *FeedService) displaySounds(title string, sounds *api.SoundSearchResponse) {
	fmt.Printf("\n🎵 %s\n", title)
	fmt.Println(string(make([]byte, len(title)+4)))

	for i, sound := range sounds.Sounds {
		fmt.Printf("%d. %s\n", i+1, sound.Name)
		fmt.Printf("   BPM: %d | Key: %s | Duration: %ds\n", sound.BPM, sound.Key, sound.Duration)
		fmt.Printf("   ▶️ %d plays | ⬇️ %d downloads\n", sound.PlayCount, sound.DownloadCount)
		if len(sound.Genre) > 0 {
			fmt.Printf("   Genre: %v\n", sound.Genre)
		}
		fmt.Printf("\n")
	}

	fmt.Printf("Showing %d of %d sounds (Page %d)\n\n", len(sounds.Sounds), sounds.TotalCount, sounds.Page)
}

func (fs *FeedService) displaySoundDetail(sound *api.Sound) {
	fmt.Printf("\n🎵 Sound Details\n")
	fmt.Printf("Name:        %s\n", sound.Name)
	if sound.Description != "" {
		fmt.Printf("Description: %s\n", sound.Description)
	}
	fmt.Printf("Duration:    %d seconds\n", sound.Duration)
	fmt.Printf("BPM:         %d\n", sound.BPM)
	fmt.Printf("Key:         %s\n", sound.Key)
	if len(sound.Genre) > 0 {
		fmt.Printf("Genre:       %v\n", sound.Genre)
	}
	fmt.Printf("Stats:\n")
	fmt.Printf("  ▶️  Plays:     %d\n", sound.PlayCount)
	fmt.Printf("  ⬇️  Downloads: %d\n", sound.DownloadCount)
	fmt.Printf("Created: %s\n\n", sound.CreatedAt.Format("2006-01-02 15:04:05"))
}

func (fs *FeedService) displayUsers(title string, users *api.UserListResponse) {
	fmt.Printf("\n👥 %s\n", title)
	fmt.Println(string(make([]byte, len(title)+4)))

	for i, user := range users.Users {
		fmt.Printf("%d. @%s\n", i+1, user.Username)
		fmt.Printf("   %s\n", user.DisplayName)
		if user.Bio != "" {
			fmt.Printf("   Bio: %s\n", user.Bio)
		}
		fmt.Printf("   👥 %d followers | 📝 %d posts\n", user.FollowerCount, user.PostCount)
		fmt.Printf("\n")
	}

	fmt.Printf("Showing %d of %d users (Page %d)\n\n", len(users.Users), users.TotalCount, users.Page)
}

// Helper function to display posts (reuse from post service)
func displayPostList(posts *api.PostListResponse) {
	for i, post := range posts.Posts {
		fmt.Printf("%d. [%s]\n", i+1, post.ID)
		fmt.Printf("   Duration: %ds | ❤️ %d | 💬 %d | 🔄 %d\n",
			post.Duration, post.LikeCount, post.CommentCount, post.RepostCount)
		fmt.Printf("   Created: %s\n", post.CreatedAt.Format("2006-01-02 15:04:05"))
		if post.IsPinned {
			fmt.Printf("   📌 Pinned\n")
		}
		if post.IsArchived {
			fmt.Printf("   📂 Archived\n")
		}
		fmt.Printf("\n")
	}

	fmt.Printf("Showing %d of %d posts (Page %d)\n\n", len(posts.Posts), posts.TotalCount, posts.Page)
}
