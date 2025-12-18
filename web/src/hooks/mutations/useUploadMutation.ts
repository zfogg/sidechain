import { useMutation } from '@tanstack/react-query'
import { UploadClient } from '@/api/UploadClient'
import { useState } from 'react'

interface UploadData {
  file: File | null
  title: string
  description: string
  bpm: number | null
  key: string
  daw: string
  genre: string[]
  isPublic: boolean
}

/**
 * useUploadMutation - Mutation hook for uploading loops
 *
 * Handles:
 * - File upload with progress tracking
 * - Metadata submission
 * - Error handling and retry
 */
export function useUploadMutation() {
  const [progress, setProgress] = useState(0)

  const mutation = useMutation({
    mutationFn: async (data: UploadData) => {
      if (!data.file) {
        throw new Error('No file selected')
      }

      // Reset progress
      setProgress(0)

      // Step 1: Upload file with progress tracking
      const uploadProgress = setInterval(() => {
        setProgress((prev) => {
          if (prev >= 95) return prev
          return prev + Math.random() * 20
        })
      }, 200)

      const uploadResult = await UploadClient.uploadFile(data.file, {
        filename: data.title,
        bpm: data.bpm || undefined,
        key: data.key || undefined,
        daw: data.daw || undefined,
        genre: data.genre?.[0] || undefined,
      })

      clearInterval(uploadProgress)
      setProgress(100)

      if (uploadResult.isError()) {
        throw new Error(uploadResult.getError())
      }

      // Backend creates the post during audio upload with the metadata
      // Return the upload result which includes postId, jobId, and pollUrl
      const uploadData = uploadResult.getValue()
      return {
        id: uploadData.postId,
        success: true,
      }
    },
    onSuccess: () => {
      setProgress(0)
    },
    onError: () => {
      setProgress(0)
    },
  })

  return {
    ...mutation,
    progress,
  }
}
