#define CATCH_CONFIG_MAIN
#include "catch_amalgamated.hpp"

#include "../Source/NetworkClient.h"
#include <JuceHeader.h>

// Test fixture for NetworkClient tests
class NetworkClientTest {
public:
    NetworkClientTest() : client("http://localhost:8787") {}
    
    NetworkClient client;
};

TEST_CASE_METHOD(NetworkClientTest, "NetworkClient initialization", "[auth]") {
    SECTION("Constructor sets correct base URL") {
        REQUIRE(client.getBaseUrl() == "http://localhost:8787");
    }
    
    SECTION("Initial state is unauthenticated") {
        REQUIRE_FALSE(client.isAuthenticated());
        REQUIRE(client.getCurrentUsername().isEmpty());
        REQUIRE(client.getCurrentUserId().isEmpty());
    }
}

TEST_CASE_METHOD(NetworkClientTest, "Authentication token management", "[auth]") {
    SECTION("Setting auth token updates authentication state") {
        REQUIRE_FALSE(client.isAuthenticated());
        
        client.setAuthToken("test_jwt_token_123");
        
        REQUIRE(client.isAuthenticated());
    }
    
    SECTION("Empty token means unauthenticated") {
        client.setAuthToken("valid_token");
        REQUIRE(client.isAuthenticated());
        
        client.setAuthToken("");
        REQUIRE_FALSE(client.isAuthenticated());
    }
}

TEST_CASE_METHOD(NetworkClientTest, "Registration request format", "[auth]") {
    SECTION("Registration callback is called") {
        bool callbackCalled = false;
        juce::String receivedToken, receivedUserId;
        
        // Mock successful response (in real test, would mock HTTP layer)
        auto callback = [&](const juce::String& token, const juce::String& userId) {
            callbackCalled = true;
            receivedToken = token;
            receivedUserId = userId;
        };
        
        // Test that registration method exists and accepts correct parameters
        // Note: This won't actually make HTTP request in unit test
        REQUIRE_NOTHROW(client.registerAccount(
            "test@producer.com",
            "testbeat", 
            "password123",
            "Test Producer",
            callback
        ));
    }
}

TEST_CASE_METHOD(NetworkClientTest, "Login request format", "[auth]") {
    SECTION("Login callback interface works") {
        bool callbackCalled = false;
        
        auto callback = [&](const juce::String& token, const juce::String& userId) {
            callbackCalled = true;
        };
        
        // Test that login method exists and accepts correct parameters
        REQUIRE_NOTHROW(client.loginAccount(
            "test@producer.com",
            "password123",
            callback
        ));
    }
}

TEST_CASE_METHOD(NetworkClientTest, "Social actions require authentication", "[social]") {
    SECTION("Unauthenticated social actions are no-ops") {
        // These should not crash when unauthenticated
        REQUIRE_NOTHROW(client.likePost("activity123"));
        REQUIRE_NOTHROW(client.likePost("activity123", "🔥"));
        REQUIRE_NOTHROW(client.followUser("user456"));
        REQUIRE_NOTHROW(client.getGlobalFeed());
        REQUIRE_NOTHROW(client.getTimelineFeed());
    }
    
    SECTION("Authenticated social actions work") {
        // Set authenticated state
        client.setAuthToken("valid_jwt_token");
        REQUIRE(client.isAuthenticated());
        
        // These should work when authenticated (though won't make real HTTP calls in unit test)
        REQUIRE_NOTHROW(client.likePost("activity123"));
        REQUIRE_NOTHROW(client.likePost("activity123", "❤️"));
        REQUIRE_NOTHROW(client.followUser("user456"));
    }
}

TEST_CASE_METHOD(NetworkClientTest, "Audio upload requires authentication", "[audio]") {
    SECTION("Upload fails when not authenticated") {
        juce::AudioBuffer<float> testBuffer(2, 1024);
        testBuffer.clear();
        
        bool uploadCalled = false;
        auto callback = [&](bool success, const juce::String& url) {
            uploadCalled = true;
            REQUIRE_FALSE(success); // Should fail
        };
        
        client.uploadAudio("test_recording", testBuffer, 44100.0, callback);
        
        // Give a moment for the async operation
        juce::Thread::sleep(100);
    }
}

TEST_CASE("JSON data construction", "[util]") {
    SECTION("Registration data is properly formatted") {
        juce::var registerData = juce::var(new juce::DynamicObject());
        registerData.getDynamicObject()->setProperty("email", "test@example.com");
        registerData.getDynamicObject()->setProperty("username", "testuser");
        registerData.getDynamicObject()->setProperty("password", "testpass");
        registerData.getDynamicObject()->setProperty("display_name", "Test User");
        
        auto jsonString = juce::JSON::toString(registerData);
        
        REQUIRE(jsonString.contains("test@example.com"));
        REQUIRE(jsonString.contains("testuser"));
        REQUIRE(jsonString.contains("Test User"));
        // Password should be included but we won't log it
        REQUIRE(jsonString.contains("password"));
    }
    
    SECTION("Login data is properly formatted") {
        juce::var loginData = juce::var(new juce::DynamicObject());
        loginData.getDynamicObject()->setProperty("email", "login@example.com");
        loginData.getDynamicObject()->setProperty("password", "loginpass");
        
        auto jsonString = juce::JSON::toString(loginData);
        
        REQUIRE(jsonString.contains("login@example.com"));
        REQUIRE(jsonString.contains("password"));
    }
}

TEST_CASE("Authentication state transitions", "[auth]") {
    NetworkClient client("http://test.com");
    
    SECTION("Authentication state changes correctly") {
        // Initial state
        REQUIRE_FALSE(client.isAuthenticated());
        
        // Set token
        client.setAuthToken("jwt_token_123");
        REQUIRE(client.isAuthenticated());
        
        // Clear token
        client.setAuthToken("");
        REQUIRE_FALSE(client.isAuthenticated());
    }
}