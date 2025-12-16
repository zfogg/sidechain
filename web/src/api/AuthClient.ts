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

  // ============== Two-Factor Authentication (2FA) ==============

  /**
   * Initiate 2FA setup (TOTP)
   * Returns QR code and secret for manual entry
   */
  static async setupTwoFactor(): Promise<
    Outcome<{ qrCode: string; secret: string; backupCodes: string[] }>
  > {
    const result = await apiClient.post<any>('/auth/2fa/setup', {});

    if (result.isOk()) {
      try {
        const data = result.getValue();
        return Outcome.ok({
          qrCode: data.qr_code || '',
          secret: data.secret || '',
          backupCodes: data.backup_codes || [],
        });
      } catch (error) {
        return Outcome.error((error as Error).message);
      }
    }

    return Outcome.error(result.getError());
  }

  /**
   * Verify TOTP code and enable 2FA
   */
  static async verifyTwoFactor(totpCode: string): Promise<Outcome<{ backupCodes: string[] }>> {
    const result = await apiClient.post<any>('/auth/2fa/verify', {
      totp_code: totpCode,
    });

    if (result.isOk()) {
      try {
        const data = result.getValue();
        return Outcome.ok({
          backupCodes: data.backup_codes || [],
        });
      } catch (error) {
        return Outcome.error((error as Error).message);
      }
    }

    return Outcome.error(result.getError());
  }

  /**
   * Disable 2FA for the current user
   */
  static async disableTwoFactor(password: string): Promise<Outcome<void>> {
    const result = await apiClient.post('/auth/2fa/disable', {
      password,
    });

    if (result.isOk()) {
      return Outcome.ok(undefined);
    }

    return Outcome.error(result.getError());
  }

  /**
   * Generate new backup codes (requires current password)
   */
  static async generateBackupCodes(password: string): Promise<Outcome<{ backupCodes: string[] }>> {
    const result = await apiClient.post<any>('/auth/2fa/backup-codes', {
      password,
    });

    if (result.isOk()) {
      try {
        const data = result.getValue();
        return Outcome.ok({
          backupCodes: data.backup_codes || [],
        });
      } catch (error) {
        return Outcome.error((error as Error).message);
      }
    }

    return Outcome.error(result.getError());
  }

  /**
   * Login with 2FA code (called after initial login if 2FA is enabled)
   */
  static async loginWith2FA(
    email: string,
    totpCodeOrBackupCode: string
  ): Promise<Outcome<AuthResponse>> {
    const result = await apiClient.post<any>('/auth/login/2fa', {
      email,
      totp_code: totpCodeOrBackupCode,
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
   * Check if current user has 2FA enabled
   */
  static async getTwoFactorStatus(): Promise<Outcome<{ isTwoFactorEnabled: boolean }>> {
    const result = await apiClient.get<any>('/auth/2fa/status');

    if (result.isOk()) {
      try {
        const data = result.getValue();
        return Outcome.ok({
          isTwoFactorEnabled: data.is_two_factor_enabled || false,
        });
      } catch (error) {
        return Outcome.error((error as Error).message);
      }
    }

    return Outcome.error(result.getError());
  }
}
