#!/bin/bash
# Doris E2E Test Initialization Script

set -e

# Use environment variable or default to 'doris' (docker-compose service name)
DORIS_HOST=${DORIS_HOST:-doris}

# Add initial delay to let Doris fully start before making connections
echo "Waiting 10 seconds before checking Doris status..."
sleep 10

echo "Waiting for Doris FE to be ready at $DORIS_HOST:9030..."
# Wait for Doris FE to be ready (max 100 seconds, check every 5 seconds)
for i in {1..20}; do
    if timeout 5 mysql -h $DORIS_HOST -P 9030 -u root --connect-timeout=3 -e "SELECT 1" &>/dev/null; then
        echo "Doris FE is ready!"
        break
    fi
    echo "Waiting for Doris FE... ($i/20)"
    sleep 5
done

echo "Waiting for Doris BE to be ready..."
# Wait for at least one BE to be alive (max 100 seconds, check every 5 seconds)
for i in {1..20}; do
    BE_ALIVE=$(timeout 5 mysql -h $DORIS_HOST -P 9030 -u root --connect-timeout=3 -e "SHOW BACKENDS" 2>/dev/null | grep -c "true" | head -n1 || echo "0")
    if [ "$BE_ALIVE" -gt 0 ]; then
        echo "Doris BE is alive and ready!"
        break
    fi
    echo "Waiting for BE to be alive... ($i/20)"
    sleep 5
done

echo "Verifying Doris cluster status..."
timeout 5 mysql -h $DORIS_HOST -P 9030 -u root --connect-timeout=3 -e "SHOW BACKENDS\G" 2>/dev/null || echo "Warning: Cannot display BE status"

echo "Creating test user and database..."
timeout 10 mysql -h $DORIS_HOST -P 9030 -u root --connect-timeout=3 <<EOF

-- Create test database
CREATE DATABASE IF NOT EXISTS test_db;

-- Use test database
USE test_db;

-- Create test table with custom_single_flatten protocol structure
CREATE TABLE IF NOT EXISTS test_table (
    time BIGINT,
    content STRING,
    value STRING,
    __tag__hostip STRING,
    __tag__hostname STRING
) DUPLICATE KEY(time)
DISTRIBUTED BY HASH(time) BUCKETS 1
PROPERTIES (
    "replication_num" = "1"
);
EOF

echo "Doris initialization completed!"

# Create marker file for healthcheck
touch /tmp/init_done

sleep infinity