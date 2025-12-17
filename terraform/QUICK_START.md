# Quick Start - Deploy Sidechain in 5 Minutes

## TL;DR - One Command to Deploy

```bash
# 1. Prepare your configuration
cp terraform.tfvars.example terraform.tfvars
# Edit terraform.tfvars with your IP, domain, and secrets

# 2. Deploy everything
terraform init && terraform apply

# 3. Done! Your backend is now running
curl https://YOUR.DOMAIN.COM/health
```

## Step-by-Step Checklist

- [ ] Have server IP address and SSH access
- [ ] Have domain name pointing to server
- [ ] Generated GitHub deploy key
- [ ] Generated server SSH key
- [ ] Generated strong secrets (passwords)

## Minimal terraform.tfvars

```hcl
server_host              = "1.2.3.4"
ssh_private_key_path     = "~/.ssh/server_key"
github_deploy_key        = file("~/.ssh/github_deploy_key")
deploy_domain            = "api.example.com"
letsencrypt_email        = "admin@example.com"
postgres_password        = "use-strong-random-password"
jwt_secret               = "use-32-char-random-secret"
```

## What Terraform Does

```
terraform init    → Initializes Terraform
terraform plan    → Shows what will be created
terraform apply   → Actually deploys everything
```

## After Deployment

```bash
# SSH to server
ssh root@YOUR.SERVER.IP

# Check services
docker-compose -f /opt/sidechain/backend/docker-compose.prod.yml ps

# Check logs
docker logs sidechain-backend

# Monitor deployments
tail -f /tmp/sidechain-update.log
```

## Automated Deployments After This

```bash
git push origin main
→ GitHub Actions builds image
→ Pushes to Docker Hub
→ Watchtower auto-deploys (within 30 seconds)
```

## Extra Configuration

For optional features like S3, OAuth, email, see `terraform.tfvars.example`

## Need Help?

See detailed guide: `DEPLOYMENT_GUIDE.md`
