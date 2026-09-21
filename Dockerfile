# Build stage
FROM ubuntu:22.04 AS builder
RUN apt-get update && apt-get install -y g++ cmake make git
WORKDIR /app
COPY . .
RUN mkdir -p build && cd build && \
    cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON .. && \
    make -j$(nproc)

# Test stage
FROM builder AS tester
RUN cd build && ctest --output-on-failure

# Production stage
FROM ubuntu:22.04 AS production
RUN apt-get update && apt-get install -y libstdc++6 && rm -rf /var/lib/apt/lists/*
COPY --from=builder /app/build/http_server /usr/local/bin/
COPY --from=builder /app/static /static
EXPOSE 8080
CMD ["http_server", "--port", "8080", "--static-dir", "/static"]
