import { useState, useEffect } from 'react'
import { useParams, useNavigate, useSearchParams } from 'react-router-dom'
import { Button } from '@/components/ui/button'
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '@/components/ui/card'
import { Spinner } from '@/components/ui/spinner'
import { useClaimDeviceMutation } from '@/hooks/mutations/useAuthMutations'

/**
 * DeviceClaim - OAuth flow for claiming a VST device
 *
 * Flow:
 * 1. User navigates to /claim/:deviceId from VST plugin
 * 2. User clicks Google/Discord button
 * 3. Backend redirects with oauth code
 * 4. This page calls claimDevice mutation
 * 5. VST receives authentication token via IPC/callback
 */
export function DeviceClaim() {
  const { deviceId } = useParams<{ deviceId: string }>()
  const navigate = useNavigate()
  const [searchParams] = useSearchParams()
  const [status, setStatus] = useState<'idle' | 'claiming' | 'success' | 'error'>('idle')
  const [errorMessage, setErrorMessage] = useState('')

  const claimMutation = useClaimDeviceMutation()

  // Check if we're returning from OAuth with an auth code
  useEffect(() => {
    const code = searchParams.get('code')
    const provider = searchParams.get('provider')
    const error = searchParams.get('error')

    if (error) {
      setStatus('error')
      setErrorMessage(error)
      return
    }

    if (code && provider && deviceId) {
      setStatus('claiming')
      claimMutation.mutate(
        {
          deviceId,
          oauthProvider: provider,
          code,
        },
        {
          onSuccess: () => {
            setStatus('success')
            // Notify VST plugin via postMessage if available
            if (window.opener) {
              window.opener.postMessage(
                {
                  type: 'device_claimed_success',
                  deviceId,
                },
                '*'
              )
            }
            // Redirect to feed after delay
            setTimeout(() => {
              navigate('/feed')
            }, 2000)
          },
          onError: (err: any) => {
            setStatus('error')
            setErrorMessage(err.message || 'Failed to claim device')
            // Notify VST plugin of error
            if (window.opener) {
              window.opener.postMessage(
                {
                  type: 'device_claimed_error',
                  deviceId,
                  error: err.message,
                },
                '*'
              )
            }
          },
        }
      )
    }
  }, [searchParams, deviceId, navigate])

  const handleGoogleClaim = () => {
    if (!deviceId) {
      setErrorMessage('Device ID is missing')
      return
    }

    // Redirect to backend OAuth endpoint
    const redirectUrl = `${window.location.origin}/claim/${deviceId}`
    const backendUrl = import.meta.env.VITE_API_URL || 'http://localhost:8787/api/v1'
    const oauthUrl = `${backendUrl}/auth/google?device_id=${encodeURIComponent(deviceId)}&redirect_uri=${encodeURIComponent(redirectUrl)}`

    window.location.href = oauthUrl
  }

  const handleDiscordClaim = () => {
    if (!deviceId) {
      setErrorMessage('Device ID is missing')
      return
    }

    // Redirect to backend OAuth endpoint
    const redirectUrl = `${window.location.origin}/claim/${deviceId}`
    const backendUrl = import.meta.env.VITE_API_URL || 'http://localhost:8787/api/v1'
    const oauthUrl = `${backendUrl}/auth/discord?device_id=${encodeURIComponent(deviceId)}&redirect_uri=${encodeURIComponent(redirectUrl)}`

    window.location.href = oauthUrl
  }

  return (
    <div className="min-h-screen flex items-center justify-center bg-gradient-to-br from-bg-primary via-bg-secondary to-bg-tertiary px-4">
      <Card className="w-full max-w-md">
        <CardHeader className="space-y-1">
          <CardTitle className="text-2xl">Claim Your Device</CardTitle>
          <CardDescription>
            Connect your Sidechain VST to your account
          </CardDescription>
        </CardHeader>

        <CardContent className="space-y-6">
          {/* Device ID Display */}
          <div className="bg-muted p-4 rounded-lg">
            <div className="text-sm text-muted-foreground mb-2">Device ID</div>
            <code className="text-sm font-mono text-primary break-all">
              {deviceId || 'Loading...'}
            </code>
          </div>

          {/* Status Messages */}
          {status === 'success' && (
            <div className="p-4 rounded-lg bg-green-500/10 text-green-700 dark:text-green-400">
              <div className="font-medium mb-1">Device Claimed Successfully!</div>
              <div className="text-sm">
                Your VST is now connected to your account. Redirecting...
              </div>
            </div>
          )}

          {status === 'error' && errorMessage && (
            <div className="p-4 rounded-lg bg-destructive/10 text-destructive">
              <div className="font-medium mb-1">Authentication Failed</div>
              <div className="text-sm">{errorMessage}</div>
            </div>
          )}

          {/* OAuth Buttons */}
          <div className="space-y-3">
            <Button
              className="w-full"
              onClick={handleGoogleClaim}
              disabled={status === 'claiming' || !deviceId}
            >
              {status === 'claiming' ? (
                <>
                  <Spinner size="sm" className="mr-2" />
                  Claiming...
                </>
              ) : (
                <>
                  <svg
                    className="mr-2 h-4 w-4"
                    viewBox="0 0 24 24"
                    fill="currentColor"
                  >
                    <path d="M22.56 12.25c0-.78-.07-1.53-.2-2.25H12v4.26h5.92c-.26 1.37-1.04 2.53-2.21 3.31v2.77h3.57c2.08-1.92 3.28-4.74 3.28-8.09z" />
                    <path d="M12 23c2.97 0 5.46-.98 7.28-2.66l-3.57-2.77c-.98.66-2.23 1.06-3.71 1.06-2.86 0-5.29-1.93-6.16-4.53H2.18v2.84C3.99 20.53 7.7 23 12 23z" />
                    <path d="M5.84 14.09c-.22-.66-.35-1.36-.35-2.09s.13-1.43.35-2.09V7.07H2.18C1.43 8.55 1 10.22 1 12s.43 3.45 1.18 4.93l2.85-2.22.81-.62z" />
                    <path d="M12 5.38c1.62 0 3.06.56 4.21 1.64l3.15-3.15C17.45 2.09 14.97 1 12 1 7.7 1 3.99 3.47 2.18 7.07l3.66 2.84c.87-2.6 3.3-4.53 6.16-4.53z" />
                  </svg>
                  Claim with Google
                </>
              )}
            </Button>

            <Button
              variant="outline"
              className="w-full"
              onClick={handleDiscordClaim}
              disabled={status === 'claiming' || !deviceId}
            >
              {status === 'claiming' ? (
                <>
                  <Spinner size="sm" className="mr-2" />
                  Claiming...
                </>
              ) : (
                <>
                  <svg
                    className="mr-2 h-4 w-4"
                    viewBox="0 0 24 24"
                    fill="currentColor"
                  >
                    <path d="M20.317 4.3671a19.8215 19.8215 0 00-4.8851-1.5152.074.074 0 00-.0784.0371c-.211.3671-.444.8426-.607 1.2182a18.27 18.27 0 00-5.487 0c-.163-.3756-.396-.8511-.607-1.2182a.077.077 0 00-.0785-.037 19.7163 19.7163 0 00-4.8852 1.515.0699.0699 0 00-.032.0274C1.053 8.645 0 12.692 0 16.6723c0 .0141.009.028.021.0328a19.902 19.902 0 005.858 2.973.078.078 0 00.085-.028c.462-.612.873-1.29 1.226-1.994a.076.076 0 00-.042-.106 13.107 13.107 0 01-1.872-.892.077.077 0 01-.008-.128 10.2 10.2 0 00.372-.294.075.075 0 01.078-.01c3.928 1.793 8.18 1.793 12.062 0a.075.075 0 01.079.009c.12.098.246.198.373.295a.077.077 0 01-.006.127 12.299 12.299 0 01-1.873.892.076.076 0 00-.041.107c.36.698.77 1.382 1.225 1.993a.076.076 0 00.084.028 19.839 19.839 0 005.859-2.972.077.077 0 00.021-.0328c0-3.9805-1.02-7.9971-4.32-10.6514a.0649.0649 0 00-.032-.0274zM8.02 13.9045c-.9-0-1.6362-.8889-1.6362-1.9779 0-1.0889.7073-1.9779 1.6362-1.9779.9412 0 1.6496.889 1.6362 1.9779 0 1.089-.7073 1.9779-1.6362 1.9779zm7.9566 0c-.9 0-1.6362-.8889-1.6362-1.9779 0-1.0889.7073-1.9779 1.6362-1.9779.9412 0 1.6496.889 1.6362 1.9779 0 1.089-.7145 1.9779-1.6362 1.9779z" />
                  </svg>
                  Claim with Discord
                </>
              )}
            </Button>
          </div>

          {/* Help Text */}
          <div className="text-xs text-muted-foreground space-y-2">
            <p>
              This will authenticate your Sidechain VST device and enable:
            </p>
            <ul className="list-disc list-inside space-y-1">
              <li>Automatic audio uploads</li>
              <li>Real-time feed syncing</li>
              <li>Social interactions (likes, comments)</li>
              <li>Access to your profile and settings</li>
            </ul>
          </div>
        </CardContent>
      </Card>
    </div>
  )
}
