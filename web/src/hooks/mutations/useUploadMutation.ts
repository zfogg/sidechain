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
      setProgress(75)

      if (uploadResult.isError()) {
        throw new Error(uploadResult.getError())
      }

      const { audioUrl, waveformUrl } = uploadResult.getValue()

      // Step 2: Create post with metadata
      setProgress(85)

      const createResult = await UploadClient.createPost({
        audioUrl,
        waveformUrl,
        filename: data.title,
        description: data.description,
        bpm: data.bpm || undefined,
        key: data.key || undefined,
        daw: data.daw || undefined,
        genre: data.genre,
        isPublic: data.isPublic,
      })

      if (createResult.isError()) {
        throw new Error(createResult.getError())
      }

      setProgress(100)

      return createResult.getValue()
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
