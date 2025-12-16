import { useEffect, useState } from 'react'
import {
  Dialog,
  DialogContent,
  DialogHeader,
  DialogTitle,
  DialogDescription,
} from '@/components/ui/dialog'
import { useKeyboardShortcuts } from '@/hooks/useKeyboardShortcuts'

/**
 * KeyboardShortcutsDialog - Display all available keyboard shortcuts
 */
export function KeyboardShortcutsDialog() {
  const [isOpen, setIsOpen] = useState(false)
  const { getShortcutsByCategory } = useKeyboardShortcuts()

  useEffect(() => {
    const handleShowShortcuts = () => setIsOpen(true)
    const handleCloseDialogs = () => setIsOpen(false)

    window.addEventListener('show-keyboard-shortcuts', handleShowShortcuts)
    window.addEventListener('close-dialogs', handleCloseDialogs)

    return () => {
      window.removeEventListener('show-keyboard-shortcuts', handleShowShortcuts)
      window.removeEventListener('close-dialogs', handleCloseDialogs)
    }
  }, [])

  const navigationShortcuts = getShortcutsByCategory('navigation')
  const globalShortcuts = getShortcutsByCategory('global')

  return (
    <Dialog open={isOpen} onOpenChange={setIsOpen}>
      <DialogContent className="max-w-2xl">
        <DialogHeader>
          <DialogTitle className="flex items-center gap-2">
            <span>⌨️</span>
            Keyboard Shortcuts
          </DialogTitle>
          <DialogDescription>
            Quick reference for keyboard shortcuts available in Sidechain
          </DialogDescription>
        </DialogHeader>

        <div className="space-y-6 max-h-96 overflow-y-auto">
          {/* Navigation Shortcuts */}
          <div>
            <h3 className="text-sm font-semibold text-foreground mb-3 uppercase tracking-wide">
              🧭 Navigation
            </h3>
            <div className="space-y-2">
              {navigationShortcuts.map(shortcut => (
                <div key={shortcut.key} className="flex items-center justify-between text-sm">
                  <span className="text-muted-foreground">{shortcut.description}</span>
                  <kbd className="px-2 py-1 bg-muted border border-border rounded text-foreground font-mono text-xs">
                    {shortcut.key}
                  </kbd>
                </div>
              ))}
            </div>
          </div>

          {/* Global Shortcuts */}
          <div>
            <h3 className="text-sm font-semibold text-foreground mb-3 uppercase tracking-wide">
              🌐 Global
            </h3>
            <div className="space-y-2">
              {globalShortcuts.map(shortcut => (
                <div key={shortcut.key} className="flex items-center justify-between text-sm">
                  <span className="text-muted-foreground">{shortcut.description}</span>
                  <kbd className="px-2 py-1 bg-muted border border-border rounded text-foreground font-mono text-xs">
                    {shortcut.key}
                  </kbd>
                </div>
              ))}
            </div>
          </div>

          {/* Tips */}
          <div className="bg-muted/50 border border-border rounded p-3 text-sm">
            <p className="text-muted-foreground">
              <span className="font-semibold">💡 Tip:</span> Press{' '}
              <kbd className="px-1 py-0.5 bg-muted border border-border rounded text-foreground font-mono text-xs">
                ?
              </kbd>{' '}
              anytime to open this dialog.
            </p>
          </div>
        </div>
      </DialogContent>
    </Dialog>
  )
}
