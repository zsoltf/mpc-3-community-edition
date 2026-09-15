FROM debian:bookworm-slim@sha256:88200866dfff7ea7f5cbcb6ec7c8a701889efe6fe859fe64d6990e4b07ea4171
RUN dpkg --add-architecture armhf && apt-get update && apt-get install -y --no-install-recommends gcc-arm-linux-gnueabihf libc6-dev-armhf-cross libasound2-dev:armhf file && rm -rf /var/lib/apt/lists/*
WORKDIR /src
