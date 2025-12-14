# Sidechain Admin CLI Tool

> **Vision**: A powerful command-line tool for Sidechain development, maintenance, and moderation
> **Location**: `backend/cmd/admin/main.go`
> **Language**: Go (shares code with backend)
> **Last Updated**: December 13, 2025

---

## Executive Summary

The Sidechain Admin CLI (`sidechain-admin`) is a Go command-line application that provides developers and administrators with powerful tools for:
- User and content management
- Moderation and report handling
- Analytics and reporting
- System health monitoring
- Development and debugging utilities

The CLI uses the same database and services as the backend, providing direct access to all data and operations.

---

## Priority Legend

- **P0 - Critical**: Essential for daily operations and incident response
- **P1 - High**: Important for ongoing maintenance and moderation
- **P2 - Medium**: Useful for analytics and development
- **P3 - Low**: Nice-to-have utilities and automation

---

## Architecture

### Project Structure
```
backend/cmd/admin/
├── main.go                 # Entry point, command registration
├── commands/
│   ├── users.go            # User management commands
│   ├── content.go          # Content management commands
│   ├── moderation.go       # Moderation commands
│   ├── analytics.go        # Analytics commands
│   ├── system.go           # System health commands
│   ├── dev.go              # Development utilities
│   ├── challenges.go       # MIDI challenge management
│   └── notifications.go    # Notification commands
├── output/
│   ├── table.go            # Table formatting
│   ├── json.go             # JSON output
│   └── colors.go           # Terminal colors
└── config/
    └── config.go           # CLI configuration
```

### Dependencies
- `github.com/spf13/cobra` - CLI framework
- `github.com/olekukonko/tablewriter` - Table output
- `github.com/fatih/color` - Colored output
- Shared: `internal/database`, `internal/models`, `internal/stream`, etc.

---

## P0: Critical - Daily Operations

### 1. CLI Framework Setup

**TODO**:
- [ ] **1.1** Create `backend/cmd/admin/main.go`
  - Initialize Cobra root command
  - Setup database connection (reuse `internal/database`)
  - Setup Stream.io client
  - Global flags: `--config`, `--json`, `--verbose`, `--dry-run`
  - Environment variable support (same as backend .env)

- [ ] **1.2** Create configuration loader
  - Load from `.env` or environment variables
  - Support config file override
  - Validate required credentials

- [ ] **1.3** Create output formatters
  - Table output (default, human-readable)
  - JSON output (for scripting/piping)
  - Colored terminal output
  - Progress indicators for long operations

### 2. User Management Commands

**Commands**: `sidechain-admin users <subcommand>`

**TODO**:
- [ ] **2.1** `users list` - List users with filters
  ```bash
  sidechain-admin users list [flags]
    --limit N           # Number of results (default 20)
    --offset N          # Pagination offset
    --sort FIELD        # Sort by: created_at, followers, posts (default: created_at)
    --order ASC|DESC    # Sort order (default: DESC)
    --verified          # Only verified users
    --private           # Only private accounts
    --has-2fa           # Only users with 2FA enabled
    --since DATE        # Registered since date
    --genre GENRE       # Filter by genre preference
    --daw DAW           # Filter by DAW preference
  ```

- [ ] **2.2** `users get <user_id|email|username>` - Get user details
  ```bash
  sidechain-admin users get john@example.com
  sidechain-admin users get @johndoe
  sidechain-admin users get 123e4567-e89b-12d3-a456-426614174000
  ```
  Output: Profile info, stats, activity, devices, 2FA status

- [ ] **2.3** `users search <query>` - Search users
  ```bash
  sidechain-admin users search "john" --fields email,username,display_name
  ```

- [ ] **2.4** `users reset-password <user_id>` - Generate password reset
  ```bash
  sidechain-admin users reset-password @johndoe
  # Outputs reset token/link
  ```

- [ ] **2.5** `users verify-email <user_id>` - Manually verify email
  ```bash
  sidechain-admin users verify-email @johndoe
  ```

- [ ] **2.6** `users disable-2fa <user_id>` - Disable 2FA (emergency)
  ```bash
  sidechain-admin users disable-2fa @johndoe --reason "User locked out"
  ```

- [ ] **2.7** `users set-verified <user_id>` - Set verified badge
  ```bash
  sidechain-admin users set-verified @johndoe --verified=true
  ```

- [ ] **2.8** `users suspend <user_id>` - Suspend user account
  ```bash
  sidechain-admin users suspend @johndoe --reason "ToS violation" --duration 7d
  sidechain-admin users unsuspend @johndoe
  ```

- [ ] **2.9** `users export <user_id>` - Export user data (GDPR)
  ```bash
  sidechain-admin users export @johndoe --output user_data.json
  # Exports: profile, posts, comments, messages, follows, etc.
  ```

- [ ] **2.10** `users devices <user_id>` - List user's devices/sessions
  ```bash
  sidechain-admin users devices @johndoe
  # Shows: device fingerprint, platform, DAW, last used
  ```

### 3. Content Management Commands

**Commands**: `sidechain-admin content <subcommand>`

**TODO**:
- [ ] **3.1** `content posts list` - List posts with filters
  ```bash
  sidechain-admin content posts list [flags]
    --user USER_ID      # Filter by user
    --since DATE        # Posts since date
    --until DATE        # Posts until date
    --genre GENRE       # Filter by genre
    --bpm-min N         # Minimum BPM
    --bpm-max N         # Maximum BPM
    --key KEY           # Filter by key
    --archived          # Include archived posts
    --has-midi          # Only posts with MIDI
    --has-project-file  # Only posts with project files
    --sort FIELD        # Sort by: created_at, likes, plays, comments
  ```

- [ ] **3.2** `content posts get <post_id>` - Get post details
  ```bash
  sidechain-admin content posts get abc123
  # Shows: metadata, engagement stats, comments, reactions, remix chain
  ```

- [ ] **3.3** `content posts delete <post_id>` - Delete post
  ```bash
  sidechain-admin content posts delete abc123 --reason "Copyright violation"
  # Logs action for audit trail
  ```

- [ ] **3.4** `content posts archive <post_id>` - Archive/unarchive post
  ```bash
  sidechain-admin content posts archive abc123
  sidechain-admin content posts unarchive abc123
  ```

- [ ] **3.5** `content posts feature <post_id>` - Feature post
  ```bash
  sidechain-admin content posts feature abc123 --duration 24h
  # Boosts in trending/discovery
  ```

- [ ] **3.6** `content comments list` - List comments
  ```bash
  sidechain-admin content comments list --post POST_ID --user USER_ID
  ```

- [ ] **3.7** `content comments delete <comment_id>` - Delete comment
  ```bash
  sidechain-admin content comments delete abc123 --reason "Harassment"
  ```

- [ ] **3.8** `content stories list` - List active stories
  ```bash
  sidechain-admin content stories list --user USER_ID --expired
  ```

- [ ] **3.9** `content bulk-delete` - Bulk delete content
  ```bash
  sidechain-admin content bulk-delete --user USER_ID --type posts --confirm
  # Deletes all posts by user (for account cleanup)
  ```

### 4. Moderation Commands

**Commands**: `sidechain-admin moderation <subcommand>`

**TODO**:
- [ ] **4.1** `moderation reports list` - List pending reports
  ```bash
  sidechain-admin moderation reports list [flags]
    --status pending|reviewing|resolved|dismissed
    --type post|comment|user
    --reason spam|harassment|copyright|inappropriate|violence|other
    --since DATE
    --limit N
  ```

- [ ] **4.2** `moderation reports get <report_id>` - Get report details
  ```bash
  sidechain-admin moderation reports get abc123
  # Shows: reporter, target content, reason, description, history
  ```

- [ ] **4.3** `moderation reports action <report_id>` - Take action
  ```bash
  sidechain-admin moderation reports action abc123 \
    --action dismiss|warn|remove|ban \
    --note "Reviewed, content is acceptable"
  ```

- [ ] **4.4** `moderation history` - View moderation history
  ```bash
  sidechain-admin moderation history --moderator ADMIN_ID --since DATE
  # Audit trail of all moderation actions
  ```

- [ ] **4.5** `moderation bans list` - List banned users
  ```bash
  sidechain-admin moderation bans list --active
  ```

- [ ] **4.6** `moderation bans add <user_id>` - Ban user
  ```bash
  sidechain-admin moderation bans add @johndoe \
    --reason "Repeated ToS violations" \
    --duration permanent|7d|30d
  ```

- [ ] **4.7** `moderation bans remove <user_id>` - Unban user
  ```bash
  sidechain-admin moderation bans remove @johndoe --note "Appeal approved"
  ```

---

## P1: High Priority - Maintenance & Monitoring

### 5. Analytics Commands

**Commands**: `sidechain-admin analytics <subcommand>`

**TODO**:
- [ ] **5.1** `analytics users` - User growth stats
  ```bash
  sidechain-admin analytics users --period day|week|month|all
  # Output: signups per period, total users, active users (DAU/WAU/MAU)
  ```

- [ ] **5.2** `analytics engagement` - Engagement metrics
  ```bash
  sidechain-admin analytics engagement --period week
  # Output: posts, likes, comments, plays, follows per period
  ```

- [ ] **5.3** `analytics top-users` - Top users by metric
  ```bash
  sidechain-admin analytics top-users \
    --metric followers|posts|likes|plays \
    --limit 20
  ```

- [ ] **5.4** `analytics top-posts` - Top posts by engagement
  ```bash
  sidechain-admin analytics top-posts \
    --metric likes|plays|comments|downloads \
    --period week \
    --limit 20
  ```

- [ ] **5.5** `analytics distribution` - Content distribution
  ```bash
  sidechain-admin analytics distribution --type genre|bpm|key|daw
  # Shows histogram of content attributes
  ```

- [ ] **5.6** `analytics retention` - User retention metrics
  ```bash
  sidechain-admin analytics retention --cohort 2025-01
  # Shows D1, D7, D30 retention for cohort
  ```

- [ ] **5.7** `analytics export` - Export analytics data
  ```bash
  sidechain-admin analytics export \
    --type users|posts|engagement \
    --since DATE --until DATE \
    --format csv|json \
    --output report.csv
  ```

### 6. System Health Commands

**Commands**: `sidechain-admin system <subcommand>`

**TODO**:
- [ ] **6.1** `system health` - Overall health check
  ```bash
  sidechain-admin system health
  # Checks: database, S3, Stream.io, Gorse, SES, WebSocket
  # Output: GREEN/YELLOW/RED status for each
  ```

- [ ] **6.2** `system database` - Database stats
  ```bash
  sidechain-admin system database
  # Shows: table sizes, connection count, slow queries
  ```

- [ ] **6.3** `system queue` - Audio processing queue status
  ```bash
  sidechain-admin system queue
  # Shows: pending jobs, processing jobs, failed jobs, avg processing time
  ```

- [ ] **6.4** `system websocket` - WebSocket metrics
  ```bash
  sidechain-admin system websocket
  # Shows: active connections, messages/sec, online users
  ```

- [ ] **6.5** `system stream` - Stream.io integration health
  ```bash
  sidechain-admin system stream
  # Shows: API status, feed counts, recent errors
  ```

- [ ] **6.6** `system gorse` - Gorse recommendations health
  ```bash
  sidechain-admin system gorse
  # Shows: API status, model freshness, recommendation counts
  ```

- [ ] **6.7** `system storage` - S3/CDN stats
  ```bash
  sidechain-admin system storage
  # Shows: bucket size, object count, recent uploads
  ```

- [ ] **6.8** `system logs` - View recent logs
  ```bash
  sidechain-admin system logs --level error --since 1h --service audio-worker
  ```

---

## P2: Medium Priority - Development & Automation

### 7. Development Commands

**Commands**: `sidechain-admin dev <subcommand>`

**TODO**:
- [ ] **7.1** `dev seed` - Seed database with test data
  ```bash
  sidechain-admin dev seed \
    --users 100 \
    --posts-per-user 5 \
    --comments-per-post 3 \
    --follows-per-user 20
  # Uses faker library for realistic data
  ```

- [ ] **7.2** `dev cleanup` - Clean up test data
  ```bash
  sidechain-admin dev cleanup --prefix test_ --confirm
  # Removes users/content with test_ prefix
  ```

- [ ] **7.3** `dev migrate` - Run database migrations
  ```bash
  sidechain-admin dev migrate up
  sidechain-admin dev migrate down 1
  sidechain-admin dev migrate status
  ```

- [ ] **7.4** `dev cache-clear` - Clear caches
  ```bash
  sidechain-admin dev cache-clear --type stream|gorse|all
  ```

- [ ] **7.5** `dev reindex` - Reindex data
  ```bash
  sidechain-admin dev reindex --type users|posts|all
  # Reindex in Stream.io/Gorse
  ```

- [ ] **7.6** `dev sync-counts` - Sync cached counts
  ```bash
  sidechain-admin dev sync-counts --type followers|likes|plays
  # Recalculates cached counts from actual data
  ```

- [ ] **7.7** `dev test-email` - Test email sending
  ```bash
  sidechain-admin dev test-email recipient@example.com
  # Sends test email via SES
  ```

- [ ] **7.8** `dev token` - Generate auth token
  ```bash
  sidechain-admin dev token --user USER_ID --expires 24h
  # Generates JWT for testing/debugging
  ```

### 8. MIDI Challenge Commands

**Commands**: `sidechain-admin challenges <subcommand>`

**TODO**:
- [ ] **8.1** `challenges list` - List all challenges
  ```bash
  sidechain-admin challenges list --status active|upcoming|ended
  ```

- [ ] **8.2** `challenges create` - Create new challenge
  ```bash
  sidechain-admin challenges create \
    --title "C Major Only Challenge" \
    --description "Create a loop using only C major notes" \
    --start "2025-01-15T00:00:00Z" \
    --end "2025-01-22T00:00:00Z" \
    --voting-end "2025-01-24T00:00:00Z" \
    --constraints '{"key": "C", "scale": "major"}'
  ```

- [ ] **8.3** `challenges get <challenge_id>` - Get challenge details
  ```bash
  sidechain-admin challenges get abc123
  # Shows: details, entry count, top entries, voting stats
  ```

- [ ] **8.4** `challenges entries <challenge_id>` - List entries
  ```bash
  sidechain-admin challenges entries abc123 --sort votes|date
  ```

- [ ] **8.5** `challenges end <challenge_id>` - End challenge early
  ```bash
  sidechain-admin challenges end abc123 --announce-winner
  ```

- [ ] **8.6** `challenges notify <challenge_id>` - Send notifications
  ```bash
  sidechain-admin challenges notify abc123 --type deadline|voting|winner
  ```

### 9. Notification Commands

**Commands**: `sidechain-admin notifications <subcommand>`

**TODO**:
- [ ] **9.1** `notifications broadcast` - Send to all users
  ```bash
  sidechain-admin notifications broadcast \
    --title "System Maintenance" \
    --message "Scheduled maintenance tonight 2-4am UTC" \
    --type system
  ```

- [ ] **9.2** `notifications send` - Send to specific user
  ```bash
  sidechain-admin notifications send @johndoe \
    --title "Account Notice" \
    --message "Your account has been verified!" \
    --type info
  ```

- [ ] **9.3** `notifications stats` - Notification delivery stats
  ```bash
  sidechain-admin notifications stats --period week
  # Shows: sent, delivered, opened, by type
  ```

---

## P3: Low Priority - Nice-to-Have

### 10. Advanced Features

**TODO**:
- [ ] **10.1** Interactive mode
  ```bash
  sidechain-admin interactive
  # REPL-style interface for chaining commands
  ```

- [ ] **10.2** Scripting support
  ```bash
  sidechain-admin script run cleanup.yaml
  # Run predefined scripts/playbooks
  ```

- [ ] **10.3** Slack/Discord integration
  ```bash
  sidechain-admin alerts setup --slack-webhook URL
  # Send alerts to Slack/Discord
  ```

- [ ] **10.4** Scheduled tasks
  ```bash
  sidechain-admin schedule add "daily-cleanup" --cron "0 0 * * *" --command "dev cleanup"
  ```

- [ ] **10.5** Audit log export
  ```bash
  sidechain-admin audit export --since DATE --output audit.json
  ```

---

## Implementation Plan

### Phase 1: Foundation (Week 1)
1. CLI framework setup with Cobra
2. Database/service initialization
3. Output formatters (table, JSON, colors)
4. Basic user commands (list, get, search)

### Phase 2: Core Management (Week 2)
1. User management commands (reset password, suspend, etc.)
2. Content management commands (posts, comments)
3. Moderation commands (reports)

### Phase 3: Monitoring & Analytics (Week 3)
1. System health commands
2. Analytics commands
3. Queue/WebSocket monitoring

### Phase 4: Development Tools (Week 4)
1. Development utilities (seed, cleanup, migrate)
2. Challenge management
3. Notification commands

---

## Usage Examples

### Daily Moderation Workflow
```bash
# Check pending reports
sidechain-admin moderation reports list --status pending

# Review a report
sidechain-admin moderation reports get REPORT_ID

# Take action
sidechain-admin moderation reports action REPORT_ID --action remove --note "Spam content"
```

### Investigating a User
```bash
# Get user details
sidechain-admin users get @suspicious_user

# Check their content
sidechain-admin content posts list --user USER_ID

# Check reports against them
sidechain-admin moderation reports list --target-user USER_ID

# If needed, suspend
sidechain-admin users suspend USER_ID --reason "Investigation pending"
```

### Daily Health Check
```bash
# Quick health check
sidechain-admin system health

# Check audio queue (if backlog)
sidechain-admin system queue

# Check for errors
sidechain-admin system logs --level error --since 24h
```

### Monthly Analytics Report
```bash
# Generate report
sidechain-admin analytics users --period month
sidechain-admin analytics engagement --period month
sidechain-admin analytics top-users --metric followers --limit 10

# Export for external analysis
sidechain-admin analytics export --type users --period month --format csv --output report.csv
```

---

## Configuration

### Environment Variables
```bash
# Database (same as backend)
POSTGRES_HOST=localhost
POSTGRES_PORT=5432
POSTGRES_USER=sidechain
POSTGRES_PASSWORD=xxx
POSTGRES_DB=sidechain

# Stream.io
STREAM_API_KEY=xxx
STREAM_API_SECRET=xxx

# AWS (for S3 and SES)
AWS_REGION=us-east-1
AWS_ACCESS_KEY_ID=xxx
AWS_SECRET_ACCESS_KEY=xxx
AWS_BUCKET=sidechain-audio

# Gorse
GORSE_URL=http://localhost:8087
GORSE_API_KEY=xxx

# CLI-specific
ADMIN_LOG_LEVEL=info
ADMIN_OUTPUT_FORMAT=table  # or json
```

### Config File (Optional)
```yaml
# ~/.sidechain-admin.yaml
database:
  host: localhost
  port: 5432
output:
  format: table
  colors: true
defaults:
  limit: 20
```

---

## Security Considerations

- [ ] CLI requires authentication (admin credentials or token)
- [ ] All actions logged to audit trail
- [ ] Dangerous operations require `--confirm` flag
- [ ] `--dry-run` mode for testing commands
- [ ] Rate limiting for API calls
- [ ] Encrypted config file support for credentials

---

*This TODO list should be treated as a living document. Update as features are completed and new requirements emerge.*
