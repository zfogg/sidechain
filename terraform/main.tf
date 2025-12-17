terraform {
  required_version = ">= 1.0"
  required_providers {
    null = {
      source  = "hashicorp/null"
      version = "~> 3.2"
    }
  }
}

provider "null" {}

# This configuration documents the infrastructure setup for Sidechain backend deployment
# For existing servers, use the provisioning script manually
# For new servers (e.g., on Hetzner, DigitalOcean), use terraform to provision

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

variable "github_deploy_key_path" {
  description = "Path to the GitHub deploy private key for cloning the repo"
  type        = string
  sensitive   = true
}

variable "docker_compose_file" {
  description = "Docker compose file to use"
  type        = string
  default     = "docker-compose.dev.yml"
}

variable "backend_port" {
  description = "Port to expose for the backend service"
  type        = number
  default     = 8787
}

variable "postgres_port" {
  description = "Port to expose for PostgreSQL"
  type        = number
  default     = 5432
}

# Local provisioning script content
locals {
  provisioning_script = file("${path.module}/provisioning/setup.sh")
  deploy_script       = file("${path.module}/provisioning/deploy.sh")
}

# Output provisioning commands for manual execution
output "provisioning_instructions" {
  value = <<-EOT
    To provision an existing server manually, run:

    ssh -p ${var.server_port} ${var.server_user}@${var.server_host} 'bash -s' < ${path.module}/provisioning/setup.sh

    To set up deployment on the server after provisioning:

    1. Add your GitHub deploy key to the server
    2. Configure environment variables
    3. Push to GitHub main branch to trigger deployment
  EOT
}

output "server_connection_string" {
  value = "${var.server_user}@${var.server_host}:${var.server_port}"
}

output "deployment_directory" {
  value = var.deployment_dir
}

output "accessible_endpoints" {
  value = {
    api       = "http://${var.server_host}:${var.backend_port}"
    database  = "${var.server_host}:${var.postgres_port}"
  }
}
