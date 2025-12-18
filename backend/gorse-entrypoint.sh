#!/bin/bash
set -e

# Create config directory if it doesn't exist
mkdir -p /tmp/gorse

# Use sed to substitute environment variables (more portable than envsubst)
sed -e "s|\${POSTGRES_USER}|${POSTGRES_USER}|g" \
    -e "s|\${POSTGRES_PASSWORD}|${POSTGRES_PASSWORD}|g" \
    /tmp/gorse.toml.template > /tmp/gorse/config.toml

# Start Gorse with the substituted config using -c flag
exec /usr/bin/gorse-in-one -c /tmp/gorse/config.toml
