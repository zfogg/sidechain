#!/bin/bash
set -e

# Substitute environment variables in gorse.toml
envsubst < /etc/gorse/config.toml.template > /etc/gorse/config.toml

# Start Gorse with the substituted config
exec /usr/bin/gorse-in-one
