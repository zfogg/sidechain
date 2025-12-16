/**
 * Image Optimization Utilities
 * Handles lazy loading, responsive images, and image caching
 */

/**
 * Create a responsive image with srcset for multiple resolutions
 * Useful for avatars, thumbnails that need different sizes
 */
export function getResponsiveImageSrcSet(
  baseUrl: string,
  sizes: number[] = [48, 96, 192, 384]
): string {
  return sizes
    .map(size => `${baseUrl}?w=${size} ${size}w`)
    .join(', ')
}

/**
 * Generate CDN URL with optimization parameters
 * Assumes CDN supports URL parameters for image optimization
 */
export function optimizeImageUrl(
  url: string,
  options?: {
    width?: number
    height?: number
    quality?: number
    format?: 'webp' | 'jpg' | 'png'
  }
): string {
  if (!url) return ''

  // Don't optimize data URIs or external URLs from third parties
  if (url.startsWith('data:')) return url
  if (!url.includes(import.meta.env.VITE_CDN_URL || '')) return url

  const params = new URLSearchParams()
  if (options?.width) params.set('w', String(options.width))
  if (options?.height) params.set('h', String(options.height))
  if (options?.quality) params.set('q', String(options.quality))
  if (options?.format) params.set('f', options.format)

  const separator = url.includes('?') ? '&' : '?'
  return `${url}${separator}${params.toString()}`
}

/**
 * Create a placeholder image data URI (1x1 pixel)
 * Used for skeleton screens and low-quality image placeholders
 */
export function createImagePlaceholder(
  color: string = '#1C1C20'
): string {
  const svg = `<svg xmlns="http://www.w3.org/2000/svg" width="1" height="1">
    <rect width="1" height="1" fill="${color}"/>
  </svg>`

  return `data:image/svg+xml;base64,${btoa(svg)}`
}

/**
 * Preload an image to cache it and get dimensions
 * Returns promise with success status
 */
export function preloadImage(url: string): Promise<HTMLImageElement> {
  return new Promise((resolve, reject) => {
    const img = new Image()
    img.onload = () => resolve(img)
    img.onerror = reject
    img.src = url
  })
}

/**
 * Setup intersection observer for lazy-loaded images
 * Replaces data-src with src when element becomes visible
 */
export function observeLazyImages(root?: Element): IntersectionObserver {
  return new IntersectionObserver(
    (entries) => {
      entries.forEach(entry => {
        if (entry.isIntersecting) {
          const img = entry.target as HTMLImageElement
          const dataSrc = img.dataset.src

          if (dataSrc && !img.src) {
            img.src = dataSrc
            img.removeAttribute('data-src')
            // Optionally add loaded animation
            img.classList.add('loaded')
          }

          observer.unobserve(img)
        }
      })
    },
    {
      root,
      rootMargin: '50px', // Start loading 50px before visible
    }
  )
}

/**
 * Image cache manager using IndexedDB for offline support
 * Stores recently viewed images to reduce network requests
 */
export class ImageCache {
  private dbName = 'SidechainImageCache'
  private storeName = 'images'
  private db: IDBDatabase | null = null

  async init(): Promise<void> {
    return new Promise((resolve, reject) => {
      const request = indexedDB.open(this.dbName, 1)

      request.onerror = () => reject(request.error)
      request.onsuccess = () => {
        this.db = request.result
        resolve()
      }

      request.onupgradeneeded = (event) => {
        const db = (event.target as IDBOpenDBRequest).result
        if (!db.objectStoreNames.contains(this.storeName)) {
          db.createObjectStore(this.storeName, { keyPath: 'url' })
        }
      }
    })
  }

  async set(url: string, data: Blob, expiresIn: number = 86400000): Promise<void> {
    if (!this.db) return

    return new Promise((resolve, reject) => {
      const transaction = this.db!.transaction([this.storeName], 'readwrite')
      const store = transaction.objectStore(this.storeName)

      store.put({
        url,
        data,
        timestamp: Date.now(),
        expiresAt: Date.now() + expiresIn,
      })

      transaction.onerror = () => reject(transaction.error)
      transaction.oncomplete = () => resolve()
    })
  }

  async get(url: string): Promise<Blob | null> {
    if (!this.db) return null

    return new Promise((resolve, reject) => {
      const transaction = this.db!.transaction([this.storeName], 'readonly')
      const store = transaction.objectStore(this.storeName)
      const request = store.get(url)

      request.onerror = () => reject(request.error)
      request.onsuccess = () => {
        const result = request.result

        if (!result) {
          resolve(null)
          return
        }

        // Check if cached item has expired
        if (result.expiresAt < Date.now()) {
          // Delete expired entry in background
          this.delete(url)
          resolve(null)
        } else {
          resolve(result.data)
        }
      }
    })
  }

  async delete(url: string): Promise<void> {
    if (!this.db) return

    return new Promise((resolve, reject) => {
      const transaction = this.db!.transaction([this.storeName], 'readwrite')
      const store = transaction.objectStore(this.storeName)

      store.delete(url)

      transaction.onerror = () => reject(transaction.error)
      transaction.oncomplete = () => resolve()
    })
  }

  async clear(): Promise<void> {
    if (!this.db) return

    return new Promise((resolve, reject) => {
      const transaction = this.db!.transaction([this.storeName], 'readwrite')
      const store = transaction.objectStore(this.storeName)

      store.clear()

      transaction.onerror = () => reject(transaction.error)
      transaction.oncomplete = () => resolve()
    })
  }
}

// Singleton instance
let imageCache: ImageCache | null = null

export async function getImageCache(): Promise<ImageCache> {
  if (!imageCache) {
    imageCache = new ImageCache()
    await imageCache.init()
  }
  return imageCache
}
