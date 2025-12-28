# Sidechain Web Frontend - Deployment Guide

## Phase 14: Deployment & DevOps

This document outlines the deployment infrastructure, CI/CD pipelines, and DevOps best practices for the Sidechain web frontend.

---

## Overview

The Sidechain web frontend uses a modern CI/CD pipeline with:

- **Source Control**: GitHub
- **CI/CD**: GitHub Actions
- **Container Registry**: GitHub Container Registry (GHCR)
- **Containerization**: Docker
- **Deployment**: SSH-based rsync to servers
- **Environments**: Staging and Production

---

## CI/CD Pipelines

### Web Tests (`.github/workflows/test-web.yml`)

Runs on every push to `main` and `develop` branches, and on all pull requests.

**Steps**:
1. **Install dependencies** - `pnpm install --frozen-lockfile`
2. **Run linter** - `pnpm lint`
3. **Run tests** - `pnpm test:ci` (with coverage)
4. **Upload coverage** - Send to Codecov

**Matrix**:
- Node.js 18.x
- Node.js 20.x

**Status Badge**:
```markdown
[![Tests](https://github.com/zfogg/sidechain/actions/workflows/test-web.yml/badge.svg?branch=main)](https://github.com/zfogg/sidechain/actions/workflows/test-web.yml)
```

### Web Build and Deploy (`.github/workflows/deploy-web.yml`)

Runs on push to `main` branch (production deployments).

**Stages**:

1. **Build Docker Image**
   - Builds multi-stage Docker image
   - Pushes to GitHub Container Registry (GHCR)
   - Tags: `main`, `sha-{commit}`, semantic versioning

2. **Test**
   - Runs full test suite with coverage
   - Builds the application
   - Uploads dist artifact

3. **Deploy to Staging**
   - Requires `staging` environment approval (manual)
   - Syncs dist files via rsync over SSH
   - Uses `STAGING_DEPLOY_*` secrets

4. **Deploy to Production**
   - Requires `production` environment approval (manual)
   - Syncs dist files via rsync over SSH
   - Uses `PROD_DEPLOY_*` secrets
   - Only runs on `main` branch

---

## Docker

### Build Locally

```bash
cd web
docker build -t sidechain-web:latest .
```

### Run Locally

```bash
docker run -p 3000:8080 sidechain-web:latest
```

Visit `http://localhost:3000`

### Multi-Stage Build

The Dockerfile uses multi-stage builds for optimization:

**Stage 1: Builder**
- Node 20 Alpine base
- Install dependencies with pnpm
- Build the application

**Stage 2: Runtime**
- Node 20 Alpine base (minimal)
- Copy built dist from builder
- Use `serve` to host static files
- Run as non-root user
- Include health check

**Benefits**:
- Reduced image size (~100MB vs ~500MB)
- No build dependencies in final image
- Security: non-root user execution
- Health check for container orchestration

### Docker Image Publishing

Images are published to GitHub Container Registry (GHCR):

```bash
# Pull image (requires authentication)
docker pull ghcr.io/zfogg/sidechain/web:main

# Run with environment variables
docker run -e VITE_API_URL=https://api.example.com \
  -p 3000:3000 \
  ghcr.io/zfogg/sidechain/web:main
```

---

## Environment Configuration

### Environment Variables

Copy `.env.example` to `.env.local` for development:

```bash
cp .env.example .env.local
```

**Variables**:

| Variable | Purpose | Example |
|----------|---------|---------|
| `VITE_API_URL` | Backend API URL | `http://localhost:8787/api/v1` |
| `VITE_WS_URL` | WebSocket URL | `ws://localhost:8787/api/v1/ws` |
| `VITE_STREAM_API_KEY` | Stream Chat API key | `xyz123...` |
| `VITE_GOOGLE_CLIENT_ID` | Google OAuth client ID | `...googleusercontent.com` |
| `VITE_DISCORD_CLIENT_ID` | Discord OAuth client ID | `123456789...` |
| `VITE_CDN_URL` | CDN base URL | `https://cdn.example.com` |
| `VITE_ERROR_TRACKING_URL` | Error tracking endpoint | `https://sentry.example.com/...` |

### Secrets Configuration

GitHub repository secrets required for CI/CD:

**Deployment Secrets**:
- `STAGING_DEPLOY_KEY` - SSH private key for staging server
- `STAGING_DEPLOY_HOST` - Staging server hostname
- `STAGING_DEPLOY_USER` - SSH user for staging
- `STAGING_DEPLOY_PATH` - Remote path on staging server

- `PROD_DEPLOY_KEY` - SSH private key for production server
- `PROD_DEPLOY_HOST` - Production server hostname
- `PROD_DEPLOY_USER` - SSH user for production
- `PROD_DEPLOY_PATH` - Remote path on production server

**Example Setup**:

```bash
# Generate SSH key (without passphrase for CI/CD)
ssh-keygen -t ed25519 -f deploy_key -N ""

# Add public key to server authorized_keys
ssh-copy-id -i deploy_key.pub user@server

# Add private key to GitHub as secret
# Settings > Secrets and variables > Actions > New repository secret
# Name: STAGING_DEPLOY_KEY
# Value: (contents of deploy_key)
```

---

## Deployment Methods

### Method 1: GitHub Actions (Automated)

Push to `main` branch → Workflow runs → Manual approval → Deploy

**Advantages**:
- Fully automated
- Test suite runs first
- Docker image built and stored
- Audit trail in GitHub

**Disadvantages**:
- Requires GitHub Actions approval
- Slower than direct deployment

### Method 2: Manual SSH Deployment

For emergency fixes or bypass:

```bash
cd web
bun run build

# Deploy to staging
rsync -avz --delete -e "ssh -i ~/.ssh/deploy_key" \
  dist/ user@staging.example.com:/var/www/sidechain/

# Deploy to production
rsync -avz --delete -e "ssh -i ~/.ssh/deploy_key" \
  dist/ user@prod.example.com:/var/www/sidechain/
```

### Method 3: Docker Compose (Local/Staging)

Deploy using Docker Compose on server:

```yaml
# docker-compose.yml
version: '3.8'

services:
  web:
    image: ghcr.io/zfogg/sidechain/web:main
    ports:
      - "3000:3000"
    environment:
      VITE_API_URL: ${VITE_API_URL}
      VITE_WS_URL: ${VITE_WS_URL}
      VITE_STREAM_API_KEY: ${VITE_STREAM_API_KEY}
    restart: unless-stopped
    healthcheck:
      test: ["CMD", "curl", "-f", "http://localhost:3000"]
      interval: 30s
      timeout: 3s
      retries: 3
```

Deploy:

```bash
docker-compose pull
docker-compose up -d
```

---

## Server Setup

### Nginx Configuration

Serve the web frontend with Nginx:

```nginx
upstream sidechain_web {
  server localhost:3000;
}

server {
  listen 80;
  listen [::]:80;
  server_name sidechain.example.com;

  # Redirect HTTP to HTTPS
  return 301 https://$server_name$request_uri;
}

server {
  listen 443 ssl http2;
  listen [::]:443 ssl http2;
  server_name sidechain.example.com;

  # SSL certificates (use Let's Encrypt)
  ssl_certificate /etc/letsencrypt/live/sidechain.example.com/fullchain.pem;
  ssl_certificate_key /etc/letsencrypt/live/sidechain.example.com/privkey.pem;

  # Security headers
  add_header Strict-Transport-Security "max-age=31536000; includeSubDomains" always;
  add_header X-Content-Type-Options "nosniff" always;
  add_header X-Frame-Options "DENY" always;
  add_header X-XSS-Protection "1; mode=block" always;
  add_header Referrer-Policy "strict-origin-when-cross-origin" always;

  # Gzip compression
  gzip on;
  gzip_types text/plain text/css application/json application/javascript text/xml application/xml application/xml+rss text/javascript;
  gzip_vary on;

  # Cache static assets
  location ~* \.(js|css|png|jpg|jpeg|gif|ico|svg|woff|woff2|ttf|eot)$ {
    expires 1y;
    add_header Cache-Control "public, immutable";
  }

  # SPA routing: serve index.html for all non-file requests
  location / {
    try_files $uri $uri/ /index.html;
    proxy_pass http://sidechain_web;
    proxy_set_header Host $host;
    proxy_set_header X-Real-IP $remote_addr;
    proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
    proxy_set_header X-Forwarded-Proto $scheme;
  }
}
```

### SSL Certificate Setup

Using Let's Encrypt with Certbot:

```bash
sudo apt-get install certbot python3-certbot-nginx

# Obtain certificate
sudo certbot certonly --nginx -d sidechain.example.com

# Auto-renewal (runs twice daily)
sudo systemctl enable certbot.timer
sudo systemctl start certbot.timer
```

---

## Monitoring & Logging

### Application Monitoring

**Metrics to Monitor**:
- Page load time (LCP, FCP, TTI)
- API response times
- Error rates
- JavaScript errors
- User engagement

**Tools**:
- **Sentry** - Error tracking
- **LogRocket** - Session replay
- **New Relic** - Performance monitoring
- **DataDog** - Infrastructure monitoring

### Log Aggregation

Collect logs from multiple sources:

```bash
# Application logs
docker logs <container-id>

# Nginx access logs
tail -f /var/log/nginx/access.log

# System logs
journalctl -u docker
```

**Tools**:
- **ELK Stack** (Elasticsearch, Logstash, Kibana)
- **Splunk**
- **CloudWatch** (AWS)
- **Stackdriver** (GCP)

### Health Checks

Docker health check runs every 30 seconds:

```bash
curl http://localhost:3000
```

Nginx health check:

```nginx
server {
  listen 9000;
  location /health {
    return 200 "healthy\n";
    add_header Content-Type text/plain;
  }
}
```

---

## Security

### Content Security Policy

Set CSP headers to prevent XSS:

```nginx
add_header Content-Security-Policy "
  default-src 'self';
  script-src 'self' 'unsafe-inline' 'unsafe-eval' https://cdn.example.com;
  style-src 'self' 'unsafe-inline' https://fonts.googleapis.com;
  img-src 'self' data: https: http:;
  font-src 'self' https://fonts.gstatic.com;
  connect-src 'self' https://api.example.com wss://api.example.com;
  frame-ancestors 'none';
  upgrade-insecure-requests;
" always;
```

### CORS Configuration

Configure CORS on backend to allow frontend requests:

```
CORS_ALLOWED_ORIGINS=https://sidechain.example.com,https://staging.example.com
```

### Rate Limiting

Implement rate limiting on backend to prevent abuse:

```go
// Backend pseudocode
limiter := NewRateLimiter(100, 1*time.Minute)
if !limiter.Allow(clientIP) {
  return TooManyRequests()
}
```

### Dependency Security

Scan dependencies for vulnerabilities:

```bash
# Check for vulnerable dependencies
pnpm audit

# Fix automatically (if possible)
pnpm audit --fix
```

**GitHub Security Features**:
- Dependabot: Automatic dependency updates
- Security advisories: Alerts on vulnerabilities
- Secret scanning: Prevents API keys from being committed

---

## Rollback Procedures

### Quick Rollback

If deployment causes issues:

```bash
# SSH to production server
ssh user@prod.example.com

# List recent deployments
ls -la /var/www/sidechain/backups/

# Rollback to previous version
rsync -avz /var/www/sidechain/backups/dist-{date}/ \
  /var/www/sidechain/dist/

# Restart application
docker-compose restart web
```

### Blue-Green Deployment

For zero-downtime deployments:

```nginx
# Nginx config with two backend servers
upstream sidechain_web_blue {
  server localhost:3001;
}

upstream sidechain_web_green {
  server localhost:3002;
}

# Point to blue initially
upstream sidechain_web_active {
  server localhost:3001;
}

server {
  location / {
    proxy_pass http://sidechain_web_active;
  }
}
```

**Deployment Process**:
1. Deploy new version to green (3002)
2. Test green thoroughly
3. Switch nginx upstream to green
4. Keep blue running for quick rollback

---

## Performance Optimization

### Asset Optimization

**Vite Build Output**:
```bash
bun run build
```

Output includes:
- Code splitting (route-based)
- Tree shaking (unused code removal)
- Minification
- Source maps for production debugging

**Analyze Bundle Size**:
```bash
bun run build -- --analyze
```

### Caching Strategy

**Cache Headers**:
- Static assets (JS, CSS, images): 1 year
- HTML (index.html): no-cache
- API responses: via React Query

**Service Worker** (optional):
```bash
# Enable offline support
pnpm add workbox-window
```

---

## Troubleshooting

### Common Issues

**Issue**: Blank page on deployment
**Solution**:
```bash
# Check if dist folder is empty
ls -la /var/www/sidechain/dist/

# Verify index.html exists
cat /var/www/sidechain/dist/index.html | head -20
```

**Issue**: API calls fail in production
**Solution**:
```bash
# Check environment variables
docker exec <container> env | grep VITE_

# Verify CORS headers
curl -H "Origin: https://sidechain.example.com" \
  -H "Access-Control-Request-Method: POST" \
  https://api.example.com/api/v1/health -v
```

**Issue**: High memory usage
**Solution**:
```bash
# Check Docker memory limits
docker stats

# Limit memory in docker-compose
services:
  web:
    deploy:
      resources:
        limits:
          memory: 512M
```

### Debug Information

Collect debug info for troubleshooting:

```bash
# Application version
curl https://sidechain.example.com/version

# System info
uname -a
docker --version
node --version

# Service status
docker-compose ps
systemctl status nginx
```

---

## Maintenance

### Regular Tasks

**Daily**:
- Monitor error logs
- Check server health
- Verify backups

**Weekly**:
- Review performance metrics
- Check for security updates
- Clean up old logs

**Monthly**:
- Rotate logs
- Update dependencies
- Capacity planning
- Performance review

### Backup Strategy

```bash
# Daily backup of current deployment
0 2 * * * rsync -avz /var/www/sidechain/dist/ \
  /var/www/sidechain/backups/dist-$(date +\%Y\%m\%d)/
```

---

## Cost Optimization

### Resource Utilization

- **CPU**: Use lighter base image (Alpine)
- **Memory**: Set appropriate limits
- **Storage**: Clean old Docker images
- **Bandwidth**: Enable gzip compression

### CDN Configuration

Serve assets through CDN for global performance:

```javascript
// Use CDN in production
const assetURL = import.meta.env.PROD
  ? 'https://cdn.example.com/assets/'
  : '/assets/'
```

---

## Checklist Before Deployment

- [ ] All tests passing (`pnpm test:ci`)
- [ ] Linter passing (`pnpm lint`)
- [ ] No security warnings (`pnpm audit`)
- [ ] Build succeeds (`bun run build`)
- [ ] Environment variables set
- [ ] Secrets configured in GitHub
- [ ] Staging deployment tested
- [ ] Performance benchmarks meet targets
- [ ] Security headers configured
- [ ] Monitoring/alerting setup
- [ ] Rollback procedure documented
- [ ] Team notified of deployment

---

## Support

For deployment issues:
1. Check logs: `docker logs <container>`
2. Review metrics in monitoring tool
3. Check GitHub Actions workflow run
4. Reference this guide's troubleshooting section
5. Open issue on GitHub with details
