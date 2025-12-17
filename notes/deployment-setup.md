# Sidechain Production Deployment Setup

## Deployment Infrastructure Created

Complete Infrastructure-as-Code setup for automated backend deployment using GitHub Actions and Terraform.

## Files Added

### Workflows
- `.github/workflows/deploy.yml` - Automatic deployment workflow on push to main

### Terraform Configuration
- `terraform/main.tf` - Infrastructure configuration
- `terraform/provisioning/setup.sh` - Server provisioning script for Ubuntu
- `terraform/README.md` - Terraform documentation and scaling guide

### Documentation
- `DEPLOYMENT.md` - Detailed step-by-step deployment guide
- `DEPLOYMENT_CHECKLIST.md` - Quick 30-minute setup checklist
- `DEPLOYMENT_SUMMARY.md` - Overview of deployment architecture

## Key Features

✅ Automatic deployment on git push to main branch
✅ Docker Compose orchestration with health checks
✅ SSH-based secure deployment
✅ Environment configuration management
✅ Zero-downtime updates
✅ Multi-server scaling support
✅ Reproducible infrastructure

## Quick Start

1. **Provision server**: Run setup.sh to install Docker
2. **Configure environment**: Set database password, API keys in .env
3. **Clone repository**: git clone to /opt/sidechain
4. **Setup GitHub deployment**: Add SSH deploy key and secrets
5. **Push to main**: GitHub Actions automatically deploys

## Deployment Flow

```
git push origin main
    ↓
GitHub Actions triggered
    ↓
SSH to 77.42.75.203
    ↓
git pull + docker-compose up -d --build
    ↓
Health check validation
    ↓
Deployment complete
```

## Next Steps

1. Run provisioning script on server
2. Configure .env file with credentials
3. Clone repository
4. Add GitHub secrets for CI/CD
5. Push to main to trigger first deployment

See DEPLOYMENT_CHECKLIST.md for detailed steps.
