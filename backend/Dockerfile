# Build stage
FROM golang:1.24-alpine AS builder

WORKDIR /app

# Copy go.mod and go.sum first for caching
COPY go.mod go.sum ./
RUN go mod download

# Copy source code
COPY . .

# Build the binary with optimizations
RUN CGO_ENABLED=0 GOOS=linux go build -mod=mod -ldflags="-s -w" -o server ./cmd/server
RUN GOMAXPROCS=4 GOMEMLIMIT=1500MiB CGO_ENABLED=0 GOOS=linux \
    go build -p 2 -mod=mod -ldflags="-s -w" -o server ./cmd/server

# Runtime stage
FROM alpine:3.19

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
