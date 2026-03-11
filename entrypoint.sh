#!/bin/bash
set -e

cd /home/tibia

# Populate volume directories with initial data if empty
for dir in dat map origmap mon npc usr; do
    if [ -z "$(ls -A ./$dir 2>/dev/null)" ]; then
        echo "Initializing $dir/ from default data..."
        cp -a /initial-data/$dir/* ./$dir/
    fi
done

# Copy .tibia config if not present
if [ ! -f .tibia ]; then
    cp /initial-data/.tibia .tibia
fi

# Apply QueryManagerPassword from environment if set (requires disguise)
if [ -n "$QUERYMANAGER_PASSWORD" ]; then
    KEY="Pm-,o%yD"
    PW_LEN=${#QUERYMANAGER_PASSWORD}
    DISGUISED=""
    for (( i=0; i<PW_LEN; i++ )); do
        K=$(printf '%d' "'${KEY:$i:1}")
        P=$(printf '%d' "'${QUERYMANAGER_PASSWORD:$i:1}")
        C=$(( (K - P + 0x5E) % 0x5E + 0x21 ))
        DISGUISED+=$(printf "\\$(printf '%03o' "$C")")
    done
    sed -i "s/^QueryManager.*/QueryManager = {(\"127.0.0.1\",7173,\"$DISGUISED\")}/" .tibia
fi

# Ensure log and save directories exist
mkdir -p ./log ./save

# Remove stale PID file from ungraceful shutdown
rm -f ./save/game.pid

# Wait for querymanager to be reachable
echo "Waiting for querymanager on port 7173..."
for i in $(seq 1 60); do
    if nc -z 127.0.0.1 7173 2>/dev/null; then
        echo "querymanager is reachable."
        break
    fi
    sleep 1
done

echo "Starting game server..."
exec ./game
