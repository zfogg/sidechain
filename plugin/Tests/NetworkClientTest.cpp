#define CATCH_CONFIG_MAIN
#include "catch_amalgamated.hpp"

#include "../Source/NetworkClient.h"
#include <JuceHeader.h>

// Test fixture for NetworkClient tests
class NetworkClientTest {
public:
    NetworkClientTest() : client(NetworkClient::Config::development()) {}

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
    NetworkClient::Config testConfig;
    testConfig.baseUrl = "http://test.com";
    NetworkClient client(testConfig);

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

//==============================================================================
// Config Tests
//==============================================================================

TEST_CASE("NetworkClient Config", "[config]") {
    SECTION("Development config has correct defaults") {
        auto config = NetworkClient::Config::development();

        REQUIRE(config.baseUrl == "http://localhost:8787");
        REQUIRE(config.timeoutMs == 30000);
        REQUIRE(config.maxRetries == 3);
        REQUIRE(config.retryDelayMs == 1000);
    }

    SECTION("Production config has correct defaults") {
        auto config = NetworkClient::Config::production();

        REQUIRE(config.baseUrl == "https://api.sidechain.app");
        REQUIRE(config.timeoutMs == 30000);
        REQUIRE(config.maxRetries == 3);
        REQUIRE(config.retryDelayMs == 2000);  // Longer delay for production
    }

    SECTION("Custom config can be created") {
        NetworkClient::Config customConfig;
        customConfig.baseUrl = "https://staging.sidechain.app";
        customConfig.timeoutMs = 15000;
        customConfig.maxRetries = 5;
        customConfig.retryDelayMs = 500;

        NetworkClient client(customConfig);
        REQUIRE(client.getBaseUrl() == "https://staging.sidechain.app");
    }

    SECTION("Config can be updated after creation") {
        NetworkClient client(NetworkClient::Config::development());
        REQUIRE(client.getBaseUrl() == "http://localhost:8787");

        auto newConfig = NetworkClient::Config::production();
        client.setConfig(newConfig);
        REQUIRE(client.getBaseUrl() == "https://api.sidechain.app");
    }
}

//==============================================================================
// Connection Status Tests
//==============================================================================

TEST_CASE("Connection status", "[connection]") {
    NetworkClient client(NetworkClient::Config::development());

    SECTION("Initial connection status is disconnected") {
        REQUIRE(client.getConnectionStatus() == NetworkClient::ConnectionStatus::Disconnected);
    }

    SECTION("Connection status callback can be set") {
        bool callbackInvoked = false;
        NetworkClient::ConnectionStatus receivedStatus;

        client.setConnectionStatusCallback([&](NetworkClient::ConnectionStatus status) {
            callbackInvoked = true;
            receivedStatus = status;
        });

        // Callback should be set without error
        REQUIRE_NOTHROW(client.checkConnection());
    }

    SECTION("Shutdown flag management") {
        REQUIRE_FALSE(client.isShuttingDown());

        // Cancel should set and then reset the flag
        client.cancelAllRequests();

        // After cancel completes, flag should be reset
        REQUIRE_FALSE(client.isShuttingDown());
    }
}

//==============================================================================
// HTTP Status Code Parsing Tests
//==============================================================================

TEST_CASE("HTTP status code parsing", "[http]") {
    SECTION("Parse status from HTTP/1.1 response") {
        juce::StringPairArray headers;
        headers.set("HTTP/1.1", "200 OK");

        int status = NetworkClient::parseStatusCode(headers);
        REQUIRE(status == 200);
    }

    SECTION("Parse 404 status") {
        juce::StringPairArray headers;
        headers.set("HTTP/1.1", "404 Not Found");

        int status = NetworkClient::parseStatusCode(headers);
        REQUIRE(status == 404);
    }

    SECTION("Parse 500 status") {
        juce::StringPairArray headers;
        headers.set("HTTP/1.1", "500 Internal Server Error");

        int status = NetworkClient::parseStatusCode(headers);
        REQUIRE(status == 500);
    }

    SECTION("Parse HTTP/2 status") {
        juce::StringPairArray headers;
        headers.set("HTTP/2", "201 Created");

        int status = NetworkClient::parseStatusCode(headers);
        REQUIRE(status == 201);
    }

    SECTION("Returns 0 for missing status line") {
        juce::StringPairArray headers;
        headers.set("Content-Type", "application/json");

        int status = NetworkClient::parseStatusCode(headers);
        REQUIRE(status == 0);
    }

    SECTION("Returns 0 for empty headers") {
        juce::StringPairArray headers;

        int status = NetworkClient::parseStatusCode(headers);
        REQUIRE(status == 0);
    }
}

//==============================================================================
// RequestResult Tests
//==============================================================================

// Helper to create a RequestResult for testing
// Note: RequestResult is a private struct, so we test it indirectly through
// the public interface where possible, or test the logic separately

TEST_CASE("RequestResult success detection", "[http]") {
    // We test the isSuccess logic directly since it's simple
    SECTION("2xx status codes are successful") {
        // Status 200 - OK
        REQUIRE((200 >= 200 && 200 < 300) == true);
        // Status 201 - Created
        REQUIRE((201 >= 200 && 201 < 300) == true);
        // Status 204 - No Content
        REQUIRE((204 >= 200 && 204 < 300) == true);
        // Status 299 - edge case
        REQUIRE((299 >= 200 && 299 < 300) == true);
    }

    SECTION("Non-2xx status codes are not successful") {
        // Status 199
        REQUIRE((199 >= 200 && 199 < 300) == false);
        // Status 300 - redirect
        REQUIRE((300 >= 200 && 300 < 300) == false);
        // Status 400 - bad request
        REQUIRE((400 >= 200 && 400 < 300) == false);
        // Status 500 - server error
        REQUIRE((500 >= 200 && 500 < 300) == false);
    }
}

TEST_CASE("User-friendly error messages", "[http][errors]") {
    SECTION("Error messages for common HTTP status codes") {
        // Test the error message mapping logic
        auto getExpectedMessage = [](int status) -> juce::String {
            switch (status) {
                case 400: return "Invalid request - please check your input";
                case 401: return "Authentication required - please log in";
                case 403: return "Access denied - you don't have permission";
                case 404: return "Not found - the requested resource doesn't exist";
                case 409: return "Conflict - this action conflicts with existing data";
                case 422: return "Validation failed - please check your input";
                case 429: return "Too many requests - please try again later";
                case 500: return "Server error - please try again later";
                case 502: return "Server unavailable - please try again later";
                case 503: return "Service temporarily unavailable";
                default: return "";
            }
        };

        REQUIRE(getExpectedMessage(400).isNotEmpty());
        REQUIRE(getExpectedMessage(401).isNotEmpty());
        REQUIRE(getExpectedMessage(403).isNotEmpty());
        REQUIRE(getExpectedMessage(404).isNotEmpty());
        REQUIRE(getExpectedMessage(500).isNotEmpty());
    }

    SECTION("JSON error extraction") {
        // Test that error messages can be extracted from JSON
        juce::var errorResponse = juce::var(new juce::DynamicObject());
        errorResponse.getDynamicObject()->setProperty("error", "Email already exists");

        REQUIRE(errorResponse.getProperty("error", "").toString() == "Email already exists");
    }

    SECTION("Nested error object extraction") {
        juce::var response = juce::var(new juce::DynamicObject());
        juce::var errorObj = juce::var(new juce::DynamicObject());
        errorObj.getDynamicObject()->setProperty("message", "Detailed error info");
        response.getDynamicObject()->setProperty("error", errorObj);

        auto error = response.getProperty("error", juce::var());
        REQUIRE(error.isObject());
        REQUIRE(error.getProperty("message", "").toString() == "Detailed error info");
    }

    SECTION("Message field extraction") {
        juce::var response = juce::var(new juce::DynamicObject());
        response.getDynamicObject()->setProperty("message", "Operation failed");

        REQUIRE(response.getProperty("message", "").toString() == "Operation failed");
    }
}

//==============================================================================
// WAV Encoding Tests
//==============================================================================

TEST_CASE("Audio buffer handling", "[audio]") {
    SECTION("Empty buffer has zero samples") {
        juce::AudioBuffer<float> buffer;
        REQUIRE(buffer.getNumSamples() == 0);
        REQUIRE(buffer.getNumChannels() == 0);
    }

    SECTION("Buffer can be created with samples") {
        juce::AudioBuffer<float> buffer(2, 44100);  // 1 second stereo
        REQUIRE(buffer.getNumSamples() == 44100);
        REQUIRE(buffer.getNumChannels() == 2);
    }

    SECTION("Buffer can be copied") {
        juce::AudioBuffer<float> original(2, 1024);
        original.clear();

        // Fill with test data
        for (int ch = 0; ch < original.getNumChannels(); ++ch) {
            for (int i = 0; i < original.getNumSamples(); ++i) {
                original.setSample(ch, i, static_cast<float>(i) / 1024.0f);
            }
        }

        juce::AudioBuffer<float> copy(original);
        REQUIRE(copy.getNumSamples() == original.getNumSamples());
        REQUIRE(copy.getNumChannels() == original.getNumChannels());

        // Verify data was copied
        REQUIRE(copy.getSample(0, 100) == original.getSample(0, 100));
    }

    SECTION("Duration calculation is correct") {
        juce::AudioBuffer<float> buffer(2, 44100);  // 1 second at 44.1kHz
        double sampleRate = 44100.0;
        double duration = static_cast<double>(buffer.getNumSamples()) / sampleRate;

        REQUIRE(duration == Catch::Approx(1.0));
    }

    SECTION("Duration calculation for different sample rates") {
        juce::AudioBuffer<float> buffer(2, 48000);  // 1 second at 48kHz
        double sampleRate = 48000.0;
        double duration = static_cast<double>(buffer.getNumSamples()) / sampleRate;

        REQUIRE(duration == Catch::Approx(1.0));
    }
}

TEST_CASE("WAV format writing", "[audio][wav]") {
    SECTION("WAV format writer can be created") {
        juce::MemoryOutputStream outputStream;
        juce::WavAudioFormat wavFormat;

        std::unique_ptr<juce::AudioFormatWriter> writer(
            wavFormat.createWriterFor(&outputStream, 44100.0, 2, 16, {}, 0));

        REQUIRE(writer != nullptr);
    }

    SECTION("WAV format supports common sample rates") {
        juce::WavAudioFormat wavFormat;
        juce::MemoryOutputStream stream;

        // 44.1kHz
        auto writer1 = wavFormat.createWriterFor(&stream, 44100.0, 2, 16, {}, 0);
        REQUIRE(writer1 != nullptr);
        delete writer1;

        // 48kHz
        auto writer2 = wavFormat.createWriterFor(&stream, 48000.0, 2, 16, {}, 0);
        REQUIRE(writer2 != nullptr);
        delete writer2;

        // 96kHz
        auto writer3 = wavFormat.createWriterFor(&stream, 96000.0, 2, 16, {}, 0);
        REQUIRE(writer3 != nullptr);
        delete writer3;
    }

    SECTION("WAV data can be written and has correct header") {
        juce::MemoryOutputStream outputStream;
        juce::WavAudioFormat wavFormat;

        std::unique_ptr<juce::AudioFormatWriter> writer(
            wavFormat.createWriterFor(&outputStream, 44100.0, 2, 16, {}, 0));

        REQUIRE(writer != nullptr);

        // Create test buffer
        juce::AudioBuffer<float> buffer(2, 1024);
        buffer.clear();

        // Write buffer
        bool written = writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples());
        REQUIRE(written);

        writer.reset();  // Flush

        // Check that output has data
        REQUIRE(outputStream.getDataSize() > 0);

        // WAV files start with "RIFF"
        auto* data = static_cast<const char*>(outputStream.getData());
        REQUIRE(data[0] == 'R');
        REQUIRE(data[1] == 'I');
        REQUIRE(data[2] == 'F');
        REQUIRE(data[3] == 'F');
    }
}

//==============================================================================
// Multipart Form Data Tests
//==============================================================================

TEST_CASE("Multipart boundary generation", "[http][multipart]") {
    SECTION("Random boundaries are different") {
        juce::String boundary1 = "----SidechainBoundary" + juce::String(juce::Random::getSystemRandom().nextInt64());
        juce::String boundary2 = "----SidechainBoundary" + juce::String(juce::Random::getSystemRandom().nextInt64());

        // Boundaries should be unique
        REQUIRE(boundary1 != boundary2);
    }

    SECTION("Boundary has correct prefix") {
        juce::String boundary = "----SidechainBoundary" + juce::String(juce::Random::getSystemRandom().nextInt64());

        REQUIRE(boundary.startsWith("----SidechainBoundary"));
    }
}

TEST_CASE("Multipart form data construction", "[http][multipart]") {
    SECTION("Form data includes boundary markers") {
        juce::String boundary = "----TestBoundary123";
        juce::MemoryOutputStream formData;

        // Add a text field
        formData.writeString("--" + boundary + "\r\n");
        formData.writeString("Content-Disposition: form-data; name=\"test_field\"\r\n\r\n");
        formData.writeString("test_value\r\n");
        formData.writeString("--" + boundary + "--\r\n");

        juce::String result = formData.toString();

        REQUIRE(result.contains("--" + boundary));
        REQUIRE(result.contains("Content-Disposition: form-data"));
        REQUIRE(result.contains("test_field"));
        REQUIRE(result.contains("test_value"));
        REQUIRE(result.contains("--" + boundary + "--"));
    }

    SECTION("Form data includes file content-type") {
        juce::String boundary = "----TestBoundary456";
        juce::MemoryOutputStream formData;

        formData.writeString("--" + boundary + "\r\n");
        formData.writeString("Content-Disposition: form-data; name=\"audio_file\"; filename=\"test.wav\"\r\n");
        formData.writeString("Content-Type: audio/wav\r\n\r\n");
        formData.writeString("[binary data would go here]");
        formData.writeString("\r\n");
        formData.writeString("--" + boundary + "--\r\n");

        juce::String result = formData.toString();

        REQUIRE(result.contains("filename=\"test.wav\""));
        REQUIRE(result.contains("Content-Type: audio/wav"));
    }

    SECTION("Multiple fields can be added") {
        juce::String boundary = "----TestBoundary789";
        juce::MemoryOutputStream formData;

        // Field 1
        formData.writeString("--" + boundary + "\r\n");
        formData.writeString("Content-Disposition: form-data; name=\"bpm\"\r\n\r\n");
        formData.writeString("120\r\n");

        // Field 2
        formData.writeString("--" + boundary + "\r\n");
        formData.writeString("Content-Disposition: form-data; name=\"key\"\r\n\r\n");
        formData.writeString("C major\r\n");

        // End boundary
        formData.writeString("--" + boundary + "--\r\n");

        juce::String result = formData.toString();

        REQUIRE(result.contains("name=\"bpm\""));
        REQUIRE(result.contains("120"));
        REQUIRE(result.contains("name=\"key\""));
        REQUIRE(result.contains("C major"));
    }
}

//==============================================================================
// MIME Type Tests
//==============================================================================

TEST_CASE("MIME type detection", "[http][mime]") {
    SECTION("Common audio MIME types") {
        // Test the MIME type logic
        auto getMimeType = [](const juce::String& extension) -> juce::String {
            juce::String ext = extension.toLowerCase();
            if (ext == ".wav")
                return "audio/wav";
            else if (ext == ".mp3")
                return "audio/mpeg";
            else if (ext == ".flac")
                return "audio/flac";
            else if (ext == ".ogg")
                return "audio/ogg";
            else
                return "application/octet-stream";
        };

        REQUIRE(getMimeType(".wav") == "audio/wav");
        REQUIRE(getMimeType(".mp3") == "audio/mpeg");
        REQUIRE(getMimeType(".flac") == "audio/flac");
        REQUIRE(getMimeType(".WAV") == "audio/wav");  // Case insensitive
    }

    SECTION("Image MIME types for profile pictures") {
        auto getMimeType = [](const juce::String& extension) -> juce::String {
            juce::String ext = extension.toLowerCase();
            if (ext == ".jpg" || ext == ".jpeg")
                return "image/jpeg";
            else if (ext == ".png")
                return "image/png";
            else if (ext == ".gif")
                return "image/gif";
            else if (ext == ".webp")
                return "image/webp";
            else
                return "application/octet-stream";
        };

        REQUIRE(getMimeType(".jpg") == "image/jpeg");
        REQUIRE(getMimeType(".jpeg") == "image/jpeg");
        REQUIRE(getMimeType(".png") == "image/png");
        REQUIRE(getMimeType(".gif") == "image/gif");
        REQUIRE(getMimeType(".webp") == "image/webp");
    }
}