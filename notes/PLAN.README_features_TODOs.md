# README Features Completion Plan

> **Goal**: Complete all features promised in README.md to match actual implementation
> **Priority**: High - README accuracy is critical for user expectations
> **Last Updated**: December 6 2025

## Status Overview

Based on feature audit, the following README features need completion:

| Feature Category | Status | Completion % |
|-----------------|--------|--------------|
| 🎛️ Native DAW Integration | ✅ Mostly Complete | ~90% |
| 📱 Social Discovery | ✅ Mostly Complete | ~85% |
| ⚡ Producer-First Design | ⚠️ Partially Complete | ~60% |
| 🎹 MIDI Magic | ✅ Mostly Complete | ~85% (R.2.1 done, R.2.2 done, R.3.3 done) |
| 🔴 Live Features | ⚠️ Partially Complete | ~60% (R.3.1 done, R.3.3 done, R.3.4 done) |

---

## Phase R.1: Producer-First Design Completion

**Goal**: Complete missing "Producer-First Design" features from README
**Duration**: 1-2 weeks
**Priority**: 🔴 High - Core differentiator

### R.1.1 "Drop to Track" Feature

> **Current State**: Only file download exists (`POST /api/v1/social/listen-duration` + file chooser). Need DAW integration.
> **Target**: One-click button that adds discovered loops directly to user's DAW project.

> **Testing**: Manual testing in DAW.
> Test: Click "Drop to Track" on post → Audio file added to DAW project → Appears in project browser.
> Test: Works in Ableton Live, FL Studio, Logic Pro.

#### R.1.1.1 Backend API for Track Drop

- [x] R.1.1.1.1 Create `POST /api/v1/posts/:id/download` endpoint
  - Returns: `{ "download_url": "cdn_url", "filename": "loop_123.mp3", "metadata": {...} }`
  - Generates temporary signed URL (expires in 5 minutes)
  - Includes metadata: BPM, key, duration, genre
  - Location: `backend/internal/handlers/feed.go`

- [x] R.1.1.1.2 Add download tracking
  - Track downloads per post (for analytics)
  - Store in `audio_posts.download_count` column
  - Increment on successful download
  - Optional: Track which users downloaded (for recommendations)

- [x] R.1.1.1.3 Write backend tests
  - Test download URL generation
  - Test download count increment
  - Test authentication required
  - File: `backend/internal/handlers/feed_test.go`

#### R.1.1.2 Plugin DAW Integration

- [ ] R.1.1.2.1 Research DAW-specific APIs for adding files
  - **Ableton Live**: Live API (Python) - may require external script
  - **FL Studio**: FL Studio API (limited) - may require file system placement
  - **Logic Pro**: Logic Remote API (limited) - may require file system placement
  - **Reaper**: ReaScript API - may require external script
  - **Universal approach**: Place file in DAW's project folder, notify user

- [x] R.1.1.2.2 Implement DAW detection
  - Detect which DAW is hosting the plugin
  - Use JUCE `AudioProcessor::getHostApplicationName()`
  - Map to DAW-specific behavior
  - Store detection in `PluginProcessor` state

- [x] R.1.1.2.3 Create `DropToTrackComponent` or integrate into `PostCard`
  - Add "Drop to Track" button to `PostCardComponent`
  - Show button on hover or in actions menu
  - Button state: "Drop to Track" / "Dropped ✓"
  - Visual feedback: Animation when dropped

- [x] R.1.1.2.4 Implement file download and placement
  - Download audio file to temporary location
  - Detect DAW project folder (if accessible)
  - Copy file to project folder with descriptive name: `{username}_{title}_{bpm}bpm.mp3`
  - Show success notification: "Loop added to project folder"
  - Fallback: If project folder not accessible, use file chooser (current behavior)

- [ ] R.1.1.2.5 Add DAW-specific integration (if possible)
  - **Ableton Live**: Attempt to trigger Live API script (if user has Live API enabled)
  - **FL Studio**: Place in FL Studio's sample folder, show notification
  - **Logic Pro**: Place in Logic's project audio files folder
  - **Reaper**: Place in Reaper's project media folder
  - **Generic**: Place in user's Documents/Sidechain/Downloads folder

- [ ] R.1.1.2.6 Handle errors gracefully
  - Show error if download fails
  - Show error if project folder not accessible
  - Fallback to file chooser if DAW integration fails
  - Log errors for debugging

#### R.1.1.3 UI Integration

- [x] R.1.1.3.1 Add "Drop to Track" button to `PostCardComponent` 🆙
  - Position: Next to "Add to DAW" button or in actions menu
  - Icon: Download arrow or DAW icon
  - Tooltip: "Add to your DAW project"
  - File: `plugin/source/ui/feed/PostCard.cpp`

- [x] R.1.1.3.2 Add download progress indicator
  - Show progress bar during download
  - Show "Downloading..." text
  - Disable button during download
  - Show success checkmark when complete

- [ ] R.1.1.3.3 Add visual feedback
  - Animate button on click
  - Show toast notification: "Loop added to project!"
  - Update button state to "Dropped ✓" (persist in session)

- [ ] R.1.1.3.4 Test in multiple DAWs
  - Test in Ableton Live (Mac + Windows)
  - Test in FL Studio (Windows)
  - Test in Logic Pro (Mac)
  - Test in Reaper (cross-platform)
  - Verify file appears in project browser

### R.1.2 Smart Recommendations

> **Current State**: Basic genre filtering exists, but no intelligent recommendations.
> **Target**: Algorithm-based recommendations based on user listening history, preferences, and similar users.

> **Testing**: Manual testing with user accounts.
> Test: Listen to several electronic tracks → "For You" feed shows more electronic content.
> Test: Follow producers in hip-hop → Recommendations include similar hip-hop producers.

#### R.1.2.1 Recommendation Algorithm Backend

- [x] R.1.2.1.1 Create recommendation service
  - File: `backend/internal/recommendations/service.go`
  - Algorithm: Collaborative filtering + content-based filtering
  - Factors: Genre preferences, BPM range, key preferences, followed users, listening history
  - Note: Implemented basic recommendation system (Gorse can be added later for advanced features)

- [x] R.1.2.1.2 Implement user preference tracking
  - Track genres user listens to (from play history)
  - Track BPM range user prefers
  - Track keys user prefers
  - Track which producers user follows
  - Store in `user_preferences` table or calculate on-the-fly

- [x] R.1.2.1.3 Implement collaborative filtering
  - Find users with similar preferences
  - Recommend posts from similar users
  - Use cosine similarity or Jaccard similarity
  - Cache similarity scores (update daily)

- [x] R.1.2.1.4 Implement content-based filtering
  - Match posts by genre, BPM, key
  - Boost posts similar to user's liked posts
  - Consider post engagement (likes, plays, comments)
  - Factor in recency (newer posts weighted higher)

- [x] R.1.2.1.5 Create `GET /api/v1/recommendations/for-you` endpoint
  - Returns: Personalized feed of recommended posts
  - Pagination: Limit 20, offset
  - Ranking: Score-based (highest score first)
  - Location: `backend/internal/handlers/recommendations.go`

- [x] R.1.2.1.6 Create `GET /api/v1/recommendations/similar-posts/:post_id` endpoint
  - Returns: Posts similar to given post
  - Similarity: Genre overlap, BPM ±10, same key
  - Use case: "More like this" feature
  - Location: `backend/internal/handlers/recommendations.go`

- [x] R.1.2.1.7 Create `GET /api/v1/recommendations/similar-users/:user_id` endpoint
  - Returns: Users with similar taste
  - Similarity: Genre preferences, BPM range, followed producers
  - Use case: "Producers you might like" feature
  - Location: `backend/internal/handlers/recommendations.go`

- [x] R.1.2.1.8 Write backend tests
  - Test recommendation algorithm with sample data
  - Test cold start (new user with no history)
  - Test recommendation diversity (not all same genre)
  - Test recommendation freshness (includes recent posts)
  - File: `backend/internal/handlers/recommendations_test.go`

#### R.1.2.2 Plugin UI Integration

- [x] R.1.2.2.1 Add "For You" tab to feed 🆙
  - Add tab to `PostsFeedComponent` tabs (Timeline, Global, Trending, **For You**)
  - Fetch recommendations from `GET /api/v1/recommendations/for-you`
  - Show loading state while fetching
  - File: `plugin/source/ui/feed/PostsFeed.cpp`

- [x] R.1.2.2.2 Add "More like this" button to posts 🆙
  - Add button to `PostCardComponent` actions menu
  - Click → Navigate to recommendations view filtered by similar posts
  - Show similar posts in feed format
  - File: `plugin/source/ui/feed/PostCard.cpp`

- [x] R.1.2.2.3 Add "Similar producers" section to discovery 🆙
  - Add section to `UserDiscoveryComponent`
  - Show "Producers you might like" based on recommendations
  - Use `GET /api/v1/recommendations/similar-users/:user_id`
  - File: `plugin/source/ui/social/UserDiscovery.cpp`

- [x] R.1.2.2.4 Add recommendation explanation (optional)
  - Show why post was recommended: "Because you like Electronic music"
  - Tooltip or badge on recommended posts
  - Help users understand recommendations

---

## Phase R.2: MIDI Magic Completion

**Goal**: Complete missing "MIDI Magic" features from README
**Duration**: 2-3 weeks
**Priority**: 🟡 Medium - Fun features, not core MVP

### R.2.1 Hidden Synth Easter Egg ✅

> **Current State**: ✅ Implemented.
> **Target**: Secret synthesizer unlocked by playing specific chord sequences in the plugin.

> **Testing**: Manual testing with MIDI input.
> Test: Play C-Em-G (basic sequence) → Synth interface appears.
> Test: Play Am-F-C-G (vi-IV-I-V) → Advanced synth unlocks.
> Test: Play Dm-G-C-Am (secret sequence) → Special preset unlocks.

#### R.2.1.1 Chord Sequence Detection ✅

- [x] R.2.1.1.1 Create `ChordSequenceDetector` class
  - File: `plugin/source/audio/ChordSequenceDetector.h/cpp`
  - Detect chord sequences from MIDI input
  - Support common chord progressions (C major, Am, F, G, etc.)
  - Real-time detection (not post-processing)

- [x] R.2.1.1.2 Implement chord recognition
  - Detect note combinations (3+ notes = chord)
  - Identify chord quality (major, minor, diminished, augmented, sus2, sus4, dominant7, major7, minor7)
  - Track chord sequence over time
  - Match against known unlock sequences

- [x] R.2.1.1.3 Define unlock sequences
  - **Sequence 1**: C-Em-G (basic) → Unlocks basic synth
  - **Sequence 2**: Am-F-C-G (vi-IV-I-V) → Unlocks advanced synth
  - **Sequence 3**: Dm-G-C-Am (secret sequence) → Unlocks special preset
  - Factory methods create sequences with callbacks

- [x] R.2.1.1.4 Integrate with MIDI capture
  - Hook into `PluginProcessor::processBlock()` MIDI events
  - Analyze MIDI events in real-time
  - Detect chord sequences as user plays
  - Trigger unlock callback when sequence matched

#### R.2.1.2 Synth Interface ✅

- [x] R.2.1.2.1 Create `HiddenSynthComponent` 🆙
  - File: `plugin/source/ui/synth/HiddenSynth.h/cpp`
  - Full synthesizer interface (oscillators, filters, envelopes)
  - Use JUCE's DSP modules (State Variable Filter)
  - Dark theme matching plugin aesthetic
  - Virtual 2-octave keyboard

- [x] R.2.1.2.2 Implement synth engine
  - File: `plugin/source/audio/SynthEngine.h/cpp`
  - Oscillator types: Sine, Square, Saw, Triangle
  - Filter: Low-pass (State Variable Filter with resonance)
  - Envelope: ADSR with configurable times
  - 8-voice polyphony with voice stealing
  - Note: LFO and effects can be added in future iteration

- [x] R.2.1.2.3 Add preset system
  - Default presets: "Init", "Fat Bass", "Soft Lead", "Pluck", "Pad", "Square Lead"
  - Preset selector in UI
  - User can switch between presets
  - Note: Custom preset saving and sharing for future iteration

- [x] R.2.1.2.4 Add unlock animation
  - Sparkle particle effect animation when synth first appears
  - Particles radiate outward with gradient colors
  - Timer-based animation system
  - Clean transition to synth interface

- [x] R.2.1.2.5 Integrate with plugin navigation
  - Added `AppView::HiddenSynth` to enum
  - Navigation from unlock callback shows synth view
  - Back button returns to previous view
  - Synth unlock state persisted via `PluginProcessor::synthUnlocked`

#### R.2.1.3 Audio Processing ✅

- [x] R.2.1.3.1 Integrate synth into audio processing
  - Added synth to `PluginProcessor::processBlock()`
  - Route MIDI input to synth when enabled
  - Mix synth output with audio buffer
  - Synth enabled/disabled via `synthEnabled` atomic flag

- [x] R.2.1.3.2 Add MIDI input routing
  - MIDI from DAW routed to both chord detector and synth
  - SynthEngine processes MIDI buffer directly
  - Note on/off handled per voice
  - Velocity sensitivity implemented

- [x] R.2.1.3.3 Add audio output routing
  - Synth output added to plugin output buffer
  - Volume control via master gain knob in UI
  - Synth can be toggled on/off
  - Works as monitor output through plugin

### R.2.2 MIDI Battle Royale

> **Current State**: ✅ Backend complete, UI components implemented, navigation integrated.
> **Target**: Weekly producer challenges with creative constraints, MIDI-focused competitions.

> **Testing**: Manual testing with multiple users.
> Test: Create MIDI challenge → Users submit entries → Voting period → Winner announced.
> Test: MIDI challenge constraints enforced (e.g., "Only use C major scale").

#### R.2.2.1 MIDI Challenge System Backend

- [x] R.2.2.1.1 Create MIDI challenge data model ✅
  - File: `backend/internal/models/midi_challenge.go`
  - Fields: `id`, `title`, `description`, `constraints`, `start_date`, `end_date`, `voting_end_date`, `status`
  - Constraints: JSON field storing rules (BPM range, key, scale, max notes, etc.)

- [x] R.2.2.1.2 Create MIDI challenge entry model ✅
  - File: `backend/internal/models/midi_challenge.go` (consolidated)
  - Fields: `id`, `challenge_id`, `user_id`, `audio_url`, `midi_data`, `submitted_at`, `vote_count`
  - Link to `AudioPost` or separate storage

- [x] R.2.2.1.3 Create MIDI challenge voting model ✅
  - File: `backend/internal/models/midi_challenge.go` (consolidated)
  - Fields: `id`, `challenge_id`, `entry_id`, `voter_id`, `voted_at`
  - One vote per user per MIDI challenge

- [x] R.2.2.1.4 Create database migrations ✅
  - `midi_challenges` table
  - `midi_challenge_entries` table
  - `midi_challenge_votes` table
  - Indexes for queries

#### R.2.2.2 MIDI Challenge API Endpoints

- [x] R.2.2.2.1 Create `GET /api/v1/midi-challenges` endpoint ✅
  - Returns: List of active and past MIDI challenges
  - Filter: `status=active`, `status=past`, `status=upcoming`
  - Sort: Most recent first
  - Location: `backend/internal/handlers/midi_challenges.go`

- [x] R.2.2.2.2 Create `GET /api/v1/midi-challenges/:id` endpoint ✅
  - Returns: Full MIDI challenge details with entries
  - Include entry count, top entries, voting status
  - Location: `backend/internal/handlers/midi_challenges.go`

- [x] R.2.2.2.3 Create `POST /api/v1/midi-challenges/:id/entries` endpoint ✅
  - Submit entry to MIDI challenge
  - Validate constraints (BPM, key, MIDI rules)
  - Upload audio + MIDI data
  - One entry per user per MIDI challenge
  - Location: `backend/internal/handlers/midi_challenges.go`

- [x] R.2.2.2.4 Create `GET /api/v1/midi-challenges/:id/entries` endpoint ✅
  - Returns: All entries for MIDI challenge
  - Sort: By vote count (descending) or submission date
  - Include user info, vote count, audio preview
  - Location: `backend/internal/handlers/midi_challenges.go`

- [x] R.2.2.2.5 Create `POST /api/v1/midi-challenges/:id/entries/:entry_id/vote` endpoint ✅
  - Vote for an entry
  - One vote per user per MIDI challenge
  - Update entry vote count
  - Location: `backend/internal/handlers/midi_challenges.go`

- [x] R.2.2.2.6 Create `POST /api/v1/midi-challenges` endpoint (admin only) ✅
  - Create new MIDI challenge
  - Set constraints, dates, description
  - Admin authentication required
  - Location: `backend/internal/handlers/midi_challenges.go`

- [x] R.2.2.2.7 Write backend tests ✅
  - Test MIDI challenge creation
  - Test entry submission with constraint validation
  - Test voting system
  - Test MIDI challenge status transitions
  - File: `backend/internal/handlers/midi_challenges_test.go`

#### R.2.2.3 Constraint Validation

- [x] R.2.2.3.1 Implement BPM constraint validation ✅
  - Check if entry BPM is within allowed range
  - Reject if outside range
  - Show error message to user

- [x] R.2.2.3.2 Implement key constraint validation ✅
  - Check if entry key matches required key
  - Reject if different key
  - Show error message to user

- [x] R.2.2.3.3 Implement scale constraint validation ✅
  - Check if all MIDI notes are within allowed scale
  - Reject if notes outside scale
  - Show which notes violated constraint

- [x] R.2.2.3.4 Implement note count constraint ✅
  - Check if MIDI has max/min note count
  - Reject if outside range
  - Show error message to user

- [x] R.2.2.3.5 Implement duration constraint ✅
  - Check if audio duration is within allowed range
  - Reject if outside range
  - Show error message to user

#### R.2.2.4 Plugin UI

- [x] R.2.2.4.1 Create `MidiChallenges` Component 🆙 ✅
  - File: `plugin/source/ui/challenges/MidiChallenges.h/cpp`
  - Display active challenges
  - Show challenge details, constraints, deadline
  - Button to view entries or submit entry
  - Add to navigation (new tab or section)

- [x] R.2.2.4.2 Create `MidiChallengeDetail` Component 🆙 ✅
  - File: `plugin/source/ui/challenges/MidiChallengeDetail.h/cpp`
  - Show full challenge description
  - List all entries with vote counts
  - Play entries (audio + MIDI visualization)
  - Vote button on each entry
  - Submit entry button (if not submitted)
  - Leaderboard with top entries (ranked by votes)

- [x] R.2.2.4.3 Create `MidiChallengeSubmission` Component 🆙 ✅
  - File: `plugin/source/ui/challenges/MidiChallengeSubmission.h/cpp`
  - Record or upload audio + MIDI
  - Show constraint checklist (BPM ✓, Key ✓, etc.)
  - Validate constraints before submission
  - Submit button (disabled if constraints not met)
  - Success confirmation
  - Note: Basic implementation complete. Can be enhanced to fully integrate with Upload component flow

- [x] R.2.2.4.4 Add challenge notifications ✅
  - Notify users when new challenge starts
  - Notify users when challenge deadline approaching
  - Notify users when voting opens
  - Notify users when challenge ends (winner announced)
  - Note: UI infrastructure exists (NotificationList component). Backend needs to create notifications via getstream.io when:
    - New challenge created (notify all users)
    - Challenge deadline approaching (notify participants)
    - Voting opens (notify participants)
    - Challenge ends (notify participants with winner announcement)

- [x] R.2.2.4.5 Add challenge leaderboard ✅
  - Show top entries by vote count
  - Show user's entry rank
  - Update in real-time during voting
  - Highlight winner when challenge ends
  - Note: Implemented in MidiChallengeDetail component

---

## Phase R.3: Live Features Completion

**Goal**: Complete "Live Features (Coming Soon)" from README
**Duration**: 3-4 weeks
**Priority**: 🟡 Medium - Advanced features, can be post-MVP

### R.3.1 Collaborative Playlists

> **Current State**: Not implemented.
> **Target**: Users can create playlists and invite others to add tracks.

> **Testing**: Manual testing with multiple users.
> Test: User A creates playlist → User B adds track → Both see updated playlist.

#### R.3.1.1 Playlist Backend

- [x] R.3.1.1.1 Create playlist data model ✅
  - File: `backend/internal/models/playlist.go`
  - Fields: `id`, `name`, `description`, `owner_id`, `is_collaborative`, `is_public`, `created_at`
  - Link to `AudioPost` entries

- [x] R.3.1.1.2 Create playlist entry model ✅
  - File: `backend/internal/models/playlist.go` (consolidated)
  - Fields: `id`, `playlist_id`, `post_id`, `added_by_user_id`, `position`, `added_at`
  - Ordered list (position field)

- [x] R.3.1.1.3 Create playlist collaborator model ✅
  - File: `backend/internal/models/playlist.go` (consolidated)
  - Fields: `id`, `playlist_id`, `user_id`, `role` (owner, editor, viewer), `added_at`
  - Permissions: owner (full control), editor (add/remove), viewer (read-only)

- [x] R.3.1.1.4 Create database migrations ✅
  - `playlists` table
  - `playlist_entries` table
  - `playlist_collaborators` table
  - Indexes for queries

#### R.3.1.2 Playlist API Endpoints

- [x] R.3.1.2.1 Create `POST /api/v1/playlists` endpoint ✅
  - Create new playlist
  - Set name, description, collaborative flag
  - Returns: Playlist with ID
  - Location: `backend/internal/handlers/playlists.go`

- [x] R.3.1.2.2 Create `GET /api/v1/playlists` endpoint ✅
  - Returns: User's playlists (owned + collaborated)
  - Filter: `owned`, `collaborated`, `public`
  - Location: `backend/internal/handlers/playlists.go`

- [x] R.3.1.2.3 Create `GET /api/v1/playlists/:id` endpoint ✅
  - Returns: Full playlist with entries
  - Include post details, collaborator list
  - Check permissions (public or user has access)
  - Location: `backend/internal/handlers/playlists.go`

- [x] R.3.1.2.4 Create `POST /api/v1/playlists/:id/entries` endpoint ✅
  - Add post to playlist
  - Check permissions (owner or editor)
  - Set position (append or insert)
  - Location: `backend/internal/handlers/playlists.go`

- [x] R.3.1.2.5 Create `DELETE /api/v1/playlists/:id/entries/:entry_id` endpoint ✅
  - Remove post from playlist
  - Check permissions (owner or editor)
  - Reorder remaining entries
  - Location: `backend/internal/handlers/playlists.go`

- [x] R.3.1.2.6 Create `POST /api/v1/playlists/:id/collaborators` endpoint ✅
  - Add collaborator to playlist
  - Check permissions (owner only)
  - Set role (editor or viewer)
  - Location: `backend/internal/handlers/playlists.go`

- [x] R.3.1.2.7 Create `DELETE /api/v1/playlists/:id/collaborators/:user_id` endpoint ✅
  - Remove collaborator from playlist
  - Check permissions (owner only)
  - Location: `backend/internal/handlers/playlists.go`

- [x] R.3.1.2.8 Write backend tests ✅
  - Test playlist creation
  - Test adding/removing entries
  - Test collaborator permissions
  - Test public/private access
  - File: `backend/internal/handlers/playlists_test.go`

#### R.3.1.3 Plugin UI

- [x] R.3.1.3.1 Create `PlaylistsComponent` ✅
  - File: `plugin/source/ui/playlists/PlaylistsComponent.h/cpp`
  - List user's playlists
  - "Create Playlist" button
  - Click playlist → Open playlist detail
  - Add to navigation (new section or profile)

- [x] R.3.1.3.2 Create `PlaylistDetailComponent` ✅
  - File: `plugin/source/ui/playlists/PlaylistDetailComponent.h/cpp`
  - Show playlist name, description, collaborators
  - List entries (posts) in order
  - Play button (play all entries sequentially)
  - "Add Track" button (if user has edit permission)
  - Reorder entries (drag and drop - future enhancement)

- [x] R.3.1.3.3 Add "Add to Playlist" to post actions ✅
  - Add menu item to `PostCardComponent` actions menu
  - Show list of user's playlists
  - Select playlist → Add post to playlist
  - Show confirmation

- [x] R.3.1.3.4 Add playlist sharing ✅
  - Share playlist link (copy to clipboard)
  - Public playlists accessible via link
  - Private playlists require authentication

### R.3.2 Remix Chains

> **Current State**: Not implemented.
> **Target**: Users can remix posts, creating a chain of remixes (remix of a remix).

> **Testing**: Manual testing with multiple users.
> Test: User A posts → User B remixes → User C remixes User B's remix → Chain visible.

#### R.3.2.1 Remix Backend

- [ ] R.3.2.1.1 Add remix relationship to `AudioPost` model
  - Add `remix_of_post_id` field (nullable, foreign key to `audio_posts.id`)
  - Add `remix_chain_depth` field (0 = original, 1 = remix, 2 = remix of remix, etc.)
  - Migration: `ALTER TABLE audio_posts ADD COLUMN remix_of_post_id UUID REFERENCES audio_posts(id)`

- [ ] R.3.2.1.2 Create remix chain query
  - Function to get full remix chain (all remixes of a post)
  - Function to get original post (traverse up chain)
  - Function to get remix count (how many remixes of this post)

- [ ] R.3.2.1.3 Update `CreatePost` endpoint
  - Accept `remix_of_post_id` in request body
  - Validate post exists and is remixable
  - Set `remix_chain_depth` (parent depth + 1)
  - Limit chain depth (max 5 levels to prevent infinite chains)

- [ ] R.3.2.1.4 Create `GET /api/v1/posts/:id/remix-chain` endpoint
  - Returns: Full remix chain (original + all remixes)
  - Ordered by chain depth
  - Include remix count for each post
  - Location: `backend/internal/handlers/feed.go`

- [ ] R.3.2.1.5 Create `GET /api/v1/posts/:id/remixes` endpoint
  - Returns: Direct remixes of this post (one level deep)
  - Paginated
  - Sorted by creation date or popularity
  - Location: `backend/internal/handlers/feed.go`

- [ ] R.3.2.1.6 Write backend tests
  - Test remix creation
  - Test remix chain traversal
  - Test remix depth limit
  - Test remix count calculation
  - File: `backend/internal/handlers/feed_test.go`

#### R.3.2.2 Plugin UI

- [ ] R.3.2.2.1 Add "Remix" button to posts 🆙
  - Add button to `PostCardComponent` actions menu
  - Click → Opens remix recording interface
  - Show remix count badge on post
  - File: `plugin/source/ui/feed/PostCard.cpp`

- [ ] R.3.2.2.2 Create remix recording interface 🆙
  - Reuse `RecordingComponent` with remix context
  - Show original post info (title, BPM, key)
  - Show remix chain depth indicator
  - Record remix (audio + MIDI)
  - Upload with `remix_of_post_id` set

- [ ] R.3.2.2.3 Add remix chain visualization 🆙
  - Show remix chain in post detail view
  - Visual tree/graph showing remix relationships
  - Click remix → Navigate to that post
  - Show "Original" badge on original post

- [ ] R.3.2.2.4 Add remix badge to posts
  - Show "Remix" badge on remixed posts
  - Show remix count (how many remixes of this post)
  - Click badge → Show remix chain

### R.3.3 Cross-DAW MIDI Collaboration

> **Current State**: ✅ Backend and UI complete. DAW project folder detection implemented for MIDI downloads.
> **Target**: MIDI as a first-class resource that users can share, download, and import into their DAW.

> **Design Decision**: MIDI is a standalone primitive (`/api/v1/midi/:id`), not tied to posts.
> This allows MIDI to be shared independently, reused across posts/stories/challenges, and
> enables a future MIDI library feature. Posts and Stories can optionally *reference* a MIDI ID.

> **Testing**: Manual testing with multiple DAWs.
> Test: User A uploads MIDI → Gets shareable link → User B downloads .mid file → Imports into DAW.
> Test: User A records post with MIDI → MIDI auto-uploaded → Available for download on post.

#### R.3.3.1 MIDI Data Model (Consolidation) ✅

> **Current State**: ✅ Implemented. MIDI types consolidated in `models/midi.go`.
> `MIDIPattern` is the first-class database entity. Story and AudioPost reference via FK.

- [x] R.3.3.1.1 Create `backend/internal/models/midi.go`
  - Created `MIDIEvent`, `MIDIData`, and `MIDIPattern` in consolidated file
  - Includes validation methods for all MIDI types
  - Custom `TableName()` method ensures correct table name `midi_patterns`

- [x] R.3.3.1.2 Update Story model to reference MIDIPattern
  - Added `MIDIPatternID *string` foreign key field
  - Added `MIDIPattern *MIDIPattern` relationship
  - Kept legacy `MIDIData *MIDIData` for backwards compatibility

- [x] R.3.3.1.3 Add optional MIDI reference to AudioPost
  - Added `MIDIPatternID *string` field (nullable UUID, foreign key)
  - Added `MIDIPattern *MIDIPattern` relationship

- [x] R.3.3.1.4 Create database migration
  - Added `MIDIPattern` to GORM AutoMigrate in `database.go`
  - Added indexes for `user_id`, `is_public`, `created_at`
  - Migration runs via AutoMigrate (GORM managed)

- [x] R.3.3.1.5 Update imports across backend
  - All handlers import from consolidated `models` package
  - Story and AudioPost handlers use FK references

#### R.3.3.2 MIDI CRUD API Endpoints ✅

- [x] R.3.3.2.1 Create `POST /api/v1/midi` endpoint
  - Implemented in `backend/internal/handlers/midi.go`
  - Validates MIDI data (events, tempo > 0, time signature)
  - Returns created MIDI pattern with ID

- [x] R.3.3.2.2 Create `GET /api/v1/midi/:id` endpoint
  - Returns MIDI metadata and events as JSON
  - Checks permissions: public patterns or owner only

- [x] R.3.3.2.3 Create `GET /api/v1/midi/:id/file` endpoint
  - Returns Standard MIDI File (.mid) binary
  - Content-Type: `audio/midi`
  - Content-Disposition with filename
  - Increments download_count

- [x] R.3.3.2.4 Create `DELETE /api/v1/midi/:id` endpoint
  - Soft delete via GORM
  - Owner only (verified via user_id check)

- [x] R.3.3.2.5 Create `GET /api/v1/midi` endpoint (list user's MIDI)
  - Returns paginated list of user's MIDI patterns
  - Supports limit/offset pagination

- [x] R.3.3.2.6 Create `PATCH /api/v1/midi/:id` endpoint
  - Update pattern name, description, visibility
  - Owner only

#### R.3.3.3 MIDI File Generation ✅

- [x] R.3.3.3.1 Create MIDI file generator utility
  - File: `backend/internal/handlers/midi_generator.go`
  - Function: `GenerateMIDIFile(data *models.MIDIData, name string) ([]byte, error)`
  - Uses `gitlab.com/gomidi/midi/v2/smf` library
  - Sets tempo, time signature, track name
  - Writes note on/off events with proper delta times
  - Supports multi-channel → multi-track files

- [x] R.3.3.3.2 Create MIDI file parser utility
  - Function: `ParseMIDIFile(data []byte) (*models.MIDIData, error)`
  - Parses Standard MIDI File
  - Extracts tempo, time signature
  - Converts note events to MIDIEvent structs
  - Handles multi-track files

- [x] R.3.3.3.3 Write unit tests for MIDI utilities
  - TestMIDIFileRoundTrip: JSON → .mid → parse back → verify data matches
  - Tests tempo, time signature, events preservation
  - File: `backend/internal/handlers/midi_test.go`

#### R.3.3.4 Backend Integration ✅

- [x] R.3.3.4.1 Update CreatePost to accept MIDI ✅
  - Add optional `midi_data` field to create post request
  - If provided: Create MIDI record, link to post
  - Or accept `midi_id` to link existing MIDI
  - File: `backend/internal/handlers/feed.go`

- [x] R.3.3.4.2 Update CreateStory to create standalone MIDI ✅
  - When Story has MIDIData, also create midi_patterns record
  - Link via midi_id
  - Keeps embedded data for backwards compat
  - File: `backend/internal/handlers/stories.go`

- [x] R.3.3.4.3 Update feed responses to include MIDI info ✅
  - Add `has_midi: true` and `midi_id: "..."` to post responses
  - Plugin uses this to show "Download MIDI" button
  - Implemented in `backend/internal/stream/client.go` (lines 1371-1380)

- [x] R.3.3.4.4 Write handler tests
  - 14 tests covering all MIDI CRUD operations
  - Tests: Create, Get, GetFile, List, Delete, Update
  - Tests: PrivateAccess, NotFound, NotOwner, InvalidTempo, InvalidTimeSignature
  - Tests: MIDIFileRoundTrip
  - All tests passing
  - File: `backend/internal/handlers/midi_test.go`

#### R.3.3.5 Plugin UI - MIDI Download ✅

- [x] R.3.3.5.1 Add `hasMidi` and `midiId` to FeedPost model
  - Added to `plugin/source/models/FeedPost.h`
  - Parses from feed JSON in `FeedPost.cpp`
  - `bool hasMidi = false;`
  - `juce::String midiId;`

- [x] R.3.3.5.2 Add "Download MIDI" button to PostCard
  - Shows only when `post.hasMidi == true`
  - Position: Above "Drop to Track" button (on hover)
  - Uses keyboard emoji icon "🎹 MIDI"
  - File: `plugin/source/ui/feed/PostCard.cpp`

- [x] R.3.3.5.3 Implement MIDI download in NetworkClient
  - Added `downloadMIDI(midiId, targetFile, callback)` method
  - Calls `GET /api/v1/midi/:id/file`
  - Saves binary .mid file to disk
  - File: `plugin/source/network/NetworkClient.cpp`

- [x] R.3.3.5.4 Implement download flow in PostsFeed
  - Click "Download MIDI" → Downloads to ~/Documents/Sidechain/MIDI/
  - Filename: `{username}_{midiId}.mid`
  - Shows success/error AlertWindow
  - File: `plugin/source/ui/feed/PostsFeed.cpp`

- [x] R.3.3.5.5 Add MIDI to Story viewer ✅
  - Stories with MIDI show download button
  - Same download flow as posts
  - File: `plugin/source/ui/stories/StoryViewer.cpp`

#### R.3.3.6 Plugin UI - MIDI Upload ✅

- [x] R.3.3.6.1 Capture MIDI during recording ✅
  - MIDICapture already captures MIDI events
  - Package as MIDIData JSON when recording completes
  - Include tempo from DAW if available

- [x] R.3.3.6.2 Update Upload component to include MIDI ✅
  - If MIDI was captured during recording, include in upload
  - Show MIDI preview (note count, duration)
  - Toggle: "Include MIDI" (default on if captured)
  - File: `plugin/source/ui/recording/Upload.cpp`

- [x] R.3.3.6.3 Add MIDI import option in recording ✅
  - "Import MIDI" button in Recording view
  - File chooser for .mid files
  - Parse and display imported MIDI
  - Use imported MIDI instead of captured
  - File: `plugin/source/ui/recording/Recording.cpp`

#### R.3.3.7 DAW Integration (Advanced)

- [x] R.3.3.7.1 Detect DAW project folder ✅
  - Reuse DAW detection from R.1.1.2.2
  - Map DAW → typical project folder structure
  - Ableton: Project folder / Samples / Imported / MIDI Files
  - FL Studio: Project folder
  - Logic: Project folder / MIDI Files (or Audio Files)
  - REAPER: Project folder
  - Cubase: Project folder / Audio
  - Studio One: Project folder
  - Pro Tools: Project folder / MIDI Files
  - File: `plugin/source/util/DAWProjectFolder.h/cpp`

- [x] R.3.3.7.2 Place MIDI file in DAW project ✅
  - Option 1: Save to DAW project folder (if detected and accessible)
  - Option 2: Save to user's Documents/Sidechain/MIDI folder (fallback)
  - Show notification with file location and DAW-specific message
  - User can drag-drop into DAW
  - Updated in `PostsFeed.cpp` and `StoryViewer.cpp`

- [x] R.3.3.7.3 (Future) Deep DAW integration - **Research Complete** ✅
  - ✅ Research: Ableton Link for tempo sync - **Documented in `notes/DAW_INTEGRATION_RESEARCH.md`**
    - Finding: VST plugins cannot control host DAW tempo (VST limitation)
    - Plugin can sync internal tempo to Link sessions
    - Recommendation: Low priority, limited value for VST plugins
  - ✅ Research: VST3 MIDI output to DAW - **Documented in `notes/DAW_INTEGRATION_RESEARCH.md`**
    - Finding: Supported in Ableton Live 11+, Cubase 2020+, REAPER 2020+, Studio One 6+
    - Not supported in Logic Pro (uses AU format)
    - Recommendation: Medium priority, highest value for workflow integration
    - Implementation: Enable MIDI output bus, route MIDI events in `processBlock()`
  - ✅ Research: ReaScript for Reaper automation - **Documented in `notes/DAW_INTEGRATION_RESEARCH.md`**
    - Finding: Requires external script files (can't execute from plugin)
    - File-based IPC communication possible
    - Recommendation: Low-Medium priority, REAPER-specific
    - Implementation: Create ReaScript library, file-based command protocol
  - 📄 **Full research document**: See `notes/DAW_INTEGRATION_RESEARCH.md` for detailed findings, implementation steps, and priority recommendations
  - **Status**: Research phase complete. Implementation deferred to post-MVP based on priority ranking.

### R.3.4 Project File Exchange ✅

> **Current State**: ✅ Implemented. Backend and plugin UI complete.
> **Target**: Users can share DAW project files (Ableton Live sets, FL Studio projects, etc.).

> **Testing**: Manual testing with multiple DAWs.
> Test: User A exports project → User B downloads → Opens in DAW → Project loads.

#### R.3.4.1 Project File Backend ✅

- [x] R.3.4.1.1 Create project file data model ✅
  - File: `backend/internal/models/project_file.go`
  - Fields: `id`, `user_id`, `audio_post_id` (optional), `filename`, `file_url`, `daw_type`, `file_size`, `is_public`, `download_count`, `created_at`
  - DAW type auto-detection from filename
  - Link to `AudioPost` (if shared with post)

- [x] R.3.4.1.2 Create database migration ✅
  - `project_files` table with GORM AutoMigrate
  - Indexes for `user_id`, `daw_type`, `audio_post_id`

- [x] R.3.4.1.3 Create `POST /api/v1/project-files` endpoint ✅
  - Accept project file metadata and CDN URL
  - Validate file type (whitelist allowed extensions via daw_type)
  - Validate file size (max 50MB)
  - Returns: Project file with ID
  - Location: `backend/internal/handlers/project_files.go`

- [x] R.3.4.1.4 Create `GET /api/v1/project-files/:id` endpoint ✅
  - Returns: Project file metadata
  - Location: `backend/internal/handlers/project_files.go`

- [x] R.3.4.1.5 Create `GET /api/v1/project-files/:id/download` endpoint ✅
  - Redirects to CDN URL
  - Increments download_count
  - Location: `backend/internal/handlers/project_files.go`

- [x] R.3.4.1.6 Create `GET /api/v1/posts/:id/project-file` endpoint ✅
  - Returns: Project file associated with post (if exists)
  - Location: `backend/internal/handlers/project_files.go`

- [x] R.3.4.1.7 Create `GET /api/v1/project-files` endpoint ✅
  - Returns: List of user's project files
  - Pagination support
  - Location: `backend/internal/handlers/project_files.go`

- [x] R.3.4.1.8 Create `DELETE /api/v1/project-files/:id` endpoint ✅
  - Soft delete project file
  - Owner only
  - Location: `backend/internal/handlers/project_files.go`

- [x] R.3.4.1.9 Write backend tests ✅
  - Test project file CRUD operations
  - Test file type validation
  - Test file size limits
  - Test download URL generation
  - Test permission checks
  - File: `backend/internal/handlers/project_files_test.go`

#### R.3.4.2 Plugin UI ✅

- [x] R.3.4.2.1 Add "Share Project File" to upload flow ✅
  - Added project file selection to `Upload` component
  - File chooser for .als, .flp, .logicx, .rpp, .cpr, .song, .bwproject, .ptx files
  - Show project file size and type in UI
  - Upload project file with audio post
  - File: `plugin/source/ui/recording/Upload.cpp`

- [x] R.3.4.2.2 Add "Download Project File" to posts ✅
  - Added button to `PostCardComponent` (shows on hover when project file available)
  - Only shows if post has project file (`hasProjectFile` field)
  - Download project file to ~/Documents/Sidechain/Projects/
  - Show success notification with file path
  - File: `plugin/source/ui/feed/PostCard.cpp`

- [x] R.3.4.2.3 Add project file upload to NetworkClient ✅
  - Added `uploadProjectFile()` method
  - DAW type auto-detection from file extension
  - File size validation (50MB max)
  - Project file uploaded after audio post creation, linked via `audio_post_id`
  - File: `plugin/source/network/NetworkClient.cpp`

- [x] R.3.4.2.4 Add project file download to NetworkClient ✅
  - Added `downloadProjectFile()` method
  - Calls `/api/v1/project-files/:id/download` endpoint
  - Saves to target file with proper extension based on DAW type
  - File: `plugin/source/network/NetworkClient.cpp`

- [x] R.3.4.2.5 Add project file info to FeedPost model ✅
  - Added `hasProjectFile`, `projectFileId`, `projectFileDaw` fields
  - Parsed from feed JSON response
  - File: `plugin/source/models/FeedPost.h/cpp`

**Note**: DAW compatibility warnings (R.3.4.2.4 from original plan) can be added in future iteration when DAW detection is fully implemented

---

## Testing Strategy

### Manual Testing Checklist

**Drop to Track:**
- [ ] Click "Drop to Track" on post → File appears in DAW project folder
- [ ] Test in Ableton Live → File appears in project browser
- [ ] Test in FL Studio → File appears in project samples
- [ ] Test in Logic Pro → File appears in project audio files
- [ ] Test error handling (no project folder access) → Falls back to file chooser

**Smart Recommendations:**
- [ ] Listen to 5+ electronic tracks → "For You" shows electronic content
- [ ] Follow hip-hop producers → Recommendations include hip-hop
- [ ] Click "More like this" → Shows similar posts
- [ ] New user (no history) → Shows trending/popular content

**Hidden Synth:**
- [ ] Play C-E-G-C sequence → Synth unlocks
- [ ] Play Am-F-C-G sequence → Advanced synth unlocks
- [ ] Synth interface appears and works
- [ ] MIDI input routes to synth
- [ ] Synth output plays through DAW

**MIDI Battle Royale:**
- [ ] Create challenge → Appears in challenges list
- [ ] Submit entry → Constraints validated
- [ ] Vote on entries → Vote count updates
- [ ] Challenge ends → Winner announced
- [ ] View challenge leaderboard → Top entries shown

**Collaborative Playlists:**
- [ ] Create playlist → Appears in playlists list
- [ ] Add track to playlist → Track appears in playlist
- [ ] Add collaborator → Collaborator can edit
- [ ] Play playlist → All tracks play sequentially

**Remix Chains:**
- [ ] Remix a post → Remix appears in chain
- [ ] Remix a remix → Chain depth increases
- [ ] View remix chain → All remixes shown
- [ ] Remix count updates correctly

**Cross-DAW MIDI:**
- [ ] Download MIDI from post → .mid file saved
- [ ] Import .mid into DAW → MIDI appears in DAW
- [ ] Test with Ableton Live → MIDI imports correctly
- [ ] Test with FL Studio → MIDI imports correctly

**Project File Exchange:**
- [ ] Upload project file with post → File attached
- [ ] Download project file → File saved to Downloads
- [ ] Open project in DAW → Project loads correctly
- [ ] Test compatibility warnings → Shows for different DAW

---

## Implementation Priority

### Phase 1 (MVP Completion) - 2-3 weeks
1. **R.1.1 Drop to Track** - Core differentiator, high user value
2. **R.1.2 Smart Recommendations** - Improves discovery, increases engagement

### Phase 2 (Engagement Features) - 2-3 weeks
3. **R.2.1 Hidden Synth Easter Egg** - Fun feature, increases retention
4. **R.2.2 MIDI Battle Royale** - Community building, gamification

### Phase 3 (Advanced Collaboration) - 3-4 weeks
5. **R.3.1 Collaborative Playlists** - Social feature, increases sharing
6. **R.3.2 Remix Chains** - Creative feature, increases engagement
7. **R.3.3 Cross-DAW MIDI** - Technical feature, enables workflow
8. **R.3.4 Project File Exchange** - Advanced feature, power users

---

## Success Criteria

- [ ] All README features implemented and testable
- [ ] Features match README descriptions
- [ ] Features work across supported DAWs (Ableton, FL Studio, Logic Pro)
- [ ] Features have proper error handling and user feedback
- [ ] Features are documented in user guide
- [ ] Features have backend test coverage (>60%)
- [ ] Features have plugin unit tests where applicable

---

*This plan complements the main development plan in `notes/PLAN.md`. Update as features are completed.*

