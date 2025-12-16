# Sidechain Admin Setup Guide

## Overview

The Sidechain system now supports admin users who can:
- Query and view any user's profile (including private profiles)
- Moderate content and reports
- Manage user accounts
- Access admin-only features in the CLI

## Backend Changes

### 1. User Model Updates
- Added `IsAdmin` boolean field to the User model (defaults to `false`)
- Field is automatically included in JWT tokens for frontend/CLI authentication
- Database schema is automatically updated via GORM AutoMigrate

### 2. JWT Claims
- Admin status is now included in JWT token claims under the `is_admin` field
- This allows the CLI to know if the authenticated user is an admin

### 3. Promote-Admin Command

A new command has been created to promote users to admin status.

**Location**: `backend/cmd/promote-admin/main.go`

**Usage**:
```bash
# Grant admin privileges to a user
cd backend
go run cmd/promote-admin/main.go -email=user@example.com

# Revoke admin privileges
go run cmd/promote-admin/main.go -email=user@example.com -revoke

# Or build the binary first
go build -mod=mod -o promote-admin ./cmd/promote-admin
./promote-admin -email=user@example.com
./promote-admin -email=user@example.com -revoke
```

**Example Output**:
```
✓ Admin privileges granted to john_doe (john@example.com)
  User ID: 550e8400-e29b-41d4-a716-446655440000
  The user must log out and log back in for changes to take effect
```

**Requirements**:
- Must be run in the backend directory with proper `.env` configuration
- Database must be initialized
- User email must exist in the database

## CLI Changes

### 1. Credentials Storage
- CLI credentials now include the `is_admin` flag
- This flag is set when the user logs in and stored locally
- Automatically updated on each login

### 2. Login Experience for Admins
When an admin user logs in, they see:
```
✓ Login successful!
Logged in as john_doe (ADMIN)

Username:        john_doe
Email:           john@example.com
Display Name:    John Doe
Followers:       150
Following:       42
Posts:           28
Email Verified:  true
Admin:           ✓ YES
```

### 3. Auth Me Command
Running `sidechain-cli auth me` now displays admin status:
```
Username:        john_doe
Email:           john@example.com
...
Admin:           ✓ YES
```

### 4. Profile Management
Admin users can view any user's profile, including private profiles. When viewing an admin's profile, it displays:
```
Username:        admin_user
...
Private:         true
Admin:           ✓ YES
```

## Workflow: Promoting a User to Admin

### Step 1: User Registration
User creates an account normally through the CLI or plugin:
```bash
sidechain-cli auth login
# Enter email: user@example.com
# Enter password: ****
```

### Step 2: Promote to Admin
Administrator runs the promote-admin command:
```bash
cd backend
go run cmd/promote-admin/main.go -email=user@example.com
```

### Step 3: User Logs Back In
User must log out and log back in for the admin status to take effect:
```bash
sidechain-cli auth logout
sidechain-cli auth login
# Now sees the ADMIN designation
```

## Future Admin Features

The following admin features are planned for implementation:

### Report Management
- View all user-submitted reports
- Filter reports by status (pending, reviewing, resolved)
- Assign reports to moderators
- Mark reports as resolved or dismissed

### User Moderation
- View user details and violation history
- Suspend/unsuspend user accounts
- Issue warnings
- Force password resets

### Content Moderation
- Delete posts and comments
- Hide content temporarily
- View reported content with context

### Dashboard
- Moderation statistics
- Pending reports count
- Recent violations
- Moderator activity log

## Security Considerations

1. **Token Expiration**: Admin status in JWT tokens expires after 24 hours. Users must log back in periodically.

2. **No Automatic Promotion**: Users cannot promote themselves. Only existing admins or database operations can grant admin status.

3. **Audit Trail**: All admin actions should be logged (implementation pending).

4. **Role Separation**: Admin status is all-or-nothing in the current implementation. Future versions may use role-based access control (RBAC).

## Technical Implementation Details

### Database Schema Change
```sql
ALTER TABLE users ADD COLUMN is_admin BOOLEAN DEFAULT FALSE;
```

This is automatically handled by GORM AutoMigrate when the server starts.

### JWT Payload Example
```json
{
  "user_id": "550e8400-e29b-41d4-a716-446655440000",
  "email": "admin@example.com",
  "username": "admin",
  "stream_user_id": "...",
  "is_admin": true,
  "exp": 1645000000,
  "iat": 1644913600
}
```

### CLI Credential File
Admin status is persisted in `~/.sidechain/credentials`:
```json
{
  "access_token": "...",
  "refresh_token": "...",
  "expires_at": "2025-01-15T10:30:00Z",
  "user_id": "550e8400-e29b-41d4-a716-446655440000",
  "username": "admin",
  "email": "admin@example.com",
  "is_admin": true
}
```

## Troubleshooting

### "User not found" error
- Verify the email address is correct (case-sensitive in some systems)
- Ensure the user has completed registration
- Check database connectivity

### Admin status not showing after login
- User must log out and log back in
- Verify the promote-admin command succeeded
- Check that JWT token includes `is_admin` claim

### Database connection errors
- Ensure `.env` file is properly configured
- Check `DATABASE_URL` environment variable
- Verify PostgreSQL is running and accessible

## Admin Impersonation Feature

Admins can run CLI commands as other users using the `--as-user` flag. This allows admins to:
- Diagnose issues from a user's perspective
- Debug API problems by testing with specific user accounts
- Manage user content and settings as an admin action
- Support and troubleshoot user issues

### CLI Usage

Run any command as another user:
```bash
# View another user's profile
sidechain-cli --as-user user@example.com profile view

# View timeline as another user
sidechain-cli --as-user user@example.com feed timeline

# Get user info as another user
sidechain-cli --as-user user@example.com auth me

# View another user's posts
sidechain-cli --as-user user@example.com post list
```

**Important Requirements**:
1. You must be logged in as an admin user
2. The target user email must exist in the system
3. The flag must come before the command: `sidechain-cli --as-user email@test.com COMMAND`

### Security & Validation

The system validates impersonation at multiple layers:

**CLI Level** (`internal/cmd/root.go`):
- Checks that user is logged in
- Verifies user has admin status
- Sets impersonation header before any API calls

**Backend Level** (`internal/middleware/impersonate.go`):
- Validates authenticated user is an admin
- Looks up impersonated user by email
- Replaces user context with impersonated user
- Returns 403 Forbidden if non-admin attempts impersonation
- Returns 404 Not Found if target user doesn't exist

**HTTP Header** (`X-Impersonate-User`):
- Only set when `--as-user` flag is provided
- Sent with every API request when flag is active
- Backend rejects requests if header is present but user isn't admin

### Implementation Details

#### CLI Changes
1. **Flag Definition** (`internal/cmd/root.go`):
   - Global `--as-user` flag added to root command
   - Validated in PersistentPreRun before any commands execute
   - Sets impersonation user in HTTP client

2. **HTTP Client** (`pkg/client/client.go`):
   - `SetImpersonateUser(email string)` - Sets the impersonation user
   - `ClearImpersonateUser()` - Clears impersonation (if needed)
   - OnBeforeRequest hook adds `X-Impersonate-User` header to all requests

#### Backend Changes
1. **Middleware** (`internal/middleware/impersonate.go`):
   - Checks for `X-Impersonate-User` header
   - Validates admin status of authenticated user
   - Replaces user context with impersonated user
   - All downstream handlers receive impersonated user's context

2. **Route Setup** (`cmd/server/main.go`):
   - Middleware applied to entire API v1 group
   - Works with all protected routes automatically

### Example Workflow

```bash
# 1. Admin logs in
sidechain-cli auth login
# Email: admin@example.com
# Password: ****
# ✓ Login successful!
# Logged in as admin (ADMIN)

# 2. Admin views their own profile
sidechain-cli auth me
# Username:        admin
# Admin:           ✓ YES

# 3. Admin views another user's profile as that user
sidechain-cli --as-user john@example.com profile view
# Username:        john
# Display Name:    John Doe
# Email:           john@example.com

# 4. Admin views John's timeline
sidechain-cli --as-user john@example.com feed timeline
# Shows John's timeline posts

# 5. Admin can perform actions as John
sidechain-cli --as-user john@example.com post upload ./track.mp3
# Uploads audio as John
```

### Error Cases

**Non-admin attempts impersonation**:
```bash
$ sidechain-cli --as-user user@example.com auth me
Error: Only admin users can impersonate other users
```

**User not logged in**:
```bash
$ sidechain-cli --as-user user@example.com auth me
Error: You must be logged in to use --as-user
```

**Target user doesn't exist**:
```bash
$ sidechain-cli --as-user nonexistent@example.com auth me
# Backend returns 404
Error: Impersonated user not found
```

### Audit and Logging

**TODO**: Implement audit logging for admin impersonation actions:
- Log which admin impersonated which user
- Log timestamp and action performed
- Store in audit log table in database
- Use for compliance and security reviews

### Future Enhancements

1. **Audit Trail**: Log all impersonation actions
2. **Impersonation Limits**: Only allow impersonation of non-admin users
3. **Temporary Sessions**: Auto-clear impersonation after X minutes
4. **Audit Events**: Send notifications when admin impersonates users
5. **Fine-grained Permissions**: Allow impersonation of specific user subsets
6. **Activity Tracking**: Show "Accessed as admin" in user activity logs

## Related Files

- **Backend Model**: `backend/internal/models/user.go`
- **Auth Service**: `backend/internal/auth/service.go`
- **Admin Command**: `backend/cmd/promote-admin/main.go`
- **Impersonation Middleware**: `backend/internal/middleware/impersonate.go`
- **Server Setup**: `backend/cmd/server/main.go`
- **CLI Credentials**: `cli/pkg/credentials/credentials.go`
- **CLI Auth Service**: `cli/pkg/service/auth.go`
- **CLI API Types**: `cli/pkg/api/types.go`
- **CLI Root Command**: `cli/internal/cmd/root.go`
- **CLI HTTP Client**: `cli/pkg/client/client.go`
