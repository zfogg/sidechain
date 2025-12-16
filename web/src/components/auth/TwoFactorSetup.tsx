import { useState } from 'react'
import { AuthClient } from '@/api/AuthClient'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import { Dialog, DialogContent, DialogDescription, DialogHeader, DialogTitle } from '@/components/ui/dialog'
import { Spinner } from '@/components/ui/spinner'

interface TwoFactorSetupProps {
  isOpen: boolean
  onClose: () => void
  onSuccess: (backupCodes: string[]) => void
}

export function TwoFactorSetup({ isOpen, onClose, onSuccess }: TwoFactorSetupProps) {
  const [step, setStep] = useState<'qr' | 'verify'>('qr')
  const [isLoading, setIsLoading] = useState(false)
  const [error, setError] = useState('')
  const [qrCode, setQrCode] = useState('')
  const [secret, setSecret] = useState('')
  const [_backupCodes, setBackupCodes] = useState<string[]>([])
  const [totpCode, setTotpCode] = useState('')
  const [copied, setCopied] = useState(false)

  // Initialize 2FA setup
  const handleStartSetup = async () => {
    setIsLoading(true)
    setError('')

    const result = await AuthClient.setupTwoFactor()

    if (result.isOk()) {
      const data = result.getValue()
      setQrCode(data.qrCode)
      setSecret(data.secret)
      setBackupCodes(data.backupCodes)
      setStep('verify')
    } else {
      setError(result.getError())
    }

    setIsLoading(false)
  }

  // Verify TOTP code
  const handleVerify = async () => {
    if (totpCode.length !== 6) {
      setError('TOTP code must be 6 digits')
      return
    }

    setIsLoading(true)
    setError('')

    const result = await AuthClient.verifyTwoFactor(totpCode)

    if (result.isOk()) {
      onSuccess(result.getValue().backupCodes)
      handleClose()
    } else {
      setError(result.getError())
    }

    setIsLoading(false)
  }

  const handleClose = () => {
    setStep('qr')
    setTotpCode('')
    setQrCode('')
    setSecret('')
    setBackupCodes([])
    setError('')
    onClose()
  }

  const handleCopySecret = () => {
    navigator.clipboard.writeText(secret)
    setCopied(true)
    setTimeout(() => setCopied(false), 2000)
  }

  return (
    <Dialog open={isOpen} onOpenChange={handleClose}>
      <DialogContent className="sm:max-w-md">
        <DialogHeader>
          <DialogTitle>Enable Two-Factor Authentication</DialogTitle>
          <DialogDescription>
            {step === 'qr'
              ? 'Scan the QR code with your authenticator app'
              : 'Enter the 6-digit code from your authenticator app'}
          </DialogDescription>
        </DialogHeader>

        {step === 'qr' ? (
          <div className="space-y-6 py-4">
            <div className="flex flex-col items-center gap-4">
              {qrCode ? (
                <div className="bg-white p-4 rounded-lg">
                  <img src={qrCode} alt="QR Code" className="w-64 h-64" />
                </div>
              ) : (
                <Button onClick={handleStartSetup} disabled={isLoading} className="w-full">
                  {isLoading ? <Spinner size="sm" className="mr-2" /> : null}
                  Generate QR Code
                </Button>
              )}

              {secret && (
                <div className="w-full space-y-2">
                  <p className="text-sm text-muted-foreground">
                    Can't scan? Enter this code manually:
                  </p>
                  <div className="flex gap-2">
                    <code className="flex-1 bg-muted p-3 rounded font-mono text-sm text-foreground break-all">
                      {secret}
                    </code>
                    <Button
                      variant="outline"
                      size="sm"
                      onClick={handleCopySecret}
                      className="px-3"
                    >
                      {copied ? '✓' : 'Copy'}
                    </Button>
                  </div>
                </div>
              )}
            </div>

            {qrCode && (
              <Button onClick={() => setStep('verify')} className="w-full">
                Next: Verify Code
              </Button>
            )}
          </div>
        ) : (
          <div className="space-y-4 py-4">
            <div>
              <label className="text-sm font-medium">6-Digit Code</label>
              <Input
                type="text"
                inputMode="numeric"
                maxLength={6}
                placeholder="000000"
                value={totpCode}
                onChange={(e) => {
                  setTotpCode(e.target.value.replace(/\D/g, '').slice(0, 6))
                  setError('')
                }}
                className="mt-2 font-mono text-center text-lg tracking-widest"
              />
            </div>

            {error && <div className="text-sm text-red-400 bg-red-500/10 p-3 rounded">{error}</div>}

            <Button
              onClick={handleVerify}
              disabled={isLoading || totpCode.length !== 6}
              className="w-full"
            >
              {isLoading ? <Spinner size="sm" className="mr-2" /> : null}
              Verify & Enable 2FA
            </Button>

            <Button
              variant="outline"
              onClick={() => setStep('qr')}
              disabled={isLoading}
              className="w-full"
            >
              Back
            </Button>
          </div>
        )}
      </DialogContent>
    </Dialog>
  )
}
