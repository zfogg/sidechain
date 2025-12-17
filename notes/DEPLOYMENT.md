# Sidechain Backend Deployment Guide

Complete step-by-step guide to deploy the Sidechain backend to your production server.

## Prerequisites

- ✓ Ubuntu 20.04+ server (77.42.75.203)
- ✓ SSH root access
- ✓ GitHub repository with this code
- ✓ getstream.io account & API credentials

## Step 1: Provision Your Server (5 minutes)

### 1a. Run Provisioning Script

```bash
# From your local machine
ssh root@77.42.75.203 'bash -s' < terraform/provisioning/setup.sh
```

**What this does:**
- Updates all system packages
- Installs Docker & Docker Compose
- Creates `/opt/sidechain` directory
- Sets up systemd service for auto-restart
- Creates `.env` template

### 1b. Verify Installation

```bash
ssh root@77.42.75.203 << 'EOF'
  echo "=== System Check ==="
  docker --version
  docker-compose --version
  ls -la /opt/sidechain/
EOF
```

**Expected output:**
```
Docker version 24.x.x
docker-compose version 2.x.x
total 20
drwxr-xr-x  2 root root 4096 Dec 17 10:30 .
drwxr-xr-x  3 root root 4096 Dec 17 10:30 ..
-rw-r--r--  1 root root  400 Dec 17 10:30 .env
```

---

## Step 2: Configure Environment (5 minutes)

### 2a. Edit Configuration File

```bash
ssh root@77.42.75.203 'nano /opt/sidechain/.env'
```

**Copy and paste, then modify:**

```bash
# Backend Configuration
BACKEND_PORT=8787
POSTGRES_USER=sidechain
POSTGRES_PASSWORD=generate_secure_password_here
POSTGRES_DB=sidechain

# GetStream Configuration (from your dashboard)
GETSTREAM_API_KEY=your-api-key-here
GETSTREAM_API_SECRET=your-api-secret-here

# JWT Secret (generate with: openssl rand -base64 32)
JWT_SECRET=generate_random_jwt_secret_here

# Audio CDN Configuration (if using S3)
CDN_BUCKET=your-s3-bucket
CDN_REGION=us-east-1
CDN_ACCESS_KEY=your-aws-access-key
CDN_SECRET_KEY=your-aws-secret-key

# Other backend settings
LOG_LEVEL=info
```

**How to generate secure values:**

```bash
# Generate strong password
openssl rand -base64 32

# Generate JWT secret
openssl rand -base64 32

# If you need another random value
python3 -c "import secrets; print(secrets.token_urlsafe(32))"
```

### 2b. Save and Verify

Press `Ctrl+O`, Enter, then `Ctrl+X` to save in nano.

Verify:
```bash
ssh root@77.42.75.203 'cat /opt/sidechain/.env | grep -v "^#" | grep -v "^$"'
```

---

## Step 3: Clone Repository (5 minutes)

### 3a. Clone the Code

```bash
ssh root@77.42.75.203 'cd /opt/sidechain && git clone https://github.com/YOUR_USERNAME/sidechain.git .'
```

**Or with SSH (if you have GitHub SSH key):**
```bash
ssh root@77.42.75.203 'cd /opt/sidechain && git clone git@github.com:YOUR_USERNAME/sidechain.git .'
```

### 3b. Verify Clone

```bash
ssh root@77.42.75.203 'ls -la /opt/sidechain/ | head -20'
```

You should see `backend/`, `docker-compose.dev.yml`, `terraform/`, etc.

---

## Step 4: Set Up GitHub Actions (10 minutes)

### 4a. Generate Deploy SSH Key

```bash
# On your local machine, generate a new key for GitHub Actions
ssh-keygen -t ed25519 \
  -f ~/.ssh/sidechain_deploy \
  -C "sidechain-github-actions" \
  -N ""

# Display the private key (you'll need this)
cat ~/.ssh/sidechain_deploy
```

### 4b. Add Public Key to Server

```bash
# Still local machine
cat ~/.ssh/sidechain_deploy.pub

# Copy the output, then run:
ssh root@77.42.75.203 << 'EOF'
  # Paste the public key content here:
  echo "PASTE_PUBLIC_KEY_HERE" >> ~/.ssh/authorized_keys
  chmod 600 ~/.ssh/authorized_keys
EOF
```

Or in one command:
```bash
ssh-copy-id -i ~/.ssh/sidechain_deploy.pub -p 22 root@77.42.75.203
```

### 4c. Add GitHub Secrets

1. Go to your GitHub repository
2. **Settings** → **Secrets and variables** → **Actions**
3. Click **"New repository secret"**

Add these secrets one by one:

| Name | Value |
|------|-------|
| `DEPLOY_SERVER_HOST` | `77.42.75.203` |
| `DEPLOY_SERVER_USER` | `root` |
| `DEPLOY_SERVER_PORT` | `22` |
| `DEPLOY_SSH_KEY` | Entire contents of `~/.ssh/sidechain_deploy` (private key) |

**For DEPLOY_SSH_KEY:**
```bash
# Display the private key to copy
cat ~/.ssh/sidechain_deploy
```

Copy everything including `-----BEGIN PRIVATE KEY-----` and `-----END PRIVATE KEY-----`.

---

## Step 5: Verify Deployment Setup (5 minutes)

### 5a. Test SSH Connection

```bash
ssh -i ~/.ssh/sidechain_deploy -p 22 root@77.42.75.203 'echo "✓ SSH connection successful"'
```

### 5b. Verify Docker

```bash
ssh -i ~/.ssh/sidechain_deploy root@77.42.75.203 << 'EOF'
  cd /opt/sidechain
  echo "=== Directory Contents ==="
  ls -la
  echo ""
  echo "=== Docker & Compose ==="
  docker ps
  echo ""
  echo "=== Environment File ==="
  cat .env | head -5
EOF
```

---

## Step 6: First Deployment (Manual)

Before using GitHub Actions, let's test manually:

### 6a. Start Services

```bash
ssh -i ~/.ssh/sidechain_deploy root@77.42.75.203 << 'EOF'
  set -euo pipefail
  cd /opt/sidechain

  echo "Starting containers..."
  docker-compose -f docker-compose.dev.yml up -d --build

  echo ""
  echo "Waiting for services..."
  sleep 5

  echo ""
  echo "Container status:"
  docker-compose -f docker-compose.dev.yml ps
EOF
```

### 6b. Check Logs

```bash
ssh root@77.42.75.203 'docker-compose -f /opt/sidechain/docker-compose.dev.yml logs --tail=50'
```

### 6c. Test API Health

```bash
curl http://77.42.75.203:8787/health
```

Expected response:
```json
{"status":"healthy"}
```

---

## Step 7: Enable GitHub Actions Deployment (2 minutes)

### 7a. Trigger Deployment from GitHub

Push a change to main branch:
```bash
git add backend/
git commit -m "trigger deployment"
git push origin main
```

### 7b. Monitor Deployment

1. Go to GitHub repository
2. Click **Actions** tab
3. Watch "Deploy Backend to Production" workflow run
4. Check real-time logs

---

## Step 8: Post-Deployment Verification (5 minutes)

### 8a. Check Services

```bash
ssh root@77.42.75.203 'docker-compose -f /opt/sidechain/docker-compose.dev.yml ps'
```

All containers should show `Up` status.

### 8b. Check API

```bash
# Health check
curl http://77.42.75.203:8787/health

# Test feed endpoint (replace TOKEN with real JWT)
curl -H "Authorization: Bearer YOUR_JWT_TOKEN" \
  http://77.42.75.203:8787/api/v1/feed/unified
```

### 8c. Check Database

```bash
ssh root@77.42.75.203 << 'EOF'
  docker-compose -f /opt/sidechain/docker-compose.dev.yml exec -T postgres \
    psql -U sidechain -d sidechain -c "SELECT version();"
EOF
```

---

## Ongoing Operations

### View Logs

```bash
# Real-time logs
ssh root@77.42.75.203 'docker-compose -f /opt/sidechain/docker-compose.dev.yml logs -f'

# Just backend
ssh root@77.42.75.203 'docker-compose -f /opt/sidechain/docker-compose.dev.yml logs backend'

# Last 100 lines
ssh root@77.42.75.203 'docker-compose -f /opt/sidechain/docker-compose.dev.yml logs --tail=100'
```

### Restart Services

```bash
ssh root@77.42.75.203 'docker-compose -f /opt/sidechain/docker-compose.dev.yml restart'
```

### Stop Services

```bash
ssh root@77.42.75.203 'docker-compose -f /opt/sidechain/docker-compose.dev.yml down'
```

### Update Environment

```bash
ssh root@77.42.75.203 << 'EOF'
  nano /opt/sidechain/.env
  # Edit, then:
  docker-compose -f /opt/sidechain/docker-compose.dev.yml restart
EOF
```

---

## Troubleshooting

### API not responding

```bash
# Check container status
ssh root@77.42.75.203 'docker-compose -f /opt/sidechain/docker-compose.dev.yml ps'

# View logs
ssh root@77.42.75.203 'docker-compose -f /opt/sidechain/docker-compose.dev.yml logs backend'

# Check port
ssh root@77.42.75.203 'netstat -tlnp | grep 8787'
```

### Database connection error

```bash
# Verify PostgreSQL is running
ssh root@77.42.75.203 'docker-compose -f /opt/sidechain/docker-compose.dev.yml ps | grep postgres'

# Check logs
ssh root@77.42.75.203 'docker-compose -f /opt/sidechain/docker-compose.dev.yml logs postgres'
```

### GitHub Actions deployment fails

1. Check the workflow logs on GitHub (Actions tab)
2. Verify SSH secrets are set correctly
3. Test SSH manually: `ssh -i ~/.ssh/sidechain_deploy root@77.42.75.203`
4. Check server logs: `ssh root@77.42.75.203 'docker-compose logs'`

### Port in use

Edit `.env` and change `BACKEND_PORT=8787` to another port, then restart:
```bash
ssh root@77.42.75.203 'docker-compose -f /opt/sidechain/docker-compose.dev.yml restart'
```

---

## Next Steps

- ✓ Set up monitoring (e.g., Prometheus)
- ✓ Add SSL certificate (nginx + Let's Encrypt)
- ✓ Configure custom domain
- ✓ Set up database backups
- ✓ Add log aggregation
- ✓ Set up alerts for failures

See `terraform/README.md` for scaling to multiple servers.
