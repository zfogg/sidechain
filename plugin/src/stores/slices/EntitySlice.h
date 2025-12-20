#pragma once

#include "../EntityStore.h"
#include "../AppStore.h"
#include "../../util/logging/Logger.h"
#include <JuceHeader.h>
#include <functional>
#include <memory>
#include <vector>

namespace Sidechain {
namespace Stores {
namespace Slices {

/**
 * EntitySlice - Bridge between EntityStore and Redux state system
 *
 * Ensures all entity updates flow through slice subscriptions, maintaining
 * a single source of truth and reactive update propagation.
 *
 * Key Responsibilities:
 * 1. Normalize and cache entities from network responses
 * 2. Notify all slice subscribers when entities change
 * 3. Handle entity expiration and invalidation
 * 4. Provide cache statistics and debugging info
 *
 * When an entity is updated in EntityStore:
 * - All existing subscribers are notified immediately
 * - Same shared_ptr instance ensures memory deduplication
 * - Slice subscribers receive consistent state
 *
 * Usage:
 *   auto& entitySlice = EntitySlice::getInstance();
 *
 *   // Cache a post (from network response)
 *   entitySlice.cachePost(post);
 *   // → All subscribers notified, EntityStore updated
 *
 *   // Subscribe to entity changes
 *   auto unsub = entitySlice.subscribeToPost(postId, [](const auto& post) {
 *       updateUI(post);
 *   });
 *
 *   // Batch cache operations
 *   entitySlice.cachePosts(std::vector<...> posts);
 */
class EntitySlice {
public:
  // ==============================================================================
  // Singleton access

  static EntitySlice &getInstance() {
    static EntitySlice instance;
    return instance;
  }

  // Deleted copy/move constructors
  EntitySlice(const EntitySlice &) = delete;
  EntitySlice &operator=(const EntitySlice &) = delete;
  EntitySlice(EntitySlice &&) = delete;
  EntitySlice &operator=(EntitySlice &&) = delete;

  // ==============================================================================
  // Initialize with dependencies

  /**
   * Initialize EntitySlice with AppStore for state notifications
   * Must be called once before using entity caching
   */
  void initialize(AppStore *appStore) {
    appStore_ = appStore;
    Util::logInfo("EntitySlice", "Initialized with AppStore");
  }

  // ==============================================================================
  // FeedPost caching

  /**
   * Cache a single post
   * Updates EntityStore and notifies all subscribers
   */
  void cachePost(const std::shared_ptr<FeedPost> &post) {
    if (!post || post->id.isEmpty()) {
      Util::logWarning("EntitySlice", "Cannot cache post - invalid post or empty ID");
      return;
    }

    auto &entityStore = EntityStore::getInstance();
    entityStore.posts().set(post->id, post);
    Util::logDebug("EntitySlice", "Cached post: " + post->id);
  }

  /**
   * Cache multiple posts at once
   */
  void cachePosts(const std::vector<std::shared_ptr<FeedPost>> &posts) {
    auto &entityStore = EntityStore::getInstance();

    std::vector<std::pair<juce::String, std::shared_ptr<FeedPost>>> entries;
    entries.reserve(posts.size());

    for (const auto &post : posts) {
      if (post && !post->id.isEmpty()) {
        entries.emplace_back(post->id, post);
      }
    }

    if (!entries.empty()) {
      entityStore.posts().setMany(entries);
      Util::logDebug("EntitySlice", "Cached " + juce::String(entries.size()) + " posts");
    }
  }

  /**
   * Get cached post
   */
  std::shared_ptr<FeedPost> getPost(const juce::String &postId) const {
    auto &entityStore = EntityStore::getInstance();
    return entityStore.posts().get(postId);
  }

  /**
   * Subscribe to post updates
   * Returns unsubscriber function
   */
  using PostObserver = std::function<void(const std::shared_ptr<FeedPost> &)>;
  std::function<void()> subscribeToPost(const juce::String &postId, PostObserver observer) {
    auto &entityStore = EntityStore::getInstance();
    return entityStore.posts().subscribe(postId, std::move(observer));
  }

  // ==============================================================================
  // User caching

  /**
   * Cache a single user
   */
  void cacheUser(const std::shared_ptr<User> &user) {
    if (!user || user->id.isEmpty()) {
      Util::logWarning("EntitySlice", "Cannot cache user - invalid user or empty ID");
      return;
    }

    auto &entityStore = EntityStore::getInstance();
    entityStore.users().set(user->id, user);
    Util::logDebug("EntitySlice", "Cached user: " + user->id);
  }

  /**
   * Cache multiple users at once
   */
  void cacheUsers(const std::vector<std::shared_ptr<User>> &users) {
    auto &entityStore = EntityStore::getInstance();

    std::vector<std::pair<juce::String, std::shared_ptr<User>>> entries;
    entries.reserve(users.size());

    for (const auto &user : users) {
      if (user && !user->id.isEmpty()) {
        entries.emplace_back(user->id, user);
      }
    }

    if (!entries.empty()) {
      entityStore.users().setMany(entries);
      Util::logDebug("EntitySlice", "Cached " + juce::String(entries.size()) + " users");
    }
  }

  /**
   * Get cached user
   */
  std::shared_ptr<User> getUser(const juce::String &userId) const {
    auto &entityStore = EntityStore::getInstance();
    return entityStore.users().get(userId);
  }

  /**
   * Subscribe to user updates
   */
  using UserObserver = std::function<void(const std::shared_ptr<User> &)>;
  std::function<void()> subscribeToUser(const juce::String &userId, UserObserver observer) {
    auto &entityStore = EntityStore::getInstance();
    return entityStore.users().subscribe(userId, std::move(observer));
  }

  // ==============================================================================
  // Message caching

  /**
   * Cache a single message
   */
  void cacheMessage(const std::shared_ptr<Message> &message) {
    if (!message || message->id.isEmpty()) {
      Util::logWarning("EntitySlice", "Cannot cache message - invalid message or empty ID");
      return;
    }

    auto &entityStore = EntityStore::getInstance();
    entityStore.messages().set(message->id, message);
    Util::logDebug("EntitySlice", "Cached message: " + message->id);
  }

  /**
   * Cache multiple messages at once
   */
  void cacheMessages(const std::vector<std::shared_ptr<Message>> &messages) {
    auto &entityStore = EntityStore::getInstance();

    std::vector<std::pair<juce::String, std::shared_ptr<Message>>> entries;
    entries.reserve(messages.size());

    for (const auto &msg : messages) {
      if (msg && !msg->id.isEmpty()) {
        entries.emplace_back(msg->id, msg);
      }
    }

    if (!entries.empty()) {
      entityStore.messages().setMany(entries);
      Util::logDebug("EntitySlice", "Cached " + juce::String(entries.size()) + " messages");
    }
  }

  /**
   * Get cached message
   */
  std::shared_ptr<Message> getMessage(const juce::String &messageId) const {
    auto &entityStore = EntityStore::getInstance();
    return entityStore.messages().get(messageId);
  }

  /**
   * Subscribe to message updates
   */
  using MessageObserver = std::function<void(const std::shared_ptr<Message> &)>;
  std::function<void()> subscribeToMessage(const juce::String &messageId, MessageObserver observer) {
    auto &entityStore = EntityStore::getInstance();
    return entityStore.messages().subscribe(messageId, std::move(observer));
  }

  // ==============================================================================
  // Story caching

  /**
   * Cache a single story
   */
  void cacheStory(const std::shared_ptr<Story> &story) {
    if (!story || story->id.isEmpty()) {
      Util::logWarning("EntitySlice", "Cannot cache story - invalid story or empty ID");
      return;
    }

    auto &entityStore = EntityStore::getInstance();
    entityStore.stories().set(story->id, story);
    Util::logDebug("EntitySlice", "Cached story: " + story->id);
  }

  /**
   * Cache multiple stories at once
   */
  void cacheStories(const std::vector<std::shared_ptr<Story>> &stories) {
    auto &entityStore = EntityStore::getInstance();

    std::vector<std::pair<juce::String, std::shared_ptr<Story>>> entries;
    entries.reserve(stories.size());

    for (const auto &story : stories) {
      if (story && !story->id.isEmpty()) {
        entries.emplace_back(story->id, story);
      }
    }

    if (!entries.empty()) {
      entityStore.stories().setMany(entries);
      Util::logDebug("EntitySlice", "Cached " + juce::String(entries.size()) + " stories");
    }
  }

  /**
   * Get cached story
   */
  std::shared_ptr<Story> getStory(const juce::String &storyId) const {
    auto &entityStore = EntityStore::getInstance();
    return entityStore.stories().get(storyId);
  }

  /**
   * Subscribe to story updates
   */
  using StoryObserver = std::function<void(const std::shared_ptr<Story> &)>;
  std::function<void()> subscribeToStory(const juce::String &storyId, StoryObserver observer) {
    auto &entityStore = EntityStore::getInstance();
    return entityStore.stories().subscribe(storyId, std::move(observer));
  }

  // ==============================================================================
  // Conversation caching

  /**
   * Cache a single conversation
   */
  void cacheConversation(const std::shared_ptr<Conversation> &conversation) {
    if (!conversation || conversation->id.isEmpty()) {
      Util::logWarning("EntitySlice", "Cannot cache conversation - invalid or empty ID");
      return;
    }

    auto &entityStore = EntityStore::getInstance();
    entityStore.conversations().set(conversation->id, conversation);
    Util::logDebug("EntitySlice", "Cached conversation: " + conversation->id);
  }

  /**
   * Cache multiple conversations at once
   */
  void cacheConversations(const std::vector<std::shared_ptr<Conversation>> &conversations) {
    auto &entityStore = EntityStore::getInstance();

    std::vector<std::pair<juce::String, std::shared_ptr<Conversation>>> entries;
    entries.reserve(conversations.size());

    for (const auto &conv : conversations) {
      if (conv && !conv->id.isEmpty()) {
        entries.emplace_back(conv->id, conv);
      }
    }

    if (!entries.empty()) {
      entityStore.conversations().setMany(entries);
      Util::logDebug("EntitySlice", "Cached " + juce::String(entries.size()) + " conversations");
    }
  }

  /**
   * Get cached conversation
   */
  std::shared_ptr<Conversation> getConversation(const juce::String &conversationId) const {
    auto &entityStore = EntityStore::getInstance();
    return entityStore.conversations().get(conversationId);
  }

  /**
   * Subscribe to conversation updates
   */
  using ConversationObserver = std::function<void(const std::shared_ptr<Conversation> &)>;
  std::function<void()> subscribeToConversation(const juce::String &conversationId, ConversationObserver observer) {
    auto &entityStore = EntityStore::getInstance();
    return entityStore.conversations().subscribe(conversationId, std::move(observer));
  }

  // ==============================================================================
  // Playlist caching

  /**
   * Cache a single playlist
   */
  void cachePlaylist(const std::shared_ptr<Playlist> &playlist) {
    if (!playlist || playlist->id.isEmpty()) {
      Util::logWarning("EntitySlice", "Cannot cache playlist - invalid or empty ID");
      return;
    }

    auto &entityStore = EntityStore::getInstance();
    entityStore.playlists().set(playlist->id, playlist);
    Util::logDebug("EntitySlice", "Cached playlist: " + playlist->id);
  }

  /**
   * Cache multiple playlists at once
   */
  void cachePlaylists(const std::vector<std::shared_ptr<Playlist>> &playlists) {
    auto &entityStore = EntityStore::getInstance();

    std::vector<std::pair<juce::String, std::shared_ptr<Playlist>>> entries;
    entries.reserve(playlists.size());

    for (const auto &playlist : playlists) {
      if (playlist && !playlist->id.isEmpty()) {
        entries.emplace_back(playlist->id, playlist);
      }
    }

    if (!entries.empty()) {
      entityStore.playlists().setMany(entries);
      Util::logDebug("EntitySlice", "Cached " + juce::String(entries.size()) + " playlists");
    }
  }

  /**
   * Get cached playlist
   */
  std::shared_ptr<Playlist> getPlaylist(const juce::String &playlistId) const {
    auto &entityStore = EntityStore::getInstance();
    return entityStore.playlists().get(playlistId);
  }

  /**
   * Subscribe to playlist updates
   */
  using PlaylistObserver = std::function<void(const std::shared_ptr<Playlist> &)>;
  std::function<void()> subscribeToPlaylist(const juce::String &playlistId, PlaylistObserver observer) {
    auto &entityStore = EntityStore::getInstance();
    return entityStore.playlists().subscribe(playlistId, std::move(observer));
  }

  // ==============================================================================
  // Notification caching

  /**
   * Cache a single notification
   */
  void cacheNotification(const std::shared_ptr<Notification> &notification) {
    if (!notification || notification->id.isEmpty()) {
      Util::logWarning("EntitySlice", "Cannot cache notification - invalid or empty ID");
      return;
    }

    auto &entityStore = EntityStore::getInstance();
    entityStore.notifications().set(notification->id, notification);
    Util::logDebug("EntitySlice", "Cached notification: " + notification->id);
  }

  /**
   * Cache multiple notifications at once
   */
  void cacheNotifications(const std::vector<std::shared_ptr<Notification>> &notifications) {
    auto &entityStore = EntityStore::getInstance();

    std::vector<std::pair<juce::String, std::shared_ptr<Notification>>> entries;
    entries.reserve(notifications.size());

    for (const auto &notif : notifications) {
      if (notif && !notif->id.isEmpty()) {
        entries.emplace_back(notif->id, notif);
      }
    }

    if (!entries.empty()) {
      entityStore.notifications().setMany(entries);
      Util::logDebug("EntitySlice", "Cached " + juce::String(entries.size()) + " notifications");
    }
  }

  /**
   * Get cached notification
   */
  std::shared_ptr<Notification> getNotification(const juce::String &notificationId) const {
    auto &entityStore = EntityStore::getInstance();
    return entityStore.notifications().get(notificationId);
  }

  /**
   * Subscribe to notification updates
   */
  using NotificationObserver = std::function<void(const std::shared_ptr<Notification> &)>;
  std::function<void()> subscribeToNotification(const juce::String &notificationId, NotificationObserver observer) {
    auto &entityStore = EntityStore::getInstance();
    return entityStore.notifications().subscribe(notificationId, std::move(observer));
  }

  // ==============================================================================
  // Challenge caching

  /**
   * Cache a single challenge
   */
  void cacheChallenge(const std::shared_ptr<MIDIChallenge> &challenge) {
    if (!challenge || challenge->id.isEmpty()) {
      Util::logWarning("EntitySlice", "Cannot cache challenge - invalid or empty ID");
      return;
    }

    auto &entityStore = EntityStore::getInstance();
    entityStore.challenges().set(challenge->id, challenge);
    Util::logDebug("EntitySlice", "Cached challenge: " + challenge->id);
  }

  /**
   * Cache multiple challenges at once
   */
  void cacheChallenges(const std::vector<std::shared_ptr<MIDIChallenge>> &challenges) {
    auto &entityStore = EntityStore::getInstance();

    std::vector<std::pair<juce::String, std::shared_ptr<MIDIChallenge>>> entries;
    entries.reserve(challenges.size());

    for (const auto &challenge : challenges) {
      if (challenge && !challenge->id.isEmpty()) {
        entries.emplace_back(challenge->id, challenge);
      }
    }

    if (!entries.empty()) {
      entityStore.challenges().setMany(entries);
      Util::logDebug("EntitySlice", "Cached " + juce::String(entries.size()) + " challenges");
    }
  }

  /**
   * Get cached challenge
   */
  std::shared_ptr<MIDIChallenge> getChallenge(const juce::String &challengeId) const {
    auto &entityStore = EntityStore::getInstance();
    return entityStore.challenges().get(challengeId);
  }

  /**
   * Subscribe to challenge updates
   */
  using ChallengeObserver = std::function<void(const std::shared_ptr<MIDIChallenge> &)>;
  std::function<void()> subscribeToChallenge(const juce::String &challengeId, ChallengeObserver observer) {
    auto &entityStore = EntityStore::getInstance();
    return entityStore.challenges().subscribe(challengeId, std::move(observer));
  }

  // ==============================================================================
  // Sound caching

  /**
   * Cache a single sound
   */
  void cacheSound(const std::shared_ptr<Sound> &sound) {
    if (!sound || sound->id.isEmpty()) {
      Util::logWarning("EntitySlice", "Cannot cache sound - invalid or empty ID");
      return;
    }

    auto &entityStore = EntityStore::getInstance();
    entityStore.sounds().set(sound->id, sound);
    Util::logDebug("EntitySlice", "Cached sound: " + sound->id);
  }

  /**
   * Cache multiple sounds at once
   */
  void cacheSounds(const std::vector<std::shared_ptr<Sound>> &sounds) {
    auto &entityStore = EntityStore::getInstance();

    std::vector<std::pair<juce::String, std::shared_ptr<Sound>>> entries;
    entries.reserve(sounds.size());

    for (const auto &sound : sounds) {
      if (sound && !sound->id.isEmpty()) {
        entries.emplace_back(sound->id, sound);
      }
    }

    if (!entries.empty()) {
      entityStore.sounds().setMany(entries);
      Util::logDebug("EntitySlice", "Cached " + juce::String(entries.size()) + " sounds");
    }
  }

  /**
   * Get cached sound
   */
  std::shared_ptr<Sound> getSound(const juce::String &soundId) const {
    auto &entityStore = EntityStore::getInstance();
    return entityStore.sounds().get(soundId);
  }

  /**
   * Subscribe to sound updates
   */
  using SoundObserver = std::function<void(const std::shared_ptr<Sound> &)>;
  std::function<void()> subscribeToSound(const juce::String &soundId, SoundObserver observer) {
    auto &entityStore = EntityStore::getInstance();
    return entityStore.sounds().subscribe(soundId, std::move(observer));
  }

  // ==============================================================================
  // Cache management

  /**
   * Invalidate a specific entity
   */
  void invalidatePost(const juce::String &postId) {
    EntityStore::getInstance().posts().invalidate(postId);
  }

  void invalidateUser(const juce::String &userId) {
    EntityStore::getInstance().users().invalidate(userId);
  }

  void invalidateStory(const juce::String &storyId) {
    EntityStore::getInstance().stories().invalidate(storyId);
  }

  void invalidateConversation(const juce::String &conversationId) {
    EntityStore::getInstance().conversations().invalidate(conversationId);
  }

  void invalidatePlaylist(const juce::String &playlistId) {
    EntityStore::getInstance().playlists().invalidate(playlistId);
  }

  void invalidateChallenge(const juce::String &challengeId) {
    EntityStore::getInstance().challenges().invalidate(challengeId);
  }

  void invalidateSound(const juce::String &soundId) {
    EntityStore::getInstance().sounds().invalidate(soundId);
  }

  /**
   * Invalidate all entities of a type
   */
  void invalidateAllPosts() {
    EntityStore::getInstance().posts().invalidateAll();
  }

  void invalidateAllUsers() {
    EntityStore::getInstance().users().invalidateAll();
  }

  void invalidateAllStories() {
    EntityStore::getInstance().stories().invalidateAll();
  }

  /**
   * Get cache statistics for debugging
   */
  struct CacheStats {
    size_t postCount = 0;
    size_t userCount = 0;
    size_t messageCount = 0;
    size_t storyCount = 0;
    size_t conversationCount = 0;
    size_t playlistCount = 0;
    size_t notificationCount = 0;
    size_t challengeCount = 0;
    size_t soundCount = 0;
  };

  CacheStats getCacheStats() const {
    CacheStats stats;
    stats.postCount = EntityStore::getInstance().posts().getStats().count;
    stats.userCount = EntityStore::getInstance().users().getStats().count;
    stats.messageCount = EntityStore::getInstance().messages().getStats().count;
    stats.storyCount = EntityStore::getInstance().stories().getStats().count;
    stats.conversationCount = EntityStore::getInstance().conversations().getStats().count;
    stats.playlistCount = EntityStore::getInstance().playlists().getStats().count;
    stats.notificationCount = EntityStore::getInstance().notifications().getStats().count;
    stats.challengeCount = EntityStore::getInstance().challenges().getStats().count;
    stats.soundCount = EntityStore::getInstance().sounds().getStats().count;
    return stats;
  }

private:
  EntitySlice() = default;

  AppStore *appStore_ = nullptr;
};

} // namespace Slices
} // namespace Stores
} // namespace Sidechain
