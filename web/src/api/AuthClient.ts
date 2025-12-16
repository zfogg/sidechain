import { apiClient } from './client'
import { Outcome } from './types'
import { UserModel } from '../models/User'
import type { User } from '../models/User'

/**
 * AuthClient - Authentication and user management
 */

export interface AuthResponse {
  token: string;
  user: User;
  expiresAt: string;
}

export class AuthClient {
  /**
   * Login with email and password
   */
  static async login(email: string, password: string): Promise<Outcome<AuthResponse>> {
    const result = await apiClient.post<any>('/auth/login', { email, password });

    if (result.isOk()) {
      try {
        const data = result.getValue();
        return Outcome.ok({
          token: data.auth?.token || data.token || '',
          user: UserModel.fromJson(data.auth?.user || data.user || {}),
          expiresAt: data.auth?.expires_at || data.expires_at || '',
        });
      } catch (error) {
        return Outcome.error((error as Error).message);
      }
    }

    return Outcome.error(result.getError());
  }

  /**
   * Register new user
   */
  static async register(
    email: string,
    username: string,
    password: string
  ): Promise<Outcome<AuthResponse>> {
    const result = await apiClient.post<any>('/auth/register', {
      email,
      username,
      password,
    });

    if (result.isOk()) {
      try {
        const data = result.getValue();
        return Outcome.ok({
          token: data.auth?.token || data.token || '',
          user: UserModel.fromJson(data.auth?.user || data.user || {}),
          expiresAt: data.auth?.expires_at || data.expires_at || '',
        });
      } catch (error) {
        return Outcome.error((error as Error).message);
      }
    }

    return Outcome.error(result.getError());
  }

  /**
   * Get current authenticated user
   */
  static async getMe(): Promise<Outcome<User>> {
    const result = await apiClient.get<any>('/auth/me');

    if (result.isOk()) {
      try {
        const user = UserModel.fromJson(result.getValue());
        return Outcome.ok(user);
      } catch (error) {
        return Outcome.error((error as Error).message);
      }
    }

    return Outcome.error(result.getError());
  }

  /**
   * Get Stream Chat token for real-time features
   */
  static async getStreamToken(): Promise<Outcome<{ token: string }>> {
    return apiClient.get('/auth/stream-token');
  }

  /**
   * Logout (client-side only, token is cleared locally)
   */
  static logout(): void {
    apiClient.clearToken();
  }

  /**
   * Refresh auth token
   */
  static async refreshToken(): Promise<Outcome<AuthResponse>> {
    const result = await apiClient.post<any>('/auth/refresh', {});

    if (result.isOk()) {
      try {
        const data = result.getValue();
        const token = data.auth?.token || data.token || '';
        if (token) {
          apiClient.setToken(token);
        }
        return Outcome.ok({
          token,
          user: UserModel.fromJson(data.auth?.user || data.user || {}),
          expiresAt: data.auth?.expires_at || data.expires_at || '',
        });
      } catch (error) {
        return Outcome.error((error as Error).message);
      }
    }

    return Outcome.error(result.getError());
  }

  /**
   * Claim a VST device with OAuth
   */
  static async claimDevice(
    deviceId: string,
    oauthProvider: string,
    code: string
  ): Promise<Outcome<AuthResponse>> {
    const result = await apiClient.post<any>('/auth/claim', {
      device_id: deviceId,
      oauth_provider: oauthProvider,
      oauth_code: code,
    });

    if (result.isOk()) {
      try {
        const data = result.getValue();
        return Outcome.ok({
          token: data.auth?.token || data.token || '',
          user: UserModel.fromJson(data.auth?.user || data.user || {}),
          expiresAt: data.auth?.expires_at || data.expires_at || '',
        });
      } catch (error) {
        return Outcome.error((error as Error).message);
      }
    }

    return Outcome.error(result.getError());
  }
}
