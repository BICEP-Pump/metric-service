#!/bin/bash
set -e

# Configuration
MOCK_URL="http://localhost:5000/verify"
TIMEOUT=90
INTERVAL=3

echo "--- Starting Integration Test ---"
docker compose -f docker-compose.test.yaml up -d --build

echo "Waiting for service to register and send metrics (Timeout: ${TIMEOUT}s)..."
found=0
start_time=$(date +%s)

while [ $(($(date +%s) - start_time)) -lt $TIMEOUT ]; do
    response=$(curl -s $MOCK_URL || echo "{}")
    
    # Use sed for robust JSON count extraction (works even without jq)
    reg_count=$(echo "$response" | sed -n 's/.*"registrations_count": \([0-9]*\).*/\1/p' || echo 0)
    met_count=$(echo "$response" | sed -n 's/.*"metrics_count": \([0-9]*\).*/\1/p' || echo 0)
    
    # Default to 0 if extraction failed
    reg_count=${reg_count:-0}
    met_count=${met_count:-0}

    echo "DEBUG: Raw response sample: $(echo $response | cut -c1-100)..."
    echo "Current status: Registrations=$reg_count, Batches=$met_count"
    
    if [ "$reg_count" -gt 0 ] && [ "$met_count" -gt 0 ]; then
        echo "SUCCESS: Integration test passed!"
        found=1
        break
    fi
    
    sleep $INTERVAL
done

if [ $found -eq 0 ]; then
    echo "ERROR: Integration test timed out after ${TIMEOUT}s"
    echo "--- SERVICE LOGS ---"
    docker compose -f docker-compose.test.yaml logs bicep-metric-service
    echo "--- MOCK BACKEND LOGS ---"
    docker compose -f docker-compose.test.yaml logs mock-backend
    docker compose -f docker-compose.test.yaml down
    exit 1
fi

docker compose -f docker-compose.test.yaml down
echo "--- Integration Test Completed Successfully ---"
