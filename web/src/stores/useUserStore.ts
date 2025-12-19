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
        const storedUser = localStorage.getItem('user-store');

        if (storedToken) {
          set({ isLoading: true });
          try {
            apiClient.setToken(storedToken);

            // Fetch current user to validate token and get fresh user data
            const response = await apiClient.get<{ user: User }>('/users/me', {}, 5000);
            if (response.isOk()) {
              const { user } = response.getValue();
              set({
                user,
                token: storedToken,
                isAuthenticated: true,
                isLoading: false,
              });
            } else {
              // Token might be invalid, try to use cached user data
              if (storedUser) {
                try {
                  const cachedState = JSON.parse(storedUser);
                  set({
                    user: cachedState.state?.user || null,
                    token: storedToken,
                    isAuthenticated: !!cachedState.state?.user,
                    isLoading: false,
                  });
                } catch {
                  throw new Error('Failed to fetch user');
                }
              } else {
                throw new Error('Failed to fetch user');
              }
            }
          } catch (error) {
            apiClient.clearToken();
            localStorage.removeItem('auth_token');
            set({
              token: null,
              isAuthenticated: false,
              isLoading: false,
              error: 'Session expired',
              user: null,
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
