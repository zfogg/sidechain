# OAuth Configuration Guide

**Last Updated:** 2025-12-17
**Security Level:** CRITICAL

## Overview

Sidechain uses OAuth 2.0 for authentication with Google and Discord. This guide explains the required configuration and security requirements.

## CRITICAL SECURITY REQUIREMENT

The `OAUTH_REDIRECT_URL` environment variable **MUST** be set explicitly. This is a **required** variable with no fallback defaults.

### Why?

Previously, the code had hardcoded fallback URLs (localhost for dev, hardcoded production domain). This created a security vulnerability where:

1. **Silent Failures**: If environment variables weren't set, OAuth would silently use localhost, which wouldn't work in production but might not trigger clear errors
2. **Configuration Drift**: Different environments could have different OAuth URLs without explicit configuration
3. **Security Risk**: Attackers could potentially intercept callbacks if the wrong URL was used

### Solution

All OAuth configuration is now **explicit and environment-driven**. The server will fail fast with a clear error message if any required OAuth variables are missing.

## Configuration Steps

### 1. Set Required Environment Variables

Create a `.env` file in the `backend/` directory with these **required** variables:

```bash
# OAuth Base URL - REQUIRED - must be set in ALL environments
# For development: http://localhost:8787
# For production: https://api.yourdomain.com (NO trailing slash)
OAUTH_REDIRECT_URL=http://localhost:8787

# Google OAuth - REQUIRED
GOOGLE_CLIENT_ID=your_google_client_id
GOOGLE_CLIENT_SECRET=your_google_client_secret

# Discord OAuth - REQUIRED
DISCORD_CLIENT_ID=your_discord_client_id
DISCORD_CLIENT_SECRET=your_discord_client_secret

# JWT Secret - REQUIRED
JWT_SECRET=your_long_random_secret_key

# Other configuration...
```

### 2. Get Google OAuth Credentials

1. Go to [Google Cloud Console](https://console.cloud.google.com/)
2. Create a new project (or select existing)
3. Enable "Google+ API"
4. Go to "Credentials" → "Create Credentials" → "OAuth 2.0 Client ID"
5. Application Type: **Web application**
6. Authorized redirect URIs:
   - **Development:** `http://localhost:8787/api/v1/auth/google/callback`
   - **Production:** `https://api.yourdomain.com/api/v1/auth/google/callback`
7. Copy Client ID and Client Secret to `.env`

### 3. Get Discord OAuth Credentials

1. Go to [Discord Developer Portal](https://discord.com/developers/applications)
2. Create a new application
3. Go to "OAuth2" → "General"
4. Note the Client ID and Client Secret
5. Go to "OAuth2" → "Redirects" and add:
   - **Development:** `http://localhost:8787/api/v1/auth/discord/callback`
   - **Production:** `https://api.yourdomain.com/api/v1/auth/discord/callback`
6. Copy Client ID and Client Secret to `.env`

### 4. Validate Configuration

Before starting the server, verify all variables are set:

```bash
cd backend
# Check that all OAuth variables are set
grep -E "OAUTH_REDIRECT_URL|GOOGLE_|DISCORD_|JWT_SECRET" .env

# Start the server - it will fail fast if anything is missing
go run cmd/server/main.go
```

If any variable is missing, you'll see an error like:

```
Failed to load OAuth configuration: OAUTH_REDIRECT_URL environment variable not set - this is REQUIRED for OAuth to work
```

## Environment-Specific Setup

### Development Environment

```bash
ENVIRONMENT=development
OAUTH_REDIRECT_URL=http://localhost:8787
GOOGLE_CLIENT_ID=<dev-google-id>
GOOGLE_CLIENT_SECRET=<dev-google-secret>
DISCORD_CLIENT_ID=<dev-discord-id>
DISCORD_CLIENT_SECRET=<dev-discord-secret>
JWT_SECRET=dev_secret_key_change_in_production
```

### Production Environment

```bash
ENVIRONMENT=production
OAUTH_REDIRECT_URL=https://api.yourdomain.com  # No trailing slash
GOOGLE_CLIENT_ID=<prod-google-id>
GOOGLE_CLIENT_SECRET=<prod-google-secret>
DISCORD_CLIENT_ID=<prod-discord-id>
DISCORD_CLIENT_SECRET=<prod-discord-secret>
JWT_SECRET=<long-random-secret-key-from-password-manager>
```

## Docker/Kubernetes Setup

When deploying with Docker or Kubernetes, pass environment variables via:

### Docker

```bash
docker run \
  -e OAUTH_REDIRECT_URL=https://api.yourdomain.com \
  -e GOOGLE_CLIENT_ID=... \
  -e GOOGLE_CLIENT_SECRET=... \
  -e DISCORD_CLIENT_ID=... \
  -e DISCORD_CLIENT_SECRET=... \
  -e JWT_SECRET=... \
  sidechain-backend:latest
```

### Kubernetes

```yaml
apiVersion: v1
kind: Secret
metadata:
  name: sidechain-oauth
type: Opaque
stringData:
  oauth-redirect-url: "https://api.yourdomain.com"
  google-client-id: "..."
  google-client-secret: "..."
  discord-client-id: "..."
  discord-client-secret: "..."
  jwt-secret: "..."
---
apiVersion: apps/v1
kind: Deployment
metadata:
  name: sidechain-backend
spec:
  template:
    spec:
      containers:
      - name: backend
        env:
        - name: OAUTH_REDIRECT_URL
          valueFrom:
            secretKeyRef:
              name: sidechain-oauth
              key: oauth-redirect-url
        - name: GOOGLE_CLIENT_ID
          valueFrom:
            secretKeyRef:
              name: sidechain-oauth
              key: google-client-id
        # ... etc for other variables
```

## Testing OAuth Flow

### Manual Testing

1. Start the backend server:
   ```bash
   cd backend
   go run cmd/server/main.go
   ```

2. Visit Google OAuth endpoint:
   ```
   http://localhost:8787/api/v1/auth/google
   ```

3. You'll be redirected to Google login, then back to:
   ```
   http://localhost:8787/api/v1/auth/google/callback?code=...&state=...
   ```

4. The server processes the callback and returns a JWT token

### Testing with curl

```bash
# Get OAuth state token
STATE=$(openssl rand -hex 16)

# Construct authorization URL
echo "https://accounts.google.com/o/oauth2/v2/auth?client_id=YOUR_CLIENT_ID&redirect_uri=http://localhost:8787/api/v1/auth/google/callback&response_type=code&scope=openid%20profile%20email&state=$STATE"

# Visit that URL in browser, complete login, capture the authorization code
# Then exchange it for a token (the server does this automatically via callback)
```

## Troubleshooting

### "OAUTH_REDIRECT_URL environment variable not set"

**Solution:** Make sure `OAUTH_REDIRECT_URL` is explicitly set in your `.env` file or environment.

### "invalid_request" error during OAuth

**Possible causes:**
1. Redirect URI doesn't match OAuth provider configuration (check Google/Discord console)
2. Client ID or Secret is wrong
3. OAuth scope is not available for your app

**Solution:** Verify all credentials match exactly in both your `.env` file and the OAuth provider console.

### OAuth callback returns 404

**Possible causes:**
1. Server isn't running
2. The `OAUTH_REDIRECT_URL` doesn't match the server's actual URL
3. Network/firewall issue blocking the callback

**Solution:**
- Verify server is running: `curl http://localhost:8787/health`
- Check `OAUTH_REDIRECT_URL` matches your server's actual address
- For development, ensure `localhost:8787` is accessible from your browser

### "Unauthorized" after successful OAuth flow

**Possible causes:**
1. JWT_SECRET is different between server start and token validation
2. Token has expired
3. User not created in database

**Solution:**
- Check JWT_SECRET is consistent
- Check server logs for database errors
- Verify OAuth user creation succeeded

## Code Implementation Details

The OAuth configuration is loaded in `backend/internal/config/oauth.go`:

```go
// Loads configuration from environment variables
oauthConfig, err := config.LoadOAuthConfig()
if err != nil {
    log.Fatal("Failed to load OAuth configuration:", err)
}

// Used in auth service
authService := auth.NewService(jwtSecret, streamClient, oauthConfig)
```

If any required variable is missing, the application fails immediately on startup with a clear error message. This is a security feature that prevents misconfiguration.

## Security Best Practices

1. **Never commit secrets**: Always use `.env` files or environment variables, never hardcode
2. **Rotate secrets**: Periodically rotate your OAuth client secrets in the provider console
3. **Use HTTPS**: In production, always use HTTPS redirect URLs (not HTTP)
4. **Monitor OAuth**: Log all OAuth failures for security monitoring
5. **Rate limit**: The OAuth endpoints have stricter rate limiting (10 req/min per IP)

## See Also

- [Google OAuth 2.0 Documentation](https://developers.google.com/identity/protocols/oauth2)
- [Discord OAuth2 Documentation](https://discord.com/developers/docs/topics/oauth2)
- [Sidechain Authentication Architecture](./AUTHENTICATION.md) (if it exists)

---

**Last Updated:** 2025-12-17
**Reviewed By:** Security Team
