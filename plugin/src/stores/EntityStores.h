#pragma once

#include "EntityCache.h"
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
#include <JuceHeader.h>
#include <memory>

namespace Sidechain {
namespace Stores {

/**
 * EntityStores - Per-type singleton entity caches
 *
 * Replaces EntityStore singleton wrapper with per-type singletons.
 * More direct access: PostCache::getInstance() vs EntityStore::getInstance().posts()
 *
 * Benefits:
 * - Each entity type has its own cache singleton
 * - Can be injected into components individually
 * - Clearer what each component depends on
 * - Easy to mock for testing
 * - No wrapper layer needed
 *
 * Usage:
 *   auto& posts = PostCache::getInstance();
 *   auto post = posts.get(postId);
 *
 *   auto& users = UserCache::getInstance();
 *   auto user = users.get(userId);
 *
 *   // Or dependency injection:
 *   class MyComponent {
 *     MyComponent(PostCache& posts, UserCache& users)
 *       : posts_(posts), users_(users) {}
 *   };
 */

// PostCache - FeedPost entities
class PostCache {
public:
  static PostCache &getInstance() {
    static PostCache instance;
    return instance;
  }

  EntityCache<FeedPost> &cache() {
    return cache_;
  }
  const EntityCache<FeedPost> &cache() const {
    return cache_;
  }

  // Convenience accessors
  std::shared_ptr<FeedPost> get(const juce::String &id) {
    return cache_.get(id);
  }
  void set(const juce::String &id, const std::shared_ptr<FeedPost> &entity) {
    cache_.set(id, entity);
  }
  template <typename Factory> std::shared_ptr<FeedPost> getOrCreate(const juce::String &id, Factory &&factory) {
    return cache_.getOrCreate(id, std::forward<Factory>(factory));
  }
  auto subscribe(const juce::String &id, std::function<void(const std::shared_ptr<FeedPost> &)> cb) {
    return cache_.subscribe(id, cb);
  }

private:
  PostCache() = default;
  EntityCache<FeedPost> cache_;
  PostCache(const PostCache &) = delete;
  PostCache &operator=(const PostCache &) = delete;
};

// UserCache - User entities
class UserCache {
public:
  static UserCache &getInstance() {
    static UserCache instance;
    return instance;
  }

  EntityCache<User> &cache() {
    return cache_;
  }
  const EntityCache<User> &cache() const {
    return cache_;
  }

  std::shared_ptr<User> get(const juce::String &id) {
    return cache_.get(id);
  }
  void set(const juce::String &id, const std::shared_ptr<User> &entity) {
    cache_.set(id, entity);
  }
  template <typename Factory> std::shared_ptr<User> getOrCreate(const juce::String &id, Factory &&factory) {
    return cache_.getOrCreate(id, std::forward<Factory>(factory));
  }
  auto subscribe(const juce::String &id, std::function<void(const std::shared_ptr<User> &)> cb) {
    return cache_.subscribe(id, cb);
  }

private:
  UserCache() = default;
  EntityCache<User> cache_;
  UserCache(const UserCache &) = delete;
  UserCache &operator=(const UserCache &) = delete;
};

// StoryCache - Story entities
class StoryCache {
public:
  static StoryCache &getInstance() {
    static StoryCache instance;
    return instance;
  }

  EntityCache<Story> &cache() {
    return cache_;
  }
  const EntityCache<Story> &cache() const {
    return cache_;
  }

  std::shared_ptr<Story> get(const juce::String &id) {
    return cache_.get(id);
  }
  void set(const juce::String &id, const std::shared_ptr<Story> &entity) {
    cache_.set(id, entity);
  }
  template <typename Factory> std::shared_ptr<Story> getOrCreate(const juce::String &id, Factory &&factory) {
    return cache_.getOrCreate(id, std::forward<Factory>(factory));
  }
  auto subscribe(const juce::String &id, std::function<void(const std::shared_ptr<Story> &)> cb) {
    return cache_.subscribe(id, cb);
  }

private:
  StoryCache() = default;
  EntityCache<Story> cache_;
  StoryCache(const StoryCache &) = delete;
  StoryCache &operator=(const StoryCache &) = delete;
};

// NotificationCache - Notification entities
class NotificationCache {
public:
  static NotificationCache &getInstance() {
    static NotificationCache instance;
    return instance;
  }

  EntityCache<Notification> &cache() {
    return cache_;
  }
  const EntityCache<Notification> &cache() const {
    return cache_;
  }

  std::shared_ptr<Notification> get(const juce::String &id) {
    return cache_.get(id);
  }
  void set(const juce::String &id, const std::shared_ptr<Notification> &entity) {
    cache_.set(id, entity);
  }
  auto subscribe(const juce::String &id, std::function<void(const std::shared_ptr<Notification> &)> cb) {
    return cache_.subscribe(id, cb);
  }

private:
  NotificationCache() = default;
  EntityCache<Notification> cache_;
  NotificationCache(const NotificationCache &) = delete;
  NotificationCache &operator=(const NotificationCache &) = delete;
};

// CommentCache - Comment entities
class CommentCache {
public:
  static CommentCache &getInstance() {
    static CommentCache instance;
    return instance;
  }

  EntityCache<Comment> &cache() {
    return cache_;
  }
  const EntityCache<Comment> &cache() const {
    return cache_;
  }

  std::shared_ptr<Comment> get(const juce::String &id) {
    return cache_.get(id);
  }
  void set(const juce::String &id, const std::shared_ptr<Comment> &entity) {
    cache_.set(id, entity);
  }
  auto subscribe(const juce::String &id, std::function<void(const std::shared_ptr<Comment> &)> cb) {
    return cache_.subscribe(id, cb);
  }

private:
  CommentCache() = default;
  EntityCache<Comment> cache_;
  CommentCache(const CommentCache &) = delete;
  CommentCache &operator=(const CommentCache &) = delete;
};

// MessageCache - Message entities
class MessageCache {
public:
  static MessageCache &getInstance() {
    static MessageCache instance;
    return instance;
  }

  EntityCache<Message> &cache() {
    return cache_;
  }
  const EntityCache<Message> &cache() const {
    return cache_;
  }

  std::shared_ptr<Message> get(const juce::String &id) {
    return cache_.get(id);
  }
  void set(const juce::String &id, const std::shared_ptr<Message> &entity) {
    cache_.set(id, entity);
  }
  auto subscribe(const juce::String &id, std::function<void(const std::shared_ptr<Message> &)> cb) {
    return cache_.subscribe(id, cb);
  }

private:
  MessageCache() = default;
  EntityCache<Message> cache_;
  MessageCache(const MessageCache &) = delete;
  MessageCache &operator=(const MessageCache &) = delete;
};

// ConversationCache - Conversation entities
class ConversationCache {
public:
  static ConversationCache &getInstance() {
    static ConversationCache instance;
    return instance;
  }

  EntityCache<Conversation> &cache() {
    return cache_;
  }
  const EntityCache<Conversation> &cache() const {
    return cache_;
  }

  std::shared_ptr<Conversation> get(const juce::String &id) {
    return cache_.get(id);
  }
  void set(const juce::String &id, const std::shared_ptr<Conversation> &entity) {
    cache_.set(id, entity);
  }
  auto subscribe(const juce::String &id, std::function<void(const std::shared_ptr<Conversation> &)> cb) {
    return cache_.subscribe(id, cb);
  }

private:
  ConversationCache() = default;
  EntityCache<Conversation> cache_;
  ConversationCache(const ConversationCache &) = delete;
  ConversationCache &operator=(const ConversationCache &) = delete;
};

// PlaylistCache - Playlist entities
class PlaylistCache {
public:
  static PlaylistCache &getInstance() {
    static PlaylistCache instance;
    return instance;
  }

  EntityCache<Playlist> &cache() {
    return cache_;
  }
  const EntityCache<Playlist> &cache() const {
    return cache_;
  }

  std::shared_ptr<Playlist> get(const juce::String &id) {
    return cache_.get(id);
  }
  void set(const juce::String &id, const std::shared_ptr<Playlist> &entity) {
    cache_.set(id, entity);
  }
  auto subscribe(const juce::String &id, std::function<void(const std::shared_ptr<Playlist> &)> cb) {
    return cache_.subscribe(id, cb);
  }

private:
  PlaylistCache() = default;
  EntityCache<Playlist> cache_;
  PlaylistCache(const PlaylistCache &) = delete;
  PlaylistCache &operator=(const PlaylistCache &) = delete;
};

// ChallengeCache - MIDIChallenge entities
class ChallengeCache {
public:
  static ChallengeCache &getInstance() {
    static ChallengeCache instance;
    return instance;
  }

  EntityCache<MIDIChallenge> &cache() {
    return cache_;
  }
  const EntityCache<MIDIChallenge> &cache() const {
    return cache_;
  }

  std::shared_ptr<MIDIChallenge> get(const juce::String &id) {
    return cache_.get(id);
  }
  void set(const juce::String &id, const std::shared_ptr<MIDIChallenge> &entity) {
    cache_.set(id, entity);
  }
  auto subscribe(const juce::String &id, std::function<void(const std::shared_ptr<MIDIChallenge> &)> cb) {
    return cache_.subscribe(id, cb);
  }

private:
  ChallengeCache() = default;
  EntityCache<MIDIChallenge> cache_;
  ChallengeCache(const ChallengeCache &) = delete;
  ChallengeCache &operator=(const ChallengeCache &) = delete;
};

// SoundCache - Sound entities
class SoundCache {
public:
  static SoundCache &getInstance() {
    static SoundCache instance;
    return instance;
  }

  EntityCache<Sound> &cache() {
    return cache_;
  }
  const EntityCache<Sound> &cache() const {
    return cache_;
  }

  std::shared_ptr<Sound> get(const juce::String &id) {
    return cache_.get(id);
  }
  void set(const juce::String &id, const std::shared_ptr<Sound> &entity) {
    cache_.set(id, entity);
  }
  auto subscribe(const juce::String &id, std::function<void(const std::shared_ptr<Sound> &)> cb) {
    return cache_.subscribe(id, cb);
  }

private:
  SoundCache() = default;
  EntityCache<Sound> cache_;
  SoundCache(const SoundCache &) = delete;
  SoundCache &operator=(const SoundCache &) = delete;
};

// DraftCache - Draft entities
class DraftCache {
public:
  static DraftCache &getInstance() {
    static DraftCache instance;
    return instance;
  }

  EntityCache<Draft> &cache() {
    return cache_;
  }
  const EntityCache<Draft> &cache() const {
    return cache_;
  }

  std::shared_ptr<Draft> get(const juce::String &id) {
    return cache_.get(id);
  }
  void set(const juce::String &id, const std::shared_ptr<Draft> &entity) {
    cache_.set(id, entity);
  }
  auto subscribe(const juce::String &id, std::function<void(const std::shared_ptr<Draft> &)> cb) {
    return cache_.subscribe(id, cb);
  }

private:
  DraftCache() = default;
  EntityCache<Draft> cache_;
  DraftCache(const DraftCache &) = delete;
  DraftCache &operator=(const DraftCache &) = delete;
};

} // namespace Stores
} // namespace Sidechain
