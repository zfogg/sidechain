# VST Test Makefile using Catch2

JUCE_DIR = ../../deps/JUCE
JUCE_MODULES = $(JUCE_DIR)/modules

# Compiler settings
CXX = clang++
CXXFLAGS = -std=c++17 -O2 -DJUCE_STANDALONE_APPLICATION=1

# Include paths
INCLUDES = -I../Source \
           -I$(JUCE_MODULES) \
           -I$(JUCE_MODULES)/juce_core \
           -I$(JUCE_MODULES)/juce_events \
           -I$(JUCE_MODULES)/juce_graphics \
           -I$(JUCE_MODULES)/juce_gui_basics

# Test targets
test: test-runner
	@echo "🧪 Running VST tests..."
	@./test-runner
	@echo "✅ VST tests complete"

# Simple test runner (minimal JUCE dependencies)
test-runner: NetworkClientTest.cpp PluginEditorTest.cpp
	@echo "🔨 Building VST test runner..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) -DCATCH_CONFIG_MAIN \
		NetworkClientTest.cpp \
		-o test-runner
	@echo "✅ Test runner built"

# Clean test artifacts  
clean:
	@rm -f test-runner
	@echo "🧹 Test artifacts cleaned"

# Test individual components
test-network: NetworkClientTest.cpp
	@echo "🧪 Testing NetworkClient..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) -DCATCH_CONFIG_MAIN \
		NetworkClientTest.cpp \
		-o test-network
	@./test-network
	@rm test-network

.PHONY: test clean test-network