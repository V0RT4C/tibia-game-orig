# Stage 1: Build
FROM debian:bookworm-slim AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    g++ gcc make libssl-dev linux-libc-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY gameserver/ gameserver/
RUN cd gameserver && make clean && make -j$(nproc)

# Stage 2: Runtime
FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
    libssl3 netcat-openbsd \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /home/tibia

# Game binary and RSA key
COPY --from=builder /src/gameserver/build/game ./game
COPY gameserver/tibia.pem ./tibia.pem

# Initial game data (copied to volumes on first run if empty)
COPY core/dat/ /initial-data/dat/
COPY core/map/ /initial-data/map/
COPY core/origmap/ /initial-data/origmap/
COPY core/mon/ /initial-data/mon/
COPY core/npc/ /initial-data/npc/

# Pre-create usr subdirectories {00..99}
RUN mkdir -p /initial-data/usr && \
    for i in $(seq -w 0 99); do mkdir -p /initial-data/usr/$i; done

# Static config
COPY core/dottibia_example /initial-data/.tibia

COPY gameserver/entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

EXPOSE 7172

ENTRYPOINT ["/entrypoint.sh"]
