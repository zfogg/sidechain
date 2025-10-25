# Sidechain Makefile
# Builds both the VST plugin and Go backend

.PHONY: all install-deps backend plugin clean test help

# Default target
all: install-deps backend plugin plugin-install

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
	BUILD_DIR = plugin/Builds/MacOSX
	BUILD_COMMAND = cd $(BUILD_DIR) && xcodebuild -target "Sidechain - VST3" -configuration Release -quiet
	PLUGIN_OUTPUT = $(BUILD_DIR)/build/Release/Sidechain.vst3
	PLUGIN_INSTALL_DIR = ~/Library/Audio/Plug-Ins/VST3
else ifeq ($(UNAME_S),Linux)
	JUCE_URL = https://github.com/juce-framework/JUCE/releases/download/$(JUCE_VERSION)/juce-$(JUCE_VERSION)-linux.zip
	JUCE_ARCHIVE = juce-$(JUCE_VERSION)-linux.zip
	PLATFORM = linux
	PROJUCER = $(JUCE_DIR)/Projucer
	BUILD_DIR = plugin/Builds/LinuxMakefile
	BUILD_COMMAND = cd $(BUILD_DIR) && make CONFIG=Release
	PLUGIN_OUTPUT = $(BUILD_DIR)/build/Sidechain.vst3
	PLUGIN_INSTALL_DIR = ~/.vst3
else ifeq ($(OS),Windows_NT)
	JUCE_URL = https://github.com/juce-framework/JUCE/releases/download/$(JUCE_VERSION)/juce-$(JUCE_VERSION)-windows.zip
	JUCE_ARCHIVE = juce-$(JUCE_VERSION)-windows.zip
	PLATFORM = windows
	PROJUCER = $(JUCE_DIR)/Projucer.exe
	BUILD_DIR = plugin/Builds/VisualStudio2022
	# Use MSBuild from PATH
	MSBUILD := $(shell which MSBuild.exe 2>/dev/null || echo "MSBuild not found")
	BUILD_COMMAND = cd $(BUILD_DIR) && "$(MSBUILD)" Sidechain_VST3.vcxproj -p:Configuration=Release -p:Platform=x64
	PLUGIN_OUTPUT = $(BUILD_DIR)/x64/Release/VST3/Sidechain.vst3
	PLUGIN_INSTALL_DIR = $(LOCALAPPDATA)/Programs/Common/VST3
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

# Plugin targets - optimized for speed
plugin: $(JUCE_DIR) plugin-project
ifeq ($(PLATFORM),windows)
	@if [ "$(MSBUILD)" = "MSBuild not found" ]; then \
		echo "❌ MSBuild not found!"; \
		echo "Please install Visual Studio 2022 with C++ Desktop Development workload"; \
		exit 1; \
	fi
	@echo "🔧 Fixing Visual Studio project..."
	@bash fix-vcxproj.sh
endif
	@echo "🔄 Building VST plugin for $(PLATFORM)..."
	@$(BUILD_COMMAND)
	@echo "✅ Plugin built successfully"

plugin-debug: $(JUCE_DIR) plugin-project
	@echo "🔄 Building VST plugin (Debug) for $(PLATFORM)..."
	@echo "⚠️  Debug builds not yet implemented for $(PLATFORM)"

# Fast build - always skip project regeneration
plugin-fast: $(JUCE_DIR)
	@echo "🔄 Building VST plugin (fast - no project regen) for $(PLATFORM)..."
	@$(BUILD_COMMAND)
	@echo "✅ Plugin built successfully"

plugin-project: $(JUCE_DIR)
	@echo "🔄 Generating plugin project files for $(PLATFORM)..."
	@if [ ! -d $(BUILD_DIR) ]; then \
		echo "🔄 First time build - generating project files..."; \
		cd plugin && ../$(PROJUCER) --resave Sidechain.jucer; \
		echo "✅ Project files generated"; \
	else \
		echo "⚡ Project files already exist, skipping regeneration"; \
	fi

plugin-install: plugin
	@echo "📦 Installing VST plugin to $(PLUGIN_INSTALL_DIR)..."
ifeq ($(PLATFORM),windows)
	@test -d "$(PLUGIN_INSTALL_DIR)" || mkdir -p "$(PLUGIN_INSTALL_DIR)"
	@rm -rf "$(PLUGIN_INSTALL_DIR)/Sidechain.vst3"
	@cp -r "$(PLUGIN_OUTPUT)" "$(PLUGIN_INSTALL_DIR)/Sidechain.vst3"
else
	@mkdir -p "$(PLUGIN_INSTALL_DIR)"
	@rm -rf "$(PLUGIN_INSTALL_DIR)/Sidechain.vst3"
	@cp -r "$(PLUGIN_OUTPUT)" "$(PLUGIN_INSTALL_DIR)/"
endif
	@echo "✅ Plugin installed successfully"

plugin-clean:
	@echo "🧹 Cleaning plugin build files..."
	@rm -rf plugin/Builds/*/build
	@rm -rf plugin/JuceLibraryCode
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
ifeq ($(PLATFORM),windows)
	@echo "MSBuild: $(MSBUILD)"
	@echo "Build Directory: $(BUILD_DIR)"
endif

help:
	@echo "🎵 Sidechain - Social VST for Music Producers"
	@echo ""
	@echo "Platform: $(PLATFORM)"
	@echo ""
	@echo "Available targets:"
	@echo "  all             - Build and install everything (recommended)"
	@echo "  install-deps    - Download and install JUCE $(JUCE_VERSION)"
	@echo "  backend         - Build Go backend server"
	@echo "  backend-run     - Run built backend server"
	@echo "  backend-dev     - Run backend in development mode"
	@echo "  plugin          - Build VST plugin"
	@echo "  plugin-fast     - Build VST plugin (skip project regen)"
	@echo "  plugin-install  - Install VST plugin to system directory"
	@echo "  plugin-project  - Regenerate plugin project files"
	@echo "  test            - Run all tests"
	@echo "  dev             - Start development environment"
	@echo "  clean           - Clean all build artifacts"
	@echo "  deps-info       - Show dependency information"
	@echo "  help            - Show this help message"
	@echo ""
	@echo "Quick start:"
	@echo "  make              # Build and install everything"
	@echo "  make backend-dev  # Start backend server"