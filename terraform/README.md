# Sidechain Backend Deployment

Infrastructure-as-Code setup for deploying Sidechain backend to custom servers with automatic CI/CD via GitHub Actions.

## Overview

This setup enables:
- **Automated deployments** on every push to main branch
- **Reproducible server setup** using provisioning scripts
- **Multi-server scalability** through Terraform
- **Zero-downtime updates** with docker-compose

## Architecture

```
GitHub Repository (main branch push)
          ↓
GitHub Actions Workflow
          ↓
SSH to Production Server
          ↓
Git pull + docker-compose up --build
          ↓
Health check API endpoint
```

## Quick Start

### 1. Provision Your Existing Server (One-time Setup)

On your server at `77.42.75.203`:

```bash
# SSH to the server
ssh root@77.42.75.203

# Run provisioning script
bash -s < <(curl -s https://raw.githubusercontent.com/YOUR_ORG/sidechain/main/terraform/provisioning/setup.sh)
```

Or locally:
```bash
ssh -p 22 root@77.42.75.203 'bash -s' < terraform/provisioning/setup.sh
```

This will:
- ✓ Update system packages
- ✓ Install Docker & Docker Compose
- ✓ Create deployment directory (`/opt/sidechain`)
- ✓ Set up systemd service for auto-restart
- ✓ Create `.env` template (you must edit it)

### 2. Configure Deployment Credentials (On Server)

After provisioning, SSH to server and configure:

```bash
ssh root@77.42.75.203

# Edit environment file
nano /opt/sidechain/.env
```

Set these variables:
```bash
# Backend Configuration
BACKEND_PORT=8787
POSTGRES_USER=sidechain
POSTGRES_PASSWORD=YOUR_SECURE_PASSWORD

# GetStream (get from getstream.io dashboard)
GETSTREAM_API_KEY=your-key
GETSTREAM_API_SECRET=your-secret

# JWT Secret (generate: openssl rand -base64 32)
JWT_SECRET=your-jwt-secret

# CDN Configuration (if using S3 or similar)
CDN_BUCKET=your-bucket
CDN_REGION=your-region
CDN_ACCESS_KEY=your-key
CDN_SECRET_KEY=your-secret
```

### 3. Clone Repository (On Server)

```bash
cd /opt/sidechain
git clone https://github.com/YOUR_ORG/sidechain.git .
# Or if using SSH:
git clone git@github.com:YOUR_ORG/sidechain.git .
```

### 4. Configure GitHub Actions Secrets

In your GitHub repository settings:

**Settings → Secrets and variables → Actions**

Add these secrets:

| Secret Name | Value | Example |
|------------|-------|---------|
| `DEPLOY_SERVER_HOST` | Your server IP | `77.42.75.203` |
| `DEPLOY_SERVER_USER` | SSH username | `root` |
| `DEPLOY_SERVER_PORT` | SSH port | `22` |
| `DEPLOY_SSH_KEY` | Private SSH key contents | See below |

#### Generate and Add SSH Deploy Key

**Option A: Using existing key**
```bash
# Get your private key (if already using it for SSH)
cat ~/.ssh/id_rsa
# Copy the entire output and paste into GitHub secret DEPLOY_SSH_KEY
```

**Option B: Generate dedicated deploy key**
```bash
ssh-keygen -t ed25519 -f deploy_key -C "github-actions-sidechain" -N ""
cat deploy_key
# Copy to GitHub secret DEPLOY_SSH_KEY
```

Then add public key to server:
```bash
# On server
ssh root@77.42.75.203 'echo "DEPLOY_KEY_CONTENTS" >> ~/.ssh/authorized_keys'
```

### 5. Test Deployment

Push to main branch:
```bash
git push origin main
```

Watch deployment in GitHub:
- Go to repository → **Actions** tab
- Click on "Deploy Backend to Production" workflow
- Monitor real-time logs

Check server status:
```bash
ssh root@77.42.75.203 'docker-compose -f /opt/sidechain/docker-compose.dev.yml ps'
```

## Manual Deployment

To deploy manually without GitHub Actions:

```bash
ssh root@77.42.75.203 << 'EOF'
  set -euo pipefail
  cd /opt/sidechain
  git fetch origin main
  git reset --hard origin/main
  docker-compose -f docker-compose.dev.yml down
  docker-compose -f docker-compose.dev.yml up -d --build
EOF
```

## Terraform Usage (for New Servers)

### Use Terraform to provision a new Ubuntu server

Create `terraform.tfvars`:
```hcl
server_host              = "your-new-server-ip"
server_user              = "root"
server_port              = 22
deployment_dir           = "/opt/sidechain"
github_deploy_key_path   = "~/.ssh/deploy_key"
docker_compose_file      = "docker-compose.dev.yml"
backend_port             = 8787
postgres_port            = 5432
```

Initialize and apply:
```bash
cd terraform
terraform init
terraform plan -var-file=terraform.tfvars
terraform apply -var-file=terraform.tfvars
```

This outputs provisioning instructions and server connection details.

## Monitoring & Troubleshooting

### Check deployment status

```bash
ssh root@77.42.75.203 'docker-compose -f /opt/sidechain/docker-compose.dev.yml ps'
```

### View logs

```bash
ssh root@77.42.75.203 'docker-compose -f /opt/sidechain/docker-compose.dev.yml logs -f backend'
```

### Common Issues

**SSH key permission denied**
```bash
# Ensure key has correct permissions on server
chmod 600 ~/.ssh/authorized_keys
chmod 700 ~/.ssh
```

**API health check failing**
```bash
# Check what's wrong
docker-compose -f /opt/sidechain/docker-compose.dev.yml logs
docker-compose -f /opt/sidechain/docker-compose.dev.yml ps
```

**Port already in use**
- Change `BACKEND_PORT` in `.env`
- Ensure firewall allows traffic on that port

### Database Migrations

If migrations fail, run manually:

```bash
ssh root@77.42.75.203 'docker-compose -f /opt/sidechain/docker-compose.dev.yml exec backend go run cmd/migrate/main.go up'
```

## Scaling to Multiple Servers

Once configured, you can easily add more servers:

1. **Provision new server** (run provisioning script)
2. **Add GitHub secret** for each server if using different hosts
3. **Create parallel deployment jobs** in GitHub Actions workflow

Example multi-server workflow snippet:
```yaml
deploy:
  strategy:
    matrix:
      server:
        - host: 77.42.75.203
          name: production-1
        - host: 77.42.75.204
          name: production-2
  steps:
    - run: ssh ${{ matrix.server.host }} '...'
```

## Security Best Practices

- ✓ Use ed25519 SSH keys (stronger than RSA)
- ✓ Restrict SSH key permissions (600)
- ✓ Use separate deploy keys for CI/CD
- ✓ Store secrets in GitHub (never in code)
- ✓ Validate .env file before deployment
- ✓ Use strong PostgreSQL passwords
- ✓ Enable firewall rules to limit access

## File Structure

```
terraform/
├── main.tf                          # Terraform configuration
├── provisioning/
│   ├── setup.sh                     # Server provisioning script
│   └── deploy.sh                    # Manual deployment helper
└── README.md                        # This file

.github/workflows/
└── deploy.yml                       # GitHub Actions workflow

```

## Advanced: Custom Domains & SSL

To expose backend via custom domain:

1. **Update DNS** to point to your server IP
2. **Add reverse proxy** (nginx) in docker-compose
3. **Get SSL certificate** (Let's Encrypt with Certbot)
4. **Update firewall** to allow port 80/443

Example nginx config can be added to `docker-compose.dev.yml`.

## Support

For issues:
1. Check GitHub Actions logs
2. SSH to server and check docker-compose logs
3. Review .env configuration
4. Ensure firewall allows SSH and backend ports
