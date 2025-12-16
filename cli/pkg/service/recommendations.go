package service

import (
	"bufio"
	"fmt"
	"os"
	"strings"

	"github.com/zfogg/sidechain/cli/pkg/api"
	"github.com/zfogg/sidechain/cli/pkg/formatter"
)

// RecommendationsService provides recommendation and discovery operations
type RecommendationsService struct{}

// NewRecommendationsService creates a new recommendations service
func NewRecommendationsService() *RecommendationsService {
	return &RecommendationsService{}
}

// BrowsePopular displays popular posts and sounds
func (rs *RecommendationsService) BrowsePopular() error {
	reader := bufio.NewReader(os.Stdin)

	fmt.Print("What to explore? (posts/sounds/users) [posts]: ")
	contentType, _ := reader.ReadString('\n')
	contentType = strings.TrimSpace(strings.ToLower(contentType))
	if contentType == "" {
		contentType = "posts"
	}

	switch contentType {
	case "posts":
		return rs.displayPopularPosts()
	case "sounds":
		return rs.displayPopularSounds()
	case "users":
		return rs.displayPopularUsers()
	default:
		return fmt.Errorf("invalid content type: %s", contentType)
	}
}

// BrowseLatest displays latest posts
func (rs *RecommendationsService) BrowseLatest() error {
	posts, err := api.GetLatestPosts(1, 20)
	if err != nil {
		return fmt.Errorf("failed to fetch latest posts: %w", err)
	}

	if len(posts.Posts) == 0 {
		fmt.Println("No latest posts found")
		return nil
	}

	formatter.PrintInfo("⏱️  Latest Posts")
	fmt.Printf("\n")
	rs.displayPostsList(posts)

	return nil
}

// BrowseForYou displays personalized for-you feed
func (rs *RecommendationsService) BrowseForYou() error {
	posts, err := api.GetForYouFeed(1, 20)
	if err != nil {
		return fmt.Errorf("failed to fetch for-you feed: %w", err)
	}

	if len(posts.Posts) == 0 {
		fmt.Println("No personalized recommendations available yet")
		return nil
	}

	formatter.PrintInfo("🎯 For You - Personalized Feed")
	fmt.Printf("\n")
	rs.displayPostsList(posts)

	return nil
}

// DiscoverByGenre explores content by genre
func (rs *RecommendationsService) DiscoverByGenre() error {
	reader := bufio.NewReader(os.Stdin)

	fmt.Print("Genre (e.g., Electronic, Hip-Hop, Ambient, Synthwave): ")
	genre, _ := reader.ReadString('\n')
	genre = strings.TrimSpace(genre)

	if genre == "" {
		return fmt.Errorf("genre cannot be empty")
	}

	users, err := api.DiscoverByGenre(genre, 1, 20)
	if err != nil {
		return fmt.Errorf("failed to discover by genre: %w", err)
	}

	if len(users.Users) == 0 {
		fmt.Printf("No producers found in genre: %s\n", genre)
		return nil
	}

	formatter.PrintInfo("🎸 Producers in %s (%d found)", genre, users.TotalCount)
	fmt.Printf("\n")
	rs.displayUsersList(users)

	return nil
}

// DiscoverSimilar finds similar producers to follow
func (rs *RecommendationsService) DiscoverSimilar() error {
	reader := bufio.NewReader(os.Stdin)

	fmt.Print("User ID (or username) to find similar producers: ")
	userID, _ := reader.ReadString('\n')
	userID = strings.TrimSpace(userID)

	if userID == "" {
		return fmt.Errorf("user ID cannot be empty")
	}

	discovery, err := api.DiscoverSimilarUsers(userID, 20)
	if err != nil {
		return fmt.Errorf("failed to discover similar users: %w", err)
	}

	if len(discovery.Users) == 0 {
		fmt.Printf("No similar producers found for: %s\n", userID)
		return nil
	}

	formatter.PrintInfo("👥 Similar Producers to %s", userID)
	fmt.Printf("\n")

	for i, user := range discovery.Users {
		fmt.Printf("[%d] %s (@%s)\n", i+1, user.DisplayName, user.Username)
		if user.Bio != "" {
			fmt.Printf("    Bio: %s\n", truncate(user.Bio, 60))
		}
		fmt.Printf("    Followers: %d | Posts: %d\n\n", user.FollowerCount, user.PostCount)
	}

	return nil
}

// DiscoverFeatured shows featured creators to follow
func (rs *RecommendationsService) DiscoverFeatured() error {
	users, err := api.DiscoverFeatured(1, 20)
	if err != nil {
		return fmt.Errorf("failed to fetch featured users: %w", err)
	}

	if len(users.Users) == 0 {
		fmt.Println("No featured producers available")
		return nil
	}

	formatter.PrintInfo("⭐ Featured Producers")
	fmt.Printf("\n")
	rs.displayUsersList(users)

	return nil
}

// DiscoverSuggested shows suggested creators based on interests
func (rs *RecommendationsService) DiscoverSuggested() error {
	users, err := api.DiscoverSuggested(1, 20)
	if err != nil {
		return fmt.Errorf("failed to fetch suggested users: %w", err)
	}

	if len(users.Users) == 0 {
		fmt.Println("No suggested producers available")
		return nil
	}

	formatter.PrintInfo("💡 Suggested for You")
	fmt.Printf("\n")
	rs.displayUsersList(users)

	return nil
}

// Helper functions

func (rs *RecommendationsService) displayPopularPosts() error {
	posts, err := api.GetPopularPosts(1, 20)
	if err != nil {
		return fmt.Errorf("failed to fetch popular posts: %w", err)
	}

	if len(posts.Posts) == 0 {
		fmt.Println("No popular posts found")
		return nil
	}

	formatter.PrintInfo("🔥 Popular Posts - Trending")
	fmt.Printf("\n")
	rs.displayPostsList(posts)
	return nil
}

func (rs *RecommendationsService) displayPopularSounds() error {
	sounds, err := api.GetPopularSounds(1, 20)
	if err != nil {
		return fmt.Errorf("failed to fetch popular sounds: %w", err)
	}

	if len(sounds.Sounds) == 0 {
		fmt.Println("No popular sounds found")
		return nil
	}

	formatter.PrintInfo("🔥 Trending Sounds")
	fmt.Printf("\n")

	for i, sound := range sounds.Sounds {
		fmt.Printf("[%d] %s\n", i+1, sound.Name)
		if len(sound.Genre) > 0 {
			fmt.Printf("    Genre: %s\n", strings.Join(sound.Genre, ", "))
		}
		if sound.BPM > 0 {
			fmt.Printf("    BPM: %d\n", sound.BPM)
		}
		fmt.Printf("    Downloads: %d\n\n", sound.DownloadCount)
	}

	return nil
}

func (rs *RecommendationsService) displayPopularUsers() error {
	users, err := api.GetPopularUsers(1, 20)
	if err != nil {
		return fmt.Errorf("failed to fetch popular users: %w", err)
	}

	if len(users.Users) == 0 {
		fmt.Println("No popular producers found")
		return nil
	}

	formatter.PrintInfo("🔥 Popular Producers")
	fmt.Printf("\n")
	rs.displayUsersList(users)
	return nil
}

func (rs *RecommendationsService) displayPostsList(posts *api.FeedResponse) {
	for i, post := range posts.Posts {
		fmt.Printf("[%d] %s\n", i+1, post.ID)
		fmt.Printf("    Author: @%s\n", post.AuthorUsername)
		fmt.Printf("    ❤️  %d | 💬 %d | 🔄 %d | 💾 %d\n",
			post.LikeCount, post.CommentCount, post.RepostCount, post.SaveCount)
		fmt.Printf("    Created: %s\n\n", post.CreatedAt.Format("2006-01-02 15:04:05"))
	}
}

func (rs *RecommendationsService) displayUsersList(users *api.UserListResponse) {
	for i, user := range users.Users {
		fmt.Printf("[%d] %s (@%s)\n", i+1, user.DisplayName, user.Username)
		if user.Bio != "" {
			fmt.Printf("    Bio: %s\n", truncate(user.Bio, 60))
		}
		fmt.Printf("    Followers: %d | Posts: %d\n\n", user.FollowerCount, user.PostCount)
	}
}
