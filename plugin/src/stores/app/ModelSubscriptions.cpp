#include "../../stores/AppStore.h"
#include "../../stores/EntityStore.h"
#include "../../network/NetworkClient.h"
#include "../../util/logging/Logger.h"
#include <JuceHeader.h>

namespace Sidechain {
namespace Stores {

// ==============================================================================
// Post-Level Subscriptions

std::function<void()> AppStore::subscribeToPost(const juce::String &postId,
                                                std::function<void(const std::shared_ptr<FeedPost> &)> callback) {
  auto &entityStore = EntityStore::getInstance();
  return entityStore.posts().subscribe(postId, callback);
}

std::function<void()>
AppStore::subscribeToPosts(std::function<void(const std::vector<std::shared_ptr<FeedPost>> &)> callback) {
  // Subscribe to all posts in EntityStore
  auto &entityStore = EntityStore::getInstance();
  return entityStore.posts().subscribeAll(callback);
}

// ==============================================================================
// User-Level Subscriptions

std::function<void()> AppStore::subscribeToUser(const juce::String &userId,
                                                std::function<void(const std::shared_ptr<User> &)> callback) {
  auto &entityStore = EntityStore::getInstance();
  return entityStore.users().subscribe(userId, callback);
}

// ==============================================================================
// Load User Profile

void AppStore::loadUser(const juce::String &userId, bool forceRefresh) {
  if (!networkClient) {
    Sidechain::Util::logError("AppStore", "NetworkClient not set");
    return;
  }

  auto &entityStore = EntityStore::getInstance();

  // Check if already cached (unless force refresh)
  if (!forceRefresh) {
    auto cached = entityStore.users().get(userId);
    if (cached) {
      return; // Already loaded
    }
  }

  // Fetch from network
  networkClient->getUser(userId, [this, userId](Outcome<juce::var> result) {
    if (!result.isOk()) {
      Sidechain::Util::logError("AppStore", "Failed to load user: " + result.getError());
      return;
    }

    auto &entityStore = EntityStore::getInstance();
    try {
      auto json = nlohmann::json::parse(result.getValue().toString().toStdString());
      entityStore.normalizeUser(json);
    } catch (const std::exception &e) {
      Sidechain::Util::logError("AppStore", "Failed to parse user JSON: " + juce::String(e.what()));
    }
  });
}

// ==============================================================================
// Load User Posts

void AppStore::loadUserPosts(const juce::String &userId, int limit, int offset) {
  if (!networkClient) {
    Sidechain::Util::logError("AppStore", "NetworkClient not set");
    return;
  }

  auto &entityStore = EntityStore::getInstance();

  networkClient->getUserPosts(userId, limit, offset, [this, userId](Outcome<juce::var> result) {
    if (!result.isOk()) {
      Sidechain::Util::logError("AppStore", "Failed to load user posts: " + result.getError());
      return;
    }

    try {
      auto var = result.getValue();
      if (var.isArray()) {
        for (int i = 0; i < var.size(); ++i) {
          EntityStore::getInstance().normalizePost(var[i]);
        }
      }
    } catch (const std::exception &e) {
      Sidechain::Util::logError("AppStore", "Failed to parse user posts: " + juce::String(e.what()));
    }
  });
}

// ==============================================================================
// Load Followers

void AppStore::loadFollowers(const juce::String &userId, int limit, int offset) {
  if (!networkClient) {
    Sidechain::Util::logError("AppStore", "NetworkClient not set");
    return;
  }

  auto &entityStore = EntityStore::getInstance();

  networkClient->getFollowers(userId, limit, offset, [this, userId, &entityStore](Outcome<juce::var> result) {
    if (!result.isOk()) {
      Sidechain::Util::logError("AppStore", "Failed to load followers: " + result.getError());
      return;
    }

    try {
      auto var = result.getValue();
      if (var.isArray()) {
        for (int i = 0; i < var.size(); ++i) {
          auto json = nlohmann::json::parse(var[i].toString().toStdString());
          entityStore.normalizeUser(json);
        }
      }
    } catch (const std::exception &e) {
      Sidechain::Util::logError("AppStore", "Failed to parse followers JSON: " + juce::String(e.what()));
    }
  });
}

// ==============================================================================
// Load Following

void AppStore::loadFollowing(const juce::String &userId, int limit, int offset) {
  if (!networkClient) {
    Sidechain::Util::logError("AppStore", "NetworkClient not set");
    return;
  }

  auto &entityStore = EntityStore::getInstance();

  networkClient->getFollowing(userId, limit, offset, [this, userId, &entityStore](Outcome<juce::var> result) {
    if (!result.isOk()) {
      Sidechain::Util::logError("AppStore", "Failed to load following: " + result.getError());
      return;
    }

    try {
      auto var = result.getValue();
      if (var.isArray()) {
        for (int i = 0; i < var.size(); ++i) {
          auto json = nlohmann::json::parse(var[i].toString().toStdString());
          entityStore.normalizeUser(json);
        }
      }
    } catch (const std::exception &e) {
      Sidechain::Util::logError("AppStore", "Failed to parse following JSON: " + juce::String(e.what()));
    }
  });
}

// ==============================================================================
// Search and Discovery

void AppStore::searchUsersAndCache(const juce::String &query, int limit, int offset) {
  if (!networkClient) {
    Sidechain::Util::logError("AppStore", "NetworkClient not set");
    return;
  }

  auto &entityStore = EntityStore::getInstance();

  networkClient->searchUsers(query, limit, offset, [this, query, &entityStore](Outcome<juce::var> result) {
    if (!result.isOk()) {
      Sidechain::Util::logError("AppStore", "Failed to search users: " + result.getError());
      return;
    }

    try {
      auto var = result.getValue();
      if (var.isArray()) {
        for (int i = 0; i < var.size(); ++i) {
          auto json = nlohmann::json::parse(var[i].toString().toStdString());
          entityStore.normalizeUser(json);
        }
      }
    } catch (const std::exception &e) {
      Sidechain::Util::logError("AppStore", "Failed to parse search results JSON: " + juce::String(e.what()));
    }
  });
}

void AppStore::loadTrendingUsersAndCache(int limit) {
  if (!networkClient) {
    Sidechain::Util::logError("AppStore", "NetworkClient not set");
    return;
  }

  // Delegate to existing method which also caches
  loadTrendingUsers();
}

void AppStore::loadFeaturedProducersAndCache(int limit) {
  if (!networkClient) {
    Sidechain::Util::logError("AppStore", "NetworkClient not set");
    return;
  }

  // Delegate to existing method which also caches
  loadFeaturedProducers();
}

void AppStore::loadSuggestedUsersAndCache(int limit) {
  if (!networkClient) {
    Sidechain::Util::logError("AppStore", "NetworkClient not set");
    return;
  }

  // Delegate to existing method which also caches
  loadSuggestedUsers();
}

} // namespace Stores
} // namespace Sidechain
