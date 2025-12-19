import { useEffect, useCallback } from 'react'
import { useNavigate } from 'react-router-dom'

export interface KeyboardShortcut {
  key: string
  ctrl?: boolean
  shift?: boolean
  alt?: boolean
  meta?: boolean
  description: string
  category: 'navigation' | 'interaction' | 'global'
  handler: () => void
}

/**
 * Keyboard shortcuts hook
 * Manages global and context-specific keyboard shortcuts
 */
export function useKeyboardShortcuts() {
  const navigate = useNavigate()

  const shortcuts: KeyboardShortcut[] = [
    // Navigation
    {
      key: 'h',
      description: 'Go to Home Feed',
      category: 'navigation',
      handler: () => navigate('/feed'),
    },
    {
      key: 'u',
      description: 'Go to Upload',
      category: 'navigation',
      handler: () => navigate('/upload'),
    },
    {
      key: 'p',
      description: 'Go to Playlists',
      category: 'navigation',
      handler: () => navigate('/playlists'),
    },
    {
      key: 'm',
      description: 'Go to Messages',
      category: 'navigation',
      handler: () => navigate('/messages'),
    },
    {
      key: 'n',
      description: 'Go to Notifications',
      category: 'navigation',
      handler: () => navigate('/notifications'),
    },
    {
      key: '/',
      description: 'Focus Search',
      category: 'navigation',
      handler: () => {
        const searchInput = document.querySelector('input[placeholder*="Search"]') as HTMLInputElement
        if (searchInput) {
          searchInput.focus()
        } else {
          navigate('/search')
        }
      },
    },

    // Global
    {
      key: '?',
      description: 'Show Keyboard Shortcuts',
      category: 'global',
      handler: () => {
        // Dispatch custom event for shortcuts modal
        window.dispatchEvent(new CustomEvent('show-keyboard-shortcuts'))
      },
    },
    {
      key: 'Escape',
      description: 'Close Dialogs',
      category: 'global',
      handler: () => {
        // Dispatch event to close open dialogs
        window.dispatchEvent(new CustomEvent('close-dialogs'))
      },
    },
  ]

  const handleKeyDown = useCallback(
    (event: KeyboardEvent) => {
      // Don't trigger shortcuts while typing in input/textarea
      const target = event.target as HTMLElement
      if (
        target.tagName === 'INPUT' ||
        target.tagName === 'TEXTAREA' ||
        target.contentEditable === 'true'
      ) {
        // Allow ? and / in inputs
        if (event.key !== '?' && event.key !== '/') {
          return
        }
      }

      // Find matching shortcut
      const shortcut = shortcuts.find(s => {
        const keyMatch = s.key.toLowerCase() === event.key.toLowerCase()
        const ctrlMatch = (s.ctrl ?? false) === (event.ctrlKey || event.metaKey)
        const shiftMatch = (s.shift ?? false) === event.shiftKey
        const altMatch = (s.alt ?? false) === event.altKey

        return keyMatch && ctrlMatch && shiftMatch && altMatch
      })

      if (shortcut) {
        event.preventDefault()
        shortcut.handler()
      }
    },
    [shortcuts, navigate]
  )

  useEffect(() => {
    window.addEventListener('keydown', handleKeyDown)
    return () => window.removeEventListener('keydown', handleKeyDown)
  }, [handleKeyDown])

  return {
    shortcuts,
    getShortcutsByCategory: (category: string) =>
      shortcuts.filter(s => s.category === category),
  }
}
