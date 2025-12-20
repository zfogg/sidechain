import { useEffect, useState } from 'react'
import { useNavigate, useSearchParams } from 'react-router-dom'
import { useUserStore } from '@/stores/useUserStore'
import { UserModel } from '@/models/User'
import { UserClient } from '@/api/UserClient'
import { AuthClient } from '@/api/AuthClient'
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
    const handleAuth = async () => {
      const token = searchParams.get('token')
      const userJson = searchParams.get('user')
      const sessionId = searchParams.get('session_id')
      const errorParam = searchParams.get('error')

      if (errorParam) {
        setError(errorParam || 'Authentication failed')
        // Redirect to login after delay
        setTimeout(() => {
          navigate(`/login?error=${encodeURIComponent(errorParam)}`)
        }, 2000)
        return
      }

      // Check for session-based flow (new backend)
      if (sessionId) {
        try {
          // Exchange session_id for token and user data via AuthClient
          const result = await AuthClient.exchangeOAuthSession(sessionId)

          if (!result.isOk()) {
            throw new Error(result.getError())
          }

          const authResponse = result.getValue()
          login(authResponse.token, authResponse.user)

          // Fetch complete profile
          if (authResponse.user.username) {
            try {
              const profileResult = await UserClient.getProfile(authResponse.user.username)
              if (profileResult.isOk()) {
                const profile = profileResult.getValue()
                useUserStore.setState((state) => ({
                  user: state.user
                    ? {
                        ...state.user,
                        profilePictureUrl: profile.profilePictureUrl || '',
                      }
                    : null,
                }))
              }
            } catch (e) {
              console.debug('Failed to fetch complete profile after auth:', e)
            }
          }

          navigate('/feed')
          return
        } catch (err) {
          console.error('Failed to exchange session:', err)
          setError('Failed to complete authentication')
          setTimeout(() => {
            navigate('/login?error=session_exchange_failed')
          }, 2000)
          return
        }
      }

      // Fallback: old token/user flow (for backward compatibility)
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

        // Fetch complete profile to ensure we have profile picture URL
        // The auth response may not include all fields, especially profile_picture_url
        if (user.username) {
          try {
            const profileResult = await UserClient.getProfile(user.username)
            if (profileResult.isOk()) {
              const profile = profileResult.getValue()
              // Update user store with complete profile data
              useUserStore.setState((state) => ({
                user: state.user
                  ? {
                      ...state.user,
                      profilePictureUrl: profile.profilePictureUrl || '',
                    }
                  : null,
              }))
            }
          } catch (e) {
            // Silently fail - user can still proceed
            console.debug('Failed to fetch complete profile after auth:', e)
          }
        }

        // Redirect to feed
        navigate('/feed')
      } catch (err) {
        console.error('Failed to parse auth response:', err)
        setError('Failed to process authentication')
        setTimeout(() => {
          navigate('/login?error=parse_error')
        }, 2000)
      }
    }

    handleAuth()
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
