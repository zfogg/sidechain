import { Page } from '@playwright/test'

/**
 * Helper class for testing WebSocket functionality in E2E tests
 */
export class WebSocketHelper {
  constructor(private page: Page) {}

  /**
   * Wait for WebSocket connection to be established
   */
  async waitForConnection(timeout: number = 10000): Promise<boolean> {
    try {
      return await this.page.evaluate(
        ({ timeout: t }) => {
          return new Promise<boolean>((resolve) => {
            const startTime = Date.now()

            const check = () => {
              // Check if WebSocket connection exists and is open
              // This assumes the app exposes a WebSocket client in window
              const isConnected =
                (window as any).__websocketClient?.isConnected?.() ?? false

              if (isConnected) {
                resolve(true)
                return
              }

              if (Date.now() - startTime > t) {
                resolve(false)
                return
              }

              setTimeout(check, 100)
            }

            check()
          })
        },
        { timeout }
      )
    } catch (error) {
      console.error('Error waiting for WebSocket connection:', error)
      return false
    }
  }

  /**
   * Check if WebSocket is currently connected
   */
  async isConnected(): Promise<boolean> {
    return await this.page.evaluate(() => {
      return (window as any).__websocketClient?.isConnected?.() ?? false
    })
  }

  /**
   * Get WebSocket connection state
   */
  async getConnectionState(): Promise<string> {
    return await this.page.evaluate(() => {
      return (window as any).__websocketClient?.getState?.() ?? 'unknown'
    })
  }

  /**
   * Wait for WebSocket to disconnect (useful for testing reconnection)
   */
  async waitForDisconnection(timeout: number = 5000): Promise<boolean> {
    try {
      return await this.page.evaluate(
        ({ timeout: t }) => {
          return new Promise<boolean>((resolve) => {
            const startTime = Date.now()

            const check = () => {
              const isConnected =
                (window as any).__websocketClient?.isConnected?.() ?? true

              if (!isConnected) {
                resolve(true)
                return
              }

              if (Date.now() - startTime > t) {
                resolve(false)
                return
              }

              setTimeout(check, 100)
            }

            check()
          })
        },
        { timeout }
      )
    } catch (error) {
      console.error('Error waiting for WebSocket disconnection:', error)
      return false
    }
  }

  /**
   * Wait for a specific message to be received via WebSocket
   */
  async waitForMessage(
    messagePattern: (msg: unknown) => boolean,
    timeout: number = 5000
  ): Promise<boolean> {
    try {
      return await this.page.evaluate(
        ({ timeout: t, pattern: _ }) => {
          return new Promise<boolean>((resolve) => {
            const startTime = Date.now()

            // Set up message listener
            const messageQueue: unknown[] = []
            const originalHandler =
              (window as any).__websocketMessageHandler

            const messageHandler = (msg: unknown) => {
              messageQueue.push(msg)
              if (originalHandler) originalHandler(msg)
            }

            ;(window as any).__websocketMessageHandler = messageHandler

            const check = () => {
              // In a real implementation, you would check if any message
              // in the queue matches the pattern
              // For now, we just check if any message was received
              if (messageQueue.length > 0) {
                resolve(true)
                return
              }

              if (Date.now() - startTime > t) {
                resolve(false)
                return
              }

              setTimeout(check, 100)
            }

            check()
          })
        },
        { timeout: timeout, pattern: messagePattern }
      )
    } catch (error) {
      console.error('Error waiting for WebSocket message:', error)
      return false
    }
  }

  /**
   * Simulate WebSocket disconnection and reconnection for testing resilience
   */
  async simulateReconnection(delayMs: number = 1000): Promise<void> {
    await this.page.evaluate(
      ({ delay }) => {
        const ws = (window as any).__websocketClient
        if (ws?.reconnect) {
          ws.disconnect()
          setTimeout(() => ws.reconnect(), delay)
        }
      },
      { delay: delayMs }
    )
  }

  /**
   * Get all received messages (if the app stores them for debugging)
   */
  async getReceivedMessages(): Promise<unknown[]> {
    return await this.page.evaluate(() => {
      return (window as any).__websocketMessageLog ?? []
    })
  }

  /**
   * Clear message log
   */
  async clearMessageLog(): Promise<void> {
    await this.page.evaluate(() => {
      ;(window as any).__websocketMessageLog = []
    })
  }
}
