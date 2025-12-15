# Sidechain Backend Makefile
# Production-ready Go backend with database migrations, testing, and deployment

.PHONY: help install deps build run dev test test-unit test-integration clean docker docker-run docker-dev migrate migrate-up migrate-down lint format

# Default target
help:
	@echo "🎵 Sidechain Backend - Production Makefile"
	@echo ""
	@echo "Setup Commands:"
	@echo "  install     - Install all dependencies (Go modules + tools)"
	@echo "  deps        - Download Go module dependencies"
	@echo ""
	@echo "Development Commands:"
	@echo "  build       - Build production binary"
	@echo "  run         - Run built binary"
	@echo "  dev         - Start development server with hot reload (local)"
	@echo "  dev-watch   - Start with file watching (requires air)"
	@echo "  docker-dev  - Start all services with docker-compose + hot reload"
	@echo "  docker-dev-stop - Stop docker-dev services"
	@echo "  docker-dev-logs - View logs from docker-dev"
	@echo ""
	@echo "Database Commands:"
	@echo "  migrate     - Run database migrations"
	@echo "  migrate-up  - Run migrations (explicit up)"
	@echo "  migrate-down - Rollback last migration"
	@echo "  db-setup    - Create database and run migrations"
	@echo "  seed        - Seed development database with realistic data"
	@echo "  seed-test   - Seed test database with minimal data"
	@echo "  seed-clean  - Remove all seed data (use with caution)"
	@echo ""
	@echo "Testing Commands:"
	@echo "  test        - Run all tests"
	@echo "  test-unit   - Run fast unit tests only"
	@echo "  test-integration - Run integration tests"
	@echo "  test-coverage - Generate test coverage report (HTML + summary)"
	@echo "  test-coverage-detail - Detailed coverage by package"
	@echo "  test-db     - Set up test database (PostgreSQL)"
	@echo "  test-db-clean - Drop test database"
	@echo "  bench       - Run benchmark tests"
	@echo ""
	@echo "Code Quality:"
	@echo "  lint        - Run Go linter (golangci-lint)"
	@echo "  format      - Format Go code (gofmt + goimports)"
	@echo "  vet         - Run Go vet"
	@echo ""
	@echo "Cleanup:"
	@echo "  clean       - Clean build artifacts"

# Install all dependencies
install: deps tools
	@echo "✅ Backend setup complete"

# Install project dependencies (downloads everything in go.mod automatically)
deps:
	@echo "📦 Downloading Go dependencies..."
	@go mod download
	@echo "✅ All dependencies downloaded"

# Install development tools (global command-line tools)
tools:
	@echo "🔧 Installing development tools..."
	@go install github.com/air-verse/air@latest
	@go install github.com/golangci/golangci-lint/cmd/golangci-lint@latest
	@go install golang.org/x/tools/cmd/goimports@latest
	@echo "✅ Development tools installed"

# Build production binary
build:
	@echo "🔨 Building Sidechain backend..."
	@CGO_ENABLED=0 GOOS=linux go build -ldflags="-w -s" -o bin/sidechain-server ./cmd/server
	@echo "✅ Binary built: bin/sidechain-server"

# Build for current platform
build-local:
	@echo "🔨 Building for local platform..."
	@go build -o bin/sidechain-server ./cmd/server
	@echo "✅ Local binary built: bin/sidechain-server"

# Run built binary
run: build-local
	@echo "🚀 Starting Sidechain backend..."
	@./bin/sidechain-server

# Development server
dev:
	@echo "🔧 Starting development server..."
	@go run ./cmd/server/main.go

# Development with hot reload (requires air)
dev-watch:
	@echo "🔥 Starting development server with hot reload..."
	@air

# Docker development with hot reload (all services + auto rebuild)
docker-dev:
	@echo "🐳 Starting all services with hot reload..."
	@docker-compose -f docker-compose.yml -f docker-compose.dev.yml up --build

# Stop docker-dev services
docker-dev-stop:
	@echo "🛑 Stopping docker-dev services..."
	@docker-compose -f docker-compose.yml -f docker-compose.dev.yml down

# View docker-dev logs
docker-dev-logs:
	@echo "📋 Showing docker-dev logs..."
	@docker-compose -f docker-compose.yml -f docker-compose.dev.yml logs -f backend

# Database setup
db-setup:
	@echo "🗄️ Setting up database..."
	@psql -U postgres -c "CREATE DATABASE sidechain;" 2>/dev/null || echo "Database exists"
	@psql -U postgres -c "CREATE DATABASE sidechain_test;" 2>/dev/null || echo "Test database exists"
	@$(MAKE) migrate
	@echo "✅ Database setup complete"

# Run migrations
migrate: migrate-up

migrate-up:
	@echo "📈 Running database migrations..."
	@go run ./cmd/migrate/main.go up
	@echo "✅ Migrations applied"

migrate-down:
	@echo "📉 Rolling back last migration..."
	@go run ./cmd/migrate/main.go down
	@echo "✅ Migration rolled back"

# Create new migration
migrate-create:
	@echo "📝 Creating new migration: $(name)"
	@go run ./cmd/migrate/main.go create $(name)

# Seed database
seed:
	@echo "🌱 Seeding development database..."
	@go run ./cmd/seed/main.go dev
	@echo "✅ Database seeded"

seed-test:
	@echo "🧪 Seeding test database..."
	@go run ./cmd/seed/main.go test
	@echo "✅ Test database seeded"

seed-clean:
	@echo "🧹 Cleaning seed data..."
	@go run ./cmd/seed/main.go clean
	@echo "✅ Seed data cleaned"

# Testing
test:
	@echo "🧪 Running all tests..."
	@$(MAKE) test-unit
	@$(MAKE) test-integration
	@echo "✅ All tests passed"

test-unit:
	@echo "⚡ Running unit tests..."
	@go test ./internal/... -v -race -p 1 -timeout 120s
	@echo "✅ Unit tests passed"

test-integration:
	@echo "🧪 Running integration tests..."
	@go test ./tests/integration/... -tags=integration -v -timeout 60s
	@echo "✅ Integration tests passed"

test-coverage:
	@echo "📊 Generating test coverage..."
	@go test ./... -p 1 -coverprofile=coverage.out -covermode=atomic
	@go tool cover -html=coverage.out -o coverage.html
	@go tool cover -func=coverage.out | tail -1
	@echo "✅ Coverage report: coverage.html"

test-coverage-detail:
	@echo "📊 Generating detailed coverage by package..."
	@go test ./... -p 1 -coverprofile=coverage.out -covermode=atomic
	@go tool cover -func=coverage.out | grep -E "^github" | head -30
	@echo ""
	@go tool cover -func=coverage.out | tail -1

# Test database setup
test-db:
	@echo "🧪 Setting up test database..."
	@psql -U postgres -c "DROP DATABASE IF EXISTS sidechain_test;" 2>/dev/null || true
	@psql -U postgres -c "CREATE DATABASE sidechain_test;" 2>/dev/null || echo "Test database exists"
	@POSTGRES_DB=sidechain_test go run ./cmd/migrate/main.go up
	@echo "✅ Test database ready"

test-db-clean:
	@echo "🧹 Dropping test database..."
	@psql -U postgres -c "DROP DATABASE IF EXISTS sidechain_test;" 2>/dev/null || true
	@echo "✅ Test database dropped"

bench:
	@echo "⚡ Running benchmarks..."
	@go test ./... -bench=. -benchmem -timeout 60s

# Code quality
lint:
	@echo "🔍 Running linter..."
	@golangci-lint run ./...
	@echo "✅ Linting passed"

format:
	@echo "💄 Formatting code..."
	@gofmt -w .
	@goimports -w .
	@echo "✅ Code formatted"

vet:
	@echo "🔍 Running go vet..."
	@go vet ./...
	@echo "✅ Vet passed"

# Cleanup
clean:
	@echo "🧹 Cleaning build artifacts..."
	@rm -rf bin/
	@rm -f coverage.out coverage.html
	@go clean -cache
	@echo "✅ Clean complete"

# Health check
health:
	@echo "❤️ Checking backend health..."
	@curl -f http://localhost:8787/health || echo "❌ Backend not running"

# Development shortcuts
dev-full: db-setup dev

quick-test: test-unit

# Production deployment helpers
build-production: clean deps test lint build

# Environment setup
env-setup:
	@echo "⚙️ Setting up environment..."
	@cp .env.example .env
	@echo "✅ Environment file created - please edit .env with your credentials"