#include "ArchivedPostsStore.h"
#include "../network/NetworkClient.h"
#include "../util/logging/Logger.h"

namespace Sidechain {
namespace Stores {

ArchivedPostsStore::ArchivedPostsStore(NetworkClient *client) : Store(ArchivedPostsState()), networkClient(client) {
  Util::logInfo("ArchivedPostsStore", "Initialized");
}

//==============================================================================
void ArchivedPostsStore::loadArchivedPosts() {
  if (networkClient == nullptr) {
    Util::logWarning("ArchivedPostsStore", "Cannot load archived posts - networkClient null");
    return;
  }

  Util::logInfo("ArchivedPostsStore", "Loading archived posts");

  updateState([](ArchivedPostsState &state) {
    state.isLoading = true;
    state.offset = 0;
    state.posts.clear();
    state.error = "";
  });

  networkClient->getArchivedPosts(20, 0, [this](Outcome<juce::var> result) { handleArchivedPostsLoaded(result); });
}

void ArchivedPostsStore::loadMoreArchivedPosts() {
  auto state = getState();
  if (!state.hasMore || state.isLoading || networkClient == nullptr) {
    return;
  }

  Util::logDebug("ArchivedPostsStore", "Loading more archived posts");

  updateState([](ArchivedPostsState &s) { s.isLoading = true; });

  networkClient->getArchivedPosts(state.limit, state.offset,
                                  [this](Outcome<juce::var> result) { handleArchivedPostsLoaded(result); });
}

void ArchivedPostsStore::refreshArchivedPosts() {
  loadArchivedPosts();
}

void ArchivedPostsStore::handleArchivedPostsLoaded(Outcome<juce::var> result) {
  if (!result.isOk()) {
    updateState([error = result.getError()](ArchivedPostsState &s) {
      s.isLoading = false;
      s.error = error;
    });
    return;
  }

  auto data = result.getValue();
  if (!data.isObject()) {
    updateState([](ArchivedPostsState &s) {
      s.isLoading = false;
      s.error = "Invalid archived posts response";
    });
    return;
  }

  auto postsArray = data.getProperty("posts", juce::var());
  auto totalCount = static_cast<int>(data.getProperty("total", 0));

  if (!postsArray.isArray()) {
    updateState([](ArchivedPostsState &s) {
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

  updateState([loadedPosts, totalCount](ArchivedPostsState &s) {
    s.posts = loadedPosts;
    s.isLoading = false;
    s.totalCount = totalCount;
    s.offset += loadedPosts.size();
    s.hasMore = s.offset < totalCount;
    s.error = "";
    s.lastUpdated = juce::Time::getCurrentTime().toMilliseconds();
  });

  Util::logDebug("ArchivedPostsStore", "Loaded " + juce::String(loadedPosts.size()) + " archived posts");
}

//==============================================================================
void ArchivedPostsStore::restorePost(const juce::String &postId) {
  if (networkClient == nullptr) {
    Util::logWarning("ArchivedPostsStore", "Cannot restore post - networkClient null");
    return;
  }

  Util::logInfo("ArchivedPostsStore", "Restoring post: " + postId);

  // Optimistic removal (since restored posts go back to active)
  updateState([this, postId](ArchivedPostsState &) { removePostFromState(postId); });

  // Send to server
  networkClient->unarchivePost(postId,
                               [this, postId](Outcome<juce::var> result) { handlePostRestored(postId, result); });
}

void ArchivedPostsStore::handlePostRestored([[maybe_unused]] const juce::String &postId, Outcome<juce::var> result) {
  if (!result.isOk()) {
    // Refresh on error to restore the post to the list
    Util::logError("ArchivedPostsStore", "Failed to restore post: " + result.getError());
    refreshArchivedPosts();
  } else {
    Util::logDebug("ArchivedPostsStore", "Post restored successfully");
  }
}

//==============================================================================
std::optional<FeedPost> ArchivedPostsStore::getPostById(const juce::String &postId) const {
  auto state = getState();
  for (const auto &post : state.posts) {
    if (post.id == postId) {
      return post;
    }
  }
  return std::nullopt;
}

//==============================================================================
void ArchivedPostsStore::removePostFromState(const juce::String &postId) {
  auto state = getState();
  for (int i = 0; i < state.posts.size(); ++i) {
    if (state.posts[i].id == postId) {
      state.posts.remove(i);
      setState(state);
      return;
    }
  }
}

void ArchivedPostsStore::updatePostInState(const FeedPost &updatedPost) {
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
