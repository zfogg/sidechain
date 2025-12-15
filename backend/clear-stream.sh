#!/bin/bash
# Clear and resync Stream.io feeds with database posts

set -e

echo "🧹 Clearing and resyncing Stream.io feeds..."
echo "=============================================="
echo ""

# Run sync from within docker container to avoid vendoring issues
docker exec sidechain-backend /bin/sh -c "cd /app && go run -mod=mod cmd/sync-stream/main.go"

echo ""
echo "✅ Stream.io feeds cleared and resynced!"
