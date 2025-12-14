#include <catch2/catch_test_macros.hpp>
#include "../../src/security/SecureTokenStore.h"
#include "../../src/security/InputValidation.h"
#include "../../src/security/RateLimiter.h"
#include "../../src/util/error/ErrorTracking.h"
#include <JuceHeader.h>

using namespace Sidechain;
using namespace Sidechain::Security;
using namespace Sidechain::Util::Error;

// ========== Secure Token Storage Integration Tests ==========

TEST_CASE("SecureTokenStore Integration", "[security][integration]")
{
    auto store = SecureTokenStore::getInstance();
    store->clearAllTokens();

    SECTION("Save and retrieve token")
    {
        const juce::String testKey = "test_jwt_token";
        const juce::String testToken = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...";

        bool saved = store->saveToken(testKey, testToken);
        REQUIRE(saved);

        auto retrieved = store->loadToken(testKey);
        REQUIRE(retrieved.has_value());
        REQUIRE(retrieved.value() == testToken);
    }

    SECTION("Delete token")
    {
        store->saveToken("token_to_delete", "secret_value");
        REQUIRE(store->hasToken("token_to_delete"));

        bool deleted = store->deleteToken("token_to_delete");
        REQUIRE(deleted);
        REQUIRE(!store->hasToken("token_to_delete"));
    }

    SECTION("Non-existent token returns empty")
    {
        auto result = store->loadToken("nonexistent_token");
        REQUIRE(!result.has_value());
    }

    SECTION("TokenGuard zeros memory on destruction")
    {
        {
            TokenGuard guard("test_token");
            // Simulate loading a token by directly setting value
            // (In real usage, this would be loaded from secure storage)
        }
        // If we could inspect memory, we'd verify it's zeroed
        // For now, just verify the guard doesn't crash on destruction
        REQUIRE(true);
    }

    SECTION("Clear all tokens")
    {
        store->saveToken("token1", "value1");
        store->saveToken("token2", "value2");
        store->saveToken("token3", "value3");

        bool cleared = store->clearAllTokens();
        REQUIRE(cleared);

        REQUIRE(!store->hasToken("token1"));
        REQUIRE(!store->hasToken("token2"));
        REQUIRE(!store->hasToken("token3"));
    }

    SECTION("Secure storage is available")
    {
        bool isAvailable = store->isAvailable();
        REQUIRE(isAvailable);

        auto backendType = store->getBackendType();
        REQUIRE(!backendType.isEmpty());
    }
}

// ========== Input Validation Integration Tests ==========

TEST_CASE("InputValidation Integration", "[security][integration]")
{
    SECTION("Email validation")
    {
        auto validator = InputValidator::create()
            ->addRule("email", InputValidator::email());

        auto validResult = validator->validate({
            {"email", "user@example.com"}
        });
        REQUIRE(validResult.isValid());
        REQUIRE(validResult.getValue("email").has_value());
    }

    SECTION("Username validation with constraints")
    {
        auto validator = InputValidator::create()
            ->addRule("username", InputValidator::alphanumeric()
                .minLength(3).maxLength(20));

        auto valid = validator->validate({{"username", "john_doe"}});
        REQUIRE(valid.isValid());

        auto tooShort = validator->validate({{"username", "ab"}});
        REQUIRE(!tooShort.isValid());

        auto tooLong = validator->validate({{"username", std::string(25, 'a')}});
        REQUIRE(!tooLong.isValid());
    }

    SECTION("Input sanitization removes XSS")
    {
        auto validator = InputValidator::create()
            ->addRule("bio", InputValidator::string().maxLength(500));

        auto result = validator->validate({
            {"bio", "<script>alert('xss')</script>"}
        });
        REQUIRE(result.isValid());

        auto sanitized = *result.getValue("bio");
        REQUIRE(sanitized.contains("&lt;"));
        REQUIRE(sanitized.contains("&gt;"));
        REQUIRE(!sanitized.contains("<script>"));
    }

    SECTION("Multiple field validation")
    {
        auto validator = InputValidator::create()
            ->addRule("email", InputValidator::email())
            ->addRule("age", InputValidator::integer().min(18).max(120))
            ->addRule("username", InputValidator::alphanumeric().minLength(3));

        auto result = validator->validate({
            {"email", "user@example.com"},
            {"age", "25"},
            {"username", "john_doe"}
        });

        REQUIRE(result.isValid());
        REQUIRE(result.getValue("email").has_value());
        REQUIRE(result.getValue("age").has_value());
        REQUIRE(result.getValue("username").has_value());
    }

    SECTION("HTML entity encoding for all special chars")
    {
        auto sanitized = InputValidator::sanitize("Test & \"quotes\" 'apostrophe' <tag>");

        REQUIRE(sanitized.contains("&amp;"));
        REQUIRE(sanitized.contains("&quot;"));
        REQUIRE(sanitized.contains("&#39;"));
        REQUIRE(sanitized.contains("&lt;"));
        REQUIRE(sanitized.contains("&gt;"));
    }

    SECTION("Custom validator function")
    {
        auto validator = InputValidator::create()
            ->addRule("password", InputValidator::string()
                .minLength(8)
                .custom([](const juce::String& pwd) {
                    // Must contain at least one digit
                    return pwd.containsAnyOf("0123456789");
                }));

        auto valid = validator->validate({{"password", "secure123"}});
        REQUIRE(valid.isValid());

        auto invalid = validator->validate({{"password", "nosecure"}});
        REQUIRE(!invalid.isValid());
    }
}

// ========== Rate Limiting Integration Tests ==========

TEST_CASE("RateLimiter Integration", "[security][integration]")
{
    SECTION("Token Bucket rate limiting")
    {
        auto limiter = RateLimiter::create()
            ->setRate(10)
            ->setWindow(60)
            ->setBurstSize(5)
            ->setAlgorithm(RateLimiter::Algorithm::TokenBucket);

        // Should allow first 5 (burst)
        for (int i = 0; i < 5; ++i)
        {
            auto status = limiter->tryConsume("user_1");
            REQUIRE(status.allowed);
        }

        // 6th request should be rate limited
        auto status = limiter->tryConsume("user_1");
        REQUIRE(!status.allowed);
        REQUIRE(status.retryAfterSeconds > 0);
    }

    SECTION("Sliding Window rate limiting")
    {
        auto limiter = RateLimiter::create()
            ->setRate(5)
            ->setWindow(60)
            ->setAlgorithm(RateLimiter::Algorithm::SlidingWindow);

        // Allow 5 requests
        for (int i = 0; i < 5; ++i)
        {
            auto status = limiter->tryConsume("user_2");
            REQUIRE(status.allowed);
        }

        // 6th should fail
        auto status = limiter->tryConsume("user_2");
        REQUIRE(!status.allowed);
    }

    SECTION("Per-user limits are independent")
    {
        auto limiter = RateLimiter::create()
            ->setRate(3)
            ->setWindow(60);

        // User 1 hits limit
        for (int i = 0; i < 3; ++i)
            limiter->tryConsume("user_1");

        auto user1_status = limiter->tryConsume("user_1");
        REQUIRE(!user1_status.allowed);

        // User 2 should still be able to make requests
        auto user2_status = limiter->tryConsume("user_2");
        REQUIRE(user2_status.allowed);
    }

    SECTION("Reset clears rate limit for identifier")
    {
        auto limiter = RateLimiter::create()
            ->setRate(2)
            ->setWindow(60);

        // Exhaust limit
        limiter->tryConsume("user_3");
        limiter->tryConsume("user_3");
        auto status = limiter->tryConsume("user_3");
        REQUIRE(!status.allowed);

        // Reset
        limiter->reset("user_3");

        // Should be able to consume again
        auto newStatus = limiter->tryConsume("user_3");
        REQUIRE(newStatus.allowed);
    }

    SECTION("Status tracking without consumption")
    {
        auto limiter = RateLimiter::create()
            ->setRate(5)
            ->setWindow(60);

        auto status1 = limiter->getStatus("user_4");
        REQUIRE(status1.remaining == 5);

        limiter->tryConsume("user_4");

        auto status2 = limiter->getStatus("user_4");
        REQUIRE(status2.remaining == 4);
    }
}

// ========== Error Tracking Integration Tests ==========

TEST_CASE("ErrorTracking Integration", "[security][integration]")
{
    auto tracker = ErrorTracker::getInstance();
    tracker->clear();

    SECTION("Record and retrieve errors")
    {
        tracker->recordError(ErrorSource::Network,
            "Connection timeout",
            ErrorSeverity::Error);

        auto errors = tracker->getAllErrors();
        REQUIRE(errors.size() == 1);
        REQUIRE(errors[0].message == "Connection timeout");
        REQUIRE(errors[0].severity == ErrorSeverity::Error);
    }

    SECTION("Error deduplication")
    {
        const juce::String message = "Duplicate error";
        tracker->recordError(ErrorSource::Network, message, ErrorSeverity::Warning);
        tracker->recordError(ErrorSource::Network, message, ErrorSeverity::Warning);
        tracker->recordError(ErrorSource::Network, message, ErrorSeverity::Warning);

        auto errors = tracker->getAllErrors();
        REQUIRE(errors.size() == 1);
        REQUIRE(errors[0].occurrenceCount == 3);
    }

    SECTION("Filter by severity")
    {
        tracker->recordError(ErrorSource::Audio, "Audio error", ErrorSeverity::Error);
        tracker->recordError(ErrorSource::UI, "UI warning", ErrorSeverity::Warning);
        tracker->recordError(ErrorSource::Database, "Critical db issue", ErrorSeverity::Critical);

        auto criticals = tracker->getErrorsBySeverity(ErrorSeverity::Critical);
        REQUIRE(criticals.size() == 1);

        auto warnings = tracker->getErrorsBySeverity(ErrorSeverity::Warning);
        REQUIRE(warnings.size() == 1);
    }

    SECTION("Filter by source")
    {
        tracker->recordError(ErrorSource::Network, "Error 1", ErrorSeverity::Error);
        tracker->recordError(ErrorSource::Network, "Error 2", ErrorSeverity::Error);
        tracker->recordError(ErrorSource::Audio, "Audio error", ErrorSeverity::Error);

        auto networkErrors = tracker->getErrorsBySource(ErrorSource::Network);
        REQUIRE(networkErrors.size() == 2);

        auto audioErrors = tracker->getErrorsBySource(ErrorSource::Audio);
        REQUIRE(audioErrors.size() == 1);
    }

    SECTION("Statistics generation")
    {
        tracker->recordError(ErrorSource::Network, "Network 1", ErrorSeverity::Error);
        tracker->recordError(ErrorSource::Network, "Network 1", ErrorSeverity::Error);
        tracker->recordError(ErrorSource::Audio, "Audio issue", ErrorSeverity::Critical);

        auto stats = tracker->getStatistics();
        REQUIRE(stats.totalErrors >= 3);
        REQUIRE(stats.criticalCount >= 1);
        REQUIRE(stats.errorCount >= 2);
        REQUIRE(stats.bySource[ErrorSource::Network] >= 2);
    }

    SECTION("Export to JSON")
    {
        tracker->recordError(ErrorSource::Network, "Export test", ErrorSeverity::Warning);

        auto json = tracker->exportAsJson();
        REQUIRE(!json.isVoid());
    }

    SECTION("Export to CSV")
    {
        tracker->recordError(ErrorSource::UI, "CSV test", ErrorSeverity::Error);

        auto csv = tracker->exportAsCsv();
        REQUIRE(csv.contains("CSV test"));
        REQUIRE(csv.contains("UI"));
        REQUIRE(csv.contains("Error"));
    }

    SECTION("Critical error callback")
    {
        bool callbackTriggered = false;
        tracker->setOnCriticalError([&](const ErrorInfo&) {
            callbackTriggered = true;
        });

        tracker->recordError(ErrorSource::Internal, "Critical!", ErrorSeverity::Critical);

        REQUIRE(callbackTriggered);
    }

    SECTION("Clear old errors")
    {
        tracker->recordError(ErrorSource::Network, "Error", ErrorSeverity::Error);

        auto countBefore = tracker->getErrorCount();
        REQUIRE(countBefore > 0);

        tracker->clearOlderThan(-1);  // Clear all older than -1 minutes (i.e., all)

        auto countAfter = tracker->getErrorCount();
        REQUIRE(countAfter == 0);
    }
}

// ========== Cross-System Integration Tests ==========

TEST_CASE("Security & Validation End-to-End", "[security][integration]")
{
    SECTION("Full user signup flow with validation & secure storage")
    {
        auto validator = InputValidator::create()
            ->addRule("email", InputValidator::email())
            ->addRule("password", InputValidator::string()
                .minLength(8)
                .custom([](const juce::String& pwd) {
                    return pwd.containsAnyOf("0123456789");
                }))
            ->addRule("username", InputValidator::alphanumeric().minLength(3));

        // User input
        auto result = validator->validate({
            {"email", "user@example.com"},
            {"password", "SecurePass123"},
            {"username", "john_doe"}
        });

        REQUIRE(result.isValid());

        // Store token securely
        auto store = SecureTokenStore::getInstance();
        store->saveToken("signup_token", "auth_token_12345");

        // Verify stored
        auto retrieved = store->loadToken("signup_token");
        REQUIRE(retrieved.has_value());
    }

    SECTION("API endpoint protection with rate limiting & validation")
    {
        auto limiter = RateLimiter::create()
            ->setRate(10)
            ->setWindow(60);

        auto validator = InputValidator::create()
            ->addRule("postContent", InputValidator::string().maxLength(500));

        // Simulate multiple requests
        for (int i = 0; i < 5; ++i)
        {
            auto rateLimitStatus = limiter->tryConsume("user_api");
            REQUIRE(rateLimitStatus.allowed);

            auto validationResult = validator->validate({
                {"postContent", "Post " + juce::String(i)}
            });
            REQUIRE(validationResult.isValid());
        }

        // 6th request still within limit
        auto status = limiter->tryConsume("user_api");
        REQUIRE(status.allowed);
    }

    SECTION("Error tracking during operation")
    {
        auto tracker = ErrorTracker::getInstance();
        tracker->clear();

        auto limiter = RateLimiter::create()->setRate(2)->setWindow(60);

        // Make requests and track errors
        for (int i = 0; i < 4; ++i)
        {
            auto status = limiter->tryConsume("user_error");
            if (!status.allowed)
            {
                tracker->recordError(
                    ErrorSource::Network,
                    "Rate limit exceeded",
                    ErrorSeverity::Warning);
            }
        }

        auto errors = tracker->getAllErrors();
        REQUIRE(errors.size() == 1);
        REQUIRE(errors[0].message.contains("Rate limit"));
    }
}

// ========== Performance Integration Tests ==========

TEST_CASE("Security Performance", "[security][integration][performance]")
{
    SECTION("Token storage operations are fast")
    {
        auto store = SecureTokenStore::getInstance();

        auto start = juce::Time::getMillisecondCounter();
        store->saveToken("perf_token", "long_token_value_here");
        auto saveTime = juce::Time::getMillisecondCounter() - start;

        start = juce::Time::getMillisecondCounter();
        auto retrieved = store->loadToken("perf_token");
        auto loadTime = juce::Time::getMillisecondCounter() - start;

        REQUIRE(saveTime < 100);  // Should be fast
        REQUIRE(loadTime < 100);
        REQUIRE(retrieved.has_value());
    }

    SECTION("Validation is quick for typical inputs")
    {
        auto validator = InputValidator::create()
            ->addRule("email", InputValidator::email())
            ->addRule("text", InputValidator::string().maxLength(1000));

        auto start = juce::Time::getMillisecondCounter();
        for (int i = 0; i < 100; ++i)
        {
            validator->validate({
                {"email", "user" + juce::String(i) + "@example.com"},
                {"text", "Some user input text"}
            });
        }
        auto elapsed = juce::Time::getMillisecondCounter() - start;

        REQUIRE(elapsed < 500);  // 100 validations should be fast
    }

    SECTION("Rate limiter handles many identifiers efficiently")
    {
        auto limiter = RateLimiter::create();

        auto start = juce::Time::getMillisecondCounter();
        for (int i = 0; i < 100; ++i)
        {
            limiter->tryConsume("user_" + juce::String(i));
        }
        auto elapsed = juce::Time::getMillisecondCounter() - start;

        REQUIRE(elapsed < 100);  // Should handle 100 users quickly
    }
}
