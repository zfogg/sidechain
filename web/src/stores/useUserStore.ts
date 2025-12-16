import { create } from 'zustand';
import { persist } from 'zustand/middleware';
import type { User } from '../models/User';
import { apiClient } from '../api/client';

/**
 * User store - Handles authentication and current user state
 * Mirrors C++ UserStore pattern
 */

interface UserStoreState {
  user: User | null;
  token: string | null;
  isAuthenticated: boolean;
  isLoading: boolean;
  error: string;
}

interface UserStoreActions {
  login: (token: string, user: User) => void;
  logout: () => void;
  setUser: (user: User) => void;
  updateUser: (updates: Partial<User>) => void;
  setLoading: (loading: boolean) => void;
  setError: (error: string) => void;
  clearError: () => void;
  restoreSession: () => Promise<void>;
}

export const useUserStore = create<UserStoreState & UserStoreActions>()(
  persist(
    (set, get) => ({
      user: null,
      token: null,
      isAuthenticated: false,
      isLoading: false,
      error: '',

      login: (token, user) => {
        apiClient.setToken(token);
        set({
          user,
          token,
          isAuthenticated: true,
          error: '',
        });
      },

      logout: () => {
        apiClient.clearToken();
        set({
          user: null,
          token: null,
          isAuthenticated: false,
          error: '',
        });
      },

      setUser: (user) => {
        set({ user });
      },

      updateUser: (updates) => {
        const current = get().user;
        if (current) {
          set({
            user: { ...current, ...updates },
          });
        }
      },

      setLoading: (loading) => {
        set({ isLoading: loading });
      },

      setError: (error) => {
        set({ error });
      },

      clearError: () => {
        set({ error: '' });
      },

      restoreSession: async () => {
        const storedToken = localStorage.getItem('auth_token');
        if (storedToken) {
          set({ isLoading: true });
          try {
            apiClient.setToken(storedToken);
            // Could validate token by fetching current user here
            set({
              token: storedToken,
              isAuthenticated: true,
              isLoading: false,
            });
          } catch (error) {
            apiClient.clearToken();
            localStorage.removeItem('auth_token');
            set({
              token: null,
              isAuthenticated: false,
              isLoading: false,
              error: 'Session expired',
            });
          }
        }
      },
    }),
    {
      name: 'user-store',
      partialize: (state) => ({
        user: state.user,
        token: state.token,
        isAuthenticated: state.isAuthenticated,
      }),
    }
  )
);
