package challenges

import (
	"context"
	"fmt"
	"log"
	"time"

	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/models"
	"github.com/zfogg/sidechain/backend/internal/stream"
	"gorm.io/gorm"
)

// NotificationService handles periodic notifications for MIDI challenges
type NotificationService struct {
	streamClient stream.StreamClientInterface
	ctx          context.Context
	cancel       context.CancelFunc
	interval     time.Duration

	// Track last checked times to avoid duplicate notifications
	lastCheckedChallengeID map[string]time.Time
}

// NewNotificationService creates a new MIDI challenge notification service
func NewNotificationService(streamClient stream.StreamClientInterface, interval time.Duration) *NotificationService {
	ctx, cancel := context.WithCancel(context.Background())
	return &NotificationService{
		streamClient:           streamClient,
		ctx:                    ctx,
		cancel:                 cancel,
		interval:               interval,
		lastCheckedChallengeID: make(map[string]time.Time),
	}
}

// Start begins the periodic notification checking process
func (s *NotificationService) Start() {
	log.Println("🔔 Starting MIDI challenge notification service")
	go s.run()
}

// Stop stops the notification service
func (s *NotificationService) Stop() {
	log.Println("🔔 Stopping MIDI challenge notification service")
	s.cancel()
}

// run executes notification checks on the configured interval
func (s *NotificationService) run() {
	// Run immediately on startup (after a short delay to let server finish starting)
	time.Sleep(10 * time.Second)
	s.checkNotifications()

	// Then run on interval
	ticker := time.NewTicker(s.interval)
	defer ticker.Stop()

	for {
		select {
		case <-ticker.C:
			s.checkNotifications()
		case <-s.ctx.Done():
			return
		}
	}
}

// checkNotifications checks for all types of challenge notifications
func (s *NotificationService) checkNotifications() {
	startTime := time.Now()
	log.Println("🔔 Checking MIDI challenge notifications...")

	// Check for approaching deadlines (24 hours before)
	s.checkApproachingDeadlines()

	// Check for challenges transitioning to voting phase
	s.checkVotingPhaseTransitions()

	// Check for challenges transitioning to ended phase
	s.checkEndedPhaseTransitions()

	duration := time.Since(startTime)
	log.Printf("✅ MIDI challenge notification check completed (took %v)", duration)
}

// checkApproachingDeadlines checks for challenges with deadlines approaching (24 hours)
func (s *NotificationService) checkApproachingDeadlines() {
	now := time.Now()
	deadlineThreshold := now.Add(24 * time.Hour)

	// Find challenges where deadline is within 24 hours but not yet passed
	var challenges []models.MIDIChallenge
	if err := database.DB.Where("end_date > ? AND end_date <= ?", now, deadlineThreshold).
		Find(&challenges).Error; err != nil {
		log.Printf("❌ Failed to query challenges with approaching deadlines: %v", err)
		return
	}

	for _, challenge := range challenges {
		// Check if we've already notified for this deadline window
		lastChecked, exists := s.lastCheckedChallengeID[challenge.ID]
		deadlineWindowStart := challenge.EndDate.Add(-25 * time.Hour) // 24-25 hours before deadline

		if exists && lastChecked.After(deadlineWindowStart) {
			continue // Already notified for this deadline window
		}

		hoursRemaining := int(time.Until(challenge.EndDate).Hours())
		if hoursRemaining <= 0 || hoursRemaining > 24 {
			continue
		}

		// Notify all participants
		if err := s.notifyParticipants(&challenge, func(userID, streamUserID string) error {
			return s.streamClient.NotifyChallengeDeadline(streamUserID, challenge.ID, challenge.Title, hoursRemaining)
		}); err != nil {
			log.Printf("⚠️ Failed to notify participants about approaching deadline for challenge %s: %v", challenge.ID, err)
		} else {
			log.Printf("✅ Notified participants about approaching deadline for challenge %s (%d hours remaining)", challenge.ID, hoursRemaining)
			s.lastCheckedChallengeID[challenge.ID] = now
		}
	}
}

// checkVotingPhaseTransitions checks for challenges that just entered voting phase
func (s *NotificationService) checkVotingPhaseTransitions() {
	now := time.Now()

	// Find challenges that recently entered voting phase (end_date passed but voting_end_date not passed)
	// Check challenges where end_date was in the last hour (to catch transitions)
	votingWindowStart := now.Add(-1 * time.Hour)

	var challenges []models.MIDIChallenge
	if err := database.DB.Where("end_date <= ? AND end_date > ? AND voting_end_date > ?", now, votingWindowStart, now).
		Find(&challenges).Error; err != nil {
		log.Printf("❌ Failed to query challenges entering voting phase: %v", err)
		return
	}

	for _, challenge := range challenges {
		// Check if we've already notified for voting phase
		lastChecked, exists := s.lastCheckedChallengeID["voting_"+challenge.ID]
		if exists && lastChecked.After(challenge.EndDate) {
			continue // Already notified
		}

		// Notify all participants
		if err := s.notifyParticipants(&challenge, func(userID, streamUserID string) error {
			return s.streamClient.NotifyChallengeVotingOpen(streamUserID, challenge.ID, challenge.Title)
		}); err != nil {
			log.Printf("⚠️ Failed to notify participants about voting phase for challenge %s: %v", challenge.ID, err)
		} else {
			log.Printf("✅ Notified participants about voting phase for challenge %s", challenge.ID)
			s.lastCheckedChallengeID["voting_"+challenge.ID] = now
		}
	}
}

// checkEndedPhaseTransitions checks for challenges that just ended
func (s *NotificationService) checkEndedPhaseTransitions() {
	now := time.Now()

	// Find challenges that recently ended (voting_end_date passed in the last hour)
	endedWindowStart := now.Add(-1 * time.Hour)

	var challenges []models.MIDIChallenge
	if err := database.DB.Where("voting_end_date <= ? AND voting_end_date > ?", now, endedWindowStart).
		Find(&challenges).Error; err != nil {
		log.Printf("❌ Failed to query challenges that ended: %v", err)
		return
	}

	for _, challenge := range challenges {
		// Check if we've already notified for end
		lastChecked, exists := s.lastCheckedChallengeID["ended_"+challenge.ID]
		if exists && lastChecked.After(challenge.VotingEndDate) {
			continue // Already notified
		}

		// Get winner
		var winnerEntry models.MIDIChallengeEntry
		winnerUserID := ""
		winnerUsername := ""
		if err := database.DB.Where("challenge_id = ?", challenge.ID).
			Order("vote_count DESC, submitted_at ASC").
			Preload("User").
			First(&winnerEntry).Error; err == nil && winnerEntry.UserID != "" {
			winnerUserID = winnerEntry.UserID
			winnerUsername = winnerEntry.User.Username
		}

		// Notify all participants with winner info
		if err := s.notifyParticipantsWithRanking(&challenge, winnerUserID, winnerUsername, func(userID, streamUserID string, rank int) error {
			return s.streamClient.NotifyChallengeEnded(
				streamUserID,
				challenge.ID,
				challenge.Title,
				winnerUserID,
				winnerUsername,
				rank,
			)
		}); err != nil {
			log.Printf("⚠️ Failed to notify participants about challenge end for challenge %s: %v", challenge.ID, err)
		} else {
			log.Printf("✅ Notified participants about challenge end for challenge %s (winner: %s)", challenge.ID, winnerUsername)
			s.lastCheckedChallengeID["ended_"+challenge.ID] = now
		}
	}
}

// notifyParticipants sends notifications to all users who submitted entries to a challenge
func (s *NotificationService) notifyParticipants(challenge *models.MIDIChallenge, notifyFunc func(userID, streamUserID string) error) error {
	var entries []models.MIDIChallengeEntry
	if err := database.DB.Where("challenge_id = ?", challenge.ID).
		Select("DISTINCT user_id").
		Find(&entries).Error; err != nil {
		return fmt.Errorf("failed to get challenge participants: %w", err)
	}

	errors := 0
	successes := 0

	for _, entry := range entries {
		var user models.User
		if err := database.DB.First(&user, "id = ?", entry.UserID).Error; err != nil {
			log.Printf("⚠️ User %s not found for challenge notification", entry.UserID)
			errors++
			continue
		}

		if user.StreamUserID == "" {
			log.Printf("⚠️ User %s has no StreamUserID, skipping notification", entry.UserID)
			continue
		}

		if err := notifyFunc(user.ID, user.StreamUserID); err != nil {
			log.Printf("⚠️ Failed to notify user %s: %v", user.ID, err)
			errors++
			continue
		}

		successes++
	}

	log.Printf("📬 Notified %d participants for challenge %s (%d errors)", successes, challenge.ID, errors)
	return nil
}

// notifyParticipantsWithRanking sends notifications with ranking information
func (s *NotificationService) notifyParticipantsWithRanking(challenge *models.MIDIChallenge, winnerUserID, winnerUsername string, notifyFunc func(userID, streamUserID string, rank int) error) error {
	var entries []models.MIDIChallengeEntry
	if err := database.DB.Where("challenge_id = ?", challenge.ID).
		Order("vote_count DESC, submitted_at ASC").
		Find(&entries).Error; err != nil {
		return fmt.Errorf("failed to get challenge entries: %w", err)
	}

	errors := 0
	successes := 0

	// Build ranking map
	ranking := make(map[string]int)
	for i, entry := range entries {
		ranking[entry.UserID] = i // 0 = first place
	}

	// Notify each participant with their rank
	for _, entry := range entries {
		var user models.User
		if err := database.DB.First(&user, "id = ?", entry.UserID).Error; err != nil {
			log.Printf("⚠️ User %s not found for challenge notification", entry.UserID)
			errors++
			continue
		}

		if user.StreamUserID == "" {
			continue
		}

		rank := ranking[user.ID]
		if err := notifyFunc(user.ID, user.StreamUserID, rank); err != nil {
			log.Printf("⚠️ Failed to notify user %s: %v", user.ID, err)
			errors++
			continue
		}

		successes++
	}

	log.Printf("📬 Notified %d participants with rankings for challenge %s (%d errors)", successes, challenge.ID, errors)
	return nil
}

// NotifyAllUsersAboutNewChallenge sends notification to all users about a new challenge
// This should be called immediately when a challenge is created
func NotifyAllUsersAboutNewChallenge(streamClient stream.StreamClientInterface, challengeID, challengeTitle string) error {
	log.Printf("🔔 Notifying all users about new challenge: %s", challengeTitle)

	// Get all users with stream IDs (in batches to avoid memory issues)
	batchSize := 100
	offset := 0
	totalNotified := 0
	errors := 0

	for {
		var users []models.User
		if err := database.DB.Where("stream_user_id != '' AND stream_user_id IS NOT NULL").
			Limit(batchSize).
			Offset(offset).
			Find(&users).Error; err != nil {
			if err == gorm.ErrRecordNotFound {
				break // No more users
			}
			return fmt.Errorf("failed to fetch users: %w", err)
		}

		if len(users) == 0 {
			break // No more users
		}

		// Notify each user
		for _, user := range users {
			if err := streamClient.NotifyChallengeCreated(user.StreamUserID, challengeID, challengeTitle); err != nil {
				log.Printf("⚠️ Failed to notify user %s about new challenge: %v", user.ID, err)
				errors++
				continue
			}
			totalNotified++
		}

		offset += batchSize

		// Log progress for large batches
		if offset%1000 == 0 {
			log.Printf("📬 Notified %d users so far...", totalNotified)
		}

		// Small delay to avoid overwhelming the notification system
		time.Sleep(100 * time.Millisecond)
	}

	log.Printf("✅ Notified %d users about new challenge %s (%d errors)", totalNotified, challengeID, errors)
	return nil
}
