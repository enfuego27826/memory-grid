# Memory Grid server — multi-stage build (toolchain stage → slim runtime).
# Builds the ws:// server (mg_server). TLS is expected to terminate at a reverse
# proxy in front of this container (the server is LIBUS_NO_SSL).

# ---- build stage ----------------------------------------------------------
FROM ubuntu:24.04 AS build
RUN apt-get update && apt-get install -y --no-install-recommends \
        g++ cmake git ca-certificates zlib1g-dev libssl-dev \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
# Copy the server sources (core + transport + CMake). FetchContent pulls
# doctest / nlohmann-json / uWebSockets(+uSockets) at configure time.
COPY . /src/server
RUN cmake -S /src/server -B /src/build -DCMAKE_BUILD_TYPE=Release \
 && cmake --build /src/build -j --target mg_server

# ---- runtime stage --------------------------------------------------------
FROM ubuntu:24.04 AS runtime
RUN apt-get update && apt-get install -y --no-install-recommends \
        zlib1g libssl3 \
    && rm -rf /var/lib/apt/lists/*
COPY --from=build /src/build/mg_server /usr/local/bin/mg_server
EXPOSE 9001
ENTRYPOINT ["/usr/local/bin/mg_server", "9001"]
