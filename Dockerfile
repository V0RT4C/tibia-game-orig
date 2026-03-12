# Stage 1: Build
FROM alpine:3.19 AS builder

RUN apk add --no-cache build-base cmake openssl-dev linux-headers

WORKDIR /src
COPY gameserver/ gameserver/
RUN cd gameserver && cmake -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build -j$(nproc)

# Run tests during build — image won't build if tests fail
RUN cd gameserver/build && ctest --output-on-failure

# Stage 2: Runtime
FROM alpine:3.19

RUN apk add --no-cache libssl3 libstdc++ libgcc netcat-openbsd

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
