#include "SavedPostsStore.h"
#include "../network/NetworkClient.h"
#include "../util/logging/Logger.h"

namespace Sidechain {
namespace Stores {

SavedPostsStore::SavedPostsStore(NetworkClient *client) : Store(SavedPostsState()), networkClient(client) {
  Util::logInfo("SavedPostsStore", "Initialized");
}

//==============================================================================
void SavedPostsStore::loadSavedPosts() {
  if (networkClient == nullptr) {
    Util::logWarning("SavedPostsStore", "Cannot load saved posts - networkClient null");
    return;
  }

  Util::logInfo("SavedPostsStore", "Loading saved posts");

  updateState([](SavedPostsState &state) {
    state.isLoading = true;
    state.offset = 0;
    state.posts.clear();
    state.error = "";
  });

  networkClient->getSavedPosts(20, 0, [this](Outcome<juce::var> result) { handleSavedPostsLoaded(result); });
}

void SavedPostsStore::loadMoreSavedPosts() {
  auto state = getState();
  if (!state.hasMore || state.isLoading || networkClient == nullptr) {
    return;
  }

  Util::logDebug("SavedPostsStore", "Loading more saved posts");

  updateState([](SavedPostsState &s) { s.isLoading = true; });

  networkClient->getSavedPosts(state.limit, state.offset,
                               [this](Outcome<juce::var> result) { handleSavedPostsLoaded(result); });
}

void SavedPostsStore::refreshSavedPosts() {
  loadSavedPosts();
}

void SavedPostsStore::handleSavedPostsLoaded(Outcome<juce::var> result) {
  if (!result.isOk()) {
    updateState([error = result.getError()](SavedPostsState &s) {
      s.isLoading = false;
      s.error = error;
    });
    return;
  }

  auto data = result.getValue();
  if (!data.isObject()) {
    updateState([](SavedPostsState &s) {
      s.isLoading = false;
      s.error = "Invalid saved posts response";
    });
    return;
  }

  auto postsArray = data.getProperty("posts", juce::var());
  auto totalCount = static_cast<int>(data.getProperty("total", 0));

  if (!postsArray.isArray()) {
    updateState([](SavedPostsState &s) {
      s.isLoading = false;
      s.error = "Invalid posts array in response";
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

  updateState([loadedPosts, totalCount](SavedPostsState &s) {
    s.posts = loadedPosts;
    s.isLoading = false;
    s.totalCount = totalCount;
    s.offset += loadedPosts.size();
    s.hasMore = s.offset < totalCount;
    s.error = "";
    s.lastUpdated = juce::Time::getCurrentTime().toMilliseconds();
  });

  Util::logDebug("SavedPostsStore", "Loaded " + juce::String(loadedPosts.size()) + " saved posts");
}

//==============================================================================
void SavedPostsStore::unsavePost(const juce::String &postId) {
  if (networkClient == nullptr) {
    Util::logWarning("SavedPostsStore", "Cannot unsave post - networkClient null");
    return;
  }

  Util::logInfo("SavedPostsStore", "Unsaving post: " + postId);

  // Optimistic removal
  updateState([this, postId](SavedPostsState &) { removePostFromState(postId); });

  // Send to server
  networkClient->unsavePost(postId, [this, postId](Outcome<juce::var> result) { handlePostUnsaved(postId, result); });
}

void SavedPostsStore::handlePostUnsaved([[maybe_unused]] const juce::String &postId, Outcome<juce::var> result) {
  if (!result.isOk()) {
    // Refresh on error to restore the post
    Util::logError("SavedPostsStore", "Failed to unsave post: " + result.getError());
    refreshSavedPosts();
  } else {
    Util::logDebug("SavedPostsStore", "Post unsaved successfully");
  }
}

//==============================================================================
std::optional<FeedPost> SavedPostsStore::getPostById(const juce::String &postId) const {
  auto state = getState();
  for (const auto &post : state.posts) {
    if (post.id == postId) {
      return post;
    }
  }
  return std::nullopt;
}

//==============================================================================
void SavedPostsStore::removePostFromState(const juce::String &postId) {
  auto state = getState();
  for (int i = 0; i < state.posts.size(); ++i) {
    if (state.posts[i].id == postId) {
      state.posts.remove(i);
      setState(state);
      return;
    }
  }
}

void SavedPostsStore::updatePostInState(const FeedPost &updatedPost) {
  auto state = getState();
  for (int i = 0; i < state.posts.size(); ++i) {
    if (state.posts[i].id == updatedPost.id) {
      state.posts.set(i, updatedPost);
      setState(state);
      return;
    }
  }
}

} // namespace Stores
} // namespace Sidechain
