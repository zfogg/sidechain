#include "PostsStore.h"
#include "../network/NetworkClient.h"
#include "../util/logging/Logger.h"

namespace Sidechain {
namespace Stores {

PostsStore::PostsStore(NetworkClient *client) : Store(PostsState()), networkClient(client) {
  Util::logInfo("PostsStore", "Initialized");
}

//==============================================================================
// Saved Posts Loading
//==============================================================================

void PostsStore::loadSavedPosts() {
  if (networkClient == nullptr) {
    Util::logWarning("PostsStore", "Cannot load saved posts - networkClient null");
    return;
  }

  Util::logInfo("PostsStore", "Loading saved posts");

  updateState([](PostsState &state) {
    state.savedPosts.isLoading = true;
    state.savedPosts.offset = 0;
    state.savedPosts.posts.clear();
    state.savedPosts.error = "";
  });

  networkClient->getSavedPosts(20, 0, [this](Outcome<juce::var> result) { handleSavedPostsLoaded(result); });
}

void PostsStore::loadMoreSavedPosts() {
  auto state = getState();
  if (!state.savedPosts.hasMore || state.savedPosts.isLoading || networkClient == nullptr) {
    return;
  }

  Util::logDebug("PostsStore", "Loading more saved posts");

  updateState([](PostsState &s) { s.savedPosts.isLoading = true; });

  networkClient->getSavedPosts(state.savedPosts.limit, state.savedPosts.offset,
                               [this](Outcome<juce::var> result) { handleSavedPostsLoaded(result); });
}

void PostsStore::refreshSavedPosts() {
  loadSavedPosts();
}

void PostsStore::handleSavedPostsLoaded(Outcome<juce::var> result) {
  if (!result.isOk()) {
    updateState([error = result.getError()](PostsState &s) {
      s.savedPosts.isLoading = false;
      s.savedPosts.error = error;
    });
    return;
  }

  auto data = result.getValue();
  if (!data.isObject()) {
    updateState([](PostsState &s) {
      s.savedPosts.isLoading = false;
      s.savedPosts.error = "Invalid saved posts response";
    });
    return;
  }

  auto postsArray = data.getProperty("posts", juce::var());
  auto totalCount = static_cast<int>(data.getProperty("total", 0));

  if (!postsArray.isArray()) {
    updateState([](PostsState &s) {
      s.savedPosts.isLoading = false;
      s.savedPosts.error = "Invalid posts array in response";
    });
    return;
  }

  juce::Array<FeedPost> loadedPosts;
  for (int i = 0; i < postsArray.size(); ++i) {
    auto post = FeedPost::fromJson(postsArray[i]);
    if (post.isValid()) {
      loadedPosts.add(post);
    }
  }

  updateState([loadedPosts, totalCount](PostsState &s) {
    s.savedPosts.posts = loadedPosts;
    s.savedPosts.isLoading = false;
    s.savedPosts.totalCount = totalCount;
    s.savedPosts.offset += loadedPosts.size();
    s.savedPosts.hasMore = s.savedPosts.offset < totalCount;
    s.savedPosts.error = "";
    s.savedPosts.lastUpdated = juce::Time::getCurrentTime().toMilliseconds();
  });

  Util::logDebug("PostsStore", "Loaded " + juce::String(loadedPosts.size()) + " saved posts");
}

//==============================================================================
// Archived Posts Loading
//==============================================================================

void PostsStore::loadArchivedPosts() {
  if (networkClient == nullptr) {
    Util::logWarning("PostsStore", "Cannot load archived posts - networkClient null");
    return;
  }

  Util::logInfo("PostsStore", "Loading archived posts");

  updateState([](PostsState &state) {
    state.archivedPosts.isLoading = true;
    state.archivedPosts.offset = 0;
    state.archivedPosts.posts.clear();
    state.archivedPosts.error = "";
  });

  networkClient->getArchivedPosts(20, 0, [this](Outcome<juce::var> result) { handleArchivedPostsLoaded(result); });
}

void PostsStore::loadMoreArchivedPosts() {
  auto state = getState();
  if (!state.archivedPosts.hasMore || state.archivedPosts.isLoading || networkClient == nullptr) {
    return;
  }

  Util::logDebug("PostsStore", "Loading more archived posts");

  updateState([](PostsState &s) { s.archivedPosts.isLoading = true; });

  networkClient->getArchivedPosts(state.archivedPosts.limit, state.archivedPosts.offset,
                                  [this](Outcome<juce::var> result) { handleArchivedPostsLoaded(result); });
}

void PostsStore::refreshArchivedPosts() {
  loadArchivedPosts();
}

void PostsStore::handleArchivedPostsLoaded(Outcome<juce::var> result) {
  if (!result.isOk()) {
    updateState([error = result.getError()](PostsState &s) {
      s.archivedPosts.isLoading = false;
      s.archivedPosts.error = error;
    });
    return;
  }

  auto data = result.getValue();
  if (!data.isObject()) {
    updateState([](PostsState &s) {
      s.archivedPosts.isLoading = false;
      s.archivedPosts.error = "Invalid archived posts response";
    });
    return;
  }

  auto postsArray = data.getProperty("posts", juce::var());
  auto totalCount = static_cast<int>(data.getProperty("total", 0));

  if (!postsArray.isArray()) {
    updateState([](PostsState &s) {
      s.archivedPosts.isLoading = false;
      s.archivedPosts.error = "Invalid posts array in response";
    });
    return;
  }

  juce::Array<FeedPost> loadedPosts;
  for (int i = 0; i < postsArray.size(); ++i) {
    auto post = FeedPost::fromJson(postsArray[i]);
    if (post.isValid()) {
      loadedPosts.add(post);
    }
  }

  updateState([loadedPosts, totalCount](PostsState &s) {
    s.archivedPosts.posts = loadedPosts;
    s.archivedPosts.isLoading = false;
    s.archivedPosts.totalCount = totalCount;
    s.archivedPosts.offset += loadedPosts.size();
    s.archivedPosts.hasMore = s.archivedPosts.offset < totalCount;
    s.archivedPosts.error = "";
    s.archivedPosts.lastUpdated = juce::Time::getCurrentTime().toMilliseconds();
  });

  Util::logDebug("PostsStore", "Loaded " + juce::String(loadedPosts.size()) + " archived posts");
}

//==============================================================================
// Saved Posts Operations
//==============================================================================

void PostsStore::unsavePost(const juce::String &postId) {
  if (networkClient == nullptr) {
    Util::logWarning("PostsStore", "Cannot unsave post - networkClient null");
    return;
  }

  Util::logInfo("PostsStore", "Unsaving post: " + postId);

  // Optimistic removal
  removePostFromSaved(postId);

  // Send to server
  networkClient->unsavePost(postId, [this, postId](Outcome<juce::var> result) { handlePostUnsaved(postId, result); });
}

std::optional<FeedPost> PostsStore::getSavedPostById(const juce::String &postId) const {
  auto state = getState();
  for (const auto &post : state.savedPosts.posts) {
    if (post.id == postId) {
      return post;
    }
  }
  return std::nullopt;
}

void PostsStore::removePostFromSaved(const juce::String &postId) {
  auto state = getState();
  for (int i = 0; i < state.savedPosts.posts.size(); ++i) {
    if (state.savedPosts.posts[i].id == postId) {
      state.savedPosts.posts.remove(i);
      setState(state);
      return;
    }
  }
}

void PostsStore::updatePostInSaved(const FeedPost &updatedPost) {
  auto state = getState();
  for (int i = 0; i < state.savedPosts.posts.size(); ++i) {
    if (state.savedPosts.posts[i].id == updatedPost.id) {
      state.savedPosts.posts.set(i, updatedPost);
      setState(state);
      return;
    }
  }
}

void PostsStore::handlePostUnsaved([[maybe_unused]] const juce::String &postId, Outcome<juce::var> result) {
  if (!result.isOk()) {
    // Refresh on error to restore the post
    Util::logError("PostsStore", "Failed to unsave post: " + result.getError());
    refreshSavedPosts();
  } else {
    Util::logDebug("PostsStore", "Post unsaved successfully");
  }
}

//==============================================================================
// Archived Posts Operations
//==============================================================================

void PostsStore::restorePost(const juce::String &postId) {
  if (networkClient == nullptr) {
    Util::logWarning("PostsStore", "Cannot restore post - networkClient null");
    return;
  }

  Util::logInfo("PostsStore", "Restoring post: " + postId);

  // Optimistic removal (since restored posts go back to active)
  removePostFromArchived(postId);

  // Send to server
  networkClient->unarchivePost(postId,
                               [this, postId](Outcome<juce::var> result) { handlePostRestored(postId, result); });
}

std::optional<FeedPost> PostsStore::getArchivedPostById(const juce::String &postId) const {
  auto state = getState();
  for (const auto &post : state.archivedPosts.posts) {
    if (post.id == postId) {
      return post;
    }
  }
  return std::nullopt;
}

void PostsStore::removePostFromArchived(const juce::String &postId) {
  auto state = getState();
  for (int i = 0; i < state.archivedPosts.posts.size(); ++i) {
    if (state.archivedPosts.posts[i].id == postId) {
      state.archivedPosts.posts.remove(i);
      setState(state);
      return;
    }
  }
}

void PostsStore::updatePostInArchived(const FeedPost &updatedPost) {
  auto state = getState();
  for (int i = 0; i < state.archivedPosts.posts.size(); ++i) {
    if (state.archivedPosts.posts[i].id == updatedPost.id) {
      state.archivedPosts.posts.set(i, updatedPost);
      setState(state);
      return;
    }
  }
}

void PostsStore::handlePostRestored([[maybe_unused]] const juce::String &postId, Outcome<juce::var> result) {
  if (!result.isOk()) {
    // Refresh on error to restore the post to the list
    Util::logError("PostsStore", "Failed to restore post: " + result.getError());
    refreshArchivedPosts();
  } else {
    Util::logDebug("PostsStore", "Post restored successfully");
  }
}

} // namespace Stores
} // namespace Sidechain
