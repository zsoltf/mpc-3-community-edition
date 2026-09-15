FROM debian@sha256:d7e12182ce18b85b93007c1dedf31f2d29e01ccf3182cc4017c709b6259bc132
RUN apt-get update && apt-get install -y --no-install-recommends gcc libc6-dev liblzma-dev libssl-dev libext2fs-dev e2fsprogs python3 qemu-user-static openssh-client sshpass file && rm -rf /var/lib/apt/lists/*
WORKDIR /work
