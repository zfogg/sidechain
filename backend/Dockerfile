# Build stage
FROM golang:1.26-alpine AS builder

WORKDIR /app

# Copy go.mod and go.sum first for caching
COPY go.mod go.sum ./
RUN go mod download

# Copy source code
COPY . .

# Build the binary with optimizations
ARG GOMEMLIMIT
ARG BUILD_PARALLELISM

RUN GOMAXPROCS=${BUILD_PARALLELISM} GOMEMLIMIT=${GOMEMLIMIT} CGO_ENABLED=0 GOOS=linux \
    go build -p ${BUILD_PARALLELISM} -mod=mod -ldflags="-s -w" -o server ./cmd/server

# Runtime stage
FROM alpine:3.23

RUN apk add --no-cache ca-certificates tzdata ffmpeg curl \
    && adduser -D -g '' appuser

WORKDIR /app

COPY --from=builder /app/server .

RUN chown -R appuser:appuser /app

USER appuser

EXPOSE 8787

HEALTHCHECK --interval=30s --timeout=10s --start-period=30s --retries=5 \
    CMD curl -f http://localhost:8787/health || exit 1

CMD ["./server"]
