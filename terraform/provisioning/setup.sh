#!/bin/bash
# Sidechain Backend Server Provisioning Script
# Usage: bash ./setup.sh
# Or remotely: ssh user@host 'bash -s' < setup.sh

set -euo pipefail

DEPLOYMENT_DIR="${DEPLOYMENT_DIR:-/opt/sidechain}"
DOCKER_COMPOSE_FILE="${DOCKER_COMPOSE_FILE:-docker-compose.dev.yml}"

echo "=== Sidechain Backend Provisioning ==="
echo "Deployment directory: $DEPLOYMENT_DIR"
echo "Docker compose file: $DOCKER_COMPOSE_FILE"

# Update system packages
echo "→ Updating system packages..."
apt-get update
apt-get upgrade -y

# Install Docker
echo "→ Installing Docker..."
if ! command -v docker &> /dev/null; then
    apt-get install -y \
        apt-transport-https \
        ca-certificates \
        curl \
        gnupg \
        lsb-release

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

# Create SSH directory for deploy keys
echo "→ Setting up SSH directory..."
mkdir -p /root/.ssh
chmod 700 /root/.ssh

# Create .env file (needs to be populated by user)
if [ ! -f "$DEPLOYMENT_DIR/.env" ]; then
    echo "→ Creating .env file (you'll need to populate this)"
    cat > "$DEPLOYMENT_DIR/.env" << 'EOF'
# Backend Configuration
BACKEND_PORT=8787
POSTGRES_USER=sidechain
POSTGRES_PASSWORD=CHANGE_ME
POSTGRES_DB=sidechain

# GetStream Configuration
GETSTREAM_API_KEY=CHANGE_ME
GETSTREAM_API_SECRET=CHANGE_ME

# JWT Configuration
JWT_SECRET=CHANGE_ME

# Audio CDN Configuration
CDN_BUCKET=CHANGE_ME
CDN_REGION=CHANGE_ME
CDN_ACCESS_KEY=CHANGE_ME
CDN_SECRET_KEY=CHANGE_ME
EOF
    chmod 600 "$DEPLOYMENT_DIR/.env"
    echo "  ⚠️  Edit $DEPLOYMENT_DIR/.env with your credentials"
fi

# Create systemd service for auto-restart (optional)
echo "→ Setting up systemd service..."
cat > /etc/systemd/system/sidechain.service << 'EOF'
[Unit]
Description=Sidechain Backend Service
After=docker.service
Requires=docker.service

[Service]
Type=oneshot
RemainAfterExit=yes
WorkingDirectory=/opt/sidechain
ExecStart=/usr/local/bin/docker-compose -f docker-compose.dev.yml up -d
ExecStop=/usr/local/bin/docker-compose -f docker-compose.dev.yml down
Restart=on-failure
RestartSec=10s
User=root

[Install]
WantedBy=multi-user.target
EOF

systemctl daemon-reload
systemctl enable sidechain

echo ""
echo "=== Provisioning Complete ==="
echo ""
echo "Next steps:"
echo "1. Edit $DEPLOYMENT_DIR/.env with your configuration"
echo "2. Clone the repository: git clone <repo> $DEPLOYMENT_DIR"
echo "3. Start the service: systemctl start sidechain"
echo "4. Check status: docker-compose -f $DEPLOYMENT_DIR/$DOCKER_COMPOSE_FILE ps"
echo ""
