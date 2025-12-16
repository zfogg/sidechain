package service

import (
	"fmt"
	"strings"
	"time"

	"github.com/zfogg/sidechain/cli/pkg/api"
	"github.com/zfogg/sidechain/cli/pkg/logger"
)

// MIDIService handles MIDI pattern operations
type MIDIService struct{}

// ChallengeService handles MIDI challenge operations
type ChallengeService struct{}

// NewMIDIService creates a new MIDI service
func NewMIDIService() *MIDIService {
	return &MIDIService{}
}

// NewChallengeService creates a new challenge service
func NewChallengeService() *ChallengeService {
	return &ChallengeService{}
}

// MIDI Pattern Operations

// ListMIDIPatterns lists user's MIDI patterns
func (ms *MIDIService) ListMIDIPatterns(limit, offset int) error {
	logger.Debug("Listing MIDI patterns", "limit", limit)

	if limit < 1 || limit > 100 {
		limit = 20
	}

	patterns, err := api.ListMIDIPatterns(limit, offset)
	if err != nil {
		logger.Error("Failed to fetch MIDI patterns", "error", err)
		return err
	}

	if len(patterns.Patterns) == 0 {
		fmt.Println("No MIDI patterns yet. Create one with `midi create`")
		return nil
	}

	ms.displayPatternList(patterns)
	return nil
}

// ViewMIDIPattern displays a specific MIDI pattern
func (ms *MIDIService) ViewMIDIPattern(patternID string) error {
	logger.Debug("Viewing MIDI pattern", "pattern_id", patternID)

	pattern, err := api.GetMIDIPattern(patternID)
	if err != nil {
		logger.Error("Failed to fetch MIDI pattern", "error", err)
		return err
	}

	ms.displayPatternDetail(pattern)
	return nil
}

// UpdateMIDIPattern updates pattern metadata
func (ms *MIDIService) UpdateMIDIPattern(patternID string, name string, description string, isPublic bool) error {
	logger.Debug("Updating MIDI pattern", "pattern_id", patternID)

	if name == "" {
		return fmt.Errorf("pattern name cannot be empty")
	}

	if len(name) > 100 {
		return fmt.Errorf("pattern name exceeds maximum length (100 characters)")
	}

	_, err := api.UpdateMIDIPattern(patternID, name, description, isPublic)
	if err != nil {
		logger.Error("Failed to update MIDI pattern", "error", err)
		return err
	}

	logger.Debug("MIDI pattern updated successfully")
	fmt.Println("✓ MIDI pattern updated")
	return nil
}

// DeleteMIDIPattern deletes a MIDI pattern
func (ms *MIDIService) DeleteMIDIPattern(patternID string) error {
	logger.Debug("Deleting MIDI pattern", "pattern_id", patternID)

	err := api.DeleteMIDIPattern(patternID)
	if err != nil {
		logger.Error("Failed to delete MIDI pattern", "error", err)
		return err
	}

	logger.Debug("MIDI pattern deleted successfully")
	fmt.Println("✓ MIDI pattern deleted")
	return nil
}

// MIDI Challenge Operations

// ListChallenges lists MIDI challenges with optional status filter
func (cs *ChallengeService) ListChallenges(status string) error {
	logger.Debug("Listing MIDI challenges", "status", status)

	challenges, err := api.GetMIDIChallenges(status)
	if err != nil {
		logger.Error("Failed to fetch challenges", "error", err)
		return err
	}

	if len(challenges.Challenges) == 0 {
		fmt.Println("No challenges available.")
		return nil
	}

	cs.displayChallengeList(challenges)
	return nil
}

// ViewChallenge displays a challenge with its entries
func (cs *ChallengeService) ViewChallenge(challengeID string) error {
	logger.Debug("Viewing challenge", "challenge_id", challengeID)

	challenge, err := api.GetMIDIChallenge(challengeID)
	if err != nil {
		logger.Error("Failed to fetch challenge", "error", err)
		return err
	}

	cs.displayChallengeDetail(challenge)
	return nil
}

// ViewChallengeEntries displays entries for a challenge
func (cs *ChallengeService) ViewChallengeEntries(challengeID string) error {
	logger.Debug("Viewing challenge entries", "challenge_id", challengeID)

	entries, err := api.GetMIDIChallengeEntries(challengeID, 100, 0)
	if err != nil {
		logger.Error("Failed to fetch challenge entries", "error", err)
		return err
	}

	if len(entries) == 0 {
		fmt.Println("No entries for this challenge yet.")
		return nil
	}

	cs.displayEntryList(entries)
	return nil
}

// SubmitEntry submits an entry to a challenge
func (cs *ChallengeService) SubmitEntry(challengeID string, audioURL string, postID *string, midiPatternID *string) error {
	logger.Debug("Submitting challenge entry", "challenge_id", challengeID)

	if audioURL == "" {
		return fmt.Errorf("audio URL cannot be empty")
	}

	entry, err := api.SubmitChallengeEntry(challengeID, audioURL, postID, midiPatternID)
	if err != nil {
		logger.Error("Failed to submit challenge entry", "error", err)
		return err
	}

	logger.Debug("Challenge entry submitted successfully", "entry_id", entry.ID)
	fmt.Println("✓ Entry submitted to challenge")
	return nil
}

// VoteEntry votes for a challenge entry
func (cs *ChallengeService) VoteEntry(challengeID string, entryID string) error {
	logger.Debug("Voting for challenge entry", "entry_id", entryID)

	err := api.VoteChallengeEntry(challengeID, entryID)
	if err != nil {
		logger.Error("Failed to vote for entry", "error", err)
		return err
	}

	logger.Debug("Vote recorded successfully")
	fmt.Println("✓ Vote recorded")
	return nil
}

// Display functions

func (ms *MIDIService) displayPatternList(patterns *api.MIDIPatternListResponse) {
	fmt.Printf("\n🎹 Your MIDI Patterns\n")
	fmt.Println(strings.Repeat("─", 60))

	for i, p := range patterns.Patterns {
		visibility := "🔒 Private"
		if p.IsPublic {
			visibility = "🌐 Public"
		}

		fmt.Printf("%2d. %s\n", i+1, p.Name)
		fmt.Printf("    Tempo: %d BPM | Key: %s | %s | ⬇ %d\n",
			p.Tempo, p.Key, visibility, p.DownloadCount)

		if p.Description != "" {
			desc := p.Description
			if len(desc) > 45 {
				desc = desc[:42] + "..."
			}
			fmt.Printf("    %s\n", desc)
		}

		if i < len(patterns.Patterns)-1 {
			fmt.Println(strings.Repeat("─", 60))
		}
	}

	fmt.Printf("\nTotal: %d patterns\n\n", patterns.TotalCount)
}

func (ms *MIDIService) displayPatternDetail(pattern *api.MIDIPattern) {
	fmt.Printf("\n🎹 %s\n", pattern.Name)
	fmt.Println(strings.Repeat("─", 60))

	if pattern.Description != "" {
		fmt.Printf("Description: %s\n", pattern.Description)
	}

	fmt.Printf("Creator: @%s\n", pattern.User.Username)
	fmt.Printf("Tempo: %d BPM\n", pattern.Tempo)
	fmt.Printf("Key: %s\n", pattern.Key)
	fmt.Printf("Duration: %.1f seconds\n", pattern.TotalTime)
	fmt.Printf("Time Signature: %d/%d\n", pattern.TimeSignature[0], pattern.TimeSignature[1])
	fmt.Printf("Events: %d\n", len(pattern.Events))

	visibility := "🔒 Private"
	if pattern.IsPublic {
		visibility = "🌐 Public"
	}
	fmt.Printf("Visibility: %s\n", visibility)
	fmt.Printf("Downloads: %d\n", pattern.DownloadCount)

	fmt.Println()
}

func (cs *ChallengeService) displayChallengeList(challenges *api.MIDIChallengeListResponse) {
	fmt.Printf("\n🏆 MIDI Challenges\n")
	fmt.Println(strings.Repeat("─", 60))

	for i, c := range challenges.Challenges {
		statusEmoji := "⏳"
		if c.Status == "active" {
			statusEmoji = "🔴"
		} else if c.Status == "voting" {
			statusEmoji = "🗳️"
		} else if c.Status == "ended" {
			statusEmoji = "✅"
		}

		fmt.Printf("%2d. %s %s\n", i+1, statusEmoji, c.Title)
		fmt.Printf("    Status: %s | Entries: %d\n", c.Status, c.EntryCount)

		if c.Status == "active" {
			timeLeft := time.Until(c.EndDate)
			hours := int(timeLeft.Hours())
			fmt.Printf("    Ends in: %dh\n", hours)
		}

		if i < len(challenges.Challenges)-1 {
			fmt.Println(strings.Repeat("─", 60))
		}
	}

	fmt.Printf("\nTotal: %d challenges\n\n", challenges.TotalCount)
}

func (cs *ChallengeService) displayChallengeDetail(detail *api.MIDIChallengeDetailResponse) {
	challenge := detail.Challenge

	fmt.Printf("\n🏆 %s\n", challenge.Title)
	fmt.Println(strings.Repeat("─", 60))

	if challenge.Description != "" {
		fmt.Printf("Description: %s\n", challenge.Description)
	}

	fmt.Printf("Status: %s\n", challenge.Status)
	fmt.Printf("Entries: %d\n", challenge.EntryCount)

	// Display constraints
	if challenge.Constraints.BPMMin != nil || challenge.Constraints.BPMMax != nil {
		fmt.Printf("BPM Range: ")
		if challenge.Constraints.BPMMin != nil {
			fmt.Printf("%d - ", *challenge.Constraints.BPMMin)
		}
		if challenge.Constraints.BPMMax != nil {
			fmt.Printf("%d\n", *challenge.Constraints.BPMMax)
		} else {
			fmt.Println()
		}
	}

	if challenge.Constraints.Key != nil {
		fmt.Printf("Key: %s\n", *challenge.Constraints.Key)
	}

	if challenge.Constraints.Scale != nil {
		fmt.Printf("Scale: %s\n", *challenge.Constraints.Scale)
	}

	if challenge.Constraints.NoteCountMin != nil || challenge.Constraints.NoteCountMax != nil {
		fmt.Printf("Note Count: ")
		if challenge.Constraints.NoteCountMin != nil {
			fmt.Printf("%d - ", *challenge.Constraints.NoteCountMin)
		}
		if challenge.Constraints.NoteCountMax != nil {
			fmt.Printf("%d\n", *challenge.Constraints.NoteCountMax)
		} else {
			fmt.Println()
		}
	}

	// Display timeline
	fmt.Println(strings.Repeat("─", 60))
	fmt.Printf("Start: %s\n", challenge.StartDate.Format("Jan 02, 15:04"))
	fmt.Printf("End: %s\n", challenge.EndDate.Format("Jan 02, 15:04"))
	fmt.Printf("Voting Ends: %s\n", challenge.VotingEndDate.Format("Jan 02, 15:04"))

	// Display top entries
	if len(detail.Entries) > 0 {
		fmt.Println(strings.Repeat("─", 60))
		fmt.Println("Top Entries:")

		displayCount := 10
		if len(detail.Entries) < displayCount {
			displayCount = len(detail.Entries)
		}

		for j := 0; j < displayCount; j++ {
			entry := detail.Entries[j]
			fmt.Printf("%2d. @%s - 👍 %d votes\n", j+1, entry.User.Username, entry.VoteCount)
		}

		if len(detail.Entries) > displayCount {
			fmt.Printf("... and %d more\n", len(detail.Entries)-displayCount)
		}
	}

	fmt.Println()
}

func (cs *ChallengeService) displayEntryList(entries []api.MIDIChallengeEntry) {
	fmt.Printf("\n📋 Challenge Entries\n")
	fmt.Println(strings.Repeat("─", 60))

	for i, entry := range entries {
		fmt.Printf("%2d. @%s\n", i+1, entry.User.Username)
		fmt.Printf("    Votes: %d | Submitted: %s\n",
			entry.VoteCount, entry.SubmittedAt.Format("Jan 02, 15:04"))

		if i < len(entries)-1 {
			fmt.Println(strings.Repeat("─", 60))
		}
	}

	fmt.Printf("\nTotal: %d entries\n\n", len(entries))
}
