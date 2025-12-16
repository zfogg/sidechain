/**
 * BackendWebSocketClient - WebSocket client for backend /api/v1/ws
 *
 * Handles real-time updates: new posts, likes, comments, follows, etc.
 * Mirrors C++ plugin's network client patterns with proper reconnection logic
 */

export type WebSocketMessageType =
  | 'new_post'
  | 'post_liked'
  | 'post_commented'
  | 'user_followed'
  | 'comment_liked'
  | 'post_saved'
  | 'notification'
  | 'error'

export interface WebSocketMessage {
  type: WebSocketMessageType
  payload: Record<string, any>
  timestamp?: number
}

export class BackendWebSocketClient {
  private ws: WebSocket | null = null
  private url: string
  private token: string | null = null
  private reconnectAttempts = 0
  private maxReconnectAttempts = 10
  private reconnectDelay = 1000 // Start with 1 second
  private maxReconnectDelay = 30000 // Cap at 30 seconds
  private reconnectTimeout: NodeJS.Timeout | null = null
  private heartbeatInterval: NodeJS.Timeout | null = null

  // Event listeners
  private listeners: Map<WebSocketMessageType | 'connect' | 'disconnect' | 'error', Set<(data: any) => void>> =
    new Map()

  constructor(baseUrl: string = import.meta.env.VITE_API_URL || 'http://localhost:8787') {
    // Convert http/https URL to ws/wss
    const wsProtocol = baseUrl.startsWith('https') ? 'wss' : 'ws'
    const cleanUrl = baseUrl.replace(/^https?:\/\//, '')

    // Strip /api/v1 suffix if already present to avoid doubling
    const baseWithoutApi = cleanUrl.endsWith('/api/v1')
      ? cleanUrl.slice(0, -7) // Remove '/api/v1'
      : cleanUrl

    this.url = `${wsProtocol}://${baseWithoutApi}/api/v1/ws`
  }

  /**
   * Connect to WebSocket with authentication token
   */
  connect(token: string) {
    if (this.ws?.readyState === WebSocket.OPEN) {
      console.warn('WebSocket already connected')
      return
    }

    this.token = token

    try {
      // Connect with token in query param (backend should validate)
      const connectUrl = `${this.url}?token=${encodeURIComponent(token)}`
      this.ws = new WebSocket(connectUrl)

      this.ws.onopen = () => {
        console.log('[WS] Connected')
        this.reconnectAttempts = 0
        this.reconnectDelay = 1000
        this.emit('connect', {})
        this.startHeartbeat()
      }

      this.ws.onmessage = (event) => {
        try {
          const message: WebSocketMessage = JSON.parse(event.data)
          console.log('[WS] Message:', message.type)
          this.emit(message.type, message.payload)
        } catch (error) {
          console.error('[WS] Failed to parse message:', error)
          this.emit('error', { message: 'Failed to parse WebSocket message' })
        }
      }

      this.ws.onerror = (error) => {
        console.error('[WS] Error:', error)
        this.emit('error', { message: 'WebSocket error' })
      }

      this.ws.onclose = () => {
        console.log('[WS] Closed')
        this.stopHeartbeat()
        this.emit('disconnect', {})
        this.reconnect()
      }
    } catch (error) {
      console.error('[WS] Failed to create WebSocket:', error)
      this.emit('error', { message: 'Failed to create WebSocket' })
      this.reconnect()
    }
  }

  /**
   * Disconnect and cleanup
   */
  disconnect() {
    this.stopHeartbeat()

    if (this.reconnectTimeout) {
      clearTimeout(this.reconnectTimeout)
      this.reconnectTimeout = null
    }

    if (this.ws) {
      this.ws.close()
      this.ws = null
    }

    console.log('[WS] Disconnected')
  }

  /**
   * Send message to server
   */
  send(type: string, payload: Record<string, any> = {}) {
    if (this.ws?.readyState !== WebSocket.OPEN) {
      console.warn('[WS] Cannot send message: WebSocket not connected')
      return
    }

    try {
      const message: WebSocketMessage = {
        type: type as WebSocketMessageType,
        payload,
        timestamp: Date.now(),
      }
      this.ws.send(JSON.stringify(message))
    } catch (error) {
      console.error('[WS] Failed to send message:', error)
    }
  }

  /**
   * Subscribe to specific event type
   * Returns unsubscribe function
   */
  on(type: WebSocketMessageType | 'connect' | 'disconnect' | 'error', callback: (data: any) => void) {
    if (!this.listeners.has(type)) {
      this.listeners.set(type, new Set())
    }
    this.listeners.get(type)!.add(callback)

    // Return unsubscribe function
    return () => this.off(type, callback)
  }

  /**
   * Unsubscribe from event type
   */
  off(type: WebSocketMessageType | 'connect' | 'disconnect' | 'error', callback: (data: any) => void) {
    this.listeners.get(type)?.delete(callback)
  }

  /**
   * Emit event to all listeners
   */
  private emit(type: WebSocketMessageType | 'connect' | 'disconnect' | 'error', data: any) {
    this.listeners.get(type)?.forEach((callback) => {
      try {
        callback(data)
      } catch (error) {
        console.error('[WS] Listener error:', error)
      }
    })
  }

  /**
   * Automatic reconnection with exponential backoff
   */
  private reconnect() {
    if (this.reconnectAttempts >= this.maxReconnectAttempts) {
      console.error('[WS] Max reconnection attempts reached')
      this.emit('error', { message: 'Max reconnection attempts reached' })
      return
    }

    this.reconnectAttempts++
    this.reconnectDelay = Math.min(
      this.reconnectDelay * 2 + Math.random() * 1000,
      this.maxReconnectDelay
    )

    console.log(
      `[WS] Reconnecting in ${this.reconnectDelay}ms (attempt ${this.reconnectAttempts}/${this.maxReconnectAttempts})`
    )

    this.reconnectTimeout = setTimeout(() => {
      if (this.token) {
        this.connect(this.token)
      }
    }, this.reconnectDelay)
  }

  /**
   * Send periodic ping to keep connection alive
   */
  private startHeartbeat() {
    this.stopHeartbeat()

    this.heartbeatInterval = setInterval(() => {
      if (this.ws?.readyState === WebSocket.OPEN) {
        this.send('ping', {})
      }
    }, 30000) // Every 30 seconds
  }

  /**
   * Stop heartbeat
   */
  private stopHeartbeat() {
    if (this.heartbeatInterval) {
      clearInterval(this.heartbeatInterval)
      this.heartbeatInterval = null
    }
  }

  /**
   * Get connection status
   */
  isConnected(): boolean {
    return this.ws?.readyState === WebSocket.OPEN
  }

  /**
   * Get all active listeners (for debugging)
   */
  getListeners() {
    const result: Record<string, number> = {}
    this.listeners.forEach((callbacks, type) => {
      result[type] = callbacks.size
    })
    return result
  }
}

// Singleton instance
let websocketClient: BackendWebSocketClient | null = null

export function getWebSocketClient(): BackendWebSocketClient {
  if (!websocketClient) {
    websocketClient = new BackendWebSocketClient()
  }
  return websocketClient
}

export function createWebSocketClient(baseUrl?: string): BackendWebSocketClient {
  return new BackendWebSocketClient(baseUrl)
}
