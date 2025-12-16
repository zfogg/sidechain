import { apiClient } from './client'
import { Outcome } from './types'

/**
 * DownloadClient - Download audio, MIDI, and project files
 * Mirrors C++ UploadDownloadClient.cpp
 */

export type DownloadType = 'audio' | 'midi' | 'project'

export interface DownloadUrl {
  url: string
  filename: string
  contentType: string
}

interface DownloadResponse {
  url: string
  filename: string
  content_type: string
}

interface DownloadHistoryItem {
  post_id: string
  filename: string
  type: DownloadType
  downloaded_at: string
}

interface DownloadHistoryResponse {
  downloads: DownloadHistoryItem[]
}

export class DownloadClient {
  /**
   * Get download URL for audio file
   */
  static async getAudioDownloadUrl(postId: string): Promise<Outcome<DownloadUrl>> {
    const result = await apiClient.post<DownloadResponse>('/download/audio', {
      post_id: postId,
    })

    if (result.isOk()) {
      try {
        const data = result.getValue()
        return Outcome.ok({
          url: data.url || '',
          filename: data.filename || 'download.mp3',
          contentType: data.content_type || 'audio/mpeg',
        })
      } catch (error) {
        return Outcome.error((error as Error).message)
      }
    }

    return Outcome.error(result.getError())
  }

  /**
   * Get download URL for MIDI file
   */
  static async getMidiDownloadUrl(postId: string): Promise<Outcome<DownloadUrl>> {
    const result = await apiClient.post<DownloadResponse>('/download/midi', {
      post_id: postId,
    })

    if (result.isOk()) {
      try {
        const data = result.getValue()
        return Outcome.ok({
          url: data.url || '',
          filename: data.filename || 'download.mid',
          contentType: data.content_type || 'audio/midi',
        })
      } catch (error) {
        return Outcome.error((error as Error).message)
      }
    }

    return Outcome.error(result.getError())
  }

  /**
   * Get download URL for project file (.als, .flp, .logic, etc.)
   */
  static async getProjectDownloadUrl(postId: string): Promise<Outcome<DownloadUrl>> {
    const result = await apiClient.post<DownloadResponse>('/download/project', {
      post_id: postId,
    })

    if (result.isOk()) {
      try {
        const data = result.getValue()
        return Outcome.ok({
          url: data.url || '',
          filename: data.filename || 'project.zip',
          contentType: data.content_type || 'application/zip',
        })
      } catch (error) {
        return Outcome.error((error as Error).message)
      }
    }

    return Outcome.error(result.getError())
  }

  /**
   * Start a download by opening the URL
   */
  static downloadFile(url: string, filename: string): void {
    const element = document.createElement('a')
    element.setAttribute('href', url)
    element.setAttribute('download', filename)
    element.style.display = 'none'
    document.body.appendChild(element)
    element.click()
    document.body.removeChild(element)
  }

  /**
   * Track that a download was initiated
   */
  static async trackDownload(
    postId: string,
    downloadType: DownloadType
  ): Promise<Outcome<void>> {
    const result = await apiClient.post(`/posts/${postId}/download`, {
      type: downloadType,
    })

    if (result.isOk()) {
      return Outcome.ok(undefined)
    }

    return Outcome.error(result.getError())
  }

  /**
   * Get download history for current user
   */
  static async getDownloadHistory(
    limit: number = 20,
    offset: number = 0
  ): Promise<
    Outcome<
      Array<{
        postId: string
        filename: string
        downloadType: DownloadType
        downloadedAt: string
      }>
    >
  > {
    const result = await apiClient.get<DownloadHistoryResponse>('/downloads/history', {
      limit,
      offset,
    })

    if (result.isOk()) {
      try {
        const data = result.getValue()
        const downloads = (data.downloads || []).map((d) => ({
          postId: d.post_id || '',
          filename: d.filename || '',
          downloadType: (d.type || 'audio') as DownloadType,
          downloadedAt: d.downloaded_at || '',
        }))
        return Outcome.ok(downloads)
      } catch (error) {
        return Outcome.error((error as Error).message)
      }
    }

    return Outcome.error(result.getError())
  }
}
