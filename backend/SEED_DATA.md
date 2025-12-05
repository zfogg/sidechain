# Seed Data Documentation

## Overview

The seed data system generates realistic test data for development and testing. It uses `gofakeit` to create believable user profiles, audio posts, comments, and social interactions.

## Usage

### Seed Development Database

```bash
# Using Make
make seed

# Or directly
go run ./cmd/seed/main.go dev
```

This creates:
- **20 users** with realistic profiles, genres, DAW preferences
- **50 audio posts** with BPM, key, genre metadata
- **100 comments** on posts
- **Hashtags** linked to posts
- **200 play history** records
- **User preferences** based on listening patterns
- **15 device** records

### Seed Test Database

```bash
make seed-test
# or
go run ./cmd/seed/main.go test
```

Creates minimal data for testing (3 users, 5 posts, 10 comments).

### Clean Seed Data

```bash
make seed-clean
# or
go run ./cmd/seed/main.go clean
```

⚠️ **Warning**: This removes all seed data from the database!

## Verification

### Verify Seed Data

```bash
go run scripts/verify-seed.go
```

Shows:
- Record counts for all tables
- Sample data preview
- Relationship verification
- Sample IDs for API testing

### Test API Endpoints

```bash
# Start the server first
make dev

# In another terminal, test the API
./scripts/test-api.sh
```

## Default Credentials

All seeded users have the password: **`password123`**

You can login with any seeded user's email address using this password.

## Seed Data Details

### Users
- Realistic usernames, emails, display names
- Bio, location, avatar URLs
- Genre preferences (1-3 genres per user)
- DAW preferences
- Random follower/following counts
- Email verified by default

### Audio Posts
- Random BPM (100-180)
- Musical keys (C, C#, D, etc., major and minor)
- Genres (house, techno, dubstep, etc.)
- Duration (4-32 seconds, typical loop length)
- Waveform SVG placeholders
- Random like/play counts
- Processing status: "complete"

### Comments
- Mix of template comments and random sentences
- Some comments marked as edited
- Random like counts

### Hashtags
- 23 popular music production hashtags
- Linked to posts (1-3 hashtags per post)
- Post counts tracked

### Play History
- Tracks user listening behavior
- Duration listened (0 to full duration)
- Completion status (90%+ = completed)

### User Preferences
- Genre weights (normalized 0-1)
- BPM range preferences
- Key preferences (most listened keys)

### Devices
- Platform (macOS, Windows, Linux)
- DAW and version
- Device fingerprints
- Last used timestamps

## Idempotency

The seed command is **idempotent** - it's safe to run multiple times:
- Checks for existing users before creating new ones
- Skips creation if data already exists
- Updates counts and relationships correctly

## Customization

Edit `internal/seed/seeder.go` to:
- Change quantities (user count, post count, etc.)
- Modify data generation logic
- Add new seed scenarios
- Adjust random distributions

## Troubleshooting

### "relation does not exist" errors
Run migrations first:
```bash
make migrate
```

### "column is of type text[] but expression is of type record"
This was fixed by updating the `UserPreference.KeyPreferences` field to use `StringArray` type. If you see this, run migrations again.

### Seed data not appearing in API
1. Ensure server is running: `make dev`
2. Login with seeded user credentials
3. Check that getstream.io integration is configured (for feed endpoints)
