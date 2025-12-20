import { useEffect, useState, ReactNode, useRef } from 'react'
import { Chat } from 'stream-chat-react'
import { StreamChat } from 'stream-chat'
import { useUserStore } from '@/stores/useUserStore'
import { useChatStore } from '@/stores/useChatStore'
import { AuthClient } from '@/api/AuthClient'
import { Spinner } from '@/components/ui/spinner'
import 'stream-chat-react/css/v2/index.css'

interface ChatProviderProps {
  children: ReactNode
}

/**
 * ChatProvider - Stream Chat integration provider with GetStream.io Presence
 *
 * Handles:
 * - Stream Chat client initialization
 * - Token acquisition from backend
 * - User connection (automatically marks user online in GetStream.io)
 * - Presence tracking (online/offline, custom status, invisible mode)
 * - Real-time presence events
 * - Error handling and reconnection
 *
 * Note: Presence is handled entirely by GetStream.io. When user connects,
 * they are automatically marked online. Presence updates are real-time via
 * GetStream.io events (user.online, user.offline, user.presence.changed, user.updated).
 *
 * The usePresence hook subscribes to these events for real-time UI updates.
 */
export function ChatProvider({ children }: ChatProviderProps) {
  const { user, token } = useUserStore()
  const { setStreamToken, setStreamConnected } = useChatStore()
  const [chatClient, setChatClient] = useState<StreamChat | null>(null)
  const [isConnecting, setIsConnecting] = useState(false)
  const connectingRef = useRef(false)

  const apiKey = import.meta.env.VITE_STREAM_API_KEY

  // Initialize Stream Chat client
  useEffect(() => {
    // Skip if chat is not configured - no logging needed
    if (!apiKey) return

    if (!user || !token) return

    const initChat = async () => {
      // Prevent duplicate connection attempts
      if (connectingRef.current) {
        return
      }

      setIsConnecting(true)
      connectingRef.current = true

      try {
        // Validate user has required fields
        if (!user.id) {
          throw new Error('User ID is not set. Please log in again.')
        }

        // Create Stream Chat instance if not already created
        let client = chatClient
        if (!client) {
          client = StreamChat.getInstance(apiKey)
          setChatClient(client)
        }

        // Only connect if not already connected
        if (client.userID === user.id) {
          setStreamConnected(true)
          setIsConnecting(false)
          connectingRef.current = false
          return
        }

        // Get Stream token from backend
        const result = await AuthClient.getStreamToken()
        if (result.isError()) {
          throw new Error(`Failed to get Stream token: ${result.getError()}`)
        }

        const streamToken = result.getValue().token
        if (!streamToken) {
          throw new Error('Stream token is empty')
        }

        setStreamToken(streamToken)

        // Connect user to Stream Chat
        console.debug('[Chat] Connecting user to Stream Chat')

        await client.connectUser(
          {
            id: user.id,
            name: user.displayName || user.username,
            image: user.profilePictureUrl,
          },
          streamToken
        )

        setStreamConnected(true)
        setIsConnecting(false)
        connectingRef.current = false
      } catch (err) {
        const errorMessage = err instanceof Error ? err.message : 'Failed to connect to Stream Chat'
        // Use debug level - chat failures are non-critical for app functionality
        console.debug('[Chat] Connection failed (graceful degradation):', errorMessage)
        setIsConnecting(false)
        connectingRef.current = false
      }
    }

    initChat()

    // Cleanup on unmount or when user logs out
    return () => {
      if (chatClient) {
        chatClient.disconnectUser()
        setStreamConnected(false)
      }
    }
  }, [user?.id, token, apiKey])

  // If chat is not configured, just render children without chat
  if (!apiKey) {
    return <>{children}</>
  }

  // Ensure we have the connected chat client before rendering
  // Don't fall back to creating an unconnected client
  if (!chatClient) {
    // Still waiting for client initialization
    if (isConnecting) {
      // Initialize a temporary client just for the Chat wrapper context
      let tempClient: StreamChat | null = null
      try {
        tempClient = StreamChat.getInstance(apiKey)
      } catch (err) {
        console.error('[Chat] Failed to create temporary Stream Chat instance:', err)
      }

      if (tempClient) {
        return (
          <Chat client={tempClient} theme="dark">
            <div className="flex items-center justify-center h-screen">
              <Spinner size="lg" />
            </div>
          </Chat>
        )
      }
    }

    // Not connecting, no client, and chat not initialized - render children anyway
    // Chat features will be unavailable but app continues to work
    return <>{children}</>
  }

  // If still connecting, show loading state
  if (isConnecting) {
    return (
      <Chat client={chatClient} theme="dark">
        <div className="flex items-center justify-center h-screen">
          <Spinner size="lg" />
        </div>
      </Chat>
    )
  }

  // Connected successfully - render children with the connected client
  // If there was an error, chat features won't work but app continues to function

  return (
    <Chat client={chatClient} theme="dark">
      {children}
    </Chat>
  )
}
