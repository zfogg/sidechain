#pragma once

#include "../models/Comment.h"
#include "../models/Conversation.h"
#include "../models/Draft.h"
#include "../models/FeedPost.h"
#include "../models/MidiChallenge.h"
#include "../models/Message.h"
#include "../models/Notification.h"
#include "../models/Playlist.h"
#include "../models/Sound.h"
#include "../models/Story.h"
#include "../models/User.h"
#include "EntityCache.h"
#include <JuceHeader.h>
#include <memory>
#include <nlohmann/json.hpp>

// Forward declarations
class NetworkClient;

namespace Sidechain {
namespace Stores {

// ==============================================================================
/**
 * EntityStore - Centralized entity cache with normalized storage
 *
 * Singleton that manages EntityCache instances for all entity types.
 * Provides:
 * - Normalized data storage (single source of truth per entity)
 * - Memory deduplication (same entity shared across UI contexts)
 * - Reactive updates (observers notified on changes)
 * - TTL-based expiration per entity type
 * - Optimistic updates with rollback
 * - WebSocket-driven cache invalidation
 *
 * Entity types managed:
 * - FeedPost (posts in feeds)
 * - Story (stories and highlights)
 * - User (user profiles)
 * - Notification (user notifications)
 * - Message (chat messages)
 * - Conversation (chat conversations)
 * - Playlist (music playlists)
 * - MIDIChallenge (MIDI challenges)
 * - Draft (unsaved drafts)
 * - Sound (sound samples/pages)
 *
 * Usage:
 *   auto& entityStore = EntityStore::getInstance();
 *
 *   // Set entity
 *   entityStore.posts().set(post.id, post);
 *
 *   // Get entity
 *   auto maybePost = entityStore.posts().get(postId);
 *
 *   // Subscribe to changes
 *   entityStore.posts().subscribe(postId, [](const Sidechain::FeedPost& post) {
 *       updateUI(post);
 *   });
 *
 *   // Fetch from network (with automatic caching)
 *   entityStore.fetchPost(postId).subscribe([](const Sidechain::FeedPost& post) {
 *       // Use post...
 *   });
 */
class EntityStore {
public:
  // ==============================================================================
  // Singleton access

  static EntityStore &getInstance() {
    static EntityStore instance;
    return instance;
  }

  // Deleted copy/move constructors
  EntityStore(const EntityStore &) = delete;
  EntityStore &operator=(const EntityStore &) = delete;
  EntityStore(EntityStore &&) = delete;
  EntityStore &operator=(EntityStore &&) = delete;

  // ==============================================================================
  // Cache accessors

  EntityCache<FeedPost> &posts() {
    return posts_;
  }
  const EntityCache<FeedPost> &posts() const {
    return posts_;
  }

  EntityCache<Story> &stories() {
    return stories_;
  }
  const EntityCache<Story> &stories() const {
    return stories_;
  }

  EntityCache<User> &users() {
    return users_;
  }
  const EntityCache<User> &users() const {
    return users_;
  }

  EntityCache<Notification> &notifications() {
    return notifications_;
  }
  const EntityCache<Notification> &notifications() const {
    return notifications_;
  }

  EntityCache<Comment> &comments() {
    return comments_;
  }
  const EntityCache<Comment> &comments() const {
    return comments_;
  }

  EntityCache<Message> &messages() {
    return messages_;
  }
  const EntityCache<Message> &messages() const {
    return messages_;
  }

  EntityCache<Conversation> &conversations() {
    return conversations_;
  }
  const EntityCache<Conversation> &conversations() const {
    return conversations_;
  }

  EntityCache<Playlist> &playlists() {
    return playlists_;
  }
  const EntityCache<Playlist> &playlists() const {
    return playlists_;
  }

  EntityCache<MIDIChallenge> &challenges() {
    return challenges_;
  }
  const EntityCache<MIDIChallenge> &challenges() const {
    return challenges_;
  }

  EntityCache<Draft> &drafts() {
    return drafts_;
  }
  const EntityCache<Draft> &drafts() const {
    return drafts_;
  }

  EntityCache<Sound> &sounds() {
    return sounds_;
  }
  const EntityCache<Sound> &sounds() const {
    return sounds_;
  }

  // ==============================================================================
  // Entity normalization helpers
  // These methods ensure memory deduplication - same entity ID returns same shared_ptr

  /**
   * Normalize FeedPost from JSON
   * If post with same ID exists, updates it; otherwise creates new shared_ptr
   * Returns the shared_ptr for use in state/UI
   */
  std::shared_ptr<FeedPost> normalizePost(const juce::var &json) {
    auto post = FeedPost::fromJson(json);
    if (post.id.isEmpty()) {
      return nullptr;
    }

    // Get or create shared_ptr for this post
    return posts_.getOrCreate(post.id, [post]() { return std::make_shared<FeedPost>(post); });
  }

  /**
   * Normalize User from JSON
   */
  std::shared_ptr<User> normalizeUser(const nlohmann::json &json) {
    try {
      auto user = User::fromJson(json);
      if (user.id.isEmpty()) {
        return nullptr;
      }

      // Get or create shared_ptr for this user
      return users_.getOrCreate(user.id, [user]() { return std::make_shared<User>(user); });
    } catch (const Json::ValidationError &e) {
      Util::logError("EntityStore", "Failed to normalize user", e.what());
      return nullptr;
    }
  }

  /**
   * Normalize Story from JSON
   * TODO: Implement Story::fromJson() serialization
   */
  std::shared_ptr<Story> normalizeStory([[maybe_unused]] const juce::var &json) {
    // TODO: Implement Story::fromJson() with proper nlohmann::json serialization
    // For now, skip normalization
    return nullptr;
    // auto story = Story::fromJson(json);
    // if (story.id.isEmpty()) {
    //   return nullptr;
    // }
    // return stories_.getOrCreate(story.id, [story]() { return std::make_shared<Story>(story); });
  }

  /**
   * Normalize Notification from JSON
   */
  std::shared_ptr<Notification> normalizeNotification(const nlohmann::json &json) {
    try {
      auto notification = Notification::fromJson(json);
      if (notification.id.isEmpty()) {
        return nullptr;
      }

      return notifications_.getOrCreate(notification.id,
                                        [notification]() { return std::make_shared<Notification>(notification); });
    } catch (const Json::ValidationError &e) {
      Util::logError("EntityStore", "Failed to normalize notification", e.what());
      return nullptr;
    }
  }

  /**
   * Normalize Comment from JSON
   */
  std::shared_ptr<Comment> normalizeComment(const nlohmann::json &json) {
    try {
      auto comment = Comment::fromJson(json);
      if (comment.id.isEmpty()) {
        return nullptr;
      }

      return comments_.getOrCreate(comment.id, [comment]() { return std::make_shared<Comment>(comment); });
    } catch (const Json::ValidationError &e) {
      Util::logError("EntityStore", "Failed to normalize comment", e.what());
      return nullptr;
    }
  }

  /**
   * Normalize Message from JSON
   */
  std::shared_ptr<Message> normalizeMessage(const nlohmann::json &json) {
    try {
      auto message = Message::fromJson(json);
      if (message.id.isEmpty()) {
        return nullptr;
      }

      return messages_.getOrCreate(message.id, [message]() { return std::make_shared<Message>(message); });
    } catch (const Json::ValidationError &e) {
      Util::logError("EntityStore", "Failed to normalize message", e.what());
      return nullptr;
    }
  }

  /**
   * Normalize Conversation from JSON
   */
  std::shared_ptr<Conversation> normalizeConversation(const nlohmann::json &json) {
    try {
      auto conversation = Conversation::fromJson(json);
      if (conversation.id.isEmpty()) {
        return nullptr;
      }

      return conversations_.getOrCreate(conversation.id,
                                        [conversation]() { return std::make_shared<Conversation>(conversation); });
    } catch (const Json::ValidationError &e) {
      Util::logError("EntityStore", "Failed to normalize conversation", e.what());
      return nullptr;
    }
  }

  /**
   * Normalize Playlist from JSON
   * TODO: Implement Playlist::fromJson() serialization
   */
  std::shared_ptr<Playlist> normalizePlaylist([[maybe_unused]] const juce::var &json) {
    // TODO: Implement Playlist::fromJson() with proper nlohmann::json serialization
    // For now, skip normalization
    return nullptr;
    // auto playlist = Playlist::fromJson(json);
    // if (playlist.id.isEmpty()) {
    //   return nullptr;
    // }
    // return playlists_.getOrCreate(playlist.id, [playlist]() { return std::make_shared<Playlist>(playlist); });
  }

  /**
   * Normalize array of posts from JSON
   * Returns vector of shared_ptrs with deduplication
   */
  std::vector<std::shared_ptr<FeedPost>> normalizePosts(const juce::Array<juce::var> &jsonArray) {
    std::vector<std::shared_ptr<FeedPost>> posts;
    posts.reserve(static_cast<size_t>(jsonArray.size()));

    for (const auto &json : jsonArray) {
      if (auto post = normalizePost(json)) {
        posts.push_back(post);
      }
    }

    return posts;
  }

  /**
   * Normalize array of playlists from JSON
   */
  std::vector<std::shared_ptr<Playlist>> normalizePlaylists(const juce::Array<juce::var> &jsonArray) {
    std::vector<std::shared_ptr<Playlist>> playlists;
    playlists.reserve(static_cast<size_t>(jsonArray.size()));

    for (const auto &json : jsonArray) {
      if (auto playlist = normalizePlaylist(json)) {
        playlists.push_back(playlist);
      }
    }

    return playlists;
  }

  /**
   * Normalize array of comments from JSON
   * Returns vector of shared_ptrs with deduplication
   */
  std::vector<std::shared_ptr<Comment>> normalizeComments(const nlohmann::json &jsonArray) {
    std::vector<std::shared_ptr<Comment>> comments;

    if (!jsonArray.is_array()) {
      return comments;
    }

    comments.reserve(jsonArray.size());

    for (const auto &json : jsonArray) {
      if (auto comment = normalizeComment(json)) {
        comments.push_back(comment);
      }
    }

    return comments;
  }

  // ==============================================================================
  // Configuration

  /**
   * Set NetworkClient for fetch operations
   */
  void setNetworkClient(NetworkClient *client) {
    networkClient_ = client;
  }

  /**
   * Configure default TTLs for each entity type
   *
   * Recommended values:
   * - Posts/feeds: 30 seconds (frequent updates)
   * - Stories: 5 minutes (24-hour lifespan, view counts change)
   * - Users: 10 minutes (profiles change infrequently)
   * - Notifications: 1 minute (real-time expectations)
   * - Comments: 1 minute (frequent updates, replies)
   * - Messages: 0 (no TTL, invalidate via WebSocket only)
   * - Conversations: 5 minutes
   * - Playlists: 5 minutes
   * - Challenges: 5 minutes
   * - Drafts: 0 (local only, no expiration)
   * - Sounds: 10 minutes
   */
  void configureDefaultTTLs() {
    posts_.setDefaultTTL(30 * 1000);             // 30 seconds
    stories_.setDefaultTTL(5 * 60 * 1000);       // 5 minutes
    users_.setDefaultTTL(10 * 60 * 1000);        // 10 minutes
    notifications_.setDefaultTTL(1 * 60 * 1000); // 1 minute
    comments_.setDefaultTTL(1 * 60 * 1000);      // 1 minute
    messages_.setDefaultTTL(0);                  // No expiration (WebSocket only)
    conversations_.setDefaultTTL(5 * 60 * 1000); // 5 minutes
    playlists_.setDefaultTTL(5 * 60 * 1000);     // 5 minutes
    challenges_.setDefaultTTL(5 * 60 * 1000);    // 5 minutes
    drafts_.setDefaultTTL(0);                    // No expiration (local only)
    sounds_.setDefaultTTL(10 * 60 * 1000);       // 10 minutes
  }

  // ==============================================================================
  // High-level fetch operations (with automatic caching)
  // These will be implemented when NetworkClient is fully migrated to nlohmann::json
  // Phase 2: Enable these methods for direct entity fetching

  // rxcpp::observable<FeedPost> fetchPost(const juce::String& id);
  // rxcpp::observable<Story> fetchStory(const juce::String& id);
  // rxcpp::observable<User> fetchUser(const juce::String& id);
  // rxcpp::observable<std::vector<FeedPost>> fetchPosts(const juce::Array<juce::String>& ids);

  // ==============================================================================
  // WebSocket event handlers (cache invalidation with typed JSON)

  /**
   * Handle post update from WebSocket
   * Updates entity in cache and notifies all subscribers
   */
  void onPostUpdated(const juce::String &postId, const juce::var &data) {
    auto updatedPost = FeedPost::fromJson(data);
    if (!updatedPost.id.isEmpty()) {
      // Get or create shared_ptr, then update it
      auto postPtr =
          posts_.getOrCreate(updatedPost.id, [updatedPost]() { return std::make_shared<FeedPost>(updatedPost); });
      *postPtr = updatedPost;
      posts_.set(postId, postPtr);
    }
  }

  /**
   * Handle story viewed event
   * Updates story view count
   */
  void onStoryViewed(const juce::String &storyId) {
    stories_.update(storyId, [](Story &story) {
      story.viewCount++;
      story.viewed = true;
    });
  }

  /**
   * Handle user profile update from WebSocket
   */
  void onUserUpdated(const juce::String &userId, const nlohmann::json &data) {
    try {
      auto updatedUser = User::fromJson(data);
      if (!updatedUser.id.isEmpty()) {
        auto userPtr =
            users_.getOrCreate(updatedUser.id, [updatedUser]() { return std::make_shared<User>(updatedUser); });
        *userPtr = updatedUser;
        users_.set(userId, userPtr);
      }
    } catch (const Json::ValidationError &e) {
      Util::logError("EntityStore", "Failed to parse user update", e.what());
      users_.invalidate(userId);
    }
  }

  /**
   * Handle new message from WebSocket
   */
  void onMessageReceived(const juce::String &conversationId, const nlohmann::json &messageData) {
    try {
      auto message = Message::fromJson(messageData);
      if (!message.id.isEmpty()) {
        auto messagePtr = messages_.getOrCreate(message.id, [message]() { return std::make_shared<Message>(message); });
        *messagePtr = message;
        messages_.set(message.id, messagePtr);

        // Update conversation's last message
        conversations_.update(conversationId, [&message](Conversation &conv) {
          conv.lastMessageId = message.id;
          conv.lastMessageText = message.text;
          conv.lastMessageSenderId = message.senderId;
          conv.lastMessageAt = message.createdAt;
          conv.unreadCount++;
        });
      }
    } catch (const Json::ValidationError &e) {
      Util::logError("EntityStore", "Failed to parse message", e.what());
    }
  }

  // ==============================================================================
  // TTL expiration management

  /**
   * Start background timer to expire stale cache entries
   * Call this once during app initialization
   */
  void startExpirationTimer() {
    if (expirationTimer_)
      return; // Already started

    // Use std::unique_ptr<juce::Timer> with custom timer implementation
    class ExpirationTimer : public juce::Timer {
    public:
      ExpirationTimer(EntityStore *store) : store_(store) {}

      void timerCallback() override {
        if (store_) {
          store_->posts_.expireStale();
          store_->stories_.expireStale();
          store_->users_.expireStale();
          store_->notifications_.expireStale();
          store_->comments_.expireStale();
          store_->messages_.expireStale();
          store_->conversations_.expireStale();
          store_->playlists_.expireStale();
          store_->challenges_.expireStale();
          store_->drafts_.expireStale();
          store_->sounds_.expireStale();
        }
      }

    private:
      EntityStore *store_;
    };

    expirationTimer_ = std::make_unique<ExpirationTimer>(this);
    expirationTimer_->startTimer(60000); // Run every 60 seconds
  }

  /**
   * Stop the expiration timer
   */
  void stopExpirationTimer() {
    if (expirationTimer_) {
      expirationTimer_->stopTimer();
      expirationTimer_.reset();
    }
  }

  // ==============================================================================
  // Manual invalidation APIs

  void invalidateAllPosts() {
    posts_.invalidateAll();
  }

  void invalidateAllStories() {
    stories_.invalidateAll();
  }

  void invalidateAllUsers() {
    users_.invalidateAll();
  }

  void invalidateAllNotifications() {
    notifications_.invalidateAll();
  }

  void invalidateAllComments() {
    comments_.invalidateAll();
  }

  void invalidateAllMessages() {
    messages_.invalidateAll();
  }

  void invalidateAllConversations() {
    conversations_.invalidateAll();
  }

  void invalidateAll() {
    posts_.invalidateAll();
    stories_.invalidateAll();
    users_.invalidateAll();
    notifications_.invalidateAll();
    comments_.invalidateAll();
    messages_.invalidateAll();
    conversations_.invalidateAll();
    playlists_.invalidateAll();
    challenges_.invalidateAll();
    drafts_.invalidateAll();
    sounds_.invalidateAll();
  }

  // ==============================================================================
  // Statistics

  size_t totalEntityCount() const {
    return posts_.size() + stories_.size() + users_.size() + notifications_.size() + comments_.size() +
           messages_.size() + conversations_.size() + playlists_.size() + challenges_.size() + drafts_.size() +
           sounds_.size();
  }

private:
  // ==============================================================================
  // Private constructor (singleton)

  EntityStore() {
    // Configure default TTLs
    configureDefaultTTLs();
  }

  ~EntityStore() {
    stopExpirationTimer();
  }

  // ==============================================================================
  // Entity caches

  EntityCache<FeedPost> posts_;
  EntityCache<Story> stories_;
  EntityCache<User> users_;
  EntityCache<Notification> notifications_;
  EntityCache<Comment> comments_;
  EntityCache<Message> messages_;
  EntityCache<Conversation> conversations_;
  EntityCache<Playlist> playlists_;
  EntityCache<MIDIChallenge> challenges_;
  EntityCache<Draft> drafts_;
  EntityCache<Sound> sounds_;

  // ==============================================================================
  // Dependencies

  NetworkClient *networkClient_ = nullptr;

  // ==============================================================================
  // Background expiration timer

  std::unique_ptr<juce::Timer> expirationTimer_;
};

} // namespace Stores
} // namespace Sidechain
