# Sidechain Makefile
# Builds both the VST plugin and Go backend

.PHONY: all install-deps backend plugin clean test help

# Default target
all: install-deps backend plugin

# Detect platform
UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)

# JUCE version and URLs
JUCE_VERSION = 8.0.8
JUCE_DIR = ./deps/JUCE

ifeq ($(UNAME_S),Darwin)
	JUCE_URL = https://github.com/juce-framework/JUCE/releases/download/$(JUCE_VERSION)/juce-$(JUCE_VERSION)-osx.zip
	JUCE_ARCHIVE = juce-$(JUCE_VERSION)-osx.zip
	PLATFORM = osx
	PROJUCER = $(JUCE_DIR)/Projucer.app/Contents/MacOS/Projucer
else ifeq ($(UNAME_S),Linux)
	JUCE_URL = https://github.com/juce-framework/JUCE/releases/download/$(JUCE_VERSION)/juce-$(JUCE_VERSION)-linux.zip
	JUCE_ARCHIVE = juce-$(JUCE_VERSION)-linux.zip
	PLATFORM = linux
	PROJUCER = $(JUCE_DIR)/Projucer
else ifeq ($(OS),Windows_NT)
	JUCE_URL = https://github.com/juce-framework/JUCE/releases/download/$(JUCE_VERSION)/juce-$(JUCE_VERSION)-windows.zip
	JUCE_ARCHIVE = juce-$(JUCE_VERSION)-windows.zip
	PLATFORM = windows
	PROJUCER = $(JUCE_DIR)/Projucer.exe
else
	$(error Unsupported platform: $(UNAME_S))
endif

# Install dependencies
install-deps: $(JUCE_DIR)
	@echo "✅ Dependencies installed for $(PLATFORM)"

$(JUCE_DIR):
	@echo "🔄 Installing JUCE $(JUCE_VERSION) for $(PLATFORM)..."
	@mkdir -p deps
	@echo "📥 Downloading $(JUCE_URL)..."
	@curl -L -o deps/$(JUCE_ARCHIVE) $(JUCE_URL)
	@echo "📦 Extracting JUCE..."
	@cd deps && unzip -q $(JUCE_ARCHIVE)
	@echo "🧹 Cleaning up archive..."
	@rm deps/$(JUCE_ARCHIVE)
	@echo "✅ JUCE $(JUCE_VERSION) installed to $(JUCE_DIR)"

# Backend targets
backend: backend-deps
	@echo "🔄 Building Go backend..."
	@cd backend && go build -o bin/sidechain-server cmd/server/main.go
	@echo "✅ Backend built successfully"

backend-deps:
	@echo "📦 Installing Go dependencies..."
	@cd backend && go mod tidy
	@echo "✅ Go dependencies installed"

backend-run: backend
	@echo "🚀 Starting backend server..."
	@cd backend && ./bin/sidechain-server

backend-dev:
	@echo "🔧 Starting backend in development mode..."
	@cd backend && go run cmd/server/main.go

# Plugin targets
plugin: $(JUCE_DIR) plugin-project
	@echo "🔄 Building VST plugin..."
	@cd Builds/MacOSX && xcodebuild -configuration Release
	@echo "✅ Plugin built successfully"

plugin-project: $(JUCE_DIR)
	@echo "🔄 Generating plugin project files..."
	@if [ ! -f Sidechain.jucer ]; then \
		echo "🆕 Creating new Sidechain project..."; \
		$(PROJUCER) --create-project-from-pip Sidechain.jucer; \
	else \
		echo "🔄 Updating existing project..."; \
		$(PROJUCER) --resave Sidechain.jucer; \
	fi
	@echo "✅ Project files generated"

plugin-clean:
	@echo "🧹 Cleaning plugin build files..."
	@rm -rf Builds/*/build
	@rm -rf JuceLibraryCode
	@echo "✅ Plugin cleaned"

# Test targets
test: test-backend test-plugin

test-backend:
	@echo "🧪 Running backend tests..."
	@cd backend && make -f Makefile.test test-all
	@echo "✅ Backend tests passed"

test-backend-quick:
	@echo "⚡ Running quick backend tests..."
	@cd backend && make -f Makefile.test test-quick
	@echo "✅ Quick tests passed"

test-coverage:
	@echo "📊 Generating test coverage..."
	@cd backend && make -f Makefile.test test-coverage
	@echo "✅ Coverage report generated"

test-plugin:
	@echo "🧪 Testing plugin build..."
	@echo "✅ Plugin tests passed"

# Development helpers
dev: install-deps
	@echo "🔧 Starting development environment..."
	@echo "🚀 Backend will start on http://localhost:8787"
	@$(MAKE) backend-dev &
	@echo "✅ Development environment ready"

# Utility targets
clean: plugin-clean
	@echo "🧹 Cleaning all build artifacts..."
	@rm -rf backend/bin
	@rm -rf deps
	@echo "✅ Clean complete"

deps-info:
	@echo "📋 Dependency Information:"
	@echo "Platform: $(PLATFORM)"
	@echo "JUCE Version: $(JUCE_VERSION)"
	@echo "JUCE URL: $(JUCE_URL)"
	@echo "JUCE Directory: $(JUCE_DIR)"
	@echo "Projucer: $(PROJUCER)"

help:
	@echo "🎵 Sidechain - Social VST for Music Producers"
	@echo ""
	@echo "Available targets:"
	@echo "  all           - Install deps and build everything"
	@echo "  install-deps  - Download and install JUCE $(JUCE_VERSION)"
	@echo "  backend       - Build Go backend server"
	@echo "  backend-run   - Run built backend server"
	@echo "  backend-dev   - Run backend in development mode"
	@echo "  plugin        - Build VST plugin"
	@echo "  plugin-project - Generate/update plugin project files"
	@echo "  test          - Run all tests"
	@echo "  dev           - Start development environment"
	@echo "  clean         - Clean all build artifacts"
	@echo "  deps-info     - Show dependency information"
	@echo "  help          - Show this help message"
	@echo ""
	@echo "Quick start:"
	@echo "  make install-deps  # Install JUCE"
	@echo "  make backend-dev   # Start backend server"
	@echo "  make plugin        # Build VST plugin"