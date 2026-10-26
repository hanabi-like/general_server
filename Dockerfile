FROM debian:bookworm-slim AS builder

RUN apt-get update \
    && apt-get install -y --no-install-recommends build-essential cmake \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY CMakeLists.txt ./
COPY include/ ./include/
COPY src/ ./src/

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --parallel 2

FROM debian:bookworm-slim AS runtime

RUN apt-get update \
    && apt-get install -y --no-install-recommends libstdc++6 \
    && rm -rf /var/lib/apt/lists/*

# FileResource resolves ./static relative to the process working directory.
WORKDIR /app
COPY --from=builder /src/build/general_server ./general_server
COPY static/ ./static/
RUN chmod 755 /app /app/general_server \
    && find /app/static -type d -exec chmod 755 {} + \
    && find /app/static -type f -exec chmod 644 {} +

USER 10001:10001
EXPOSE 8080
ENTRYPOINT ["/app/general_server"]
CMD ["0.0.0.0", "8080"]
