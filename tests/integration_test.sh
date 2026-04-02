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
    # curl -sS is better (shows errors, but stays silent on progress)
    # --fail: return non-zero if HTTP error status like 404/500
    # --connect-timeout 2: don't hang too long
    response=$(curl -sS --fail --connect-timeout 2 "$MOCK_URL" || echo "ERRORED")

    if [ "$response" == "ERRORED" ] || [ -z "$response" ]; then
        echo "DEBUG: Waiting for mock-backend to be reachable..."
    else
        # Robust parsing: handles both minified and pretty JSON, spaces and newlines
        reg_count=$(echo "$response" | grep -oE '"registrations_count":\s*[0-9]+' | grep -oE '[0-9]+' | head -1 || echo 0)
        met_count=$(echo "$response" | grep -oE '"metrics_count":\s*[0-9]+' | grep -oE '[0-9]+' | head -1 || echo 0)
        
        # Default to 0 if extraction failed
        reg_count=${reg_count:-0}
        met_count=${met_count:-0}

        echo "DEBUG: Response: $response"
        echo "Current status: Registrations=$reg_count, Batches=$met_count"
        
        if [ "$met_count" -gt 0 ] && [ "$reg_count" -gt 0 ]; then
            echo "SUCCESS: Integration test passed!"
            found=1
            break
        fi
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
