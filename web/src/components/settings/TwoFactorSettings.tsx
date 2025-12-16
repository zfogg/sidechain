import { useEffect, useState } from 'react'
import { AuthClient } from '@/api/AuthClient'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import { Spinner } from '@/components/ui/spinner'
import { TwoFactorSetup } from '@/components/auth/TwoFactorSetup'
import { BackupCodes } from '@/components/auth/BackupCodes'

export function TwoFactorSettings() {
  const [isLoading, setIsLoading] = useState(true)
  const [isTwoFactorEnabled, setIsTwoFactorEnabled] = useState(false)
  const [error, setError] = useState('')
  const [showSetupDialog, setShowSetupDialog] = useState(false)
  const [showBackupCodesDialog, setShowBackupCodesDialog] = useState(false)
  const [backupCodes, setBackupCodes] = useState<string[]>([])
  const [showDisablePassword, setShowDisablePassword] = useState(false)
  const [disablePassword, setDisablePassword] = useState('')
  const [isDisabling, setIsDisabling] = useState(false)

  // Load 2FA status
  useEffect(() => {
    const loadStatus = async () => {
      setIsLoading(true)
      const result = await AuthClient.getTwoFactorStatus()

      if (result.isOk()) {
        setIsTwoFactorEnabled(result.getValue().isTwoFactorEnabled)
      } else {
        setError('Failed to load 2FA status')
      }

      setIsLoading(false)
    }

    loadStatus()
  }, [])

  const handleSetupSuccess = (codes: string[]) => {
    setIsTwoFactorEnabled(true)
    setBackupCodes(codes)
    setShowBackupCodesDialog(true)
  }

  const handleDisable = async () => {
    if (!disablePassword) {
      setError('Password is required')
      return
    }

    setIsDisabling(true)
    setError('')

    const result = await AuthClient.disableTwoFactor(disablePassword)

    if (result.isOk()) {
      setIsTwoFactorEnabled(false)
      setShowDisablePassword(false)
      setDisablePassword('')
    } else {
      setError(result.getError())
    }

    setIsDisabling(false)
  }

  const handleRegenerateBackupCodes = async () => {
    if (!disablePassword) {
      setError('Password is required')
      return
    }

    setIsDisabling(true)
    setError('')

    const result = await AuthClient.generateBackupCodes(disablePassword)

    if (result.isOk()) {
      setBackupCodes(result.getValue().backupCodes)
      setShowBackupCodesDialog(true)
      setShowDisablePassword(false)
      setDisablePassword('')
    } else {
      setError(result.getError())
    }

    setIsDisabling(false)
  }

  if (isLoading) {
    return (
      <div className="flex items-center justify-center py-8">
        <Spinner size="sm" />
      </div>
    )
  }

  return (
    <div className="space-y-6">
      <div className="flex items-start justify-between">
        <div>
          <h3 className="font-semibold text-foreground mb-1">Two-Factor Authentication</h3>
          <p className="text-sm text-muted-foreground">
            {isTwoFactorEnabled
              ? 'Your account is protected with 2FA'
              : 'Add an extra layer of security to your account'}
          </p>
        </div>

        <div className="flex items-center gap-2">
          <div
            className={`w-3 h-3 rounded-full ${isTwoFactorEnabled ? 'bg-green-500' : 'bg-gray-500'}`}
          />
          <span className="text-sm font-medium">
            {isTwoFactorEnabled ? 'Enabled' : 'Disabled'}
          </span>
        </div>
      </div>

      {error && <div className="text-sm text-red-400 bg-red-500/10 p-3 rounded">{error}</div>}

      <div className="flex gap-2">
        {!isTwoFactorEnabled ? (
          <Button onClick={() => setShowSetupDialog(true)} className="gap-2">
            Enable 2FA
          </Button>
        ) : (
          <>
            <Button
              variant="outline"
              onClick={() => setShowDisablePassword(!showDisablePassword)}
              className="gap-2"
            >
              Manage 2FA
            </Button>
          </>
        )}
      </div>

      {showDisablePassword && isTwoFactorEnabled && (
        <div className="space-y-3 p-4 bg-muted rounded-lg">
          <p className="text-sm text-muted-foreground">Enter your password to continue:</p>

          <Input
            type="password"
            placeholder="Password"
            value={disablePassword}
            onChange={(e) => {
              setDisablePassword(e.target.value)
              setError('')
            }}
          />

          <div className="flex gap-2">
            <Button
              variant="outline"
              onClick={() => handleRegenerateBackupCodes()}
              disabled={isDisabling || !disablePassword}
              className="flex-1"
            >
              {isDisabling ? <Spinner size="sm" className="mr-2" /> : null}
              Regenerate Backup Codes
            </Button>

            <Button
              variant="destructive"
              onClick={handleDisable}
              disabled={isDisabling || !disablePassword}
              className="flex-1"
            >
              {isDisabling ? <Spinner size="sm" className="mr-2" /> : null}
              Disable 2FA
            </Button>
          </div>

          <Button
            variant="ghost"
            onClick={() => {
              setShowDisablePassword(false)
              setDisablePassword('')
              setError('')
            }}
            className="w-full"
          >
            Cancel
          </Button>
        </div>
      )}

      <TwoFactorSetup
        isOpen={showSetupDialog}
        onClose={() => setShowSetupDialog(false)}
        onSuccess={handleSetupSuccess}
      />

      <BackupCodes
        backupCodes={backupCodes}
        isOpen={showBackupCodesDialog}
        onClose={() => setShowBackupCodesDialog(false)}
      />
    </div>
  )
}
