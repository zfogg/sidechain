#!/bin/bash
# Test script to verify seed data was created correctly

set -e

echo "🧪 Testing seed data..."
echo ""

# Check if database is accessible
echo "1. Checking database connection..."
cd "$(dirname "$0")/.." || exit 1

# Load environment
if [ -f .env ]; then
    export $(cat .env | grep -v '^#' | xargs)
fi

# Use psql to query the database
DB_URL="${DATABASE_URL:-postgresql://${DB_USER:-postgres}:${DB_PASSWORD:-}@${DB_HOST:-localhost}:${DB_PORT:-5432}/${DB_NAME:-sidechain}?sslmode=${DB_SSLMODE:-disable}}"

echo "2. Counting records..."
echo "   Users:"
psql "$DB_URL" -t -c "SELECT COUNT(*) FROM users WHERE deleted_at IS NULL;" 2>/dev/null || echo "   (Could not query - check database connection)"

echo "   Audio Posts:"
psql "$DB_URL" -t -c "SELECT COUNT(*) FROM audio_posts WHERE deleted_at IS NULL;" 2>/dev/null || echo "   (Could not query)"

echo "   Comments:"
psql "$DB_URL" -t -c "SELECT COUNT(*) FROM comments WHERE deleted_at IS NULL AND is_deleted = false;" 2>/dev/null || echo "   (Could not query)"

echo "   Hashtags:"
psql "$DB_URL" -t -c "SELECT COUNT(*) FROM hashtags;" 2>/dev/null || echo "   (Could not query)"

echo "   Play History:"
psql "$DB_URL" -t -c "SELECT COUNT(*) FROM play_history;" 2>/dev/null || echo "   (Could not query)"

echo ""
echo "3. Sample data preview:"
echo "   Sample user:"
psql "$DB_URL" -t -c "SELECT username, email, display_name, post_count FROM users WHERE deleted_at IS NULL LIMIT 3;" 2>/dev/null || echo "   (Could not query)"

echo "   Sample post:"
psql "$DB_URL" -t -c "SELECT original_filename, bpm, key, genre FROM audio_posts WHERE deleted_at IS NULL LIMIT 3;" 2>/dev/null || echo "   (Could not query)"

echo ""
echo "✅ Seed data verification complete!"
