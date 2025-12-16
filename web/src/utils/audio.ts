import { Howl } from 'howler'

/**
 * Audio playback utilities using Howler.js
 * Provides a simple interface for playing audio files with controls
 */

export interface AudioPlayer {
  play: () => void
  pause: () => void
  stop: () => void
  seek: (time: number) => void
  setVolume: (volume: number) => void
  destroy: () => void
  isPlaying: () => boolean
  getDuration: () => number
  getProgress: () => number
}

/**
 * Create an audio player for a URL
 * Handles audio format compatibility and error handling
 */
export function createAudioPlayer(url: string, onEnd?: () => void): AudioPlayer {
  let howl: Howl | null = null

  const getHowl = (): Howl => {
    if (!howl) {
      howl = new Howl({
        src: [url],
        format: ['mp3', 'wav'],
        preload: true,
        onend: () => {
          onEnd?.()
        },
        onload: () => {
          console.log('[Audio] Loaded:', url)
        },
        onloaderror: () => {
          console.error('[Audio] Error loading:', url)
        },
      } as any)
    }
    return howl
  }

  return {
    play: () => {
      getHowl().play()
    },

    pause: () => {
      getHowl().pause()
    },

    stop: () => {
      const h = getHowl()
      h.stop()
      h.seek(0)
    },

    seek: (time: number) => {
      getHowl().seek(time)
    },

    setVolume: (volume: number) => {
      getHowl().volume(Math.max(0, Math.min(1, volume)))
    },

    destroy: () => {
      if (howl) {
        howl.unload()
        howl = null
      }
    },

    isPlaying: () => {
      return howl?.playing() || false
    },

    getDuration: () => {
      return howl?.duration() || 0
    },

    getProgress: () => {
      const howlInstance = howl
      if (!howlInstance) return 0
      const duration = howlInstance.duration()
      const seek = howlInstance.seek() as number
      return duration > 0 ? seek / duration : 0
    },
  }
}

/**
 * Format time in seconds to MM:SS
 */
export function formatTime(seconds: number): string {
  if (!seconds || seconds < 0) return '0:00'

  const mins = Math.floor(seconds / 60)
  const secs = Math.floor(seconds % 60)
  return `${mins}:${secs.toString().padStart(2, '0')}`
}

/**
 * Parse genre array to readable string
 */
export function formatGenres(genres: string[]): string {
  if (!genres || genres.length === 0) return 'Unknown'
  return genres.slice(0, 3).join(', ')
}
