import { useCallback } from 'react'
import monitor from '@/utils/monitoring'

/**
 * useAPIMetrics - Hook for tracking API call performance
 * Automatically records metrics for slow API calls
 */
export function useAPIMetrics() {
  const recordAPICall = useCallback(
    async <T,>(
      endpoint: string,
      fn: () => Promise<T>,
      options?: {
        slowThreshold?: number // Default 1000ms
      }
    ): Promise<T> => {
      const slowThreshold = options?.slowThreshold ?? 1000

      return monitor.measureAsync(
        `API: ${endpoint}`,
        fn,
        {
          endpoint,
          slowThreshold,
        }
      )
    },
    []
  )

  const getSlowAPICalls = useCallback(() => {
    const metrics = monitor.getMetrics()
    return metrics
      .filter(m => m.name.startsWith('API:') && m.duration > 1000)
      .sort((a, b) => b.duration - a.duration)
  }, [])

  const getAverageAPILatency = useCallback((endpoint: string) => {
    return monitor.getAverageDuration(`API: ${endpoint}`)
  }, [])

  return {
    recordAPICall,
    getSlowAPICalls,
    getAverageAPILatency,
    getAllMetrics: () => monitor.getMetrics(),
    getErrors: () => monitor.getErrors(),
  }
}
