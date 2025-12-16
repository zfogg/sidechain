#include "../AppStore.h"
#include "../../util/logging/Logger.h"

namespace Sidechain {
namespace Stores {

void AppStore::loadFeed(FeedType feedType, bool forceRefresh) {
  // TODO: Implement feed loading
  updateFeedState([feedType](PostsState &state) {
    state.currentFeedType = feedType;
    state.feedError = "";
  });
}

void AppStore::refreshCurrentFeed() {
  // TODO: Implement feed refresh
}

void AppStore::loadMore() {
  // TODO: Implement load more
}

void AppStore::switchFeedType(FeedType feedType) {
  updateFeedState([feedType](PostsState &state) { state.currentFeedType = feedType; });
}

void AppStore::toggleLike(const juce::String &postId) {
  // TODO: Implement toggle like
}

void AppStore::toggleSave(const juce::String &postId) {
  // TODO: Implement toggle save
}

void AppStore::toggleRepost(const juce::String &postId) {
  // TODO: Implement toggle repost
}

void AppStore::addReaction(const juce::String &postId, const juce::String &emoji) {
  // TODO: Implement add reaction
}

void AppStore::loadSavedPosts() {
  // TODO: Implement load saved posts
}

void AppStore::loadMoreSavedPosts() {
  // TODO: Implement load more saved posts
}

void AppStore::unsavePost(const juce::String &postId) {
  // TODO: Implement unsave post
}

void AppStore::loadArchivedPosts() {
  // TODO: Implement load archived posts
}

void AppStore::loadMoreArchivedPosts() {
  // TODO: Implement load more archived posts
}

void AppStore::restorePost(const juce::String &postId) {
  // TODO: Implement restore post
}

} // namespace Stores
} // namespace Sidechain
