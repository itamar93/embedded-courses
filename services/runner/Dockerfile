# Build from the repository root:
#   docker build -f services/runner/Dockerfile -t embedded-courses-runner .
# Run untrusted learner code only under a sandboxing runtime, for example:
#   docker run --runtime=runsc --network=none --memory=1g --cpus=2 -p 8080:8080 embedded-courses-runner

FROM ubuntu:24.04 AS qemu
RUN apt-get update && apt-get install -y --no-install-recommends \
        git ca-certificates build-essential ninja-build pkg-config python3 python3-venv \
        libglib2.0-dev libpixman-1-dev flex bison \
    && rm -rf /var/lib/apt/lists/*
COPY emulator/qemu /emulator
RUN /emulator/build.sh /opt/qemu

FROM golang:1.24 AS runner
WORKDIR /src
COPY services/runner/ .
RUN CGO_ENABLED=0 go build -o /runner .

FROM ubuntu:24.04
RUN apt-get update && apt-get install -y --no-install-recommends \
        gcc-arm-none-eabi binutils-arm-none-eabi make libglib2.0-0t64 libpixman-1-0 \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --create-home runner
COPY --from=qemu /opt/qemu/bin/qemu-system-arm /usr/local/bin/
COPY --from=runner /runner /usr/local/bin/runner
COPY courses /srv/courses
USER runner
EXPOSE 8080
ENTRYPOINT ["runner", "-addr", ":8080", "-courses", "/srv/courses", "-qemu", "/usr/local/bin/qemu-system-arm", "-work", "/tmp/builds"]
