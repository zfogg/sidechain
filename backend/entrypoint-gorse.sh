#!/bin/bash
# Entrypoint script for Gorse - substitutes environment variables in config and starts gorse

set -e

# Default values for development
POSTGRES_USER="${POSTGRES_USER:-sidechain}"
POSTGRES_PASSWORD="${POSTGRES_PASSWORD:-sidechain_dev_password}"
POSTGRES_HOST="${POSTGRES_HOST:-postgres}"
POSTGRES_PORT="${POSTGRES_PORT:-5432}"
POSTGRES_GORSE_DB="${POSTGRES_GORSE_DB:-gorse}"
REDIS_URL="${REDIS_URL:-redis://redis:6379}"
GORSE_API_KEY="${GORSE_API_KEY:-sidechain_gorse_api_key}"

# Create the actual config from template using sed for substitution
echo "Generating Gorse config with environment variables..."

# Use sed to substitute variables
sed \
  -e "s|\${POSTGRES_USER}|$POSTGRES_USER|g" \
  -e "s|\${POSTGRES_PASSWORD}|$POSTGRES_PASSWORD|g" \
  -e "s|\${POSTGRES_HOST}|$POSTGRES_HOST|g" \
  -e "s|\${POSTGRES_PORT}|$POSTGRES_PORT|g" \
  -e "s|\${POSTGRES_GORSE_DB}|$POSTGRES_GORSE_DB|g" \
  -e "s|\${REDIS_URL}|$REDIS_URL|g" \
  -e "s|\${GORSE_API_KEY}|$GORSE_API_KEY|g" \
  /etc/gorse/config.toml.template > /etc/gorse/config.toml

echo "Config generated:"
cat /etc/gorse/config.toml

# Start gorse
echo "Starting Gorse..."
exec /usr/bin/gorse-in-one -c /etc/gorse/config.toml
