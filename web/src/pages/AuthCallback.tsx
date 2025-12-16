import { useEffect, useState } from 'react'
import { useNavigate, useSearchParams } from 'react-router-dom'
import { useUserStore } from '@/stores/useUserStore'
import { UserModel } from '@/models/User'
import { Spinner } from '@/components/ui/spinner'

/**
 * AuthCallback - Handles OAuth redirect from Google/Discord
 *
 * The backend redirects back with:
 * - token: JWT token
 * - user: JSON-encoded user data
 * - error: Error message if auth failed
 */
export function AuthCallback() {
  const navigate = useNavigate()
  const [searchParams] = useSearchParams()
  const { login } = useUserStore()
  const [error, setError] = useState<string>('')

  useEffect(() => {
    const token = searchParams.get('token')
    const userJson = searchParams.get('user')
    const errorParam = searchParams.get('error')

    if (errorParam) {
      setError(errorParam || 'Authentication failed')
      // Redirect to login after delay
      setTimeout(() => {
        navigate(`/login?error=${encodeURIComponent(errorParam)}`)
      }, 2000)
      return
    }

    if (!token || !userJson) {
      setError('Missing authentication data')
      setTimeout(() => {
        navigate('/login?error=missing_data')
      }, 2000)
      return
    }

    try {
      // Decode the user JSON (it should be URL-encoded by backend)
      const userData = JSON.parse(decodeURIComponent(userJson))
      const user = UserModel.fromJson(userData)

      // Login with the token and user data
      login(token, user)

      // Redirect to feed
      navigate('/feed')
    } catch (err) {
      console.error('Failed to parse auth response:', err)
      setError('Failed to process authentication')
      setTimeout(() => {
        navigate('/login?error=parse_error')
      }, 2000)
    }
  }, [searchParams, navigate, login])

  if (error) {
    return (
      <div className="min-h-screen flex items-center justify-center bg-gradient-to-br from-bg-primary via-bg-secondary to-bg-tertiary px-4">
        <div className="w-full max-w-md">
          <div className="bg-card border border-border/50 rounded-2xl shadow-2xl p-8">
            <div className="text-center space-y-4">
              <div className="flex justify-center">
                <div className="w-16 h-16 rounded-full bg-red-500/20 flex items-center justify-center">
                  <svg className="w-8 h-8 text-red-500" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                    <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M12 8v4m0 4v.01M21 12a9 9 0 11-18 0 9 9 0 0118 0z" />
                  </svg>
                </div>
              </div>
              <div>
                <div className="text-lg font-bold text-foreground mb-2">Authentication Failed</div>
                <div className="text-red-400 text-sm mb-4">{error}</div>
              </div>
              <div className="text-muted-foreground text-sm">Redirecting to login...</div>
            </div>
          </div>
        </div>
      </div>
    )
  }

  return (
    <div className="min-h-screen flex items-center justify-center bg-gradient-to-br from-bg-primary via-bg-secondary to-bg-tertiary px-4">
      <div className="w-full max-w-md">
        <div className="bg-card border border-border/50 rounded-2xl shadow-2xl p-8">
          <div className="text-center space-y-6">
            <div className="flex justify-center">
              <Spinner size="lg" />
            </div>
            <div>
              <div className="text-xl font-bold text-foreground mb-2">Completing Authentication</div>
              <div className="text-muted-foreground text-sm">Signing you in and setting up your account...</div>
            </div>
          </div>
        </div>
      </div>
    </div>
  )
}
