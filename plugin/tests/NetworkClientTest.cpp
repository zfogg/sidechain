#include <catch2/catch_test_macros.hpp>
#include "NetworkClient.h"

//==============================================================================
TEST_CASE("NetworkClient Config", "[NetworkClient]")
{
    SECTION("development config has localhost URL")
    {
        auto config = NetworkClient::Config::development();
        REQUIRE(config.baseUrl == "http://localhost:8787");
        REQUIRE(config.timeoutMs == 30000);
        REQUIRE(config.maxRetries == 3);
        REQUIRE(config.retryDelayMs == 1000);
    }

    SECTION("production config has production URL")
    {
        auto config = NetworkClient::Config::production();
        REQUIRE(config.baseUrl == "https://api.sidechain.app");
        REQUIRE(config.timeoutMs == 30000);
        REQUIRE(config.maxRetries == 3);
        REQUIRE(config.retryDelayMs == 2000);
    }
}

//==============================================================================
TEST_CASE("NetworkClient initial state", "[NetworkClient]")
{
    NetworkClient client;

    SECTION("starts disconnected")
    {
        REQUIRE(client.getConnectionStatus() == NetworkClient::ConnectionStatus::Disconnected);
    }

    SECTION("starts not authenticated")
    {
        REQUIRE(client.isAuthenticated() == false);
    }

    SECTION("default config is development")
    {
        REQUIRE(client.getBaseUrl() == "http://localhost:8787");
    }

    SECTION("is not shutting down initially")
    {
        REQUIRE(client.isShuttingDown() == false);
    }

    SECTION("username and userId are empty initially")
    {
        REQUIRE(client.getCurrentUsername().isEmpty());
        REQUIRE(client.getCurrentUserId().isEmpty());
    }
}

//==============================================================================
TEST_CASE("NetworkClient with custom config", "[NetworkClient]")
{
    NetworkClient::Config customConfig;
    customConfig.baseUrl = "https://test.example.com";
    customConfig.timeoutMs = 5000;
    customConfig.maxRetries = 1;
    customConfig.retryDelayMs = 500;

    NetworkClient client(customConfig);

    SECTION("uses custom base URL")
    {
        REQUIRE(client.getBaseUrl() == "https://test.example.com");
    }

    SECTION("getConfig returns the config")
    {
        auto config = client.getConfig();
        REQUIRE(config.baseUrl == "https://test.example.com");
        REQUIRE(config.timeoutMs == 5000);
        REQUIRE(config.maxRetries == 1);
        REQUIRE(config.retryDelayMs == 500);
    }
}

//==============================================================================
TEST_CASE("NetworkClient authentication state", "[NetworkClient]")
{
    NetworkClient client;

    SECTION("setAuthToken enables authentication")
    {
        REQUIRE(client.isAuthenticated() == false);

        client.setAuthToken("test-token-12345");

        REQUIRE(client.isAuthenticated() == true);
    }

    SECTION("empty token disables authentication")
    {
        client.setAuthToken("valid-token");
        REQUIRE(client.isAuthenticated() == true);

        client.setAuthToken("");
        REQUIRE(client.isAuthenticated() == false);
    }
}

//==============================================================================
TEST_CASE("NetworkClient config modification", "[NetworkClient]")
{
    NetworkClient client;

    SECTION("setConfig updates configuration")
    {
        auto newConfig = NetworkClient::Config::production();
        client.setConfig(newConfig);

        REQUIRE(client.getBaseUrl() == "https://api.sidechain.app");
        REQUIRE(client.getConfig().retryDelayMs == 2000);
    }
}

//==============================================================================
TEST_CASE("NetworkClient connection status callback", "[NetworkClient]")
{
    NetworkClient client;

    SECTION("can set connection status callback without crash")
    {
        bool callbackSet = false;

        client.setConnectionStatusCallback([&callbackSet](NetworkClient::ConnectionStatus status) {
            callbackSet = true;
        });

        // Just verify no crash - actual callback invocation requires network activity
        REQUIRE_NOTHROW(client.cancelAllRequests());
    }
}

//==============================================================================
TEST_CASE("NetworkClient request cancellation", "[NetworkClient]")
{
    NetworkClient client;

    SECTION("cancelAllRequests does not crash")
    {
        REQUIRE_NOTHROW(client.cancelAllRequests());
    }

    SECTION("cancelAllRequests resets shutting down flag after completion")
    {
        // cancelAllRequests sets the flag during cleanup, then resets it
        client.cancelAllRequests();
        // After completion, the flag is reset to allow future requests
        REQUIRE(client.isShuttingDown() == false);
    }
}

//==============================================================================
// Note: Testing actual HTTP methods (registerAccount, loginAccount, uploadAudio, etc.)
// would require either:
// 1. A mock HTTP server
// 2. Dependency injection for the HTTP layer
// 3. Integration tests with a real backend
//
// These unit tests focus on state management and configuration.
// Integration tests should be added separately to test actual network behavior.
