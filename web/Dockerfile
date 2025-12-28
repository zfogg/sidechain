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
FROM oven/bun:1-alpine

WORKDIR /app

# Copy built application from builder
COPY --from=builder /app/dist ./dist
COPY package.json ./

# Create non-root user for security
RUN addgroup -g 1001 -S bunuser && adduser -S bunuser -u 1001

USER bunuser

# Expose port
EXPOSE 3000

# Health check
HEALTHCHECK --interval=30s --timeout=3s --start-period=5s --retries=3 \
  CMD wget --no-verbose --tries=1 --spider http://localhost:3000 || exit 1

# Start the application using bun's built-in static file server
CMD ["bun", "run", "--bun", "vite", "preview", "--host", "0.0.0.0", "--port", "3000"]
