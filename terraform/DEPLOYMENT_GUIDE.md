# Sidechain Backend - Production Deployment Guide

This guide shows how to deploy Sidechain to a new server using Infrastructure as Code with Terraform. Everything is fully reproducible - you only need an IP address and you can deploy the entire production stack.

## Prerequisites

1. **Terraform** installed (v1.0+)
2. **An Ubuntu 20.04+ server** with SSH access
3. **GitHub deploy key** for cloning the repository
4. **Domain name** pointing to your server
5. **GitHub Actions secrets** already configured (from initial setup)

## Step 1: Generate Required Keys

### SSH Key for Server Access

```bash
ssh-keygen -t ed25519 -f ~/.ssh/sidechain-server-key -N ""
```

### GitHub Deploy Key

1. Generate a new SSH keypair:
```bash
ssh-keygen -t ed25519 -f ~/.ssh/sidechain-deploy-key -N ""
```

2. Add the **public** key as a Deploy Key:
   - Go to https://github.com/zfogg/sidechain/settings/keys/new
   - Paste contents of `~/.ssh/sidechain-deploy-key.pub`
   - Click "Allow write access" (optional, for future deploys)
   - Save

3. Keep the **private** key for Terraform

## Step 2: Prepare Terraform Configuration

### Copy the example config:

```bash
cd terraform
cp terraform.tfvars.example terraform.tfvars
```

### Edit `terraform.tfvars` with your values:

```hcl
server_host              = "YOUR.SERVER.IP.ADDRESS"
server_user              = "root"
ssh_private_key_path     = "~/.ssh/sidechain-server-key"
github_deploy_key        = file("~/.ssh/sidechain-deploy-key")
deploy_domain            = "api.yourdomain.com"
letsencrypt_email        = "your-email@example.com"
postgres_password        = "strong-random-password-here"
jwt_secret               = "another-strong-random-32char-secret"
```

### Generate strong secrets:

```bash
# Generate a strong password
openssl rand -base64 32

# Generate JWT secret
openssl rand -base64 32
```

## Step 3: Set Up DNS

Point your domain to your server's IP:

```bash
# In your DNS provider, create an A record:
api.yourdomain.com  A  YOUR.SERVER.IP.ADDRESS
```

Allow up to 24 hours for DNS propagation (usually instant).

## Step 4: Deploy with Terraform

### Initialize Terraform:

```bash
cd terraform
terraform init
```

### Preview the changes:

```bash
terraform plan
```

### Apply the configuration:

```bash
terraform apply
```

This will:
- SSH into your server
- Install Docker and Docker Compose
- Clone the Sidechain repository
- Create the `.env` file with all production credentials
- Set up the `update-and-restart.sh` script
- Configure a cron job for automatic updates
- Start all services (backend, database, Redis, Elasticsearch, Gorse, Caddy, Watchtower)
- Wait for services to be healthy

### Provisioning takes ~2-3 minutes. You'll see:

```
null_resource.provision_server: Provisioning with 'remote-exec'...
...
Apply complete! Resources have been created.

Outputs:

backend_url = "https://api.yourdomain.com/api/v1"
health_check_url = "https://api.yourdomain.com/health"
deployment_complete = "✅ Sidechain backend provisioned successfully"
ci_cd_workflow = "Git Push → GitHub Actions Build → Docker Hub Push → Watchtower Auto-Deploy"
```

## Step 5: Verify Deployment

### Test the health check:

```bash
curl https://api.yourdomain.com/health
```

You should see:
```json
{"service":"sidechain-backend","status":"ok","timestamp":"2025-12-17T15:30:05Z"}
```

### SSH to the server and verify services:

```bash
ssh root@YOUR.SERVER.IP

# Check all services are running
docker-compose -f /opt/sidechain/backend/docker-compose.prod.yml ps

# Check backend logs
docker logs sidechain-backend --tail 20

# Monitor auto-update log
tail -f /tmp/sidechain-update.log
```

## Step 6: Configure GitHub Secrets (if not already done)

These should already be configured, but verify:

1. Go to https://github.com/zfogg/sidechain/settings/secrets/actions
2. Ensure these secrets exist:
   - `DOCKER_USERNAME` - Your Docker Hub username
   - `DOCKER_PASSWORD` - Your Docker Hub password

## Deployment Architecture

```
Your Local Machine
    ↓ (git push)
GitHub Repository
    ↓ (trigger)
GitHub Actions Build Workflow
    ↓ (build and push)
Docker Hub
    ↓ (image available)
Production Server (Watchtower)
    ↓ (detects new image every 30s)
Watchtower Auto-Pulls & Restarts
    ↓ (every 30 seconds max)
Services Running New Version
```

## Automated CI/CD Workflow

Once deployed, all you need to do is push to the main branch:

```bash
# Make code changes
git add backend/...
git commit -m "fix: update something"
git push origin main
```

The automatic deployment pipeline:
1. **Triggers** - GitHub Actions detects push to main
2. **Builds** - Creates new Docker image
3. **Pushes** - Uploads to Docker Hub (`zfogg/sidechain-backend:latest`)
4. **Detects** - Watchtower sees new image (max 30s delay)
5. **Deploys** - Pulls new image and restarts containers (automatic)

## Configuration Management

The `update-and-restart.sh` cron job runs every minute:
- Pulls latest code from git
- Detects if `docker-compose.prod.yml` or `Caddyfile` changed
- Automatically restarts services if config changed
- Every 5 minutes, ensures services are running with `docker-compose up -d`

## What Gets Provisioned

### Infrastructure
- Docker and Docker Compose installed and configured
- SSH key for GitHub access
- Cron job for automatic updates

### Containers
- **Backend** - Sidechain Go API server
- **PostgreSQL** - Database
- **Redis** - Caching and real-time features
- **Elasticsearch** - Search engine
- **Gorse** - Recommendation engine
- **Caddy** - HTTPS reverse proxy with Let's Encrypt
- **Watchtower** - Automatic Docker image updates

### Configuration
- `.env` file with all secrets
- `docker-compose.prod.yml` for container orchestration
- `Caddyfile` for HTTPS and reverse proxy
- Auto-update script for git synchronization

## Monitoring

### Check service status:
```bash
ssh root@YOUR.SERVER.IP
docker-compose -f /opt/sidechain/backend/docker-compose.prod.yml ps
```

### View logs:
```bash
# Backend logs
docker logs sidechain-backend -f

# All logs
docker-compose -f /opt/sidechain/backend/docker-compose.prod.yml logs -f

# Auto-update log
tail -f /tmp/sidechain-update.log
```

### Check auto-deployment status:
```bash
# See recent updates
tail /tmp/sidechain-update.log

# Monitor real-time
tail -f /tmp/sidechain-update.log
```

## Troubleshooting

### Service not starting
```bash
ssh root@YOUR.SERVER.IP
docker logs sidechain-backend --tail 50
docker logs sidechain-postgres --tail 50
```

### Let's Encrypt certificate issues
```bash
docker logs sidechain-caddy --tail 50
# Usually caused by DNS not pointing to server yet
```

### Auto-update script not running
```bash
crontab -l  # Check cron job exists
tail /tmp/sidechain-update.log  # Check logs
```

### Git authentication failing
```bash
# Verify GitHub deploy key is configured
cat /root/.ssh/config
cat /root/.ssh/github_deploy_key

# Test SSH connection
ssh -T git@github.com
```

## Scaling & Additional Deployments

To deploy to another server:

1. Get the new server's IP address
2. Update `server_host` in `terraform.tfvars`
3. Run `terraform apply` again
4. Terraform will provision the new server identically

All servers use the same GitHub repository and Docker images, ensuring consistency.

## Removing a Deployment

To tear down all resources:

```bash
terraform destroy
```

This will:
- Remove all containers and volumes
- Stop services
- Clean up the deployment directory (optional - you can keep it)

## Security Notes

- All secrets are passed directly to the server via SSH
- Secrets are stored in the `.env` file with 600 permissions (read-only by root)
- GitHub deploy key has read-only access to the repository
- All traffic to the API is encrypted with Let's Encrypt HTTPS
- Caddy automatically handles SSL certificate renewal

## Support

If you encounter issues:

1. Check Terraform output for error details
2. SSH to the server and check Docker logs
3. Verify DNS is pointing to the server
4. Ensure GitHub deploy key is added to the repository
5. Check that all required secrets are in `terraform.tfvars`
