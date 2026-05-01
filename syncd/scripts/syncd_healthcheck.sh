#!/usr/bin/env bash

HEALTH_FILE="/var/run/syncd/health.json"

if [ ! -f "$HEALTH_FILE" ]; then
    echo "Health file not found"
    exit 1
fi

STATUS=$(grep '"watchdog_status"' "$HEALTH_FILE" | awk -F'"' '{print $4}')

if [ "$STATUS" == "healthy" ]; then
    echo "syncd is healthy"
    exit 0
else
    echo "syncd is stuck/unhealthy"
    exit 1
fi
