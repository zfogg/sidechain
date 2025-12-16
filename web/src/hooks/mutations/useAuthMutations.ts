import { useMutation } from '@tanstack/react-query'
import { AuthClient } from '@/api/AuthClient'
import { useUserStore } from '@/stores/useUserStore'
import { User } from '@/models/User'

/**
 * useLoginMutation - Mutation for user login with email and password
 *
 * Usage:
 * ```tsx
 * const { mutate: login, isPending } = useLoginMutation()
 * const handleLogin = () => login({ email: 'user@example.com', password: 'pass' })
 * ```
 */
export function useLoginMutation() {
  const { login } = useUserStore()

  return useMutation({
    mutationFn: async ({
      email,
      password,
    }: {
      email: string
      password: string
    }) => {
      const result = await AuthClient.login(email, password)
      if (result.isError()) {
        throw new Error(result.getError())
      }
      return result.getValue()
    },
    onSuccess: (data) => {
      const user = User.fromJson(data.user)
      login(data.token, user)
    },
  })
}

/**
 * useRegisterMutation - Mutation for user registration
 *
 * Usage:
 * ```tsx
 * const { mutate: register, isPending } = useRegisterMutation()
 * const handleRegister = () => register({
 *   email: 'user@example.com',
 *   username: 'username',
 *   password: 'password'
 * })
 * ```
 */
export function useRegisterMutation() {
  const { login } = useUserStore()

  return useMutation({
    mutationFn: async ({
      email,
      username,
      password,
    }: {
      email: string
      username: string
      password: string
    }) => {
      const result = await AuthClient.register(email, username, password)
      if (result.isError()) {
        throw new Error(result.getError())
      }
      return result.getValue()
    },
    onSuccess: (data) => {
      const user = User.fromJson(data.user)
      login(data.token, user)
    },
  })
}

/**
 * useRefreshTokenMutation - Mutation for refreshing the auth token
 */
export function useRefreshTokenMutation() {
  const { login } = useUserStore()

  return useMutation({
    mutationFn: async () => {
      const result = await AuthClient.refreshToken()
      if (result.isError()) {
        throw new Error(result.getError())
      }
      return result.getValue()
    },
    onSuccess: (data) => {
      const user = User.fromJson(data.user)
      login(data.token, user)
    },
  })
}

/**
 * useClaimDeviceMutation - Mutation for claiming a VST device with OAuth
 *
 * Usage:
 * ```tsx
 * const { mutate: claimDevice } = useClaimDeviceMutation()
 * const handleClaim = () => claimDevice({
 *   deviceId: 'device-123',
 *   oauthProvider: 'google',
 *   code: 'auth-code-from-oauth'
 * })
 * ```
 */
export function useClaimDeviceMutation() {
  const { login } = useUserStore()

  return useMutation({
    mutationFn: async ({
      deviceId,
      oauthProvider,
      code,
    }: {
      deviceId: string
      oauthProvider: string
      code: string
    }) => {
      const result = await AuthClient.claimDevice(deviceId, oauthProvider, code)
      if (result.isError()) {
        throw new Error(result.getError())
      }
      return result.getValue()
    },
    onSuccess: (data) => {
      const user = User.fromJson(data.user)
      login(data.token, user)
    },
  })
}
