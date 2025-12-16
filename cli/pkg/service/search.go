package service

import (
	"bufio"
	"fmt"
	"os"
	"strings"

	"github.com/zfogg/sidechain/cli/pkg/api"
	"github.com/zfogg/sidechain/cli/pkg/formatter"
)

// SearchService provides search and discovery operations
type SearchService struct{}

// NewSearchService creates a new search service
func NewSearchService() *SearchService {
	return &SearchService{}
}

// SearchUsers searches for users by username or display name
func (ss *SearchService) SearchUsers() error {
	reader := bufio.NewReader(os.Stdin)

	fmt.Print("Search query (username or name): ")
	query, _ := reader.ReadString('\n')
	query = strings.TrimSpace(query)

	if query == "" {
		return fmt.Errorf("search query cannot be empty")
	}

	// Search users
	resp, err := api.SearchUsers(query, 20, 0)
	if err != nil {
		return fmt.Errorf("failed to search users: %w", err)
	}

	if len(resp.Users) == 0 {
		fmt.Println("No users found")
		return nil
	}

	// Create table data
	headers := []string{"Username", "Display Name", "Bio", "Followers", "Posts"}
	rows := make([][]string, len(resp.Users))

	for i, user := range resp.Users {
		rows[i] = []string{
			user.Username,
			user.DisplayName,
			truncate(user.Bio, 30),
			fmt.Sprintf("%d", user.FollowerCount),
			fmt.Sprintf("%d", user.PostCount),
		}
	}

	fmt.Printf("\n🔍 Search Results for \"%s\" (%d found)\n\n", query, resp.TotalCount)
	formatter.PrintTable(headers, rows)

	return nil
}

// GetTrendingUsers retrieves trending producers
func (ss *SearchService) GetTrendingUsers() error {
	// Fetch trending users (using existing API function with default pagination)
	resp, err := api.GetTrendingUsers(1, 20)
	if err != nil {
		return fmt.Errorf("failed to fetch trending users: %w", err)
	}

	if len(resp.Users) == 0 {
		fmt.Println("No trending users found")
		return nil
	}

	// Create table data
	headers := []string{"Username", "Display Name", "Followers", "Posts"}
	rows := make([][]string, len(resp.Users))

	for i, user := range resp.Users {
		rows[i] = []string{
			user.Username,
			user.DisplayName,
			fmt.Sprintf("%d", user.FollowerCount),
			fmt.Sprintf("%d", user.PostCount),
		}
	}

	fmt.Printf("\n📈 Trending Producers\n\n")
	formatter.PrintTable(headers, rows)

	return nil
}

// GetFeaturedProducers retrieves featured/curated producers
func (ss *SearchService) GetFeaturedProducers() error {
	// Fetch featured producers (using existing API function)
	resp, err := api.GetFeaturedUsers(1, 10)
	if err != nil {
		return fmt.Errorf("failed to fetch featured producers: %w", err)
	}

	if len(resp.Users) == 0 {
		fmt.Println("No featured producers found")
		return nil
	}

	// Create table data
	headers := []string{"Username", "Display Name", "Bio", "Followers"}
	rows := make([][]string, len(resp.Users))

	for i, user := range resp.Users {
		rows[i] = []string{
			user.Username,
			user.DisplayName,
			truncate(user.Bio, 35),
			fmt.Sprintf("%d", user.FollowerCount),
		}
	}

	fmt.Printf("\n⭐ Featured Producers\n\n")
	formatter.PrintTable(headers, rows)

	return nil
}

// GetSuggestedUsers retrieves personalized suggestions
func (ss *SearchService) GetSuggestedUsers() error {
	// Fetch suggested users (using existing API function)
	resp, err := api.GetSuggestedUsers(1, 10)
	if err != nil {
		return fmt.Errorf("failed to fetch suggested users: %w", err)
	}

	if len(resp.Users) == 0 {
		fmt.Println("No suggestions available at this time")
		return nil
	}

	// Create table data
	headers := []string{"Username", "Display Name", "Bio", "Followers"}
	rows := make([][]string, len(resp.Users))

	for i, user := range resp.Users {
		rows[i] = []string{
			user.Username,
			user.DisplayName,
			truncate(user.Bio, 35),
			fmt.Sprintf("%d", user.FollowerCount),
		}
	}

	fmt.Printf("\n💡 Suggested For You\n\n")
	formatter.PrintTable(headers, rows)

	return nil
}

// ListGenres displays available genres
func (ss *SearchService) ListGenres() error {
	// Fetch genres
	resp, err := api.GetAvailableGenres()
	if err != nil {
		return fmt.Errorf("failed to fetch genres: %w", err)
	}

	if len(resp.Genres) == 0 {
		fmt.Println("No genres available")
		return nil
	}

	// Create table data
	headers := []string{"Genre", "Producers"}
	rows := make([][]string, len(resp.Genres))

	for i, genre := range resp.Genres {
		rows[i] = []string{
			genre.Name,
			fmt.Sprintf("%d", genre.UserCount),
		}
	}

	fmt.Printf("\n🎵 Available Genres (%d total)\n\n", len(resp.Genres))
	formatter.PrintTable(headers, rows)

	return nil
}

// GetUsersByGenre retrieves users in a specific genre
func (ss *SearchService) GetUsersByGenre() error {
	reader := bufio.NewReader(os.Stdin)

	fmt.Print("Genre name: ")
	genre, _ := reader.ReadString('\n')
	genre = strings.TrimSpace(genre)

	if genre == "" {
		return fmt.Errorf("genre cannot be empty")
	}

	// Fetch users by genre (using existing API function)
	resp, err := api.GetUsersByGenre(genre, 1, 20)
	if err != nil {
		return fmt.Errorf("failed to fetch users by genre: %w", err)
	}

	if len(resp.Users) == 0 {
		fmt.Printf("No users found for genre: %s\n", genre)
		return nil
	}

	// Create table data
	headers := []string{"Username", "Display Name", "Followers", "Posts"}
	rows := make([][]string, len(resp.Users))

	for i, user := range resp.Users {
		rows[i] = []string{
			user.Username,
			user.DisplayName,
			fmt.Sprintf("%d", user.FollowerCount),
			fmt.Sprintf("%d", user.PostCount),
		}
	}

	fmt.Printf("\n🎵 Producers in \"%s\" Genre (%d found)\n\n", genre, resp.TotalCount)
	formatter.PrintTable(headers, rows)

	return nil
}

// GetSimilarUsers retrieves users with similar music taste
func (ss *SearchService) GetSimilarUsers() error {
	reader := bufio.NewReader(os.Stdin)

	fmt.Print("User ID to find similar producers for: ")
	userID, _ := reader.ReadString('\n')
	userID = strings.TrimSpace(userID)

	if userID == "" {
		return fmt.Errorf("user ID cannot be empty")
	}

	// Fetch similar users
	resp, err := api.GetSimilarUsers(userID, 10)
	if err != nil {
		return fmt.Errorf("failed to fetch similar users: %w", err)
	}

	if len(resp.Users) == 0 {
		fmt.Println("No similar users found")
		return nil
	}

	// Create table data
	headers := []string{"Username", "Display Name", "Followers", "Posts", "Following"}
	rows := make([][]string, len(resp.Users))

	for i, user := range resp.Users {
		following := "No"
		if user.IsFollowing {
			following = "✓ Yes"
		}
		rows[i] = []string{
			user.Username,
			user.DisplayName,
			fmt.Sprintf("%d", user.FollowerCount),
			fmt.Sprintf("%d", user.PostCount),
			following,
		}
	}

	fmt.Printf("\n👥 Similar Producers\n\n")
	formatter.PrintTable(headers, rows)

	return nil
}
