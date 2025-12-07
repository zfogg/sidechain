package util

import (
	"strconv"
	"strings"
)

// ParseInt parses a string to an integer, returning defaultValue if parsing fails
func ParseInt(s string, defaultValue int) int {
	if val, err := strconv.Atoi(s); err == nil {
		return val
	}
	return defaultValue
}

// ParseIntParam parses a string to an integer, returning an error if parsing fails
func ParseIntParam(s string) (int, error) {
	return strconv.Atoi(s)
}

// ParseFloat parses a string to a float64, returning defaultValue if parsing fails
func ParseFloat(s string, defaultValue float64) float64 {
	if val, err := strconv.ParseFloat(s, 64); err == nil {
		return val
	}
	return defaultValue
}

// ParseGenreArray parses a comma-separated string of genres into a slice
// Currently returns a single-item slice, but can be enhanced to split on commas
func ParseGenreArray(s string) []string {
	if s == "" {
		return []string{}
	}
	// Simple comma-separated parsing - could be improved
	// For now, return as single item (maintains backward compatibility)
	if strings.Contains(s, ",") {
		genres := strings.Split(s, ",")
		result := make([]string, 0, len(genres))
		for _, g := range genres {
			g = strings.TrimSpace(g)
			if g != "" {
				result = append(result, g)
			}
		}
		return result
	}
	return []string{s}
}
