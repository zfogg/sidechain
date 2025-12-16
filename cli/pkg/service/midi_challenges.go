package service

import (
	"bufio"
	"fmt"
	"os"
	"strings"

	"github.com/zfogg/sidechain/cli/pkg/api"
	"github.com/zfogg/sidechain/cli/pkg/formatter"
	"github.com/zfogg/sidechain/cli/pkg/logger"
)

// MIDIChallengesService provides MIDI challenge operations
type MIDIChallengesService struct{}

// NewMIDIChallengesService creates a new MIDI challenges service
func NewMIDIChallengesService() *MIDIChallengesService {
	return &MIDIChallengesService{}
}

// ListChallenges displays all MIDI challenges with optional status filter
func (mcs *MIDIChallengesService) ListChallenges(status string) error {
	logger.Debug("Listing MIDI challenges", "status", status)

	response, err := api.GetMIDIChallenges(status)
	if err != nil {
		return err
	}

	if len(response.Challenges) == 0 {
		fmt.Println("No MIDI challenges found")
		return nil
	}

	title := "🎹 MIDI Challenges"
	if status != "" {
		title += fmt.Sprintf(" (%s)", strings.ToTitle(status))
	}

	// Convert to pointers
	challenges := make([]*api.MIDIChallenge, len(response.Challenges))
	for i := range response.Challenges {
		challenges[i] = &response.Challenges[i]
	}

	mcs.displayChallengesList(title, challenges)
	return nil
}

// ViewChallenge displays a single challenge with details
func (mcs *MIDIChallengesService) ViewChallenge(challengeID string) error {
	logger.Debug("Viewing MIDI challenge", "challenge_id", challengeID)

	detailResponse, err := api.GetMIDIChallenge(challengeID)
	if err != nil {
		return err
	}

	mcs.displayChallengeDetails(&detailResponse.Challenge)
	return nil
}

// ViewLeaderboard displays challenge entries ranked by votes
func (mcs *MIDIChallengesService) ViewLeaderboard(challengeID string) error {
	logger.Debug("Viewing challenge leaderboard", "challenge_id", challengeID)

	detailResponse, err := api.GetMIDIChallenge(challengeID)
	if err != nil {
		return err
	}

	entries, err := api.GetMIDIChallengeEntries(challengeID, 100, 0)
	if err != nil {
		return err
	}

	if len(entries) == 0 {
		fmt.Printf("No entries yet for '%s'\n", detailResponse.Challenge.Title)
		return nil
	}

	// Sort entries by vote count (descending)
	for i := 0; i < len(entries); i++ {
		for j := i + 1; j < len(entries); j++ {
			if entries[j].VoteCount > entries[i].VoteCount {
				entries[i], entries[j] = entries[j], entries[i]
			}
		}
	}

	// Convert to pointers
	entriesPtr := make([]*api.MIDIChallengeEntry, len(entries))
	for i := range entries {
		entriesPtr[i] = &entries[i]
	}

	mcs.displayLeaderboard(&detailResponse.Challenge, entriesPtr)
	return nil
}

// SubmitEntry interactively submits an entry to a challenge
func (mcs *MIDIChallengesService) SubmitEntry(challengeID string) error {
	logger.Debug("Submitting challenge entry", "challenge_id", challengeID)

	// Get challenge to show details
	detailResponse, err := api.GetMIDIChallenge(challengeID)
	if err != nil {
		return err
	}

	challenge := &detailResponse.Challenge

	// Check challenge status
	if challenge.Status != "active" {
		return fmt.Errorf("challenge is not accepting submissions (status: %s)", challenge.Status)
	}

	formatter.PrintInfo("📝 Submit Entry to Challenge")
	fmt.Printf("Challenge: %s\n", challenge.Title)
	fmt.Printf("Status: %s\n", challenge.Status)
	fmt.Printf("Submission deadline: %s\n\n", challenge.EndDate.Format("2006-01-02 15:04:05"))

	// Display constraints
	constraints := challenge.Constraints
	if constraints.BPMMin != nil || constraints.BPMMax != nil {
		if constraints.BPMMin != nil && constraints.BPMMax != nil {
			fmt.Printf("BPM: %d - %d\n", *constraints.BPMMin, *constraints.BPMMax)
		} else if constraints.BPMMin != nil {
			fmt.Printf("BPM: %d+\n", *constraints.BPMMin)
		} else if constraints.BPMMax != nil {
			fmt.Printf("BPM: up to %d\n", *constraints.BPMMax)
		}
	}

	if constraints.Key != nil && *constraints.Key != "" {
		fmt.Printf("Key: %s\n", *constraints.Key)
	}

	if constraints.Scale != nil && *constraints.Scale != "" {
		fmt.Printf("Scale: %s\n", *constraints.Scale)
	}

	if constraints.NoteCountMin != nil || constraints.NoteCountMax != nil {
		if constraints.NoteCountMin != nil && constraints.NoteCountMax != nil {
			fmt.Printf("Note count: %d - %d\n", *constraints.NoteCountMin, *constraints.NoteCountMax)
		}
	}

	if constraints.DurationMin != nil || constraints.DurationMax != nil {
		if constraints.DurationMin != nil && constraints.DurationMax != nil {
			fmt.Printf("Duration: %.1f - %.1f seconds\n", *constraints.DurationMin, *constraints.DurationMax)
		}
	}

	fmt.Printf("\n")

	reader := bufio.NewReader(os.Stdin)

	fmt.Print("Audio URL: ")
	audioURL, _ := reader.ReadString('\n')
	audioURL = strings.TrimSpace(audioURL)

	if audioURL == "" {
		return fmt.Errorf("audio URL is required")
	}

	// Optional: post ID or pattern ID
	fmt.Print("Post ID (optional): ")
	postIDStr, _ := reader.ReadString('\n')
	postIDStr = strings.TrimSpace(postIDStr)
	var postID *string
	if postIDStr != "" {
		postID = &postIDStr
	}

	fmt.Print("MIDI Pattern ID (optional): ")
	patternIDStr, _ := reader.ReadString('\n')
	patternIDStr = strings.TrimSpace(patternIDStr)
	var midiPatternID *string
	if patternIDStr != "" {
		midiPatternID = &patternIDStr
	}

	entry, err := api.SubmitChallengeEntry(challengeID, audioURL, postID, midiPatternID)
	if err != nil {
		return err
	}

	formatter.PrintSuccess("Entry submitted successfully!")
	fmt.Printf("Entry ID: %s\n", entry.ID)
	fmt.Printf("Submitted at: %s\n\n", entry.SubmittedAt.Format("2006-01-02 15:04:05"))

	return nil
}

// VoteForEntry votes for a challenge entry
func (mcs *MIDIChallengesService) VoteForEntry(challengeID string) error {
	logger.Debug("Voting for entry", "challenge_id", challengeID)

	detailResponse, err := api.GetMIDIChallenge(challengeID)
	if err != nil {
		return err
	}

	challenge := &detailResponse.Challenge

	if challenge.Status != "voting" {
		return fmt.Errorf("voting is not open for this challenge (status: %s)", challenge.Status)
	}

	// Display leaderboard first
	entries, err := api.GetMIDIChallengeEntries(challengeID, 100, 0)
	if err != nil {
		return err
	}

	if len(entries) == 0 {
		return fmt.Errorf("no entries to vote on")
	}

	// Sort entries by vote count (descending)
	for i := 0; i < len(entries); i++ {
		for j := i + 1; j < len(entries); j++ {
			if entries[j].VoteCount > entries[i].VoteCount {
				entries[i], entries[j] = entries[j], entries[i]
			}
		}
	}

	// Convert to pointers
	entriesPtr := make([]*api.MIDIChallengeEntry, len(entries))
	for i := range entries {
		entriesPtr[i] = &entries[i]
	}

	mcs.displayLeaderboard(challenge, entriesPtr)

	reader := bufio.NewReader(os.Stdin)
	fmt.Print("\nEntry ID to vote for: ")
	entryID, _ := reader.ReadString('\n')
	entryID = strings.TrimSpace(entryID)

	if entryID == "" {
		return fmt.Errorf("entry ID is required")
	}

	err = api.VoteChallengeEntry(challengeID, entryID)
	if err != nil {
		return err
	}

	formatter.PrintSuccess("Vote recorded successfully!")
	return nil
}

// Helper display functions

func (mcs *MIDIChallengesService) displayChallengesList(title string, challenges []*api.MIDIChallenge) {
	fmt.Printf("\n%s\n", title)
	fmt.Println(strings.Repeat("=", len(title)))
	fmt.Printf("\n")

	for i, challenge := range challenges {
		statusEmoji := mcs.getStatusEmoji(challenge.Status)
		fmt.Printf("%d. %s %s\n", i+1, statusEmoji, challenge.Title)
		fmt.Printf("   ID: %s\n", challenge.ID)
		fmt.Printf("   Status: %s\n", challenge.Status)

		if challenge.Status == "upcoming" {
			fmt.Printf("   Starts: %s\n", challenge.StartDate.Format("2006-01-02 15:04"))
		} else if challenge.Status == "active" {
			fmt.Printf("   Deadline: %s\n", challenge.EndDate.Format("2006-01-02 15:04"))
		} else if challenge.Status == "voting" {
			fmt.Printf("   Voting ends: %s\n", challenge.VotingEndDate.Format("2006-01-02 15:04"))
			fmt.Printf("   Entries: %d\n", challenge.EntryCount)
		}

		fmt.Printf("\n")
	}
}

func (mcs *MIDIChallengesService) displayChallengeDetails(challenge *api.MIDIChallenge) {
	fmt.Printf("\n🎹 %s\n", challenge.Title)
	fmt.Println(strings.Repeat("=", len(challenge.Title)+2))

	fmt.Printf("\nID: %s\n", challenge.ID)
	fmt.Printf("Status: %s\n", challenge.Status)
	fmt.Printf("\nDates:\n")
	fmt.Printf("  Start: %s\n", challenge.StartDate.Format("2006-01-02 15:04:05"))
	fmt.Printf("  Submit deadline: %s\n", challenge.EndDate.Format("2006-01-02 15:04:05"))
	fmt.Printf("  Voting deadline: %s\n", challenge.VotingEndDate.Format("2006-01-02 15:04:05"))

	fmt.Printf("\nConstraints:\n")
	constraints := challenge.Constraints
	if constraints.BPMMin != nil || constraints.BPMMax != nil {
		if constraints.BPMMin != nil && constraints.BPMMax != nil {
			fmt.Printf("  BPM: %d - %d\n", *constraints.BPMMin, *constraints.BPMMax)
		} else if constraints.BPMMin != nil {
			fmt.Printf("  BPM: %d+\n", *constraints.BPMMin)
		} else if constraints.BPMMax != nil {
			fmt.Printf("  BPM: up to %d\n", *constraints.BPMMax)
		}
	}

	if constraints.Key != nil && *constraints.Key != "" {
		fmt.Printf("  Key: %s\n", *constraints.Key)
	}

	if constraints.Scale != nil && *constraints.Scale != "" {
		fmt.Printf("  Scale: %s\n", *constraints.Scale)
	}

	if constraints.NoteCountMin != nil || constraints.NoteCountMax != nil {
		if constraints.NoteCountMin != nil && constraints.NoteCountMax != nil {
			fmt.Printf("  Note count: %d - %d\n", *constraints.NoteCountMin, *constraints.NoteCountMax)
		}
	}

	if constraints.DurationMin != nil || constraints.DurationMax != nil {
		if constraints.DurationMin != nil && constraints.DurationMax != nil {
			fmt.Printf("  Duration: %.1f - %.1f seconds\n", *constraints.DurationMin, *constraints.DurationMax)
		}
	}

	if challenge.Description != "" {
		fmt.Printf("\nDescription:\n%s\n", challenge.Description)
	}

	fmt.Printf("\nEntries: %d\n\n", challenge.EntryCount)
}

func (mcs *MIDIChallengesService) displayLeaderboard(challenge *api.MIDIChallenge, entries []*api.MIDIChallengeEntry) {
	fmt.Printf("\n🏆 Leaderboard: %s\n", challenge.Title)
	fmt.Println(strings.Repeat("=", len(challenge.Title)+16))
	fmt.Printf("Status: %s\n\n", challenge.Status)

	fmt.Printf("%-2s | %-8s | %-20s | %s\n", "#", "Votes", "User", "Submitted")
	fmt.Println(strings.Repeat("-", 60))

	for i, entry := range entries {
		username := entry.User.Username
		if username == "" {
			username = "unknown"
		}

		fmt.Printf("%-2d | %-8d | %-20s | %s\n",
			i+1,
			entry.VoteCount,
			mcs.truncate(username, 20),
			entry.SubmittedAt.Format("2006-01-02 15:04"),
		)
	}

	fmt.Printf("\n")
}

func (mcs *MIDIChallengesService) getStatusEmoji(status string) string {
	switch status {
	case "active":
		return "🟢"
	case "voting":
		return "🔵"
	case "ended":
		return "⚫"
	case "upcoming":
		return "⚪"
	default:
		return "❓"
	}
}

func (mcs *MIDIChallengesService) truncate(s string, maxLen int) string {
	if len(s) > maxLen {
		return s[:maxLen-3] + "..."
	}
	return s
}
