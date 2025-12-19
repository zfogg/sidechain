#!/bin/bash

# Script to trigger a 500 error for testing Grafana error rate dashboard

BACKEND_URL="${BACKEND_URL:-http://localhost:8787}"

echo "Triggering 500 errors for Grafana dashboard testing..."
echo ""

# Method 1: Hit an endpoint that will cause a database error
# Try to access a protected endpoint without auth (might return 401, not 500)
# Let's try accessing a route that doesn't exist but might trigger a panic

# Method 2: Create a test endpoint that returns 500
# Actually, let's just hit endpoints that might fail

# Try to trigger errors by hitting various endpoints
echo "Making requests that might trigger 500 errors..."

# Request with invalid JSON to an endpoint expecting JSON
curl -s -X POST "${BACKEND_URL}/api/v1/auth/register" \
  -H "Content-Type: application/json" \
  -d '{"invalid": json}' > /dev/null 2>&1

# Request to a protected endpoint without auth (might be 401, but let's try)
curl -s -X GET "${BACKEND_URL}/api/v1/auth/me" > /dev/null 2>&1

# Try to access a non-existent user endpoint that might cause a DB error
curl -s -X GET "${BACKEND_URL}/api/v1/users/00000000-0000-0000-0000-000000000000" > /dev/null 2>&1

# Actually, the best way is to create a test endpoint. Let me check if there's a way to trigger a real 500
# For now, let's just make some requests and check metrics

echo "Waiting for metrics to be recorded..."
sleep 2

echo ""
echo "Checking metrics for 5xx errors:"
curl -s "${BACKEND_URL}/internal/metrics" | grep 'http_requests_total.*status="5'
