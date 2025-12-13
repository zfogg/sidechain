//==============================================================================
// PlaylistClient.cpp - Playlist operations
// Part of NetworkClient implementation split
//==============================================================================

#include "../NetworkClient.h"
#include "../../util/Log.h"
#include "../../util/Async.h"

//==============================================================================
void NetworkClient::createPlaylist(const juce::String& name,
                                   const juce::String& description,
                                   bool isCollaborative,
                                   bool isPublic,
                                   ResponseCallback callback)
{
    if (callback == nullptr)
        return;

    auto* obj = new juce::DynamicObject();
    obj->setProperty("name", name);
    if (description.isNotEmpty())
        obj->setProperty("description", description);
    obj->setProperty("is_collaborative", isCollaborative);
    obj->setProperty("is_public", isPublic);

    post("/api/v1/playlists", juce::var(obj), callback);
}

void NetworkClient::getPlaylists(const juce::String& filter, ResponseCallback callback)
{
    if (callback == nullptr)
        return;

    juce::String endpoint = "/api/v1/playlists";
    if (filter.isNotEmpty() && filter != "all")
        endpoint += "?filter=" + filter;

    get(endpoint, callback);
}

void NetworkClient::getPlaylist(const juce::String& playlistId, ResponseCallback callback)
{
    if (callback == nullptr)
        return;

    get("/api/v1/playlists/" + playlistId, callback);
}

void NetworkClient::addPlaylistEntry(const juce::String& playlistId,
                                     const juce::String& postId,
                                     int position,
                                     ResponseCallback callback)
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty("post_id", postId);
    if (position >= 0)
        obj->setProperty("position", position);

    post("/api/v1/playlists/" + playlistId + "/entries", juce::var(obj), callback);
}

void NetworkClient::removePlaylistEntry(const juce::String& playlistId,
                                        const juce::String& entryId,
                                        ResponseCallback callback)
{
    del("/api/v1/playlists/" + playlistId + "/entries/" + entryId, callback);
}

void NetworkClient::addPlaylistCollaborator(const juce::String& playlistId,
                                            const juce::String& userId,
                                            const juce::String& role,
                                            ResponseCallback callback)
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty("user_id", userId);
    obj->setProperty("role", role);

    post("/api/v1/playlists/" + playlistId + "/collaborators", juce::var(obj), callback);
}

void NetworkClient::removePlaylistCollaborator(const juce::String& playlistId,
                                               const juce::String& userId,
                                               ResponseCallback callback)
{
    del("/api/v1/playlists/" + playlistId + "/collaborators/" + userId, callback);
}
