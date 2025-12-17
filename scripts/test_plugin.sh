#!/bin/bash

# Kill any running plugins
pkill -f "Sidechain_artefacts/Debug/Standalone/Sidechain" 2>/dev/null || true
sleep 1

# Clear old log
rm plugin.log 2>/dev/null || true

# Rebuild plugin
make plugin-fast

# Start plugin in background
./plugin/build/src/core/Sidechain_artefacts/Debug/Standalone/Sidechain >/dev/null 2>&1 &
PLUGIN_PID=$!

# Wait for it to start
sleep 2

echo "Plugin running with PID: $PLUGIN_PID"
