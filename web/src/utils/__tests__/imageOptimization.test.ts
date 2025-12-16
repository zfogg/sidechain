import { describe, it, expect, vi } from 'vitest'
import {
  getResponsiveImageSrcSet,
  optimizeImageUrl,
  createImagePlaceholder,
  preloadImage,
} from '../imageOptimization'

describe('imageOptimization', () => {
  describe('getResponsiveImageSrcSet', () => {
    it('should generate responsive srcset with default sizes', () => {
      const baseUrl = 'https://cdn.example.com/image.jpg'
      const srcset = getResponsiveImageSrcSet(baseUrl)

      expect(srcset).toContain('48w')
      expect(srcset).toContain('96w')
      expect(srcset).toContain('192w')
      expect(srcset).toContain('384w')
    })

    it('should generate responsive srcset with custom sizes', () => {
      const baseUrl = 'https://cdn.example.com/image.jpg'
      const srcset = getResponsiveImageSrcSet(baseUrl, [100, 200, 300])

      expect(srcset).toContain('100w')
      expect(srcset).toContain('200w')
      expect(srcset).toContain('300w')
      expect(srcset).not.toContain('48w')
    })

    it('should include width parameters in URLs', () => {
      const baseUrl = 'https://cdn.example.com/image.jpg'
      const srcset = getResponsiveImageSrcSet(baseUrl, [100])

      expect(srcset).toContain('w=100')
    })
  })

  describe('optimizeImageUrl', () => {
    it('should return empty string for empty URL', () => {
      const result = optimizeImageUrl('')
      expect(result).toBe('')
    })

    it('should not modify data URIs', () => {
      const dataUri = 'data:image/svg+xml;base64,abcd'
      const result = optimizeImageUrl(dataUri)
      expect(result).toBe(dataUri)
    })

    it('should add width parameter', () => {
      const url = 'https://cdn.example.com/image.jpg'
      const result = optimizeImageUrl(url, { width: 200 })
      expect(result).toContain('w=200')
    })

    it('should add quality parameter', () => {
      const url = 'https://cdn.example.com/image.jpg'
      const result = optimizeImageUrl(url, { quality: 80 })
      expect(result).toContain('q=80')
    })

    it('should add format parameter', () => {
      const url = 'https://cdn.example.com/image.jpg'
      const result = optimizeImageUrl(url, { format: 'webp' })
      expect(result).toContain('f=webp')
    })

    it('should combine multiple parameters', () => {
      const url = 'https://cdn.example.com/image.jpg'
      const result = optimizeImageUrl(url, {
        width: 100,
        height: 100,
        quality: 85,
        format: 'webp',
      })

      expect(result).toContain('w=100')
      expect(result).toContain('h=100')
      expect(result).toContain('q=85')
      expect(result).toContain('f=webp')
    })

    it('should handle URLs that already have query parameters', () => {
      const url = 'https://cdn.example.com/image.jpg?existing=param'
      const result = optimizeImageUrl(url, { width: 100 })
      expect(result).toContain('existing=param')
      expect(result).toContain('w=100')
    })
  })

  describe('createImagePlaceholder', () => {
    it('should create valid data URI', () => {
      const placeholder = createImagePlaceholder()
      expect(placeholder).toMatch(/^data:image\/svg\+xml;base64,/)
    })

    it('should use provided color', () => {
      const placeholder = createImagePlaceholder('#FF0000')
      expect(placeholder).toContain('fill=%22%23FF0000%22')
    })

    it('should use default color if not provided', () => {
      const placeholder = createImagePlaceholder()
      expect(placeholder).toBeTruthy()
    })

    it('should be decodable as base64', () => {
      const placeholder = createImagePlaceholder('#000000')
      const base64 = placeholder.split(',')[1]
      const decoded = Buffer.from(base64, 'base64').toString('utf-8')
      expect(decoded).toContain('svg')
      expect(decoded).toContain('#000000')
    })
  })

  describe('preloadImage', () => {
    it('should resolve when image loads', async () => {
      // Mock Image constructor
      const mockImage = {
        onload: null as (() => void) | null,
        onerror: null as (() => void) | null,
        src: '',
      }

      const ImageMock = vi.fn(() => mockImage)
      global.Image = ImageMock as any

      const promise = preloadImage('https://example.com/image.jpg')

      // Simulate image load
      if (mockImage.onload) {
        mockImage.onload()
      }

      const result = await promise
      expect(result).toBe(mockImage)
    })

    it('should reject when image fails to load', async () => {
      const mockImage = {
        onload: null as (() => void) | null,
        onerror: null as (() => void) | null,
        src: '',
      }

      const ImageMock = vi.fn(() => mockImage)
      global.Image = ImageMock as any

      const promise = preloadImage('https://example.com/invalid.jpg')

      // Simulate image error
      if (mockImage.onerror) {
        mockImage.onerror()
      }

      await expect(promise).rejects.toThrow()
    })

    it('should set image src', async () => {
      const mockImage = {
        onload: null as (() => void) | null,
        onerror: null as (() => void) | null,
        src: '',
      }

      const ImageMock = vi.fn(() => mockImage)
      global.Image = ImageMock as any

      const imageUrl = 'https://example.com/image.jpg'
      preloadImage(imageUrl)

      expect(mockImage.src).toBe(imageUrl)
    })
  })
})
