# Build stage
FROM alpine:3.20 AS builder

RUN apk add --no-cache \
    build-base \
    cmake \
    git \
    linux-headers

WORKDIR /app
COPY . .

RUN mkdir -p build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    make -j$(nproc)

# Final stage
FROM alpine:3.20

RUN apk add --no-cache \
    libstdc++ \
    ca-certificates

WORKDIR /app
COPY --from=builder /app/build/bicep-metric-service .

# Default environment variables
ENV METRIC_ENDPOINT=""
ENV REGISTRATION_ENDPOINT=""
ENV SCRAPE_INTERVAL=10
ENV METRIC_EXPORT_MODE="core"
ENV SERVICE_NAME="bicep-metric-service"
ENV SERVICE_PORT=8080
ENV SERVICE_IP="127.0.0.1"

EXPOSE 8080

CMD ["./bicep-metric-service"]
