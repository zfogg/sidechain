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

    // Request interceptor - handle FormData uploads and ensure auth token is always set
    this.client.interceptors.request.use((config) => {
      // Ensure auth token is always set from localStorage (in case it was updated)
      const currentToken = localStorage.getItem('auth_token');
      if (currentToken && currentToken !== this.token) {
        this.token = currentToken;
        this.updateAuthHeader();
      }
      
      // For FormData, axios must auto-detect and set multipart/form-data
      // Remove any Content-Type override so axios can detect FormData
      if (config.data instanceof FormData) {
        // Delete from headers to prevent the default 'application/json' from interfering
        delete config.headers['Content-Type'];
        // Also ensure transformRequest is not overriding
        config.transformRequest = [(data) => data];
      }
      
      // Log request for debugging feed issues
      if (config.url?.includes('/feed/')) {
        console.log(`[ApiClient] Making ${config.method?.toUpperCase()} request to ${config.url}`, {
          hasToken: !!this.token,
          tokenPrefix: this.token?.substring(0, 20),
          params: config.params
        });
      }
      
      return config;
    });

    // Response interceptor - handle auth errors and log feed responses
    this.client.interceptors.response.use(
      (response) => {
        // Log feed responses for debugging
        if (response.config.url?.includes('/feed/')) {
          const data = response.data;
          const activityCount = data?.activities?.length ?? 0;
          const responseKeys = Object.keys(data || {});
          
          // Check if activities is actually an array
          const activitiesIsArray = Array.isArray(data?.activities);
          const activitiesType = typeof data?.activities;
          
          console.log(`[ApiClient] Feed response from ${response.config.url}:`, {
            status: response.status,
            activityCount,
            hasActivities: activityCount > 0,
            activitiesIsArray,
            activitiesType,
            responseKeysCount: responseKeys.length,
            responseKeys: responseKeys.slice(0, 10), // Only show first 10 keys
            // Log a sample of the response structure
            responseSample: JSON.stringify(data).substring(0, 500)
          });
          
          // Warn if response has suspiciously many keys
          if (responseKeys.length > 100) {
            console.warn(`[ApiClient] Suspicious response: ${responseKeys.length} keys in response object. Expected 2-3 keys (activities, meta).`);
          }
          
          // Warn if activities is not an array
          if (!activitiesIsArray && data?.activities !== undefined) {
            console.error(`[ApiClient] activities is not an array! Type: ${activitiesType}, Value:`, data.activities);
          }
        }
        return response;
      },
      (error) => {
        if (error.response?.status === 401) {
          // Token expired, logout
          console.error('[ApiClient] 401 Unauthorized - token expired or invalid');
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
  async get<T>(
    url: string, 
    params?: Record<string, string | number | boolean | (string | number)[]>,
    headers?: Record<string, string>
  ): Promise<Outcome<T>> {
    try {
      const response = await this.client.get<T>(url, { params, headers });
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
  async delete<T>(url: string, data?: object): Promise<Outcome<T>> {
    try {
      const response = await this.client.delete<T>(url, { data });
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
    } catch (error: any) {
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
