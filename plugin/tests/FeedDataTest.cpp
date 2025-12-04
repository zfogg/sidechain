#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "FeedPost.h"
#include "FeedDataManager.h"

using Catch::Approx;

//==============================================================================
// FeedPost Tests
//==============================================================================

TEST_CASE("FeedPost basic construction", "[FeedPost]")
{
    FeedPost post;

    SECTION("Default values")
    {
        REQUIRE(post.id.isEmpty());
        REQUIRE(post.foreignId.isEmpty());
        REQUIRE(post.actor.isEmpty());
        REQUIRE(post.audioUrl.isEmpty());
        REQUIRE(post.durationSeconds == Approx(0.0f));
        REQUIRE(post.bpm == 0);
        REQUIRE(post.likeCount == 0);
        REQUIRE(post.playCount == 0);
        REQUIRE(post.commentCount == 0);
        REQUIRE(post.isLiked == false);
        REQUIRE(post.status == FeedPost::Status::Unknown);
        REQUIRE(post.genres.isEmpty());
    }

    SECTION("isValid returns false for empty post")
    {
        REQUIRE(post.isValid() == false);
    }
}

//==============================================================================
TEST_CASE("FeedPost::extractUserId", "[FeedPost]")
{
    SECTION("Standard user:id format")
    {
        REQUIRE(FeedPost::extractUserId("user:12345") == "12345");
        REQUIRE(FeedPost::extractUserId("user:abc-def-123") == "abc-def-123");
    }

    SECTION("Stream User SU:user:id format")
    {
        REQUIRE(FeedPost::extractUserId("SU:user:12345") == "12345");
        REQUIRE(FeedPost::extractUserId("SU:user:test-user") == "test-user");
    }

    SECTION("SU: without user prefix")
    {
        REQUIRE(FeedPost::extractUserId("SU:12345") == "12345");
    }

    SECTION("No prefix - returns as-is")
    {
        REQUIRE(FeedPost::extractUserId("12345") == "12345");
        REQUIRE(FeedPost::extractUserId("plain-id") == "plain-id");
    }

    SECTION("Empty string")
    {
        REQUIRE(FeedPost::extractUserId("").isEmpty());
    }
}

//==============================================================================
TEST_CASE("FeedPost::formatTimeAgo", "[FeedPost]")
{
    auto now = juce::Time::getCurrentTime();

    SECTION("Just now (< 60 seconds)")
    {
        auto recent = now - juce::RelativeTime::seconds(30);
        REQUIRE(FeedPost::formatTimeAgo(recent) == "just now");

        auto veryRecent = now - juce::RelativeTime::seconds(5);
        REQUIRE(FeedPost::formatTimeAgo(veryRecent) == "just now");
    }

    SECTION("Minutes ago")
    {
        auto oneMin = now - juce::RelativeTime::minutes(1);
        REQUIRE(FeedPost::formatTimeAgo(oneMin) == "1 min ago");

        auto fiveMins = now - juce::RelativeTime::minutes(5);
        REQUIRE(FeedPost::formatTimeAgo(fiveMins) == "5 mins ago");

        auto thirtyMins = now - juce::RelativeTime::minutes(30);
        REQUIRE(FeedPost::formatTimeAgo(thirtyMins) == "30 mins ago");
    }

    SECTION("Hours ago")
    {
        auto oneHour = now - juce::RelativeTime::hours(1);
        REQUIRE(FeedPost::formatTimeAgo(oneHour) == "1 hour ago");

        auto fiveHours = now - juce::RelativeTime::hours(5);
        REQUIRE(FeedPost::formatTimeAgo(fiveHours) == "5 hours ago");

        auto twentyThreeHours = now - juce::RelativeTime::hours(23);
        REQUIRE(FeedPost::formatTimeAgo(twentyThreeHours) == "23 hours ago");
    }

    SECTION("Days ago")
    {
        auto oneDay = now - juce::RelativeTime::days(1);
        REQUIRE(FeedPost::formatTimeAgo(oneDay) == "1 day ago");

        auto threeDays = now - juce::RelativeTime::days(3);
        REQUIRE(FeedPost::formatTimeAgo(threeDays) == "3 days ago");

        auto sixDays = now - juce::RelativeTime::days(6);
        REQUIRE(FeedPost::formatTimeAgo(sixDays) == "6 days ago");
    }

    SECTION("Weeks ago")
    {
        auto oneWeek = now - juce::RelativeTime::days(7);
        REQUIRE(FeedPost::formatTimeAgo(oneWeek) == "1 week ago");

        auto twoWeeks = now - juce::RelativeTime::days(14);
        REQUIRE(FeedPost::formatTimeAgo(twoWeeks) == "2 weeks ago");
    }

    SECTION("Months ago")
    {
        auto oneMonth = now - juce::RelativeTime::days(35);
        REQUIRE(FeedPost::formatTimeAgo(oneMonth) == "1 month ago");

        auto threeMonths = now - juce::RelativeTime::days(100);
        REQUIRE(FeedPost::formatTimeAgo(threeMonths) == "3 months ago");
    }

    SECTION("Years ago")
    {
        auto oneYear = now - juce::RelativeTime::days(400);
        REQUIRE(FeedPost::formatTimeAgo(oneYear) == "1 year ago");

        auto twoYears = now - juce::RelativeTime::days(800);
        REQUIRE(FeedPost::formatTimeAgo(twoYears) == "2 years ago");
    }

    SECTION("Future time returns just now")
    {
        auto future = now + juce::RelativeTime::hours(1);
        REQUIRE(FeedPost::formatTimeAgo(future) == "just now");
    }
}

//==============================================================================
TEST_CASE("FeedPost::fromJson basic parsing", "[FeedPost]")
{
    SECTION("Parse complete activity JSON")
    {
        juce::String jsonStr = R"({
            "id": "act-123",
            "foreign_id": "loop:uuid-456",
            "actor": "user:789",
            "verb": "posted",
            "object": "loop:uuid-456",
            "time": "2024-06-15T10:30:00.000Z",
            "audio_url": "https://cdn.example.com/audio.mp3",
            "waveform": "<svg>...</svg>",
            "duration_seconds": 30.5,
            "duration_bars": 8,
            "bpm": 120,
            "key": "F minor",
            "daw": "Ableton Live",
            "genre": ["Hip-Hop", "Lo-Fi"],
            "like_count": 42,
            "play_count": 100,
            "comment_count": 5,
            "is_liked": true,
            "status": "ready"
        })";

        auto json = juce::JSON::parse(jsonStr);
        auto post = FeedPost::fromJson(json);

        REQUIRE(post.id == "act-123");
        REQUIRE(post.foreignId == "loop:uuid-456");
        REQUIRE(post.actor == "user:789");
        REQUIRE(post.userId == "789");
        REQUIRE(post.verb == "posted");
        REQUIRE(post.object == "loop:uuid-456");
        REQUIRE(post.audioUrl == "https://cdn.example.com/audio.mp3");
        REQUIRE(post.waveformSvg == "<svg>...</svg>");
        REQUIRE(post.durationSeconds == Approx(30.5f));
        REQUIRE(post.durationBars == 8);
        REQUIRE(post.bpm == 120);
        REQUIRE(post.key == "F minor");
        REQUIRE(post.daw == "Ableton Live");
        REQUIRE(post.genres.size() == 2);
        REQUIRE(post.genres[0] == "Hip-Hop");
        REQUIRE(post.genres[1] == "Lo-Fi");
        REQUIRE(post.likeCount == 42);
        REQUIRE(post.playCount == 100);
        REQUIRE(post.commentCount == 5);
        REQUIRE(post.isLiked == true);
        REQUIRE(post.status == FeedPost::Status::Ready);
        REQUIRE(post.isValid() == true);
    }

    SECTION("Parse with nested actor_data")
    {
        juce::String jsonStr = R"({
            "id": "act-123",
            "actor": "user:789",
            "audio_url": "https://cdn.example.com/audio.mp3",
            "actor_data": {
                "username": "producer_one",
                "avatar_url": "https://cdn.example.com/avatar.jpg"
            }
        })";

        auto json = juce::JSON::parse(jsonStr);
        auto post = FeedPost::fromJson(json);

        REQUIRE(post.username == "producer_one");
        REQUIRE(post.userAvatarUrl == "https://cdn.example.com/avatar.jpg");
    }

    SECTION("Parse with nested user object")
    {
        juce::String jsonStr = R"({
            "id": "act-123",
            "actor": "user:789",
            "audio_url": "https://cdn.example.com/audio.mp3",
            "user": {
                "username": "beat_maker",
                "avatar_url": "https://cdn.example.com/avatar2.jpg"
            }
        })";

        auto json = juce::JSON::parse(jsonStr);
        auto post = FeedPost::fromJson(json);

        REQUIRE(post.username == "beat_maker");
        REQUIRE(post.userAvatarUrl == "https://cdn.example.com/avatar2.jpg");
    }

    SECTION("Parse single genre string")
    {
        juce::String jsonStr = R"({
            "id": "act-123",
            "audio_url": "https://cdn.example.com/audio.mp3",
            "genre": "Electronic"
        })";

        auto json = juce::JSON::parse(jsonStr);
        auto post = FeedPost::fromJson(json);

        REQUIRE(post.genres.size() == 1);
        REQUIRE(post.genres[0] == "Electronic");
    }

    SECTION("Parse different status values")
    {
        auto testStatus = [](const juce::String& statusStr, FeedPost::Status expected)
        {
            juce::String jsonStr = R"({"id": "act-123", "audio_url": "test.mp3", "status": ")" + statusStr + R"("})";
            auto json = juce::JSON::parse(jsonStr);
            auto post = FeedPost::fromJson(json);
            return post.status == expected;
        };

        REQUIRE(testStatus("ready", FeedPost::Status::Ready));
        REQUIRE(testStatus("READY", FeedPost::Status::Ready));
        REQUIRE(testStatus("Ready", FeedPost::Status::Ready));
        REQUIRE(testStatus("processing", FeedPost::Status::Processing));
        REQUIRE(testStatus("failed", FeedPost::Status::Failed));
        REQUIRE(testStatus("unknown", FeedPost::Status::Unknown));
        REQUIRE(testStatus("garbage", FeedPost::Status::Unknown));
    }
}

//==============================================================================
TEST_CASE("FeedPost::toJson serialization", "[FeedPost]")
{
    FeedPost post;
    post.id = "test-id";
    post.foreignId = "loop:test-uuid";
    post.actor = "user:123";
    post.verb = "posted";
    post.object = "loop:test-uuid";
    post.userId = "123";
    post.username = "test_user";
    post.userAvatarUrl = "https://example.com/avatar.jpg";
    post.audioUrl = "https://cdn.example.com/audio.mp3";
    post.waveformSvg = "<svg></svg>";
    post.durationSeconds = 45.0f;
    post.durationBars = 16;
    post.bpm = 140;
    post.key = "A minor";
    post.daw = "FL Studio";
    post.genres.add("Trap");
    post.genres.add("Bass");
    post.likeCount = 10;
    post.playCount = 50;
    post.commentCount = 3;
    post.isLiked = false;
    post.status = FeedPost::Status::Ready;
    post.timestamp = juce::Time::getCurrentTime();

    auto json = post.toJson();

    SECTION("Core identifiers serialized")
    {
        REQUIRE(json.getProperty("id", "").toString() == "test-id");
        REQUIRE(json.getProperty("foreign_id", "").toString() == "loop:test-uuid");
        REQUIRE(json.getProperty("actor", "").toString() == "user:123");
        REQUIRE(json.getProperty("verb", "").toString() == "posted");
        REQUIRE(json.getProperty("object", "").toString() == "loop:test-uuid");
    }

    SECTION("User data serialized in nested object")
    {
        auto userObj = json.getProperty("user", juce::var());
        REQUIRE(userObj.getProperty("id", "").toString() == "123");
        REQUIRE(userObj.getProperty("username", "").toString() == "test_user");
        REQUIRE(userObj.getProperty("avatar_url", "").toString() == "https://example.com/avatar.jpg");
    }

    SECTION("Audio metadata serialized")
    {
        REQUIRE(json.getProperty("audio_url", "").toString() == "https://cdn.example.com/audio.mp3");
        REQUIRE(json.getProperty("waveform", "").toString() == "<svg></svg>");
        REQUIRE(static_cast<float>(json.getProperty("duration_seconds", 0.0)) == Approx(45.0f));
        REQUIRE(static_cast<int>(json.getProperty("duration_bars", 0)) == 16);
        REQUIRE(static_cast<int>(json.getProperty("bpm", 0)) == 140);
        REQUIRE(json.getProperty("key", "").toString() == "A minor");
        REQUIRE(json.getProperty("daw", "").toString() == "FL Studio");
    }

    SECTION("Genres serialized as array")
    {
        auto genreVar = json.getProperty("genre", juce::var());
        REQUIRE(genreVar.isArray());
        REQUIRE(genreVar.size() == 2);
        REQUIRE(genreVar[0].toString() == "Trap");
        REQUIRE(genreVar[1].toString() == "Bass");
    }

    SECTION("Social metrics serialized")
    {
        REQUIRE(static_cast<int>(json.getProperty("like_count", 0)) == 10);
        REQUIRE(static_cast<int>(json.getProperty("play_count", 0)) == 50);
        REQUIRE(static_cast<int>(json.getProperty("comment_count", 0)) == 3);
        REQUIRE(static_cast<bool>(json.getProperty("is_liked", true)) == false);
    }

    SECTION("Status serialized as string")
    {
        REQUIRE(json.getProperty("status", "").toString() == "ready");
    }
}

//==============================================================================
TEST_CASE("FeedPost JSON round-trip", "[FeedPost]")
{
    FeedPost original;
    original.id = "round-trip-id";
    original.foreignId = "loop:round-trip";
    original.actor = "user:456";
    original.verb = "posted";
    original.userId = "456";
    original.username = "round_trip_user";
    original.audioUrl = "https://cdn.example.com/round-trip.mp3";
    original.durationSeconds = 60.0f;
    original.bpm = 128;
    original.key = "C major";
    original.genres.add("House");
    original.likeCount = 25;
    original.status = FeedPost::Status::Ready;
    original.timestamp = juce::Time::getCurrentTime();

    // Serialize and parse back
    auto json = original.toJson();
    auto jsonStr = juce::JSON::toString(json);
    auto parsedJson = juce::JSON::parse(jsonStr);
    auto restored = FeedPost::fromJson(parsedJson);

    REQUIRE(restored.id == original.id);
    REQUIRE(restored.foreignId == original.foreignId);
    REQUIRE(restored.actor == original.actor);
    REQUIRE(restored.audioUrl == original.audioUrl);
    REQUIRE(restored.durationSeconds == Approx(original.durationSeconds));
    REQUIRE(restored.bpm == original.bpm);
    REQUIRE(restored.key == original.key);
    REQUIRE(restored.genres.size() == original.genres.size());
    REQUIRE(restored.genres[0] == original.genres[0]);
    REQUIRE(restored.likeCount == original.likeCount);
    REQUIRE(restored.status == original.status);
}

//==============================================================================
TEST_CASE("FeedPost::isValid", "[FeedPost]")
{
    SECTION("Valid post has id and audio_url")
    {
        FeedPost post;
        post.id = "test-id";
        post.audioUrl = "https://example.com/audio.mp3";
        REQUIRE(post.isValid() == true);
    }

    SECTION("Invalid without id")
    {
        FeedPost post;
        post.audioUrl = "https://example.com/audio.mp3";
        REQUIRE(post.isValid() == false);
    }

    SECTION("Invalid without audio_url")
    {
        FeedPost post;
        post.id = "test-id";
        REQUIRE(post.isValid() == false);
    }
}

//==============================================================================
// FeedResponse Tests
//==============================================================================

TEST_CASE("FeedResponse default values", "[FeedResponse]")
{
    FeedResponse response;

    REQUIRE(response.posts.isEmpty());
    REQUIRE(response.limit == 20);
    REQUIRE(response.offset == 0);
    REQUIRE(response.total == 0);
    REQUIRE(response.hasMore == false);
    REQUIRE(response.error.isEmpty());
}

//==============================================================================
// FeedDataManager Tests
//==============================================================================

TEST_CASE("FeedDataManager basic construction", "[FeedDataManager]")
{
    FeedDataManager manager;

    SECTION("Default state")
    {
        REQUIRE(manager.isFetching() == false);
        REQUIRE(manager.getCurrentFeedType() == FeedDataManager::FeedType::Timeline);
        REQUIRE(manager.getCurrentOffset() == 0);
        REQUIRE(manager.getCurrentLimit() == 20);
        REQUIRE(manager.hasMorePosts() == true);
        REQUIRE(manager.getLoadedPostsCount() == 0);
        REQUIRE(manager.getCacheTTL() == 300);
    }

    SECTION("Configure cache TTL")
    {
        manager.setCacheTTL(600);
        REQUIRE(manager.getCacheTTL() == 600);
    }

    SECTION("Set feed type")
    {
        manager.setCurrentFeedType(FeedDataManager::FeedType::Global);
        REQUIRE(manager.getCurrentFeedType() == FeedDataManager::FeedType::Global);
    }
}

//==============================================================================
TEST_CASE("FeedDataManager cache validity", "[FeedDataManager]")
{
    FeedDataManager manager;

    SECTION("Cache initially invalid")
    {
        REQUIRE(manager.isCacheValid(FeedDataManager::FeedType::Timeline) == false);
        REQUIRE(manager.isCacheValid(FeedDataManager::FeedType::Global) == false);
    }

    SECTION("Cached feed returns empty when invalid")
    {
        auto cached = manager.getCachedFeed(FeedDataManager::FeedType::Timeline);
        REQUIRE(cached.posts.isEmpty());
    }
}

//==============================================================================
TEST_CASE("FeedDataManager cache clearing", "[FeedDataManager]")
{
    FeedDataManager manager;

    SECTION("Clear all cache")
    {
        manager.clearCache();
        REQUIRE(manager.isCacheValid(FeedDataManager::FeedType::Timeline) == false);
        REQUIRE(manager.isCacheValid(FeedDataManager::FeedType::Global) == false);
    }

    SECTION("Clear specific feed cache")
    {
        manager.clearCache(FeedDataManager::FeedType::Timeline);
        REQUIRE(manager.isCacheValid(FeedDataManager::FeedType::Timeline) == false);
    }
}

//==============================================================================
TEST_CASE("FeedDataManager network client configuration", "[FeedDataManager]")
{
    FeedDataManager manager;

    SECTION("No network client by default")
    {
        REQUIRE(manager.getNetworkClient() == nullptr);
    }

    SECTION("Set network client")
    {
        NetworkClient client;
        manager.setNetworkClient(&client);
        REQUIRE(manager.getNetworkClient() == &client);
    }
}

//==============================================================================
TEST_CASE("FeedDataManager fetch without network client", "[FeedDataManager]")
{
    FeedDataManager manager;
    bool callbackCalled = false;
    FeedResponse receivedResponse;

    // Try to fetch without network client
    manager.fetchFeed(FeedDataManager::FeedType::Timeline, [&](const FeedResponse& response)
    {
        callbackCalled = true;
        receivedResponse = response;
    });

    // Process pending messages
    juce::MessageManager::getInstance()->runDispatchLoopUntil(100);

    REQUIRE(callbackCalled == true);
    REQUIRE(receivedResponse.error.isNotEmpty());
    REQUIRE(receivedResponse.error.containsIgnoreCase("network client"));
}

//==============================================================================
TEST_CASE("FeedDataManager loadMorePosts without initial fetch", "[FeedDataManager]")
{
    FeedDataManager manager;
    bool callbackCalled = false;

    manager.loadMorePosts([&](const FeedResponse& response)
    {
        callbackCalled = true;
        // Should return empty response since no network client
    });

    juce::MessageManager::getInstance()->runDispatchLoopUntil(100);

    // Without network client, should still call callback
    REQUIRE(callbackCalled == true);
}

//==============================================================================
TEST_CASE("FeedDataManager refresh clears cache", "[FeedDataManager]")
{
    FeedDataManager manager;
    bool callbackCalled = false;

    manager.refreshFeed([&](bool success, const juce::String& error)
    {
        callbackCalled = true;
        // Should fail because no network client
        REQUIRE(success == false);
        REQUIRE(error.containsIgnoreCase("network client"));
    });

    juce::MessageManager::getInstance()->runDispatchLoopUntil(100);

    REQUIRE(callbackCalled == true);
    REQUIRE(manager.getLoadedPostsCount() == 0);
}
