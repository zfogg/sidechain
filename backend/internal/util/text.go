package util

import (
	"strings"
)

// ExtractMentions extracts @username mentions from text content
// Returns a slice of unique usernames (lowercase, without @ symbol)
func ExtractMentions(content string) []string {
	var mentions []string
	words := strings.Fields(content)
	seen := make(map[string]bool)

	for _, word := range words {
		if strings.HasPrefix(word, "@") && len(word) > 1 {
			// Clean the username (remove trailing punctuation)
			username := strings.TrimPrefix(word, "@")
			username = strings.TrimRight(username, ".,!?;:")
			username = strings.ToLower(username)

			if !seen[username] && len(username) >= 3 && len(username) <= 30 {
				seen[username] = true
				mentions = append(mentions, username)
			}
		}
	}
	return mentions
}
