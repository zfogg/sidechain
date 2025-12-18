package main

import (
	"fmt"
	"io"
	"net/http"
	"net/url"
	"strconv"
	"strings"

	"encoding/json"

	"github.com/spf13/cobra"
)

var searchCmd = &cobra.Command{
	Use:   "search",
	Short: "Search for users, posts, and stories",
	Long:  "Commands for searching users, posts, and stories with various filters",
}

var searchUsersCmd = &cobra.Command{
	Use:   "users <query>",
	Short: "Search for users by username or display name",
	Long: `Search for users with full-text search powered by Elasticsearch.

Examples:
  sidechain search users "john"
  sidechain search users "producer" --limit 50`,
	Args: cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		limit, _ := cmd.Flags().GetInt("limit")
		offset, _ := cmd.Flags().GetInt("offset")
		return searchUsers(args[0], limit, offset)
	},
}

var searchPostsCmd = &cobra.Command{
	Use:   "posts <query>",
	Short: "Search for posts/loops by title, filename, or metadata",
	Long: `Search for posts with filters for BPM, key, genre, and more.

Examples:
  sidechain search posts "house loop"
  sidechain search posts "loop" --bpm-min 120 --bpm-max 130 --genre "House"`,
	Args: cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		limit, _ := cmd.Flags().GetInt("limit")
		offset, _ := cmd.Flags().GetInt("offset")
		bpmMin, _ := cmd.Flags().GetInt("bpm-min")
		bpmMax, _ := cmd.Flags().GetInt("bpm-max")
		key, _ := cmd.Flags().GetString("key")
		genres, _ := cmd.Flags().GetStringSlice("genres")
		return searchPosts(args[0], limit, offset, bpmMin, bpmMax, key, genres)
	},
}

var searchStoriesCmd = &cobra.Command{
	Use:   "stories <creator>",
	Short: "Search for stories by creator username",
	Long: `Search for ephemeral stories from a specific creator.

Examples:
  sidechain search stories "producer_name"
  sidechain search stories "artist" --limit 20`,
	Args: cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		limit, _ := cmd.Flags().GetInt("limit")
		offset, _ := cmd.Flags().GetInt("offset")
		return searchStories(args[0], limit, offset)
	},
}

func init() {
	searchCmd.AddCommand(searchUsersCmd)
	searchCmd.AddCommand(searchPostsCmd)
	searchCmd.AddCommand(searchStoriesCmd)

	// Flags for search users
	searchUsersCmd.Flags().IntP("limit", "l", 20, "Maximum number of results")
	searchUsersCmd.Flags().IntP("offset", "o", 0, "Result offset for pagination")

	// Flags for search posts
	searchPostsCmd.Flags().IntP("limit", "l", 20, "Maximum number of results")
	searchPostsCmd.Flags().IntP("offset", "o", 0, "Result offset for pagination")
	searchPostsCmd.Flags().Int("bpm-min", 0, "Minimum BPM filter")
	searchPostsCmd.Flags().Int("bpm-max", 200, "Maximum BPM filter")
	searchPostsCmd.Flags().String("key", "", "Musical key filter (e.g., 'C Major', 'A Minor')")
	searchPostsCmd.Flags().StringSlice("genres", []string{}, "Genre filters (comma-separated or repeated)")

	// Flags for search stories
	searchStoriesCmd.Flags().IntP("limit", "l", 20, "Maximum number of results")
	searchStoriesCmd.Flags().IntP("offset", "o", 0, "Result offset for pagination")
}

func searchUsers(query string, limit, offset int) error {
	if query == "" {
		return fmt.Errorf("search query cannot be empty")
	}

	params := url.Values{}
	params.Set("q", query)
	params.Set("limit", strconv.Itoa(limit))
	params.Set("offset", strconv.Itoa(offset))

	endpoint := apiURL + "/api/v1/search/users?" + params.Encode()

	req, err := http.NewRequest("GET", endpoint, nil)
	if err != nil {
		return fmt.Errorf("failed to create request: %w", err)
	}

	req.Header.Set("Authorization", "Bearer "+authToken)

	client := &http.Client{}
	resp, err := client.Do(req)
	if err != nil {
		return fmt.Errorf("failed to make request: %w", err)
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return fmt.Errorf("failed to read response: %w", err)
	}

	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		var errResp map[string]interface{}
		json.Unmarshal(body, &errResp)
		if msg, ok := errResp["message"].(string); ok {
			return fmt.Errorf("API error: %s", msg)
		}
		return fmt.Errorf("API error: status %d", resp.StatusCode)
	}

	var result map[string]interface{}
	if err := json.Unmarshal(body, &result); err != nil {
		return fmt.Errorf("failed to parse response: %w", err)
	}

	if output == "json" {
		fmt.Println(string(body))
	} else {
		printUserSearchResults(result)
	}

	return nil
}

func searchPosts(query string, limit, offset, bpmMin, bpmMax int, key string, genres []string) error {
	if query == "" {
		return fmt.Errorf("search query cannot be empty")
	}

	params := url.Values{}
	params.Set("q", query)
	params.Set("limit", strconv.Itoa(limit))
	params.Set("offset", strconv.Itoa(offset))

	if bpmMin > 0 {
		params.Set("bpm_min", strconv.Itoa(bpmMin))
	}
	if bpmMax > 0 && bpmMax < 200 {
		params.Set("bpm_max", strconv.Itoa(bpmMax))
	}
	if key != "" {
		params.Set("key", key)
	}
	if len(genres) > 0 {
		params.Set("genres", strings.Join(genres, ","))
	}

	endpoint := apiURL + "/api/v1/search/posts?" + params.Encode()

	req, err := http.NewRequest("GET", endpoint, nil)
	if err != nil {
		return fmt.Errorf("failed to create request: %w", err)
	}

	req.Header.Set("Authorization", "Bearer "+authToken)

	client := &http.Client{}
	resp, err := client.Do(req)
	if err != nil {
		return fmt.Errorf("failed to make request: %w", err)
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return fmt.Errorf("failed to read response: %w", err)
	}

	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		var errResp map[string]interface{}
		json.Unmarshal(body, &errResp)
		if msg, ok := errResp["message"].(string); ok {
			return fmt.Errorf("API error: %s", msg)
		}
		return fmt.Errorf("API error: status %d", resp.StatusCode)
	}

	var result map[string]interface{}
	if err := json.Unmarshal(body, &result); err != nil {
		return fmt.Errorf("failed to parse response: %w", err)
	}

	if output == "json" {
		fmt.Println(string(body))
	} else {
		printPostSearchResults(result)
	}

	return nil
}

func searchStories(creator string, limit, offset int) error {
	if creator == "" {
		return fmt.Errorf("creator username cannot be empty")
	}

	params := url.Values{}
	params.Set("q", creator)
	params.Set("limit", strconv.Itoa(limit))
	params.Set("offset", strconv.Itoa(offset))

	endpoint := apiURL + "/api/v1/search/stories?" + params.Encode()

	req, err := http.NewRequest("GET", endpoint, nil)
	if err != nil {
		return fmt.Errorf("failed to create request: %w", err)
	}

	req.Header.Set("Authorization", "Bearer "+authToken)

	client := &http.Client{}
	resp, err := client.Do(req)
	if err != nil {
		return fmt.Errorf("failed to make request: %w", err)
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return fmt.Errorf("failed to read response: %w", err)
	}

	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		var errResp map[string]interface{}
		json.Unmarshal(body, &errResp)
		if msg, ok := errResp["message"].(string); ok {
			return fmt.Errorf("API error: %s", msg)
		}
		return fmt.Errorf("API error: status %d", resp.StatusCode)
	}

	var result map[string]interface{}
	if err := json.Unmarshal(body, &result); err != nil {
		return fmt.Errorf("failed to parse response: %w", err)
	}

	if output == "json" {
		fmt.Println(string(body))
	} else {
		printStorySearchResults(result)
	}

	return nil
}

func printUserSearchResults(result map[string]interface{}) {
	fmt.Printf("\n🔍 User Search Results\n")
	fmt.Printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n")

	users, ok := result["users"].([]interface{})
	if !ok || len(users) == 0 {
		fmt.Printf("No users found\n\n")
		return
	}

	for i, u := range users {
		user, ok := u.(map[string]interface{})
		if !ok {
			continue
		}

		fmt.Printf("\n%d. ", i+1)
		if username, ok := user["username"].(string); ok {
			fmt.Printf("@%s", username)
		}
		if displayName, ok := user["display_name"].(string); ok && displayName != "" {
			fmt.Printf(" (%s)", displayName)
		}

		if followerCount, ok := user["follower_count"].(float64); ok {
			fmt.Printf(" · %d followers", int(followerCount))
		}

		fmt.Printf("\n")

		if bio, ok := user["bio"].(string); ok && bio != "" {
			fmt.Printf("   Bio: %s\n", bio)
		}
	}

	fmt.Printf("\n")
}

func printPostSearchResults(result map[string]interface{}) {
	fmt.Printf("\n🎵 Post Search Results\n")
	fmt.Printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n")

	posts, ok := result["posts"].([]interface{})
	if !ok || len(posts) == 0 {
		fmt.Printf("No posts found\n\n")
		return
	}

	for i, p := range posts {
		post, ok := p.(map[string]interface{})
		if !ok {
			continue
		}

		fmt.Printf("\n%d. ", i+1)
		if filename, ok := post["filename"].(string); ok {
			fmt.Printf("%s", filename)
		}

		if user, ok := post["user"].(map[string]interface{}); ok {
			if username, ok := user["username"].(string); ok {
				fmt.Printf(" by @%s", username)
			}
		}

		fmt.Printf("\n")

		// Metadata
		var metadata []string
		if bpm, ok := post["bpm"].(float64); ok && bpm > 0 {
			metadata = append(metadata, fmt.Sprintf("%d BPM", int(bpm)))
		}
		if key, ok := post["key"].(string); ok && key != "" {
			metadata = append(metadata, key)
		}
		if daw, ok := post["daw"].(string); ok && daw != "" {
			metadata = append(metadata, daw)
		}

		if len(metadata) > 0 {
			fmt.Printf("   %s\n", strings.Join(metadata, " · "))
		}

		// Engagement stats
		var stats []string
		if likes, ok := post["like_count"].(float64); ok && likes > 0 {
			stats = append(stats, fmt.Sprintf("❤️ %d", int(likes)))
		}
		if plays, ok := post["play_count"].(float64); ok && plays > 0 {
			stats = append(stats, fmt.Sprintf("🎧 %d", int(plays)))
		}
		if comments, ok := post["comment_count"].(float64); ok && comments > 0 {
			stats = append(stats, fmt.Sprintf("💬 %d", int(comments)))
		}

		if len(stats) > 0 {
			fmt.Printf("   %s\n", strings.Join(stats, " "))
		}
	}

	fmt.Printf("\n")
}

func printStorySearchResults(result map[string]interface{}) {
	fmt.Printf("\n📖 Story Search Results\n")
	fmt.Printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n")

	stories, ok := result["stories"].([]interface{})
	if !ok || len(stories) == 0 {
		fmt.Printf("No stories found\n\n")
		return
	}

	for i, s := range stories {
		story, ok := s.(map[string]interface{})
		if !ok {
			continue
		}

		fmt.Printf("\n%d. ", i+1)
		if title, ok := story["title"].(string); ok && title != "" {
			fmt.Printf("%s", title)
		} else {
			fmt.Printf("Untitled Story")
		}

		if user, ok := story["user"].(map[string]interface{}); ok {
			if username, ok := user["username"].(string); ok {
				fmt.Printf(" by @%s", username)
			}
		}

		fmt.Printf("\n")

		if description, ok := story["description"].(string); ok && description != "" {
			fmt.Printf("   %s\n", description)
		}

		// View count
		if viewCount, ok := story["view_count"].(float64); ok && viewCount > 0 {
			fmt.Printf("   👁️ %d views\n", int(viewCount))
		}
	}

	fmt.Printf("\n")
}
