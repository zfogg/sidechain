#!/bin/bash
set -e

# Create config directory if it doesn't exist
mkdir -p /tmp/gorse

# Substitute environment variables in gorse.toml template
envsubst < /tmp/gorse.toml.template > /tmp/gorse/config.toml

# Start Gorse with the substituted config using -c flag
exec /usr/bin/gorse-in-one -c /tmp/gorse/config.toml
