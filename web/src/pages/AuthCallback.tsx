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
      <div className="min-h-screen flex items-center justify-center bg-gradient-to-br from-bg-primary via-bg-secondary to-bg-tertiary">
        <div className="text-center space-y-4">
          <div className="text-red-500 text-lg font-medium">{error}</div>
          <div className="text-muted-foreground">Redirecting to login...</div>
        </div>
      </div>
    )
  }

  return (
    <div className="min-h-screen flex items-center justify-center bg-gradient-to-br from-bg-primary via-bg-secondary to-bg-tertiary">
      <div className="text-center space-y-4">
        <Spinner size="lg" className="mx-auto" />
        <div className="text-lg font-medium">Completing authentication...</div>
        <div className="text-muted-foreground">Please wait while we set up your account</div>
      </div>
    </div>
  )
}
