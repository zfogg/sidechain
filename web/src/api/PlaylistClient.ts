/**
 * PlaylistClient - API client for playlist operations
 * Handles CRUD operations for playlists
 */

import { apiClient } from './client'
import { Outcome } from './types'
import { Playlist, PlaylistModel } from '@/models/Playlist'

export class PlaylistClient {
  /**
   * Get all playlists for current user
   */
  static async getUserPlaylists(limit: number = 20, offset: number = 0): Promise<Outcome<Playlist[]>> {
    const result = await apiClient.get<{ playlists: any[] }>('/playlists', { limit, offset })

    if (result.isOk()) {
      const playlists = result.getValue().playlists.map(PlaylistModel.fromJson)
      return Outcome.ok(playlists)
    }

    return Outcome.error(result.getError())
  }

  /**
   * Get public playlists (discovery/feed)
   */
  static async getPublicPlaylists(limit: number = 20, offset: number = 0): Promise<Outcome<Playlist[]>> {
    const result = await apiClient.get<{ playlists: any[] }>('/playlists/public', { limit, offset })

    if (result.isOk()) {
      const playlists = result.getValue().playlists.map(PlaylistModel.fromJson)
      return Outcome.ok(playlists)
    }

    return Outcome.error(result.getError())
  }

  /**
   * Get playlist by ID
   */
  static async getPlaylist(playlistId: string): Promise<Outcome<Playlist>> {
    const result = await apiClient.get<{ playlist: any }>(`/playlists/${playlistId}`)

    if (result.isOk()) {
      const playlist = PlaylistModel.fromJson(result.getValue().playlist)
      return Outcome.ok(playlist)
    }

    return Outcome.error(result.getError())
  }

  /**
   * Get playlists for a specific user
   */
  static async getUserPlaylistsByUsername(username: string, limit: number = 20, offset: number = 0): Promise<Outcome<Playlist[]>> {
    const result = await apiClient.get<{ playlists: any[] }>(`/users/${username}/playlists`, {
      limit,
      offset,
    })

    if (result.isOk()) {
      const playlists = result.getValue().playlists.map(PlaylistModel.fromJson)
      return Outcome.ok(playlists)
    }

    return Outcome.error(result.getError())
  }

  /**
   * Create new playlist
   */
  static async createPlaylist(title: string, description: string = '', isPublic: boolean = false): Promise<Outcome<Playlist>> {
    const result = await apiClient.post<{ playlist: any }>('/playlists', {
      title,
      description,
      is_public: isPublic,
    })

    if (result.isOk()) {
      const playlist = PlaylistModel.fromJson(result.getValue().playlist)
      return Outcome.ok(playlist)
    }

    return Outcome.error(result.getError())
  }

  /**
   * Update playlist metadata
   */
  static async updatePlaylist(
    playlistId: string,
    updates: {
      title?: string
      description?: string
      isPublic?: boolean
      isCollaborative?: boolean
      coverUrl?: string
    }
  ): Promise<Outcome<Playlist>> {
    const payload: any = {}
    if (updates.title) payload.title = updates.title
    if (updates.description) payload.description = updates.description
    if (updates.isPublic !== undefined) payload.is_public = updates.isPublic
    if (updates.isCollaborative !== undefined) payload.is_collaborative = updates.isCollaborative
    if (updates.coverUrl) payload.cover_url = updates.coverUrl

    const result = await apiClient.put<{ playlist: any }>(`/playlists/${playlistId}`, payload)

    if (result.isOk()) {
      const playlist = PlaylistModel.fromJson(result.getValue().playlist)
      return Outcome.ok(playlist)
    }

    return Outcome.error(result.getError())
  }

  /**
   * Delete playlist
   */
  static async deletePlaylist(playlistId: string): Promise<Outcome<void>> {
    return apiClient.delete(`/playlists/${playlistId}`)
  }

  /**
   * Add track to playlist
   */
  static async addTrackToPlaylist(playlistId: string, postId: string): Promise<Outcome<Playlist>> {
    const result = await apiClient.post<{ playlist: any }>(`/playlists/${playlistId}/tracks`, {
      post_id: postId,
    })

    if (result.isOk()) {
      const playlist = PlaylistModel.fromJson(result.getValue().playlist)
      return Outcome.ok(playlist)
    }

    return Outcome.error(result.getError())
  }

  /**
   * Remove track from playlist
   */
  static async removeTrackFromPlaylist(playlistId: string, postId: string): Promise<Outcome<Playlist>> {
    const result = await apiClient.delete<{ playlist: any }>(`/playlists/${playlistId}/tracks?post_id=${postId}`)

    if (result.isOk()) {
      const playlist = PlaylistModel.fromJson(result.getValue().playlist)
      return Outcome.ok(playlist)
    }

    return Outcome.error(result.getError())
  }

  /**
   * Reorder tracks in playlist
   */
  static async reorderTracks(playlistId: string, trackIds: string[]): Promise<Outcome<Playlist>> {
    const result = await apiClient.put<{ playlist: any }>(`/playlists/${playlistId}/reorder`, {
      track_ids: trackIds,
    })

    if (result.isOk()) {
      const playlist = PlaylistModel.fromJson(result.getValue().playlist)
      return Outcome.ok(playlist)
    }

    return Outcome.error(result.getError())
  }

  /**
   * Follow/unfollow playlist
   */
  static async toggleFollowPlaylist(playlistId: string, shouldFollow: boolean): Promise<Outcome<void>> {
    if (shouldFollow) {
      return apiClient.post(`/playlists/${playlistId}/follow`, {})
    } else {
      return apiClient.delete(`/playlists/${playlistId}/follow`)
    }
  }

  /**
   * Get playlists that contain a specific track
   */
  static async getPlaylistsContainingTrack(postId: string): Promise<Outcome<Playlist[]>> {
    const result = await apiClient.get<{ playlists: any[] }>(`/playlists/containing/${postId}`)

    if (result.isOk()) {
      const playlists = result.getValue().playlists.map(PlaylistModel.fromJson)
      return Outcome.ok(playlists)
    }

    return Outcome.error(result.getError())
  }
}
