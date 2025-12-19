#!/bin/bash

# Script to trigger a 500 error and verify it appears in Grafana
# This will make multiple requests to ensure the error rate is visible

BACKEND_URL="${BACKEND_URL:-http://localhost:8787}"

echo "Triggering 500 errors for Grafana dashboard..."
echo ""

# Check if test-error endpoint exists (after backend restart)
if curl -s -f "${BACKEND_URL}/test-error" > /dev/null 2>&1; then
    echo "Using /test-error endpoint..."
    for i in {1..10}; do
        curl -s "${BACKEND_URL}/test-error" > /dev/null 2>&1
        echo -n "."
    done
    echo ""
else
    echo "Test endpoint not available. Triggering errors via invalid requests..."
    # Make requests that might cause 500 errors
    for i in {1..10}; do
        # Invalid JSON to endpoints expecting JSON
        curl -s -X POST "${BACKEND_URL}/api/v1/auth/register" \
            -H "Content-Type: application/json" \
            -d 'invalid json' > /dev/null 2>&1
        
        # Try accessing protected endpoints incorrectly
        curl -s -X GET "${BACKEND_URL}/api/v1/users/00000000-0000-0000-0000-000000000000" \
            -H "Authorization: Bearer invalid-token" > /dev/null 2>&1
        
        echo -n "."
    done
    echo ""
fi

echo ""
echo "Waiting for metrics to be recorded..."
sleep 3

echo ""
echo "Checking for 5xx errors in metrics:"
curl -s "${BACKEND_URL}/internal/metrics" | grep -E 'http_requests_total.*status="5' || echo "No 5xx errors found yet"

echo ""
echo "Checking error rate calculation:"
TOTAL=$(curl -s "${BACKEND_URL}/internal/metrics" | grep 'http_requests_total{' | wc -l)
ERRORS=$(curl -s "${BACKEND_URL}/internal/metrics" | grep -E 'http_requests_total.*status="5' | wc -l)
echo "Total request metrics: ${TOTAL}"
echo "5xx error metrics: ${ERRORS}"

if [ "$ERRORS" -gt 0 ]; then
    echo ""
    echo "✅ 5xx errors detected! Check Grafana dashboard at:"
    echo "   http://localhost:3000/d/system-health/system-health?from=now-5m&to=now&timezone=browser&orgId=1&refresh=10s"
    echo ""
    echo "The 'Error Rate %' gauge should show a value > 0"
else
    echo ""
    echo "⚠️  No 5xx errors found. You may need to:"
    echo "   1. Restart the backend to enable /test-error endpoint"
    echo "   2. Or trigger actual errors in your application"
fi
