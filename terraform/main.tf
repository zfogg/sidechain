terraform {
  required_version = ">= 1.0"
  required_providers {
    null = {
      source  = "hashicorp/null"
      version = "~> 3.2"
    }
    aws = {
      source  = "hashicorp/aws"
      version = "~> 5.0"
    }
  }
}

provider "null" {}

provider "aws" {
  profile = "zfogg"
  region  = "us-west-2"
}

# ============================================================
# Sidechain Production Deployment - Infrastructure as Code
# ============================================================
# This configuration provisions a complete Sidechain backend
# deployment on a new server. It is fully reproducible and
# requires only an SSH-accessible server and GitHub credentials.

# ============================================================
# Server Configuration
# ============================================================

variable "server_host" {
  description = "IP address or hostname of the deployment server"
  type        = string
}

variable "server_user" {
  description = "SSH user for deployment"
  type        = string
  default     = "root"
}

variable "server_port" {
  description = "SSH port"
  type        = number
  default     = 22
}

variable "deployment_dir" {
  description = "Directory where the application will be deployed"
  type        = string
  default     = "/opt/sidechain"
}

variable "ssh_private_key_path" {
  description = "Path to SSH private key for server access"
  type        = string
  sensitive   = true
}

# ============================================================
# GitHub Configuration
# ============================================================

variable "github_deploy_key" {
  description = "GitHub deploy key (SSH private key for git@github.com)"
  type        = string
  sensitive   = true
  validation {
    condition     = can(regex("BEGIN OPENSSH PRIVATE KEY|BEGIN RSA PRIVATE KEY", var.github_deploy_key))
    error_message = "Must be a valid SSH private key."
  }
}

# ============================================================
# Production Environment Variables
# ============================================================

variable "deploy_domain" {
  description = "Domain name for the deployment (used for Caddy and Let's Encrypt)"
  type        = string
}

variable "letsencrypt_email" {
  description = "Email address for Let's Encrypt certificate renewal notifications"
  type        = string
}

# ============================================================
# Database Configuration
# ============================================================

variable "postgres_password" {
  description = "PostgreSQL password for sidechain user"
  type        = string
  sensitive   = true
}

variable "postgres_user" {
  description = "PostgreSQL username"
  type        = string
  default     = "sidechain"
}

variable "postgres_db" {
  description = "PostgreSQL database name"
  type        = string
  default     = "sidechain"
}

# ============================================================
# Security Configuration
# ============================================================

variable "jwt_secret" {
  description = "JWT secret for authentication tokens"
  type        = string
  sensitive   = true
  validation {
    condition     = length(var.jwt_secret) >= 32
    error_message = "JWT secret must be at least 32 characters."
  }
}

variable "gorse_api_key" {
  description = "Gorse recommendation engine API key"
  type        = string
  sensitive   = true
  default     = "sidechain_gorse_api_key"
}

# ============================================================
# Stream.io Configuration (Optional)
# ============================================================

variable "stream_api_key" {
  description = "Stream.io API key for chat and notifications"
  type        = string
  sensitive   = true
  default     = ""
}

variable "stream_api_secret" {
  description = "Stream.io API secret"
  type        = string
  sensitive   = true
  default     = ""
}

# ============================================================
# AWS/S3 Configuration (Optional)
# ============================================================

variable "storage_provider" {
  description = "Storage provider (local or aws)"
  type        = string
  default     = "local"
}

variable "aws_region" {
  description = "AWS region for S3"
  type        = string
  default     = "us-west-2"
}

variable "aws_bucket" {
  description = "AWS S3 bucket for audio storage"
  type        = string
  default     = "sidechain-media"
}

variable "aws_access_key_id" {
  description = "AWS access key ID"
  type        = string
  sensitive   = true
  default     = ""
}

variable "aws_secret_access_key" {
  description = "AWS secret access key"
  type        = string
  sensitive   = true
  default     = ""
}

variable "cdn_base_url" {
  description = "CDN base URL for audio downloads"
  type        = string
  default     = ""
}

# ============================================================
# OAuth Configuration (Optional)
# ============================================================

variable "google_client_id" {
  description = "Google OAuth client ID"
  type        = string
  sensitive   = true
  default     = ""
}

variable "google_client_secret" {
  description = "Google OAuth client secret"
  type        = string
  sensitive   = true
  default     = ""
}

variable "discord_client_id" {
  description = "Discord OAuth client ID"
  type        = string
  sensitive   = true
  default     = ""
}

variable "discord_client_secret" {
  description = "Discord OAuth client secret"
  type        = string
  sensitive   = true
  default     = ""
}

# ============================================================
# Email Configuration (Optional - AWS SES)
# ============================================================

variable "ses_from_email" {
  description = "Email address for sending password reset emails"
  type        = string
  default     = "hi@sidechain.live"
}

variable "ses_from_name" {
  description = "Name to appear in password reset emails"
  type        = string
  default     = "Sidechain"
}

# ============================================================
# Generate .env file content
# ============================================================

locals {
  env_file_content = trimspace(<<-EOT
    # === Server Configuration ===
    PORT=8787
    HOST=0.0.0.0
    ENVIRONMENT=production
    LOG_LEVEL=info

    # === Database ===
    DATABASE_URL=postgres://${var.postgres_user}:${var.postgres_password}@postgres:5432/${var.postgres_db}?sslmode=disable
    POSTGRES_USER=${var.postgres_user}
    POSTGRES_PASSWORD=${var.postgres_password}
    POSTGRES_DB=${var.postgres_db}

    # === Security ===
    JWT_SECRET=${var.jwt_secret}
    CORS_ORIGINS=https://${var.deploy_domain}

    # === Domain & HTTPS ===
    DOMAIN=${var.deploy_domain}
    LETSENCRYPT_EMAIL=${var.letsencrypt_email}
    BASE_URL=https://${var.deploy_domain}

    # === Gorse Recommendation Engine ===
    GORSE_URL=http://gorse:8088
    GORSE_API_KEY=${var.gorse_api_key}

    # === Stream.io Chat ===
    STREAM_API_KEY=${var.stream_api_key}
    STREAM_API_SECRET=${var.stream_api_secret}

    # === Storage ===
    STORAGE_PROVIDER=${var.storage_provider}
    AWS_REGION=${var.aws_region}
    AWS_BUCKET=${var.aws_bucket}
    AWS_ACCESS_KEY_ID=${var.aws_access_key_id}
    AWS_SECRET_ACCESS_KEY=${var.aws_secret_access_key}
    CDN_BASE_URL=${var.cdn_base_url != "" ? var.cdn_base_url : "https://${var.deploy_domain}"}

    # === OAuth ===
    GOOGLE_CLIENT_ID=${var.google_client_id}
    GOOGLE_CLIENT_SECRET=${var.google_client_secret}
    DISCORD_CLIENT_ID=${var.discord_client_id}
    DISCORD_CLIENT_SECRET=${var.discord_client_secret}

    # === Email (AWS SES) ===
    SES_FROM_EMAIL=${var.ses_from_email}
    SES_FROM_NAME=${var.ses_from_name}
  EOT
  )

  provisioning_script = file("${path.module}/provisioning/setup.sh")
}

# ============================================================
# Provisioning - Run setup script on remote server
# ============================================================

resource "null_resource" "provision_server" {
  provisioner "remote-exec" {
    inline = ["mkdir -p /tmp/sidechain-deploy"]

    connection {
      type        = "ssh"
      user        = var.server_user
      private_key = file(var.ssh_private_key_path)
      host        = var.server_host
      port        = var.server_port
      timeout     = "10m"
    }
  }

  provisioner "file" {
    source      = "${path.module}/provisioning/setup.sh"
    destination = "/tmp/sidechain-deploy/setup.sh"

    connection {
      type        = "ssh"
      user        = var.server_user
      private_key = file(var.ssh_private_key_path)
      host        = var.server_host
      port        = var.server_port
      timeout     = "10m"
    }
  }

  provisioner "remote-exec" {
    inline = [
      "export DEPLOYMENT_DIR='${var.deployment_dir}'",
      "export DEPLOY_DOMAIN='${var.deploy_domain}'",
      "export LETSENCRYPT_EMAIL='${var.letsencrypt_email}'",
      "export GITHUB_DEPLOY_KEY='${var.github_deploy_key}'",
      "export ENV_FILE_CONTENT='${base64encode(local.env_file_content)}'",
      "export POSTGRES_PASSWORD='${var.postgres_password}'",
      "export JWT_SECRET='${var.jwt_secret}'",
      "bash /tmp/sidechain-deploy/setup.sh"
    ]

    connection {
      type        = "ssh"
      user        = var.server_user
      private_key = file(var.ssh_private_key_path)
      host        = var.server_host
      port        = var.server_port
      timeout     = "25m"
    }
  }

  triggers = {
    github_deploy_key = md5(var.github_deploy_key)
    env_content       = md5(local.env_file_content)
  }
}

# ============================================================
# Outputs
# ============================================================

output "deployment_complete" {
  description = "Deployment status"
  value       = "✅ Sidechain backend provisioned successfully"
}

output "server_connection_string" {
  description = "SSH connection string"
  value       = "${var.server_user}@${var.server_host}:${var.server_port}"
}

output "deployment_directory" {
  description = "Application deployment directory"
  value       = var.deployment_dir
}

output "deployment_domain" {
  description = "Public domain for the deployment"
  value       = var.deploy_domain
}

output "backend_url" {
  description = "Backend API URL"
  value       = "https://${var.deploy_domain}/api/v1"
}

output "health_check_url" {
  description = "Health check endpoint"
  value       = "https://${var.deploy_domain}/health"
}

output "ci_cd_workflow" {
  description = "Automated CI/CD workflow"
  value       = <<-EOT
    Git Push → GitHub Actions Build → Docker Hub Push → Watchtower Auto-Deploy

    Any commit to main branch will automatically:
    1. Build Docker image
    2. Push to Docker Hub (zfogg/sidechain-backend:latest)
    3. Watchtower detects new image (every 30 seconds)
    4. Services restart with new image (within 30 seconds)
    5. Cron job ensures configuration is synchronized from git
  EOT
}

output "next_steps" {
  description = "Instructions for first-time setup"
  value       = <<-EOT
    ✅ Server provisioning complete!

    1. Connect to your server:
       ssh ${var.server_user}@${var.server_host}

    2. Verify services are running:
       docker-compose -f ${var.deployment_dir}/backend/docker-compose.prod.yml ps

    3. Check logs:
       docker logs sidechain-backend

    4. Test the API:
       curl https://${var.deploy_domain}/health

    5. Monitor auto-deployments:
       tail -f /tmp/sidechain-update.log

    Subsequent Updates:
    - Just push to GitHub main branch
    - GitHub Actions will automatically build and deploy
    - Watchtower on the server will handle container updates
    - Cron job will keep configuration synchronized
  EOT
}
