import axios from 'axios'
import type { AxiosInstance, AxiosError } from 'axios'
import { Outcome } from './types'

/**
 * Base HTTP client using Axios
 * Handles authentication, error handling, and response parsing
 * Mirrors C++ NetworkClient pattern
 */

export class ApiClient {
  private client: AxiosInstance;
  private token: string | null = null;

  constructor(baseURL: string) {
    this.client = axios.create({
      baseURL,
      timeout: 30000,
      headers: {
        'Content-Type': 'application/json',
      },
    });

    // Load token from localStorage if available
    const storedToken = localStorage.getItem('auth_token');
    if (storedToken) {
      this.token = storedToken;
      this.updateAuthHeader();
    }

    // Request interceptor - handle FormData uploads
    this.client.interceptors.request.use((config) => {
      // For FormData, don't set Content-Type header
      // Let axios auto-detect and set multipart/form-data with boundary
      if (config.data instanceof FormData) {
        delete config.headers['Content-Type'];
      }
      return config;
    });

    // Response interceptor - handle auth errors
    this.client.interceptors.response.use(
      (response) => response,
      (error) => {
        if (error.response?.status === 401) {
          // Token expired, logout
          this.clearToken();
          window.location.href = '/login?error=session_expired';
        }
        return Promise.reject(error);
      }
    );
  }

  /**
   * Set authentication token
   */
  setToken(token: string): void {
    this.token = token;
    localStorage.setItem('auth_token', token);
    this.updateAuthHeader();
  }

  /**
   * Clear authentication token
   */
  clearToken(): void {
    this.token = null;
    localStorage.removeItem('auth_token');
    delete this.client.defaults.headers.common['Authorization'];
  }

  /**
   * Get current token
   */
  getToken(): string | null {
    return this.token;
  }

  /**
   * Check if authenticated
   */
  isAuthenticated(): boolean {
    return this.token !== null;
  }

  /**
   * Update Authorization header
   */
  private updateAuthHeader(): void {
    if (this.token) {
      this.client.defaults.headers.common['Authorization'] = `Bearer ${this.token}`;
    }
  }

  /**
   * GET request with Outcome<T> return type
   */
  async get<T>(url: string, params?: Record<string, string | number | boolean | (string | number)[]>): Promise<Outcome<T>> {
    try {
      const response = await this.client.get<T>(url, { params });
      return Outcome.ok(response.data);
    } catch (error) {
      return this.handleError<T>(error as Error | AxiosError);
    }
  }

  /**
   * POST request with Outcome<T> return type
   */
  async post<T>(url: string, data?: object): Promise<Outcome<T>> {
    try {
      const response = await this.client.post<T>(url, data);
      return Outcome.ok(response.data);
    } catch (error) {
      return this.handleError<T>(error as Error | AxiosError);
    }
  }

  /**
   * PUT request with Outcome<T> return type
   */
  async put<T>(url: string, data?: object): Promise<Outcome<T>> {
    try {
      const response = await this.client.put<T>(url, data);
      return Outcome.ok(response.data);
    } catch (error) {
      return this.handleError<T>(error as Error | AxiosError);
    }
  }

  /**
   * PATCH request with Outcome<T> return type
   */
  async patch<T>(url: string, data?: object): Promise<Outcome<T>> {
    try {
      const response = await this.client.patch<T>(url, data);
      return Outcome.ok(response.data);
    } catch (error) {
      return this.handleError<T>(error as Error | AxiosError);
    }
  }

  /**
   * DELETE request with Outcome<T> return type
   */
  async delete<T>(url: string): Promise<Outcome<T>> {
    try {
      const response = await this.client.delete<T>(url);
      return Outcome.ok(response.data);
    } catch (error) {
      return this.handleError<T>(error as Error | AxiosError);
    }
  }

  /**
   * File upload with FormData
   * Request interceptor automatically removes JSON Content-Type header for FormData
   * Axios then auto-detects FormData and sets multipart/form-data with boundary
   */
  async upload<T>(url: string, formData: FormData): Promise<Outcome<T>> {
    try {
      const response = await this.client.post<T>(url, formData);
      return Outcome.ok(response.data);
    } catch (error) {
      return this.handleError<T>(error as Error | AxiosError);
    }
  }

  /**
   * Handle errors and convert to Outcome
   */
  private handleError<T>(error: Error | AxiosError): Outcome<T> {
    if (axios.isAxiosError(error)) {
      const message = error.response?.data?.message ||
                      error.response?.data?.error ||
                      error.message ||
                      'Unknown error occurred';
      return Outcome.error(String(message));
    }

    if (error instanceof Error) {
      return Outcome.error(error.message);
    }

    return Outcome.error('Unknown error occurred');
  }
}

/**
 * Singleton API client instance
 */
export const apiClient = new ApiClient(
  import.meta.env.VITE_API_URL || 'http://localhost:8787/api/v1'
);
