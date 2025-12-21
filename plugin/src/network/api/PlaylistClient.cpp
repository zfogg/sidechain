// ==============================================================================
// PlaylistClient.cpp - Playlist operations
// Part of NetworkClient implementation split
// ==============================================================================

#include "../../util/Async.h"
#include "../../util/Log.h"
#include "../NetworkClient.h"
#include "Common.h"

using namespace Sidechain::Network::Api;

// ==============================================================================
void NetworkClient::createPlaylist(const juce::String &name, const juce::String &description, bool isCollaborative,
                                   bool isPublic, ResponseCallback callback) {
  if (callback == nullptr)
    return;

  auto *obj = new juce::DynamicObject();
  obj->setProperty("name", name);
  if (description.isNotEmpty())
    obj->setProperty("description", description);
  obj->setProperty("is_collaborative", isCollaborative);
  obj->setProperty("is_public", isPublic);

  post(buildApiPath("/playlists"), juce::var(obj), callback);
}

void NetworkClient::deletePlaylist(const juce::String &playlistId, ResponseCallback callback) {
  if (callback == nullptr)
    return;

  del(buildApiPath("/playlists/") + playlistId, callback);
}

void NetworkClient::getPlaylists(const juce::String &filter, ResponseCallback callback) {
  if (callback == nullptr)
    return;

  juce::String endpoint = buildApiPath("/playlists");
  if (filter.isNotEmpty() && filter != "all")
    endpoint += "?filter=" + filter;

  get(endpoint, callback);
}

void NetworkClient::getPlaylist(const juce::String &playlistId, ResponseCallback callback) {
  if (callback == nullptr)
    return;

  get(buildApiPath("/playlists/") + playlistId, callback);
}

void NetworkClient::addPlaylistEntry(const juce::String &playlistId, const juce::String &postId, int position,
                                     ResponseCallback callback) {
  auto *obj = new juce::DynamicObject();
  obj->setProperty("post_id", postId);
  if (position >= 0)
    obj->setProperty("position", position);

  post(buildApiPath("/playlists/") + playlistId + "/entries", juce::var(obj), callback);
}

void NetworkClient::removePlaylistEntry(const juce::String &playlistId, const juce::String &entryId,
                                        ResponseCallback callback) {
  del(buildApiPath("/playlists/") + playlistId + "/entries/" + entryId, callback);
}

void NetworkClient::addPlaylistCollaborator(const juce::String &playlistId, const juce::String &userId,
                                            const juce::String &role, ResponseCallback callback) {
  auto *obj = new juce::DynamicObject();
  obj->setProperty("user_id", userId);
  obj->setProperty("role", role);

  post(buildApiPath("/playlists/") + playlistId + "/collaborators", juce::var(obj), callback);
}

void NetworkClient::removePlaylistCollaborator(const juce::String &playlistId, const juce::String &userId,
                                               ResponseCallback callback) {
  del(buildApiPath("/playlists/") + playlistId + "/collaborators/" + userId, callback);
}
