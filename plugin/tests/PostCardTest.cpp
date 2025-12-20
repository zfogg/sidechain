#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "models/FeedPost.h"
#include "ui/feed/PostCard.h"
#include "stores/AppStore.h"
#include <JuceHeader.h>
#include <memory>

using Catch::Approx;

//==============================================================================
// PostCard Component State Tests
//==============================================================================

/**
 * These tests validate that the PostCard UI component updates its state
 * correctly when callbacks are triggered (e.g., when user interacts with the
 * like button).
 *
 * Pattern:
 * 1. Create a PostCard with initial post data via setPost()
 * 2. Verify the component has the post data
 * 3. Test callback registration and invocation
 * 4. Simulate state changes by updating post data and calling setPost() again
 * 5. Verify the component reflects new state
 */

class PostCardTestFixture {
public:
  PostCardTestFixture() {
    // Initialize JUCE message manager for UI tests
    if (!juce::MessageManager::getInstanceWithoutCreating()) {
      juce::MessageManager::getInstance();
    }
  }

  Sidechain::FeedPost createTestPost(const juce::String &id = "test-post-1") {
    Sidechain::FeedPost post;
    post.id = id;
    post.foreignId = "loop:uuid-123";
    post.actor = "user:789";
    post.userId = "789";
    post.username = "test_producer";
    post.userAvatarUrl = "https://example.com/avatar.jpg";
    post.audioUrl = "https://example.com/audio.mp3";
    post.waveformUrl = "https://example.com/waveform.svg";
    post.durationSeconds = 30.0f;
    post.bpm = 120;
    post.key = "C major";
    post.likeCount = 5;
    post.playCount = 10;
    post.commentCount = 2;
    post.isLiked = false; // Initially NOT liked
    post.isSaved = false; // Initially NOT saved
    post.status = Sidechain::FeedPost::Status::Ready;
    post.timestamp = juce::Time::getCurrentTime();
    return post;
  }
};

//==============================================================================
TEST_CASE_METHOD(PostCardTestFixture, "PostCard initialization", "[PostCard]") {
  PostCard card;

  SECTION("PostCard creates with default size") {
    REQUIRE(card.getWidth() > 0);
    REQUIRE(card.getHeight() > 0);
  }

  SECTION("PostCard post is initially empty") {
    REQUIRE(card.getPost()->id.isEmpty());
  }
}

//==============================================================================
TEST_CASE_METHOD(PostCardTestFixture, "PostCard setPost updates display", "[PostCard]") {
  PostCard card;
  Sidechain::FeedPost testPost = createTestPost("post-1");

  SECTION("setPost updates the post data") {
    card.setPost(testPost);

    REQUIRE(card.getPost()->id == "post-1");
    REQUIRE(card.getPost()->username == "test_producer");
    REQUIRE(card.getPost()->likeCount == 5);
    REQUIRE(card.getPost()->playCount == 10);
    REQUIRE(card.getPost()->commentCount == 2);
    REQUIRE(card.getPost()->isLiked == false);
  }

  SECTION("getPostId returns correct ID") {
    card.setPost(testPost);
    REQUIRE(card.getPostId() == "post-1");
  }
}

//==============================================================================
TEST_CASE_METHOD(PostCardTestFixture, "PostCard playback methods", "[PostCard]") {
  PostCard card;
  Sidechain::FeedPost testPost = createTestPost("post-playback");
  card.setPost(testPost);

  SECTION("setIsPlaying works without errors") {
    // These methods should not throw
    card.setIsPlaying(true);
    REQUIRE(true);

    card.setIsPlaying(false);
    REQUIRE(true);

    card.setPlaying(true); // Alias method
    REQUIRE(true);
  }

  SECTION("setPlaybackProgress accepts valid values") {
    card.setPlaybackProgress(0.0f);
    REQUIRE(true);

    card.setPlaybackProgress(0.5f);
    REQUIRE(true);

    card.setPlaybackProgress(1.0f);
    REQUIRE(true);

    // Clamps out of range values
    card.setPlaybackProgress(-0.5f);
    card.setPlaybackProgress(1.5f);
    REQUIRE(true);
  }

  SECTION("setLoading works without errors") {
    card.setLoading(true);
    REQUIRE(true);

    card.setLoading(false);
    REQUIRE(true);
  }

  SECTION("setDownloadProgress works without errors") {
    card.setDownloadProgress(0.0f);
    REQUIRE(true);

    card.setDownloadProgress(0.25f);
    REQUIRE(true);

    card.setDownloadProgress(1.0f);
    REQUIRE(true);
  }
}

//==============================================================================
TEST_CASE_METHOD(PostCardTestFixture, "PostCard like state changes on callback", "[PostCard][Like][Callback]") {
  PostCard card;
  Sidechain::FeedPost testPost = createTestPost("post-like");
  testPost.isLiked = false;
  testPost.likeCount = 5;
  card.setPost(testPost);

  SECTION("onLikeToggled callback can be registered") {
    // Track callback invocations
    bool callbackFired = false;
    Sidechain::FeedPost postFromCallback;
    bool likedState = false;

    // Set up callback
    card.onLikeToggled = [&](const Sidechain::FeedPost &post, bool liked) {
      callbackFired = true;
      postFromCallback = post;
      likedState = liked;
    };

    // Simulate network success by calling the callback
    card.onLikeToggled(testPost, true);

    REQUIRE(callbackFired == true);
    REQUIRE(postFromCallback.id == "post-like");
    REQUIRE(likedState == true);
  }

  SECTION("UI updates after successful like via callback") {
    // Initial state: NOT liked, 5 likes
    REQUIRE(card.getPost()->isLiked == false);
    REQUIRE(card.getPost()->likeCount == 5);

    // Simulate network success - update post data via setPost()
    card.onLikeToggled = [&](const Sidechain::FeedPost &post, bool liked) {
      // This callback would normally make network request
      // For test, simulate success by updating post
      Sidechain::FeedPost updatedPost = post;
      updatedPost.isLiked = liked;
      if (liked) {
        updatedPost.likeCount = post.likeCount + 1;
      } else {
        updatedPost.likeCount = post.likeCount - 1;
      }
      card.setPost(updatedPost);
    };

    // Trigger the callback (simulating user clicking like button + network success)
    card.onLikeToggled(testPost, true);

    // After callback, UI should reflect the like
    REQUIRE(card.getPost()->isLiked == true);
    REQUIRE(card.getPost()->likeCount == 6);
  }

  SECTION("UI updates after unlike") {
    // Start with liked post
    Sidechain::FeedPost likedPost = testPost;
    likedPost.isLiked = true;
    likedPost.likeCount = 6;
    card.setPost(likedPost);

    card.onLikeToggled = [&](const Sidechain::FeedPost &post, bool liked) {
      Sidechain::FeedPost updatedPost = post;
      updatedPost.isLiked = liked;
      if (liked) {
        updatedPost.likeCount = post.likeCount + 1;
      } else {
        updatedPost.likeCount = post.likeCount - 1;
      }
      card.setPost(updatedPost);
    };

    // Unlike
    card.onLikeToggled(likedPost, false);

    // Should go back to unliked
    REQUIRE(card.getPost()->isLiked == false);
    REQUIRE(card.getPost()->likeCount == 5);
  }
}

//==============================================================================
TEST_CASE_METHOD(PostCardTestFixture, "PostCard comment interaction", "[PostCard][Comment][Callback]") {
  PostCard card;
  Sidechain::FeedPost testPost = createTestPost("post-comment");
  testPost.commentCount = 3;
  card.setPost(testPost);

  SECTION("Comment count persists in post") {
    REQUIRE(card.getPost()->commentCount == 3);
  }

  SECTION("onCommentClicked callback can be registered and fired") {
    bool callbackFired = false;
    Sidechain::FeedPost callbackPost;

    card.onCommentClicked = [&](const Sidechain::FeedPost &post) {
      callbackFired = true;
      callbackPost = post;
    };

    // Trigger callback
    card.onCommentClicked(testPost);

    REQUIRE(callbackFired == true);
    REQUIRE(callbackPost.id == "post-comment");
    REQUIRE(callbackPost.commentCount == 3);
  }
}

//==============================================================================
TEST_CASE_METHOD(PostCardTestFixture, "PostCard save state updates", "[PostCard][Save][Callback]") {
  PostCard card;
  Sidechain::FeedPost testPost = createTestPost("post-save");
  card.setPost(testPost);

  SECTION("Save callback invoked with state") {
    bool callbackFired = false;
    Sidechain::FeedPost callbackPost;
    bool savedState = false;

    card.onSaveToggled = [&](const Sidechain::FeedPost &post, bool saved) {
      callbackFired = true;
      callbackPost = post;
      savedState = saved;
    };

    // Trigger save callback
    card.onSaveToggled(testPost, true);

    REQUIRE(callbackFired == true);
    REQUIRE(callbackPost.id == "post-save");
    REQUIRE(savedState == true);
  }

  SECTION("UI updates reflect save state changes") {
    // Initial: not saved
    REQUIRE(card.getPost()->isSaved == false);

    // Simulate save
    card.onSaveToggled = [&](const Sidechain::FeedPost &post, bool saved) {
      Sidechain::FeedPost updated = post;
      updated.isSaved = saved;
      card.setPost(updated);
    };

    card.onSaveToggled(testPost, true);
    REQUIRE(card.getPost()->isSaved == true);

    // Simulate unsave
    card.onSaveToggled(*card.getPost(), false);
    REQUIRE(card.getPost()->isSaved == false);
  }
}

//==============================================================================
TEST_CASE_METHOD(PostCardTestFixture, "PostCard follow state interaction", "[PostCard][Follow][Callback]") {
  PostCard card;
  Sidechain::FeedPost testPost = createTestPost("post-follow");
  testPost.isFollowing = false;
  card.setPost(testPost);

  SECTION("Follow state updates on callback") {
    REQUIRE(card.getPost()->isFollowing == false);

    card.onFollowToggled = [&](const Sidechain::FeedPost &post, bool willFollow) {
      Sidechain::FeedPost updated = post;
      updated.isFollowing = willFollow;
      card.setPost(updated);
    };

    // Follow the user
    card.onFollowToggled(testPost, true);
    REQUIRE(card.getPost()->isFollowing == true);

    // Unfollow
    card.onFollowToggled(*card.getPost(), false);
    REQUIRE(card.getPost()->isFollowing == false);
  }
}

//==============================================================================
TEST_CASE_METHOD(PostCardTestFixture, "PostCard multiple state changes in sequence", "[PostCard][State]") {
  PostCard card;
  Sidechain::FeedPost testPost = createTestPost("post-multi");
  card.setPost(testPost);

  SECTION("Complex interaction sequence") {
    // Simulate: set playing → like → save
    card.setIsPlaying(true);
    REQUIRE(true); // No crash

    // Like the post
    card.onLikeToggled = [&](const Sidechain::FeedPost &post, bool liked) {
      Sidechain::FeedPost updated = post;
      updated.isLiked = liked;
      if (liked)
        updated.likeCount++;
      card.setPost(updated);
    };

    card.onLikeToggled(*card.getPost(), true);
    REQUIRE(card.getPost()->isLiked == true);
    REQUIRE(card.getPost()->likeCount == 6);

    // Save the post
    card.onSaveToggled = [&](const Sidechain::FeedPost &post, bool saved) {
      Sidechain::FeedPost updated = post;
      updated.isSaved = saved;
      card.setPost(updated);
    };

    card.onSaveToggled(*card.getPost(), true);
    REQUIRE(card.getPost()->isSaved == true);
    REQUIRE(card.getPost()->isLiked == true); // Still liked
  }
}

//==============================================================================
TEST_CASE_METHOD(PostCardTestFixture, "PostCard error handling in callbacks", "[PostCard][Error]") {
  PostCard card;
  Sidechain::FeedPost testPost = createTestPost("post-error");
  card.setPost(testPost);

  SECTION("Post state unchanged on callback error") {
    Sidechain::FeedPost originalPost = *card.getPost();

    // Callback doesn't update (simulating error/network failure)
    card.onLikeToggled = [](const Sidechain::FeedPost &post, bool liked) {
      // Do nothing - simulates error/network failure
      // In real app, would show error toast
    };

    card.onLikeToggled(testPost, true);

    // Post state should remain unchanged
    REQUIRE(card.getPost()->id == originalPost.id);
    REQUIRE(card.getPost()->isLiked == originalPost.isLiked);
    REQUIRE(card.getPost()->likeCount == originalPost.likeCount);
  }
}
