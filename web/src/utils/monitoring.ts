/**
 * Performance Monitoring & Error Tracking
 * Captures metrics for analytics and debugging
 */

export interface PerformanceMetric {
  name: string
  duration: number
  timestamp: number
  metadata?: Record<string, any>
}

export interface ErrorLog {
  message: string
  stack?: string
  timestamp: number
  context?: Record<string, any>
  severity: 'error' | 'warning' | 'info'
}

class PerformanceMonitor {
  private metrics: PerformanceMetric[] = []
  private errors: ErrorLog[] = []
  private maxMetrics = 100
  private maxErrors = 50

  /**
   * Record a custom performance metric
   */
  recordMetric(
    name: string,
    duration: number,
    metadata?: Record<string, any>
  ): void {
    const metric: PerformanceMetric = {
      name,
      duration,
      timestamp: Date.now(),
      metadata,
    }

    this.metrics.push(metric)

    // Keep only recent metrics in memory
    if (this.metrics.length > this.maxMetrics) {
      this.metrics = this.metrics.slice(-this.maxMetrics)
    }

    // Log slow operations
    if (duration > 1000) {
      console.warn(`Slow operation: ${name} took ${duration}ms`, metadata)
    }
  }

  /**
   * Measure async function execution time
   */
  async measureAsync<T>(
    name: string,
    fn: () => Promise<T>,
    metadata?: Record<string, any>
  ): Promise<T> {
    const start = performance.now()
    try {
      const result = await fn()
      const duration = performance.now() - start
      this.recordMetric(name, duration, metadata)
      return result
    } catch (error) {
      const duration = performance.now() - start
      this.recordMetric(name, duration, { ...metadata, error: true })
      throw error
    }
  }

  /**
   * Measure sync function execution time
   */
  measure<T>(
    name: string,
    fn: () => T,
    metadata?: Record<string, any>
  ): T {
    const start = performance.now()
    try {
      const result = fn()
      const duration = performance.now() - start
      this.recordMetric(name, duration, metadata)
      return result
    } catch (error) {
      const duration = performance.now() - start
      this.recordMetric(name, duration, { ...metadata, error: true })
      throw error
    }
  }

  /**
   * Log an error
   */
  logError(
    message: string,
    error?: unknown,
    context?: Record<string, any>
  ): void {
    const stack = error instanceof Error ? error.stack : undefined

    const errorLog: ErrorLog = {
      message,
      stack,
      timestamp: Date.now(),
      context,
      severity: 'error',
    }

    this.errors.push(errorLog)

    // Keep only recent errors in memory
    if (this.errors.length > this.maxErrors) {
      this.errors = this.errors.slice(-this.maxErrors)
    }

    // Send to error tracking service in production
    if (import.meta.env.PROD) {
      this.sendErrorToService(errorLog)
    }

    console.error(message, error, context)
  }

  /**
   * Log a warning
   */
  logWarning(message: string, context?: Record<string, any>): void {
    const errorLog: ErrorLog = {
      message,
      timestamp: Date.now(),
      context,
      severity: 'warning',
    }

    this.errors.push(errorLog)

    if (this.errors.length > this.maxErrors) {
      this.errors = this.errors.slice(-this.maxErrors)
    }

    console.warn(message, context)
  }

  /**
   * Get collected metrics for analysis
   */
  getMetrics(): PerformanceMetric[] {
    return [...this.metrics]
  }

  /**
   * Get average duration for a metric name
   */
  getAverageDuration(name: string): number {
    const matching = this.metrics.filter(m => m.name === name)
    if (matching.length === 0) return 0

    const total = matching.reduce((sum, m) => sum + m.duration, 0)
    return total / matching.length
  }

  /**
   * Get collected errors
   */
  getErrors(): ErrorLog[] {
    return [...this.errors]
  }

  /**
   * Clear collected data
   */
  clear(): void {
    this.metrics = []
    this.errors = []
  }

  /**
   * Send error to tracking service
   */
  private sendErrorToService(errorLog: ErrorLog): void {
    // This would integrate with services like:
    // - Sentry (https://sentry.io)
    // - Rollbar (https://rollbar.com)
    // - LogRocket (https://logrocket.com)
    // - Custom backend endpoint

    // Example implementation:
    if (import.meta.env.VITE_ERROR_TRACKING_URL) {
      navigator.sendBeacon(
        import.meta.env.VITE_ERROR_TRACKING_URL,
        JSON.stringify({
          ...errorLog,
          userAgent: navigator.userAgent,
          url: window.location.href,
        })
      )
    }
  }
}

// Singleton instance
const monitor = new PerformanceMonitor()

/**
 * Global error handler
 */
window.addEventListener('error', (event) => {
  monitor.logError(
    `Uncaught error: ${event.message}`,
    event.error,
    {
      filename: event.filename,
      lineno: event.lineno,
      colno: event.colno,
    }
  )
})

/**
 * Global unhandled promise rejection handler
 */
window.addEventListener('unhandledrejection', (event) => {
  monitor.logError(
    `Unhandled promise rejection: ${event.reason}`,
    event.reason instanceof Error ? event.reason : new Error(String(event.reason))
  )
})

export default monitor
