/**
 * Outcome<T> - Type-safe result wrapper
 * Mirrors the C++ plugin's error handling pattern
 * Enables monadic operations (map, flatMap, onSuccess, onError)
 */

export class Outcome<T> {
  private constructor(
    private value: T | null,
    private error: string | null
  ) {}

  /**
   * Create a successful outcome
   */
  static ok<T>(value: T): Outcome<T> {
    return new Outcome(value, null);
  }

  /**
   * Create an error outcome
   */
  static error<T = never>(error: string): Outcome<T> {
    return new Outcome<T>(null as T | null, error);
  }

  /**
   * Check if outcome is successful
   */
  isOk(): boolean {
    return this.value !== null;
  }

  /**
   * Check if outcome is an error
   */
  isError(): boolean {
    return this.error !== null;
  }

  /**
   * Get the value (throws if error)
   */
  getValue(): T {
    if (!this.isOk()) throw new Error(`Outcome is error: ${this.error}`);
    return this.value!;
  }

  /**
   * Get the error (throws if ok)
   */
  getError(): string {
    if (!this.isError()) throw new Error('Outcome is ok');
    return this.error!;
  }

  /**
   * Transform the value if ok, otherwise return error unchanged
   */
  map<U>(fn: (value: T) => U): Outcome<U> {
    if (this.isOk()) {
      try {
        return Outcome.ok(fn(this.getValue()));
      } catch (error) {
        return Outcome.error((error as Error).message);
      }
    }
    return Outcome.error(this.getError());
  }

  /**
   * Chain outcomes - flatMap
   */
  flatMap<U>(fn: (value: T) => Outcome<U>): Outcome<U> {
    if (this.isOk()) {
      try {
        return fn(this.getValue());
      } catch (error) {
        return Outcome.error((error as Error).message);
      }
    }
    return Outcome.error(this.getError());
  }

  /**
   * Execute side effect if successful
   */
  onSuccess(fn: (value: T) => void): this {
    if (this.isOk()) {
      try {
        fn(this.getValue());
      } catch (error) {
        console.error('onSuccess error:', error);
      }
    }
    return this;
  }

  /**
   * Execute side effect if error
   */
  onError(fn: (error: string) => void): this {
    if (this.isError()) {
      try {
        fn(this.getError());
      } catch (error) {
        console.error('onError error:', error);
      }
    }
    return this;
  }

  /**
   * Unwrap with default value
   */
  unwrapOr(defaultValue: T): T {
    return this.isOk() ? this.getValue() : defaultValue;
  }

  /**
   * Convert to Promise
   */
  toPromise(): Promise<T> {
    if (this.isOk()) {
      return Promise.resolve(this.getValue());
    }
    return Promise.reject(new Error(this.getError()));
  }
}
