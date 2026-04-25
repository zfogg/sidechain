# Multi-stage build for optimized Docker image
# Stage 1: Build
FROM oven/bun:1-alpine AS builder

WORKDIR /app

# Copy package files
COPY package.json bun.lock ./

# Install dependencies
RUN bun install --frozen-lockfile

# Copy source code
COPY . .

# Build the application
RUN bun run build

# Stage 2: Runtime
FROM caddy:2-alpine

WORKDIR /app

# Copy built application from builder
COPY --from=builder /app/dist ./dist

# Copy Caddyfile
COPY Caddyfile /etc/caddy/Caddyfile

# Create non-root user for security
RUN addgroup -g 1001 -S caddy && adduser -S caddy -u 1001 && \
    chown -R caddy:caddy /app

USER caddy

# Expose port
EXPOSE 80

# Health check
HEALTHCHECK --interval=30s --timeout=3s --start-period=5s --retries=3 \
  CMD wget --no-verbose --tries=1 --spider http://localhost:80 || exit 1

# Start Caddy
CMD ["caddy", "run", "--config", "/etc/caddy/Caddyfile"]
