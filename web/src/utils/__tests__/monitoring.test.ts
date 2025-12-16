import { describe, it, expect, beforeEach, vi, afterEach } from 'vitest'
import monitor from '../monitoring'

describe('PerformanceMonitor', () => {
  beforeEach(() => {
    monitor.clear()
  })

  describe('recordMetric()', () => {
    it('should record a metric', () => {
      monitor.recordMetric('test_operation', 100)

      const metrics = monitor.getMetrics()
      expect(metrics).toHaveLength(1)
      expect(metrics[0].name).toBe('test_operation')
      expect(metrics[0].duration).toBe(100)
    })

    it('should include timestamp', () => {
      const now = Date.now()
      monitor.recordMetric('test_operation', 100)

      const metrics = monitor.getMetrics()
      expect(metrics[0].timestamp).toBeGreaterThanOrEqual(now)
    })

    it('should include metadata', () => {
      monitor.recordMetric('test_operation', 100, { userId: '123' })

      const metrics = monitor.getMetrics()
      expect(metrics[0].metadata).toEqual({ userId: '123' })
    })

    it('should warn on slow operations', () => {
      const warnSpy = vi.spyOn(console, 'warn').mockImplementation()
      monitor.recordMetric('slow_operation', 1500)

      expect(warnSpy).toHaveBeenCalledWith(
        expect.stringContaining('Slow operation'),
        expect.any(String)
      )

      warnSpy.mockRestore()
    })

    it('should keep only recent metrics', () => {
      // Record 150 metrics (max is 100)
      for (let i = 0; i < 150; i++) {
        monitor.recordMetric(`operation_${i}`, 100)
      }

      const metrics = monitor.getMetrics()
      expect(metrics).toHaveLength(100)
      // Should keep the last 100
      expect(metrics[0].name).toBe('operation_50')
    })
  })

  describe('measureAsync()', () => {
    it('should measure async operation', async () => {
      const result = await monitor.measureAsync('async_op', async () => {
        await new Promise((resolve) => setTimeout(resolve, 10))
        return 'done'
      })

      expect(result).toBe('done')

      const metrics = monitor.getMetrics()
      expect(metrics).toHaveLength(1)
      expect(metrics[0].name).toBe('async_op')
      expect(metrics[0].duration).toBeGreaterThanOrEqual(10)
    })

    it('should handle errors in async operation', async () => {
      const error = new Error('async error')

      await expect(
        monitor.measureAsync('async_op', async () => {
          throw error
        })
      ).rejects.toThrow('async error')

      const metrics = monitor.getMetrics()
      expect(metrics).toHaveLength(1)
      expect(metrics[0].metadata?.error).toBe(true)
    })

    it('should pass metadata', async () => {
      await monitor.measureAsync(
        'async_op',
        async () => 'result',
        { userId: '123' }
      )

      const metrics = monitor.getMetrics()
      expect(metrics[0].metadata?.userId).toBe('123')
    })
  })

  describe('measure()', () => {
    it('should measure sync operation', () => {
      const result = monitor.measure('sync_op', () => {
        return 42
      })

      expect(result).toBe(42)

      const metrics = monitor.getMetrics()
      expect(metrics).toHaveLength(1)
      expect(metrics[0].name).toBe('sync_op')
    })

    it('should handle errors in sync operation', () => {
      const error = new Error('sync error')

      expect(() => {
        monitor.measure('sync_op', () => {
          throw error
        })
      }).toThrow('sync error')

      const metrics = monitor.getMetrics()
      expect(metrics[0].metadata?.error).toBe(true)
    })
  })

  describe('logError()', () => {
    it('should log an error', () => {
      monitor.logError('Test error', new Error('Test'))

      const errors = monitor.getErrors()
      expect(errors).toHaveLength(1)
      expect(errors[0].message).toBe('Test error')
      expect(errors[0].severity).toBe('error')
    })

    it('should capture stack trace', () => {
      const error = new Error('Test error')
      monitor.logError('Logging error', error)

      const errors = monitor.getErrors()
      expect(errors[0].stack).toContain('Test error')
    })

    it('should include context', () => {
      monitor.logError('Test error', new Error(), { userId: '123' })

      const errors = monitor.getErrors()
      expect(errors[0].context?.userId).toBe('123')
    })

    it('should keep only recent errors', () => {
      // Record 60 errors (max is 50)
      for (let i = 0; i < 60; i++) {
        monitor.logError(`error_${i}`, new Error())
      }

      const errors = monitor.getErrors()
      expect(errors).toHaveLength(50)
    })
  })

  describe('logWarning()', () => {
    it('should log a warning', () => {
      monitor.logWarning('Test warning')

      const errors = monitor.getErrors()
      expect(errors).toHaveLength(1)
      expect(errors[0].message).toBe('Test warning')
      expect(errors[0].severity).toBe('warning')
    })

    it('should include context', () => {
      monitor.logWarning('Test warning', { data: 'value' })

      const errors = monitor.getErrors()
      expect(errors[0].context?.data).toBe('value')
    })
  })

  describe('getAverageDuration()', () => {
    it('should calculate average duration', () => {
      monitor.recordMetric('operation', 100)
      monitor.recordMetric('operation', 200)
      monitor.recordMetric('operation', 300)

      const avg = monitor.getAverageDuration('operation')
      expect(avg).toBe(200)
    })

    it('should return 0 for non-existent metric', () => {
      const avg = monitor.getAverageDuration('non_existent')
      expect(avg).toBe(0)
    })
  })

  describe('clear()', () => {
    it('should clear all metrics and errors', () => {
      monitor.recordMetric('operation', 100)
      monitor.logError('error', new Error())

      expect(monitor.getMetrics()).toHaveLength(1)
      expect(monitor.getErrors()).toHaveLength(1)

      monitor.clear()

      expect(monitor.getMetrics()).toHaveLength(0)
      expect(monitor.getErrors()).toHaveLength(0)
    })
  })

  describe('global error handlers', () => {
    it('should capture unhandled errors', () => {
      const event = new ErrorEvent('error', {
        message: 'Global error',
        error: new Error('Test error'),
      })

      window.dispatchEvent(event)

      const errors = monitor.getErrors()
      expect(errors.length).toBeGreaterThan(0)
    })

    it('should capture unhandled promise rejections', () => {
      const event = new PromiseRejectionEvent('unhandledrejection', {
        reason: 'Promise rejection',
        promise: Promise.reject('rejection'),
      })

      window.dispatchEvent(event)

      const errors = monitor.getErrors()
      expect(errors.length).toBeGreaterThan(0)
    })
  })
})
