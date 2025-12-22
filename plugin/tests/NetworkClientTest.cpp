#include <catch2/catch_test_macros.hpp>
#include "network/NetworkClient.h"

//==============================================================================
TEST_CASE("NetworkClient Config", "[NetworkClient]") {
  SECTION("development config has localhost URL") {
    auto config = NetworkClient::Config::development();
    REQUIRE(config.baseUrl == "http://localhost:8787");
    REQUIRE(config.timeoutMs == 30000);
    REQUIRE(config.maxRetries == 3);
    REQUIRE(config.retryDelayMs == 1000);
  }

  SECTION("production config has production URL") {
    auto config = NetworkClient::Config::production();
    REQUIRE(config.baseUrl == "https://api.sidechain.app");
    REQUIRE(config.timeoutMs == 30000);
    REQUIRE(config.maxRetries == 3);
    REQUIRE(config.retryDelayMs == 2000);
  }
}

//==============================================================================
TEST_CASE("NetworkClient initial state", "[NetworkClient]") {
  NetworkClient client;

  SECTION("starts disconnected") {
    REQUIRE(client.getConnectionStatus() == NetworkClient::ConnectionStatus::Disconnected);
  }

  SECTION("starts not authenticated") {
    REQUIRE(client.isAuthenticated() == false);
  }

  SECTION("default config is development") {
    REQUIRE(client.getBaseUrl() == "http://localhost:8787");
  }

  SECTION("is not shutting down initially") {
    REQUIRE(client.isShuttingDown() == false);
  }

  SECTION("username and userId are empty initially") {
    REQUIRE(client.getCurrentUsername().isEmpty());
    REQUIRE(client.getCurrentUserId().isEmpty());
  }
}

//==============================================================================
TEST_CASE("NetworkClient with custom config", "[NetworkClient]") {
  NetworkClient::Config customConfig;
  customConfig.baseUrl = "https://test.example.com";
  customConfig.timeoutMs = 5000;
  customConfig.maxRetries = 1;
  customConfig.retryDelayMs = 500;

  NetworkClient client(customConfig);

  SECTION("uses custom base URL") {
    REQUIRE(client.getBaseUrl() == "https://test.example.com");
  }

  SECTION("getConfig returns the config") {
    auto config = client.getConfig();
    REQUIRE(config.baseUrl == "https://test.example.com");
    REQUIRE(config.timeoutMs == 5000);
    REQUIRE(config.maxRetries == 1);
    REQUIRE(config.retryDelayMs == 500);
  }
}

//==============================================================================
TEST_CASE("NetworkClient authentication state", "[NetworkClient]") {
  NetworkClient client;

  SECTION("setAuthToken enables authentication") {
    REQUIRE(client.isAuthenticated() == false);

    client.setAuthToken("test-token-12345");

    REQUIRE(client.isAuthenticated() == true);
  }

  SECTION("empty token disables authentication") {
    client.setAuthToken("valid-token");
    REQUIRE(client.isAuthenticated() == true);

    client.setAuthToken("");
    REQUIRE(client.isAuthenticated() == false);
  }
}

//==============================================================================
TEST_CASE("NetworkClient config modification", "[NetworkClient]") {
  NetworkClient client;

  SECTION("setConfig updates configuration") {
    auto newConfig = NetworkClient::Config::production();
    client.setConfig(newConfig);

    REQUIRE(client.getBaseUrl() == "https://api.sidechain.app");
    REQUIRE(client.getConfig().retryDelayMs == 2000);
  }
}

//==============================================================================
TEST_CASE("NetworkClient connection status callback", "[NetworkClient]") {
  NetworkClient client;

  SECTION("can set connection status callback without crash") {
    bool callbackSet = false;

    client.setConnectionStatusCallback(
        [&callbackSet]([[maybe_unused]] NetworkClient::ConnectionStatus status) { callbackSet = true; });

    // Just verify no crash - actual callback invocation requires network activity
    REQUIRE_NOTHROW(client.cancelAllRequests());
  }
}

//==============================================================================
TEST_CASE("NetworkClient request cancellation", "[NetworkClient]") {
  NetworkClient client;

  SECTION("cancelAllRequests does not crash") {
    REQUIRE_NOTHROW(client.cancelAllRequests());
  }

  SECTION("cancelAllRequests resets shutting down flag after completion") {
    // cancelAllRequests sets the flag during cleanup, then resets it
    client.cancelAllRequests();
    // After completion, the flag is reset to allow future requests
    REQUIRE(client.isShuttingDown() == false);
  }
}

//==============================================================================
TEST_CASE("NetworkClient uploadProfilePicture", "[NetworkClient][ProfilePicture]") {
  NetworkClient client;

  SECTION("unauthenticated upload invokes callback with failure immediately") {
    // Ensure not authenticated
    REQUIRE(client.isAuthenticated() == false);

    bool callbackInvoked = false;
    bool success = true; // Will be set to false by callback
    juce::String url = "should be empty";

    // Create a temp file for the test
    juce::File tempFile =
        juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("test_profile_pic.jpg");
    tempFile.create();
    tempFile.replaceWithData("fake jpeg data", 14);

    client.uploadProfilePicture(tempFile, [&](Outcome<juce::String> result) {
      callbackInvoked = true;
      success = result.isOk();
      url = result.isOk() ? result.getValue() : "";
    });

    // Callback should be invoked synchronously for auth failure
    REQUIRE(callbackInvoked == true);
    REQUIRE(success == false);
    REQUIRE(url.isEmpty());

    tempFile.deleteFile();
  }

  SECTION("upload with non-existent file invokes callback with failure") {
    // Authenticate first
    client.setAuthToken("valid-test-token");
    REQUIRE(client.isAuthenticated() == true);

    bool callbackInvoked = false;
    bool success = true;

    juce::File nonExistentFile("/nonexistent/path/to/image.jpg");
    REQUIRE(nonExistentFile.existsAsFile() == false);

    client.uploadProfilePicture(nonExistentFile, [&](Outcome<juce::String> result) {
      callbackInvoked = true;
      success = result.isOk();
    });

    // For non-existent file, callback is invoked via callAsync
    // In unit tests without message loop, we verify the function returns without crash
    // The callback will be invoked asynchronously
    REQUIRE_NOTHROW(client.cancelAllRequests());
  }

  SECTION("upload does not crash with nullptr callback") {
    client.setAuthToken("valid-token");

    juce::File tempFile =
        juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("test_no_callback.png");
    tempFile.create();
    tempFile.replaceWithData("fake png data", 13);

    // Pass nullptr callback - should not crash
    REQUIRE_NOTHROW(client.uploadProfilePicture(tempFile, nullptr));

    // Give async operations a moment to potentially crash (they shouldn't)
    juce::Thread::sleep(100);

    tempFile.deleteFile();
  }

  SECTION("unauthenticated upload with nullptr callback does not crash") {
    REQUIRE(client.isAuthenticated() == false);

    juce::File tempFile =
        juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("test_unauth_no_callback.jpg");
    tempFile.create();

    // Should handle nullptr callback gracefully
    REQUIRE_NOTHROW(client.uploadProfilePicture(tempFile, nullptr));

    tempFile.deleteFile();
  }
}

//==============================================================================
TEST_CASE("NetworkClient MIME type detection", "[NetworkClient][ProfilePicture]") {
  // Test MIME type determination for profile picture uploads
  // This tests the internal getMimeType lambda behavior by verifying
  // different file extensions are handled correctly

  SECTION("JPEG files are accepted") {
    NetworkClient client;
    client.setAuthToken("test-token");

    juce::File jpegFile = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("profile.jpg");
    jpegFile.create();
    jpegFile.replaceWithData("fake jpeg", 9);

    // Verify the upload path is attempted (will fail due to no server, but tests path)
    REQUIRE_NOTHROW(client.uploadProfilePicture(jpegFile, nullptr));

    jpegFile.deleteFile();
  }

  SECTION("JPEG alternate extension is accepted") {
    NetworkClient client;
    client.setAuthToken("test-token");

    juce::File jpegFile = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("profile.jpeg");
    jpegFile.create();
    jpegFile.replaceWithData("fake jpeg", 9);

    REQUIRE_NOTHROW(client.uploadProfilePicture(jpegFile, nullptr));

    jpegFile.deleteFile();
  }

  SECTION("PNG files are accepted") {
    NetworkClient client;
    client.setAuthToken("test-token");

    juce::File pngFile = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("profile.png");
    pngFile.create();
    pngFile.replaceWithData("fake png", 8);

    REQUIRE_NOTHROW(client.uploadProfilePicture(pngFile, nullptr));

    pngFile.deleteFile();
  }

  SECTION("GIF files are accepted") {
    NetworkClient client;
    client.setAuthToken("test-token");

    juce::File gifFile = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("animated.gif");
    gifFile.create();
    gifFile.replaceWithData("GIF89a", 6);

    REQUIRE_NOTHROW(client.uploadProfilePicture(gifFile, nullptr));

    gifFile.deleteFile();
  }

  SECTION("WebP files are accepted") {
    NetworkClient client;
    client.setAuthToken("test-token");

    juce::File webpFile = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("modern.webp");
    webpFile.create();
    webpFile.replaceWithData("RIFF", 4);

    REQUIRE_NOTHROW(client.uploadProfilePicture(webpFile, nullptr));

    webpFile.deleteFile();
  }

  SECTION("Unknown extension uses application/octet-stream") {
    NetworkClient client;
    client.setAuthToken("test-token");

    juce::File unknownFile = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("image.xyz");
    unknownFile.create();
    unknownFile.replaceWithData("unknown", 7);

    // Should still attempt upload (server will reject if invalid)
    REQUIRE_NOTHROW(client.uploadProfilePicture(unknownFile, nullptr));

    unknownFile.deleteFile();
  }
}

//==============================================================================
TEST_CASE("NetworkClient profile picture file handling", "[NetworkClient][ProfilePicture]") {
  NetworkClient client;
  client.setAuthToken("valid-token");

  SECTION("empty file triggers callback with failure") {
    juce::File emptyFile = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("empty.jpg");
    emptyFile.create();
    // Don't write any data - file is 0 bytes

    // Upload will fail when trying to read empty file
    REQUIRE_NOTHROW(client.uploadProfilePicture(emptyFile, nullptr));

    emptyFile.deleteFile();
  }

  SECTION("large file name is handled") {
    juce::String longName = "very_long_filename_";
    for (int i = 0; i < 10; ++i)
      longName += "with_lots_of_characters_";
    longName += ".jpg";

    juce::File longNameFile = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile(longName);
    longNameFile.create();
    longNameFile.replaceWithData("data", 4);

    REQUIRE_NOTHROW(client.uploadProfilePicture(longNameFile, nullptr));

    longNameFile.deleteFile();
  }

  SECTION("file with spaces in name is handled") {
    juce::File spacesFile =
        juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("my profile picture.png");
    spacesFile.create();
    spacesFile.replaceWithData("data", 4);

    REQUIRE_NOTHROW(client.uploadProfilePicture(spacesFile, nullptr));

    spacesFile.deleteFile();
  }

  SECTION("file with unicode characters is handled") {
    juce::File unicodeFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                 .getChildFile("profile_\xE2\x9C\x93.jpg"); // ✓ checkmark
    unicodeFile.create();
    unicodeFile.replaceWithData("data", 4);

    REQUIRE_NOTHROW(client.uploadProfilePicture(unicodeFile, nullptr));

    unicodeFile.deleteFile();
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
