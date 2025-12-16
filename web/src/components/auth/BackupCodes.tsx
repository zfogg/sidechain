import { useState } from 'react'
import { Button } from '@/components/ui/button'
import { Dialog, DialogContent, DialogDescription, DialogHeader, DialogTitle } from '@/components/ui/dialog'

interface BackupCodesProps {
  backupCodes: string[]
  isOpen: boolean
  onClose: () => void
}

export function BackupCodes({ backupCodes, isOpen, onClose }: BackupCodesProps) {
  const [hasCopied, setHasCopied] = useState(false)
  const [hasDownloaded, setHasDownloaded] = useState(false)

  const handleCopyAll = () => {
    const text = backupCodes.join('\n')
    navigator.clipboard.writeText(text)
    setHasCopied(true)
    setTimeout(() => setHasCopied(false), 2000)
  }

  const handleDownload = () => {
    const text = `Sidechain Two-Factor Authentication Backup Codes\n\nSave these codes in a secure location. Each code can be used once to log in if you lose access to your authenticator.\n\n${backupCodes.map((code, i) => `${i + 1}. ${code}`).join('\n')}`

    const element = document.createElement('a')
    element.setAttribute('href', `data:text/plain;charset=utf-8,${encodeURIComponent(text)}`)
    element.setAttribute('download', 'sidechain-backup-codes.txt')
    element.style.display = 'none'
    document.body.appendChild(element)
    element.click()
    document.body.removeChild(element)

    setHasDownloaded(true)
    setTimeout(() => setHasDownloaded(false), 2000)
  }

  return (
    <Dialog open={isOpen} onOpenChange={onClose}>
      <DialogContent className="sm:max-w-md">
        <DialogHeader>
          <DialogTitle>Backup Codes</DialogTitle>
          <DialogDescription>
            Save these codes in a secure location. Each can be used once to log in.
          </DialogDescription>
        </DialogHeader>

        <div className="space-y-4 py-4">
          <div className="space-y-2">
            {backupCodes.map((code, i) => (
              <div
                key={i}
                className="flex items-center justify-between p-3 bg-muted rounded font-mono text-sm"
              >
                <span>{i + 1}.</span>
                <span className="tracking-wider">{code}</span>
              </div>
            ))}
          </div>

          <div className="flex gap-2">
            <Button
              variant="outline"
              onClick={handleCopyAll}
              className="flex-1"
              disabled={hasCopied}
            >
              {hasCopied ? '✓ Copied' : 'Copy All'}
            </Button>
            <Button
              variant="outline"
              onClick={handleDownload}
              className="flex-1"
              disabled={hasDownloaded}
            >
              {hasDownloaded ? '✓ Downloaded' : 'Download'}
            </Button>
          </div>

          <Button onClick={onClose} className="w-full">
            Done
          </Button>
        </div>
      </DialogContent>
    </Dialog>
  )
}
