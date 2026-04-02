#!/bin/bash
set -e

# Configuration
MOCK_URL="http://localhost:5000/verify"
TIMEOUT=60
INTERVAL=2

echo "--- Starting Integration Test ---"
docker compose -f docker-compose.test.yaml up -d --build

echo "Waiting for service to register and send metrics..."
found=0
start_time=$(date +%s)

while [ $(($(date +%s) - start_time)) -lt $TIMEOUT ]; do
    response=$(curl -s $MOCK_URL || echo "{}")
    
    reg_count=$(echo $response | grep -o '"registrations_count": [0-9]*' | grep -o '[0-9]*' || echo 0)
    met_count=$(echo $response | grep -o '"metrics_count": [0-9]*' | grep -o '[0-9]*' || echo 0)
    
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
    echo "Service Logs:"
    docker compose -f docker-compose.test.yaml logs bicep-metric-service
    echo "Mock Backend Logs:"
    docker compose -f docker-compose.test.yaml logs mock-backend
    docker compose -f docker-compose.test.yaml down
    exit 1
fi

docker compose -f docker-compose.test.yaml down
echo "--- Integration Test Completed Successfully ---"
