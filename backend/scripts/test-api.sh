#!/bin/bash
# Test API endpoints with seed data

set -e

BASE_URL="${API_URL:-http://localhost:8787}"
API_BASE="${BASE_URL}/api/v1"

echo "🧪 Testing API endpoints with seed data..."
echo ""

# First, we need to login to get a token
echo "1. Getting authentication token..."
echo "   (Note: Using first seeded user with password 'password123')"

# Get a user email from the database or use a known one
# For now, we'll try to login with a test user
LOGIN_RESPONSE=$(curl -s -X POST "${API_BASE}/auth/login" \
  -H "Content-Type: application/json" \
  -d '{
    "email": "test@example.com",
    "password": "password123"
  }' || echo "")

if [ -z "$LOGIN_RESPONSE" ] || echo "$LOGIN_RESPONSE" | grep -q "error"; then
  echo "   ⚠️  Could not login - you may need to:"
  echo "      1. Start the server: make dev"
  echo "      2. Register a user or use an existing seeded user"
  echo "      3. Update this script with correct credentials"
  echo ""
  echo "   Testing unauthenticated endpoints only..."
  echo ""
  
  # Test health endpoint
  echo "2. Testing health endpoint..."
  curl -s "${BASE_URL}/health" | jq '.' || echo "   ❌ Health check failed"
  echo ""
  
  exit 0
fi

TOKEN=$(echo "$LOGIN_RESPONSE" | jq -r '.token // .access_token // empty')

if [ -z "$TOKEN" ] || [ "$TOKEN" = "null" ]; then
  echo "   ❌ Failed to get token"
  echo "   Response: $LOGIN_RESPONSE"
  exit 1
fi

echo "   ✅ Got token"
echo ""

# Test authenticated endpoints
echo "2. Testing authenticated endpoints..."

echo "   📋 Getting user profile..."
curl -s -X GET "${API_BASE}/auth/me" \
  -H "Authorization: Bearer ${TOKEN}" | jq '.username, .email, .post_count' || echo "   ❌ Failed"

echo ""
echo "   📰 Getting timeline feed..."
curl -s -X GET "${API_BASE}/feed/timeline?limit=5" \
  -H "Authorization: Bearer ${TOKEN}" | jq '.posts | length' || echo "   ❌ Failed"

echo ""
echo "   🌍 Getting global feed..."
curl -s -X GET "${API_BASE}/feed/global?limit=5" \
  -H "Authorization: Bearer ${TOKEN}" | jq '.posts | length' || echo "   ❌ Failed"

echo ""
echo "   🔍 Searching users..."
curl -s -X GET "${API_BASE}/search/users?q=test&limit=5" \
  -H "Authorization: Bearer ${TOKEN}" | jq '.users | length' || echo "   ❌ Failed"

echo ""
echo "✅ API testing complete!"
echo ""
echo "💡 Tip: To test with a specific seeded user, first login via:"
echo "   curl -X POST ${API_BASE}/auth/login \\"
echo "     -H 'Content-Type: application/json' \\"
echo "     -d '{\"email\":\"USER_EMAIL\",\"password\":\"password123\"}'"
