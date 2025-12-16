import { apiClient } from './client'
import { Outcome } from './types'

/**
 * ChatClient - Handles direct messaging operations
 *
 * Stream Chat React handles most messaging via SDK.
 * This client handles backend-specific operations.
 *
 * API Endpoints:
 * POST /chat/direct/:userId - Create/get direct message channel with user
 * POST /chat/channels/:id/upload - Upload file to channel
 */
export class ChatClient {
  /**
   * Create or get direct message channel with a user
   */
  static async getOrCreateDirectChannel(userId: string): Promise<Outcome<string>> {
    const result = await apiClient.post<{ channel_id: string }>(`/chat/direct/${userId}`, {})

    if (result.isError()) {
      return Outcome.error(result.getError())
    }

    return Outcome.ok(result.getValue().channel_id)
  }

  /**
   * Upload file to channel (returns URL)
   */
  static async uploadFile(channelId: string, file: File): Promise<Outcome<string>> {
    const formData = new FormData()
    formData.append('file', file)

    try {
      const token = localStorage.getItem('auth_token')
      const response = await fetch(
        `${import.meta.env.VITE_API_URL}/chat/channels/${channelId}/upload`,
        {
          method: 'POST',
          body: formData,
          headers: {
            Authorization: `Bearer ${token}`,
          },
        }
      )

      if (!response.ok) {
        return Outcome.error(`Upload failed: ${response.statusText}`)
      }

      const data = await response.json()
      return Outcome.ok(data.url)
    } catch (error: any) {
      return Outcome.error(error.message)
    }
  }

  /**
   * Mark all messages in channel as read
   */
  static async markChannelAsRead(channelId: string): Promise<Outcome<void>> {
    return apiClient.post(`/chat/channels/${channelId}/mark-read`, {})
  }
}
