#!/bin/sh
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
    i=0
    while [ "$i" -lt "$PW_LEN" ]; do
        k_char=$(printf '%s' "$KEY" | cut -c$((i + 1)))
        p_char=$(printf '%s' "$QUERYMANAGER_PASSWORD" | cut -c$((i + 1)))
        K=$(printf '%d' "'$k_char")
        P=$(printf '%d' "'$p_char")
        C=$(( (K - P + 94) % 94 + 33 ))
        DISGUISED="${DISGUISED}$(printf "\\$(printf '%03o' "$C")")"
        i=$((i + 1))
    done
    sed -i "s/^QueryManager.*/QueryManager = {(\"127.0.0.1\",7173,\"$DISGUISED\")}/" .tibia
fi

# Ensure log and save directories exist
mkdir -p ./log ./save

# Remove stale PID file from ungraceful shutdown
rm -f ./save/game.pid

# Wait for querymanager to be reachable
echo "Waiting for querymanager on port 7173..."
i=0
while [ "$i" -lt 60 ]; do
    if nc -z 127.0.0.1 7173 2>/dev/null; then
        echo "querymanager is reachable."
        break
    fi
    sleep 1
    i=$((i + 1))
done

echo "Starting game server..."
exec ./game
