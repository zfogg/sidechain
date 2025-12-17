#!/bin/bash
# Sidechain Backend Server Provisioning Script
# This script sets up a complete production environment on a new server
# Usage: ssh root@server 'bash -s' < setup.sh

set -euo pipefail

# Configuration
DEPLOYMENT_DIR="${DEPLOYMENT_DIR:-/opt/sidechain}"
DEPLOY_DOMAIN="${DEPLOY_DOMAIN:-example.com}"
LETSENCRYPT_EMAIL="${LETSENCRYPT_EMAIL:-admin@example.com}"
GITHUB_DEPLOY_KEY="${GITHUB_DEPLOY_KEY:-}"
ENV_FILE_CONTENT="${ENV_FILE_CONTENT:-}"
POSTGRES_PASSWORD="${POSTGRES_PASSWORD:-}"
JWT_SECRET="${JWT_SECRET:-}"

echo "=== Sidechain Backend Production Provisioning ==="
echo "Deployment directory: $DEPLOYMENT_DIR"
echo "Domain: $DEPLOY_DOMAIN"
echo "Email: $LETSENCRYPT_EMAIL"

# Update system packages
echo "→ Updating system packages..."
apt-get update
apt-get upgrade -y

# Install required tools
echo "→ Installing required tools..."
apt-get install -y \
    git \
    curl \
    wget \
    gnupg \
    ca-certificates \
    lsb-release \
    apt-transport-https

# Install Docker
echo "→ Installing Docker..."
if ! command -v docker &> /dev/null; then
    curl -fsSL https://download.docker.com/linux/ubuntu/gpg | gpg --dearmor -o /usr/share/keyrings/docker-archive-keyring.gpg
    echo \
        "deb [arch=amd64 signed-by=/usr/share/keyrings/docker-archive-keyring.gpg] https://download.docker.com/linux/ubuntu \
        $(lsb_release -cs) stable" | tee /etc/apt/sources.list.d/docker.list > /dev/null
    apt-get update
    apt-get install -y docker-ce docker-ce-cli containerd.io docker-compose-plugin
    systemctl start docker
    systemctl enable docker
else
    echo "  Docker already installed"
fi

# Install Docker Compose (standalone)
echo "→ Installing Docker Compose..."
if ! command -v docker-compose &> /dev/null; then
    DOCKER_COMPOSE_VERSION="2.24.0"
    curl -L "https://github.com/docker/compose/releases/download/v${DOCKER_COMPOSE_VERSION}/docker-compose-$(uname -s)-$(uname -m)" -o /usr/local/bin/docker-compose
    chmod +x /usr/local/bin/docker-compose
else
    echo "  Docker Compose already installed"
fi

# Verify versions
echo "→ Installed versions:"
docker --version
docker-compose --version

# Create deployment directory
echo "→ Setting up deployment directory: $DEPLOYMENT_DIR"
mkdir -p "$DEPLOYMENT_DIR"
cd "$DEPLOYMENT_DIR"

# Set up SSH directory and deploy key
echo "→ Setting up SSH directory..."
mkdir -p /root/.ssh
chmod 700 /root/.ssh

if [ -n "$GITHUB_DEPLOY_KEY" ]; then
    echo "→ Adding GitHub deploy key..."
    echo "$GITHUB_DEPLOY_KEY" > /root/.ssh/github_deploy_key
    chmod 600 /root/.ssh/github_deploy_key

    # Configure git to use the deploy key
    cat > /root/.ssh/config << 'SSHCONFIG'
Host github.com
    HostName github.com
    User git
    IdentityFile /root/.ssh/github_deploy_key
    StrictHostKeyChecking=accept-new
SSHCONFIG
    chmod 600 /root/.ssh/config
fi

# Clone repository
echo "→ Cloning repository..."
if [ ! -d "$DEPLOYMENT_DIR/.git" ]; then
    git clone git@github.com:zfogg/sidechain.git "$DEPLOYMENT_DIR"
else
    echo "  Repository already cloned"
fi

# Create .env file
echo "→ Creating .env file..."
if [ -n "$ENV_FILE_CONTENT" ]; then
    # Decode base64 content passed from Terraform
    echo "$ENV_FILE_CONTENT" | base64 -d > "$DEPLOYMENT_DIR/backend/.env"
    chmod 600 "$DEPLOYMENT_DIR/backend/.env"
    echo "  .env file created from provided content"
else
    echo "  ⚠️  No .env content provided - you'll need to create it manually"
    echo "  Run: ssh root@$DEPLOY_DOMAIN 'cat > /opt/sidechain/backend/.env' < your_env_file"
fi

# Create the auto-update script
echo "→ Creating auto-update script..."
cat > "$DEPLOYMENT_DIR/update-and-restart.sh" << 'UPDATESCRIPT'
#!/bin/bash
set -e

cd /opt/sidechain

# Pull latest code from git
if git pull origin main > /tmp/git-pull.log 2>&1; then
    echo "[$(date)] Git pull successful" >> /tmp/sidechain-update.log

    # Check if docker-compose or Caddyfile changed
    if git diff HEAD~1 HEAD --name-only 2>/dev/null | grep -qE "(docker-compose|Caddyfile)"; then
        echo "[$(date)] docker-compose or Caddyfile changed, restarting services" >> /tmp/sidechain-update.log
        cd /opt/sidechain/backend
        docker-compose -f docker-compose.prod.yml up -d
        echo "[$(date)] Services updated successfully" >> /tmp/sidechain-update.log
    fi
else
    echo "[$(date)] Git pull failed" >> /tmp/sidechain-update.log
    cat /tmp/git-pull.log >> /tmp/sidechain-update.log
    exit 1
fi

# Periodic restart every 5 minutes to ensure everything is up
MINUTE=$(date +%M)
if [ $((MINUTE % 5)) -eq 0 ]; then
    echo "[$(date)] Running periodic restart with prod compose" >> /tmp/sidechain-update.log
    cd /opt/sidechain/backend
    docker-compose -f docker-compose.prod.yml up -d
    echo "[$(date)] Periodic restart completed" >> /tmp/sidechain-update.log
fi
UPDATESCRIPT

chmod +x "$DEPLOYMENT_DIR/update-and-restart.sh"
echo "  Auto-update script created"

# Set up cron job for auto-deployment
echo "→ Setting up cron job..."
CRON_JOB="* * * * * /opt/sidechain/update-and-restart.sh"
(crontab -l 2>/dev/null | grep -v "update-and-restart" || true; echo "$CRON_JOB") | crontab -
echo "  Cron job installed"

# Start the backend services
echo "→ Starting services..."
cd "$DEPLOYMENT_DIR/backend"
docker-compose -f docker-compose.prod.yml up -d
echo "  Services started"

# Wait for services to be healthy
echo "→ Waiting for services to be healthy..."
sleep 30
docker-compose -f docker-compose.prod.yml ps

echo ""
echo "=== Provisioning Complete ==="
echo ""
echo "Deployment Information:"
echo "  Server:      $DEPLOY_DOMAIN"
echo "  Directory:   $DEPLOYMENT_DIR"
echo "  Domain:      $DEPLOY_DOMAIN"
echo "  Email:       $LETSENCRYPT_EMAIL"
echo ""
echo "Services:"
docker-compose -f docker-compose.prod.yml ps
echo ""
echo "Next steps:"
echo "1. Verify all services are healthy: docker-compose -f $DEPLOYMENT_DIR/backend/docker-compose.prod.yml ps"
echo "2. Check logs: docker logs sidechain-backend"
echo "3. Test API: curl http://$DEPLOY_DOMAIN/health"
echo ""
