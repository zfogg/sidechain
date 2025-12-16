import { describe, it, expect } from 'vitest'
import { Outcome } from '../types'

describe('Outcome<T>', () => {
  describe('creation', () => {
    it('should create successful outcome with value', () => {
      const outcome = Outcome.ok('success')
      expect(outcome.isOk()).toBe(true)
      expect(outcome.isError()).toBe(false)
      expect(outcome.getValue()).toBe('success')
    })

    it('should create error outcome with message', () => {
      const outcome = Outcome.error<string>('something went wrong')
      expect(outcome.isOk()).toBe(false)
      expect(outcome.isError()).toBe(true)
      expect(outcome.getError()).toBe('something went wrong')
    })
  })

  describe('isOk() and isError()', () => {
    it('should correctly identify successful outcomes', () => {
      const outcome = Outcome.ok(42)
      expect(outcome.isOk()).toBe(true)
      expect(outcome.isError()).toBe(false)
    })

    it('should correctly identify error outcomes', () => {
      const outcome = Outcome.error<number>('error')
      expect(outcome.isOk()).toBe(false)
      expect(outcome.isError()).toBe(true)
    })
  })

  describe('getValue()', () => {
    it('should return value for successful outcome', () => {
      const outcome = Outcome.ok('test')
      expect(outcome.getValue()).toBe('test')
    })

    it('should throw error when called on error outcome', () => {
      const outcome = Outcome.error<string>('error')
      expect(() => outcome.getValue()).toThrow()
    })
  })

  describe('getError()', () => {
    it('should return error message for error outcome', () => {
      const outcome = Outcome.error<string>('test error')
      expect(outcome.getError()).toBe('test error')
    })

    it('should throw error when called on successful outcome', () => {
      const outcome = Outcome.ok('success')
      expect(() => outcome.getError()).toThrow()
    })
  })

  describe('map()', () => {
    it('should transform successful value', () => {
      const outcome = Outcome.ok(5)
      const mapped = outcome.map((n) => n * 2)
      expect(mapped.getValue()).toBe(10)
    })

    it('should propagate error through map', () => {
      const outcome = Outcome.error<number>('error')
      const mapped = outcome.map((n) => n * 2)
      expect(mapped.isError()).toBe(true)
      expect(mapped.getError()).toBe('error')
    })

    it('should change value type', () => {
      const outcome = Outcome.ok('hello')
      const mapped = outcome.map((s) => s.length)
      expect(mapped.getValue()).toBe(5)
    })
  })

  describe('flatMap()', () => {
    it('should chain successful outcomes', () => {
      const outcome = Outcome.ok(5)
      const result = outcome.flatMap((n) => Outcome.ok(n * 2))
      expect(result.getValue()).toBe(10)
    })

    it('should handle error from flatMap function', () => {
      const outcome = Outcome.ok(5)
      const result = outcome.flatMap(() => Outcome.error<number>('flatmap error'))
      expect(result.isError()).toBe(true)
      expect(result.getError()).toBe('flatmap error')
    })

    it('should propagate original error', () => {
      const outcome = Outcome.error<number>('original error')
      const result = outcome.flatMap((n) => Outcome.ok(n * 2))
      expect(result.isError()).toBe(true)
      expect(result.getError()).toBe('original error')
    })
  })

  describe('onSuccess()', () => {
    it('should call callback on successful outcome', () => {
      let called = false
      let value = ''

      Outcome.ok('success').onSuccess((v) => {
        called = true
        value = v
      })

      expect(called).toBe(true)
      expect(value).toBe('success')
    })

    it('should not call callback on error outcome', () => {
      let called = false

      Outcome.error<string>('error').onSuccess(() => {
        called = true
      })

      expect(called).toBe(false)
    })

    it('should be chainable', () => {
      let callCount = 0

      const result = Outcome.ok('test')
        .onSuccess(() => {
          callCount++
        })
        .onSuccess(() => {
          callCount++
        })

      expect(callCount).toBe(2)
      expect(result.getValue()).toBe('test')
    })
  })

  describe('onError()', () => {
    it('should call callback on error outcome', () => {
      let called = false
      let error = ''

      Outcome.error<string>('test error').onError((e) => {
        called = true
        error = e
      })

      expect(called).toBe(true)
      expect(error).toBe('test error')
    })

    it('should not call callback on successful outcome', () => {
      let called = false

      Outcome.ok('success').onError(() => {
        called = true
      })

      expect(called).toBe(false)
    })

    it('should be chainable', () => {
      let callCount = 0

      const result = Outcome.error<string>('error')
        .onError(() => {
          callCount++
        })
        .onError(() => {
          callCount++
        })

      expect(callCount).toBe(2)
      expect(result.getError()).toBe('error')
    })
  })

  describe('monadic chaining', () => {
    it('should support complex monadic chains', () => {
      const result = Outcome.ok('hello')
        .map((s) => s.length)
        .flatMap((len) => {
          if (len > 0) {
            return Outcome.ok(len * 2)
          }
          return Outcome.error<number>('empty string')
        })
        .map((n) => n + 10)

      expect(result.getValue()).toBe(20) // 5 * 2 + 10
    })

    it('should short-circuit on error', () => {
      let mapCalled = false
      let flatMapCalled = false

      const result = Outcome.error<number>('initial error')
        .map((n) => {
          mapCalled = true
          return n * 2
        })
        .flatMap((n) => {
          flatMapCalled = true
          return Outcome.ok(n)
        })

      expect(mapCalled).toBe(false)
      expect(flatMapCalled).toBe(false)
      expect(result.getError()).toBe('initial error')
    })
  })
})
