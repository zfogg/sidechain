#!/usr/bin/env make -f

# Sidechain Makefile
# Builds both the VST plugin (via CMake) and Go backend

.PHONY: all install install-deps backend plugin clean test test-plugin-unit test-plugin-coverage help

# Default target
all: backend plugin plugin-install

# Detect platform
UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)

# JUCE version for FetchContent fallback info
JUCE_VERSION = 8.0.11
JUCE_DIR = ./deps/JUCE


Black   = "\e[1;30m"
Red     = "\e[1;31m"
Green   = "\e[1;32m"
Yellow  = "\e[1;33m"
Blue    = "\e[1;34m"
Purple  = "\e[1;35m"
Cyan    = "\e[1;36m"
White   = "\e[1;37m"
Reset   = "\e[0m"

# Build directory
BUILD_DIR = plugin/build

# Build type (defaults to Debug, can be overridden: make CMAKE_BUILD_TYPE=Release)
CMAKE_BUILD_TYPE ?= Debug


# Platform-specific settings
ifeq ($(UNAME_S),Darwin)
	PLATFORM = macos
	PLUGIN_OUTPUT = plugin/build/src/core/Sidechain_artefacts/$(CMAKE_BUILD_TYPE)/VST3/Sidechain.vst3
	AU_OUTPUT = plugin/build/src/core/Sidechain_artefacts/$(CMAKE_BUILD_TYPE)/AU/Sidechain.component
	PLUGIN_INSTALL_DIR = ~/Library/Audio/Plug-Ins/VST3
	AU_INSTALL_DIR = ~/Library/Audio/Plug-Ins/Components
	CMAKE_GENERATOR = -G Ninja
else ifeq ($(UNAME_S),Linux)
	PLATFORM = linux
	PLUGIN_OUTPUT = plugin/build/src/core/Sidechain_artefacts/$(CMAKE_BUILD_TYPE)/VST3/Sidechain.vst3
	PLUGIN_INSTALL_DIR = ~/.vst3
	CMAKE_GENERATOR = -G Ninja
else ifeq ($(OS),Windows_NT)
	PLATFORM = windows
	PLUGIN_OUTPUT = plugin/build/src/core/Sidechain_artefacts/$(CMAKE_BUILD_TYPE)/VST3/Sidechain.vst3
	PLUGIN_INSTALL_DIR = $(LOCALAPPDATA)/Programs/Common/VST3
	CMAKE_GENERATOR = -G "Visual Studio 17 2022" -A x64
else
	$(error Unsupported platform: $(UNAME_S))
endif

# Install JUCE (optional - CMake can fetch it automatically)
install-deps:
	@echo "📦 Dependencies will be fetched automatically by CMake if needed"
	@echo "✅ Ready to build for $(PLATFORM)"

# Legacy target for manual JUCE installation
install-juce:
	@if [ ! -d "$(JUCE_DIR)" ]; then \
		echo "🔄 Downloading JUCE $(JUCE_VERSION)..."; \
		mkdir -p deps; \
		cd deps && git clone --depth 1 --branch $(JUCE_VERSION) https://github.com/juce-framework/JUCE.git; \
		echo "✅ JUCE $(JUCE_VERSION) installed to $(JUCE_DIR)"; \
	else \
		echo "✅ JUCE already installed at $(JUCE_DIR)"; \
	fi

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

# Plugin targets using CMake
plugin: plugin-configure
	@echo "🔄 Building VST plugin for $(PLATFORM) ($(CMAKE_BUILD_TYPE))..."
	@cmake --build $(BUILD_DIR) --config $(CMAKE_BUILD_TYPE) --parallel
	@echo "✅ Plugin built successfully"

plugin-configure:
	@echo "🔄 Configuring CMake build for $(PLATFORM) ($(CMAKE_BUILD_TYPE))..."
	@# Use Ninja if available, otherwise fall back to platform default
	@if command -v ninja >/dev/null 2>&1; then \
		echo "✅ Using Ninja generator (faster parallel builds)"; \
		cmake -S plugin -B $(BUILD_DIR) -G Ninja -DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE) -DCMAKE_EXPORT_COMPILE_COMMANDS=ON $(if $(SIDECHAIN_BUILD_STANDALONE),-DSIDECHAIN_BUILD_STANDALONE=ON,); \
	else \
		echo "⚠️  Ninja not found, using $(CMAKE_GENERATOR). Install ninja for faster builds."; \
		cmake -S plugin -B $(BUILD_DIR) $(CMAKE_GENERATOR) -DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE) -DCMAKE_EXPORT_COMPILE_COMMANDS=ON $(if $(SIDECHAIN_BUILD_STANDALONE),-DSIDECHAIN_BUILD_STANDALONE=ON,); \
	fi
	@echo "✅ CMake configuration complete"
	@if [ -f "$(BUILD_DIR)/compile_commands.json" ]; then \
		ln -sf "$(BUILD_DIR)/compile_commands.json" compile_commands.json 2>/dev/null || true; \
		echo "✅ Linked compile_commands.json to project root"; \
	fi

# Fast build - skip configuration if already done
plugin-fast:
	@echo "🔄 Building VST plugin (fast - no reconfigure) for $(PLATFORM) ($(CMAKE_BUILD_TYPE))..."
	@# Check for corrupted ninja cache and auto-fix by reconfiguring
	@if [ -f "$(BUILD_DIR)/.ninja_deps" ]; then \
		if ! ninja -C $(BUILD_DIR) -t cleandead >/dev/null 2>&1; then \
			echo "⚠️  Detected corrupted ninja cache, reconfiguring..."; \
			$(MAKE) plugin-configure > /dev/null; \
		fi \
	fi
	@cmake --build $(BUILD_DIR) --config $(CMAKE_BUILD_TYPE) --parallel
	@echo "✅ Plugin built successfully"

# Clean ninja cache (useful if builds keep recompiling everything)
plugin-clean-cache:
	@echo "🧹 Cleaning ninja dependency cache..."
	@rm -f $(BUILD_DIR)/.ninja_deps $(BUILD_DIR)/.ninja_log
	@echo "✅ Ninja cache cleared (next build will rebuild dependency info)"

# Rebuild plugin only (clean plugin files but keep cached dependencies)
plugin-rebuild: plugin-configure
	@echo "🔄 Rebuilding plugin (keeping cached dependencies) for $(PLATFORM) ($(CMAKE_BUILD_TYPE))..."
	@# Clean only plugin object files, not cached dependencies
	@find $(BUILD_DIR) -path "$(BUILD_DIR)/src" -name "*.o" -delete 2>/dev/null || true
	@find $(BUILD_DIR) -path "$(BUILD_DIR)/CMakeFiles/Sidechain*.dir" -name "*.o" -delete 2>/dev/null || true
	@rm -rf $(BUILD_DIR)/src/core/Sidechain_artefacts 2>/dev/null || true
	@cmake --build $(BUILD_DIR) --config $(CMAKE_BUILD_TYPE) --parallel
	@echo "✅ Plugin rebuilt successfully (dependencies cached)"

# CMake install (use with sudo for system-wide install)
install: plugin
	@echo "📦 Installing plugin using cmake --install..."
	@cmake --install $(BUILD_DIR) --config $(CMAKE_BUILD_TYPE)
	@echo "✅ Plugin installed successfully"

plugin-install: plugin
	@echo "📦 Installing VST plugin to $(PLUGIN_INSTALL_DIR)..."
	@mkdir -p "$(PLUGIN_INSTALL_DIR)"
	@rm -rf "$(PLUGIN_INSTALL_DIR)/Sidechain.vst3"
	@cp -r "$(PLUGIN_OUTPUT)" "$(PLUGIN_INSTALL_DIR)/"
ifeq ($(PLATFORM),macos)
	@echo "📦 Installing AU plugin to $(AU_INSTALL_DIR)..."
	@mkdir -p "$(AU_INSTALL_DIR)"
	@rm -rf "$(AU_INSTALL_DIR)/Sidechain.component"
	@cp -r "$(AU_OUTPUT)" "$(AU_INSTALL_DIR)/"
endif
	@echo "✅ Plugin(s) installed successfully"

plugin-clean:
	@echo "🧹 Cleaning plugin build files..."
	@rm -rf $(BUILD_DIR)
	@echo "✅ Plugin cleaned"

# Test targets
test: test-backend test-plugin

test-backend:
	@echo "🧪 Running backend tests..."
	@cd backend && $(MAKE) test-unit
	@echo "✅ Backend tests passed"

test-backend-quick:
	@echo "⚡ Running quick backend tests..."
	@cd backend && $(MAKE) quick-test
	@echo "✅ Quick tests passed"

test-coverage:
	@echo "📊 Generating test coverage..."
	@cd backend && $(MAKE) test-coverage
	@echo "✅ Coverage report generated"

test-plugin:
	@echo "🧪 Testing plugin build..."
	@test -d "$(PLUGIN_OUTPUT)" || (echo "❌ Plugin not built" && exit 1)
	@echo "✅ Plugin build verified"

# Plugin unit tests with Catch2
test-plugin-unit: test-plugin-configure
	@echo "🧪 Building and running plugin unit tests..."
	@cmake --build $(BUILD_DIR) --config Release --target SidechainTests --parallel
	@echo "🧪 Running AudioCapture tests..."
	@cd $(BUILD_DIR) && ./SidechainTests "[AudioCapture]" --reporter compact
	@echo "🧪 Running FeedPost tests..."
	@cd $(BUILD_DIR) && ./SidechainTests "[FeedPost]" --reporter compact
	@echo "🧪 Running NetworkClient tests..."
	@cd $(BUILD_DIR) && ./SidechainTests "[NetworkClient]" --reporter compact || true
	@echo "✅ Plugin unit tests passed"

# Plugin unit tests with JUnit XML output (for CI)
test-plugin-unit-ci: test-plugin-configure
	@echo "🧪 Building plugin unit tests for CI..."
	@cmake --build $(BUILD_DIR) --config Release --target SidechainTests --parallel
	@mkdir -p $(BUILD_DIR)/test-results
	@echo "🧪 Running AudioCapture tests..."
	@cd $(BUILD_DIR) && ./SidechainTests "[AudioCapture]" --reporter junit --out test-results/audio-capture.xml
	@echo "🧪 Running FeedPost tests..."
	@cd $(BUILD_DIR) && ./SidechainTests "[FeedPost]" --reporter junit --out test-results/feed-post.xml
	@echo "🧪 Running NetworkClient tests..."
	@cd $(BUILD_DIR) && ./SidechainTests "[NetworkClient]" --reporter junit --out test-results/network-client.xml || true
	@echo "✅ Plugin CI tests complete - results in $(BUILD_DIR)/test-results/"

test-plugin-configure:
	@echo "🔄 Configuring CMake build with tests..."
	@cmake -S plugin -B $(BUILD_DIR) $(CMAKE_GENERATOR) -DCMAKE_BUILD_TYPE=Release -DSIDECHAIN_BUILD_TESTS=ON

# Plugin test coverage
test-plugin-coverage: test-plugin-coverage-configure
	@echo "📊 Building and running plugin tests with coverage..."
	@cmake --build $(BUILD_DIR) --config Debug --target SidechainTests --parallel
	@echo "🧪 Running tests with coverage instrumentation..."
	@$(BUILD_DIR)/SidechainTests
	@echo "📊 Generating coverage report..."
	@lcov --capture --directory $(BUILD_DIR) --output-file $(BUILD_DIR)/coverage.info --ignore-errors mismatch
	@lcov --remove $(BUILD_DIR)/coverage.info '/usr/*' '*/deps/*' '*/Catch2/*' '*/build/*' --output-file $(BUILD_DIR)/coverage.info --ignore-errors unused
	@genhtml $(BUILD_DIR)/coverage.info --output-directory $(BUILD_DIR)/coverage-html
	@echo "✅ Coverage report generated at $(BUILD_DIR)/coverage-html/index.html"

test-plugin-coverage-configure:
	@echo "🔄 Configuring CMake build with tests and coverage..."
	@cmake -S plugin -B $(BUILD_DIR) $(CMAKE_GENERATOR) -DCMAKE_BUILD_TYPE=Debug -DSIDECHAIN_BUILD_TESTS=ON -DSIDECHAIN_ENABLE_COVERAGE=ON

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
	@echo "JUCE Directory: $(JUCE_DIR)"
	@echo "Build Directory: $(BUILD_DIR)"
	@echo "Plugin Output: $(PLUGIN_OUTPUT)"
	@echo "Install Directory: $(PLUGIN_INSTALL_DIR)"

help:
	@echo "🎵 Sidechain - Social VST for Music Producers"
	@echo ""
	@echo "Platform: $(PLATFORM)"
	@echo ""
	@echo "Available targets:"
	@echo "  all                   - Build backend and plugin (recommended)"
	@echo "  install-deps          - Show dependency info (CMake fetches automatically)"
	@echo "  install-juce          - Manually install JUCE to deps/"
	@echo "  backend               - Build Go backend server"
	@echo "  backend-run           - Run built backend server"
	@echo "  backend-dev           - Run backend in development mode"
	@echo "  plugin                - Build VST plugin via CMake"
	@echo "  plugin-fast           - Build VST plugin (skip reconfigure)"
	@echo "  plugin-rebuild        - Rebuild plugin only (keep cached deps)"
	@echo "  plugin-install        - Install plugin to system directory"
	@echo "  plugin-clean          - Clean plugin build files"
	@echo "  plugin-clean-cache    - Clear ninja cache (if rebuilding everything)"
	@echo "  test                  - Run all tests"
	@echo "  test-plugin-unit      - Run plugin C++ unit tests"
	@echo "  test-plugin-coverage  - Run tests with coverage report"
	@echo "  dev                   - Start development environment"
	@echo "  clean                 - Clean all build artifacts"
	@echo "  deps-info             - Show dependency information"
	@echo "  help                  - Show this help message"
	@echo ""
	@echo "Build options:"
	@echo "  make CMAKE_BUILD_TYPE=Release - Build release version"
	@echo "  make SIDECHAIN_BUILD_STANDALONE=ON - Build Standalone app (in addition to VST3/AU)"
	@echo ""
	@echo "Quick start:"
	@echo "  make              # Build everything"
	@echo "  make plugin       # Build just the plugin"
	@echo "  make backend-dev  # Start backend server"
	@echo ""
	@echo "Troubleshooting:"
	@echo "  If builds keep recompiling everything:"
	@echo "    make plugin-clean-cache && make plugin-fast"

cloc:
	@printf $(Purple)"documentation:\n"$(Reset)
	@cloc --progress=1 --force-lang='Markdown,dox' --force-lang='XML,in' README.md docs
	@printf $(Purple)"\nbackend:\n"$(Reset)
	@cloc --progress=1 --include-lang='Go,Markdown,Sql,Yaml' backend
	@printf $(Purple)"\nplugin:\n"$(Reset)
	@cloc --progress=1 --include-lang='C,C/C++ Header,Objective-C' plugin

