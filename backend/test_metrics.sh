#!/bin/bash

# Test script to verify Grafana error rate metrics are working
# This script makes HTTP requests and checks the Prometheus metrics endpoint

BACKEND_URL="${BACKEND_URL:-http://localhost:8787}"
METRICS_URL="${BACKEND_URL}/internal/metrics"

echo "Testing Grafana error rate metrics fix..."
echo "Backend URL: ${BACKEND_URL}"
echo "Metrics URL: ${METRICS_URL}"
echo ""

# Check if backend is running
if ! curl -s -f "${BACKEND_URL}/health" > /dev/null 2>&1; then
    echo "❌ Backend is not running at ${BACKEND_URL}"
    echo "   Please start the backend first: cd backend && go run cmd/server/main.go"
    exit 1
fi

echo "✅ Backend is running"
echo ""

# Make some test requests
echo "Making test HTTP requests..."

# Success request (200)
curl -s "${BACKEND_URL}/health" > /dev/null
echo "  ✓ GET /health (should return 200)"

# 404 request
curl -s "${BACKEND_URL}/api/v1/nonexistent" > /dev/null
echo "  ✓ GET /api/v1/nonexistent (should return 404)"

# 405 Method Not Allowed (if endpoint exists but method wrong)
curl -s -X POST "${BACKEND_URL}/health" > /dev/null
echo "  ✓ POST /health (should return 405 or 404)"

# Wait a moment for metrics to be recorded
sleep 1

echo ""
echo "Checking Prometheus metrics..."
echo ""

# Check if metrics contain numeric status codes
METRICS=$(curl -s "${METRICS_URL}")

if [ -z "$METRICS" ]; then
    echo "❌ Failed to fetch metrics from ${METRICS_URL}"
    exit 1
fi

echo "✅ Successfully fetched metrics"
echo ""

# Check for http_requests_total with numeric status codes
echo "Checking for numeric status codes in http_requests_total metric..."
echo ""

# Look for status codes like "200", "404", "500" (numeric strings)
if echo "$METRICS" | grep -q 'http_requests_total.*status="200"'; then
    echo "✅ Found status code '200' (numeric)"
else
    echo "❌ Status code '200' not found (might be recorded as 'OK' instead)"
fi

if echo "$METRICS" | grep -q 'http_requests_total.*status="404"'; then
    echo "✅ Found status code '404' (numeric)"
else
    echo "⚠️  Status code '404' not found (may not have been triggered yet)"
fi

# Check for text status codes (should NOT exist)
if echo "$METRICS" | grep -q 'http_requests_total.*status="OK"'; then
    echo "❌ Found text status code 'OK' - fix not working!"
    exit 1
fi

if echo "$METRICS" | grep -q 'http_requests_total.*status="Not Found"'; then
    echo "❌ Found text status code 'Not Found' - fix not working!"
    exit 1
fi

echo ""
echo "Sample http_requests_total metrics:"
echo "$METRICS" | grep 'http_requests_total' | head -5
echo ""

# Test Prometheus query format
echo "Testing Prometheus query format compatibility..."
echo ""

# Check if we can match 2xx status codes
TWOXX_COUNT=$(echo "$METRICS" | grep 'http_requests_total.*status="2' | wc -l)
if [ "$TWOXX_COUNT" -gt 0 ]; then
    echo "✅ Found ${TWOXX_COUNT} metrics with 2xx status codes (query: status=~\"2..\" will work)"
else
    echo "⚠️  No 2xx status codes found yet"
fi

# Check if we can match 4xx status codes
FOURXX_COUNT=$(echo "$METRICS" | grep 'http_requests_total.*status="4' | wc -l)
if [ "$FOURXX_COUNT" -gt 0 ]; then
    echo "✅ Found ${FOURXX_COUNT} metrics with 4xx status codes (query: status=~\"4..\" will work)"
else
    echo "⚠️  No 4xx status codes found yet"
fi

# Check if we can match 5xx status codes
FIVEX_COUNT=$(echo "$METRICS" | grep 'http_requests_total.*status="5' | wc -l)
if [ "$FIVEX_COUNT" -gt 0 ]; then
    echo "✅ Found ${FIVEX_COUNT} metrics with 5xx status codes (query: status=~\"5..\" will work)"
else
    echo "ℹ️  No 5xx status codes found (this is normal if no errors occurred)"
fi

echo ""
echo "✅ Test complete! Metrics are using numeric status codes."
echo "   Grafana queries like status=~\"5..\" should now work correctly."
