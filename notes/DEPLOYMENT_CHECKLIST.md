# Sidechain Deployment Checklist

Quick checklist to get production deployment running in ~30 minutes.

## Phase 1: Server Provisioning (5 min)

- [ ] Run provisioning script on server:
  ```bash
  ssh root@77.42.75.203 'bash -s' < terraform/provisioning/setup.sh
  ```
- [ ] Verify Docker installed:
  ```bash
  ssh root@77.42.75.203 'docker --version && docker-compose --version'
  ```

## Phase 2: Configure Environment (5 min)

- [ ] SSH to server and edit `.env`:
  ```bash
  ssh root@77.42.75.203 'nano /opt/sidechain/.env'
  ```
- [ ] Fill in these values from your services:
  - `POSTGRES_PASSWORD` - Generate: `openssl rand -base64 32`
  - `GETSTREAM_API_KEY` - From getstream.io dashboard
  - `GETSTREAM_API_SECRET` - From getstream.io dashboard
  - `JWT_SECRET` - Generate: `openssl rand -base64 32`
  - `CDN_*` - If using S3 or similar

- [ ] Save file (Ctrl+O, Enter, Ctrl+X in nano)

## Phase 3: Clone Repository (3 min)

- [ ] Clone repo to deployment directory:
  ```bash
  ssh root@77.42.75.203 'cd /opt/sidechain && git clone https://github.com/YOUR_USERNAME/sidechain.git .'
  ```
- [ ] Or with SSH key if you have one set up:
  ```bash
  ssh root@77.42.75.203 'cd /opt/sidechain && git clone git@github.com:YOUR_USERNAME/sidechain.git .'
  ```

## Phase 4: Generate Deploy SSH Key (3 min)

On your local machine:

- [ ] Generate new SSH key:
  ```bash
  ssh-keygen -t ed25519 \
    -f ~/.ssh/sidechain_deploy \
    -C "sidechain-github-actions" \
    -N ""
  ```

- [ ] Add public key to server:
  ```bash
  ssh-copy-id -i ~/.ssh/sidechain_deploy.pub root@77.42.75.203
  ```

- [ ] Get private key for GitHub (copy entire output):
  ```bash
  cat ~/.ssh/sidechain_deploy
  ```

## Phase 5: Configure GitHub Secrets (5 min)

In GitHub repository:

- [ ] Go to **Settings** → **Secrets and variables** → **Actions**

- [ ] Add `DEPLOY_SERVER_HOST`:
  - Value: `77.42.75.203`

- [ ] Add `DEPLOY_SERVER_USER`:
  - Value: `root`

- [ ] Add `DEPLOY_SERVER_PORT`:
  - Value: `22`

- [ ] Add `DEPLOY_SSH_KEY`:
  - Value: (entire contents of `~/.ssh/sidechain_deploy` private key file)

## Phase 6: Test Manual Deployment (5 min)

- [ ] Start services manually:
  ```bash
  ssh -i ~/.ssh/sidechain_deploy root@77.42.75.203 << 'EOF'
    cd /opt/sidechain
    docker-compose -f docker-compose.dev.yml up -d --build
  EOF
  ```

- [ ] Wait 10 seconds, then verify:
  ```bash
  ssh root@77.42.75.203 'docker-compose -f /opt/sidechain/docker-compose.dev.yml ps'
  ```

- [ ] Check API health:
  ```bash
  curl http://77.42.75.203:8787/health
  ```

## Phase 7: Test GitHub Actions (2 min)

- [ ] Push to main branch:
  ```bash
  git add backend/
  git commit -m "test deployment"
  git push origin main
  ```

- [ ] Monitor deployment:
  - Go to GitHub repository
  - Click **Actions** tab
  - Watch "Deploy Backend to Production" workflow

- [ ] Verify deployment succeeded:
  ```bash
  curl http://77.42.75.203:8787/health
  ```

---

## Verification Checklist

After deployment is complete:

- [ ] API responds to health check: `curl http://77.42.75.203:8787/health`
- [ ] All containers running: `docker-compose ps`
- [ ] No errors in logs: `docker-compose logs`
- [ ] Database connected: Check backend logs for connection success
- [ ] GitHub Actions workflow completes successfully
- [ ] Automatic deployment works on next push to main

---

## Documentation

For detailed information, see:

- **Full Setup Guide**: `DEPLOYMENT.md`
- **Terraform & Scaling**: `terraform/README.md`
- **GitHub Actions Workflow**: `.github/workflows/deploy.yml`
- **Provisioning Script**: `terraform/provisioning/setup.sh`

---

## Troubleshooting Quick Links

| Problem | Solution |
|---------|----------|
| SSH connection refused | Check IP is correct, firewall allows port 22 |
| Docker not found | Re-run provisioning script |
| API not responding | Check `docker-compose logs` for errors |
| GitHub Actions fails | Check GitHub secrets are set correctly |
| Port already in use | Change `BACKEND_PORT` in `.env`, restart |

---

## Support Commands

```bash
# View all logs
ssh root@77.42.75.203 'docker-compose -f /opt/sidechain/docker-compose.dev.yml logs -f'

# View just backend
ssh root@77.42.75.203 'docker-compose -f /opt/sidechain/docker-compose.dev.yml logs backend'

# Restart everything
ssh root@77.42.75.203 'docker-compose -f /opt/sidechain/docker-compose.dev.yml restart'

# Check disk usage
ssh root@77.42.75.203 'df -h'

# Check running processes
ssh root@77.42.75.203 'docker ps -a'
```

---

**Estimated Total Time**: ~30 minutes (first time)
**Subsequent Deployments**: Automatic via GitHub Actions on push to main
