import { useEffect } from 'react'
import monitor from '@/utils/monitoring'

interface PerformanceEntryWithTime extends PerformanceEntry {
  renderTime?: number
  loadTime?: number
  processingDuration?: number
}

export interface WebVital {
  name: string
  value: number
  rating: 'good' | 'needs-improvement' | 'poor'
}

/**
 * useWebVitals - Hook to track Google Core Web Vitals
 *
 * Metrics:
 * - LCP (Largest Contentful Paint): 0-2.5s = good, 2.5-4s = needs improvement, >4s = poor
 * - FID (First Input Delay): 0-100ms = good, 100-300ms = needs improvement, >300ms = poor
 * - CLS (Cumulative Layout Shift): 0-0.1 = good, 0.1-0.25 = needs improvement, >0.25 = poor
 */
export function useWebVitals() {
  useEffect(() => {
    // Check if browser supports Web Vitals API
    if ('web-vital' in window) {
      // Use web-vital library if available
      return
    }

    // Fallback: Manual implementation using PerformanceObserver
    try {
      // Measure LCP (Largest Contentful Paint)
      const lcpObserver = new PerformanceObserver((list) => {
        const entries = list.getEntries()
        const lastEntry = entries[entries.length - 1] as PerformanceEntryWithTime
        const value = lastEntry.renderTime || lastEntry.loadTime || 0

        const lcp: WebVital = {
          name: 'LCP',
          value,
          rating:
            value < 2500
              ? 'good'
              : value < 4000
                ? 'needs-improvement'
                : 'poor',
        }

        monitor.recordMetric(lcp.name, lcp.value, { rating: lcp.rating })
      })

      lcpObserver.observe({ entryTypes: ['largest-contentful-paint'] })

      // Measure FID (First Input Delay)
      const fidObserver = new PerformanceObserver((list) => {
        const entries = list.getEntries()

        entries.forEach((entry) => {
          const processingDuration = (entry as PerformanceEntryWithTime).processingDuration || 0
          const fid: WebVital = {
            name: 'FID',
            value: processingDuration,
            rating:
              processingDuration < 100
                ? 'good'
                : processingDuration < 300
                  ? 'needs-improvement'
                  : 'poor',
          }

          monitor.recordMetric(fid.name, fid.value, { rating: fid.rating })
        })
      })

      fidObserver.observe({ entryTypes: ['first-input'] })

      // Measure CLS (Cumulative Layout Shift)
      let clsValue = 0
      const clsObserver = new PerformanceObserver((list) => {
        list.getEntries().forEach((entry) => {
          if (!(entry as any).hadRecentInput) {
            clsValue += (entry as any).value
          }
        })

        const cls: WebVital = {
          name: 'CLS',
          value: clsValue,
          rating: clsValue < 0.1 ? 'good' : clsValue < 0.25 ? 'needs-improvement' : 'poor',
        }

        monitor.recordMetric(cls.name, cls.value, { rating: cls.rating })
      })

      clsObserver.observe({ entryTypes: ['layout-shift'] })

      // Cleanup
      return () => {
        lcpObserver.disconnect()
        fidObserver.disconnect()
        clsObserver.disconnect()
      }
    } catch (error) {
      monitor.logWarning('Web Vitals not supported in this browser', { error })
    }
  }, [])
}

/**
 * Get Web Vitals ratings summary
 * Useful for displaying in dev tools or settings
 */
export function getWebVitalsSummary(): Record<string, WebVital | null> {
  const metrics = monitor.getMetrics()

  const lcpMetric = metrics.filter(m => m.name === 'LCP').pop()
  const fidMetric = metrics.filter(m => m.name === 'FID').pop()
  const clsMetric = metrics.filter(m => m.name === 'CLS').pop()

  return {
    lcp: lcpMetric
      ? {
          name: 'LCP',
          value: lcpMetric.duration,
          rating:
            lcpMetric.duration < 2500 ? 'good' : lcpMetric.duration < 4000 ? 'needs-improvement' : 'poor',
        }
      : null,
    fid: fidMetric
      ? {
          name: 'FID',
          value: fidMetric.duration,
          rating:
            fidMetric.duration < 100 ? 'good' : fidMetric.duration < 300 ? 'needs-improvement' : 'poor',
        }
      : null,
    cls: clsMetric
      ? {
          name: 'CLS',
          value: clsMetric.duration,
          rating:
            clsMetric.duration < 0.1 ? 'good' : clsMetric.duration < 0.25 ? 'needs-improvement' : 'poor',
        }
      : null,
  }
}
