import { describe, it, expect, vi } from 'vitest'
import { screen } from '@testing-library/react'
import { Avatar } from '../avatar'
import { renderWithProviders } from '@/test/utils'

describe('Avatar Component', () => {
  it('should render avatar with image', () => {
    renderWithProviders(
      <Avatar
        src="https://example.com/avatar.jpg"
        alt="Test User"
        size="md"
      />
    )

    const img = screen.getByAltText('Test User')
    expect(img).toBeInTheDocument()
    expect(img).toHaveAttribute('src', 'https://example.com/avatar.jpg')
  })

  it('should render with lazy loading by default', () => {
    renderWithProviders(
      <Avatar
        src="https://example.com/avatar.jpg"
        alt="Test User"
      />
    )

    const img = screen.getByAltText('Test User')
    expect(img).toHaveAttribute('loading', 'lazy')
  })

  it('should render with eager loading when lazy is false', () => {
    renderWithProviders(
      <Avatar
        src="https://example.com/avatar.jpg"
        alt="Test User"
        lazy={false}
      />
    )

    const img = screen.getByAltText('Test User')
    expect(img).toHaveAttribute('loading', 'eager')
  })

  describe('sizes', () => {
    it('should render small avatar', () => {
      const { container } = renderWithProviders(
        <Avatar
          src="https://example.com/avatar.jpg"
          alt="Test User"
          size="sm"
        />
      )

      const div = container.querySelector('.w-8')
      expect(div).toBeInTheDocument()
    })

    it('should render medium avatar', () => {
      const { container } = renderWithProviders(
        <Avatar
          src="https://example.com/avatar.jpg"
          alt="Test User"
          size="md"
        />
      )

      const div = container.querySelector('.w-10')
      expect(div).toBeInTheDocument()
    })

    it('should render large avatar', () => {
      const { container } = renderWithProviders(
        <Avatar
          src="https://example.com/avatar.jpg"
          alt="Test User"
          size="lg"
        />
      )

      const div = container.querySelector('.w-16')
      expect(div).toBeInTheDocument()
    })
  })

  it('should apply custom className', () => {
    const { container } = renderWithProviders(
      <Avatar
        src="https://example.com/avatar.jpg"
        alt="Test User"
        className="custom-class"
      />
    )

    const div = container.querySelector('.custom-class')
    expect(div).toBeInTheDocument()
  })

  it('should show placeholder on image load error', () => {
    const { container } = renderWithProviders(
      <Avatar
        src="https://example.com/broken.jpg"
        alt="Test User"
      />
    )

    const img = screen.getByAltText('Test User')

    // Simulate image load error
    const event = new Event('error')
    img.dispatchEvent(event)

    // Check that placeholder was set (it's a data URI)
    expect(img.getAttribute('src')).toContain('data:image/svg+xml')
  })

  it('should handle missing src gracefully', () => {
    renderWithProviders(
      <Avatar
        alt="Test User"
        size="md"
      />
    )

    const img = screen.getByAltText('Test User')
    expect(img).toBeInTheDocument()
    // Should use placeholder
    expect(img.getAttribute('src')).toContain('data:')
  })

  it('should add animate-pulse class while loading', () => {
    const { container } = renderWithProviders(
      <Avatar
        src="https://example.com/avatar.jpg"
        alt="Test User"
      />
    )

    const img = screen.getByAltText('Test User') as HTMLImageElement

    // Initially should be loading
    expect(img.className).toContain('animate-pulse')

    // Simulate load
    const event = new Event('load')
    img.dispatchEvent(event)

    // Should remove animate-pulse
    expect(img.className).not.toContain('animate-pulse')
  })

  it('should be rounded circular', () => {
    const { container } = renderWithProviders(
      <Avatar
        src="https://example.com/avatar.jpg"
        alt="Test User"
      />
    )

    const div = container.querySelector('.rounded-full')
    expect(div).toBeInTheDocument()
  })
})
