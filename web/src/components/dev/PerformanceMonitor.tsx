import { useState, useEffect } from 'react'
import monitor from '@/utils/monitoring'
import { getWebVitalsSummary } from '@/hooks/useWebVitals'

/**
 * PerformanceMonitor - Dev tool for viewing performance metrics
 * Only visible in development mode
 */
export function PerformanceMonitor() {
  const [isOpen, setIsOpen] = useState(false)
  const [metrics, setMetrics] = useState(monitor.getMetrics())
  const [errors, setErrors] = useState(monitor.getErrors())

  // Auto-refresh metrics every 2 seconds
  useEffect(() => {
    const interval = setInterval(() => {
      setMetrics([...monitor.getMetrics()])
      setErrors([...monitor.getErrors()])
    }, 2000)

    return () => clearInterval(interval)
  }, [])

  if (!import.meta.env.DEV) {
    return null
  }

  const slowCalls = metrics
    .filter(m => m.name.startsWith('API:') && m.duration > 1000)
    .slice(-5)

  const webVitals = getWebVitalsSummary()

  return (
    <div className="fixed bottom-4 right-4 z-50">
      <button
        onClick={() => setIsOpen(!isOpen)}
        className="bg-coral-pink hover:bg-coral-pink/90 text-white px-3 py-2 rounded-lg text-xs font-medium shadow-lg transition-colors"
        title="Performance Monitor (Dev Only)"
      >
        {isOpen ? '✕' : '⚡'} Metrics
      </button>

      {isOpen && (
        <div className="absolute bottom-12 right-0 bg-bg-secondary border border-border rounded-lg p-4 w-96 max-h-96 overflow-auto shadow-xl text-xs">
          {/* Web Vitals */}
          <div className="mb-4">
            <h3 className="font-bold text-coral-pink mb-2">Core Web Vitals</h3>
            <div className="space-y-1 text-muted-foreground">
              {webVitals.lcp && (
                <div>
                  LCP:{' '}
                  <span className={webVitals.lcp.rating === 'good' ? 'text-mint-green' : 'text-amber-500'}>
                    {webVitals.lcp.value.toFixed(0)}ms ({webVitals.lcp.rating})
                  </span>
                </div>
              )}
              {webVitals.fid && webVitals.fid.value !== undefined && (
                <div>
                  FID:{' '}
                  <span className={webVitals.fid.rating === 'good' ? 'text-mint-green' : 'text-amber-500'}>
                    {webVitals.fid.value.toFixed(0)}ms ({webVitals.fid.rating})
                  </span>
                </div>
              )}
              {webVitals.cls && webVitals.cls.value !== undefined && (
                <div>
                  CLS:{' '}
                  <span className={webVitals.cls.rating === 'good' ? 'text-mint-green' : 'text-amber-500'}>
                    {webVitals.cls.value.toFixed(3)} ({webVitals.cls.rating})
                  </span>
                </div>
              )}
            </div>
          </div>

          {/* Slow API Calls */}
          {slowCalls.length > 0 && (
            <div className="mb-4 pb-4 border-t border-border">
              <h3 className="font-bold text-amber-500 mb-2">Slow API Calls (&gt;1s)</h3>
              <div className="space-y-1 text-muted-foreground">
                {slowCalls.map((m, i) => (
                  <div key={i} className="text-xs">
                    {m.name}: <span className="text-red-400">{m.duration.toFixed(0)}ms</span>
                  </div>
                ))}
              </div>
            </div>
          )}

          {/* Errors */}
          {errors.length > 0 && (
            <div className="pb-4 border-t border-border">
              <h3 className="font-bold text-red-400 mb-2">Errors ({errors.length})</h3>
              <div className="space-y-1">
                {errors.slice(-3).map((e, i) => (
                  <div key={i} className="text-xs text-red-300 truncate">
                    {e.message}
                  </div>
                ))}
              </div>
            </div>
          )}

          {/* Stats */}
          <div className="text-muted-foreground border-t border-border pt-2">
            <div>Metrics recorded: {metrics.length}</div>
            <div>Errors logged: {errors.length}</div>
          </div>
        </div>
      )}
    </div>
  )
}
