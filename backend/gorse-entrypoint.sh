#!/bin/bash
set -e

# Substitute environment variables in gorse.toml template
envsubst < /tmp/gorse.toml.template > /etc/gorse/config.toml

# Start Gorse with the substituted config
exec /usr/bin/gorse-in-one
