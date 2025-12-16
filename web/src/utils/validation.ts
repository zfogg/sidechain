/**
 * Validation utilities for forms and data
 */

/**
 * Validate email format
 */
export function isValidEmail(email: string): boolean {
  const emailRegex = /^[^\s@]+@[^\s@]+\.[^\s@]+$/
  return emailRegex.test(email)
}

/**
 * Validate username (3-32 chars, alphanumeric + underscore)
 */
export function isValidUsername(username: string): boolean {
  const usernameRegex = /^[a-zA-Z0-9_]{3,32}$/
  return usernameRegex.test(username)
}

/**
 * Validate password (min 8 chars)
 */
export function isValidPassword(password: string): boolean {
  return password.length >= 8
}

/**
 * Validate URL
 */
export function isValidUrl(url: string): boolean {
  try {
    new URL(url)
    return true
  } catch {
    return false
  }
}

/**
 * Validate audio file
 */
export function isValidAudioFile(file: File): boolean {
  const audioMimes = ['audio/mpeg', 'audio/wav', 'audio/ogg', 'audio/webm']
  return audioMimes.includes(file.type)
}

/**
 * Validate image file
 */
export function isValidImageFile(file: File): boolean {
  const imageMimes = ['image/jpeg', 'image/png', 'image/webp', 'image/gif']
  return imageMimes.includes(file.type)
}

/**
 * Check if user is online (simple check - can be enhanced)
 */
export function isUserOnline(lastSeen: Date): boolean {
  const fiveMinutesAgo = new Date(Date.now() - 5 * 60 * 1000)
  return lastSeen > fiveMinutesAgo
}

/**
 * Validate BPM
 */
export function isValidBPM(bpm: number): boolean {
  return bpm >= 20 && bpm <= 300
}

/**
 * Validate musical key
 */
export function isValidKey(key: string): boolean {
  const validKeys = [
    'C', 'C#', 'Db', 'D', 'D#', 'Eb', 'E', 'F',
    'F#', 'Gb', 'G', 'G#', 'Ab', 'A', 'A#', 'Bb', 'B',
    'Cm', 'C#m', 'Dbm', 'Dm', 'D#m', 'Ebm', 'Em', 'Fm',
    'F#m', 'Gbm', 'Gm', 'G#m', 'Abm', 'Am', 'A#m', 'Bbm', 'Bm',
  ]
  return validKeys.includes(key)
}

/**
 * Validate genre
 */
export function isValidGenre(genre: string): boolean {
  const validGenres = [
    'Electronic', 'House', 'Techno', 'Trance', 'Ambient',
    'Hip-Hop', 'Rap', 'R&B', 'Soul', 'Jazz',
    'Rock', 'Pop', 'Indie', 'Alternative', 'Punk',
    'Classical', 'Orchestral', 'Ambient', 'Experimental',
    'Drum and Bass', 'Dubstep', 'Trap', 'Future Bass',
  ]
  return validGenres.includes(genre)
}
